#pragma once

#include <string>

class FileHandler {
public:
    static std::string convertSrtToAss(const std::string& srtContent);
}; 