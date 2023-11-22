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
static size_t image_callback(void* contents, size_t size, size_t nmemb, FILE* file) {
    return fwrite(contents, size, nmemb, file);
}

std::string makeAbsoluteUrl(const string& baseUrl, const string& relativeUrl) {
    // Check if the relative URL is already an absolute URL
    if (relativeUrl.find("://") != std::string::npos) {
        return relativeUrl; // Already an absolute URL
    }

    // Parse the base URL to extract the scheme and authority
    std::istringstream baseStream(baseUrl);
    std::string scheme, authority;
    baseStream >> scheme;
    std::getline(baseStream, authority, '/'); // Extract authority (domain and port)

    // Remove trailing slashes from the base URL
    authority.erase(std::remove(authority.begin(), authority.end(), '/'), authority.end());

    // Combine the scheme, authority, and relative path to form the absolute URL
    std::ostringstream absoluteUrl;
    absoluteUrl << scheme << "://" << authority << '/' << relativeUrl;

    return absoluteUrl.str();
}

string gen_filename(string& url) {
    // Find the last '/' in the URL
    size_t lastSlashPos = url.rfind('/');
    
    // Check if a slash was found
    if (lastSlashPos != std::string::npos) {
        // Extract the substring after the last '/'
        return url.substr(lastSlashPos + 1);
    }
    assert(false); // this should work all the time 
    //(TODO: change this to exception throw later)
    return "";
}


// Function to download an image from a URL and save it to a file
void download_image(const char* image_url, const char* file_name) {
    CURL* curl = curl_easy_init();

    if (curl) {
        FILE* file = fopen(file_name, "wb");

        if (file) {
            curl_easy_setopt(curl, CURLOPT_URL, image_url);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, image_callback);
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

void extract_images(GumboNode* node, string& base_url) {
    if (node->type == GUMBO_NODE_ELEMENT) {
        GumboAttribute* src = nullptr;

        if (node->v.element.tag == GUMBO_TAG_IMG && 
            (src = gumbo_get_attribute(&node->v.element.attributes, "src"))) {
            string image_url(src->value);

            // Check if the relative URL is already an absolute URL
            if (image_url.find("://") == std::string::npos) {
                image_url = base_url + image_url; // a relative URL is found
            }
            cout << "Found image: " << image_url << endl;
            string filename = gen_filename(image_url);
            download_image(image_url.c_str(), filename.c_str());
        }

        // Recursive call to visit child nodes
        GumboVector* children = &node->v.element.children;
        for (size_t i = 0; i < children->length; i++) {
            extract_images((GumboNode*)(children->data[i]), base_url);
        }
    }
}


// Function to recursively extract text content
void extract_text(const GumboNode* node, ofstream& file) {
    if (node->type == GUMBO_NODE_TEXT) {
        cout << node->v.text.text << " ";
        file << node->v.text.text << " ";
    } else if (node->type == GUMBO_NODE_ELEMENT &&
               node->v.element.tag != GUMBO_TAG_SCRIPT &&
               node->v.element.tag != GUMBO_TAG_STYLE) {
        
        GumboVector children = node->v.element.children;
        for (size_t i = 0; i < children.length; i++) {
            extract_text((GumboNode*)children.data[i], file);
        }
    }
}

// Function to parse HTML and extract text content using Gumbo Parser
void parse_html_and_save_text(const string& html, const char* file_name) {
    ofstream file(file_name);
    GumboOutput* output = gumbo_parse(html.c_str());
    if (output) {
        extract_text(output->root, file);
        gumbo_destroy_output(&kGumboDefaultOptions, output);
    }
    file.close();
}

void parse_html(const char* html_content, string& base_url) {
    GumboOutput* output = gumbo_parse(html_content);
    extract_images(output->root, base_url);
    gumbo_destroy_output(&kGumboDefaultOptions, output);
}


void download_html(const char* url, std::string& downloaded_html) {
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
    // Set the URL to download
    const char* url = "https://www.w3schools.com/graphics/svg_inhtml.asp";
    const char* file_name = "downloaded_file.txt";
    string html;
    string base_url(url);

    base_url = extract_base_url(base_url);

    download_html(url, html);
    // parse_html_and_save_text(html, file_name);
    parse_html(html.c_str(), base_url);

    return 0;
}
