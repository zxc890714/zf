#include <iostream>
#include <thread>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

class AsioClient {
public:
    AsioClient(const std::string& host, const std::string& port)
        : io_context_(), resolver_(io_context_), socket_(io_context_), host_(host), port_(port) {
    }

    void start() {
        // �ڶ����߳�������io_context
        io_thread_ = std::thread([this]() { io_context_.run(); });

        // �첽������������ַ�Ͷ˿�
        tcp::resolver::query query(host_, port_);
        resolver_.async_resolve(query,
            [this](const boost::system::error_code& ec, tcp::resolver::results_type results) {
                if (!ec) {
                    async_connect(results);
                } else {
                    std::cerr << "Resolve error: " << ec.message() << "\n";
                }
            }
        );
    }

    void stop() {
        // ֹͣio_context���ȴ��߳̽���
        io_context_.stop();
        if (io_thread_.joinable()) {
            io_thread_.join();
        }
    }

private:
    void async_connect(const tcp::resolver::results_type& endpoints) {
        // �첽���ӷ�����
        boost::asio::async_connect(socket_, endpoints,
            [this](const boost::system::error_code& ec, const tcp::endpoint&) {
                if (!ec) {
                    send_message("Hello from client!\n");
                } else {
                    std::cerr << "Connect error: " << ec.message() << "\n";
                }
            }
        );
    }

    void send_message(const std::string& message) {
        // �첽������Ϣ
        boost::asio::async_write(socket_, boost::asio::buffer(message),
            [this](const boost::system::error_code& ec, std::size_t /*length*/) {
                if (!ec) {
                    receive_message();
                } else {
                    std::cerr << "Send error: " << ec.message() << "\n";
                }
            }
        );
    }

    void receive_message() {
        // �첽������Ϣ
        boost::asio::async_read(socket_, boost::asio::buffer(reply_, 1024),
            [this](const boost::system::error_code& ec, std::size_t length) {
                if (!ec) {
                    std::cout << "Reply from server: " << std::string(reply_, length) << "\n";
                } else {
                    std::cerr << "Receive error: " << ec.message() << "\n";
                }
            }
        );
    }

    boost::asio::io_context io_context_;
    tcp::resolver resolver_;
    tcp::socket socket_;
    char reply_[1024];
    std::thread io_thread_;
    std::string host_;
    std::string port_;
};

int main() {
    try {
        // �����ͻ��˶���
        AsioClient client("localhost", "8080");

        // �����ͻ���
        client.start();

        // �ȴ��û�������ֹͣ�ͻ���
        std::cout << "Press Enter to exit...\n";
        std::cin.get();

        // ֹͣ�ͻ���
        client.stop();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}