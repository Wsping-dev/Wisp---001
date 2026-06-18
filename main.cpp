#include <iostream>
#include <string>
#include <map>
#include <functional>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sstream>
#include <vector>
#include <cstring>
#include <time.h>
#include <arpa/inet.h>
#include <thread>
#include <mutex>
#include <fstream>
#include <sys/stat.h>
#include <openssl/md5.h>
#include <cstdlib>
#include <openssl/evp.h>
#include <dirent.h>
#include "http.h"
#include "db.h"
#include "upload.h"
#include"utils.h"


using namespace std;


const int PORT = 8080;
const int BUFFER_SIZE = 4096;
const string WEB_ROOT = "./public";

mutex log_mutex;
mutex session_mtx;





// 把 %E4%BD%A0 这种还原成 UTF-8 中文
std::string url_decode(const std::string &s) {
    std::string res;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            char hex[3] = {s[i+1], s[i+2], 0};
            int val = strtol(hex, nullptr, 16);
            res.push_back((char)val);
            i += 2;
        } else if (s[i] == '+') {
            res.push_back(' ');
        } else {
            res.push_back(s[i]);
        }
    }
    return res;
}

// -------------------------- MD5 --------------------------
string md5(const string& s) {
    unsigned char res[EVP_MAX_MD_SIZE];
    unsigned int len = 0;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_md5(), nullptr);
    EVP_DigestUpdate(ctx, s.c_str(), s.size());
    EVP_DigestFinal_ex(ctx, res, &len);
    EVP_MD_CTX_free(ctx);

    char hex[33] = {0};
    for (unsigned int i = 0; i < len; ++i)
        sprintf(hex + i*2, "%02x", res[i]);
    return string(hex);
}

// -------------------------- Session --------------------------
map<string, string> sessions;

string createSession(const string& user) {
    lock_guard<mutex> lock(session_mtx);
    string sid = to_string(time(0)) + to_string(rand());
    sessions[sid] = user;
    return sid;
}

bool checkSession(const string& sid, string& user) {
    lock_guard<mutex> lock(session_mtx);
    auto it = sessions.find(sid);
    if (it == sessions.end()) return false;
    user = it->second;
    return true;
}

// -------------------------- 路由 --------------------------
using RouteHandler = function<void(const HttpRequest&, HttpResponse&)>;
map<string, map<string, RouteHandler>> routes;

void addRoute(const string& method, const string& path, RouteHandler handler) {
    routes[method][path] = handler;
}

bool findRoute(const HttpRequest& req, RouteHandler& handler) {
    auto methodIt = routes.find(req.method);
    if (methodIt == routes.end()) return false;

    // 截断 URL 参数 ? 后面的内容
    string path = req.path;
    size_t pos = path.find('?');
    if (pos != string::npos) {
        path = path.substr(0, pos);
    }

    auto pathIt = methodIt->second.find(path);
    if (pathIt == methodIt->second.end()) return false;

    handler = pathIt->second;
    return true;
}

// -------------------------- 工具 --------------------------
string getCurrentTime() {
    char buf[64];
    time_t now = time(0);
    struct tm tm = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

string getTodayDate() {
    return getCurrentTime().substr(0,10);
}

void logAccessToFile(const string& ip, const HttpRequest& req, int code) {
    lock_guard<mutex> lock(log_mutex);
    string date = getTodayDate();
    ofstream logfile("./logs/" + date + ".log", ios::app);
    logfile << "[" << getCurrentTime() << "] " << ip << " "
            << req.method << " " << req.path << " " << code << endl;
    cout << "[" << getCurrentTime() << "] " << ip << " "
         << req.method << " " << req.path << " " << code << endl;
}

string getContentType(const string& path) {
    if (path.size() >=5 && path.substr(path.size()-5)==".html") return "text/html";
    if (path.size() >=4 && path.substr(path.size()-4)==".css") return "text/css";
    if (path.size() >=3 && path.substr(path.size()-3)==".js") return "application/javascript";
    if (path.size() >=4 && path.substr(path.size()-4)==".png") return "image/png";
    if ((path.size() >=4 && path.substr(path.size()-4)==".jpg") ||
        (path.size() >=5 && path.substr(path.size()-5)==".jpeg"))
        return "image/jpeg";
    if (path.size() >=4 && path.substr(path.size()-4)==".ico") return "image/x-icon";
    return "text/plain";
}

bool readFile(const string& path, string& content) {
    ifstream file(path, ios::binary);
    if (!file) return false;
    content = string((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    return true;
}

void mkdirIfNotExists(const string& dir) {
    struct stat st;
    if (stat(dir.c_str(), &st) != 0)
        mkdir(dir.c_str(), 0755);
}

string getOrigin(const HttpRequest& req) {
    auto it = req.headers.find("Origin");
    if (it != req.headers.end()) {
        return it->second;
    }
    return "http://127.0.0.1:8080"; // 兜底
}

// -------------------------- 路由处理 --------------------------
void handleRoot(const HttpRequest& req, HttpResponse& res) {
    res.body = "<h1>Hello, Root!</h1>";
    res.headers["Content-Type"] = "text/html; charset=utf-8";
}

void handleUser(const HttpRequest& req, HttpResponse& res) {
    string cookie;
    auto it = req.headers.find("Cookie");
    if (it != req.headers.end()) cookie = it->second;

    string sid, user;
    if (cookie.size()>=4 && cookie.substr(0,4)=="sid=") sid = cookie.substr(4);

    if (!checkSession(sid, user)) {
        res.statusCode = 403;
        res.body = "<h1 style='color:red'>403 未登录</h1>";
        res.headers["Content-Type"] = "text/html; charset=utf-8";
        return;
    }
    res.body = "<h1>欢迎，" + user + "</h1><p>您已登录</p>";
    res.headers["Content-Type"] = "text/html; charset=utf-8";
}

void handleAbout(const HttpRequest& req, HttpResponse& res) {
    res.body = "<h1>About This Server</h1>";
    res.headers["Content-Type"] = "text/html; charset=utf-8";
}

void handleApiLogin(const HttpRequest& req, HttpResponse& res)
{
    res.headers["Access-Control-Allow-Origin"] = getOrigin(req);
    res.headers["Access-Control-Allow-Credentials"] = "true";

    auto json = parseSimpleJSON(req.body);
    string username = json["username"];
    string password = json["password"];
    string pwdMd5 = md5(password);

    MYSQL* conn = getDBConn();
    if (!conn)
    {
        res.body = R"({"code":500,"msg":"数据库连接失败"})";
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        return;
    }

    char sql[256] = {0};
    sprintf(sql, "select * from users where username='%s' and password='%s'",
        username.c_str(), pwdMd5.c_str());
    int ret = mysql_query(conn, sql);

    if (ret == 0)
    {
        MYSQL_RES* result = mysql_store_result(conn);
        MYSQL_ROW row = mysql_fetch_row(result);
        if (row)
        {
            string sid = createSession(username);
            // ✅ 修复 Cookie，让前端能正常登录跳转
            res.headers["Set-Cookie"] = "sid=" + sid + "; Path=/";
            res.body = R"({"code":200,"msg":"登录成功"})";
        }
        else
        {
            res.body = R"({"code":401,"msg":"用户名或密码错误"})";
        }
        mysql_free_result(result);
    }
    else
    {
        res.body = R"({"code":500,"msg":"查询异常"})";
    }
    closeDBConn(conn);
    res.headers["Content-Type"] = "application/json; charset=utf-8";
}

void handleApiRegister(const HttpRequest& req, HttpResponse& res) {
    res.headers["Access-Control-Allow-Origin"] = getOrigin(req);
    res.headers["Access-Control-Allow-Credentials"] = "true";
    auto json = parseSimpleJSON(req.body);
    string username = json["username"];
    string password = json["password"];
    string pwdMd5 = md5(password);

    if (username.empty() || password.empty()) {
        res.body = R"({"code":400,"msg":"用户名或密码不能为空"})";
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        return;
    }

    MYSQL* conn = getDBConn();
    if (!conn) {
        res.body = R"({"code":500,"msg":"数据库连接失败"})";
        res.headers["Content-Type"] = "application/json; charset=utf-8";
        return;
    }

    char checkSql[256] = {0};
    sprintf(checkSql, "select * from users where username='%s'", username.c_str());
    mysql_query(conn, checkSql);
    MYSQL_RES* resCheck = mysql_store_result(conn);
    MYSQL_ROW row = mysql_fetch_row(resCheck);

    if (row) {
        res.body = R"({"code":409,"msg":"用户名已存在"})";
        mysql_free_result(resCheck);
    } else {
        char insertSql[256] = {0};
        sprintf(insertSql, "insert into users(username,password) values('%s','%s')",
                username.c_str(), pwdMd5.c_str());
        if (mysql_query(conn, insertSql) == 0)
            res.body = R"({"code":200,"msg":"注册成功"})";
        else
            res.body = R"({"code":500,"msg":"注册失败"})";
    }
    closeDBConn(conn);
    res.headers["Content-Type"] = "application/json; charset=utf-8";
}

void handleApiUsers(const HttpRequest& req, HttpResponse& res) {
    res.headers["Content-Type"] = "application/json; charset=utf-8";
    res.headers["Access-Control-Allow-Origin"] = getOrigin(req);
    res.headers["Access-Control-Allow-Credentials"] = "true";
    MYSQL* conn = getDBConn();
    if (!conn) {
        res.body = R"({"code":500,"msg":"DB error"})";
        return;
    }
    mysql_query(conn, "select username from users");
    MYSQL_RES* result = mysql_store_result(conn);
    MYSQL_ROW row;
    string json = "[";
    while ((row = mysql_fetch_row(result))) json += "\"" + string(row[0]) + "\",";
    if (json.size()>1) json.pop_back();
    json += "]";
    res.body = json;
    mysql_free_result(result);
    closeDBConn(conn);
}


// -------------------------- 请求分发 --------------------------
HttpResponse handleRequest(const HttpRequest& req) {
    HttpResponse res;
    RouteHandler handler;

    if (findRoute(req, handler)) {
        handler(req, res);
        return res;
    }

    string decoded_path = url_decode(req.path);
    string filePath = WEB_ROOT + decoded_path;
    if (filePath == WEB_ROOT + "/") filePath = WEB_ROOT + "/index.html";

    struct stat st{};
    if (stat(filePath.c_str(), &st) == 0 && readFile(filePath, res.body)) {
        res.headers["Content-Type"] = getContentType(filePath);
    } else {
        res.statusCode = 404;
        res.body = R"(
        <html><head><title>404 Not Found</title></head>
        <body style="text-align:center;margin-top:50px;">
            <h1 style="color:red;font-size:48px;">404</h1>
            <p>页面不存在</p>
        </body></html>
        )";
        res.headers["Content-Type"] = "text/html; charset=utf-8";
    }
    return res;
}

HttpRequest parseRequest(const string& raw) {
    HttpRequest req;
    istringstream iss(raw);
    string line;
    getline(iss, line);
    istringstream ls(line);
    ls >> req.method >> req.path >> req.version;

    while (getline(iss, line) && line != "\r" && !line.empty()) {
        if (line.back() == '\r') line.pop_back();
        size_t colon = line.find(": ");
        if (colon != string::npos) {
            string key = line.substr(0, colon);
            string val = line.substr(colon + 2);
            req.headers[key] = val;
        }
    }
    string body((istreambuf_iterator<char>(iss)), {});
    req.body = body;
    return req;
}

void handleClient(int client_fd, struct sockaddr_in client_addr) {
    char buffer[BUFFER_SIZE] = {0};
    string ip = inet_ntoa(client_addr.sin_addr);

    struct timeval timeout{};
    timeout.tv_sec = 1;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    string rawRequest;
    ssize_t n;
    while ((n = read(client_fd, buffer, BUFFER_SIZE)) > 0)
        rawRequest.append(buffer, n);

    if (rawRequest.empty()) { close(client_fd); return; }

    HttpRequest req = parseRequest(rawRequest);
    HttpResponse res = handleRequest(req);
    logAccessToFile(ip, req, res.statusCode);

    string resp = res.toString();
    send(client_fd, resp.c_str(), resp.size(), 0);
    close(client_fd);
}

int main() {
    mkdirIfNotExists("./public");
    mkdirIfNotExists("./public/uploads");
    mkdirIfNotExists("./logs");

    addRoute("GET",   "/",           handleRoot);
    addRoute("GET",   "/user",       handleUser);
    addRoute("GET",   "/about",      handleAbout);
    addRoute("POST",  "/api/login",  handleApiLogin);
    addRoute("POST",  "/api/register", handleApiRegister);
    addRoute("GET",   "/api/users",  handleApiUsers);
    addRoute("POST",  "/api/upload", handleApiUpload);
    addRoute("GET",   "/api/files",  handleApiFiles);
    addRoute("GET",   "/api/delete", handleApiDelete);
    addRoute("POST", "/api/delete", handleApiDelete);

    auto optionsHandler = [&](const HttpRequest& req, HttpResponse& res) {
    string origin = getOrigin(req);
    res.headers["Access-Control-Allow-Origin"] = origin;
    res.headers["Access-Control-Allow-Credentials"] = "true";
    res.headers["Access-Control-Allow-Methods"] = "POST,GET,OPTIONS";
    res.headers["Access-Control-Allow-Headers"] = "Content-Type";
    };

    addRoute("OPTIONS", "/api/login", optionsHandler);
    addRoute("OPTIONS", "/api/register", optionsHandler);
    addRoute("OPTIONS", "/api/upload", optionsHandler);
    addRoute("OPTIONS", "/api/files", optionsHandler);
    addRoute("OPTIONS", "/api/delete", optionsHandler);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10);

    cout << "✅ 服务器启动：http://127.0.0.1:8080" << endl;

    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        thread t(handleClient, client_fd, client_addr);
        t.detach();
    }
    close(server_fd);
    return 0;
}