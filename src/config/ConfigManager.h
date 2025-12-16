#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <fstream>
#include <stdexcept>

// Simple JSON parser (using nlohmann/json would be better, but keeping dependencies minimal)
// For production, consider using nlohmann/json library

class ConfigManager {
public:
    static ConfigManager& getInstance();
    
    // Initialize configuration from file
    bool loadConfig(const std::string& configPath);
    
    // Application settings
    std::string getAppName() const;
    std::string getAppVersion() const;
    bool isDebugMode() const;
    
    // Video settings
    std::string getVideoSourceType() const;
    std::string getVideoSourcePath() const;
    int getCameraIndex() const;
    int getFrameWidth() const;
    int getFrameHeight() const;
    int getProcessingInterval() const;
    int getReconnectAttempts() const;
    int getReconnectDelayMs() const;
    
    // OCR settings
    std::string getOCRLanguage() const;
    int getOCRConfidenceThreshold() const;
    std::string getTesseractConfig() const;
    int getPageSegmentationMode() const;
    
    // Vital signs settings
    std::string getDefaultSpO2() const;
    std::vector<std::string> getVitalSignLabels() const;
    int getSpO2HistorySize() const;
    
    // Validation ranges
    struct ValidationRanges {
        int hr_min, hr_max;
        int spo2_min, spo2_max;
        int abp_systolic_min, abp_systolic_max;
        int abp_diastolic_min, abp_diastolic_max;
    };
    ValidationRanges getValidationRanges() const;
    
    // ML Model settings
    bool isMLModelEnabled() const;
    int getMLInputWidth() const;
    int getMLInputHeight() const;
    float getMLConfidenceThreshold() const;
    
    // Database settings
    bool isDatabaseEnabled() const;
    std::string getDBType() const;
    std::string getDBHost() const;
    int getDBPort() const;
    std::string getDBName() const;
    std::string getDBUser() const;
    std::string getDBPassword() const;
    int getDBConnectionPoolSize() const;
    int getDBConnectionTimeout() const;
    int getDBRetryAttempts() const;
    int getDBRetryDelayMs() const;
    
    // Output settings
    bool isCSVEnabled() const;
    std::string getCSVFile() const;
    bool isConsoleOutputEnabled() const;
    
    // Logging settings
    std::string getLogLevel() const;
    bool isConsoleLoggingEnabled() const;
    bool isFileLoggingEnabled() const;
    std::string getLogFilePath() const;
    int getMaxLogFileSizeMB() const;
    int getMaxLogFiles() const;
    std::string getLogPattern() const;
    
    // Monitoring settings
    int getHealthCheckIntervalSec() const;
    bool isMetricsEnabled() const;
    bool isAlertOnError() const;
    
private:
    ConfigManager() = default;
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
    // Simple config storage
    std::map<std::string, std::string> config_;
    bool loaded_ = false;
    
    // Helper methods
    std::string getString(const std::string& key, const std::string& defaultValue = "") const;
    int getInt(const std::string& key, int defaultValue = 0) const;
    bool getBool(const std::string& key, bool defaultValue = false) const;
    float getFloat(const std::string& key, float defaultValue = 0.0f) const;
    
    // Parse JSON manually (simplified)
    void parseJSON(const std::string& content);
    std::string extractValue(const std::string& content, const std::string& key) const;
};

#endif // CONFIG_MANAGER_H
