#include <iostream>
#include <boost/asio.hpp>
#include <thread>

using boost::asio::ip::tcp;

#define SERVER_PORT 8080

void handle_client(tcp::socket socket) {
    try {
        std::cout << "Client connected" << std::endl;

        // 读取客户端消息
        char data[1024] = {0};
        boost::system::error_code error;
        size_t length = socket.read_some(boost::asio::buffer(data), error);

        if (error == boost::asio::error::eof) {
            std::cout << "Connection closed cleanly by peer" << std::endl;
        } else if (error) {
            throw boost::system::system_error(error);
        }

        std::cout << "Received message: " << std::string(data, length) << std::endl;

        // 发送响应到客户端
        const std::string response = "Hello from server";
        boost::asio::write(socket, boost::asio::buffer(response), error);

        if (error) {
            throw boost::system::system_error(error);
        }

        std::cout << "Response sent to client. Closing connection." << std::endl;
    } catch (std::exception& e) {
        std::cerr << "Exception in thread: " << e.what() << std::endl;
    }
}

int main() {
    try {
        boost::asio::io_context io_context;

        // 创建接受连接的端点
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), SERVER_PORT));

        std::cout << "Server is listening on port " << SERVER_PORT << std::endl;

        while (true) {
            // 接受一个连接
            tcp::socket socket(io_context);
            acceptor.accept(socket);

            // 创建一个新线程来处理这个客户端连接
            std::thread(handle_client, std::move(socket)).detach();
        }
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}