#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include "SPIFFS.h"
#include "config.h"
#include "wifi_attack.h"

// --- Класс для Captive Portal ---
class CaptiveRequestHandler : public AsyncWebHandler {
private:
    String target_ssid;
    
public:
    CaptiveRequestHandler(const String& ssid) : target_ssid(ssid) {}
    virtual ~CaptiveRequestHandler() {}
    
    bool canHandle(AsyncWebServerRequest *request) override { return true; }
    void handleRequest(AsyncWebServerRequest *request) override;
};

// --- Класс для управления веб-сервером ---
class WebServerManager {
private:
    AsyncWebServer server;
    DNSServer dnsServer;
    CaptiveRequestHandler* captiveHandler;
    bool setup_mode;
    bool evil_twin_active;
    
    // Статистика
    unsigned long credentials_captured;
    unsigned long last_activity;
    
public:
    WebServerManager();
    ~WebServerManager();
    
    // Инициализация
    bool init();
    
    // Режимы работы
    bool startSetupMode();
    bool startEvilTwin(const AttackConfig& config);
    void stop();
    
    // Обработка запросов
    void handleLoop();
    
    // Статистика
    unsigned long getCredentialsCaptured() const { return credentials_captured; }
    unsigned long getLastActivity() const { return last_activity; }
    bool isSetupMode() const { return setup_mode; }
    bool isEvilTwinActive() const { return evil_twin_active; }
    
private:
    // Обработчики для режима настройки
    void setupRoutes();
    void handleRoot(AsyncWebServerRequest *request);
    void handleScanClients(AsyncWebServerRequest *request);
    void handleClientsResult(AsyncWebServerRequest *request);
    void handleLoot(AsyncWebServerRequest *request);
    void handleAttack(AsyncWebServerRequest *request);
    
    // Обработчики для Evil Twin
    void setupEvilTwinRoutes();
    void handleTryPassword(AsyncWebServerRequest *request);
    void handleGetWifiCreds(AsyncWebServerRequest *request);
    
    // Утилиты
    void saveCredentials(const String& ssid, const String& password);
    String generateNetworkTable();
    String generateClientsList();
    bool validateRequest(AsyncWebServerRequest *request, const std::vector<String>& required_params);
    void updateActivity() { last_activity = millis(); }
};

// --- Глобальная переменная ---
extern WebServerManager webServerManager;

#endif // WEB_SERVER_H
