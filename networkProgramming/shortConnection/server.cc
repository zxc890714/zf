#include <iostream>
#include <boost/asio.hpp>
#include <thread>

using boost::asio::ip::tcp;

#define SERVER_PORT 8080

void handle_client(tcp::socket socket) {
    try {
        std::cout << "Client connected" << std::endl;

        // ��ȡ�ͻ�����Ϣ
        char data[1024] = {0};
        boost::system::error_code error;
        size_t length = socket.read_some(boost::asio::buffer(data), error);

        if (error == boost::asio::error::eof) {
            std::cout << "Connection closed cleanly by peer" << std::endl;
        } else if (error) {
            throw boost::system::system_error(error);
        }

        std::cout << "Received message: " << std::string(data, length) << std::endl;

        // ������Ӧ���ͻ���
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

        // �����������ӵĶ˵�
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), SERVER_PORT));

        std::cout << "Server is listening on port " << SERVER_PORT << std::endl;

        while (true) {
            // ����һ������
            tcp::socket socket(io_context);
            acceptor.accept(socket);

            // ����һ�����߳�����������ͻ�������
            std::thread(handle_client, std::move(socket)).detach();
        }
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}