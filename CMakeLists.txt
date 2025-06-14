cmake_minimum_required(VERSION 3.10)
project(http_server)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 包含 FetchContent 模块
include(FetchContent)

# 确保 third_party 目录存在
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# 配置 spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0  # 使用稳定版本
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/spdlog
)

# 配置 spdlog 选项
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)

# 下载并配置 spdlog
FetchContent_MakeAvailable(spdlog)

# 添加源文件
set(SOURCES
    src/copy.cpp
    src/http_handler.cpp
    src/file_handler.cpp
)

# 创建可执行文件
add_executable(server ${SOURCES})

# 链接依赖库
target_link_libraries(server PRIVATE
    spdlog::spdlog
    pthread
)

# 设置包含目录
target_include_directories(server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# 添加 srt2ass 可执行文件
add_executable(srt2ass_executable
    srt2ass/main.cpp
    srt2ass/srt2ass_api.cpp
    srt2ass/SRTReader.cpp
    srt2ass/srtTransfer.cpp
    srt2ass/ASSWriter.cpp
    srt2ass/SRTChecker.cpp
)

# 设置 srt2ass 的包含目录和链接库
target_include_directories(srt2ass_executable PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/srt2ass
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/spdlog/include
)

target_link_libraries(srt2ass_executable PRIVATE
    spdlog::spdlog
)

# 创建日志目录
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/logs) 