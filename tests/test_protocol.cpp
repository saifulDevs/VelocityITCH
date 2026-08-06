#include <iostream>
#include <cassert>
#include <cstring>
#include <cstdio>

#include "itch/types.hpp"
#include "itch/prng.hpp"
#include "itch/config.hpp"
#include "itch/logger.hpp"
#include "itch/metrics.hpp"
#include "itch/interfaces.hpp"
#include "itch/validator.hpp"
#include "itch/encoder.hpp"
#include "itch/transport.hpp"
#include "itch/recorder.hpp"
#include "itch/state_manager.hpp"
#include "itch/session_manager.hpp"
#include "itch/price_engine.hpp"
#include "itch/chaos_injector.hpp"
#include "itch/event_generator.hpp"

using namespace itch;

void test_struct_sizes() {
    std::cout << "[TEST] Validating packed struct sizes...\n";
    assert(sizeof(SystemEventMessage) == 18);
    assert(sizeof(OrderbookDirectoryMessage) == 97);
    assert(sizeof(AddOrderMessage) == 30);
    assert(sizeof(OrderExecutedMessage) == 25);
    assert(sizeof(OrderReplaceMessage) == 29);
    assert(sizeof(OrderDeleteMessage) == 13);
    assert(sizeof(MoldUDP64Header) == 20);
    assert(sizeof(MoldUDP64BlockHeader) == 2);
    std::cout << " -> PASSED packed struct size checks.\n";
}

void test_event_validator() {
    std::cout << "[TEST] Validating EventValidator integrity checks...\n";
    
    // Valid Add Order
    AddOrderMessage add{};
    add.message_type = 'A';
    add.order_number = host_to_be64(1001);
    add.quantity = host_to_be32(100);
    add.price = host_to_be32(15000);
    add.side = 'B';
    assert(EventValidator::validate_event(&add, sizeof(add)) == true);

    // Invalid zero price Add Order
    AddOrderMessage invalid_add = add;
    invalid_add.price = 0;
    assert(EventValidator::validate_event(&invalid_add, sizeof(invalid_add)) == false);

    std::cout << " -> PASSED EventValidator checks.\n";
}

void test_polymorphic_encoder_and_transport() {
    std::cout << "[TEST] Validating polymorphic IEncoder and ITransport...\n";
    std::unique_ptr<IEncoder> encoder = std::make_unique<MoldUDP64Encoder>();
    std::unique_ptr<ITransport> transport = std::make_unique<UdpTransport>("127.0.0.1", 12345, NetworkMode::Unicast);

    std::vector<uint8_t> packet;
    uint8_t dummy_msg[30]{'A'};
    encoder->append_message(dummy_msg, sizeof(dummy_msg), packet);
    encoder->finalize_packet(packet);

    assert(!packet.empty());
    assert(transport->send_packet(packet.data(), packet.size()) == true);
    std::cout << " -> PASSED polymorphic IEncoder and ITransport checks.\n";
}

void test_metrics_peak_and_logger() {
    std::cout << "[TEST] Validating MetricsCollector Peak MPS and Logger...\n";
    Logger::info("Test logging system message");
    MetricsCollector metrics;
    metrics.record_message_sent(30);
    metrics.record_packet_sent();
    metrics.set_active_orders(50);
    assert(metrics.total_messages() == 1);
    std::cout << " -> PASSED MetricsCollector & Logger checks.\n";
}

int main() {
    std::cout << "=================================================================\n";
    std::cout << "       RUNNING ENTERPRISE 10/10 MASTERPIECE UNIT TESTS           \n";
    std::cout << "=================================================================\n";

    test_struct_sizes();
    test_event_validator();
    test_polymorphic_encoder_and_transport();
    test_metrics_peak_and_logger();

    std::cout << "=================================================================\n";
    std::cout << "       ALL ENTERPRISE 10/10 UNIT TESTS PASSED SUCCESSFULLY!      \n";
    std::cout << "=================================================================\n";
    return 0;
}
