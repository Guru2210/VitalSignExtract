#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <ctime>
#include <iomanip>
#include <sstream>

// Log levels
enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3,
    CRITICAL = 4
};

class Logger {
public:
    static Logger& getInstance();
    
    // Initialize logger
    void init(const std::string& logFilePath, 
              LogLevel level = LogLevel::INFO,
              bool consoleEnabled = true,
              bool fileEnabled = true,
              size_t maxFileSizeMB = 10,
              int maxFiles = 5);
    
    // Log methods
    void debug(const std::string& message);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);
    void critical(const std::string& message);
    
    // Set log level
    void setLogLevel(LogLevel level);
    
    // Flush logs
    void flush();
    
private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    void log(LogLevel level, const std::string& message);
    std::string formatMessage(LogLevel level, const std::string& message);
    std::string levelToString(LogLevel level);
    std::string getCurrentTimestamp();
    void rotateLogFile();
    void checkAndRotate();
    
    std::ofstream logFile_;
    std::mutex mutex_;
    std::string logFilePath_;
    LogLevel currentLevel_ = LogLevel::INFO;
    bool consoleEnabled_ = true;
    bool fileEnabled_ = true;
    size_t maxFileSizeBytes_ = 10 * 1024 * 1024; // 10MB default
    int maxFiles_ = 5;
    bool initialized_ = false;
};

// Convenience macros
#define LOG_DEBUG(msg) Logger::getInstance().debug(msg)
#define LOG_INFO(msg) Logger::getInstance().info(msg)
#define LOG_WARN(msg) Logger::getInstance().warn(msg)
#define LOG_ERROR(msg) Logger::getInstance().error(msg)
#define LOG_CRITICAL(msg) Logger::getInstance().critical(msg)

#endif // LOGGER_H
