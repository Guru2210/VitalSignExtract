
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <tesseract/baseapi.h>
#include <fstream>
#include <iostream>
#include <regex>
#include <deque>
#include <chrono>
#include <iomanip>
#include <map>
#include <string>
#include <cmath>
#include <csignal>
#include <atomic>
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
#include "unistd.h"
#include "src/config/ConfigManager.h"
#include "src/utils/Logger.h"
#include "src/database/DatabaseManager.h"

using namespace cv;
using namespace cv::dnn;
using namespace std;

// Global shutdown flag for graceful termination
static atomic<bool> g_shutdown(false);

// Signal handler for graceful shutdown
void signalHandler(int signum) {
    LOG_INFO("Interrupt signal (" + to_string(signum) + ") received. Shutting down gracefully...");
    g_shutdown = true;
}

// Initialize Tesseract OCR
static tesseract::TessBaseAPI ocr;

// Define key labels and patterns
static vector<string> labels;
static regex spo2_pattern(R"(\bsp[o0]2\b)", regex_constants::icase);
static regex bp_pattern(R"(^\d{2,3}/\d{2,3}$)");

deque<string> spo2_history;
string last_spo2_value;

// Use the model's input dimensions from model metadata
static float features[EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT];

// Struct to store detected text and position
struct DetectedText {
    string word;
    int x, y, w, h;
};

// Function to calculate Euclidean distance
double calculateDistance(int x1, int y1, int x2, int y2) {
    return sqrt(pow(x1 - x2, 2) + pow(y1 - y2, 2));
}

// Function to find the closest number to a label
string findClosestNumber(const DetectedText& labelData, const vector<DetectedText>& detectedNumbers) {
    if (detectedNumbers.empty()) return "0";
    
    string closestNum = "0";
    double minDistance = numeric_limits<double>::max();

    for (const auto& num : detectedNumbers) {
        double distance = calculateDistance(labelData.x, labelData.y, num.x, num.y);
        if (distance < minDistance) {
            minDistance = distance;
            closestNum = num.word;
        }
    }
    return closestNum;
}

// Function to process the frame and extract values
map<string, string> processFrame(const Mat& frame) {
    map<string, string> extractedValues;
    vector<DetectedText> detectedNumbers;
    map<string, DetectedText> detectedLabels;

    // Initialize detected labels
    for (const auto& label : labels) {
        detectedLabels[label] = {"", -1, -1, -1, -1};
    }

    // Set Tesseract OCR image
    ocr.SetImage(frame.data, frame.cols, frame.rows, 3, frame.step);
    ocr.Recognize(0);

    // Get detected text with position data
    tesseract::ResultIterator* ri = ocr.GetIterator();
    if (ri != nullptr) {
        do {
            const char* text = ri->GetUTF8Text(tesseract::RIL_WORD);
            float conf = ri->Confidence(tesseract::RIL_WORD);
            int x, y, w, h;

            int confidenceThreshold = ConfigManager::getInstance().getOCRConfidenceThreshold();
            if (text && conf > confidenceThreshold) {
                ri->BoundingBox(tesseract::RIL_WORD, &x, &y, &w, &h);
                DetectedText detected{text, x, y, w, h};

                // Check for labels
                if (regex_search(detected.word, spo2_pattern)) {
                    detectedLabels["SpO2"] = detected;
                } else if (find(labels.begin(), labels.end(), detected.word) != labels.end()) {
                    detectedLabels[detected.word] = detected;
                } else if (regex_match(detected.word, bp_pattern) || detected.word.find("/") != string::npos) {
                    detectedNumbers.push_back(detected);
                }
            }
            delete[] text;
        } while (ri->Next(tesseract::RIL_WORD));
    }

    // Extract values for each label
    for (const auto& label : labels) {
        extractedValues[label] = findClosestNumber(detectedLabels[label], detectedNumbers);
    }

    // ABP format validation
    if (!regex_match(extractedValues["ABP"], bp_pattern)) {
        extractedValues["HR"] = "0";
        extractedValues["SpO2"] = "0";
        extractedValues["ABP"] = "0";
    } else {
        // Use last known SpO₂ value if missing
        if (extractedValues["SpO2"] == "0" || extractedValues["SpO2"].empty()) {
            extractedValues["SpO2"] = last_spo2_value;
        } else {
            last_spo2_value = extractedValues["SpO2"];
        }
    }

    return extractedValues;
}

// Function to resize and crop frame
void resize_and_crop(cv::Mat *in_frame, cv::Mat *out_frame) {
    float factor_w = static_cast<float>(EI_CLASSIFIER_INPUT_WIDTH) / static_cast<float>(in_frame->cols);
    float factor_h = static_cast<float>(EI_CLASSIFIER_INPUT_HEIGHT) / static_cast<float>(in_frame->rows);
    float largest_factor = factor_w > factor_h ? factor_w : factor_h;

    cv::Size resize_size(static_cast<int>(largest_factor * in_frame->cols),
                         static_cast<int>(largest_factor * in_frame->rows));
    cv::Mat resized;
    cv::resize(*in_frame, resized, resize_size);

    int crop_x = resize_size.width > resize_size.height ? (resize_size.width - resize_size.height) / 2 : 0;
    int crop_y = resize_size.height > resize_size.width ? (resize_size.height - resize_size.width) / 2 : 0;
    cv::Rect crop_region(crop_x, crop_y, EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT);
    *out_frame = resized(crop_region);
}

// Initialize camera with retry logic
VideoCapture initializeCamera(int& retryCount) {
    ConfigManager& config = ConfigManager::getInstance();
    VideoCapture cap;
    
    string sourceType = config.getVideoSourceType();
    int maxAttempts = config.getReconnectAttempts();
    int delayMs = config.getReconnectDelayMs();
    
    for (int attempt = 1; attempt <= maxAttempts; attempt++) {
        if (sourceType == "camera") {
            int cameraIndex = config.getCameraIndex();
            LOG_INFO("Attempting to open camera " + to_string(cameraIndex) + " (attempt " + to_string(attempt) + "/" + to_string(maxAttempts) + ")");
            cap.open(cameraIndex);
        } else {
            string videoPath = config.getVideoSourcePath();
            LOG_INFO("Attempting to open video file: " + videoPath + " (attempt " + to_string(attempt) + "/" + to_string(maxAttempts) + ")");
            cap.open(videoPath);
        }
        
        if (cap.isOpened()) {
            LOG_INFO("Video source opened successfully");
            retryCount = 0;
            return cap;
        }
        
        LOG_WARN("Failed to open video source, attempt " + to_string(attempt) + "/" + to_string(maxAttempts));
        
        if (attempt < maxAttempts) {
            this_thread::sleep_for(chrono::milliseconds(delayMs));
        }
    }
    
    LOG_ERROR("Failed to open video source after " + to_string(maxAttempts) + " attempts");
    return cap;
}

int main(int argc, char** argv) {
    // Register signal handler for graceful shutdown
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Load configuration
    ConfigManager& config = ConfigManager::getInstance();
    string configPath = (argc > 1) ? argv[1] : "config/config.json";
    
    if (!config.loadConfig(configPath)) {
        cerr << "Error: Failed to load configuration from: " << configPath << endl;
        cerr << "Using default configuration values" << endl;
    }
    
    // Initialize logger
    Logger& logger = Logger::getInstance();
    LogLevel logLevel = LogLevel::INFO;
    string logLevelStr = config.getLogLevel();
    if (logLevelStr == "debug") logLevel = LogLevel::DEBUG;
    else if (logLevelStr == "warn") logLevel = LogLevel::WARN;
    else if (logLevelStr == "error") logLevel = LogLevel::ERROR;
    
    logger.init(
        config.getLogFilePath(),
        logLevel,
        config.isConsoleLoggingEnabled(),
        config.isFileLoggingEnabled(),
        config.getMaxLogFileSizeMB(),
        config.getMaxLogFiles()
    );
    
    LOG_INFO("=== Vital Sign Extraction System Starting ===");
    LOG_INFO("Application: " + config.getAppName() + " v" + config.getAppVersion());
    
    // Initialize vital sign parameters from config
    labels = config.getVitalSignLabels();
    last_spo2_value = config.getDefaultSpO2();
    
    // Initialize Tesseract OCR
    if (ocr.Init(NULL, config.getOCRLanguage().c_str())) {
        LOG_CRITICAL("Could not initialize Tesseract OCR");
        return -1;
    }
    LOG_INFO("Tesseract OCR initialized successfully");
    
    // Initialize database if enabled
    DatabaseManager& db = DatabaseManager::getInstance();
    bool dbEnabled = config.isDatabaseEnabled();
    
    if (dbEnabled) {
        LOG_INFO("Initializing database connection...");
        if (db.init(
            config.getDBHost(),
            config.getDBPort(),
            config.getDBName(),
            config.getDBUser(),
            config.getDBPassword(),
            config.getDBConnectionPoolSize(),
            config.getDBConnectionTimeout()
        )) {
            LOG_INFO("Database connected successfully");
            if (!db.createTables()) {
                LOG_ERROR("Failed to create database tables");
            }
        } else {
            LOG_ERROR("Database initialization failed, continuing without database");
            dbEnabled = false;
        }
    }
    
    // Initialize video capture
    int cameraRetryCount = 0;
    VideoCapture cap = initializeCamera(cameraRetryCount);
    if (!cap.isOpened()) {
        LOG_CRITICAL("Could not access video source");
        ocr.End();
        return -1;
    }
    
    // Initialize CSV output if enabled
    ofstream csvFile;
    if (config.isCSVEnabled()) {
        string csvPath = config.getCSVFile();
        csvFile.open(csvPath);
        if (!csvFile.is_open()) {
            LOG_ERROR("Unable to open CSV file for writing: " + csvPath);
        } else {
            csvFile << "Time,HR,SpO2,ABP,ECG_Classification,ECG_Confidence" << endl;
            LOG_INFO("CSV output enabled: " + csvPath);
        }
    }
    
    int frame_count = 0;
    int processingInterval = config.getProcessingInterval();
    bool debugMode = config.isDebugMode();
    
    LOG_INFO("Starting main processing loop...");
    LOG_INFO("Processing interval: every " + to_string(processingInterval) + " frames");
    
    while (!g_shutdown) {
        Mat frame;
        cap >> frame;
        
        if (frame.empty()) {
            LOG_WARN("Empty frame received");
            
            // Attempt to reconnect
            cap.release();
            cap = initializeCamera(cameraRetryCount);
            
            if (!cap.isOpened()) {
                LOG_ERROR("Failed to reconnect to video source");
                break;
            }
            continue;
        }

        if (frame_count % processingInterval == 0) {
            auto timestamp = chrono::system_clock::now();
            time_t time_now = chrono::system_clock::to_time_t(timestamp);
            ostringstream timeStream;
            timeStream << put_time(localtime(&time_now), "%Y-%m-%d %H:%M:%S");
            string timeStr = timeStream.str();

            // Extract vital signs
            map<string, string> healthData = processFrame(frame);
            
            // Prepare cropped frame for ML inference
            cv::Mat cropped;
            resize_and_crop(&frame, &cropped);

            // Prepare features for ML model
            size_t feature_ix = 0;
            for (int rx = 0; rx < cropped.rows; rx++) {
                for (int cx = 0; cx < cropped.cols; cx++) {
                    cv::Vec3b pixel = cropped.at<cv::Vec3b>(rx, cx);
                    uint8_t b = pixel.val[0];
                    uint8_t g = pixel.val[1];
                    uint8_t r = pixel.val[2];
                    features[feature_ix++] = (r << 16) + (g << 8) + b;
                }
            }

            // Run ML classifier
            string ecgClassification = "unknown";
            float ecgConfidence = 0.0f;
            
            if (config.isMLModelEnabled()) {
                ei_impulse_result_t result;
                signal_t signal;
                numpy::signal_from_buffer(features, EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT, &signal);
                EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);
                
                if (res == 0) {
                    LOG_DEBUG("ML Inference - DSP: " + to_string(result.timing.dsp) + "ms, Classification: " + to_string(result.timing.classification) + "ms");
                    
                    // Find highest confidence classification
                    float maxConfidence = 0.0f;
                    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
                        if (result.classification[ix].value > maxConfidence) {
                            maxConfidence = result.classification[ix].value;
                            ecgClassification = result.classification[ix].label;
                            ecgConfidence = result.classification[ix].value;
                        }
                        
                        if (debugMode) {
                            LOG_DEBUG("  " + string(result.classification[ix].label) + ": " + to_string(result.classification[ix].value));
                        }
                    }
                } else {
                    LOG_ERROR("ML classifier failed with error: " + to_string(res));
                }
            }
            
            // Output results
            if (config.isConsoleOutputEnabled()) {
                cout << "Time: " << timeStr 
                     << " | HR: " << healthData["HR"] 
                     << " | SpO₂: " << healthData["SpO2"] 
                     << " | ABP: " << healthData["ABP"]
                     << " | ECG: " << ecgClassification << " (" << ecgConfidence << ")"
                     << endl;
            }
            
            // Save to CSV
            if (csvFile.is_open()) {
                csvFile << timeStr << "," 
                       << healthData["HR"] << "," 
                       << healthData["SpO2"] << "," 
                       << healthData["ABP"] << ","
                       << ecgClassification << ","
                       << ecgConfidence << endl;
            }
            
            // Save to database
            if (dbEnabled && db.isConnected()) {
                VitalSignData data;
                data.timestamp = timeStr;
                data.hr = healthData["HR"];
                data.spo2 = healthData["SpO2"];
                data.abp = healthData["ABP"];
                data.ecg_classification = ecgClassification;
                data.ecg_confidence = ecgConfidence;
                
                if (!db.insertVitalSign(data)) {
                    LOG_WARN("Failed to insert data to database, attempting reconnect...");
                    if (db.reconnect(config.getDBRetryAttempts(), config.getDBRetryDelayMs())) {
                        db.insertVitalSign(data); // Retry once after reconnect
                    }
                }
            }
            
            if (debugMode) {
                cv::imshow("Video", cropped);
                if (cv::waitKey(10) >= 0) break;
            }
        }

        if (waitKey(1) == 'q') {
            LOG_INFO("User requested shutdown");
            break;
        }

        frame_count++;
    }

    // Cleanup
    LOG_INFO("Shutting down...");
    cap.release();
    destroyAllWindows();
    if (csvFile.is_open()) {
        csvFile.close();
        LOG_INFO("CSV file closed");
    }
    ocr.End();
    db.disconnect();
    logger.flush();

    LOG_INFO("=== Vital Sign Extraction System Stopped ===");
    return 0;
}
