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
    // !�������п��õ�ipv4��ַ
    boost::asio::ip::address ipAddress = boost::asio::ip::address_v4::any();

    // !����endPoint����
    boost::asio::ip::tcp::endpoint ep(ipAddress, port);

    return 0;
  }

  int creatSocket() {
    boost::asio::io_context ios;
    
  }
  
};