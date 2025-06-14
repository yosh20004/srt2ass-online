#!/bin/bash

# 默认并发数
DEFAULT_CONCURRENCY=3

# 显示使用方法
show_usage() {
    echo "使用方法: $0 [并发数]"
    echo "参数说明:"
    echo "  并发数: 可选参数，指定并发测试的请求数量（默认为 $DEFAULT_CONCURRENCY）"
    echo "示例:"
    echo "  $0        # 使用默认并发数 $DEFAULT_CONCURRENCY 运行测试"
    echo "  $0 5      # 使用 5 个并发请求运行测试"
}

# 检查是否安装了 Python
if ! command -v python3 &> /dev/null; then
    echo "错误: 未找到 Python3，请先安装 Python3"
    exit 1
fi

# 检查是否安装了必要的 Python 包
if ! python3 -c "import requests" &> /dev/null; then
    echo "正在安装必要的 Python 包..."
    pip3 install requests
fi

# 获取并发数参数
concurrency=$1
if [ -z "$concurrency" ]; then
    concurrency=$DEFAULT_CONCURRENCY
fi

# 验证并发数是否为数字
if ! [[ "$concurrency" =~ ^[0-9]+$ ]]; then
    echo "错误: 并发数必须是数字"
    show_usage
    exit 1
fi

# 检查测试脚本是否存在
if [ ! -f "test/test.py" ]; then
    echo "错误: 未找到测试脚本 test/test.py"
    exit 1
fi

echo "开始运行测试..."
echo "并发数: $concurrency"
echo "----------------------------------------"

# 运行测试脚本
python3 test/test.py "$concurrency"

# 检查测试是否成功
if [ $? -eq 0 ]; then
    echo "----------------------------------------"
    echo "测试完成"
else
    echo "----------------------------------------"
    echo "测试过程中出现错误"
    exit 1
fi 