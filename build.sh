#!/bin/bash

# 设置颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 打印带颜色的信息
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 检查是否安装了必要的工具
check_dependencies() {
    print_info "检查依赖..."
    
    if ! command -v cmake &> /dev/null; then
        print_error "未找到 cmake，请先安装 cmake"
        exit 1
    fi
    
    if ! command -v g++ &> /dev/null; then
        print_error "未找到 g++，请先安装 g++"
        exit 1
    fi
}

# 创建并进入构建目录
setup_build_dir() {
    print_info "设置构建目录..."
    
    if [ ! -d "build" ]; then
        mkdir build
    fi
    
    cd build || {
        print_error "无法进入 build 目录"
        exit 1
    }
}

# 运行 CMake 配置
run_cmake() {
    print_info "运行 CMake 配置..."
    
    cmake .. || {
        print_error "CMake 配置失败"
        exit 1
    }
}

# 编译项目
build_project() {
    print_info "开始编译..."
    
    make -j$(nproc) || {
        print_error "编译失败"
        exit 1
    }
}

# 检查编译结果
check_build_result() {
    print_info "检查编译结果..."
    
    if [ ! -f "srt2ass_executable" ]; then
        print_error "srt2ass_executable 未生成"
        exit 1
    fi
    
    if [ ! -f "server" ]; then
        print_error "server 未生成"
        exit 1
    fi
    
    print_info "编译成功！"
    print_info "生成的文件："
    echo "  - srt2ass_executable"
    echo "  - server"
}

# 主函数
main() {
    print_info "开始构建项目..."
    
    check_dependencies
    setup_build_dir
    run_cmake
    build_project
    check_build_result
    
    print_info "构建完成！"
}

# 运行主函数
main 