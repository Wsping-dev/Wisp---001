#ifndef DB_H
#define DB_H

#include <mysql/mysql.h>

MYSQL* getDBConn();
void closeDBConn(MYSQL* conn);

#endif