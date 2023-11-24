// main.cpp
//#include "urlsmanager.h"
//#include "downloader.h"
#include <gumbo.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <string>
#include <cstring>
#include <curl/curl.h>
#include <functional>
#include <assert.h>

using namespace std;

// Callback function to write received data to a string
size_t text_callback(void* contents, size_t size, size_t nmemb, std::string* output) {
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

string extract_base_url(string& url) {
    // Find the position of the first '/' after the "://" part
    size_t pos = url.find("://");
    if (pos != std::string::npos) {
        pos = url.find('/', pos + 3); // Skip the "://" part and find the next '/'
        if (pos != std::string::npos) {
            return url.substr(0, pos);
        }
    }

    return ""; // Return an empty string if no match is found
}

/**
 * @brief Recursively visits the Gumbo nodes and if it finds content
 *        that should be downloaded sets up the url and the filename
 *        to call the download_content function.
 * 
 * @param node The current GumboNode we are pointing at.
 * @param base_url the base url in case the content is stored with 
 *                 a relative url.
 */
void extract_content(GumboNode* node, string& base_url) {
    if (node->type == GUMBO_NODE_ELEMENT) {
        GumboAttribute* src = nullptr;

        // Checks if the node is an (image or a video or an audio) and has a src
        if ((node->v.element.tag == GUMBO_TAG_VIDEO  ||
            node->v.element.tag == GUMBO_TAG_IMG     ||
            node->v.element.tag == GUMBO_TAG_AUDIO)  &&
            (src = gumbo_get_attribute(&node->v.element.attributes, "src"))) {
                
            string url(src->value);

            // Check if the url is already an absolute one
            if (url.find("://") == string::npos) {
                cout << "found a relative url" << endl;
                url = base_url + url; // a relative url is found
            }

            // Check if the url is base64 encoded, if it is skip
            if (url.find("base64") == string::npos) {
                cout << "Found content: " << url << endl;
                string filename = url.substr(url.find_last_of('/') + 1);
                string path = "downloads/" + filename;
                download_content(url.c_str(), path.c_str());
            } //TODO: maybe decode it and save it in the future
        }
        
        // Recursive call to visit child nodes
        GumboVector* children = &node->v.element.children;
        for (size_t i = 0; i < children->length; i++) {
            extract_content((GumboNode*)(children->data[i]), base_url);
        }
    }
}

// Function to recursively extract text content
void extract_text(GumboNode* node, ofstream& file) {
    if (node->type == GUMBO_NODE_TEXT) {
        cout << node->v.text.text << " ";
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
void parse_html(const char* html_content, string& base_url, const char* file_name) {
    ofstream file(file_name);
    GumboOutput* output = gumbo_parse(html_content);
    extract_text(output->root, file);
    extract_content(output->root, base_url);
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

int main() {
    // Set the url to download
    const char* url = "https://sponixtech.com/spboard-technology/";
    const char* file_name = "downloaded_file.txt";
    string html;
    string base_url(url);

    base_url = extract_base_url(base_url);
    download_html(url, html);
    parse_html(html.c_str(), base_url, file_name);

    return 0;
}
