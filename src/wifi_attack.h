#ifndef WIFI_ATTACK_H
#define WIFI_ATTACK_H

#include <Arduino.h>
#include <WiFi.h>
#include <vector>
#include "esp_wifi.h"
#include "freertos/queue.h"
#include "config.h"

// --- Структуры для Wi-Fi пакетов ---
typedef struct {
    uint16_t frame_ctrl;
    uint16_t duration_id;
    uint8_t addr1[6]; // Receiver
    uint8_t addr2[6]; // Transmitter
    uint8_t addr3[6]; // BSSID
    uint16_t seq_ctrl;
    uint8_t addr4[6]; // Optional
} wifi_ieee80211_mac_hdr_t;

typedef struct {
    wifi_ieee80211_mac_hdr_t hdr;
    uint8_t payload[0];
} wifi_ieee80211_packet_t;

// --- Класс для управления WiFi атаками ---
class WiFiAttackManager {
private:
    std::vector<String> found_clients;
    volatile bool sniffing_active;
    unsigned long sniffing_start_time;
    QueueHandle_t sniffer_queue;
    AttackConfig current_attack_config;
    
    // Статистика
    unsigned long packets_sent;
    unsigned long attack_start_time;
    
public:
    WiFiAttackManager();
    ~WiFiAttackManager();
    
    // Инициализация
    bool init();
    
    // Сканирование сетей
    std::vector<WiFiNetwork> scanNetworks();
    
    // Сниффинг клиентов
    bool startClientSniffing(const char* ssid, const char* bssid, int channel);
    void stopClientSniffing();
    bool isSniffingActive() const { return sniffing_active; }
    std::vector<String> getFoundClients() const { return found_clients; }
    void processSnifferQueue();
    
    // Deauth атаки
    bool performDeauthAttack(int duration_ms, const uint8_t* client_mac = nullptr);
    
    // Статистика
    unsigned long getPacketsSent() const { return packets_sent; }
    unsigned long getAttackDuration() const;
    
    // Утилиты
    static const char* getEncryptionTypeStr(wifi_auth_mode_t type);
    
    // Callback для сниффера
    static void snifferCallback(void* buf, wifi_promiscuous_pkt_type_t type);
    
private:
    void resetStats();
    bool validateAttackConfig();
};

// --- Структура для информации о WiFi сети ---
struct WiFiNetwork {
    String ssid;
    String bssid;
    int32_t rssi;
    uint8_t channel;
    wifi_auth_mode_t encryption;
    
    WiFiNetwork(const String& s, const String& b, int32_t r, uint8_t c, wifi_auth_mode_t e)
        : ssid(s), bssid(b), rssi(r), channel(c), encryption(e) {}
};

// --- Глобальная переменная ---
extern WiFiAttackManager wifiAttackManager;

#endif // WIFI_ATTACK_H
