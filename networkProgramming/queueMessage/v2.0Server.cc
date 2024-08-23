#include <asio.hpp>
#include <iostream>
#include <thread>
#include <atomic>
#include <string>
#include <vector>

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(asio::ip::tcp::socket socket)
        : socket_(std::move(socket)) {
        try {
            remote_endpoint_ = socket_.remote_endpoint();
        } catch (const std::exception& e) {
            std::cerr << "Error getting remote endpoint: " << e.what() << std::endl;
        }
    }

    void start() {
        doRead();
    }

private:
    void doRead() {
        auto self(shared_from_this());
        socket_.async_read_some(asio::buffer(data_, max_length),
            [this, self](std::error_code ec, std::size_t length) {
                if (!ec) {
                    std::string received_data(data_, length);
                    std::cout << "Received from " << remote_endpoint_ << ": " << received_data << std::endl;
                    
                    // Process the received data
                    std::string response = "Server processed: " + received_data;
                    
                    // Send the response back
                    doWrite(response);
                } else if (ec != asio::error::operation_aborted) {
                    std::cerr << "Read error: " << ec.message() << std::endl;
                }
            });
    }

    void doWrite(const std::string& message) {
        auto self(shared_from_this());
        asio::async_write(socket_, asio::buffer(message),
            [this, self](std::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    std::cout << "Response sent to " << remote_endpoint_ << std::endl;
                    socket_.close();
                } else if (ec != asio::error::operation_aborted) {
                    std::cerr << "Write error: " << ec.message() << std::endl;
                }
            });
    }

    asio::ip::tcp::socket socket_;
    asio::ip::tcp::endpoint remote_endpoint_;
    enum { max_length = 1024 };
    char data_[max_length];
};

class Server {
public:
    Server(asio::io_context& io_context, short port)
        : acceptor_(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)),
          is_running_(true) {
        doAccept();
    }

    void stop() {
        is_running_ = false;
        acceptor_.close();
    }

private:
    void doAccept() {
        acceptor_.async_accept(
            [this](std::error_code ec, asio::ip::tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket))->start();
                } else {
                    std::cerr << "Accept error: " << ec.message() << std::endl;
                }

                if (is_running_) {
                    doAccept();
                }
            });
    }

    asio::ip::tcp::acceptor acceptor_;
    std::atomic<bool> is_running_;
};

int main() {
    try {
        asio::io_context io_context;
        Server server(io_context, 8080);

        std::vector<std::thread> threads;
        for (int i = 0; i < 4; ++i) {
            threads.emplace_back([&io_context]() {
                io_context.run();
            });
        }

        std::cout << "Server is running. Press Enter to stop." << std::endl;
        std::cin.get();

        server.stop();
        io_context.stop();

        for (auto& t : threads) {
            t.join();
        }
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}