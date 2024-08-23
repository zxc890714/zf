#include <asio.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>

class SimulatorClient {
public:
    SimulatorClient(asio::io_context& io_context, const std::string& host, unsigned short port)
        : socket_(io_context, asio::ip::udp::endpoint(asio::ip::udp::v4(), 0)),
          endpoint_(asio::ip::address::from_string(host), port) {}

    void send(const std::string& message) {
        socket_.send_to(asio::buffer(message), endpoint_);
    }

private:
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint endpoint_;
};

void clientThread(int id, SimulatorClient& client, std::atomic<bool>& running) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 5000);  // Random delay between 1 to 5 seconds

    while (running) {
        std::string message = "Message from client " + std::to_string(id);
        client.send(message);
        std::cout << "Client " << id << " sent: " << message << std::endl;

        // Random delay before sending next message
        std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));
    }
}

int main() {
    try {
        asio::io_context io_context;
        SimulatorClient client(io_context, "127.0.0.1", 12345);  // 发送到 localhost 的 12345 端口

        std::thread io_thread([&io_context]() { io_context.run(); });

        std::atomic<bool> running(true);
        std::vector<std::thread> client_threads;

        int num_clients = 5;  // 模拟 5 个并发客户端
        for (int i = 0; i < num_clients; ++i) {
            client_threads.emplace_back(clientThread, i, std::ref(client), std::ref(running));
        }

        std::cout << "Simulating " << num_clients << " concurrent clients. Press Enter to stop." << std::endl;
        std::cin.get();

        running = false;
        for (auto& t : client_threads) {
            t.join();
        }

        io_context.stop();
        io_thread.join();
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}