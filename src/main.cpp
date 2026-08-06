#include <iostream>
#include <string>
#include <csignal>
#include <atomic>

#include "itch/config.hpp"
#include "itch/scheduler.hpp"

std::atomic<bool> g_running{true};

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_running = false;
    }
}

void print_usage(const char* prog) {
    std::cout << "Dhaka Stock Exchange (DSE) Synthetic ITCH Generator (SOLID Refactored Architecture)\n"
              << "Usage: " << prog << " [options]\n\n"
              << "Network & Performance:\n"
              << "  --ip <address>          Target IP (Default: 239.255.0.1)\n"
              << "  --port <number>         Target UDP Port (Default: 12345)\n"
              << "  --mode <multi|uni>      Network mode: 'multicast' or 'unicast' (Default: multicast)\n"
              << "  --framing <mold|raw>    Framing: 'moldudp64' or 'raw' (Default: moldudp64)\n"
              << "  --rate <mps>            Target MPS (0 = Max throughput, Default: 1000000)\n"
              << "  --count <num>           Total message limit (0 = Continuous stream, Default: 0)\n\n"
              << "Market Kinetics & Session Lifecycle:\n"
              << "  --replay <file.itch>    Replay historical binary ITCH capture log\n"
              << "  --replay-speed <speed>  Playback speed (1.0x, 2.0x, 0.0 = Max speed, Default: 1.0)\n"
              << "  --record <file.itch>    Record generated ITCH stream to binary file for future replay\n"
              << "  --session-flow          Simulate market phase lifecycle (PreOpen -> Auction -> Continuous -> Halt -> Close)\n"
              << "  --price-walk            Enable Random Walk & Mean Reversion price movement model\n"
              << "  --diurnal               Enable intraday U-shaped volume throughput profile\n\n"
              << "Network Chaos & Fault Injection:\n"
              << "  --drop-rate <pct>       Packet loss percentage (0.0 to 100.0%)\n"
              << "  --seq-gap <step>        Inject MoldUDP64 sequence gap every N packets\n"
              << "  --jitter-us <micros>    Inject network latency jitter delay (microseconds)\n"
              << "  --help                  Display this help message\n";
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    itch::Config config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--ip" && i + 1 < argc) {
            config.ip = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            config.port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--mode" && i + 1 < argc) {
            std::string m = argv[++i];
            if (m == "unicast" || m == "uni") config.net_mode = itch::NetworkMode::Unicast;
            else config.net_mode = itch::NetworkMode::Multicast;
        } else if (arg == "--framing" && i + 1 < argc) {
            std::string f = argv[++i];
            if (f == "raw") config.framing_mode = itch::FramingMode::Raw;
            else config.framing_mode = itch::FramingMode::MoldUDP64;
        } else if (arg == "--rate" && i + 1 < argc) {
            config.target_rate_mps = std::stoull(argv[++i]);
        } else if (arg == "--count" && i + 1 < argc) {
            config.max_count = std::stoull(argv[++i]);
        } else if (arg == "--replay" && i + 1 < argc) {
            config.replay_file = argv[++i];
        } else if (arg == "--replay-speed" && i + 1 < argc) {
            config.replay_speed = std::stod(argv[++i]);
        } else if (arg == "--record" && i + 1 < argc) {
            config.record_file = argv[++i];
        } else if (arg == "--session-flow") {
            config.enable_session_flow = true;
        } else if (arg == "--price-walk") {
            config.enable_price_walk = true;
        } else if (arg == "--diurnal") {
            config.enable_diurnal = true;
        } else if (arg == "--drop-rate" && i + 1 < argc) {
            config.packet_drop_rate = std::stod(argv[++i]);
        } else if (arg == "--seq-gap" && i + 1 < argc) {
            config.sequence_gap_step = std::stoull(argv[++i]);
        } else if (arg == "--jitter-us" && i + 1 < argc) {
            config.max_jitter_us = std::stoull(argv[++i]);
        } else if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
    }

    try {
        itch::EngineScheduler scheduler(config);
        scheduler.run(g_running);
    } catch (const std::exception& ex) {
        std::cerr << "[ERROR] Engine exception: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
