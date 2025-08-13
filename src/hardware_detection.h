/*
 * Hardware Detection and Auto-Configuration
 * 
 * This module automatically detects ESP32-S3 hardware capabilities
 * and configures the system accordingly.
 */

#ifndef HARDWARE_DETECTION_H
#define HARDWARE_DETECTION_H

#include <Arduino.h>
#include "config.h"

class HardwareDetection {
private:
    static bool s3_detected;
    static bool psram_available;
    static size_t flash_size;
    static size_t psram_size;
    static uint32_t cpu_frequency;
    static String board_model;
    
public:
    // Hardware detection methods
    static bool detectHardware();
    static bool isESP32S3();
    static bool isPSRAMAvailable();
    static size_t getFlashSize();
    static size_t getPSRAMSize();
    static uint32_t getCPUFrequency();
    static String getBoardModel();
    
    // Auto-configuration methods
    static void autoConfigureMemory();
    static void autoConfigureWiFi();
    static void autoConfigurePerformance();
    
    // Validation methods
    static bool validateHardware();
    static bool validateMemoryConfiguration();
    static bool validateWiFiConfiguration();
    
    // Information display
    static void printHardwareInfo();
    static void printConfiguration();
    
    // Optimization recommendations
    static void recommendOptimizations();

    // Configuration printing
    static void printConfiguration();
    
private:
    static void detectChipModel();
    static void detectMemoryConfiguration();
    static void detectWiFiCapabilities();
    static void applyOptimalSettings();
};

// Hardware capability structure
struct HardwareCapabilities {
    bool esp32s3;
    bool psram_8mb;
    bool flash_16mb;
    bool usb_native;
    bool dual_core;
    uint32_t max_cpu_freq;
    uint32_t max_wifi_power;
    size_t max_clients;
    size_t optimal_buffer_size;
    
    HardwareCapabilities() :
        esp32s3(false),
        psram_8mb(false),
        flash_16mb(false),
        usb_native(false),
        dual_core(false),
        max_cpu_freq(240),
        max_wifi_power(20),
        max_clients(50),
        optimal_buffer_size(1024) {}
};

// Auto-configuration profiles
enum ConfigProfile {
    PROFILE_AUTO,           // Automatic detection
    PROFILE_PERFORMANCE,    // Maximum performance
    PROFILE_BALANCED,       // Balanced performance/power
    PROFILE_POWER_SAVE,     // Power optimized
    PROFILE_MINIMAL,        // Minimal resources
    PROFILE_DEBUG          // Debug optimized
};

class AutoConfigurator {
private:
    static ConfigProfile current_profile;
    static HardwareCapabilities capabilities;
    
public:
    // Profile management
    static void setProfile(ConfigProfile profile);
    static ConfigProfile getProfile();
    static void applyProfile();
    
    // Automatic configuration
    static void autoDetectAndConfigure();
    static void configureForProfile(ConfigProfile profile);
    
    // Manual overrides
    static void overrideMemorySettings(size_t max_clients, size_t buffer_size);
    static void overrideWiFiSettings(uint8_t tx_power, uint8_t channel);
    static void overridePerformanceSettings(uint32_t cpu_freq, bool dual_core);
    
    // Validation and testing
    static bool validateConfiguration();
    static bool testMemoryPerformance();
    static bool testWiFiPerformance();
    static bool testSystemStability();
    
private:
    static void applyPerformanceProfile();
    static void applyBalancedProfile();
    static void applyPowerSaveProfile();
    static void applyMinimalProfile();
    static void applyDebugProfile();
};

// Inline implementations for critical functions
inline bool HardwareDetection::isESP32S3() {
    return s3_detected;
}

inline bool HardwareDetection::isPSRAMAvailable() {
    return psram_available;
}

inline size_t HardwareDetection::getFlashSize() {
    return flash_size;
}

inline size_t HardwareDetection::getPSRAMSize() {
    return psram_size;
}

inline uint32_t HardwareDetection::getCPUFrequency() {
    return cpu_frequency;
}

inline String HardwareDetection::getBoardModel() {
    return board_model;
}

// Macros for easy hardware checking
#define IS_ESP32S3() HardwareDetection::isESP32S3()
#define HAS_PSRAM() HardwareDetection::isPSRAMAvailable()
#define GET_FLASH_SIZE() HardwareDetection::getFlashSize()
#define GET_PSRAM_SIZE() HardwareDetection::getPSRAMSize()

// Auto-configuration macro
#define AUTO_CONFIGURE_HARDWARE() do { \
    HardwareDetection::detectHardware(); \
    AutoConfigurator::autoDetectAndConfigure(); \
} while(0)

// Conditional compilation based on detected hardware
#ifdef ESP32S3
    #define HARDWARE_SPECIFIC_INIT() do { \
        if (HardwareDetection::isESP32S3()) { \
            Serial.println("ESP32-S3 detected - enabling optimizations"); \
            AutoConfigurator::setProfile(PROFILE_PERFORMANCE); \
        } \
    } while(0)
#else
    #define HARDWARE_SPECIFIC_INIT() do { \
        Serial.println("ESP32 detected - using standard configuration"); \
        AutoConfigurator::setProfile(PROFILE_BALANCED); \
    } while(0)
#endif

// Global hardware detection instance
extern HardwareDetection hardwareDetector;
extern AutoConfigurator autoConfig;

#endif // HARDWARE_DETECTION_H
