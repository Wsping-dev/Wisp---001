#include "db.h"
#include <iostream>
using namespace std;

// -------- 改成你自己的 --------
#define DB_HOST "localhost"
#define DB_USER "root"
#define DB_PASS "123456"      // 你的 MySQL 密码
#define DB_NAME "user_system" // 你的库名
#define DB_PORT 3306
// ------------------------------

static MYSQL *g_conn = nullptr;

MYSQL* getDBConn() {
    if (g_conn != nullptr) {
        // 简单断线重连
        if (mysql_ping(g_conn) == 0) {
            return g_conn;
        } else {
            cout << "连接断开，尝试重连..." << endl;
            mysql_close(g_conn);
            g_conn = nullptr;
        }
    }

    g_conn = mysql_init(nullptr);
    if (!g_conn) {
        cout << "mysql_init 失败" << endl;
        return nullptr;
    }

    if (!mysql_real_connect(
            g_conn,
            DB_HOST,
            DB_USER,
            DB_PASS,
            DB_NAME,
            DB_PORT,
            nullptr,
            0
    )) {
        cout << "连接失败: " << mysql_error(g_conn) << endl;
        mysql_close(g_conn);
        g_conn = nullptr;
        return nullptr;
    }

    mysql_query(g_conn, "set names utf8");
    cout << "数据库连接成功（user_system）" << endl;
    return g_conn;
}

void closeDBConn(MYSQL* conn) {
    // 长连接模式：这里不真正关闭
    // if (conn) mysql_close(conn);
}
