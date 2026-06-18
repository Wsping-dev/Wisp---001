#ifndef HTTP_H
#define HTTP_H

#include <string>
#include <map>
#include <sstream>

struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct HttpResponse {
    int statusCode = 200;
    std::string statusText = "OK";
    std::map<std::string, std::string> headers;
    std::string body;

    std::string toString() const {
        std::stringstream ss;
        ss << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n";
        for (auto& p : headers) {
            ss << p.first << ": " << p.second << "\r\n";
        }
        ss << "Content-Length: " << body.size() << "\r\n";
        ss << "Connection: close\r\n\r\n";
        ss << body;
        return ss.str();
    }
};

#endif