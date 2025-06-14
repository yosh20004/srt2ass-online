#pragma once

#include <string>

class HttpHandler {
public:
    static std::string handleRequest(const std::string& request);

private:
    static std::string serveFile(const std::string& filePath);
    static std::string handleFileUpload(const std::string& request);
}; 