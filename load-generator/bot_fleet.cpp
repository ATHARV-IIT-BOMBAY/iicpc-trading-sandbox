#include <iostream>
#include <vector>
#include <chrono>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unordered_map>
#include <algorithm>
#include <netinet/tcp.h>

// --- UPGRADE: Custom Binary Protocol ---
#pragma pack(push, 1) 
struct BinaryOrder {
    uint64_t order_id;   
    char symbol[8];      
    uint8_t side;        
    double price;        
    uint32_t quantity;   
};                       
#pragma pack(pop)

const int NUM_SOCKETS = 200;      // Massive concurrency
const int ORDERS_PER_SOCKET = 25; // 200 * 25 = 5000 Total Orders
const char* TARGET_IP = "127.0.0.1";
const int TARGET_PORT = 9000;

// State tracker for our Event Loop
struct ConnectionState {
    int bot_id;
    int orders_sent = 0;
    int orders_acked = 0;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
    std::vector<uint64_t> latencies;
};

// --- HELPER: Set Socket to Non-Blocking ---
void set_non_blocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags != -1) {
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    }
}

int main() {
    std::cout << "--- ASYNC EVENT-DRIVEN LOAD GENERATOR BOOTING ---" << std::endl;
    
    int kq = kqueue();
    if (kq == -1) {
        perror("kqueue failed");
        return 1;
    }

    std::unordered_map<int, ConnectionState> active_connections;
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(TARGET_PORT);
    inet_pton(AF_INET, TARGET_IP, &serv_addr.sin_addr);

    std::cout << "[SYSTEM] Spawning " << NUM_SOCKETS << " asynchronous sockets..." << std::endl;

    // 1. Initialize all sockets
    for (int i = 0; i < NUM_SOCKETS; i++) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        set_non_blocking(sock);
        
        // --- THE HFT UPGRADE: Disable Nagle's Algorithm ---
        // Forces the OS to send the 29-byte struct instantly without buffering
        int flag = 1;
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
        
        // Non-blocking connect (will return EINPROGRESS, which is normal)
        connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
        
        active_connections[sock].bot_id = i;
        active_connections[sock].latencies.reserve(ORDERS_PER_SOCKET);

        // Tell the macOS kernel: Let me know when this socket is ready to WRITE
        // EV_ONESHOT means it triggers once and removes itself from the queue automatically
        struct kevent ev;
        EV_SET(&ev, sock, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, NULL);
        kevent(kq, &ev, 1, NULL, 0, NULL);
    }

    int sockets_remaining = NUM_SOCKETS;
    struct kevent events[1024];

    std::cout << "[SYSTEM] Entering kqueue Event Loop..." << std::endl;

    // 2. THE EVENT LOOP (Single-threaded, zero context switching)
    while (sockets_remaining > 0) {
        // Sleep until the kernel tells us a socket is ready
        int num_ready = kevent(kq, NULL, 0, events, 1024, NULL);
        
        for (int i = 0; i < num_ready; i++) {
            int sock = events[i].ident;
            ConnectionState& state = active_connections[sock];

            // KERNEL SAYS: READY TO WRITE
            if (events[i].filter == EVFILT_WRITE) {
                BinaryOrder payload;
                payload.order_id = static_cast<uint64_t>(state.bot_id * 1000000 + state.orders_sent); 
                strncpy(payload.symbol, "AAPL", sizeof(payload.symbol));
                payload.side = 0; 
                payload.price = 150.25;
                payload.quantity = 100;

                state.start_time = std::chrono::high_resolution_clock::now();
                send(sock, &payload, sizeof(BinaryOrder), 0);
                state.orders_sent++;

                // Now tell the kernel to watch for the ACK (Readiness to READ)
                struct kevent ev;
                EV_SET(&ev, sock, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, NULL);
                kevent(kq, &ev, 1, NULL, 0, NULL);
            } 
            // KERNEL SAYS: DATA ARRIVED (READ)
            else if (events[i].filter == EVFILT_READ) {
                uint64_t ack_id;
                int bytes = recv(sock, &ack_id, sizeof(ack_id), 0);
                
                auto end_time = std::chrono::high_resolution_clock::now();

                if (bytes == sizeof(uint64_t)) {
                    auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - state.start_time).count();
                    state.latencies.push_back(latency);
                    state.orders_acked++;
                }

                if (state.orders_acked == ORDERS_PER_SOCKET) {
                    close(sock);
                    sockets_remaining--;
                } else {
                    // Ready for the next order. Watch for WRITE again.
                    struct kevent ev;
                    EV_SET(&ev, sock, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, NULL);
                    kevent(kq, &ev, 1, NULL, 0, NULL);
                }
            }
        }
    }

    std::cout << "--- ALL BOTS DISENGAGED ---" << std::endl;

    // --- PHASE 3: THE TELEMETRY MATH ---
    std::vector<uint64_t> master_latencies;
    for (const auto& pair : active_connections) {
        const auto& bot_lats = pair.second.latencies;
        master_latencies.insert(master_latencies.end(), bot_lats.begin(), bot_lats.end());
    }

    if (master_latencies.empty()) {
        std::cout << "No data collected." << std::endl;
        return 0;
    }

    std::sort(master_latencies.begin(), master_latencies.end());
    size_t total_orders = master_latencies.size();
    uint64_t p50 = master_latencies[total_orders * 0.50];
    uint64_t p90 = master_latencies[total_orders * 0.90];
    uint64_t p99 = master_latencies[total_orders * 0.99];

    std::cout << "\n======================================" << std::endl;
    std::cout << "      FINAL TELEMETRY REPORT          " << std::endl;
    std::cout << "======================================" << std::endl;
    std::cout << " Total Orders Processed : " << total_orders << std::endl;
    std::cout << " p50 Latency (Median)   : " << p50 << " ns" << std::endl;
    std::cout << " p90 Latency            : " << p90 << " ns" << std::endl;
    std::cout << " p99 Latency (Tail)     : " << p99 << " ns" << std::endl;
    std::cout << "======================================\n" << std::endl;

    // --- PHASE 4: BEAM TO LEADERBOARD ---
    int api_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in api_addr;
    api_addr.sin_family = AF_INET;
    api_addr.sin_port = htons(8000); 
    inet_pton(AF_INET, "127.0.0.1", &api_addr.sin_addr);

    if (connect(api_sock, (struct sockaddr*)&api_addr, sizeof(api_addr)) == 0) {
        char json_payload[256];
        int json_len = snprintf(json_payload, sizeof(json_payload),
            "{\"p50\":%llu,\"p90\":%llu,\"p99\":%llu}", 
            (unsigned long long)p50, (unsigned long long)p90, (unsigned long long)p99);

        char http_request[1024];
        snprintf(http_request, sizeof(http_request),
            "POST /submit-telemetry/ HTTP/1.1\r\n"
            "Host: 127.0.0.1:8000\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n\r\n%s", 
            json_len, json_payload);

        send(api_sock, http_request, strlen(http_request), 0);
        close(api_sock);
        std::cout << "[SYSTEM] Telemetry successfully beamed to Live Leaderboard." << std::endl;
    }

    return 0;
}