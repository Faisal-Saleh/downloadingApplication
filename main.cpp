// main.cpp
//#include "urlsmanager.h"
//#include "downloader.h"
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
#include <vector>
#include "json.hpp"

using json = nlohmann::json;
using namespace std;
namespace fs = filesystem;

// Callback function to write received data to a string
size_t text_callback(void* contents, size_t size, size_t nmemb, string* output) {
    size_t total_size = size * nmemb;
    output->append((char*)contents, total_size);
    return total_size;
}

// Callback function for libcurl to write downloaded content to a file
static size_t write_callback(void* contents, size_t size, size_t nmemb, FILE* file) {
    return fwrite(contents, size, nmemb, file);
}

/**
 * @brief Function to download the content at the given url and
 *        save it in a file that has the name file_name.
 *        The content that this function might store could be 
 *        an image or an audio or a video
 * 
 * @param url  The http url that we need to perform a get request at.
 * @param file_name The name of the file we need to create and save the content in
 */
void download_content(const char* url, const char* file_name) {
    CURL* curl = curl_easy_init();

    if (curl) {
        FILE* file = fopen(file_name, "wb");

        if (file) {
            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);

            // Perform the request
            CURLcode res = curl_easy_perform(curl);

            // Check for errors
            if (res != CURLE_OK) {
                fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            }

            // Clean up
            fclose(file);
        } else {
            fprintf(stderr, "Failed to open file for writing\n");
        }

        // Clean up
        curl_easy_cleanup(curl);
    }
}

// Function that manually extracts the base url (header)
// from the full url given in the json file.
string extract_base_url(string& url) {
    // Find the position of the first '/' after the "://" part
    size_t pos = url.find("://");
    if (pos != string::npos) {
        pos = url.find('/', pos + 3); // Skip the "://" part and find the next '/'
        if (pos != string::npos) {
            return url.substr(0, pos);
        }
    }

    return ""; // Return an empty string if no match is found
}

string get_url_content_type(const char* url) {
    CURL* curl;
    CURLcode res;

    curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);

        // Set the callback function to handle the response
        string response;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, text_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        // Perform the request
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            // Check the Content-Type header in the response
            char* content_type;
            res = curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type);

            if (res == CURLE_OK) {
                string result(content_type);
                // Clean up
                curl_easy_cleanup(curl);
                return result;
            } else {
                fprintf(stderr, "curl_easy_getinfo() failed: %s\n", curl_easy_strerror(res));
                // Clean up
                curl_easy_cleanup(curl);
                return "";
            }
        }
    }
    
    // Clean up
    curl_easy_cleanup(curl);

    return "";

}

/**
 * @brief Recursively visits the Gumbo nodes and gets the content type of the
 *        current node. If it finds content that should be downloaded it will
 *        setup the filename and call the download_content function to download
 *        that content. If it finds a url that we can visit it should add it 
 *        to the list of new urls we should visit.
 * 
 * @param node     The current GumboNode we are pointing at.
 * @param base_url The base url in case we find a relative url.
 */
void extract_urls(GumboNode* node, string& base_url) {
    if (node->type == GUMBO_NODE_ELEMENT) {
        GumboAttribute *attr = nullptr;
        attr = gumbo_get_attribute(&node->v.element.attributes, "href");
        if (!attr) 
            attr = gumbo_get_attribute(&node->v.element.attributes, "src");

        if (attr) {
            string url(attr->value);
            // Check if the url is already an absolute one
            if (url.find("://") == string::npos) {
                cout << "found a relative url" << endl;
                url = base_url + url; // a relative url is found
            }

            // cout << "Found URL: " << url << endl;
            string content_type = get_url_content_type(url.c_str());

            // If the content is an image or a video or an audio download it.
            // Note: if the content is base64 encoded it will not recognize it
            // TODO: take care of this later (maybe?)
            if (content_type.find("image") != string::npos ||
                content_type.find("video") != string::npos ||
                content_type.find("audio") != string::npos) {
                
                string filename = url.substr(url.find_last_of('/') + 1);
                string path = "contents/" + filename;
                download_content(url.c_str(), path.c_str()); 
            } else if (content_type.find("html") != string::npos) {
                // Add this to the list of urls we extracted
            }
            
        }

        GumboVector* children = &node->v.element.children;
        for (unsigned int i = 0; i < children->length; ++i) {
            extract_urls(static_cast<GumboNode*>(children->data[i]), base_url);
        }
    }
}

// Function to recursively extract text content
void extract_text(GumboNode* node, ofstream& file) {
    if (node->type == GUMBO_NODE_TEXT) {
        // cout << node->v.text.text << " ";
        file << node->v.text.text << " ";
    } else if (node->type == GUMBO_NODE_ELEMENT &&
               node->v.element.tag != GUMBO_TAG_SCRIPT &&
               node->v.element.tag != GUMBO_TAG_STYLE) {
        
        // Recursive call to visit child nodes
        GumboVector* children = &node->v.element.children;
        for (size_t i = 0; i < children->length; i++) {
            extract_text((GumboNode*)children->data[i], file);
        }
    }
}

/**
 * @brief  Function to parse HTML and extract the text, images, audios
 *         and videos using the gumbo parser
 * 
 * @param html_content pure html string
 * @param base_url     some content is given as relaticve urls so we use this 
 *                     to find the absolute url and use curl again to retrieve
 *                     the content.
 * @param file_name    the file name that we store the text at.
 */
void parse_html(const char* html_content, string& base_url, string& file_name) {
    ofstream file(file_name);
    GumboOutput* output = gumbo_parse(html_content);
    extract_text(output->root, file);
    extract_urls(output->root, base_url);
    gumbo_destroy_output(&kGumboDefaultOptions, output);
    file.close();
}

/**
 * @brief This function is called at the beginning to download the pure html
 *        at the given url and store it inside the given string.
 * 
 * @param url Html link that we need to retreive the pure html from.
 * @param downloaded_html String that we save the html in.
 */
void download_html(const char* url, string& downloaded_html) {
    CURL* curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, text_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &downloaded_html);

        // Perform the request
        CURLcode res = curl_easy_perform(curl);

        // Check for errors
        if (res != CURLE_OK) {
            cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << " \n";
        }

        // Clean up
        curl_easy_cleanup(curl);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <json_file_path>" << endl;
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
    vector<pair<string, int>> url_depth_vector;
    for (const auto& entry : data) {
        url_depth_vector.emplace_back(entry["url"], entry["depth"]);
    }

    for (const auto& entry : url_depth_vector) {
        string url = entry.first;
        string fname = url.substr(6); //remove https:
        fname.erase(remove(fname.begin(), fname.end(), '.'), fname.end());
        fname.erase(remove(fname.begin(), fname.end(), '/'), fname.end());
        string file_name = "text/" + fname + ".txt";
        string base_url = extract_base_url(url);
        string html;

        fs::create_directory("text");
        fs::create_directory("contents");

        download_html(url.c_str(), html);
        parse_html(html.c_str(), base_url, file_name);
    }

    return 0;
}
