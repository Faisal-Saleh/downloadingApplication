// main.cpp
//#include "urlsmanager.h"
//#include "downloader.h"
#include <gumbo.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <curl/curl.h>
#include <functional>

// Callback function to write received data to a string
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t total_size = size * nmemb;
    output->append((char*)contents, total_size);
    return total_size;
}

// Function to recursively extract text content
void ExtractText(const GumboNode* node, FILE* file) {
    if (node->type == GUMBO_NODE_TEXT) {
        std::cout << node->v.text.text << " ";
        fwrite(node->v.text.text, 1, strlen((node->v.text.text)), file);
        fputc(' ', file);
    } else if (node->type == GUMBO_NODE_ELEMENT &&
               node->v.element.tag != GUMBO_TAG_SCRIPT &&
               node->v.element.tag != GUMBO_TAG_STYLE) {
        for (unsigned int i = 0; i < node->v.element.children.length; ++i) {
            ExtractText((GumboNode*)node->v.element.children.data[i], file);
        }
    }
}

// Function to parse HTML and extract text content using Gumbo Parser
void ParseHTMLAndStoreText(const std::string& html, FILE* file) {
   GumboOutput* output = gumbo_parse(html.c_str());
    if (output) {
        ExtractText(output->root, file);
        gumbo_destroy_output(&kGumboDefaultOptions, output);
    }
}

int main() {
    // Initialize libcurl
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Error initializing libcurl" << std::endl;
        return 1;
    }

    // Set the URL to download
    const char* url = "https://www.w3schools.com/cpp/default.asp";
    curl_easy_setopt(curl, CURLOPT_URL, url);

    // Set the callback function to write data to a file
    FILE* file = fopen("downloaded_file.txt", "wb");
    if (!file) {
        std::cerr << "Error opening file for writing" << std::endl;
        curl_easy_cleanup(curl);
        return 1;
    }

    // Set the callback function to write data to a string
    std::string html;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &html);

    // Perform the HTTP GET request
    CURLcode res = curl_easy_perform(curl);

    // Check for errors
    if (res != CURLE_OK) {
        std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
    } else {
        // Parse HTML and extract text content
        ParseHTMLAndStoreText(html, file);
    }

    // Clean up
    curl_easy_cleanup(curl);
    fclose(file);

    return 0;
}
