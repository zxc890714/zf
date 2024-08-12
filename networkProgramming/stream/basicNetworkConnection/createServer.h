#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <string>
#include <iostream>

using std::cout;
using std::endl;
using std::string;


class CreateServer {
public:
  int createEndPoint() {
    unsigned int port = 1234;
    // !监听所有可用的ipv4地址
    boost::asio::ip::address ipAddress = boost::asio::ip::address_v4::any();

    // !创建endPoint对象
    boost::asio::ip::tcp::endpoint ep(ipAddress, port);

    return 0;
  }

  int creatSocket() {
    boost::asio::io_context ios;
    
  }
  
};