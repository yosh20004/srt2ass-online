#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <map>
#include "http_handler.h"
#include "file_handler.h"
#include "spdlog/spdlog.h"

namespace fs = std::filesystem;

std::string HttpHandler::handleRequest(const std::string& request) {
    std::istringstream requestStream(request);
    std::string method, path, version;
    requestStream >> method >> path >> version;

    if (method == "GET") {
        if (path == "/" || path == "/index.html") {
            return serveFile("public/index.html");
        }
        return "HTTP/1.1 404 Not Found\r\n\r\n";
    } else if (method == "POST" && path == "/convert") {
        return handleFileUpload(request);
    }

    return "HTTP/1.1 405 Method Not Allowed\r\n\r\n";
}

std::string HttpHandler::serveFile(const std::string& filePath) {
    spdlog::info("尝试打开文件: {}", filePath);
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        spdlog::error("文件打开失败: {}", filePath);
        return "HTTP/1.1 404 Not Found\r\n\r\n";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    std::string contentType = "text/html";
    if (filePath.length() >= 4 && filePath.substr(filePath.length() - 4) == ".css") {
        contentType = "text/css";
    } else if (filePath.length() >= 3 && filePath.substr(filePath.length() - 3) == ".js") {
        contentType = "text/javascript";
    }

    std::string response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: " + contentType + "; charset=utf-8\r\n";
    response += "Content-Length: " + std::to_string(content.length()) + "\r\n";
    response += "\r\n";
    response += content;

    return response;
}

std::string HttpHandler::handleFileUpload(const std::string& request) {
    // Find the boundary
    size_t boundaryPos = request.find("boundary=");
    if (boundaryPos == std::string::npos) {
        spdlog::error("未找到 boundary");
        return "HTTP/1.1 400 Bad Request\r\n\r\n";
    }

    std::string boundary = request.substr(boundaryPos + 9);
    boundary = boundary.substr(0, boundary.find("\r\n"));
    spdlog::debug("找到 boundary: {}", boundary);

    // 找到文件内容的开始位置
    std::string boundaryStart = "--" + boundary;
    std::string boundaryEnd = "--" + boundary + "--";
    
    size_t contentStart = request.find("\r\n\r\n", request.find(boundaryStart));
    if (contentStart == std::string::npos) {
        spdlog::error("未找到文件内容开始位置");
        return "HTTP/1.1 400 Bad Request\r\n\r\n";
    }
    contentStart += 4;  // 跳过 \r\n\r\n

    // 找到文件内容的结束位置
    size_t contentEnd = request.find(boundaryEnd, contentStart);
    if (contentEnd == std::string::npos) {
        spdlog::error("未找到文件内容结束位置");
        return "HTTP/1.1 400 Bad Request\r\n\r\n";
    }

    // 提取文件内容
    std::string fileContent = request.substr(contentStart, contentEnd - contentStart);
    
    // 移除末尾的 \r\n
    while (!fileContent.empty() && (fileContent.back() == '\r' || fileContent.back() == '\n')) {
        fileContent.pop_back();
    }

    spdlog::debug("提取的文件内容大小: {} 字节", fileContent.size());
    if (fileContent.empty()) {
        spdlog::error("文件内容为空");
        return "HTTP/1.1 400 Bad Request\r\n\r\n";
    }

    try {
        // Convert SRT to ASS
        std::string assContent = FileHandler::convertSrtToAss(fileContent);

        // Prepare response
        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: text/plain; charset=utf-8\r\n";
        response += "Content-Disposition: attachment; filename=subtitle.ass\r\n";
        response += "Content-Length: " + std::to_string(assContent.length()) + "\r\n";
        response += "\r\n";
        response += assContent;

        return response;
    } catch (const std::exception& e) {
        spdlog::error("转换失败: {}", e.what());
        return "HTTP/1.1 500 Internal Server Error\r\n\r\n";
    }
} 