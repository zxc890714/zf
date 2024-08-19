#include <iostream>
#include <queue>
#include <thread>
#include <mutex>
#include <chrono>
#include <memory>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;
using namespace std::chrono_literals;

class MessageHandler : public std::enable_shared_from_this<MessageHandler> {
public:
    MessageHandler(const std::string& ip, const std::string& port, int timeout_seconds)
        : timeout_duration_(timeout_seconds), ip_(ip), port_(port) {}

    void init() {
        io_context_ = std::make_shared<boost::asio::io_context>();
        socket_ = std::make_shared<tcp::socket>(*io_context_);
        timer_ = std::make_shared<boost::asio::steady_timer>(*io_context_);
        
        connect(ip_, port_);
    }

    void enqueueMessage(const std::string& message) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        message_queue_.push(message);
        resetTimeout();
    }

    void processMessages() {
        timer_->expires_after(timeout_duration_);
        timer_->async_wait([self = shared_from_this()](const boost::system::error_code& ec) {
            if (!ec) {
                std::cout << "Timeout reached, disconnecting." << std::endl;
                self->socket_->close();
            }
        });

        std::thread([self = shared_from_this()] {
            while (true) {
                std::string message;
                {
                    std::lock_guard<std::mutex> lock(self->queue_mutex_);
                    if (self->message_queue_.empty()) continue;
                    message = self->message_queue_.front();
                    self->message_queue_.pop();
                }
                self->sendMessage(message);
            }
        }).detach();
    }

    void run() {
        io_context_->run();
    }

private:
    void connect(const std::string& ip, const std::string& port) {
        tcp::endpoint endpoint(boost::asio::ip::make_address(ip), std::stoi(port));
        socket_->async_connect(endpoint,
            [self = shared_from_this()](boost::system::error_code ec) {
                if (!ec) {
                    std::cout << "Connected successfully" << std::endl;
                    self->processMessages();
                } else {
                    std::cerr << "Connect failed: " << ec.message() << std::endl;
                }
            });
    }

    void sendMessage(const std::string& message) {
        boost::asio::async_write(*socket_, boost::asio::buffer(message),
            [](boost::system::error_code ec, std::size_t /*length*/) {
                if (ec) {
                    std::cerr << "Send failed: " << ec.message() << std::endl;
                }
            });
    }

    void resetTimeout() {
        timer_->expires_after(timeout_duration_);
    }

    std::shared_ptr<boost::asio::io_context> io_context_;
    std::shared_ptr<tcp::socket> socket_;
    std::shared_ptr<boost::asio::steady_timer> timer_;
    std::queue<std::string> message_queue_;
    std::mutex queue_mutex_;
    std::chrono::seconds timeout_duration_;
    std::string ip_;
    std::string port_;
};

int main() {
    auto handler = std::make_shared<MessageHandler>("127.0.0.1", "12345", 10); // 10 seconds timeout
    handler->init();
    handler->run();

    // Simulate message receiving
    for (int i = 0; i < 50; ++i) {
        handler->enqueueMessage("Message " + std::to_string(i));
    }

    return 0;
}