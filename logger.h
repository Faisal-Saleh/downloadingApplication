#include <iostream>
#include <fstream>
#include <ctime>
#include <memory>

#ifndef _LOGGER_H_
#define _LOGGER_H_

using namespace std;

enum class LogType {
    INFO,
    ERROR
};

class Logger {
private:
    ofstream file; // File stream if logging to a file
    bool log_console;
public:
    Logger() {} // Default constructor for console output
    Logger(const std::string& file_name);

    ~Logger();

    void log(LogType type, const std::string& message);
};

#endif