
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <tesseract/baseapi.h>
#include <fstream>
#include <iostream>
#include <regex>
#include <deque>
#include <chrono>
#include <iomanip>
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
#include "unistd.h"
#include <map>
#include <string>
#include <cmath>

using namespace cv;
using namespace cv::dnn;
using namespace std;

// Initialize Tesseract OCR
static tesseract::TessBaseAPI ocr;

// Define key labels and patterns
static vector<string> labels = {"HR", "SpO2", "ABP"};
static regex spo2_pattern(R"(\bsp[o0]2\b)", regex_constants::icase);
static regex bp_pattern(R"(^\d{2,3}/\d{2,3}$)");

deque<string> spo2_history;
string last_spo2_value = "81"; // Default fallback for SpO₂

static bool use_debug = false;
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

            if (text && conf > 50) { // Confidence threshold
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

int main() {
    if (ocr.Init(NULL, "eng")) {
        cerr << "Error: Could not initialize tesseract." << endl;
        return -1;
    }

    VideoCapture cap("/home/KK/1.mp4");
    if (!cap.isOpened()) {
        cerr << "Error: Could not access the camera." << endl;
        return -1;
    }

    ofstream csvFile("live_vital_signs_output.csv");
    if (!csvFile.is_open()) {
        cerr << "Error: Unable to open CSV file for writing." << endl;
        return -1;
    }
    csvFile << "Time,HR,SpO2,ABP" << endl;

    int frame_count = 0;

    while (true) {
        Mat frame;
        cap >> frame;
        if (frame.empty()) {
            cerr << "Warning: Empty frame received. Skipping..." << endl;
            continue;
        }

        if (frame_count % 300 == 0) {
            auto timestamp = chrono::system_clock::now();
            time_t time_now = chrono::system_clock::to_time_t(timestamp);
            ostringstream timeStream;
            timeStream << put_time(localtime(&time_now), "%H:%M:%S");
            string timeStr = timeStream.str();

            map<string, string> healthData = processFrame(frame);
            csvFile << timeStr << "," << healthData["HR"] << "," << healthData["SpO2"] << "," << healthData["ABP"] << endl;
            cout << "Time: " << timeStr << " | HR: " << healthData["HR"] << " | SpO₂: " << healthData["SpO2"] << " | ABP: " << healthData["ABP"] << endl;
            cv::Mat cropped;
            resize_and_crop(&frame, &cropped);

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

            ei_impulse_result_t result;
            signal_t signal;
            numpy::signal_from_buffer(features, EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT, &signal);
            EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);
            if (res != 0) {
                printf("ERR: Failed to run classifier (%d)\n", res);
                return 1;
            }

            printf("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.): \n",
                    result.timing.dsp, result.timing.classification, result.timing.anomaly);

            printf("#Classification results:\n");
            for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
                printf("%s: %.05f\n", result.classification[ix].label, result.classification[ix].value);
            }
    
            if (use_debug) {
                cv::imshow("Video", cropped);
                if (cv::waitKey(10) >= 0) break;
            }
       
       
        }

        

        if (waitKey(1) == 'q') break;

        frame_count++;
    }

    cap.release();
    destroyAllWindows();
    csvFile.close();
    ocr.End();

    cout << "\nData saved to live_vital_signs_output.csv" << endl;
    return 0;
}
