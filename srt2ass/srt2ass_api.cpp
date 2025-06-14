#include "srt2ass_api.h"
#include "SRTReader.h"
#include "srtTransfer.h"
#include "ASSWriter.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <ctime>
#include "spdlog/spdlog.h"

namespace fs = std::filesystem;

std::string srt2ass_convert(const std::string& srt_content) {
    // 生成唯一临时文件名
    std::string tempSrtPath = "temp_" + std::to_string(std::time(nullptr)) + ".srt";
    std::string tempAssPath = tempSrtPath.substr(0, tempSrtPath.size() - 3) + "ass";
    spdlog::info("[srt2ass_convert] SRT路径: {}, ASS路径: {}", tempSrtPath, tempAssPath);
    // 写入SRT内容
    std::ofstream tempFile(tempSrtPath);
    tempFile << srt_content;
    tempFile.close();
    try {
        // 读取SRT
        SRTReader reader(tempSrtPath);
        int lineCount = 0;
        while (true) {
            try {
                spdlog::info("readLine 第 {} 次", ++lineCount);
                reader.readLine();
            } catch (const SRTReaderError& e) {
                spdlog::info("readLine 抛出异常: {}", e.what());
                if (std::string(e.what()) == std::string("EOF")) break;
                throw;
            }
        }
        // 转换
        srtTransfer transfer(reader.getEntry());
        auto assSubtitles = transfer.getEntry();
        // 写入ASS
        ASSWriter writer(tempAssPath, assSubtitles);
        writer.write();
        // 读取ASS内容
        std::ifstream assFile(tempAssPath);
        std::stringstream assContent;
        assContent << assFile.rdbuf();
        assFile.close();
        // 清理
        fs::remove(tempSrtPath);
        fs::remove(tempAssPath);
        return assContent.str();
    } catch (const std::exception& e) {
        spdlog::error("srt2ass_convert异常: {}", e.what());
        if (fs::exists(tempSrtPath)) fs::remove(tempSrtPath);
        if (fs::exists(tempAssPath)) fs::remove(tempAssPath);
        throw;
    } catch (...) {
        spdlog::error("srt2ass_convert发生未知异常");
        if (fs::exists(tempSrtPath)) fs::remove(tempSrtPath);
        if (fs::exists(tempAssPath)) fs::remove(tempAssPath);
        throw;
    }
} 