#include "DatabaseManager.h"
#include "../utils/Logger.h"
#include <sstream>
#include <thread>
#include <chrono>

DatabaseManager& DatabaseManager::getInstance() {
    static DatabaseManager instance;
    return instance;
}

DatabaseManager::~DatabaseManager() {
    disconnect();
}

bool DatabaseManager::init(const std::string& host,
                           int port,
                           const std::string& dbname,
                           const std::string& user,
                           const std::string& password,
                           int poolSize,
                           int timeout) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    host_ = host;
    port_ = port;
    dbname_ = dbname;
    user_ = user;
    password_ = password;
    poolSize_ = poolSize;
    timeout_ = timeout;
    
    initialized_ = true;
    LOG_INFO("DatabaseManager initialized with host: " + host + ", database: " + dbname);
    
    return connect();
}

std::string DatabaseManager::buildConnectionString() {
    std::ostringstream oss;
    oss << "host=" << host_
        << " port=" << port_
        << " dbname=" << dbname_
        << " user=" << user_
        << " password=" << password_
        << " connect_timeout=" << timeout_;
    return oss.str();
}

bool DatabaseManager::connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (conn_ != nullptr && PQstatus(conn_) == CONNECTION_OK) {
        LOG_DEBUG("Database already connected");
        return true;
    }
    
    std::string connStr = buildConnectionString();
    conn_ = PQconnectdb(connStr.c_str());
    
    if (PQstatus(conn_) != CONNECTION_OK) {
        std::string error = PQerrorMessage(conn_);
        LOG_ERROR("Database connection failed: " + error);
        PQfinish(conn_);
        conn_ = nullptr;
        return false;
    }
    
    LOG_INFO("Database connected successfully");
    return true;
}

void DatabaseManager::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (conn_ != nullptr) {
        PQfinish(conn_);
        conn_ = nullptr;
        LOG_INFO("Database disconnected");
    }
}

bool DatabaseManager::isConnected() {
    std::lock_guard<std::mutex> lock(mutex_);
    return conn_ != nullptr && PQstatus(conn_) == CONNECTION_OK;
}

bool DatabaseManager::createTables() {
    std::string createTableQuery = R"(
        CREATE TABLE IF NOT EXISTS vital_signs (
            id SERIAL PRIMARY KEY,
            timestamp TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
            hr VARCHAR(10),
            spo2 VARCHAR(10),
            abp VARCHAR(20),
            ecg_classification VARCHAR(50),
            ecg_confidence REAL,
            created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
        );
        
        CREATE INDEX IF NOT EXISTS idx_vital_signs_timestamp ON vital_signs(timestamp);
    )";
    
    if (executeQuery(createTableQuery)) {
        LOG_INFO("Database tables created/verified successfully");
        return true;
    }
    
    LOG_ERROR("Failed to create database tables");
    return false;
}

bool DatabaseManager::executeQuery(const std::string& query) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (conn_ == nullptr || PQstatus(conn_) != CONNECTION_OK) {
        LOG_ERROR("Cannot execute query: Database not connected");
        return false;
    }
    
    PGresult* res = PQexec(conn_, query.c_str());
    ExecStatusType status = PQresultStatus(res);
    
    bool success = (status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK);
    
    if (!success) {
        std::string error = PQerrorMessage(conn_);
        LOG_ERROR("Query execution failed: " + error);
    }
    
    PQclear(res);
    return success;
}

PGresult* DatabaseManager::executeQueryWithResult(const std::string& query) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (conn_ == nullptr || PQstatus(conn_) != CONNECTION_OK) {
        LOG_ERROR("Cannot execute query: Database not connected");
        return nullptr;
    }
    
    PGresult* res = PQexec(conn_, query.c_str());
    ExecStatusType status = PQresultStatus(res);
    
    if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
        std::string error = PQerrorMessage(conn_);
        LOG_ERROR("Query execution failed: " + error);
        PQclear(res);
        return nullptr;
    }
    
    return res;
}

std::string DatabaseManager::escapeString(const std::string& input) {
    if (conn_ == nullptr) return input;
    
    char* escaped = PQescapeLiteral(conn_, input.c_str(), input.length());
    if (escaped == nullptr) {
        LOG_WARN("Failed to escape string: " + input);
        return input;
    }
    
    std::string result(escaped);
    PQfreemem(escaped);
    return result;
}

bool DatabaseManager::insertVitalSign(const VitalSignData& data) {
    if (!isConnected()) {
        LOG_ERROR("Cannot insert data: Database not connected");
        return false;
    }
    
    std::ostringstream query;
    query << "INSERT INTO vital_signs (timestamp, hr, spo2, abp, ecg_classification, ecg_confidence) VALUES ("
          << "'" << data.timestamp << "', "
          << "'" << data.hr << "', "
          << "'" << data.spo2 << "', "
          << "'" << data.abp << "', "
          << "'" << data.ecg_classification << "', "
          << data.ecg_confidence
          << ");";
    
    if (executeQuery(query.str())) {
        LOG_DEBUG("Vital sign data inserted successfully");
        return true;
    }
    
    LOG_ERROR("Failed to insert vital sign data");
    return false;
}

std::vector<VitalSignData> DatabaseManager::getRecentVitalSigns(int limit) {
    std::vector<VitalSignData> results;
    
    std::ostringstream query;
    query << "SELECT timestamp, hr, spo2, abp, ecg_classification, ecg_confidence "
          << "FROM vital_signs ORDER BY timestamp DESC LIMIT " << limit << ";";
    
    PGresult* res = executeQueryWithResult(query.str());
    if (res == nullptr) {
        return results;
    }
    
    int rows = PQntuples(res);
    for (int i = 0; i < rows; i++) {
        VitalSignData data;
        data.timestamp = PQgetvalue(res, i, 0);
        data.hr = PQgetvalue(res, i, 1);
        data.spo2 = PQgetvalue(res, i, 2);
        data.abp = PQgetvalue(res, i, 3);
        data.ecg_classification = PQgetvalue(res, i, 4);
        data.ecg_confidence = std::stof(PQgetvalue(res, i, 5));
        results.push_back(data);
    }
    
    PQclear(res);
    LOG_DEBUG("Retrieved " + std::to_string(rows) + " vital sign records");
    
    return results;
}

bool DatabaseManager::healthCheck() {
    if (!isConnected()) {
        return false;
    }
    
    PGresult* res = executeQueryWithResult("SELECT 1;");
    if (res != nullptr) {
        PQclear(res);
        return true;
    }
    
    return false;
}

bool DatabaseManager::reconnect(int maxAttempts, int delayMs) {
    LOG_INFO("Attempting to reconnect to database...");
    
    for (int attempt = 1; attempt <= maxAttempts; attempt++) {
        LOG_INFO("Reconnection attempt " + std::to_string(attempt) + "/" + std::to_string(maxAttempts));
        
        disconnect();
        
        if (connect()) {
            LOG_INFO("Database reconnected successfully");
            return true;
        }
        
        if (attempt < maxAttempts) {
            LOG_WARN("Reconnection failed, waiting " + std::to_string(delayMs) + "ms before retry...");
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        }
    }
    
    LOG_ERROR("Failed to reconnect to database after " + std::to_string(maxAttempts) + " attempts");
    return false;
}
