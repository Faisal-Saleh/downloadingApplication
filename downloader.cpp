/**
 * @file downloader.cpp
 * @author Faisal Abdelmonem
 * @brief  This is the implementation of the downloader threads. The url manager
 *         creates as many instances as it wants of this object and each instance
 *         is responsible of downloading the content (text, image, audio, video)
 *         that exists at the url. The downloading process involves using the 
 *         libcurl library and performing a get request to acquire the pure html
 *         at that url. We then feed that html to a gumbo-parser which parses it
 *         and internally creates a tree with labels that we can then use to
 *         extract all the content we need and download it as well. I ended up
 *         ignoring the labels that the gumbo library generates because some 
 *         content is not properly labeled. The more effective solution was 
 *         to get the pure html of the content I want to download and then 
 *         read the header of that. If it an image, audio, or video we would
 *         try to download it, and if it is text/html we add it as url in the 
 *         url_list of the urls manager so that we fetch it and download later.
 *         Extracting the text process was done separately because that was 
 *         properly labeled in the gumbo library and with that the download
 *         process would be complete and we ask the urlsmanager for another url
 *         to download.
 * @date 2023-11-25
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include "downloader.h"

/**
 * @brief Construct a new downloader::downloader object, here we
 *        set the downloading_url variable to true indicating that the
 *        thread is currently downloading. We also create the downloader
 *        thread and detach it.
 * 
 * @param urlmanager 
 */
downloader::downloader(urlsmanager* urlmanager): url_manager(urlmanager) {
    downloading_url = true;
    downloader_thread = thread(&downloader::start, this);
    downloader_thread.detach();
}

/**
 * @brief downloading_url getter that tells us if the thread is downloading a url
 * 
 * @return true if there is a url being downloaded
 * @return false otherwise
 */
bool downloader::is_downloading() {
    return downloading_url;
}

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
 * @brief Function that manually extracts the base url (header) from the
 *        url given in the json file. We need this function because
 *        sometimes in the html there are relative urls so we use the
 *        base url in an attempt to construct the absolute url and download
 *        the content there.
 * 
 * @param inp_url url that we are currently downloading the text from.
 * @return string the header part of the url.
 */
string downloader::extract_base_url(string& inp_url) {
    // Find the position of the first '/' after the "://" part
    size_t pos = inp_url.find("://");
    if (pos != string::npos) {
        pos = inp_url.find('/', pos + 3); // Skip the "://" part and find the next '/'
        if (pos != string::npos) {
            return inp_url.substr(0, pos);
        }
    }

    return ""; // Return an empty string if no match is found
}

/**
 * @brief This function is called at the beginning to download the pure html
 *        at the given url and store it inside the given string.
 * 
 * @param url Html link that we need to retreive the pure html from.
 * @param downloaded_html String that we save the html in.
 */
void downloader::download_html(string& downloaded_html) {
    
    lock_guard<mutex> lock(url_manager->curl_mutex);
    
    cout << "acquired the lock and will download the html now\n";

    CURL* curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, main_url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, text_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &downloaded_html);

        // Perform the request
        CURLcode res = curl_easy_perform(curl);

        // Check for errors
        if (res != CURLE_OK) {
            // cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << " \n";
            string message = "URL Not Found: " + main_url;
            url_manager->log(LogType::ERROR, message);
        } else {
            string message = "Successful URL: " + string(main_url);
            url_manager->log(LogType::INFO, message);
        }

        // Clean up
        curl_easy_cleanup(curl);
    }
}

/**
 * @brief  Function to parse HTML and extract the text, images, audios
 *         and videos using the gumbo parser
 * 
 * @param html_content pure html string
 * @param file_name    the file name that we store the text at.
 */
void downloader::parse_html(const char* html_content, string& file_name) {
    ofstream file(file_name);
    GumboOutput* output = gumbo_parse(html_content);
    extract_text(output->root, file);
    extract_urls(output->root);
    gumbo_destroy_output(&kGumboDefaultOptions, output);
    file.close();
}

/**
 * @brief Function to recursively extract the text content by visiting
 *        all the gumbo node and saving anything that has a text tag
 *        to the file.
 * 
 * @param node The Gumbo node we are currently pointing to.
 * @param file The file that we need to save the text in.
 */
void downloader::extract_text(GumboNode* node, ofstream& file) {
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
 * @brief Given a url this function will return the type of the content
 *        this url represents. This could be an image, audio, video, html
 *        css and many more. we are only interested in downloadable content
 *        (image, audio, or video) or something else because that helps us
 *        in determining whether we need to download the content or add the
 *        url to the urls manager thread.
 * 
 * @param url Url that we need to get the content of.
 * @return string The content of the url.
 */
string downloader::get_url_content_type(const char* url) {
    CURL* curl;
    CURLcode res;

    lock_guard<mutex> lock(url_manager->curl_mutex);
    
    curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);

        // Set the callback function to handle the response
        string response;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, text_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        // Perform the get request
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            string message = "URL Not Found: " + string(url);
            // url_manager->log(LogType::ERROR, message);
            // Log if needed.
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
                string message = "Could Not get URL Header: " + string(url);
                url_manager->log(LogType::ERROR, message);
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
 * @brief Function to download the content at the given url and
 *        save it in a file that has the name file_name.
 *        The content that this function might store could be 
 *        an image or an audio or a video
 * 
 * @param url  The http url that we need to perform a get request at.
 * @param file_name The name of the file we need to create and save the content in
 */
void downloader::download_content(const char* url, const char* file_name) {

    lock_guard<mutex> lock(url_manager->curl_mutex);

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
                // fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
                string message = "URL Not Found: " + string(url);
                url_manager->log(LogType::ERROR, message);
            } else {
                string message = "Successful URL: " + string(url);
                url_manager->log(LogType::INFO, message);
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
void downloader::extract_urls(GumboNode* node) {
    if (node->type == GUMBO_NODE_ELEMENT) {
        GumboAttribute *attr = nullptr;
        attr = gumbo_get_attribute(&node->v.element.attributes, "href");
        if (!attr) 
            attr = gumbo_get_attribute(&node->v.element.attributes, "src");

        if (attr) {
            string url(attr->value);
            // Check if the url is already an absolute one
            if (url.find("://") == string::npos) {
                // cout << "found a relative url" << endl;
                url = base_url + url; // a relative url is found
            }

            string content_type;
            try {
                content_type = get_url_content_type(url.c_str());
            } catch(const std::exception& e) {
                // std::cerr << e.what() << '\n';
                string message = "URL Not Found: " + url;
                // url_manager->log(LogType::ERROR, message);
            }
            

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
                url_manager->add_url(url, depth - 1);
            }
            
        }

        GumboVector* children = &node->v.element.children;
        for (unsigned int i = 0; i < children->length; ++i) {
            extract_urls(static_cast<GumboNode*>(children->data[i]));
        }
    }
}

/**
 * @brief This function acts as the main function to the downloader threads.
 *        Here we get the url we need to download and save its html in a string
 *        and then parse that html and store the necessary content. This function
 *        needs to continue running because when it exits the respective
 *        downloader thread will terminate.
 * 
 */
void downloader::start() {
    auto pair = url_manager->get_url();
    main_url = string(pair.first);
    depth = pair.second;

    while (depth != -1) {
        base_url = extract_base_url(main_url);
        string html;

        // cout << "Main url is: " << main_url << endl;
        string fname = main_url.substr(6); //remove https:
        fname.erase(remove(fname.begin(), fname.end(), '.'), fname.end());
        fname.erase(remove(fname.begin(), fname.end(), '/'), fname.end());
        string file_name = "text/" + fname + ".txt";


        // cout << "going to download the html\n";
        download_html(html);
        // cout << html << endl;
        // cout << "going to parse the html\n";
        parse_html(html.c_str(), file_name);
        // cout << "done with the url\n";

        auto pair = url_manager->get_url();
        main_url = string(pair.first);
        depth = pair.second;
    }
    downloading_url = false;
}
