#ifndef UPLOAD_H
#define UPLOAD_H

#include "http.h"

void handleApiUpload(const HttpRequest& req, HttpResponse& res);
void handleApiFiles(const HttpRequest& req, HttpResponse& res);
void handleApiDelete(const HttpRequest& req, HttpResponse& res);
void mkdirIfNotExists(const std::string& dir);

#endif