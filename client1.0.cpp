#include <boost/asio.hpp>
#include <chrono>
#include <deque>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// ============ 连接参数结构 ============
struct ConnInfo {
  std::string ip;
  uint16_t port;
  bool auto_reconnect = true;
};

// ============ Connection =============
class Connection : public std::enable_shared_from_this<Connection> {
public:
  using Ptr = std::shared_ptr<Connection>;
  using Message = std::vector<uint8_t>;
  using MessageQueue = std::deque<Message>;
  using DisconnectCallback = std::function<void(const std::string &, bool)>;

  static Ptr create(boost::asio::io_context &io, const std::string &key) {
    return Ptr(new Connection(io, key));
  }

  boost::asio::ip::tcp::socket &socket() { return socket_; }

  void push_message(const Message &msg) {
    auto self = shared_from_this();
    boost::asio::post(strand_, [this, self, msg]() {
      if (dead_)
        return;
      bool was_empty = msg_queue_.empty();
      msg_queue_.push_back(msg);
      if (was_empty && !writing_) {
        writing_ = true;
        do_write();
      }
    });
  }

  void close(bool need_reconnect = false) {
    auto self = shared_from_this();
    boost::asio::post(strand_, [this, self, need_reconnect]() {
      if (dead_)
        return;
      dead_ = true;
      boost::system::error_code ec;
      socket_.close(ec);
      if (on_disconnect_)
        on_disconnect_(conn_key_, need_reconnect);
    });
  }

  void set_disconnect_callback(DisconnectCallback cb) {
    on_disconnect_ = std::move(cb);
  }

  void start() {
    // 起点为写，消息推送后会自动循环写读
  }

  std::string key() const { return conn_key_; }

private:
  Connection(boost::asio::io_context &io, const std::string &key)
      : socket_(io), strand_(boost::asio::make_strand(io)), writing_(false),
        dead_(false), conn_key_(key) {}

  void do_write() {
    if (msg_queue_.empty() || dead_) {
      writing_ = false;
      return;
    }
    auto self = shared_from_this();
    boost::asio::async_write(
        socket_, boost::asio::buffer(msg_queue_.front()),
        boost::asio::bind_executor(
            strand_, [this, self](boost::system::error_code ec, std::size_t) {
              if (dead_)
                return;
              if (!ec) {
                msg_queue_.pop_front();
                do_read();
              } else {
                writing_ = false;
                handle_disconnect("[ERR ] Write error: " + ec.message(), true);
              }
            }));
  }

  void do_read() {
    if (dead_)
      return;
    auto self = shared_from_this();
    boost::asio::async_read_until(
        socket_, buffer_, '\n',
        boost::asio::bind_executor(
            strand_, [this, self](boost::system::error_code ec, std::size_t) {
              if (dead_)
                return;
              if (!ec) {
                std::istream is(&buffer_);
                std::string line;
                std::getline(is, line);
                std::cout << "[" << conn_key_ << "] [RECV] " << line
                          << std::endl;
                do_write();
              } else {
                writing_ = false;
                handle_disconnect("[ERR ] Read error: " + ec.message(), true);
              }
            }));
  }

  void handle_disconnect(const std::string &msg, bool need_reconnect) {
    if (dead_)
      return;
    dead_ = true;
    std::cerr << "[" << conn_key_ << "] " << msg << std::endl;
    boost::system::error_code ignore_ec;
    socket_.close(ignore_ec);
    if (on_disconnect_)
      on_disconnect_(conn_key_, need_reconnect);
  }

  boost::asio::ip::tcp::socket socket_;
  boost::asio::strand<boost::asio::io_context::executor_type> strand_;
  MessageQueue msg_queue_;
  boost::asio::streambuf buffer_;
  bool writing_;
  bool dead_;
  std::string conn_key_;
  DisconnectCallback on_disconnect_;
};

// ============ ClientManager =============
class ClientManager : public std::enable_shared_from_this<ClientManager> {
public:
  using ConnectionPtr = Connection::Ptr;

  ClientManager(boost::asio::io_context &io) : io_context_(io), timer_(io) {}

  void add_connection(const std::string &ip, uint16_t port,
                      bool auto_reconnect = true) {
    std::string key = ip + ":" + std::to_string(port);
    {
      std::lock_guard<std::mutex> lock(mtx_);
      conn_infos_[key] = ConnInfo{ip, port, auto_reconnect};
    }
    do_connect(key);
  }

  void close_connection(const std::string &key) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = conn_infos_.find(key);
    if (it != conn_infos_.end())
      it->second.auto_reconnect = false;
    auto conn_it = connections_.find(key);
    if (conn_it != connections_.end()) {
      conn_it->second->close(false);
    }
  }

  void send_message(const std::string &key, const Connection::Message &msg) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = connections_.find(key);
    if (it != connections_.end()) {
      it->second->push_message(msg);
    }
  }

  void start_send_loop() {
    timer_.expires_after(std::chrono::seconds(1));
    timer_.async_wait([this](boost::system::error_code ec) {
      if (!ec) {
        std::lock_guard<std::mutex> lock(mtx_);
        for (auto &kv : connections_) {
          kv.second->push_message(make_msg());
        }
        start_send_loop();
      }
    });
  }

  void on_connection_closed(const std::string &key, bool need_reconnect) {
    ConnInfo info;
    {
      std::lock_guard<std::mutex> lock(mtx_);
      connections_.erase(key);
      auto it = conn_infos_.find(key);
      if (it != conn_infos_.end())
        info = it->second;
    }
    if (need_reconnect && info.auto_reconnect) {
      std::cout << "[INFO] Connection lost, will retry: " << key << std::endl;
      retry_connect_later(key);
    } else {
      std::cout << "[INFO] Connection closed, not reconnect: " << key
                << std::endl;
      std::lock_guard<std::mutex> lock(mtx_);
      conn_infos_.erase(key);
    }
  }

  // 热重载接口：关闭所有旧连接，按新参数重建
  void reload_connections(const std::vector<ConnInfo> &new_params) {
    // 1. 关闭所有旧连接
    std::vector<std::string> keys;
    {
      std::lock_guard<std::mutex> lock(mtx_);
      for (const auto &kv : connections_)
        keys.push_back(kv.first);
    }
    for (const auto &key : keys) {
      close_connection(key);
    }

    // 2. 清空旧参数
    {
      std::lock_guard<std::mutex> lock(mtx_);
      conn_infos_.clear();
    }

    // 3. 新建连接
    for (const auto &info : new_params) {
      add_connection(info.ip, info.port, info.auto_reconnect);
    }
  }

private:
  void do_connect(const std::string &key) {
    ConnInfo info;
    {
      std::lock_guard<std::mutex> lock(mtx_);
      info = conn_infos_[key];
    }
    auto conn = Connection::create(io_context_, key);

    std::weak_ptr<ClientManager> wp = shared_from_this();
    conn->set_disconnect_callback(
        [wp](const std::string &k, bool need_reconnect) {
          if (auto mgr = wp.lock())
            mgr->on_connection_closed(k, need_reconnect);
        });

    boost::asio::ip::tcp::endpoint ep(
        boost::asio::ip::address::from_string(info.ip), info.port);
    conn->socket().async_connect(ep, [=](boost::system::error_code ec) {
      if (!ec) {
        {
          std::lock_guard<std::mutex> lock(mtx_);
          connections_[key] = conn;
        }
        conn->start();
        std::cout << "[INFO] Connected: " << key << std::endl;
      } else {
        std::cerr << "[ERR ] Connect failed: " << key << " : " << ec.message()
                  << std::endl;
        retry_connect_later(key);
      }
    });
  }

  void retry_connect_later(const std::string &key) {
    auto timer = std::make_shared<boost::asio::steady_timer>(io_context_);
    timer->expires_after(std::chrono::seconds(3));
    std::weak_ptr<ClientManager> wp = shared_from_this();
    timer->async_wait([wp, key, timer](boost::system::error_code ec) {
      if (!ec) {
        if (auto mgr = wp.lock()) {
          mgr->do_connect(key);
        }
      }
    });
  }

  static Connection::Message make_msg() {
    std::string str = "hello\n";
    return Connection::Message(str.begin(), str.end());
  }

  boost::asio::io_context &io_context_;
  boost::asio::steady_timer timer_;
  std::map<std::string, ConnInfo> conn_infos_;
  std::map<std::string, ConnectionPtr> connections_;
  std::mutex mtx_;
};

// ============ main ============
int main() {
  boost::asio::io_context io_context;
  auto manager = std::make_shared<ClientManager>(io_context);

  // 初始连接
  manager->add_connection("127.0.0.1", 8080, true);
  manager->add_connection("127.0.0.1", 8081, true);
  manager->start_send_loop();

  // IO线程
  std::vector<std::thread> threads;
  for (int i = 0; i < 2; ++i)
    threads.emplace_back([&io_context] { io_context.run(); });

  // 10秒后“切换所有连接”
  std::this_thread::sleep_for(std::chrono::seconds(10));
  std::vector<ConnInfo> new_params = {{"127.0.0.1", 9000, true},
                                      {"127.0.0.1", 9001, true}};
  manager->reload_connections(new_params);

  for (auto &t : threads)
    t.join();
  return 0;



}
