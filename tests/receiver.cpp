#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <csignal>
#include <atomic>
#include <iomanip>
#include <cstring>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "itch/types.hpp"

std::atomic<bool> g_running{true};

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_running = false;
    }
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::string ip = "239.255.0.1";
    uint16_t port = 12345;
    bool is_multicast = true;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--ip" && i + 1 < argc) ip = argv[++i];
        else if (arg == "--port" && i + 1 < argc) port = static_cast<uint16_t>(std::stoi(argv[++i]));
        else if (arg == "--unicast") is_multicast = false;
    }

    std::cout << "[RECEIVER] Starting ITCH Client Receiver on " << ip << ":" << port 
              << " (" << (is_multicast ? "Multicast" : "Unicast") << ")...\n";

    int sock = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    int reuse = 1;
    ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    int rcvbuf = 16 * 1024 * 1024;
    ::setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    sockaddr_in bind_addr{};
    std::memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(port);
    bind_addr.sin_addr.s_addr = is_multicast ? INADDR_ANY : inet_addr(ip.c_str());

    if (::bind(sock, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    if (is_multicast) {
        ip_mreq mreq{};
        mreq.imr_multiaddr.s_addr = inet_addr(ip.c_str());
        mreq.imr_interface.s_addr = INADDR_ANY;
        if (::setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
            perror("IP_ADD_MEMBERSHIP failed");
            return 1;
        }
    }

    uint8_t buffer[65536];
    uint64_t total_packets = 0;
    uint64_t total_messages = 0;
    uint64_t total_bytes = 0;

    uint64_t system_events = 0;
    uint64_t dir_events = 0;
    uint64_t add_events = 0;
    uint64_t exec_events = 0;
    uint64_t replace_events = 0;
    uint64_t delete_events = 0;

    auto start_time = std::chrono::high_resolution_clock::now();
    auto last_report = start_time;

    while (g_running) {
        ssize_t bytes_read = ::recv(sock, buffer, sizeof(buffer), 0);
        if (bytes_read <= 0) continue;

        total_packets++;
        total_bytes += bytes_read;

        // Decode MoldUDP64 or Raw
        if (bytes_read >= static_cast<ssize_t>(sizeof(itch::MoldUDP64Header))) {
            auto* mold_hdr = reinterpret_cast<itch::MoldUDP64Header*>(buffer);
            uint16_t msg_count = itch::be16_to_host(mold_hdr->message_count);
            
            if (msg_count > 0 && msg_count < 1000) {
                total_messages += msg_count;
                size_t offset = sizeof(itch::MoldUDP64Header);
                
                for (uint16_t i = 0; i < msg_count && offset < static_cast<size_t>(bytes_read); ++i) {
                    if (offset + sizeof(itch::MoldUDP64BlockHeader) > static_cast<size_t>(bytes_read)) break;
                    
                    auto* block = reinterpret_cast<itch::MoldUDP64BlockHeader*>(buffer + offset);
                    uint16_t msg_len = itch::be16_to_host(block->message_length);
                    offset += sizeof(itch::MoldUDP64BlockHeader);

                    if (offset + msg_len > static_cast<size_t>(bytes_read)) break;

                    char msg_type = static_cast<char>(buffer[offset]);
                    switch (msg_type) {
                        case 'S': system_events++; break;
                        case 'R': dir_events++; break;
                        case 'A': add_events++; break;
                        case 'E': exec_events++; break;
                        case 'U': replace_events++; break;
                        case 'D': delete_events++; break;
                    }
                    offset += msg_len;
                }
            } else {
                // Raw payload single message
                total_messages++;
                char msg_type = static_cast<char>(buffer[0]);
                switch (msg_type) {
                    case 'S': system_events++; break;
                    case 'R': dir_events++; break;
                    case 'A': add_events++; break;
                    case 'E': exec_events++; break;
                    case 'U': replace_events++; break;
                    case 'D': delete_events++; break;
                }
            }
        }

        auto now = std::chrono::high_resolution_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_report).count() >= 2) {
            double elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - start_time).count() / 1000000.0;
            double mps = total_messages / elapsed;
            std::cout << "[RECEIVER STATS] Total Msgs: " << total_messages 
                      << " | Avg MPS: " << std::fixed << std::setprecision(0) << mps
                      << " | Add: " << add_events << " Exec: " << exec_events
                      << " Del: " << delete_events << " Repl: " << replace_events << "\n";
            last_report = now;
        }
    }

    ::close(sock);
    std::cout << "[RECEIVER] Shut down cleanly. Total Messages Received: " << total_messages << "\n";
    return 0;
}
