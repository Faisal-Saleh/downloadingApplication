/**
 * @file logger.cpp
 * @author Faisal Abdelmonem
 * @brief  THis is a simple logger file. It only logs two types INFO and ERROR
 *         given the type and the message the logger will store it in the file
 *         that the main thread gave to the urls manager. In addition to the 
 *         log type of the message we also log the time for the convenience
 *         of the user.
 * @date 2023-11-27
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include "logger.h"

/**
 * @brief Construct a new Logger:: Logger object to log at the given filename
 *        or log at cout if no file name is given.
 * 
 * @param fileName The name that the output log file will have.
 */
Logger::Logger(const std::string& fileName) : file(fileName) {
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << fileName << std::endl;
        // Fallback to console output in case of file opening failure
        log_console = true;
    } else {
        log_console = false;
    }
}

/**
 * @brief Destroy the Logger:: Logger object which closes the file the moment
 *        The logger goes out of scope.
 * 
 */
Logger::~Logger() {
    if (file.is_open()) {
        file.close();
    }
}

/**
 * @brief Function that logs the type of the message and the message along with
 *        the time it was logged.
 * 
 * @param type Either INFO or ERROR
 * @param message The message that we need to log. Usually successful or failed.
 */
void Logger::log(LogType type, const std::string& message) {
    // Get current time
    std::time_t t = std::time(nullptr);
    std::tm* now = std::localtime(&t);

    // Format the log message with timestamp
    char timestamp[20];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", now);

    // Log to the output stream
    switch (type) {
        case LogType::INFO:
            if (log_console) {
                cout << "[" << timestamp << "] [INFO] " << message << endl;
            } else {
                file << "[" << timestamp << "] [INFO] " << message << endl;
            }
            break;
        case LogType::ERROR:
            if (log_console) {
                cout << "[" << timestamp << "] [ERROR] " << message << std::endl;
            } else {
                file << "[" << timestamp << "] [ERROR] " << message << std::endl;
            }
            break;
    }
}
