#include "ConfigManager.h"
#include <iostream>
#include <sstream>
#include <algorithm>

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::loadConfig(const std::string& configPath) {
    try {
        std::ifstream file(configPath);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open config file: " << configPath << std::endl;
            return false;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        
        parseJSON(content);
        loaded_ = true;
        
        std::cout << "Configuration loaded successfully from: " << configPath << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading config: " << e.what() << std::endl;
        return false;
    }
}

// Helper to extract value from JSON (simplified parser)
std::string ConfigManager::extractValue(const std::string& content, const std::string& key) const {
    std::string searchKey = "\"" + key + "\"";
    size_t pos = content.find(searchKey);
    if (pos == std::string::npos) return "";
    
    pos = content.find(":", pos);
    if (pos == std::string::npos) return "";
    
    pos = content.find_first_not_of(" \t\n\r", pos + 1);
    if (pos == std::string::npos) return "";
    
    char delimiter = content[pos];
    if (delimiter == '"') {
        size_t start = pos + 1;
        size_t end = content.find('"', start);
        if (end != std::string::npos) {
            return content.substr(start, end - start);
        }
    } else if (delimiter == 't' || delimiter == 'f') {
        // Boolean
        size_t end = content.find_first_of(",\n}", pos);
        std::string value = content.substr(pos, end - pos);
        value.erase(std::remove_if(value.begin(), value.end(), ::isspace), value.end());
        return value;
    } else if (std::isdigit(delimiter) || delimiter == '-') {
        // Number
        size_t end = content.find_first_of(",\n}", pos);
        std::string value = content.substr(pos, end - pos);
        value.erase(std::remove_if(value.begin(), value.end(), ::isspace), value.end());
        return value;
    }
    
    return "";
}

void ConfigManager::parseJSON(const std::string& content) {
    // Simplified JSON parser - extracts key-value pairs
    // For production, use nlohmann/json or similar library
    
    // Application settings
    config_["app.name"] = extractValue(content, "name");
    config_["app.version"] = extractValue(content, "version");
    config_["app.debug_mode"] = extractValue(content, "debug_mode");
    
    // Video settings
    config_["video.source_type"] = extractValue(content, "source_type");
    config_["video.source_path"] = extractValue(content, "source_path");
    config_["video.camera_index"] = extractValue(content, "camera_index");
    config_["video.frame_width"] = extractValue(content, "frame_width");
    config_["video.frame_height"] = extractValue(content, "frame_height");
    config_["video.processing_interval"] = extractValue(content, "processing_interval");
    config_["video.reconnect_attempts"] = extractValue(content, "reconnect_attempts");
    config_["video.reconnect_delay_ms"] = extractValue(content, "reconnect_delay_ms");
    
    // OCR settings
    config_["ocr.language"] = extractValue(content, "language");
    config_["ocr.confidence_threshold"] = extractValue(content, "confidence_threshold");
    config_["ocr.tesseract_config"] = extractValue(content, "tesseract_config");
    config_["ocr.page_segmentation_mode"] = extractValue(content, "page_segmentation_mode");
    
    // Vital signs
    config_["vital_signs.default_spo2"] = extractValue(content, "default_spo2");
    config_["vital_signs.spo2_history_size"] = extractValue(content, "spo2_history_size");
    config_["vital_signs.hr_min"] = extractValue(content, "hr_min");
    config_["vital_signs.hr_max"] = extractValue(content, "hr_max");
    config_["vital_signs.spo2_min"] = extractValue(content, "spo2_min");
    config_["vital_signs.spo2_max"] = extractValue(content, "spo2_max");
    config_["vital_signs.abp_systolic_min"] = extractValue(content, "abp_systolic_min");
    config_["vital_signs.abp_systolic_max"] = extractValue(content, "abp_systolic_max");
    config_["vital_signs.abp_diastolic_min"] = extractValue(content, "abp_diastolic_min");
    config_["vital_signs.abp_diastolic_max"] = extractValue(content, "abp_diastolic_max");
    
    // ML Model
    config_["ml_model.enabled"] = extractValue(content, "enabled");
    config_["ml_model.input_width"] = extractValue(content, "input_width");
    config_["ml_model.input_height"] = extractValue(content, "input_height");
    config_["ml_model.confidence_threshold"] = extractValue(content, "confidence_threshold");
    
    // Database
    config_["database.enabled"] = extractValue(content, "enabled");
    config_["database.type"] = extractValue(content, "type");
    config_["database.host"] = extractValue(content, "host");
    config_["database.port"] = extractValue(content, "port");
    config_["database.database"] = extractValue(content, "database");
    config_["database.user"] = extractValue(content, "user");
    config_["database.password"] = extractValue(content, "password");
    config_["database.connection_pool_size"] = extractValue(content, "connection_pool_size");
    config_["database.connection_timeout"] = extractValue(content, "connection_timeout");
    config_["database.retry_attempts"] = extractValue(content, "retry_attempts");
    config_["database.retry_delay_ms"] = extractValue(content, "retry_delay_ms");
    
    // Output
    config_["output.csv_enabled"] = extractValue(content, "csv_enabled");
    config_["output.csv_file"] = extractValue(content, "csv_file");
    config_["output.console_output"] = extractValue(content, "console_output");
    
    // Logging
    config_["logging.level"] = extractValue(content, "level");
    config_["logging.console_enabled"] = extractValue(content, "console_enabled");
    config_["logging.file_enabled"] = extractValue(content, "file_enabled");
    config_["logging.file_path"] = extractValue(content, "file_path");
    config_["logging.max_file_size_mb"] = extractValue(content, "max_file_size_mb");
    config_["logging.max_files"] = extractValue(content, "max_files");
    config_["logging.pattern"] = extractValue(content, "pattern");
    
    // Monitoring
    config_["monitoring.health_check_interval_sec"] = extractValue(content, "health_check_interval_sec");
    config_["monitoring.metrics_enabled"] = extractValue(content, "metrics_enabled");
    config_["monitoring.alert_on_error"] = extractValue(content, "alert_on_error");
}

// Helper methods
std::string ConfigManager::getString(const std::string& key, const std::string& defaultValue) const {
    auto it = config_.find(key);
    return (it != config_.end() && !it->second.empty()) ? it->second : defaultValue;
}

int ConfigManager::getInt(const std::string& key, int defaultValue) const {
    auto it = config_.find(key);
    if (it != config_.end() && !it->second.empty()) {
        try {
            return std::stoi(it->second);
        } catch (...) {
            return defaultValue;
        }
    }
    return defaultValue;
}

bool ConfigManager::getBool(const std::string& key, bool defaultValue) const {
    auto it = config_.find(key);
    if (it != config_.end() && !it->second.empty()) {
        std::string value = it->second;
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        return value == "true" || value == "1";
    }
    return defaultValue;
}

float ConfigManager::getFloat(const std::string& key, float defaultValue) const {
    auto it = config_.find(key);
    if (it != config_.end() && !it->second.empty()) {
        try {
            return std::stof(it->second);
        } catch (...) {
            return defaultValue;
        }
    }
    return defaultValue;
}

// Application settings
std::string ConfigManager::getAppName() const { return getString("app.name", "VitalSignExtractor"); }
std::string ConfigManager::getAppVersion() const { return getString("app.version", "1.0.0"); }
bool ConfigManager::isDebugMode() const { return getBool("app.debug_mode", false); }

// Video settings
std::string ConfigManager::getVideoSourceType() const { return getString("video.source_type", "file"); }
std::string ConfigManager::getVideoSourcePath() const { return getString("video.source_path", ""); }
int ConfigManager::getCameraIndex() const { return getInt("video.camera_index", 0); }
int ConfigManager::getFrameWidth() const { return getInt("video.frame_width", 640); }
int ConfigManager::getFrameHeight() const { return getInt("video.frame_height", 480); }
int ConfigManager::getProcessingInterval() const { return getInt("video.processing_interval", 300); }
int ConfigManager::getReconnectAttempts() const { return getInt("video.reconnect_attempts", 5); }
int ConfigManager::getReconnectDelayMs() const { return getInt("video.reconnect_delay_ms", 2000); }

// OCR settings
std::string ConfigManager::getOCRLanguage() const { return getString("ocr.language", "eng"); }
int ConfigManager::getOCRConfidenceThreshold() const { return getInt("ocr.confidence_threshold", 50); }
std::string ConfigManager::getTesseractConfig() const { return getString("ocr.tesseract_config", ""); }
int ConfigManager::getPageSegmentationMode() const { return getInt("ocr.page_segmentation_mode", 3); }

// Vital signs settings
std::string ConfigManager::getDefaultSpO2() const { return getString("vital_signs.default_spo2", "81"); }
std::vector<std::string> ConfigManager::getVitalSignLabels() const {
    return {"HR", "SpO2", "ABP"};
}
int ConfigManager::getSpO2HistorySize() const { return getInt("vital_signs.spo2_history_size", 10); }

ConfigManager::ValidationRanges ConfigManager::getValidationRanges() const {
    ValidationRanges ranges;
    ranges.hr_min = getInt("vital_signs.hr_min", 30);
    ranges.hr_max = getInt("vital_signs.hr_max", 200);
    ranges.spo2_min = getInt("vital_signs.spo2_min", 70);
    ranges.spo2_max = getInt("vital_signs.spo2_max", 100);
    ranges.abp_systolic_min = getInt("vital_signs.abp_systolic_min", 70);
    ranges.abp_systolic_max = getInt("vital_signs.abp_systolic_max", 200);
    ranges.abp_diastolic_min = getInt("vital_signs.abp_diastolic_min", 40);
    ranges.abp_diastolic_max = getInt("vital_signs.abp_diastolic_max", 130);
    return ranges;
}

// ML Model settings
bool ConfigManager::isMLModelEnabled() const { return getBool("ml_model.enabled", true); }
int ConfigManager::getMLInputWidth() const { return getInt("ml_model.input_width", 96); }
int ConfigManager::getMLInputHeight() const { return getInt("ml_model.input_height", 96); }
float ConfigManager::getMLConfidenceThreshold() const { return getFloat("ml_model.confidence_threshold", 0.7f); }

// Database settings
bool ConfigManager::isDatabaseEnabled() const { return getBool("database.enabled", false); }
std::string ConfigManager::getDBType() const { return getString("database.type", "postgresql"); }
std::string ConfigManager::getDBHost() const { return getString("database.host", "localhost"); }
int ConfigManager::getDBPort() const { return getInt("database.port", 5432); }
std::string ConfigManager::getDBName() const { return getString("database.database", "vital_signs_db"); }
std::string ConfigManager::getDBUser() const { return getString("database.user", "vitalsign_user"); }
std::string ConfigManager::getDBPassword() const { return getString("database.password", ""); }
int ConfigManager::getDBConnectionPoolSize() const { return getInt("database.connection_pool_size", 5); }
int ConfigManager::getDBConnectionTimeout() const { return getInt("database.connection_timeout", 30); }
int ConfigManager::getDBRetryAttempts() const { return getInt("database.retry_attempts", 3); }
int ConfigManager::getDBRetryDelayMs() const { return getInt("database.retry_delay_ms", 1000); }

// Output settings
bool ConfigManager::isCSVEnabled() const { return getBool("output.csv_enabled", true); }
std::string ConfigManager::getCSVFile() const { return getString("output.csv_file", "live_vital_signs_output.csv"); }
bool ConfigManager::isConsoleOutputEnabled() const { return getBool("output.console_output", true); }

// Logging settings
std::string ConfigManager::getLogLevel() const { return getString("logging.level", "info"); }
bool ConfigManager::isConsoleLoggingEnabled() const { return getBool("logging.console_enabled", true); }
bool ConfigManager::isFileLoggingEnabled() const { return getBool("logging.file_enabled", true); }
std::string ConfigManager::getLogFilePath() const { return getString("logging.file_path", "logs/vitalsign.log"); }
int ConfigManager::getMaxLogFileSizeMB() const { return getInt("logging.max_file_size_mb", 10); }
int ConfigManager::getMaxLogFiles() const { return getInt("logging.max_files", 5); }
std::string ConfigManager::getLogPattern() const { 
    return getString("logging.pattern", "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v"); 
}

// Monitoring settings
int ConfigManager::getHealthCheckIntervalSec() const { return getInt("monitoring.health_check_interval_sec", 60); }
bool ConfigManager::isMetricsEnabled() const { return getBool("monitoring.metrics_enabled", true); }
bool ConfigManager::isAlertOnError() const { return getBool("monitoring.alert_on_error", true); }
