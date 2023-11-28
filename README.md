# Downloading Application
    Implemented a web scraper in C++ that given a set of urls and their depths in a json
    file it will parse those urls and download their content and all the contents of the
    urls that are reachable from these urls and repeat the process depth times. In other 
    words this application performs something like a bfs in the web visiting links and
    downloading their contents. The application uses multi-threading where it has several
    downloader threads and one urls manager thread that gives the downloader threads
    the urls they need to download.

## Setup
    The application uses libcurl to perform the get requests and get the html given a url.
    It also uses gumbo-parser to parse the pure html and be able to extract all the 
    content we wish to download from it. We also use json.hpp which is a header library
    to be able to parse the json and parse all the urls we need to work with.

    json.hpp is a header only library that needs to be included, however libcurl and 
    gumbo-parser are libraries that we need to download on the machine. For this project
    I used a wsl2 environment and the instructions to download the library where as follows:
### Libcurl
    The libcurl library does not have any dependencies and to install it 
    you need to go to the terminal and run these lines:
    sudo apt-get update
    sudo apt-get install libcurl4-openssl-dev

### Gumbo-parser
    We use the package manager to install the necessary dependencies:
    sudo apt update
    sudo apt install build-essential
    sudo apt install automake libtool cmake

    clone the gumbo repo:
    git clone https://github.com/google/gumbo-parser.git

    build and install gumbo-parser:
    cd gumbo-parser
    ./autogen.sh
    ./configure
    make
    sudo make install

    With that you should be able to include all the necessary libraries and run the code.

## Compiling
    When compiling the code we need link gumbo and libcurl, I used g++17 to compile the
    code and in order to compile and run we need to perform these commands:

    g++ -o app main.cpp urlsmanager.cpp downloader.cpp logger.cpp -lcurl -lgumbo
    ./app <json_file> <logger_file>

## Documentation
    The code is explained in better detail and with design choices justified in the
    implementation files of the code (*.cpp). 