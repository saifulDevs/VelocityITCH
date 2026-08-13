#pragma once

#include <vector>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>

#include "config.hpp"
#include "metrics.hpp"
#include "logger.hpp"
#include "interfaces.hpp"
#include "validator.hpp"
#include "session_manager.hpp"
#include "price_engine.hpp"
#include "event_generator.hpp"
#include "encoder.hpp"
#include "chaos_injector.hpp"
#include "transport.hpp"
#include "recorder.hpp"
#include "historical_replay.hpp"

namespace itch {

class EngineScheduler {
private:
    Config config_;
    MetricsCollector metrics_;
    
    SessionManager session_mgr_;
    PriceEngine price_engine_;
    EventGenerator generator_;
    
    std::unique_ptr<IEncoder> encoder_;
    ChaosInjector chaos_injector_;
    std::unique_ptr<ITransport> transport_;
    std::unique_ptr<IRecorder> recorder_;

    std::chrono::high_resolution_clock::time_point last_rate_check_;
    uint64_t msgs_in_current_window_{0};

public:
    explicit EngineScheduler(Config config)
        : config_(config),
          generator_(config.protocol_mode),
          chaos_injector_(ChaosConfig{
              config.packet_drop_rate,
              0.0,
              config.sequence_gap_step,
              config.sequence_gap_size,
              config.max_jitter_us
          }) {

        // Polymorphic encoder selection based on config
        generator_.set_ratios(config_.add_ratio, config_.exec_ratio);

        if (config_.framing_mode == FramingMode::Raw) {
            encoder_ = std::make_unique<RawItchEncoder>();
        } else if (config_.framing_mode == FramingMode::SoupBinTCP) {
            encoder_ = std::make_unique<SoupBinTCPEncoder>();
        } else {
            encoder_ = std::make_unique<MoldUDP64Encoder>();
        }

        // Polymorphic transport initialization
        if (config_.net_mode == NetworkMode::TcpServer) {
            transport_ = std::make_unique<TcpServerTransport>(config_.ip, config_.port);
        } else {
            transport_ = std::make_unique<UdpTransport>(config_.ip, config_.port, config_.net_mode);
        }

        // Polymorphic recorder initialization
        if (!config_.record_file.empty()) {
            recorder_ = std::make_unique<BinaryStreamRecorder>(config_.record_file);
        }

        last_rate_check_ = std::chrono::high_resolution_clock::now();
    }

    void run(std::atomic<bool>& running_flag) {
        config_.print();

        if (!config_.replay_file.empty()) {
            HistoricalReplayEngine replay_engine(config_.replay_file, config_.replay_speed);
            NetworkDispatcher legacy_disp(config_.ip, config_.port, config_.net_mode, config_.framing_mode, config_.target_rate_mps);
            replay_engine.replay(legacy_disp);
            return;
        }

        Logger::info("EngineScheduler: Initializing market session...");

        // 1. Send System Event 'O' (Start of Messages)
        auto msg_o = generator_.generate_start_of_messages();
        dispatch_raw_event(&msg_o, sizeof(msg_o));

        // 2. Send System Event 'S' (Start of System Hours)
        auto msg_s = generator_.generate_start_of_system_hours();
        dispatch_raw_event(&msg_s, sizeof(msg_s));

        // 2b. Send System Event 'Q' (Start of Market Hours / Continuous Trading Open)
        auto msg_q = generator_.generate_start_of_market_hours();
        dispatch_raw_event(&msg_q, sizeof(msg_q));

        // 3. Send Company Directory 'P'
        auto company_directories = generator_.generate_company_directories();
        for (const auto& cdir : company_directories) {
            dispatch_raw_event(&cdir, sizeof(cdir));
        }

        // 4. Send Orderbook Directory 'R'
        auto directories = generator_.generate_directory();
        for (const auto& dir : directories) {
            dispatch_raw_event(&dir, sizeof(dir));
        }

        // 4. Send Orderbook Trading Action 'H'
        auto trading_actions = generator_.generate_trading_actions();
        for (const auto& action : trading_actions) {
            dispatch_raw_event(&action, sizeof(action));
        }

        // 5. Send Reference Prices 'A' (with 0 order id and 0 quantity)
        auto ref_prices = generator_.generate_ref_prices();
        for (const auto& ref : ref_prices) {
            dispatch_raw_event(&ref, sizeof(ref));
        }

        flush_encoder();
        Logger::info("EngineScheduler: Session active. Pipeline processing events...");

        uint8_t event_buf[256];

        while (running_flag) {
            // Dynamic Session Transition Checks
            if (config_.enable_session_flow && metrics_.total_messages() % 100000 == 0 && metrics_.total_messages() > 0) {
                if (session_mgr_.current_state() == MarketSessionState::ContinuousTrading) {
                    session_mgr_.transition_to(MarketSessionState::TradingHalt);
                    auto halt = session_mgr_.create_system_event(MarketSessionState::TradingHalt, 0);
                    dispatch_raw_event(&halt, sizeof(halt));
                } else if (session_mgr_.current_state() == MarketSessionState::TradingHalt) {
                    session_mgr_.transition_to(MarketSessionState::ContinuousTrading);
                    auto resume = session_mgr_.create_system_event(MarketSessionState::TradingResume, 0);
                    dispatch_raw_event(&resume, sizeof(resume));
                }
            }

            // Generate event payload
            size_t len = generator_.generate_next_event(event_buf);
            dispatch_raw_event(event_buf, len);

            if (config_.max_count > 0 && metrics_.total_messages() >= config_.max_count) {
                break;
            }

            // Report telemetry stats
            if (metrics_.total_messages() % 50000 == 0) {
                metrics_.set_active_orders(generator_.active_orders_count());
                metrics_.report_telemetry();
            }
        }

        // Send Market Close event
        auto close_msg = session_mgr_.create_system_event(MarketSessionState::MarketClose, 0);
        dispatch_raw_event(&close_msg, sizeof(close_msg));
        flush_encoder();

        if (recorder_) {
            recorder_->close();
        }

        metrics_.print_final_summary();
    }

private:
    void dispatch_raw_event(const void* data, size_t len) {
        // Validation Layer: Pre-encoding data integrity & bounds check
        if (!EventValidator::validate_event(data, len)) {
            Logger::error("Validation Failed: Event payload dropped before encoding!");
            return;
        }

        // Recorder Layer: Optional binary stream recording
        if (recorder_ && recorder_->is_active()) {
            recorder_->record_message(data, len);
        }

        metrics_.record_message_sent(len);

        std::vector<uint8_t> packet;
        if (encoder_->append_message(data, len, packet)) {
            send_encoded_packet(packet);
        }

        throttle_rate();
    }

    void flush_encoder() {
        std::vector<uint8_t> packet;
        if (encoder_->finalize_packet(packet)) {
            send_encoded_packet(packet);
        }
    }

    void send_encoded_packet(const std::vector<uint8_t>& packet) {
        if (packet.empty()) return;

        // Sequence Gap Chaos Fault
        uint64_t gap_skip = chaos_injector_.evaluate_sequence_gap();
        if (gap_skip > 0) {
            encoder_->skip_sequence_numbers(gap_skip);
            metrics_.record_sequence_gap();
        }

        // Packet Drop Chaos Fault
        if (chaos_injector_.should_drop_packet()) {
            metrics_.record_packet_dropped();
            return;
        }

        chaos_injector_.apply_jitter();

        if (transport_->send_packet(packet.data(), packet.size())) {
            metrics_.record_packet_sent();
        }
    }

    inline void throttle_rate() {
        if (config_.target_rate_mps == 0) return;

        msgs_in_current_window_++;
        uint64_t batch_size = std::min<uint64_t>(1000ULL, std::max<uint64_t>(1ULL, config_.target_rate_mps / 20));
        if (msgs_in_current_window_ >= batch_size) {
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(now - last_rate_check_).count();
            
            double expected_us = (static_cast<double>(batch_size) / static_cast<double>(config_.target_rate_mps)) * 1000000.0;
            
            if (elapsed_us < expected_us) {
                auto sleep_us = static_cast<int64_t>(expected_us - elapsed_us);
                if (sleep_us > 100) {
                    std::this_thread::sleep_for(std::chrono::microseconds(sleep_us));
                } else {
                    while (std::chrono::duration_cast<std::chrono::microseconds>(
                               std::chrono::high_resolution_clock::now() - last_rate_check_).count() < expected_us) {
                        #if defined(__x86_64__) || defined(_M_X64)
                        __builtin_ia32_pause();
                        #endif
                    }
                }
            }

            last_rate_check_ = std::chrono::high_resolution_clock::now();
            msgs_in_current_window_ = 0;
        }
    }
};

} // namespace itch
