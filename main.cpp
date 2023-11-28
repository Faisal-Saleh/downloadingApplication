/**
 * @file main.cpp
 * @author Faisal Abdelmonem
 * @brief  This is the main thread that sets up everything for the urlsmanager
 *         thread. The main function takes the input from the command line
 *         where we take the json file that has the url's we need to scrape
 *         along with the log file (if any) that we need to log our actions in.
 *         The main function parses through the input and creates an instance
 *         of the urlsmanager object and gives it all the url's it parsed.
 *         It also creates an instance of a logger and starts the UrlManager
 *         thread. In this function we also create two directories contents
 *         and text where we save the data we scraped there.
 * @date 2023-11-21
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include "urlsmanager.h"
#include "downloader.h"
#include "logger.h"
#include <gumbo.h>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <string>
#include <cstring>
#include <curl/curl.h>
#include <functional>
#include <assert.h>
#include <deque>
#include "json.hpp"

using json = nlohmann::json;
using namespace std;
namespace fs = filesystem;


int main(int argc, char* argv[]) {
    string log_file;

    if (argc == 3) {
        log_file = argv[2];
    }
    
    Logger logger(log_file);

    if (argc != 2 && argc != 3) {
        cerr << "Usage: " << argv[0] << " <json_file>" << "log_file" << endl;
        return 1;
    }

    string json_file_path = argv[1];

    // Read json from file
    ifstream file(json_file_path);
    if (!file.is_open()) {
        cerr << "Error opening file: " << json_file_path << endl;
        return 1;
    }

    json data;
    file >> data;

    // Read from the json data the urls and depths and store them in a vector
    deque<pair<string, int>> url_depth_list;
    for (const auto& entry : data) {
        url_depth_list.emplace_back(string(entry["url"]), entry["depth"]);
    }


    fs::create_directory("text");
    fs::create_directory("contents");

    {
        urlsmanager url_manager(url_depth_list, &logger); 

      // The destructor of myUrlsManager will be automatically called when this block exits
    } // The join in the destructor ensures that the thread has completed before the object is destroyed

    return 0;
}
