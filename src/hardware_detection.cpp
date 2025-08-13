#include "hardware_detection.h"
#include "memory_manager.h"
#include "config.h"

// Статические переменные
bool HardwareDetection::s3_detected = false;
bool HardwareDetection::psram_available = false;
size_t HardwareDetection::flash_size = 0;
size_t HardwareDetection::psram_size = 0;
uint32_t HardwareDetection::cpu_frequency = 0;
String HardwareDetection::board_model = "Unknown";

ConfigProfile AutoConfigurator::current_profile = PROFILE_AUTO;
HardwareCapabilities AutoConfigurator::capabilities;

bool HardwareDetection::detectHardware() {
    Serial.println("[HW] Starting hardware detection...");
    
    detectChipModel();
    detectMemoryConfiguration();
    detectWiFiCapabilities();
    
    return validateHardware();
}

void HardwareDetection::detectChipModel() {
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    cpu_frequency = getCpuFrequencyMhz();
    
    // Определяем модель чипа
    if (chip_info.model == CHIP_ESP32S3) {
        s3_detected = true;
        board_model = "ESP32-S3";
        Serial.printf("[HW] ESP32-S3 detected, %d cores, %d MHz\n", 
                     chip_info.cores, cpu_frequency);
    } else if (chip_info.model == CHIP_ESP32) {
        s3_detected = false;
        board_model = "ESP32";
        Serial.printf("[HW] ESP32 detected, %d cores, %d MHz\n", 
                     chip_info.cores, cpu_frequency);
    } else {
        board_model = "ESP32-Unknown";
        Serial.printf("[HW] Unknown ESP32 variant detected\n");
    }
}

void HardwareDetection::detectMemoryConfiguration() {
    // Flash память
    flash_size = ESP.getFlashChipSize();
    Serial.printf("[HW] Flash: %.2f MB\n", flash_size / (1024.0 * 1024.0));
    
    // PSRAM
    psram_available = psramFound();
    if (psram_available) {
        psram_size = ESP.getPsramSize();
        Serial.printf("[HW] PSRAM: %.2f MB (%.2f MB free)\n", 
                     psram_size / (1024.0 * 1024.0),
                     ESP.getFreePsram() / (1024.0 * 1024.0));
    } else {
        psram_size = 0;
        Serial.println("[HW] PSRAM: Not available");
    }
    
    // Общая память
    Serial.printf("[HW] Heap: %.2f KB total, %.2f KB free\n",
                 ESP.getHeapSize() / 1024.0,
                 ESP.getFreeHeap() / 1024.0);
}

void HardwareDetection::detectWiFiCapabilities() {
    WiFi.mode(WIFI_STA);
    delay(100);
    
    Serial.println("[HW] WiFi capabilities detected");
    // Дополнительные проверки WiFi можно добавить здесь
}

bool HardwareDetection::validateHardware() {
    bool valid = true;
    
    if (flash_size < 4 * 1024 * 1024) {
        Serial.println("[HW] WARNING: Flash size < 4MB may cause issues");
        valid = false;
    }
    
    if (cpu_frequency < 160) {
        Serial.println("[HW] WARNING: CPU frequency < 160MHz may affect performance");
    }
    
    return valid;
}

void HardwareDetection::printHardwareInfo() {
    Serial.println("\n=== Hardware Information ===");
    Serial.printf("Board: %s\n", board_model.c_str());
    Serial.printf("CPU: %d MHz\n", cpu_frequency);
    Serial.printf("Flash: %.2f MB\n", flash_size / (1024.0 * 1024.0));
    if (psram_available) {
        Serial.printf("PSRAM: %.2f MB\n", psram_size / (1024.0 * 1024.0));
    } else {
        Serial.println("PSRAM: Not available");
    }
    Serial.printf("Free Heap: %.2f KB\n", ESP.getFreeHeap() / 1024.0);
    Serial.println("============================\n");
}

void AutoConfigurator::autoDetectAndConfigure() {
    Serial.println("[CONFIG] Auto-detecting optimal configuration...");
    
    // Заполняем capabilities на основе обнаруженного оборудования
    capabilities.esp32s3 = HardwareDetection::isESP32S3();
    capabilities.psram_8mb = HardwareDetection::getPSRAMSize() >= 8 * 1024 * 1024;
    capabilities.flash_16mb = HardwareDetection::getFlashSize() >= 16 * 1024 * 1024;
    capabilities.dual_core = true; // Предполагаем dual-core
    capabilities.max_cpu_freq = HardwareDetection::getCPUFrequency();
    
    // Определяем оптимальный профиль
    if (capabilities.esp32s3 && capabilities.psram_8mb) {
        setProfile(PROFILE_PERFORMANCE);
        Serial.println("[CONFIG] High-performance profile selected (ESP32-S3 + 8MB PSRAM)");
    } else if (capabilities.esp32s3) {
        setProfile(PROFILE_BALANCED);
        Serial.println("[CONFIG] Balanced profile selected (ESP32-S3)");
    } else {
        setProfile(PROFILE_BALANCED);
        Serial.println("[CONFIG] Balanced profile selected (ESP32)");
    }
    
    applyProfile();

    // Применяем оптимизированные константы на основе обнаруженного оборудования
    applyHardwareOptimizedConstants(
        capabilities.esp32s3,
        capabilities.psram_8mb || HardwareDetection::isPSRAMAvailable(),
        HardwareDetection::getPSRAMSize()
    );
}

void AutoConfigurator::setProfile(ConfigProfile profile) {
    current_profile = profile;
}

void AutoConfigurator::applyProfile() {
    switch (current_profile) {
        case PROFILE_PERFORMANCE:
            applyPerformanceProfile();
            break;
        case PROFILE_BALANCED:
            applyBalancedProfile();
            break;
        case PROFILE_POWER_SAVE:
            applyPowerSaveProfile();
            break;
        case PROFILE_MINIMAL:
            applyMinimalProfile();
            break;
        case PROFILE_DEBUG:
            applyDebugProfile();
            break;
        default:
            applyBalancedProfile();
            break;
    }
}

void AutoConfigurator::applyPerformanceProfile() {
    Serial.println("[CONFIG] Applying performance profile...");
    
    // Настройки для максимальной производительности
    capabilities.max_clients = 100;
    capabilities.optimal_buffer_size = 2048;
    
    // Применяем настройки к MemoryManager
    // (это потребует модификации MemoryManager для поддержки динамических настроек)
}

void AutoConfigurator::applyBalancedProfile() {
    Serial.println("[CONFIG] Applying balanced profile...");
    
    capabilities.max_clients = capabilities.esp32s3 ? 75 : 50;
    capabilities.optimal_buffer_size = capabilities.esp32s3 ? 1536 : 1024;
}

void AutoConfigurator::applyPowerSaveProfile() {
    Serial.println("[CONFIG] Applying power-save profile...");
    
    capabilities.max_clients = 25;
    capabilities.optimal_buffer_size = 512;
}

void AutoConfigurator::applyMinimalProfile() {
    Serial.println("[CONFIG] Applying minimal profile...");
    
    capabilities.max_clients = 10;
    capabilities.optimal_buffer_size = 256;
}

void AutoConfigurator::applyDebugProfile() {
    Serial.println("[CONFIG] Applying debug profile...");
    
    capabilities.max_clients = 50;
    capabilities.optimal_buffer_size = 1024;
}

void AutoConfigurator::printConfiguration() {
    Serial.println("\n=== Applied Configuration ===");
    Serial.printf("Profile: %s\n", 
                 current_profile == PROFILE_PERFORMANCE ? "Performance" :
                 current_profile == PROFILE_BALANCED ? "Balanced" :
                 current_profile == PROFILE_POWER_SAVE ? "Power Save" :
                 current_profile == PROFILE_MINIMAL ? "Minimal" :
                 current_profile == PROFILE_DEBUG ? "Debug" : "Auto");
    Serial.printf("Max Clients: %d\n", capabilities.max_clients);
    Serial.printf("Buffer Size: %d bytes\n", capabilities.optimal_buffer_size);
    Serial.printf("PSRAM Usage: %s\n", capabilities.psram_8mb ? "Enabled" : "Disabled");
    Serial.println("=============================\n");
}

bool AutoConfigurator::validateConfiguration() {
    // Проверяем, что конфигурация применима к текущему оборудованию
    if (capabilities.max_clients > 100 && !capabilities.esp32s3) {
        Serial.println("[CONFIG] WARNING: High client count on non-S3 hardware");
        return false;
    }
    
    if (capabilities.optimal_buffer_size > 2048 && !capabilities.psram_8mb) {
        Serial.println("[CONFIG] WARNING: Large buffers without sufficient PSRAM");
        return false;
    }
    
    return true;
}

void HardwareDetection::recommendOptimizations() {
    Serial.println("\n=== Optimization Recommendations ===");

    if (s3_detected) {
        Serial.println("✓ ESP32-S3 detected - excellent performance capabilities");

        if (psram_available && psram_size >= 8 * 1024 * 1024) {
            Serial.println("✓ 8MB+ PSRAM available - enable high-performance mode");
            Serial.println("  → Recommendation: Use PROFILE_PERFORMANCE");
        } else if (psram_available) {
            Serial.println("✓ PSRAM available - enable enhanced mode");
            Serial.println("  → Recommendation: Use PROFILE_BALANCED with PSRAM optimizations");
        } else {
            Serial.println("⚠ No PSRAM detected - consider PSRAM upgrade for better performance");
            Serial.println("  → Recommendation: Use PROFILE_BALANCED");
        }

        if (cpu_frequency >= 240) {
            Serial.println("✓ CPU running at maximum frequency");
        } else {
            Serial.println("⚠ CPU not at maximum frequency - check power settings");
        }

    } else {
        Serial.println("ℹ ESP32 detected - standard performance");

        if (psram_available) {
            Serial.println("✓ PSRAM available - good for enhanced performance");
            Serial.println("  → Recommendation: Use PROFILE_BALANCED with PSRAM");
        } else {
            Serial.println("ℹ No PSRAM - using standard configuration");
            Serial.println("  → Recommendation: Use PROFILE_BALANCED or PROFILE_POWER_SAVE");
        }
    }

    if (flash_size >= 16 * 1024 * 1024) {
        Serial.println("✓ Large flash size - excellent for logging and web content");
    } else if (flash_size >= 8 * 1024 * 1024) {
        Serial.println("✓ Good flash size - sufficient for most operations");
    } else {
        Serial.println("⚠ Limited flash size - consider reducing log retention");
    }

    Serial.println("=====================================\n");
}

void HardwareDetection::printConfiguration() {
    Serial.println("\n=== Current Configuration ===");
    Serial.printf("Hardware: %s\n", board_model.c_str());
    Serial.printf("CPU: %d MHz\n", cpu_frequency);
    Serial.printf("Flash: %.2f MB\n", flash_size / (1024.0 * 1024.0));
    if (psram_available) {
        Serial.printf("PSRAM: %.2f MB\n", psram_size / (1024.0 * 1024.0));
    }
    Serial.printf("Profile: %s\n",
                 AutoConfigurator::current_profile == PROFILE_PERFORMANCE ? "Performance" :
                 AutoConfigurator::current_profile == PROFILE_BALANCED ? "Balanced" :
                 AutoConfigurator::current_profile == PROFILE_POWER_SAVE ? "Power Save" :
                 AutoConfigurator::current_profile == PROFILE_MINIMAL ? "Minimal" :
                 AutoConfigurator::current_profile == PROFILE_DEBUG ? "Debug" : "Auto");
    Serial.println("=============================\n");
}

// Глобальные экземпляры
HardwareDetection hardwareDetector;
AutoConfigurator autoConfig;
