#include "monitoring.h"
#include <ArduinoJson.h>

// --- Глобальные переменные ---
SystemMonitor systemMonitor;
ReportGenerator reportGenerator(&systemMonitor);

// --- Реализация SystemMonitor ---
SystemMonitor::SystemMonitor() 
    : max_log_entries(200), max_attack_history(50), 
      metrics_update_interval(5000), last_metrics_update(0) {
    memset(&current_metrics, 0, sizeof(current_metrics));
    log_buffer.reserve(max_log_entries);
    attack_history.reserve(max_attack_history);
}

SystemMonitor::~SystemMonitor() {
    saveLogsToFile();
}

bool SystemMonitor::init() {
    logMessage(LOG_INFO, "Initializing SystemMonitor");
    
    // Инициализация метрик
    current_metrics.total_heap = ESP.getHeapSize();
    current_metrics.min_free_heap = ESP.getFreeHeap();
    
    // Загрузка логов из файла
    loadLogsFromFile();
    
    // Первое обновление метрик
    updateMetrics();
    
    log(LOG_INFO, "MONITOR", "SystemMonitor initialized successfully");
    return true;
}

void SystemMonitor::log(LogLevel level, const String& component, const String& message) {
    // Создание записи лога
    LogEntry entry(level, component, message);
    
    // Добавление в буфер
    log_buffer.push_back(entry);
    
    // Ротация буфера при необходимости
    if (log_buffer.size() > max_log_entries) {
        rotateLogBuffer();
    }
    
    // Обновление счетчиков
    updateComponentCounter(component);
    updateLevelCounter(level);
    
    // Вывод в Serial (если уровень позволяет)
    if (level <= LOG_LEVEL) {
        Serial.printf("[%s] [%s] %s: %s\n", 
                     formatTimestamp(entry.timestamp).c_str(),
                     logLevelToString(level).c_str(),
                     component.c_str(),
                     message.c_str());
    }
    
    // Автосохранение критических сообщений
    if (level <= LOG_ERROR) {
        saveLogsToFile();
    }
}

void SystemMonitor::logAttack(const AttackStatistics& attack) {
    attack_history.push_back(attack);
    
    if (attack_history.size() > max_attack_history) {
        rotateAttackHistory();
    }
    
    current_metrics.attacks_performed++;
    
    log(LOG_INFO, "ATTACK", 
        "Attack logged: " + attack.target_ssid + 
        ", Duration: " + String(attack.duration_ms) + "ms" +
        ", Packets: " + String(attack.packets_sent) +
        ", Success: " + (attack.success ? "Yes" : "No"));
}

void SystemMonitor::updateMetrics() {
    unsigned long now = millis();
    if (now - last_metrics_update < metrics_update_interval) {
        return;
    }
    
    // Обновление базовых метрик
    current_metrics.uptime_ms = now;
    current_metrics.free_heap = ESP.getFreeHeap();
    current_metrics.min_free_heap = min(current_metrics.min_free_heap, current_metrics.free_heap);
    
    // Расчет фрагментации кучи
    if (current_metrics.total_heap > 0) {
        current_metrics.heap_fragmentation = 
            100.0f * (1.0f - (float)ESP.getMaxAllocHeap() / (float)current_metrics.free_heap);
    }
    
    // WiFi метрики
    if (WiFi.getMode() != WIFI_OFF) {
        current_metrics.wifi_signal_strength = WiFi.RSSI();
    }
    
    // Обновление времени последней активности
    current_metrics.last_activity = now;
    
    last_metrics_update = now;
}

std::vector<LogEntry> SystemMonitor::getRecentLogs(size_t count) const {
    std::vector<LogEntry> recent;
    size_t start = log_buffer.size() > count ? log_buffer.size() - count : 0;
    
    for (size_t i = start; i < log_buffer.size(); i++) {
        recent.push_back(log_buffer[i]);
    }
    
    return recent;
}

std::vector<AttackStatistics> SystemMonitor::getAttackHistory(size_t count) const {
    std::vector<AttackStatistics> recent;
    size_t start = attack_history.size() > count ? attack_history.size() - count : 0;
    
    for (size_t i = start; i < attack_history.size(); i++) {
        recent.push_back(attack_history[i]);
    }
    
    return recent;
}

String SystemMonitor::generateSystemReport() const {
    String report = "=== SYSTEM REPORT ===\n";
    report += "Uptime: " + formatUptime() + "\n";
    report += "Free Heap: " + String(current_metrics.free_heap) + " bytes\n";
    report += "Heap Usage: " + String(getMemoryUsagePercent(), 1) + "%\n";
    report += "Heap Fragmentation: " + String(current_metrics.heap_fragmentation, 1) + "%\n";
    report += "WiFi Signal: " + String(current_metrics.wifi_signal_strength) + " dBm\n";
    report += "Attacks Performed: " + String(current_metrics.attacks_performed) + "\n";
    report += "Credentials Captured: " + String(current_metrics.credentials_captured) + "\n";
    report += "Clients Discovered: " + String(current_metrics.clients_discovered) + "\n";
    report += "Log Entries: " + String(log_buffer.size()) + "\n";
    
    return report;
}

String SystemMonitor::generateAttackReport() const {
    String report = "=== ATTACK HISTORY ===\n";
    
    for (const auto& attack : attack_history) {
        report += "Target: " + attack.target_ssid + "\n";
        report += "Duration: " + String(attack.duration_ms) + "ms\n";
        report += "Packets: " + String(attack.packets_sent) + "\n";
        report += "Success: " + (attack.success ? "Yes" : "No") + "\n";
        report += "---\n";
    }
    
    return report;
}

String SystemMonitor::generateMetricsJSON() const {
    DynamicJsonDocument doc(1024);
    
    doc["uptime_ms"] = current_metrics.uptime_ms;
    doc["free_heap"] = current_metrics.free_heap;
    doc["total_heap"] = current_metrics.total_heap;
    doc["min_free_heap"] = current_metrics.min_free_heap;
    doc["heap_fragmentation"] = current_metrics.heap_fragmentation;
    doc["wifi_packets_sent"] = current_metrics.wifi_packets_sent;
    doc["wifi_packets_received"] = current_metrics.wifi_packets_received;
    doc["credentials_captured"] = current_metrics.credentials_captured;
    doc["clients_discovered"] = current_metrics.clients_discovered;
    doc["attacks_performed"] = current_metrics.attacks_performed;
    doc["cpu_usage_percent"] = current_metrics.cpu_usage_percent;
    doc["wifi_signal_strength"] = current_metrics.wifi_signal_strength;
    doc["last_activity"] = current_metrics.last_activity;
    
    String result;
    serializeJson(doc, result);
    return result;
}

bool SystemMonitor::saveLogsToFile() {
    File file = SPIFFS.open(log_file_path, "w");
    if (!file) {
        return false;
    }
    
    DynamicJsonDocument doc(8192);
    JsonArray logs = doc.createNestedArray("logs");
    
    // Сохраняем последние 100 записей
    size_t start = log_buffer.size() > 100 ? log_buffer.size() - 100 : 0;
    for (size_t i = start; i < log_buffer.size(); i++) {
        JsonObject entry = logs.createNestedObject();
        entry["timestamp"] = log_buffer[i].timestamp;
        entry["level"] = static_cast<int>(log_buffer[i].level);
        entry["component"] = log_buffer[i].component;
        entry["message"] = log_buffer[i].message;
    }
    
    serializeJson(doc, file);
    file.close();
    return true;
}

bool SystemMonitor::loadLogsFromFile() {
    File file = SPIFFS.open(log_file_path, "r");
    if (!file) {
        return false;
    }
    
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        return false;
    }
    
    JsonArray logs = doc["logs"];
    for (JsonObject entry : logs) {
        LogEntry logEntry(
            static_cast<LogLevel>(entry["level"].as<int>()),
            entry["component"].as<String>(),
            entry["message"].as<String>()
        );
        logEntry.timestamp = entry["timestamp"];
        log_buffer.push_back(logEntry);
    }
    
    return true;
}

void SystemMonitor::checkAlerts() {
    // Проверка памяти
    if (getMemoryUsagePercent() > 85.0f) {
        log(LOG_WARN, "MONITOR", "High memory usage: " + String(getMemoryUsagePercent(), 1) + "%");
    }
    
    // Проверка фрагментации кучи
    if (current_metrics.heap_fragmentation > 50.0f) {
        log(LOG_WARN, "MONITOR", "High heap fragmentation: " + String(current_metrics.heap_fragmentation, 1) + "%");
    }
    
    // Проверка WiFi сигнала
    if (current_metrics.wifi_signal_strength < -80 && WiFi.getMode() != WIFI_OFF) {
        log(LOG_WARN, "MONITOR", "Weak WiFi signal: " + String(current_metrics.wifi_signal_strength) + " dBm");
    }
}

bool SystemMonitor::isSystemHealthy() const {
    return getMemoryUsagePercent() < 90.0f && 
           current_metrics.heap_fragmentation < 60.0f &&
           current_metrics.free_heap > 10000;
}

std::vector<String> SystemMonitor::getActiveAlerts() const {
    std::vector<String> alerts;
    
    if (getMemoryUsagePercent() > 85.0f) {
        alerts.push_back("High memory usage");
    }
    
    if (current_metrics.heap_fragmentation > 50.0f) {
        alerts.push_back("High heap fragmentation");
    }
    
    if (current_metrics.wifi_signal_strength < -80 && WiFi.getMode() != WIFI_OFF) {
        alerts.push_back("Weak WiFi signal");
    }
    
    if (current_metrics.free_heap < 10000) {
        alerts.push_back("Low memory");
    }
    
    return alerts;
}

void SystemMonitor::cleanup() {
    // Очистка старых логов (старше 24 часов)
    unsigned long cutoff = millis() - (24 * 60 * 60 * 1000);
    
    auto it = log_buffer.begin();
    while (it != log_buffer.end()) {
        if (it->timestamp < cutoff) {
            it = log_buffer.erase(it);
        } else {
            ++it;
        }
    }
    
    // Очистка старой истории атак
    if (attack_history.size() > max_attack_history) {
        attack_history.erase(attack_history.begin(), 
                           attack_history.begin() + (attack_history.size() - max_attack_history));
    }
}

float SystemMonitor::getMemoryUsagePercent() const {
    if (current_metrics.total_heap == 0) return 0.0f;
    return 100.0f * (1.0f - (float)current_metrics.free_heap / (float)current_metrics.total_heap);
}

String SystemMonitor::formatUptime() const {
    unsigned long seconds = current_metrics.uptime_ms / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;
    
    String uptime = "";
    if (days > 0) uptime += String(days) + "d ";
    if (hours % 24 > 0) uptime += String(hours % 24) + "h ";
    if (minutes % 60 > 0) uptime += String(minutes % 60) + "m ";
    uptime += String(seconds % 60) + "s";
    
    return uptime;
}

// --- Приватные методы ---
void SystemMonitor::rotateLogBuffer() {
    if (log_buffer.size() > max_log_entries) {
        log_buffer.erase(log_buffer.begin(), log_buffer.begin() + (log_buffer.size() - max_log_entries));
    }
}

void SystemMonitor::rotateAttackHistory() {
    if (attack_history.size() > max_attack_history) {
        attack_history.erase(attack_history.begin(), 
                           attack_history.begin() + (attack_history.size() - max_attack_history));
    }
}

String SystemMonitor::logLevelToString(LogLevel level) const {
    switch (level) {
        case LOG_ERROR: return "ERROR";
        case LOG_WARN: return "WARN";
        case LOG_INFO: return "INFO";
        case LOG_DEBUG: return "DEBUG";
        default: return "UNKNOWN";
    }
}

String SystemMonitor::formatTimestamp(unsigned long timestamp) const {
    unsigned long seconds = timestamp / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%02lu:%02lu:%02lu", 
             hours % 24, minutes % 60, seconds % 60);
    return String(buffer);
}

void SystemMonitor::updateComponentCounter(const String& component) {
    component_counters[component]++;
}

void SystemMonitor::updateLevelCounter(LogLevel level) {
    level_counters[level]++;
}

// --- Реализация ReportGenerator ---
String ReportGenerator::generateDashboardHTML() const {
    SystemMetrics metrics = monitor->getMetrics();
    auto alerts = monitor->getActiveAlerts();

    String html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>System Dashboard</title>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
        .dashboard { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; }
        .card { background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .metric { display: flex; justify-content: space-between; margin: 10px 0; }
        .alert { background: #ffebee; color: #c62828; padding: 10px; border-radius: 4px; margin: 5px 0; }
        .success { background: #e8f5e8; color: #2e7d32; }
        .warning { background: #fff3e0; color: #f57c00; }
        .error { background: #ffebee; color: #c62828; }
        .progress { width: 100%; height: 20px; background: #eee; border-radius: 10px; overflow: hidden; }
        .progress-bar { height: 100%; background: #4caf50; transition: width 0.3s; }
        h2 { color: #333; border-bottom: 2px solid #ddd; padding-bottom: 10px; }
    </style>
</head>
<body>
    <h1>🛡️ System Dashboard</h1>
    <div class="dashboard">
        <div class="card">
            <h2>📊 System Metrics</h2>
            <div class="metric"><span>Uptime:</span><span>)" + monitor->formatUptime() + R"(</span></div>
            <div class="metric"><span>Free Memory:</span><span>)" + String(metrics.free_heap) + R"( bytes</span></div>
            <div class="metric"><span>Memory Usage:</span><span>)" + String(monitor->getMemoryUsagePercent(), 1) + R"(%</span></div>
            <div class="progress">
                <div class="progress-bar" style="width: )" + String(monitor->getMemoryUsagePercent()) + R"(%;"></div>
            </div>
            <div class="metric"><span>WiFi Signal:</span><span>)" + String(metrics.wifi_signal_strength) + R"( dBm</span></div>
        </div>

        <div class="card">
            <h2>⚡ Attack Statistics</h2>
            <div class="metric"><span>Attacks Performed:</span><span>)" + String(metrics.attacks_performed) + R"(</span></div>
            <div class="metric"><span>Credentials Captured:</span><span>)" + String(metrics.credentials_captured) + R"(</span></div>
            <div class="metric"><span>Clients Discovered:</span><span>)" + String(metrics.clients_discovered) + R"(</span></div>
            <div class="metric"><span>Packets Sent:</span><span>)" + String(metrics.wifi_packets_sent) + R"(</span></div>
        </div>

        <div class="card">
            <h2>🚨 System Health</h2>)";

    if (alerts.empty()) {
        html += R"(<div class="alert success">✅ All systems operational</div>)";
    } else {
        for (const auto& alert : alerts) {
            html += R"(<div class="alert error">⚠️ )" + alert + R"(</div>)";
        }
    }

    html += R"(
        </div>

        <div class="card">
            <h2>📝 Recent Activity</h2>)";

    auto recent_logs = monitor->getRecentLogs(5);
    for (const auto& log : recent_logs) {
        String level_class = getLogLevelBadge(log.level);
        html += R"(<div class="metric )" + level_class + R"(">
            <span>[)" + log.component + R"(]</span>
            <span>)" + log.message + R"(</span>
        </div>)";
    }

    html += R"(
        </div>
    </div>
    <script>
        setTimeout(() => location.reload(), 30000); // Auto-refresh every 30 seconds
    </script>
</body>
</html>)";

    return html;
}

String ReportGenerator::generateLogsHTML() const {
    auto logs = monitor->getRecentLogs(100);

    String html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>System Logs</title>
    <meta charset="UTF-8">
    <style>
        body { font-family: monospace; margin: 20px; background: #1e1e1e; color: #fff; }
        .log-entry { margin: 5px 0; padding: 5px; border-radius: 3px; }
        .error { background: rgba(244, 67, 54, 0.2); }
        .warn { background: rgba(255, 152, 0, 0.2); }
        .info { background: rgba(33, 150, 243, 0.2); }
        .debug { background: rgba(76, 175, 80, 0.2); }
        .timestamp { color: #888; }
        .component { color: #4fc3f7; font-weight: bold; }
        .message { color: #fff; }
    </style>
</head>
<body>
    <h1>📝 System Logs</h1>
    <div id="logs">)";

    for (const auto& log : logs) {
        String level_class = "";
        switch (log.level) {
            case LOG_ERROR: level_class = "error"; break;
            case LOG_WARN: level_class = "warn"; break;
            case LOG_INFO: level_class = "info"; break;
            case LOG_DEBUG: level_class = "debug"; break;
        }

        html += R"(<div class="log-entry )" + level_class + R"(">
            <span class="timestamp">)" + monitor->formatTimestamp(log.timestamp) + R"(</span>
            <span class="component">[)" + log.component + R"(]</span>
            <span class="message">)" + escapeHTML(log.message) + R"(</span>
        </div>)";
    }

    html += R"(
    </div>
    <script>
        document.getElementById('logs').scrollTop = document.getElementById('logs').scrollHeight;
    </script>
</body>
</html>)";

    return html;
}

String ReportGenerator::escapeHTML(const String& text) const {
    String escaped = text;
    escaped.replace("&", "&amp;");
    escaped.replace("<", "&lt;");
    escaped.replace(">", "&gt;");
    escaped.replace("\"", "&quot;");
    escaped.replace("'", "&#x27;");
    return escaped;
}

String ReportGenerator::getLogLevelBadge(LogLevel level) const {
    switch (level) {
        case LOG_ERROR: return "error";
        case LOG_WARN: return "warning";
        case LOG_INFO: return "success";
        case LOG_DEBUG: return "";
        default: return "";
    }
}
