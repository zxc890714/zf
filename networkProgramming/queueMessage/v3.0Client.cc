#include <asio.hpp>
#include <iostream>
#include <thread>
#include <string>
#include <atomic>

class Client {
public:
    Client(asio::io_context& io_context, const std::string& host, const std::string& port)
        : io_context_(io_context), socket_(io_context), udp_socket_(io_context),
          is_running_(true) {
        tcp_resolver_ = std::make_unique<asio::ip::tcp::resolver>(io_context);
        tcp_endpoints_ = tcp_resolver_->resolve(host, port);
        
        udp_socket_.open(asio::ip::udp::v4());
        udp_socket_.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), 12345));
        
        startUdpReceive();
    }

    void stop() {
        is_running_ = false;
        udp_socket_.close();
    }

    void onMessage(const std::string& message, const std::string& source_ip) {
        asio::async_connect(socket_, tcp_endpoints_,
            [this, message, source_ip](std::error_code ec, asio::ip::tcp::endpoint) {
                if (!ec) {
                    std::string full_message = source_ip + ": " + message;
                    doWrite(full_message);
                } else {
                    std::cout << "Connect error: " << ec.message() << std::endl;
                }
            });
    }

private:
    void doWrite(const std::string& message) {
        asio::async_write(socket_, asio::buffer(message),
            [this](std::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    doRead();
                } else {
                    std::cout << "Write error: " << ec.message() << std::endl;
                }
            });
    }

    void doRead() {
        socket_.async_read_some(asio::buffer(data_, max_length),
            [this](std::error_code ec, std::size_t length) {
                if (!ec) {
                    std::cout << "Received from server: " << std::string(data_, length) << std::endl;
                    socket_.close();
                } else {
                    std::cout << "Read error: " << ec.message() << std::endl;
                }
            });
    }

    void startUdpReceive() {
        udp_socket_.async_receive_from(
            asio::buffer(udp_data_, max_length), udp_sender_endpoint_,
            [this](std::error_code ec, std::size_t bytes_recvd) {
                if (!ec && bytes_recvd > 0) {
                    std::string message(udp_data_, bytes_recvd);
                    std::string sender_ip = udp_sender_endpoint_.address().to_string();
                    std::cout << "Received UDP message from " << sender_ip << ": " << message << std::endl;
                    onMessage(message, sender_ip);
                }

                if (is_running_) {
                    startUdpReceive();
                }
            });
    }

    asio::io_context& io_context_;
    asio::ip::tcp::socket socket_;
    std::unique_ptr<asio::ip::tcp::resolver> tcp_resolver_;
    asio::ip::tcp::resolver::results_type tcp_endpoints_;
    
    asio::ip::udp::socket udp_socket_;
    asio::ip::udp::endpoint udp_sender_endpoint_;
    
    std::atomic<bool> is_running_;
    enum { max_length = 1024 };
    char data_[max_length];
    char udp_data_[max_length];
};

int main() {
    try {
        asio::io_context io_context;
        Client client(io_context, "localhost", "8080");

        std::thread t([&io_context]() { io_context.run(); });

        std::cout << "Client is running. Press Enter to stop." << std::endl;
        std::cin.get();

        client.stop();
        io_context.stop();
        t.join();
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}