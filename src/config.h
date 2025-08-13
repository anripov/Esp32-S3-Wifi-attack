#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <EEPROM.h>
#include "freertos/semphr.h"

// ESP32-S3 specific configurations
#ifdef ESP32S3
    #define BOARD_NAME "ESP32-S3 DevKitC-1"
    #define PSRAM_SIZE (8 * 1024 * 1024)  // 8MB PSRAM
    #define FLASH_SIZE (16 * 1024 * 1024) // 16MB Flash
    #define CPU_FREQ_MHZ 240
    #define WIFI_TX_POWER 20  // Max TX power for ESP32-S3
#else
    #define BOARD_NAME "ESP32 DevKit"
    #define PSRAM_SIZE (4 * 1024 * 1024)  // 4MB PSRAM
    #define FLASH_SIZE (4 * 1024 * 1024)  // 4MB Flash
    #define CPU_FREQ_MHZ 240
    #define WIFI_TX_POWER 20
#endif

// --- Константы безопасности ---
#define MAX_SSID_LENGTH 32
#define MAX_PASSWORD_LENGTH 64
#define MAC_ADDRESS_LENGTH 18

// Динамические константы (устанавливаются автоматически при инициализации)
extern size_t DYNAMIC_MAX_CLIENTS;
extern size_t DYNAMIC_QUEUE_SIZE;
extern size_t DYNAMIC_MAX_LOG_ENTRIES;
extern size_t DYNAMIC_STRING_POOL_SIZE;
extern size_t DYNAMIC_BUFFER_POOL_SIZE;

// Макросы для обратной совместимости
#define MAX_CLIENTS DYNAMIC_MAX_CLIENTS
#define QUEUE_SIZE DYNAMIC_QUEUE_SIZE
#define MAX_LOG_ENTRIES DYNAMIC_MAX_LOG_ENTRIES
#define STRING_POOL_SIZE DYNAMIC_STRING_POOL_SIZE
#define BUFFER_POOL_SIZE DYNAMIC_BUFFER_POOL_SIZE

#define SNIFFING_TIMEOUT_MS 15000
#define MAX_DEAUTH_DURATION_MS 60000

// --- Константы ---
#define SETUP_SSID "EvilTwin_Config"
#define CONFIG_MAGIC 0xDEADBEEF

// --- Логирование ---
enum LogLevel {
    LOG_ERROR = 0,
    LOG_WARN = 1,
    LOG_INFO = 2,
    LOG_DEBUG = 3
};

#define LOG_LEVEL LOG_INFO

// --- Состояния системы ---
enum SystemState {
    STATE_SETUP,
    STATE_ATTACK
};

// --- Структура конфигурации ---
struct AttackConfig {
    char target_ssid[MAX_SSID_LENGTH + 1];
    uint8_t target_bssid[6];
    char target_client_mac[MAC_ADDRESS_LENGTH + 1];
    int target_channel;
    int deauth_duration_ms;
    uint32_t magic_number;
    uint32_t crc32;
};

#define EEPROM_SIZE sizeof(AttackConfig) + 1

// --- Класс для управления конфигурацией ---
class ConfigManager {
private:
    AttackConfig current_config;
    SemaphoreHandle_t config_mutex;
    
public:
    ConfigManager();
    ~ConfigManager();
    
    bool init();
    bool loadConfig();
    bool saveConfig(const AttackConfig& config);
    bool getConfig(AttackConfig& config);
    bool setConfig(const AttackConfig& config);
    
    // Валидация
    static bool isValidSSID(const char* ssid);
    static bool isValidMacAddress(const char* mac);
    static bool isValidChannel(int channel);
    static bool isValidDuration(int duration_ms);
    static bool parseMac(const char* macStr, uint8_t* macArray);
    
    // Утилиты
    static void safeStrncpy(char* dest, const char* src, size_t dest_size);
    static uint32_t calculateCRC32(const uint8_t* data, size_t length);
};

// --- Функции логирования ---
void logMessage(LogLevel level, const char* format, ...);
bool sanitizeInput(String& input, size_t maxLength);

// --- Функции динамической конфигурации ---
void initializeDynamicConstants();
void applyHardwareOptimizedConstants(bool is_esp32s3, bool has_psram, size_t psram_size);

// --- Глобальные переменные ---
extern ConfigManager configManager;
extern SystemState currentState;

#endif // CONFIG_H
