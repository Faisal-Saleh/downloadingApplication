/**
 * @file urlsmanager.cpp
 * @author Faisal Abdelmonem
 * @brief  This file contains the implementation of the url manager.
 *         The url manager keeps track of all the urls we need to scrape
 *         and their respective depths. As a small optimization I also keep
 *         track of the urls we visited before so that I don't download them
 *         again. The url manager starts 4 downloader threads and when each
 *         thread is started it is detached and runs independantly. I chose 4
 *         because it is a number that is less than the number of cores in the
 *         wsl environment and mainly just to show multithreading in the program
 *         however at the end of the day libcurl which I use to perform the get 
 *         requests has some shared state and therefore required synchronization
 *         that makes it appear sequential. After the 4 downloader threads are
 *         running each thread asks the url manager for a url to download using
 *         the get_url function and if they ever find urls with depth > 0 they
 *         add them to the url manager using add_url. whenever a downloader
 *         thread also downloads a url or fails on it, it reports back to the 
 *         url manager using the logger.
 * @date 2023-11-25
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include "urlsmanager.h"
#include "downloader.h"

/**
 * @brief Construct a new urlsmanager::urlsmanager object.
 * 
 * @param url_list The json parsed list given from the main function that
 *                 we need to scrape.
 * @param logger   The logger that the downloader thread uses to communicate
 *                 back the status of the downloads.
 */
urlsmanager::urlsmanager(deque<pair<string, int>> url_list, Logger* logger): url_depth_list(url_list), logger(logger) {
    url_manager_thread = thread(&urlsmanager::start, this);
}

/**
 * @brief Destroy the urlsmanager::urlsmanager object, this destructor is called
 *        automatically when the object exists from the scope and it'll 
 *        immediately call join making the main thread wait on the urls manager.
 * 
 */
urlsmanager::~urlsmanager() {
    if (url_manager_thread.joinable()) {
        url_manager_thread.join();
    }
}

/**
 * @brief Function that the downloader thread uses when it finds a url
 *        that it should visit next. if the depth is zero then we are
 *        not required download the content at that url.
 * 
 * @param url Url to be downloaded next.
 * @param depth Depth of the url we will download.
 */
void urlsmanager::add_url(string& url, int depth) {
    std::lock_guard<std::mutex> lock(mutex);  // Lock to ensure thread safety

    if (depth <= 0) return; // Make sure you are not adding a url with depth le 0
    auto it = find(visited_before.begin(), visited_before.end(), url);

    // Haven't seen this url before
    if (it == visited_before.end()) {
        visited_before.emplace_back(url);
        url_depth_list.emplace_back(url, depth);
    }
}

/**
 * @brief When the downloader thread is done downloading the url it calls this 
 *        function to find a new url to download if the function returns {"", -1}
 *        then there are no more urls to download and the downloader thread will
 *        exit.
 * 
 * @return pair<string, int> A pair of a url and its depth to be downloaded
 */
pair<string, int> urlsmanager::get_url(void) {
    std::lock_guard<std::mutex> lock(mutex);  // Lock to ensure thread safety

    while (!url_depth_list.empty()) {
        auto res = url_depth_list.front();
        url_depth_list.pop_front();
        if (res.second != 0) {
            return res;
        }
    }
    return {"", -1};
}

/**
 * @brief The downloader threads use this function to log the status of the
 *        downloads. The main thread sets up the logger with the logger file
 *        and the threads only log two messages; INFO and ERROR
 * 
 * @param type the LogType of the message; INFO, ERROR.
 * @param message the message to be logged in the file.
 */
void urlsmanager::log(LogType type, const std::string& message) {
    logger->log(type, message);
}

/**
 * @brief The function that is run when a thread is created. This function acts
 *        as a main function for this file. Here we create all the downloader
 *        threads and then we loop as long as there are threads still downloading
 *        this is important because when this function exits the main thread
 *        will join on it and then the main function will exit and the program
 *        will terminate which will prevent us from downloading.
 * 
 */
void urlsmanager::start() {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    vector<downloader*> downloader_threads;

    for(size_t i = 0; i < 4; i++) {
        downloader downloader_thread(this);
        downloader_threads.emplace_back(&downloader_thread);
    }

    bool downloading = false;

    while(true) {
        for (downloader* dthread : downloader_threads) {
            downloading = downloading || dthread->is_downloading();
        }
        
        if (!downloading) {
            break;
        } else {
            downloading = false;
        }
    }

    cout << "Done downloading the urls\n";

    curl_global_cleanup();

}
