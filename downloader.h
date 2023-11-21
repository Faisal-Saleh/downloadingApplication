// downloader.h
#include <thread>
#include "urlsmanager.h"

class Downloader {
public:
    void start();
    void downloadUrl(const std::string& url, int depth);
    // Other methods...
private:
    UrlManager* urlManager;
    std::thread downloaderThread;
    // Other members...
};
