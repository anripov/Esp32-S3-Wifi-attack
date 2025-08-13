#ifndef MONITORING_H
#define MONITORING_H

#include <Arduino.h>
#include <vector>
#include <map>
#include "SPIFFS.h"
#include "config.h"

// --- Структуры для мониторинга ---
struct SystemMetrics {
    unsigned long uptime_ms;
    size_t free_heap;
    size_t total_heap;
    size_t min_free_heap;
    float heap_fragmentation;
    unsigned long wifi_packets_sent;
    unsigned long wifi_packets_received;
    unsigned long credentials_captured;
    unsigned long clients_discovered;
    unsigned long attacks_performed;
    float cpu_usage_percent;
    int wifi_signal_strength;
    unsigned long last_activity;
};

struct LogEntry {
    unsigned long timestamp;
    LogLevel level;
    String component;
    String message;
    
    LogEntry(LogLevel l, const String& comp, const String& msg) 
        : timestamp(millis()), level(l), component(comp), message(msg) {}
};

struct AttackStatistics {
    unsigned long start_time;
    unsigned long duration_ms;
    unsigned long packets_sent;
    unsigned long clients_targeted;
    String target_ssid;
    String target_bssid;
    bool success;
    String result_description;
};

// --- Класс для мониторинга системы ---
class SystemMonitor {
private:
    std::vector<LogEntry> log_buffer;
    std::vector<AttackStatistics> attack_history;
    SystemMetrics current_metrics;
    
    // Настройки
    size_t max_log_entries;
    size_t max_attack_history;
    unsigned long metrics_update_interval;
    unsigned long last_metrics_update;
    
    // Файлы логов
    const char* log_file_path = "/system.log";
    const char* metrics_file_path = "/metrics.json";
    const char* attacks_file_path = "/attacks.json";
    
    // Статистика
    std::map<String, unsigned long> component_counters;
    std::map<LogLevel, unsigned long> level_counters;
    
public:
    SystemMonitor();
    ~SystemMonitor();
    
    // Инициализация
    bool init();
    
    // Логирование
    void log(LogLevel level, const String& component, const String& message);
    void logAttack(const AttackStatistics& attack);
    
    // Метрики
    void updateMetrics();
    SystemMetrics getMetrics() const { return current_metrics; }
    
    // Статистика
    std::vector<LogEntry> getRecentLogs(size_t count = 50) const;
    std::vector<AttackStatistics> getAttackHistory(size_t count = 10) const;
    std::map<String, unsigned long> getComponentStats() const { return component_counters; }
    std::map<LogLevel, unsigned long> getLevelStats() const { return level_counters; }
    
    // Отчеты
    String generateSystemReport() const;
    String generateAttackReport() const;
    String generateMetricsJSON() const;
    
    // Управление файлами
    bool saveLogsToFile();
    bool loadLogsFromFile();
    bool clearLogs();
    bool exportLogs(const String& format = "json") const;
    
    // Алерты и уведомления
    void checkAlerts();
    bool isSystemHealthy() const;
    std::vector<String> getActiveAlerts() const;
    
    // Утилиты
    void cleanup(); // Очистка старых данных
    size_t getLogBufferSize() const { return log_buffer.size(); }
    float getMemoryUsagePercent() const;
    String formatUptime() const;
    
private:
    void rotateLogBuffer();
    void rotateAttackHistory();
    String logLevelToString(LogLevel level) const;
    LogLevel stringToLogLevel(const String& level) const;
    String formatTimestamp(unsigned long timestamp) const;
    void updateComponentCounter(const String& component);
    void updateLevelCounter(LogLevel level);
};

// --- Класс для отчетности ---
class ReportGenerator {
private:
    SystemMonitor* monitor;
    
public:
    ReportGenerator(SystemMonitor* mon) : monitor(mon) {}
    
    // HTML отчеты
    String generateDashboardHTML() const;
    String generateLogsHTML() const;
    String generateAttacksHTML() const;
    String generateMetricsHTML() const;
    
    // JSON отчеты
    String generateSystemJSON() const;
    String generateAttacksJSON() const;
    String generateMetricsJSON() const;
    
    // CSV отчеты
    String generateLogsCSV() const;
    String generateAttacksCSV() const;
    
    // Статистические отчеты
    String generateStatisticsHTML() const;
    String generatePerformanceHTML() const;
    
private:
    String escapeHTML(const String& text) const;
    String formatDuration(unsigned long duration_ms) const;
    String getLogLevelBadge(LogLevel level) const;
    String getSuccessBadge(bool success) const;
};

// --- Глобальные переменные ---
extern SystemMonitor systemMonitor;
extern ReportGenerator reportGenerator;

// --- Макросы для удобного логирования ---
#define LOG_SYSTEM(level, message) systemMonitor.log(level, "SYSTEM", message)
#define LOG_WIFI(level, message) systemMonitor.log(level, "WIFI", message)
#define LOG_WEB(level, message) systemMonitor.log(level, "WEB", message)
#define LOG_CONFIG(level, message) systemMonitor.log(level, "CONFIG", message)
#define LOG_ATTACK(level, message) systemMonitor.log(level, "ATTACK", message)

#endif // MONITORING_H
