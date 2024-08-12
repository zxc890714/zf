#include <iostream>
#include <boost/asio.hpp>
#include <cstring> // 包含 std::strncpy
#include <thread> // 包含 std::this_thread::sleep_for
#include <chrono> // 包含 std::chrono::seconds

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

void sendDataToServer(const std::string& serverIp, int serverPort, const EventInfo& event) {
    boost::asio::io_context io_context;
    tcp::socket socket(io_context);
    tcp::resolver resolver(io_context);
    boost::asio::connect(socket, resolver.resolve(serverIp, std::to_string(serverPort)));
    boost::asio::write(socket, boost::asio::buffer(&event, sizeof(EventInfo)));
}

void safeStrncpy(char* dest, const char* src, size_t destSize) {
    std::strncpy(dest, src, destSize - 1);
    dest[destSize - 1] = '\0'; // 确保字符串以空字符结尾
}

int main() {
    FreezingTimeInfo freezeInfo;
    freezeInfo.freezingTimestamp = 1627889182;
    safeStrncpy(freezeInfo.freezingSystemDateTime, "2024-08-05T12:00:00Z", sizeof(freezeInfo.freezingSystemDateTime));

    EventInfo event;
    safeStrncpy(event.ipAddress, "192.168.1.1", sizeof(event.ipAddress));
    safeStrncpy(event.protocol, "TCP", sizeof(event.protocol));
    safeStrncpy(event.macAddress, "00:1A:2B:3C:4D:5E", sizeof(event.macAddress));
    safeStrncpy(event.dateTime, "2024-08-05T12:00:00Z", sizeof(event.dateTime));
    safeStrncpy(event.eventType, "TemperatureAlert", sizeof(event.eventType));
    safeStrncpy(event.eventState, "Active", sizeof(event.eventState));
    safeStrncpy(event.eventDescription, "Temperature threshold exceeded", sizeof(event.eventDescription));

    event.portNo = 8080;
    event.channelID = 1;
    event.activePostCount = 5;
    event.stopLineDistance = 10;
    event.radarDetectDistance = 50;
    event.freezingTimeInfo = freezeInfo;

    while (true) {
        sendDataToServer("127.0.0.1", 8080, event);
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    return 0;
}