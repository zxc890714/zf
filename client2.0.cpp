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

// ============ 事件类型 ============
enum class EventType { EVENT_A, EVENT_B, EVENT_C };

// ============ 事件消息 ============
struct EventMsg {
  std::string data;
};

// ============ 连接参数结构 ============
struct ConnInfo {
  std::string ip;
  uint16_t port;
  bool auto_reconnect = true;
  EventType event_type = EventType::EVENT_A;  // 新增事件类型
};

// ============ Connection =============
class Connection : public std::enable_shared_from_this<Connection> {
public:
  using Ptr = std::shared_ptr<Connection>;
  using Message = std::vector<uint8_t>;
  using MessageQueue = std::deque<Message>;
  using DisconnectCallback = std::function<void(const std::string &, bool)>;

  static Ptr create(boost::asio::io_context &io, const std::string &key, EventType event_type) {
    return Ptr(new Connection(io, key, event_type));
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

  // 新增：返回事件类型
  EventType event_type() const { return event_type_; }

  // 新增：线程安全地将事件消息压入本连接队列
  void enqueue_event(const EventMsg& msg) {
    std::lock_guard<std::mutex> lock(event_mtx_);
    event_msgs_.push_back(msg);
  }

  // 新增：拉取并清空所有事件消息
  std::vector<EventMsg> fetch_and_clear_events() {
    std::lock_guard<std::mutex> lock(event_mtx_);
    std::vector<EventMsg> ret(event_msgs_.begin(), event_msgs_.end());
    event_msgs_.clear();
    return ret;
  }

private:
  Connection(boost::asio::io_context &io, const std::string &key, EventType event_type)
      : socket_(io), strand_(boost::asio::make_strand(io)), writing_(false),
        dead_(false), conn_key_(key), event_type_(event_type) {}

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
  EventType event_type_;              // 新增
  std::deque<EventMsg> event_msgs_;   // 新增：本连接的事件队列
  std::mutex event_mtx_;              // 新增：保护事件队列
  DisconnectCallback on_disconnect_;
};

// ============ ClientManager =============
class ClientManager : public std::enable_shared_from_this<ClientManager> {
public:
  using ConnectionPtr = Connection::Ptr;

  ClientManager(boost::asio::io_context &io) : io_context_(io), timer_(io) {}

  // 新增：支持事件类型
  void add_connection(const std::string &ip, uint16_t port,
                      bool auto_reconnect = true, EventType type = EventType::EVENT_A) {
    std::string key = ip + ":" + std::to_string(port);
    {
      std::lock_guard<std::mutex> lock(mtx_);
      conn_infos_[key] = ConnInfo{ip, port, auto_reconnect, type};
    }
    do_connect(key, type);
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
      retry_connect_later(key, info.event_type);
    } else {
      std::cout << "[INFO] Connection closed, not reconnect: " << key
                << std::endl;
      std::lock_guard<std::mutex> lock(mtx_);
      conn_infos_.erase(key);
    }
  }

  // 热重载接口：关闭所有旧连接，按新参数重建
  void reload_connections(const std::vector<ConnInfo> &new_params) {
    std::vector<std::string> keys;
    {
      std::lock_guard<std::mutex> lock(mtx_);
      for (const auto &kv : connections_)
        keys.push_back(kv.first);
    }
    for (const auto &key : keys) {
      close_connection(key);
    }

    {
      std::lock_guard<std::mutex> lock(mtx_);
      conn_infos_.clear();
    }

    for (const auto &info : new_params) {
      add_connection(info.ip, info.port, info.auto_reconnect, info.event_type);
    }
  }

  // 新增：线程安全地将消息推到所有订阅该事件类型的连接
  void on_redis_event(EventType type, const std::string& msg) {
    std::lock_guard<std::mutex> lock(mtx_);
    for (auto& kv : connections_) {
      if (kv.second->event_type() == type) {
        kv.second->enqueue_event(EventMsg{msg});
      }
    }
  }

  // 新增：让所有连接取出并打印自己的事件消息队列
  void dispatch_events() {
    std::lock_guard<std::mutex> lock(mtx_);
    for (auto& kv : connections_) {
      auto conn = kv.second;
      std::vector<EventMsg> events = conn->fetch_and_clear_events();
      for (const auto& ev : events) {
        std::cout << "[" << conn->key() << "] recv event: " << ev.data << std::endl;
      }
    }
  }

private:
  void do_connect(const std::string &key, EventType type) {
    ConnInfo info;
    {
      std::lock_guard<std::mutex> lock(mtx_);
      info = conn_infos_[key];
    }
    auto conn = Connection::create(io_context_, key, type);

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
        retry_connect_later(key, type);
      }
    });
  }

  void retry_connect_later(const std::string &key, EventType type) {
    auto timer = std::make_shared<boost::asio::steady_timer>(io_context_);
    timer->expires_after(std::chrono::seconds(3));
    std::weak_ptr<ClientManager> wp = shared_from_this();
    timer->async_wait([wp, key, timer, type](boost::system::error_code ec) {
      if (!ec) {
        if (auto mgr = wp.lock()) {
          mgr->do_connect(key, type);
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

  // 初始连接，指定各自事件类型
  manager->add_connection("127.0.0.1", 8080, true, EventType::EVENT_A);
  manager->add_connection("127.0.0.1", 8081, true, EventType::EVENT_B);
  manager->start_send_loop();

  // IO线程
  std::vector<std::thread> threads;
  for (int i = 0; i < 2; ++i)
    threads.emplace_back([&io_context] { io_context.run(); });

  // 模拟“Redis订阅”线程推送事件
  std::thread([manager] {
    for (int i = 0; i < 5; ++i) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      manager->on_redis_event(EventType::EVENT_A, "redis msg to A " + std::to_string(i));
      manager->on_redis_event(EventType::EVENT_B, "redis msg to B " + std::to_string(i));
    }
  }).detach();

  // 每秒调度所有连接拉取自己的事件队列消息并打印
  std::thread([manager] {
    for (int i = 0; i < 6; ++i) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      manager->dispatch_events();
    }
  }).join();

  for (auto &t : threads)
    t.join();
  return 0;
}
