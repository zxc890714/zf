#include <iostream>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

struct data
{
    
    


};




void send_message(boost::asio::io_context& io_context, const std::string& message) {
    try {
        // ������������ַ�Ͷ˿�
        tcp::resolver resolver(io_context);
        tcp::resolver::results_type endpoints = resolver.resolve(SERVER_IP, std::to_string(SERVER_PORT));

        // �����������׽���
        tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);

        std::cout << "Connected to server" << std::endl;

        // ������Ϣ��������
        boost::asio::write(socket, boost::asio::buffer(message));

        std::cout << "Message sent: " << message << std::endl;

        // ��ȡ��������Ӧ
        char reply[1024] = {0};
        boost::system::error_code error;
        size_t reply_length = socket.read_some(boost::asio::buffer(reply), error);

        if (error == boost::asio::error::eof) {
            std::cout << "Connection closed cleanly by peer" << std::endl;
        } else if (error) {
            throw boost::system::system_error(error);
        }

        std::cout << "Server: " << std::string(reply, reply_length) << std::endl;
        std::cout << "Closing connection." << std::endl;
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

int main() {
    try {
        boost::asio::io_context io_context;

        std::string message;
        while (true) {
            std::cout << "Enter message to send (or 'exit' to quit): ";
            std::getline(std::cin, message);

            if (message == "exit") {
                break;
            }

            send_message(io_context, message);
        }
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}