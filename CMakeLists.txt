cmake_minimum_required(VERSION 3.10)
project(http_server)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 确保 third_party 目录存在
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# 配置 spdlog 选项
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)

# 添加 spdlog 子目录
add_subdirectory(third_party/spdlog)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/srt2ass/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/srt2ass/srt2ass_api.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/srt2ass/SRTReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/srt2ass/srtTransfer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/srt2ass/ASSWriter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/srt2ass/SRTChecker.cpp
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