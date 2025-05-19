#include <boost/algorithm/string/trim.hpp>
#include <boost/asio.hpp>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

using namespace boost::asio;
using ip::tcp;

class connection : public std::enable_shared_from_this<connection> {
public:
  // ... 你的成员变量 ...

  void do_read() {
    auto self(shared_from_this());
    auto response_streambuf = std::make_shared<streambuf>();

    // 1. 读响应头
    async_read_until(
        socket_, *response_streambuf, "\r\n\r\n",
        [self, response_streambuf](const boost::system::error_code &ec,
                                   std::size_t len) {
          if (ec) {
            std::cerr << "Read head error: " << ec.message() << std::endl;
            return;
          }
          std::istream response_stream(response_streambuf.get());
          std::string head(len, '\0');
          response_stream.read(&head[0], len);

          // 解析头部，判断是否chunked
          bool is_chunked = false;
          size_t content_length = 0;

          // 伪头解析
          std::istringstream header_stream(head);
          std::string line;
          while (std::getline(header_stream, line) && line != "\r") {
            boost::algorithm::trim(line);
            if (line.find("Transfer-Encoding:") != std::string::npos &&
                line.find("chunked") != std::string::npos)
              is_chunked = true;
            if (line.find("Content-Length:") != std::string::npos)
              content_length = std::atoi(line.substr(15).c_str());
          }

          auto body_buffer = std::make_shared<std::string>();
          if (is_chunked) {
            self->read_chunk(response_streambuf, body_buffer);
          } else if (content_length > 0) {
            self->read_content_length_body(response_streambuf, body_buffer,
                                           content_length);
          } else {
            // 没有body，直接回调
            self->handle_response(*body_buffer);
          }
        });
  }

  // 读取固定长度body
  void read_content_length_body(std::shared_ptr<boost::asio::streambuf> buf,
                                std::shared_ptr<std::string> body,
                                size_t content_length) {
    auto self(shared_from_this());
    std::size_t already = buf->size();
    if (already >= content_length) {
      std::istream is(buf.get());
      std::vector<char> tmp(content_length);
      is.read(tmp.data(), content_length);
      body->append(tmp.data(), content_length);
      handle_response(*body);
      return;
    }
    async_read(socket_, *buf, transfer_exactly(content_length - already),
               [self, buf, body, content_length](
                   const boost::system::error_code &ec, std::size_t) {
                 if (ec) {
                   std::cerr << "Read body error: " << ec.message()
                             << std::endl;
                   return;
                 }
                 std::istream is(buf.get());
                 std::vector<char> tmp(content_length);
                 is.read(tmp.data(), content_length);
                 body->append(tmp.data(), content_length);
                 self->handle_response(*body);
               });
  }

  // 递归异步读取chunk（核心）
  void read_chunk(std::shared_ptr<boost::asio::streambuf> buf,
                  std::shared_ptr<std::string> body) {
    auto self(shared_from_this());
    async_read_until(
        socket_, *buf, "\r\n",
        [self, buf, body](const boost::system::error_code &ec, std::size_t) {
          if (ec) {
            std::cerr << "Read chunk size error: " << ec.message() << std::endl;
            return;
          }
          std::istream is(buf.get());
          std::string size_line;
          std::getline(is, size_line);
          boost::algorithm::trim(size_line);
          std::stringstream ss;
          ss << std::hex << size_line;
          std::size_t chunk_size = 0;
          ss >> chunk_size;

          if (chunk_size == 0) {
            // 最后一个chunk，读结尾\r\n
            async_read_until(
                socket_, *buf, "\r\n",
                [self, buf, body](const boost::system::error_code &ec,
                                  std::size_t) {
                  if (ec) {
                    std::cerr << "Read chunk end error: " << ec.message()
                              << std::endl;
                    return;
                  }
                  self->handle_response(*body);
                });
            return;
          }

          // 读chunk内容+结尾\r\n
          async_read(socket_, *buf, transfer_exactly(chunk_size + 2),
                     [self, buf, body, chunk_size](
                         const boost::system::error_code &ec, std::size_t) {
                       if (ec) {
                         std::cerr << "Read chunk body error: " << ec.message()
                                   << std::endl;
                         return;
                       }
                       std::istream is2(buf.get());
                       std::vector<char> tmp(chunk_size);
                       is2.read(tmp.data(), chunk_size);
                       body->append(tmp.data(), chunk_size);
                       // 跳过\r\n
                       char crlf[2];
                       is2.read(crlf, 2);
                       // 递归读下一个chunk
                       self->read_chunk(buf, body);
                     });
        });
  }

  void handle_response(const std::string &body) {
    std::cout << "完整响应体: \n" << body << std::endl;
    // 这里可以继续发下一个请求，或关闭socket等
  }

  tcp::socket socket_;
  // ... 你的其他成员 ...
};
