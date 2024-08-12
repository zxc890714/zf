#include <iostream>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

// 确保结构体按字节打包
#pragma pack(push, 1)

struct FreezingTimeInfo {
    uint64_t freezingTimestamp;
    char freezingSystemDateTime[20];
};

struct EventInfo {
    char ipAddress[16];
    char protocol[8];
    char macAddress[18];
    char dateTime[20];
    char eventType[16];
    char eventState[16];
    char eventDescription[64];
    int32_t portNo;
    int32_t channelID;
    int32_t activePostCount;
    int32_t stopLineDistance;
    int32_t radarDetectDistance;
    FreezingTimeInfo freezingTimeInfo;
};

#pragma pack(pop)

void handleReceivedData(EventInfo* event) {
    std::cout << "Received Event Information:" << std::endl;
    std::cout << "IP Address: " << event->ipAddress << std::endl;
    std::cout << "Port No: " << event->portNo << std::endl;
    std::cout << "Protocol: " << event->protocol << std::endl;
    std::cout << "MAC Address: " << event->macAddress << std::endl;
    std::cout << "Channel ID: " << event->channelID << std::endl;
    std::cout << "DateTime: " << event->dateTime << std::endl;
    std::cout << "Active Post Count: " << event->activePostCount << std::endl;
    std::cout << "Event Type: " << event->eventType << std::endl;
    std::cout << "Event State: " << event->eventState << std::endl;
    std::cout << "Event Description: " << event->eventDescription << std::endl;
    std::cout << "Stop Line Distance: " << event->stopLineDistance << std::endl;
    std::cout << "Radar Detect Distance: " << event->radarDetectDistance << std::endl;
    std::cout << "Freezing Timestamp: " << event->freezingTimeInfo.freezingTimestamp << std::endl;
    std::cout << "Freezing System DateTime: " << event->freezingTimeInfo.freezingSystemDateTime << std::endl;
}

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket)
        : socket_(std::move(socket)) {}

    void start() {
        doRead();
    }

private:
    void doRead() {
        auto self(shared_from_this());
        boost::asio::async_read(socket_, boost::asio::buffer(&event_, sizeof(EventInfo)),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    handleReceivedData(&event_);
                    doRead(); // 继续读取下一条数据
                } else {
                    std::cerr << "Read Error: " << ec.message() << std::endl;
                }
            });
    }

    tcp::socket socket_;
    EventInfo event_;
};

class Server {
public:
    Server(boost::asio::io_context& io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
        startAccept();
    }

private:
    void startAccept() {
        auto socket = std::make_shared<tcp::socket>(acceptor_.get_executor());
        acceptor_.async_accept(*socket, [this, socket](boost::system::error_code ec) {
            if (!ec) {
                std::make_shared<Session>(std::move(*socket))->start();
            } else {
                std::cerr << "Accept Error: " << ec.message() << std::endl;
            }
            startAccept();
        });
    }

    tcp::acceptor acceptor_;
};

int main() {
    try {
        boost::asio::io_context io_context;
        Server server(io_context, 8080);
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}