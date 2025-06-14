/*************************************************************************
  > File Name: main.cpp
  > Author: yosh20004
  > Mail: 2172622103@qq.com 
  > Created Time: 2024/7/17 19:24:10
 ************************************************************************/

#include "SRTReader.h"
#include "srtTransfer.h"
#include "ASSWriter.h"
#include <ctime>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include <filesystem>

namespace fs = std::filesystem;

std::string path;
std::string Tpath;

auto analyse(int argc, char* argv[]) -> const bool {
    if (argc == 1) {
        spdlog::error("必要参数缺失");
        return false;
    } else if (argc == 2) {
        path = argv[1];
        if (path.ends_with(".srt")) {
            Tpath = std::string(argv[1]);
            Tpath.erase(Tpath.length() - 3);
            Tpath += "ass";
            spdlog::info("输入文件: {}, 输出文件: {}", path, Tpath);
            return true;
        } else {
            spdlog::error("输入文件不是 .srt 格式: {}", path);
            return false;
        }
    } else {
        path = argv[1];
        Tpath = argv[2];
        spdlog::info("输入文件: {}, 输出文件: {}", path, Tpath);
        return true;
    }
}

int main(int argc, char* argv[]) {
    try {
        // 确保日志目录存在
        fs::create_directories("logs");
        
        // 初始化日志系统
        auto logger = spdlog::rotating_logger_mt("srt2ass", "logs/srt2ass.log", 1024*1024*5, 3);
        spdlog::set_default_logger(logger);
        spdlog::set_level(spdlog::level::debug);
        spdlog::flush_on(spdlog::level::debug);

        spdlog::info("开始转换字幕文件");
        
        if (!analyse(argc, argv)) {
            spdlog::error("参数分析失败");
            return -1;
        }

        spdlog::debug("开始读取SRT文件: {}", path);
        auto a = SRTReader(path);
        a.readLine();
        spdlog::debug("SRT文件读取完成");

        spdlog::debug("开始转换字幕格式");
        auto sT = srtTransfer(a.getEntry());
        spdlog::debug("字幕格式转换完成");

        spdlog::debug("开始写入ASS文件: {}", Tpath);
        auto aW = ASSWriter(Tpath, sT.getEntry());
        aW.write();
        spdlog::info("ASS文件写入完成: {}", Tpath);
        
        return 0;
    } catch (const std::exception& e) {
        // 如果日志系统还没有初始化，使用标准错误输出
        if (!spdlog::get("srt2ass")) {
            std::cerr << "错误: " << e.what() << std::endl;
        } else {
            spdlog::error("转换过程中发生错误: {}", e.what());
        }
        return -1;
    }
}

