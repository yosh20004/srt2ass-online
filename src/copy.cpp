#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include <cstring> // for strerror, memset

#include <unistd.h>      // for close, read, write
#include <sys/types.h>
#include <sys/socket.h>  // for socket, bind, listen, accept
#include <netinet/in.h>  // for sockaddr_in
#include <arpa/inet.h>   // for inet_ntoa
#include <fcntl.h>       // for fcntl, O_NONBLOCK
#include <sys/epoll.h>   // for epoll
#include <cerrno>        // for errno

#include "common/peer_info.h"
#include "spdlog/spdlog.h"
#include "ThreadPool/ThreadPool.h"
#include "http_handler.h"
#include "spdlog/sinks/basic_file_sink.h"

// ================= 全局配置和变量 =================
std::atomic<int> active_clients_count(0); // 活跃客户端数量

// 从环境变量读取端口号，如果没有设置则使用默认值8080
int get_port_from_env() {
    const char* port_str = std::getenv("SERVER_PORT");
    if (port_str) {
        try {
            int port = std::stoi(port_str);
            if (port > 0 && port < 65536) {
                return port;
            }
        } catch (const std::exception& e) {
            spdlog::warn("Invalid port number in SERVER_PORT environment variable: {}. Using default port 8080.", port_str);
        }
    }
    return 8080;
}

constexpr int BACKLOG = 1024; // 增加了backlog的大小
constexpr int MAX_CLIENTS = 1024;
constexpr int MAX_EVENTS = MAX_CLIENTS + 1;
constexpr int WORKERS_NUM = 32;  // 减少线程数，避免过度并发
constexpr size_t MAX_REQUEST_SIZE = 1024 * 1024 * 20; // 20MB
constexpr int TASK_BUFFER_SIZE = 4096;

ThreadPool pool(WORKERS_NUM);

// ================= 客户端状态管理 =================

struct ClientState {
    int fd;
    
    // 为每个客户端维护一个独立的读缓冲区，以解决TCP"分包"问题
    std::vector<char> read_buffer; 

    // 写缓冲区
    std::vector<char> write_buffer;
    size_t write_pos = 0;
    bool has_pending_write = false; // 用于指示当前客户端连接是否有尚未发送完毕的数据
    
    // 标志是否正在关闭连接(防止重复关闭)
    std::atomic<bool> is_closing{false}; 
};

std::unordered_map<int, std::shared_ptr<ClientState>> client_states;
std::mutex client_states_mutex;
int global_epoll_fd = -1; // 全局epoll fd，供线程池任务使用

// 声明清理函数，以便在任务函数中调用
void cleanup_client(std::shared_ptr<ClientState> state);

// 获取或创建客户端状态
std::shared_ptr<ClientState> get_client_state(int fd) {
    std::lock_guard<std::mutex> lk(client_states_mutex);
    auto it = client_states.find(fd);

    if (it == client_states.end()) { 
        auto state = std::make_shared<ClientState>();
        state->fd = fd;
        client_states[fd] = state;
        return state;
    }
    return it->second;
}

// 移除客户端状态
void remove_client_state(int fd) {
    std::lock_guard<std::mutex> lk(client_states_mutex);
    client_states.erase(fd);
}

// ================= 任务处理函数 =================

// 处理写入任务
inline void handle_write_task(int client_fd) {
    auto state = get_client_state(client_fd);
    if (!state || state->is_closing.load() || !state->has_pending_write) {
        return;
    }

    auto clientInfo = get_peer_info(client_fd);
    spdlog::debug("[WriteTask] Handling write for fd {} from {}:{}", 
                  client_fd, 
                  clientInfo ? clientInfo->ip_address : "?.?.?.?", 
                  clientInfo ? clientInfo->port : 0);

    size_t total_to_send = state->write_buffer.size();
    const char* data_ptr = state->write_buffer.data();

    while (state->write_pos < total_to_send) {
        ssize_t bytes_sent = send(client_fd,
                                  data_ptr + state->write_pos,
                                  total_to_send - state->write_pos,
                                  MSG_NOSIGNAL); // 使用MSG_NOSIGNAL防止EPIPE信号
        
        if (bytes_sent < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                // 发送缓冲区满(接收方发生网络拥塞，此时不可能发送了) 等待下一次EPOLLOUT
                spdlog::debug("[WriteTask] Send buffer full for fd {}, waiting for next EPOLLOUT", client_fd);
                return; 
            } else {
                spdlog::error("[WriteTask] send failed for fd {}: {}", client_fd, strerror(errno));
                cleanup_client(state); // 调用统一的清理函数
                return;
            } 
        }

        if (bytes_sent == 0) {
            spdlog::info("[WriteTask] Connection seems closed by peer during send for fd {}", client_fd);
            cleanup_client(state);
            return;
        } 

        state->write_pos += bytes_sent;
    }

    // 所有数据发送完成
    spdlog::info("[WriteTask] All data sent for fd {}, now closing connection.", client_fd);
    state->has_pending_write = false;
    
    // HTTP/1.1 Connection: close 模式，发送完后直接关闭
    cleanup_client(state);
}

// 处理读取和业务逻辑任务
inline void handle_client_task(int client_fd) {
    auto state = get_client_state(client_fd);
    if (!state || state->is_closing.load()) {
        return;
    }

    // 添加原子标志，防止重复处理
    static std::atomic<bool> processing{false};
    bool expected = false;
    if (!processing.compare_exchange_strong(expected, true)) {
        return; // 已经在处理中，直接返回
    }

    char temp_buffer[TASK_BUFFER_SIZE];
    ssize_t valread = recv(client_fd, temp_buffer, TASK_BUFFER_SIZE, 0);
    
    if (valread > 0) {
        // 将读到的数据追加到客户端自己的读缓冲区
        state->read_buffer.insert(state->read_buffer.end(), temp_buffer, temp_buffer + valread);
    } else if (valread == 0) {
        // 表示客户端主动关闭连接
        spdlog::info("[ReadTask] Client fd {} disconnected (EOF).", client_fd);
        cleanup_client(state);
        processing.store(false);
        return;
    } else { // valread < 0
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            // 没有更多数据可读
            spdlog::debug("[ReadTask] No more data available for fd {} (EAGAIN/EWOULDBLOCK).", client_fd);
        } else {
            spdlog::error("[ReadTask] recv failed for fd {}: {}", client_fd, strerror(errno));
            cleanup_client(state);
        }
        processing.store(false);
        return;
    }

    // 防止恶意请求耗尽内存
    if (state->read_buffer.size() > MAX_REQUEST_SIZE) {
        spdlog::warn("[ReadTask] Request data limit exceeded for fd {}. Closing connection.", client_fd);
        cleanup_client(state);
        processing.store(false);
        return;
    }

    // ----- 开始处理已接收的数据 -----
    std::string request_str(state->read_buffer.begin(), state->read_buffer.end());
    size_t header_end_pos = request_str.find("\r\n\r\n");

    if (header_end_pos != std::string::npos) {
        // 解析Content-Length
        size_t content_length = 0;
        size_t content_length_pos = request_str.find("Content-Length: ");
        if (content_length_pos != std::string::npos) {
            content_length = std::stoul(request_str.substr(content_length_pos + 16));
        }

        // 检查是否收到完整的请求体
        size_t body_start = header_end_pos + 4;
        if (content_length == 0 || request_str.length() >= body_start + content_length) {
            spdlog::info("[LogicTask] Received a complete request from fd {}", client_fd);

            // 处理请求
            std::string response = HttpHandler::handleRequest(request_str);

            // 将响应数据放入写缓冲区
            state->write_buffer.assign(response.begin(), response.end());
            state->write_pos = 0;
            state->has_pending_write = true;

            // 从读缓冲区中移除已经处理过的请求
            size_t request_size = body_start + content_length;
            state->read_buffer.erase(state->read_buffer.begin(), state->read_buffer.begin() + request_size);

            // 尝试立刻发送
            handle_write_task(client_fd);
        } else {
            // 请求体不完整，等待更多数据
            spdlog::debug("[LogicTask] Incomplete request body from fd {}, waiting for more data.", client_fd);
        }
    } else {
        // 请求头不完整，等待更多数据
        spdlog::debug("[LogicTask] Incomplete request header from fd {}, waiting for more data.", client_fd);
    }

    processing.store(false);
}

// 统一的客户端清理函数
void cleanup_client(std::shared_ptr<ClientState> state) {
    if (!state) return;

    // 使用原子操作确保只有一个线程可以执行清理
    bool expected = false;
    if (state->is_closing.compare_exchange_strong(expected, true)) {
        int client_fd = state->fd;

        spdlog::info("[Cleanup] Starting cleanup for fd {}", client_fd);

        // 1. 从epoll中移除
        if (epoll_ctl(global_epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr) == -1) {
           spdlog::error("[Cleanup] epoll_ctl DEL failed for fd {}: {}", client_fd, strerror(errno));  
        }

        // 2. 从全局状态map中移除
        remove_client_state(client_fd);

        // 3. 活跃连接数减一
        active_clients_count.fetch_sub(1, std::memory_order_relaxed);
        
        // 4. 关闭套接字
        close(client_fd);

        spdlog::info("[Cleanup] Client fd {} closed. Active clients: {}", client_fd, active_clients_count.load());
    }
}

// ================= 主函数和服务器设置 =================
void init(); // 假设这个函数在别处定义

int main() {
    init(); // 调用您自己的初始化函数

    int server_fd;
    struct sockaddr_in server_addr;
    int port = get_port_from_env();

    // 创建服务器套接字 (非阻塞)
    server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (server_fd < 0) {
        spdlog::critical("Socket creation failed: {}", strerror(errno));
        return -1;
    }

    // 设置SO_REUSEADDR选项
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        spdlog::critical("setsockopt SO_REUSEADDR failed: {}", strerror(errno));
        close(server_fd);
        return -1;
    }

    // 绑定地址和端口
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        spdlog::critical("Bind failed: {}", strerror(errno));
        close(server_fd); 
        return -1;
    }
    
    // 开始监听
    if (listen(server_fd, BACKLOG) < 0) {
        spdlog::critical("Listen failed: {}", strerror(errno));
        close(server_fd); 
        return -1;
    }

    // 创建epoll实例
    global_epoll_fd = epoll_create1(0);
    if (global_epoll_fd == -1) {
        spdlog::critical("epoll_create1 failed: {}", strerror(errno));
        close(server_fd);
        return -1;
    }

    // 将服务器套接字添加到epoll
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = server_fd;
    if (epoll_ctl(global_epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
        spdlog::critical("epoll_ctl ADD server_fd failed: {}", strerror(errno));
        close(server_fd);
        close(global_epoll_fd);
        return -1;
    }

    struct epoll_event events[MAX_EVENTS];
    spdlog::info("Server listening on port {}...", port);

    // ================= 主事件循环 =================
    while (true) {
        int num_events = epoll_wait(global_epoll_fd, events, MAX_EVENTS, -1);
        if (num_events < 0) {
            if (errno == EINTR) continue; // 被信号中断，正常
            spdlog::error("epoll_wait failed: {}", strerror(errno));
            break;
        }

        for (int i = 0; i < num_events; ++i) {
            int current_fd = events[i].data.fd;
            uint32_t current_events = events[i].events;

            if (current_fd == server_fd) {
                // ----- 处理新连接 -----
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
                
                if (new_socket < 0) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        spdlog::error("accept error: {}", strerror(errno));
                    }
                    continue;
                }

                if (active_clients_count >= MAX_CLIENTS) {
                    spdlog::warn("Max clients reached. Connection from {} refused.", inet_ntoa(client_addr.sin_addr));
                    close(new_socket);
                    continue;
                }
                
                // 将新客户端设为非阻塞
                int flags = fcntl(new_socket, F_GETFL, 0);
                fcntl(new_socket, F_SETFL, flags | O_NONBLOCK);
                
                // 将新客户端添加到epoll
                event.events = EPOLLIN; // 使用水平触发模式
                event.data.fd = new_socket;
                if (epoll_ctl(global_epoll_fd, EPOLL_CTL_ADD, new_socket, &event) == -1) {
                    spdlog::error("epoll_ctl ADD new_socket failed: {}", strerror(errno));
                    close(new_socket);
                    continue;
                }

                active_clients_count.fetch_add(1, std::memory_order_relaxed);
                get_client_state(new_socket); // 初始化客户端状态
                spdlog::info("New connection accepted: fd = {}, from {}:{}. Active clients = {}", 
                             new_socket, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), active_clients_count.load());

            } else {
                // ----- 处理已连接客户端的事件 -----
                if ((current_events & EPOLLERR) || (current_events & EPOLLHUP)) {
                    spdlog::warn("EPOLLERR or EPOLLHUP on fd {}. Closing.", current_fd);
                    cleanup_client(get_client_state(current_fd));
                    continue;
                }

                if (current_events & EPOLLIN) {
                    spdlog::debug("[MainLoop] EPOLLIN event on fd {}. Enqueueing read task.", current_fd);
                    try {
                        // 检查客户端状态是否已经在处理中
                        auto state = get_client_state(current_fd);
                        if (state && !state->is_closing.load()) {
                            pool.enqueue(handle_client_task, current_fd);
                        }
                    } catch (const std::exception& e) {
                        spdlog::error("Failed to enqueue read task for fd {}: {}", current_fd, e.what());
                        cleanup_client(get_client_state(current_fd));
                    }
                }

                if (current_events & EPOLLOUT) {
                    spdlog::debug("[MainLoop] EPOLLOUT event on fd {}. Enqueueing write task.", current_fd);
                    try {
                        auto state = get_client_state(current_fd);
                        if (state && !state->is_closing.load() && state->has_pending_write) {
                            pool.enqueue(handle_write_task, current_fd);
                        }
                    } catch (const std::exception& e) {
                        spdlog::error("Failed to enqueue write task for fd {}: {}", current_fd, e.what());
                        cleanup_client(get_client_state(current_fd));
                    }
                }
            }
        }
    }

    close(server_fd);
    close(global_epoll_fd);
    spdlog::info("Server shutting down.");
    return 0;
}


void init() {
    const char* log_file = std::getenv("LOG_FILE");
    if (!log_file) {
        log_file = "logs/server.log";  // 默认值
    }
    auto file_logger = spdlog::basic_logger_mt("file_logger", log_file);
    file_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
    file_logger->set_level(spdlog::level::debug);
    spdlog::set_default_logger(file_logger);
    spdlog::flush_on(spdlog::level::debug);
    spdlog::info("Server initialized with logging to file: {}", log_file);
}