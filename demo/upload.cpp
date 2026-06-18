#include "upload.h"
#include <iostream>
#include <fstream>
#include <dirent.h>
#include <cstring>
#include <sys/stat.h>

#include"utils.h"

using namespace std;



void handleApiUpload(const HttpRequest& req, HttpResponse& res)
{
    res.headers["Access-Control-Allow-Origin"] = "*";
    res.headers["Access-Control-Allow-Credentials"] = "true";
    res.headers["Content-Type"] = "application/json; charset=utf-8";

    string boundary;
    auto it = req.headers.find("Content-Type");
    if (it != req.headers.end()) {
        size_t pos = it->second.find("boundary=");
        if (pos != string::npos) {
            boundary = "--" + it->second.substr(pos + 9);
        }
    }

    if (boundary.empty()) {
        res.body = R"({"code":400,"msg":"上传格式错误"})";
        return;
    }

    size_t pos1 = req.body.find("filename=\"");
    if (pos1 == string::npos) {
        res.body = R"({"code":400,"msg":"未找到文件"})";
        return;
    }
    pos1 += 10;
    size_t pos2 = req.body.find("\"", pos1);
    string filename = req.body.substr(pos1, pos2 - pos1);

    size_t dataStart = req.body.find("\r\n\r\n", pos2) + 4;
    size_t dataEnd = req.body.find(boundary, dataStart) - 2;
    string fileData = req.body.substr(dataStart, dataEnd - dataStart);

    mkdirIfNotExists("./public/uploads");
    ofstream out("./public/uploads/" + filename, ios::binary);
    out.write(fileData.c_str(), fileData.size());
    out.close();

    res.body = R"({"code":200,"msg":"上传成功"})";
}

void handleApiFiles(const HttpRequest& req, HttpResponse& res)
{
    res.headers["Access-Control-Allow-Origin"] = "*";
    res.headers["Access-Control-Allow-Credentials"] = "true";
    res.headers["Content-Type"] = "application/json; charset=utf-8";

    DIR* dir = opendir("./public/uploads");
    if (!dir) {
        res.body = R"({"code":200,"files":[]})";
        return;
    }

    string json = "[";
    dirent* entry;
    while ((entry = readdir(dir))) {
        if (string(entry->d_name) == "." || string(entry->d_name) == "..")
            continue;
        if (json.size() > 1) json += ",";
        json += "\"" + string(entry->d_name) + "\"";
    }
    json += "]";
    closedir(dir);

    res.body = "{\"code\":200,\"files\":" + json + "}";
}

void handleApiDelete(const HttpRequest& req, HttpResponse& res) {
    // 从 POST body 里解析文件名
    auto json = parseSimpleJSON(req.body);
    std::string filename = json["name"];
    std::string filepath = "./public/uploads/" + filename;

    // 调试用：打印路径，确认是否正确
    printf("尝试删除文件: %s\n", filepath.c_str());

    // 执行删除
    if (remove(filepath.c_str()) == 0) {
        res.body = R"({"code":200,"msg":"删除成功"})";
    } else {
        res.body = R"({"code":500,"msg":"删除失败，文件不存在或无权限"})";
    }
    res.headers["Content-Type"] = "application/json; charset=utf-8";
}