// #include <boost/asio/detail/chrono.hpp>
// #include <boost/asio/io_context.hpp>
// #include <boost/asio/steady_timer.hpp>
#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <string>
#include <iostream>

using std::cout;
using std::endl;
using std::string;


class BasicNetWork {
    public:
    // !����IP���� ������ip�����������ip��ַ��Ϣ
      int ClientEndPoint() {
        std::string rawIp = "0.0.0.0";
        unsigned int portNumber = 1234;
        boost::system::error_code ec;
        boost::asio::ip::address ipAddress =boost::asio::ip::address::from_string(rawIp, ec);
        if (0 != ec.value()) {
            cout << "����Ip��ַ����: "   <<ec.value() << " :" << ec.message() << endl;
            return ec.value();
        }
        // !���ɶϵ����
        boost::asio::ip::tcp::endpoint ep(ipAddress, portNumber);
        return 0;
      }
      
    };
