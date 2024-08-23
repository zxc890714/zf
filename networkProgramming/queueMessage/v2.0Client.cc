#include <asio.hpp>
#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <string>

struct Message {
    std::string data;
    std::string sender_ip;
};

class ClientThread {
public:
    ClientThread(asio::io_context& io_context, const std::string& server_host, unsigned short server_port)
        : io_context_(io_context),
          server_endpoint_(asio::ip::address::from_string(server_host), server_port),
          is_running_(true) {
    }

    void run() {
        while (is_running_) {
            try {
                io_context_.run();
                break; // io_context.run() exited normally
            } catch (const std::exception& e) {
                std::cerr << "Exception in client thread: " << e.what() << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }

    void stop() {
        is_running_ = false;
        io_context_.stop();
    }

    void sendMessage(const Message& message) {
        asio::post(io_context_, [this, message]() {
            auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);
            socket->async_connect(server_endpoint_, [this, socket, message](const asio::error_code& error) {
                if (!error) {
                    doWrite(socket, message);
                } else {
                    std::cerr << "Connection failed: " << error.message() << std::endl;
                }
            });
        });
    }

private:
    void doWrite(std::shared_ptr<asio::ip::tcp::socket> socket, const Message& message) {
        asio::async_write(*socket,
            asio::buffer(message.data),
            [this, socket](const asio::error_code& error, std::size_t /*bytes_transferred*/) {
                if (!error) {
                    std::cout << "Message sent successfully" << std::endl;
                } else {
                    std::cerr << "Write error: " << error.message() << std::endl;
                }
                socket->close();
            });
    }

    asio::io_context& io_context_;
    asio::ip::tcp::endpoint server_endpoint_;
    std::atomic<bool> is_running_;
};

class MessageServer {
public:
    MessageServer(asio::io_context& io_context, const std::string& server_host, unsigned short server_port)
        : io_context_(io_context),
          work_guard_(asio::make_work_guard(io_context)),
          signals_(io_context, SIGINT, SIGTERM),
          is_running_(true),
          client_thread_(io_context, server_host, server_port) {

        signals_.async_wait([this](const asio::error_code& error, int signal_number) {
            if (!error) {
                std::cout << "Received signal " << signal_number << ", initiating shutdown..." << std::endl;
                stop();
            }
        });
    }

    void run() {
        std::cout << "Server is running. Press Ctrl+C to stop." << std::endl;

        // 启动客户端线程
        std::thread client([this]() { client_thread_.run(); });

        // 启动消息处理线程
        std::thread message_thread([this]() { processMessages(); });

        while (is_running_) {
            try {
                io_context_.run();
                break; // io_context.run() exited normally
            }
            catch (const std::exception& e) {
                std::cerr << "Exception in io_context.run(): " << e.what() << std::endl;
            }
        }

        client.join();
        message_thread.join();
        std::cout << "Server has stopped." << std::endl;
    }

    void stop() {
        if (!is_running_.exchange(false)) {
            return; // 已经在停止过程中
        }

        std::cout << "Stopping server..." << std::endl;

        client_thread_.stop();

        work_guard_.reset();

        std::this_thread::sleep_for(std::chrono::seconds(1));

        io_context_.stop();

        // 通知消息处理线程退出
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            message_queue_.push({.data = "EXIT", .sender_ip = ""});
        }
        queue_condition_.notify_one();
    }

    void onMessage(const std::string& data, const std::string& sender_ip) {
        // 在这里处理接收到的数据
        std::cout << "Received message from " << sender_ip << ": " << data << std::endl;

        // 处理数据并放入消息队列
        std::string processed_data = processData(data);
        enqueueMessage(processed_data, sender_ip);
    }

private:
    std::string processData(const std::string& data) {
        // 在这里实现数据处理逻辑
        // 这里只是一个简单的示例，实际应用中可能需要更复杂的处理
        return "Processed: " + data;
    }

    void enqueueMessage(const std::string& message, const std::string& sender_ip) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        message_queue_.push({.data = message, .sender_ip = sender_ip});
        queue_condition_.notify_one();
    }

    void processMessages() {
        while (is_running_) {
            Message message;
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                queue_condition_.wait(lock, [this] { 
                    return !message_queue_.empty() || !is_running_; 
                });

                if (!is_running_ && message_queue_.empty()) {
                    break;
                }

                message = std::move(message_queue_.front());
                message_queue_.pop();
            }

            if (message.data == "EXIT") {
                break;
            }

            sendMessage(message);
        }
    }

    void sendMessage(const Message& message) {
        // 使用客户端线程发送消息
        client_thread_.sendMessage(message);
    }

    asio::io_context& io_context_;
    asio::executor_work_guard<asio::io_context::executor_type> work_guard_;
    asio::signal_set signals_;
    std::atomic<bool> is_running_;

    std::queue<Message> message_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_condition_;

    ClientThread client_thread_;
};

int main() {
    try {
        asio::io_context io_context;
        MessageServer server(io_context, "127.0.0.1", 8080);  // 假设目标服务器在本地8080端口

        std::thread io_thread([&server]() {
            server.run();
        });

        // 模拟接收消息
        for (int i = 0; i < 10; ++i) {
            server.onMessage("Test message " + std::to_string(i), "192.168.1." + std::to_string(i));
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        io_thread.join();
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}