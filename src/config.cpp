#include "config.h"
#include "esp_crc.h"
#include <cstring>

// Динамические константы (инициализируются при запуске)
size_t DYNAMIC_MAX_CLIENTS = 50;        // По умолчанию для ESP32
size_t DYNAMIC_QUEUE_SIZE = 20;
size_t DYNAMIC_MAX_LOG_ENTRIES = 200;
size_t DYNAMIC_STRING_POOL_SIZE = 50;
size_t DYNAMIC_BUFFER_POOL_SIZE = 20;

// --- Глобальные переменные ---
ConfigManager configManager;
SystemState currentState = STATE_SETUP;

// --- Функции логирования ---
void logMessage(LogLevel level, const char* format, ...) {
    if (level > LOG_LEVEL) return;
    
    const char* level_str[] = {"ERROR", "WARN", "INFO", "DEBUG"};
    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "[%lu]", millis());
    
    char message[256];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    Serial.printf("%s [%s] %s\n", timestamp, level_str[level], message);
}

bool sanitizeInput(String& input, size_t maxLength) {
    if (input.length() > maxLength) {
        input = input.substring(0, maxLength);
        logMessage(LOG_WARN, "Input truncated to %d characters", maxLength);
    }

    // Оптимизированная замена символов (избегаем множественных копирований)
    input.reserve(input.length() * 1.2); // Резервируем место для экранирования

    // Используем более эффективный подход с одним проходом
    String result;
    result.reserve(input.length() * 1.2);

    for (size_t i = 0; i < input.length(); i++) {
        char c = input.charAt(i);
        switch (c) {
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '"': result += "&quot;"; break;
            case '\'': result += "&#x27;"; break;
            case '&': result += "&amp;"; break;
            default: result += c; break;
        }
    }

    input = std::move(result); // Используем move семантику
    return true;
}

// --- Реализация ConfigManager ---
ConfigManager::ConfigManager() : config_mutex(nullptr) {
    memset(&current_config, 0, sizeof(current_config));
}

ConfigManager::~ConfigManager() {
    if (config_mutex) {
        vSemaphoreDelete(config_mutex);
    }
}

bool ConfigManager::init() {
    // Создание мьютекса
    config_mutex = xSemaphoreCreateMutex();
    if (config_mutex == NULL) {
        logMessage(LOG_ERROR, "Failed to create config mutex!");
        return false;
    }
    
    // Инициализация EEPROM
    if (!EEPROM.begin(EEPROM_SIZE)) {
        logMessage(LOG_ERROR, "EEPROM initialization failed!");
        return false;
    }
    
    logMessage(LOG_INFO, "ConfigManager initialized successfully");
    return true;
}

bool ConfigManager::loadConfig() {
    if (xSemaphoreTake(config_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        logMessage(LOG_ERROR, "Failed to acquire config mutex for loading");
        return false;
    }
    
    AttackConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    
    EEPROM.get(1, cfg);
    
    // Проверка магического числа
    if (cfg.magic_number != CONFIG_MAGIC) {
        logMessage(LOG_ERROR, "Invalid magic number in config: 0x%08X", cfg.magic_number);
        xSemaphoreGive(config_mutex);
        return false;
    }
    
    // Проверка CRC
    uint32_t crc = calculateCRC32((uint8_t*)&cfg, sizeof(AttackConfig) - sizeof(uint32_t));
    if (crc != cfg.crc32) {
        logMessage(LOG_ERROR, "Config CRC mismatch: expected 0x%08X, got 0x%08X", cfg.crc32, crc);
        xSemaphoreGive(config_mutex);
        return false;
    }
    
    // Валидация данных
    if (!isValidSSID(cfg.target_ssid)) {
        logMessage(LOG_ERROR, "Invalid SSID in config");
        xSemaphoreGive(config_mutex);
        return false;
    }
    
    if (!isValidChannel(cfg.target_channel)) {
        logMessage(LOG_ERROR, "Invalid channel in config: %d", cfg.target_channel);
        xSemaphoreGive(config_mutex);
        return false;
    }
    
    if (!isValidDuration(cfg.deauth_duration_ms)) {
        logMessage(LOG_ERROR, "Invalid duration in config: %d", cfg.deauth_duration_ms);
        xSemaphoreGive(config_mutex);
        return false;
    }
    
    current_config = cfg;
    logMessage(LOG_INFO, "Config loaded successfully: SSID=%s, Channel=%d", 
               cfg.target_ssid, cfg.target_channel);
    
    xSemaphoreGive(config_mutex);
    return true;
}

bool ConfigManager::saveConfig(const AttackConfig& config) {
    if (xSemaphoreTake(config_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        logMessage(LOG_ERROR, "Failed to acquire config mutex for saving");
        return false;
    }
    
    AttackConfig cfg_to_save = config;
    cfg_to_save.magic_number = CONFIG_MAGIC;
    
    // Вычисляем CRC без учета самого поля CRC
    cfg_to_save.crc32 = calculateCRC32((uint8_t*)&cfg_to_save, sizeof(AttackConfig) - sizeof(uint32_t));
    
    EEPROM.put(1, cfg_to_save);
    if (!EEPROM.commit()) {
        logMessage(LOG_ERROR, "Failed to save config to EEPROM");
        xSemaphoreGive(config_mutex);
        return false;
    }
    
    current_config = cfg_to_save;
    logMessage(LOG_INFO, "Config saved successfully: SSID=%s, Channel=%d", 
               config.target_ssid, config.target_channel);
    
    xSemaphoreGive(config_mutex);
    return true;
}

bool ConfigManager::getConfig(AttackConfig& config) {
    if (xSemaphoreTake(config_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        logMessage(LOG_ERROR, "Failed to acquire config mutex for getting");
        return false;
    }
    
    config = current_config;
    xSemaphoreGive(config_mutex);
    return true;
}

bool ConfigManager::setConfig(const AttackConfig& config) {
    if (xSemaphoreTake(config_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        logMessage(LOG_ERROR, "Failed to acquire config mutex for setting");
        return false;
    }
    
    current_config = config;
    xSemaphoreGive(config_mutex);
    return true;
}

// --- Статические методы валидации ---
bool ConfigManager::isValidSSID(const char* ssid) {
    if (!ssid) return false;
    size_t len = strlen(ssid);
    return len > 0 && len <= MAX_SSID_LENGTH;
}

bool ConfigManager::isValidMacAddress(const char* mac) {
    if (!mac || strlen(mac) != 17) return false;
    
    for (int i = 0; i < 17; i++) {
        if (i % 3 == 2) {
            if (mac[i] != ':') return false;
        } else {
            if (!isxdigit(mac[i])) return false;
        }
    }
    return true;
}

bool ConfigManager::isValidChannel(int channel) {
    return channel >= 1 && channel <= 13;
}

bool ConfigManager::isValidDuration(int duration_ms) {
    return duration_ms > 0 && duration_ms <= MAX_DEAUTH_DURATION_MS;
}

bool ConfigManager::parseMac(const char* macStr, uint8_t* macArray) {
    if (!macStr || !macArray || !isValidMacAddress(macStr)) {
        logMessage(LOG_ERROR, "Invalid MAC address format: %s", macStr ? macStr : "NULL");
        return false;
    }
    
    int result = sscanf(macStr, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", 
                       &macArray[0], &macArray[1], &macArray[2], 
                       &macArray[3], &macArray[4], &macArray[5]);
    
    if (result != 6) {
        logMessage(LOG_ERROR, "Failed to parse MAC address: %s", macStr);
        return false;
    }
    
    logMessage(LOG_DEBUG, "Parsed MAC: %02X:%02X:%02X:%02X:%02X:%02X", 
               macArray[0], macArray[1], macArray[2], macArray[3], macArray[4], macArray[5]);
    return true;
}

void ConfigManager::safeStrncpy(char* dest, const char* src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) return;
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

uint32_t ConfigManager::calculateCRC32(const uint8_t* data, size_t length) {
    return esp_rom_crc32_le(0, data, length);
}

// --- Функции динамической конфигурации ---
void initializeDynamicConstants() {
    // Устанавливаем значения по умолчанию для ESP32
    DYNAMIC_MAX_CLIENTS = 50;
    DYNAMIC_QUEUE_SIZE = 20;
    DYNAMIC_MAX_LOG_ENTRIES = 200;
    DYNAMIC_STRING_POOL_SIZE = 50;
    DYNAMIC_BUFFER_POOL_SIZE = 20;

    Serial.println("[CONFIG] Dynamic constants initialized with default values");
}

void applyHardwareOptimizedConstants(bool is_esp32s3, bool has_psram, size_t psram_size) {
    Serial.println("[CONFIG] Applying hardware-optimized constants...");

    if (is_esp32s3) {
        if (has_psram && psram_size >= 8 * 1024 * 1024) {
            // ESP32-S3 с 8MB+ PSRAM - максимальная производительность
            DYNAMIC_MAX_CLIENTS = 100;
            DYNAMIC_QUEUE_SIZE = 50;
            DYNAMIC_MAX_LOG_ENTRIES = 500;
            DYNAMIC_STRING_POOL_SIZE = 100;
            DYNAMIC_BUFFER_POOL_SIZE = 50;
            Serial.println("[CONFIG] High-performance constants applied (ESP32-S3 + 8MB PSRAM)");
        } else if (has_psram) {
            // ESP32-S3 с PSRAM - улучшенная производительность
            DYNAMIC_MAX_CLIENTS = 75;
            DYNAMIC_QUEUE_SIZE = 35;
            DYNAMIC_MAX_LOG_ENTRIES = 350;
            DYNAMIC_STRING_POOL_SIZE = 75;
            DYNAMIC_BUFFER_POOL_SIZE = 35;
            Serial.println("[CONFIG] Enhanced constants applied (ESP32-S3 + PSRAM)");
        } else {
            // ESP32-S3 без PSRAM - стандартная производительность
            DYNAMIC_MAX_CLIENTS = 60;
            DYNAMIC_QUEUE_SIZE = 25;
            DYNAMIC_MAX_LOG_ENTRIES = 250;
            DYNAMIC_STRING_POOL_SIZE = 60;
            DYNAMIC_BUFFER_POOL_SIZE = 25;
            Serial.println("[CONFIG] Standard constants applied (ESP32-S3)");
        }
    } else {
        // Обычный ESP32
        if (has_psram) {
            // ESP32 с PSRAM
            DYNAMIC_MAX_CLIENTS = 60;
            DYNAMIC_QUEUE_SIZE = 25;
            DYNAMIC_MAX_LOG_ENTRIES = 300;
            DYNAMIC_STRING_POOL_SIZE = 60;
            DYNAMIC_BUFFER_POOL_SIZE = 25;
            Serial.println("[CONFIG] Enhanced constants applied (ESP32 + PSRAM)");
        } else {
            // ESP32 без PSRAM - базовые настройки
            DYNAMIC_MAX_CLIENTS = 50;
            DYNAMIC_QUEUE_SIZE = 20;
            DYNAMIC_MAX_LOG_ENTRIES = 200;
            DYNAMIC_STRING_POOL_SIZE = 50;
            DYNAMIC_BUFFER_POOL_SIZE = 20;
            Serial.println("[CONFIG] Basic constants applied (ESP32)");
        }
    }

    Serial.printf("[CONFIG] Applied: MAX_CLIENTS=%d, QUEUE_SIZE=%d, LOG_ENTRIES=%d\n",
                 DYNAMIC_MAX_CLIENTS, DYNAMIC_QUEUE_SIZE, DYNAMIC_MAX_LOG_ENTRIES);
}
