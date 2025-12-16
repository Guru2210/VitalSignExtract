#include "Logger.h"
#include <iostream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    if (logFile_.is_open()) {
        logFile_.close();
    }
}

void Logger::init(const std::string& logFilePath, 
                  LogLevel level,
                  bool consoleEnabled,
                  bool fileEnabled,
                  size_t maxFileSizeMB,
                  int maxFiles) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    logFilePath_ = logFilePath;
    currentLevel_ = level;
    consoleEnabled_ = consoleEnabled;
    fileEnabled_ = fileEnabled;
    maxFileSizeBytes_ = maxFileSizeMB * 1024 * 1024;
    maxFiles_ = maxFiles;
    
    if (fileEnabled_) {
        // Create directory if it doesn't exist
        fs::path logPath(logFilePath_);
        fs::path logDir = logPath.parent_path();
        if (!logDir.empty() && !fs::exists(logDir)) {
            fs::create_directories(logDir);
        }
        
        // Open log file in append mode
        logFile_.open(logFilePath_, std::ios::app);
        if (!logFile_.is_open()) {
            std::cerr << "Error: Could not open log file: " << logFilePath_ << std::endl;
            fileEnabled_ = false;
        }
    }
    
    initialized_ = true;
    info("Logger initialized");
}

void Logger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentLevel_ = level;
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warn(const std::string& message) {
    log(LogLevel::WARN, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

void Logger::critical(const std::string& message) {
    log(LogLevel::CRITICAL, message);
}

void Logger::log(LogLevel level, const std::string& message) {
    if (!initialized_) {
        // Fallback to console if not initialized
        std::cout << message << std::endl;
        return;
    }
    
    if (level < currentLevel_) {
        return; // Skip if below current log level
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string formattedMessage = formatMessage(level, message);
    
    // Console output
    if (consoleEnabled_) {
        if (level >= LogLevel::ERROR) {
            std::cerr << formattedMessage << std::endl;
        } else {
            std::cout << formattedMessage << std::endl;
        }
    }
    
    // File output
    if (fileEnabled_ && logFile_.is_open()) {
        logFile_ << formattedMessage << std::endl;
        logFile_.flush();
        
        // Check if rotation is needed
        checkAndRotate();
    }
}

std::string Logger::formatMessage(LogLevel level, const std::string& message) {
    std::ostringstream oss;
    oss << "[" << getCurrentTimestamp() << "] "
        << "[" << levelToString(level) << "] "
        << message;
    return oss.str();
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:    return "DEBUG";
        case LogLevel::INFO:     return "INFO ";
        case LogLevel::WARN:     return "WARN ";
        case LogLevel::ERROR:    return "ERROR";
        case LogLevel::CRITICAL: return "CRIT ";
        default:                 return "UNKNOWN";
    }
}

std::string Logger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

void Logger::checkAndRotate() {
    if (!logFile_.is_open()) return;
    
    // Get current file size
    logFile_.seekp(0, std::ios::end);
    size_t fileSize = logFile_.tellp();
    
    if (fileSize >= maxFileSizeBytes_) {
        rotateLogFile();
    }
}

void Logger::rotateLogFile() {
    if (logFile_.is_open()) {
        logFile_.close();
    }
    
    // Rotate existing log files
    for (int i = maxFiles_ - 1; i > 0; --i) {
        std::string oldFile = logFilePath_ + "." + std::to_string(i);
        std::string newFile = logFilePath_ + "." + std::to_string(i + 1);
        
        if (fs::exists(oldFile)) {
            if (i == maxFiles_ - 1) {
                fs::remove(oldFile); // Remove oldest file
            } else {
                fs::rename(oldFile, newFile);
            }
        }
    }
    
    // Rename current log file
    if (fs::exists(logFilePath_)) {
        std::string rotatedFile = logFilePath_ + ".1";
        fs::rename(logFilePath_, rotatedFile);
    }
    
    // Open new log file
    logFile_.open(logFilePath_, std::ios::app);
    if (!logFile_.is_open()) {
        std::cerr << "Error: Could not reopen log file after rotation" << std::endl;
        fileEnabled_ = false;
    }
}

void Logger::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (logFile_.is_open()) {
        logFile_.flush();
    }
}
