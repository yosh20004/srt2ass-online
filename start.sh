#!/bin/bash

# 配置目标文件位置
SRT2ASS_EXECUTABLE="$(pwd)/build/srt2ass_executable"
SERVER_EXECUTABLE="./build/server"
LOG_DIR="./logs"
PUBLIC_DIR="./public"
LOG_FILE="$LOG_DIR/server.log"
SERVER_PORT=8080  # 默认端口号

# 确保日志目录存在
mkdir -p $LOG_DIR

# 设置环境变量
export SRT2ASS_EXECUTABLE
export LOG_DIR
export LOG_FILE
export SERVER_PORT

# 打印当前工作目录和可执行文件信息
echo "当前工作目录: $(pwd)"
echo "服务器端口: $SERVER_PORT"
ls -l $SRT2ASS_EXECUTABLE

# 启动服务，将输出重定向到日志文件
$SERVER_EXECUTABLE > "$LOG_FILE" 2>&1 