#include "SRTChecker.h"
#include <vector>
#include "timecheck.h"
#include "spdlog/spdlog.h"

struct IdxParseError : SRTCheckError {
    IdxParseError(const char* msg) : SRTCheckError(msg) {}
    std::vector<int> nums;
};

struct FileParseError : SRTCheckError {
    FileParseError(const char* msg) : SRTCheckError(msg) {}
};

struct TimeStampError : SRTCheckError {
    TimeStampError(const char* msg) : SRTCheckError(msg) {}
    std::vector<std::string_view> errLines;
};

const bool SRTChecker::test() {
    spdlog::debug("开始检查SRT文件格式");

    try {
        empty();
    } catch (FileParseError& e) {
        spdlog::error("文件解析错误: {}", e.what());
        return false;
    }

    try {
        idx_check();
    } catch (IdxParseError& e) {
        spdlog::warn("索引检查警告: {}", e.what());
        int showNum = e.nums.size() >= 4 ? 3 : e.nums.size();
        std::string missingIndices;
        for (int i = 0; i < showNum; ++i) {
            missingIndices += std::to_string(e.nums[i]) + " ";
        }
        if (showNum == 3) {
            missingIndices += "...";
        }
        spdlog::warn("缺失的索引: {}", missingIndices);
    }

    try {
        time_check();
    } catch (TimeStampError& e) {
        spdlog::error("时间戳格式错误: {}", e.what());
        int showNum = e.errLines.size() >= 4 ? 3 : e.errLines.size();
        std::string errorLines;
        for (int i = 0; i < showNum; i++) {
            errorLines += std::string(e.errLines[i]) + "\n";
        }
        if (showNum == 3) {
            errorLines += "...";
        }
        spdlog::error("错误的时间戳行:\n{}", errorLines);
        return false;
    }

    spdlog::debug("SRT文件格式检查通过");
    return true;
}

void SRTChecker::idx_check() {
    IdxParseError e("部分索引缺失");

    for (int correct_idx = 1; const auto& i : this -> file_data) {
        if (i.index.value_or(-1) != correct_idx) {
            e.nums.push_back(correct_idx);
        } 
        correct_idx += 1;
    }

    if (!e.nums.empty()) {
        throw e;
    }
}

void SRTChecker::empty() {
    if (this -> file_data.empty()) {
        spdlog::error("文件为空或解析失败");
        throw FileParseError("原始文件解析失败");
    }
}

void SRTChecker::time_check() {
    TimeStampError e("时间戳格式非法");

    for (const auto& i : this -> file_data) {
        if (!__check(i.Time)) {
            e.errLines.push_back(i.Time);
        }
    }

    if (!e.errLines.empty()) {
        throw e;
    }
}


