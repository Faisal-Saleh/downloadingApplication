// urlsmanager.h
#include <string>
#include <vector>
#include <deque>
#include <algorithm>
#include <thread>
#include <mutex>
#include "logger.h"

#ifndef _URLSMANAGER_H_
#define _URLSMANAGER_H_

using namespace std;

class urlsmanager {
private:
    Logger* logger;
    deque<pair<string, int>> url_depth_list;
    vector<string> visited_before;
    thread url_manager_thread;
    mutex lists_mutex;
public:
    mutex curl_mutex;
    urlsmanager(deque<pair<string, int>> url_list, Logger* logger);
    urlsmanager(const urlsmanager&);
    ~urlsmanager(void);
    void add_url(string& url, int depth);
    pair<string, int> get_url(void);
    void log(LogType type, const std::string& message);
    void start();
};

#endif