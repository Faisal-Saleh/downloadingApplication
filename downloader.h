// downloader.h
#include <thread>
#include <string>
#include <iostream>
#include <fstream>
#include <gumbo.h>
#include <curl/curl.h>
#include <mutex>
#include "urlsmanager.h"
#include "logger.h"

#ifndef _DOWNLOADER_H_
#define _DOWNLOADER_H_

using namespace std;

class downloader {
private:
    urlsmanager* url_manager;
    thread downloader_thread;
    string main_url;
    string base_url;
    int depth;
    downloader(const downloader&);
    bool downloading_url;
public:
    bool is_downloading();
    downloader(urlsmanager* urlmanger);
    ~downloader() {}
    void start();
    string extract_base_url(string& inp_url);
    void download_html(string& downloaded_html);
    void parse_html(const char* html_content, string& file_name);
    void extract_text(GumboNode* node, ofstream& file);
    void extract_urls(GumboNode* node);
    string get_url_content_type(const char* url);
    void download_content(const char* url, const char* file_name);
};

#endif