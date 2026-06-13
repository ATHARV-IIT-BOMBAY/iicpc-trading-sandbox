#include <iostream>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <map>
#include <queue>

// --- The Exact Same Binary Protocol ---
#pragma pack(push, 1)
struct BinaryOrder {
    uint64_t order_id;
    char symbol[8];
    uint8_t side;        // 0 = BUY, 1 = SELL
    double price;
    uint32_t quantity;
};
#pragma pack(pop)

void set_non_blocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags != -1) {
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    }
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    set_non_blocking(server_fd);

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(9000); 

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        return 1;
    }
    
    listen(server_fd, 1000);

    std::cout << "[CONTESTANT ENGINE] Quantitative Limit Order Book Online. Port 9000..." << std::endl;

    std::vector<struct pollfd> fds;
    fds.push_back({server_fd, POLLIN, 0});

    uint64_t total_processed = 0;

    // --- THE ORDER BOOK DATA STRUCTURES ---
    // BUY Book: Highest prices at the top
    std::map<double, std::queue<BinaryOrder>, std::greater<double>> buy_book;
    // SELL Book: Lowest prices at the top
    std::map<double, std::queue<BinaryOrder>> sell_book;

    // The Single-Threaded Event Loop
    while (true) {
        int ret = poll(fds.data(), fds.size(), -1);
        if (ret < 0) break;

        for (size_t i = 0; i < fds.size(); i++) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == server_fd) {
                    // Accept a new bot connection from the Fleet
                    int new_socket = accept(server_fd, nullptr, nullptr);
                    if (new_socket >= 0) {
                        set_non_blocking(new_socket);
                        int flag = 1;
                        setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
                        fds.push_back({new_socket, POLLIN, 0});
                    }
                } else {
                    // Read the raw memory payload
                    BinaryOrder order;
                    int valread = recv(fds[i].fd, &order, sizeof(BinaryOrder), 0);
                    
                    if (valread == sizeof(BinaryOrder)) {
                        total_processed++;
                        
                        // --- THE MATCHING ALGORITHM ---
                        if (order.side == 0) { 
                            // 1. INCOMING BUY ORDER
                            while (order.quantity > 0 && !sell_book.empty()) {
                                auto best_sell = sell_book.begin();
                                if (best_sell->first > order.price) break; // Seller wants too much

                                auto& sell_queue = best_sell->second;
                                BinaryOrder& resting_sell = sell_queue.front();

                                if (resting_sell.quantity <= order.quantity) {
                                    order.quantity -= resting_sell.quantity;
                                    sell_queue.pop();
                                    if (sell_queue.empty()) sell_book.erase(best_sell);
                                } else {
                                    resting_sell.quantity -= order.quantity;
                                    order.quantity = 0;
                                }
                            }
                            if (order.quantity > 0) {
                                buy_book[order.price].push(order);
                            }

                        } else { 
                            // 2. INCOMING SELL ORDER
                            while (order.quantity > 0 && !buy_book.empty()) {
                                auto best_buy = buy_book.begin();
                                if (best_buy->first < order.price) break; // Buyer offering too little

                                auto& buy_queue = best_buy->second;
                                BinaryOrder& resting_buy = buy_queue.front();

                                if (resting_buy.quantity <= order.quantity) {
                                    order.quantity -= resting_buy.quantity;
                                    buy_queue.pop();
                                    if (buy_queue.empty()) buy_book.erase(best_buy);
                                } else {
                                    resting_buy.quantity -= order.quantity;
                                    order.quantity = 0;
                                }
                            }
                            if (order.quantity > 0) {
                                sell_book[order.price].push(order);
                            }
                        }
                        
                        // 3. Fire the 8-byte Binary ACK back to the Fleet
                        send(fds[i].fd, &order.order_id, sizeof(uint64_t), 0);
                        
                    } else if (valread <= 0) {
                        // Bot disconnected
                        close(fds[i].fd);
                        fds.erase(fds.begin() + i);
                        i--;
                    }
                }
            }
        }
        
        if (total_processed >= 5000) {
            std::cout << "[CONTESTANT ENGINE] Processed 5000 orders. LOB Clean Shutdown." << std::endl;
            break;
        }
    }
    
    return 0;
}