#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <map>

// 函数声明（完整版本）
std::map<std::string, std::string> parseSimpleJSON(const std::string& body);

// 函数实现
inline std::map<std::string, std::string> parseSimpleJSON(const std::string& body) {
    std::map<std::string, std::string> res;
    size_t pos = 0;
    while (pos < body.size()) {
        // 查找键名（用双引号包裹）
        size_t keyStart = body.find('"', pos);
        if (keyStart == std::string::npos) break;
        size_t keyEnd = body.find('"', keyStart + 1);
        if (keyEnd == std::string::npos) break;
        std::string key = body.substr(keyStart + 1, keyEnd - keyStart - 1);

        // 查找值（用双引号包裹）
        size_t valStart = body.find('"', keyEnd + 1);
        if (valStart == std::string::npos) break;
        size_t valEnd = body.find('"', valStart + 1);
        if (valEnd == std::string::npos) break;
        std::string val = body.substr(valStart + 1, valEnd - valStart - 1);

        res[key] = val;
        pos = valEnd + 1;
    }
    return res;
}

#endif