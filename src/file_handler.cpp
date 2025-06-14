#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstdlib>
#include "file_handler.h"
#include "spdlog/spdlog.h"

namespace fs = std::filesystem;

std::string FileHandler::convertSrtToAss(const std::string& srtContent) {
    spdlog::info("convertSrtToAss 被调用");
    
    // 检查输入内容
    if (srtContent.empty()) {
        spdlog::error("输入内容为空");
        throw std::runtime_error("输入内容为空");
    }

    // 检查文件内容是否以数字开头（SRT 文件的第一行应该是序号）
    size_t firstNonSpace = srtContent.find_first_not_of(" \t\r\n");
    if (firstNonSpace == std::string::npos || !std::isdigit(srtContent[firstNonSpace])) {
        spdlog::error("文件内容格式不正确，第一行不是序号");
        throw std::runtime_error("文件内容格式不正确");
    }

    std::string tempSrtPath = "temp_" + std::to_string(std::time(nullptr)) + ".srt";
    std::string tempAssPath = tempSrtPath.substr(0, tempSrtPath.size() - 3) + "ass";
    spdlog::info("SRT路径: {}, ASS路径: {}", tempSrtPath, tempAssPath);

    // 写入临时文件
    std::ofstream tempFile(tempSrtPath);
    if (!tempFile.is_open()) {
        spdlog::error("无法创建临时文件: {}", tempSrtPath);
        throw std::runtime_error("无法创建临时文件");
    }
    tempFile << srtContent;
    tempFile.close();

    // 检查文件大小
    std::ifstream checkFile(tempSrtPath, std::ios::binary | std::ios::ate);
    size_t srtSize = checkFile.tellg();
    checkFile.close();
    spdlog::info("SRT文件大小: {} 字节", srtSize);

    if (srtSize == 0) {
        fs::remove(tempSrtPath);
        spdlog::error("写入的临时文件为空");
        throw std::runtime_error("写入的临时文件为空");
    }

    try {
        // 获取可执行文件路径，优先使用环境变量
        const char* exe_env = std::getenv("SRT2ASS_EXECUTABLE");
        std::string exe_path = exe_env ? exe_env : "./srt2ass_executable";
        spdlog::info("SRT2ASS_EXECUTABLE 路径: {}", exe_path);
        // 调用 srt2ass 可执行文件
        std::string cmd = exe_path + " " + tempSrtPath;
        spdlog::debug("执行命令: {}", cmd);
        int ret = system(cmd.c_str());
        spdlog::info("srt2ass_executable 返回值: {}", ret);

        if (ret != 0) {
            throw std::runtime_error("srt2ass 执行失败，返回码: " + std::to_string(ret));
        }

        // 检查 ASS 文件是否存在
        if (!fs::exists(tempAssPath)) {
            throw std::runtime_error("ASS 文件未生成");
        }

        // 读取 ASS 文件内容
        std::ifstream assFile(tempAssPath);
        if (!assFile.is_open()) {
            throw std::runtime_error("无法打开 ASS 文件: " + tempAssPath);
        }

        assFile.seekg(0, std::ios::end);
        size_t assSize = assFile.tellg();
        assFile.seekg(0, std::ios::beg);
        spdlog::info("ASS文件大小: {} 字节", assSize);

        if (assSize == 0) {
            throw std::runtime_error("生成的 ASS 文件为空");
        }

        std::stringstream assContent;
        assContent << assFile.rdbuf();
        assFile.close();

        // 清理临时文件
        fs::remove(tempSrtPath);
        fs::remove(tempAssPath);

        return assContent.str();
    } catch (const std::exception& e) {
        spdlog::error("SRT转ASS失败: {}", e.what());
        if (fs::exists(tempSrtPath)) fs::remove(tempSrtPath);
        if (fs::exists(tempAssPath)) fs::remove(tempAssPath);
        throw;
    }
} 