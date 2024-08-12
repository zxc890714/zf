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
    // !创建IP对象， 创建的ip对象里面会有ip地址信息
      int ClientEndPoint() {
        std::string rawIp = "0.0.0.0";
        unsigned int portNumber = 1234;
        boost::system::error_code ec;
        boost::asio::ip::address ipAddress =boost::asio::ip::address::from_string(rawIp, ec);
        if (0 != ec.value()) {
            cout << "解析Ip地址错误: "   <<ec.value() << " :" << ec.message() << endl;
            return ec.value();
        }
        // !生成断点对象
        boost::asio::ip::tcp::endpoint ep(ipAddress, portNumber);
        return 0;
      }
      
    };
