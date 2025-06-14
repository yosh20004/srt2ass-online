# HTTP服务器

这是一个基于C++20的HTTP服务器实现，使用epoll进行事件驱动，支持多线程处理请求。

## 依赖要求

- CMake 3.10或更高版本
- C++20兼容的编译器
- Git

## 构建步骤

1. 克隆仓库：
```bash
git clone <your-repository-url>
cd <repository-directory>
```

2. 创建构建目录：
```bash
mkdir build
cd build
```

3. 配置和构建项目：
```bash
cmake ..
make
```

## 运行服务器

构建完成后，在build目录下运行：
```bash
./server
```

服务器默认监听8080端口。

## 环境变量

- `LOG_FILE`: 设置日志文件路径（默认为 `logs/server.log`）
- `SERVER_PORT`: 设置服务器监听端口（默认为 `8080`）

## 项目结构

```
.
├── CMakeLists.txt    # CMake构建配置
├── src/             # 源代码目录
│   ├── copy.cpp     # 主服务器实现
│   └── http_handler.cpp  # HTTP请求处理
└── include/         # 头文件目录
```

## 特性

- 使用epoll进行事件驱动
- 多线程处理请求
- 非阻塞I/O
- 支持HTTP/1.1
- 异步日志记录
- 可配置的端口号 