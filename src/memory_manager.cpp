#include "memory_manager.h"
#include "config.h"

// --- Глобальные переменные ---
MemoryManager* MemoryManager::instance = nullptr;
MemoryManager* memoryManager = nullptr;
StringPool globalStringPool(30);

// --- Реализация MemoryManager ---
MemoryManager* MemoryManager::getInstance() {
    if (instance == nullptr) {
        instance = new MemoryManager();
    }
    return instance;
}

MemoryManager::MemoryManager()
    : peak_heap_usage(0), current_allocations(0),
      total_allocations(0), total_deallocations(0),
      max_string_pool_size(50), max_buffer_pool_size(20),
      buffer_size(1024), psram_threshold(512 * 1024) {
    memoryManager = this;
}

MemoryManager::~MemoryManager() {
    cleanup();
}

bool MemoryManager::init() {
    logMessage(LOG_INFO, "Initializing MemoryManager");
    
    // Инициализация пулов
    initializePools();
    
    // Начальная статистика
    updateStats();
    
    logMessage(LOG_INFO, "MemoryManager initialized. Free heap: %d bytes", getFreeHeap());
    return true;
}

void MemoryManager::cleanup() {
    cleanupPools();
    
    if (instance == this) {
        instance = nullptr;
    }
}

String* MemoryManager::acquireString() {
    if (!string_pool.empty()) {
        String* str = string_pool.back();
        string_pool.pop_back();
        str->clear(); // Очищаем содержимое
        return str;
    }
    
    // Создаем новую строку если пул пуст
    String* str = new String();
    trackAllocation(sizeof(String));
    return str;
}

void MemoryManager::releaseString(String* str) {
    if (!str) return;
    
    if (string_pool.size() < MAX_STRING_POOL_SIZE) {
        str->clear(); // Очищаем содержимое
        string_pool.push_back(str);
    } else {
        delete str;
        trackDeallocation(sizeof(String));
    }
}

uint8_t* MemoryManager::acquireBuffer() {
    if (!buffer_pool.empty()) {
        uint8_t* buffer = buffer_pool.back();
        buffer_pool.pop_back();
        return buffer;
    }
    
    // Создаем новый буфер если пул пуст
    uint8_t* buffer = new uint8_t[BUFFER_SIZE];
    trackAllocation(BUFFER_SIZE);
    return buffer;
}

void MemoryManager::releaseBuffer(uint8_t* buffer) {
    if (!buffer) return;
    
    if (buffer_pool.size() < MAX_BUFFER_POOL_SIZE) {
        buffer_pool.push_back(buffer);
    } else {
        delete[] buffer;
        trackDeallocation(BUFFER_SIZE);
    }
}

size_t MemoryManager::getFreeHeap() const {
    return ESP.getFreeHeap();
}

size_t MemoryManager::getUsedHeap() const {
    return ESP.getHeapSize() - ESP.getFreeHeap();
}

float MemoryManager::getFragmentation() const {
    size_t free_heap = getFreeHeap();
    if (free_heap == 0) return 100.0f;
    
    size_t max_alloc = ESP.getMaxAllocHeap();
    return 100.0f * (1.0f - (float)max_alloc / (float)free_heap);
}

void MemoryManager::updateStats() {
    size_t current_used = getUsedHeap();
    if (current_used > peak_heap_usage) {
        peak_heap_usage = current_used;
    }
}

void MemoryManager::printStats() const {
    logMessage(LOG_INFO, "=== Memory Statistics ===");
    logMessage(LOG_INFO, "Free Heap: %d bytes", getFreeHeap());
    logMessage(LOG_INFO, "Used Heap: %d bytes", getUsedHeap());
    logMessage(LOG_INFO, "Peak Usage: %d bytes", peak_heap_usage);
    logMessage(LOG_INFO, "Fragmentation: %.1f%%", getFragmentation());
    logMessage(LOG_INFO, "String Pool: %d/%d", string_pool.size(), MAX_STRING_POOL_SIZE);
    logMessage(LOG_INFO, "Buffer Pool: %d/%d", buffer_pool.size(), MAX_BUFFER_POOL_SIZE);
    logMessage(LOG_INFO, "Total Allocations: %d", total_allocations);
    logMessage(LOG_INFO, "Total Deallocations: %d", total_deallocations);
}

bool MemoryManager::isMemoryHealthy() const {
    return getFreeHeap() > 10000 && getFragmentation() < 50.0f;
}

void MemoryManager::defragment() {
    // Принудительная дефрагментация через временное выделение памяти
    std::vector<void*> temp_allocs;
    
    // Выделяем небольшие блоки для принуждения к дефрагментации
    for (int i = 0; i < 10; i++) {
        void* ptr = malloc(1024);
        if (ptr) {
            temp_allocs.push_back(ptr);
        }
    }
    
    // Освобождаем временные блоки
    for (void* ptr : temp_allocs) {
        free(ptr);
    }
    
    logMessage(LOG_INFO, "Memory defragmentation completed");
}

void MemoryManager::forceGarbageCollection() {
    // Очистка пулов
    cleanupPools();
    initializePools();
    
    // Принудительная дефрагментация
    defragment();
    
    logMessage(LOG_INFO, "Garbage collection completed");
}

void* MemoryManager::alignedAlloc(size_t size, size_t alignment) {
    void* ptr = nullptr;
    
    // Выравнивание памяти для лучшей производительности
    size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);
    ptr = malloc(aligned_size);
    
    if (ptr) {
        trackAllocation(aligned_size);
    }
    
    return ptr;
}

void MemoryManager::alignedFree(void* ptr) {
    if (ptr) {
        free(ptr);
        // Размер неизвестен, поэтому не отслеживаем деаллокацию
    }
}

void MemoryManager::initializePools() {
    // Предварительное выделение строк в пуле
    string_pool.reserve(MAX_STRING_POOL_SIZE);
    for (size_t i = 0; i < MAX_STRING_POOL_SIZE / 2; i++) {
        string_pool.push_back(new String());
    }
    
    // Предварительное выделение буферов в пуле
    buffer_pool.reserve(MAX_BUFFER_POOL_SIZE);
    for (size_t i = 0; i < MAX_BUFFER_POOL_SIZE / 2; i++) {
        buffer_pool.push_back(new uint8_t[BUFFER_SIZE]);
    }
    
    logMessage(LOG_DEBUG, "Memory pools initialized");
}

void MemoryManager::cleanupPools() {
    // Очистка пула строк
    for (String* str : string_pool) {
        delete str;
    }
    string_pool.clear();
    
    // Очистка пула буферов
    for (uint8_t* buffer : buffer_pool) {
        delete[] buffer;
    }
    buffer_pool.clear();
    
    logMessage(LOG_DEBUG, "Memory pools cleaned up");
}

void MemoryManager::trackAllocation(size_t size) {
    current_allocations += size;
    total_allocations++;
}

void MemoryManager::trackDeallocation(size_t size) {
    if (current_allocations >= size) {
        current_allocations -= size;
    }
    total_deallocations++;
}

// --- Реализация StringPool ---
StringPool::StringPool(size_t size) : pool_size(size) {
    pool.reserve(size);
    in_use.reserve(size);
    
    for (size_t i = 0; i < size; i++) {
        pool.emplace_back();
        in_use.push_back(false);
    }
}

StringPool::~StringPool() {
    clear();
}

String* StringPool::acquire() {
    for (size_t i = 0; i < pool_size; i++) {
        if (!in_use[i]) {
            in_use[i] = true;
            pool[i].clear();
            return &pool[i];
        }
    }
    
    // Пул полон, возвращаем новую строку
    return new String();
}

void StringPool::release(String* str) {
    if (!str) return;
    
    // Проверяем, принадлежит ли строка пулу
    for (size_t i = 0; i < pool_size; i++) {
        if (&pool[i] == str) {
            in_use[i] = false;
            str->clear();
            return;
        }
    }
    
    // Строка не из пула, удаляем её
    delete str;
}

void StringPool::clear() {
    for (size_t i = 0; i < pool_size; i++) {
        in_use[i] = false;
        pool[i].clear();
    }
}

size_t StringPool::getUsageCount() const {
    size_t count = 0;
    for (bool used : in_use) {
        if (used) count++;
    }
    return count;
}

float StringPool::getUsagePercent() const {
    return 100.0f * getUsageCount() / pool_size;
}

// --- Реализация MemoryProfiler ---
MemoryProfiler::MemoryProfiler(const String& name) 
    : operation_name(name), start_time(millis()), start_heap(ESP.getFreeHeap()) {
    logMessage(LOG_DEBUG, "Memory profiling started: %s (Free: %d bytes)", 
               name.c_str(), start_heap);
}

MemoryProfiler::~MemoryProfiler() {
    unsigned long duration = millis() - start_time;
    size_t end_heap = ESP.getFreeHeap();
    int heap_diff = (int)end_heap - (int)start_heap;
    
    logMessage(LOG_DEBUG, "Memory profiling completed: %s (Duration: %lu ms, Heap change: %+d bytes)", 
               operation_name.c_str(), duration, heap_diff);
}

void MemoryProfiler::checkpoint(const String& description) {
    unsigned long current_time = millis();
    size_t current_heap = ESP.getFreeHeap();
    int heap_diff = (int)current_heap - (int)start_heap;

    logMessage(LOG_DEBUG, "Memory checkpoint [%s]: %s (Elapsed: %lu ms, Heap change: %+d bytes)",
               operation_name.c_str(), description.c_str(),
               current_time - start_time, heap_diff);
}

// --- Методы динамической конфигурации ---
void MemoryManager::configure(size_t string_pool_size, size_t buffer_pool_size, size_t buf_size) {
    max_string_pool_size = string_pool_size;
    max_buffer_pool_size = buffer_pool_size;
    buffer_size = buf_size;

    Serial.printf("[MEMORY] Configuration updated: strings=%d, buffers=%d, buffer_size=%d\n",
                 max_string_pool_size, max_buffer_pool_size, buffer_size);

    // Пересоздаем пулы с новыми размерами
    cleanupPools();
    initializePools();
}

void MemoryManager::applyHardwareOptimizations() {
    Serial.println("[MEMORY] Applying hardware-specific optimizations...");

    // Определяем оптимальные настройки на основе доступной памяти
    size_t free_heap = ESP.getFreeHeap();
    bool has_psram = psramFound();
    size_t psram_size = has_psram ? ESP.getPsramSize() : 0;

    if (has_psram && psram_size >= 8 * 1024 * 1024) {
        // ESP32-S3 с 8MB+ PSRAM
        configure(100, 50, 2048);
        psram_threshold = 1024 * 1024; // 1MB
        Serial.println("[MEMORY] High-performance configuration applied (8MB+ PSRAM)");
    } else if (has_psram && psram_size >= 4 * 1024 * 1024) {
        // ESP32-S3 с 4MB+ PSRAM
        configure(75, 35, 1536);
        psram_threshold = 512 * 1024; // 512KB
        Serial.println("[MEMORY] Enhanced configuration applied (4MB+ PSRAM)");
    } else if (has_psram) {
        // Любой ESP32 с PSRAM
        configure(60, 25, 1024);
        psram_threshold = 256 * 1024; // 256KB
        Serial.println("[MEMORY] PSRAM-optimized configuration applied");
    } else if (free_heap > 200 * 1024) {
        // ESP32 с большим количеством heap
        configure(50, 20, 1024);
        Serial.println("[MEMORY] Standard configuration applied (large heap)");
    } else {
        // ESP32 с ограниченной памятью
        configure(25, 10, 512);
        Serial.println("[MEMORY] Minimal configuration applied (limited memory)");
    }

    Serial.printf("[MEMORY] PSRAM threshold: %d KB\n", psram_threshold / 1024);
}

// PSRAM support methods
void* MemoryManager::psramAlloc(size_t size) {
    if (!psramFound()) {
        return nullptr;
    }

    void* ptr = ps_malloc(size);
    if (ptr) {
        trackAllocation(size);
        Serial.printf("[MEMORY] PSRAM allocated: %d bytes\n", size);
    }
    return ptr;
}

void MemoryManager::psramFree(void* ptr) {
    if (ptr && psramFound()) {
        free(ptr);
        // Размер не отслеживается для PSRAM, но это не критично
        Serial.println("[MEMORY] PSRAM freed");
    }
}

bool MemoryManager::isPsramAvailable() const {
    return psramFound();
}

size_t MemoryManager::getPsramSize() const {
    return psramFound() ? ESP.getPsramSize() : 0;
}

size_t MemoryManager::getFreePsram() const {
    return psramFound() ? ESP.getFreePsram() : 0;
}
