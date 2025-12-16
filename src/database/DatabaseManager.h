#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <string>
#include <memory>
#include <vector>
#include <mutex>
#include <libpq-fe.h>

// Structure to hold vital sign data
struct VitalSignData {
    std::string timestamp;
    std::string hr;
    std::string spo2;
    std::string abp;
    std::string ecg_classification;
    float ecg_confidence;
};

class DatabaseManager {
public:
    static DatabaseManager& getInstance();
    
    // Initialize database connection
    bool init(const std::string& host,
              int port,
              const std::string& dbname,
              const std::string& user,
              const std::string& password,
              int poolSize = 5,
              int timeout = 30);
    
    // Database operations
    bool connect();
    void disconnect();
    bool isConnected();
    
    // Create tables if they don't exist
    bool createTables();
    
    // Insert vital sign data
    bool insertVitalSign(const VitalSignData& data);
    
    // Query operations (for future use)
    std::vector<VitalSignData> getRecentVitalSigns(int limit = 100);
    
    // Health check
    bool healthCheck();
    
    // Reconnect with retry logic
    bool reconnect(int maxAttempts = 3, int delayMs = 1000);
    
private:
    DatabaseManager() = default;
    ~DatabaseManager();
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
    
    PGconn* conn_ = nullptr;
    std::mutex mutex_;
    
    std::string host_;
    int port_;
    std::string dbname_;
    std::string user_;
    std::string password_;
    int poolSize_;
    int timeout_;
    
    bool initialized_ = false;
    
    // Helper methods
    std::string buildConnectionString();
    bool executeQuery(const std::string& query);
    PGresult* executeQueryWithResult(const std::string& query);
    std::string escapeString(const std::string& input);
};

#endif // DATABASE_MANAGER_H
