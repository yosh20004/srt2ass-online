#include "SRTReader.h"
#include <cstdlib>
#include <fstream>


struct FileNotExist : SRTReaderError {
    const std::string& path;
};


void removeBOM(std::string& str) {
    const unsigned char BOM[] = {0xEF, 0xBB, 0xBF};
    if (str.size() >= 3 &&
            (unsigned char)str[0] == BOM[0] &&
            (unsigned char)str[1] == BOM[1] &&
            (unsigned char)str[2] == BOM[2]) {
        str.erase(0,3);
    }
    //有些文件开头有bom 处理之
}


SRTReader::SRTReader(const std::string& _path) : path(_path) {
    auto is_file_exist = [&_path]() {
        std::ifstream in(_path);
        if (!in.is_open()) {
            throw FileNotExist{"[severe error] file not existed: ", _path};
        }
    };

    try {
        is_file_exist();
    } catch (FileNotExist& e) {
        std::cout << e.what() ;
        std::cout << e.path << std::endl;
    }

}

const bool SRTReader::isNumber(std::string& str) {
    /*检查该行是否为数字*/
    removeBOM(str);
    const int offset = str.ends_with("\r") ? 1 : 0; 

    if (!str.empty() && std::all_of(str.begin(), str.end() - offset, [](char c){
                return std::isdigit(c);}))
        return true;

    else return false;
}



const bool SRTReader::check_empty(std::string_view data) {
    // 该函数在 readLine 中被调用，此时行尾的 \r\n 已被移除。
    // 判断是否为空行或仅包含空白字符。
    return data.find_first_not_of(" \t\r\n") == std::string_view::npos;
}



void SRTReader::readLine() {
    const std::string& input_file = this -> path;
    std::ifstream in(input_file, std::ios::binary);
    std::string data;
    SubtitleEntry a;

    enum STEP {
        initIndex,
        initTime,
        initText,
        __blank,
        skipblank
    } state= initIndex;


    auto __initIndex = [&]() -> STEP {
        if (check_empty(data)) {
            return initIndex;
        }
        if (isNumber(data)) {
            a.index = stoi(data);
            return initTime;
        } else {
            a.Time = data;
            a.index = std::nullopt;
            return initText;
        }
    };

    auto __initTime = [&]() -> STEP {
        a.Time = data;
        // 移除行尾的 \r\n 或 \n
        while (!a.Time.empty() && (a.Time.back() == '\r' || a.Time.back() == '\n')) {
            a.Time.pop_back();
        }
        a.text = "";
        return initText;
    };

    auto __initText = [&]() -> STEP {
        if (check_empty(data)) {
            this->file_data.push_back(a);
            return initIndex;
        } else {
            // 移除行尾的 \r\n 或 \n
            std::string line = data;
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
                line.pop_back();
            }
            if (!a.text.empty()) {
                a.text += "\n";
            }
            a.text += line;
        }
        return initText;
    };


    while (std::getline(in, data)) {
        // 移除行尾的 \r\n 或 \n
        while (!data.empty() && (data.back() == '\r' || data.back() == '\n')) {
            data.pop_back();
        }

        if (state == initIndex) {
            state = __initIndex();
        }
        else if (state == initTime) {
            state = __initTime();
        }
        else if (state == initText) {
            state = __initText();
        }
    }

    if (state == initText) {file_data.push_back(a);} 
    /*有时候文件结尾没有空行
     *导致最后一个字幕块保存不下来 这一句代码尝试解决之
     */
}



void SRTReader::__test_write_back() {
    std::ofstream out("./test.ass", std::ios::binary);
    auto& t = this -> getEntry();

    for (const auto& i : t) {
        try {
            out << std::to_string(i.index.value()) + "\r";
            out << i.Time + "\r";
            out << i.text + "\r";
            out << "\r";
        } catch (const std::exception& e) {
            out << i.Time + "\r";
            out << i.text + "\r";
            out << "\r";
        }
    }
}

