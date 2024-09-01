#include <asio.hpp>
#include <queue>
#include <string>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <memory>
#include <iostream>
#include <unordered_map>
#include <future>

class ClientConnection : public std::enable_shared_from_this<ClientConnection> {
public:
    ClientConnection(asio::io_context& io_context, const std::string& ip, unsigned short port)
        : io_context_(io_context), socket_(io_context), timer_(io_context),
          endpoint_(asio::ip::make_address(ip), port) {}

    void connect() {
        auto self(shared_from_this());
        std::cout << "33333" << std::endl;
        socket_.async_connect(endpoint_,
            [this, self](const asio::error_code& ec) {
                if (!ec) {
                    std::cout << "Connected to " << endpoint_ << std::endl;
                } else {
                    std::cout << "Connection failed: " << ec.message() << std::endl;
                }
            });
    }

    std::future<bool> async_write(std::string message) {
        auto promise = std::make_shared<std::promise<bool>>();
        auto future = promise->get_future();

        auto self(shared_from_this());
        asio::async_write(socket_, asio::buffer(message),
            [this, self, promise](std::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    std::cout << "Message sent to " << endpoint_ << std::endl;
                    promise->set_value(true);
                } else {
                    std::cout << "Write failed: " << ec.message() << std::endl;
                    promise->set_value(false);
                    close();
                }
                timer_.cancel();
            });

        // 设置定时器
        timer_.expires_after(std::chrono::seconds(5));
        timer_.async_wait([this, self, promise](const asio::error_code& ec) {
            if (!ec) {
                std::cout << "Write operation timed out for " << endpoint_ << std::endl;
                promise->set_value(false);
                // close();
            }
        });

        return future;
    }

    asio::ip::tcp::socket& socket() { return socket_; }
    const asio::ip::tcp::endpoint& endpoint() const { return endpoint_; }

private:
    void close() {
        asio::error_code ec;
        socket_.close(ec);
    }

    asio::io_context& io_context_;
    asio::ip::tcp::socket socket_;
    asio::steady_timer timer_;
    asio::ip::tcp::endpoint endpoint_;
};

class Server {
public:
    Server(asio::io_context& io_context)
        : io_context_(io_context) {}

    void start_message_processing_thread() {
        message_thread_ = std::thread([this]() {
            message_processing_thread();
        });
    }

    void add_message(const std::string& ip, unsigned short port, std::string message) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            message_queue_.push({ip, port, std::move(message)});
        }
        std::cout << "通知别人啦" << std::endl;
        queue_condition_.notify_one();
    }

    void stop() {
        stop_flag_ = true;
        queue_condition_.notify_all();
        if (message_thread_.joinable()) {
            message_thread_.join();
        }
    }

private:
    struct Message {
        std::string ip;
        unsigned short port;
        std::string content;
    };

    void message_processing_thread() {
        while (!stop_flag_) {
            Message msg;
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                queue_condition_.wait(lock, [this] { return !message_queue_.empty() || stop_flag_; });
                if (stop_flag_) break;
                msg = std::move(message_queue_.front());
                message_queue_.pop();
            }

            process_message(msg);
        }
    }

    void process_message(const Message& msg) {
        std::string key = msg.ip + ":" + std::to_string(msg.port);
        auto it = connections_.find(key);
        if (it == connections_.end()) {
            // 如果连接不存在，创建新的连接
            auto connection = std::make_shared<ClientConnection>(io_context_, msg.ip, msg.port);
            connections_[key] = connection;
            connection->connect();
            std::cout << "22222222222" << std::endl;
            it = connections_.find(key);
        }
        std::cout << "111111111111" << std::endl;
        auto future = std::make_shared<std::future<bool>>(it->second->async_write(msg.content));

        asio::post(io_context_, [future, key, it]()
        {
            bool result = future->get();
           if(result) 
           {
                std::cout << "Message successfully sent to " << key << std::endl;
           }
           else
           {
                std::cout << "Failed to send message to " << key << std::endl;
                it->second.
           }
        });
    }

    asio::io_context& io_context_;
    std::unordered_map<std::string, std::shared_ptr<ClientConnection>> connections_;
    std::queue<Message> message_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_condition_;
    std::thread message_thread_;
    std::atomic<bool> stop_flag_{false};
};

#include <asio.hpp>
// ... 其他包含的头文件 ...

int main() {
    try {
        asio::io_context io_context;
        
        // 使用 executor_work_guard 来保持 io_context 运行
        auto work_guard = asio::make_work_guard(io_context);

        std::thread io_thread([&io_context]() {
            io_context.run();
        });

        Server server(io_context);
        
        server.start_message_processing_thread();

        // 模拟添加消息
        std::this_thread::sleep_for(std::chrono::seconds(2));
        for (int i = 0; i < 10; ++i) {
            if(!io_context.stopped()) {
                std::cout << "io_context is running" << std::endl;
            } else {
                std::cout << "io_context has stopped" << std::endl;
            }
            server.add_message("127.0.0.1", 8080, "Hello, World! " + std::to_string(i));
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // 停止服务器
        std::cout << "Stopping server..." << std::endl;
        server.stop();

        // 重置 work_guard，允许 io_context 在完成所有任务后停止
        work_guard.reset();

        io_thread.join();
        std::cout << "Server stopped." << std::endl;
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
