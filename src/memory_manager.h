#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <memory>

// --- Класс для управления памятью ---
class MemoryManager {
private:
    static MemoryManager* instance;
    
    // Статистика памяти
    size_t peak_heap_usage;
    size_t current_allocations;
    size_t total_allocations;
    size_t total_deallocations;
    
    // Пулы объектов для часто используемых типов
    std::vector<String*> string_pool;
    std::vector<uint8_t*> buffer_pool;
    
    // Динамические настройки (настраиваются автоматически)
    size_t max_string_pool_size;
    size_t max_buffer_pool_size;
    size_t buffer_size;
    size_t psram_threshold;
    
public:
    static MemoryManager* getInstance();
    
    // Инициализация и очистка
    bool init();
    void cleanup();

    // Динамическая конфигурация
    void configure(size_t string_pool_size, size_t buffer_pool_size, size_t buf_size);
    void applyHardwareOptimizations();
    
    // Управление строками
    String* acquireString();
    void releaseString(String* str);
    
    // Управление буферами
    uint8_t* acquireBuffer();
    void releaseBuffer(uint8_t* buffer);
    
    // Статистика
    size_t getFreeHeap() const;
    size_t getUsedHeap() const;
    size_t getPeakHeapUsage() const { return peak_heap_usage; }
    float getFragmentation() const;
    
    // Мониторинг
    void updateStats();
    void printStats() const;
    bool isMemoryHealthy() const;
    
    // Оптимизация
    void defragment();
    void forceGarbageCollection();
    
    // Утилиты
    void* alignedAlloc(size_t size, size_t alignment = 4);
    void alignedFree(void* ptr);

    // PSRAM support for ESP32-S3
    void* psramAlloc(size_t size);
    void psramFree(void* ptr);
    bool isPsramAvailable() const;
    size_t getPsramSize() const;
    size_t getFreePsram() const;
    
private:
    MemoryManager();
    ~MemoryManager();
    
    void initializePools();
    void cleanupPools();
    void trackAllocation(size_t size);
    void trackDeallocation(size_t size);
};

// --- Умные указатели для автоматического управления памятью ---
template<typename T>
class ManagedPtr {
private:
    T* ptr;
    bool auto_release;
    
public:
    explicit ManagedPtr(T* p = nullptr, bool auto_rel = true) 
        : ptr(p), auto_release(auto_rel) {}
    
    ~ManagedPtr() {
        if (auto_release && ptr) {
            delete ptr;
        }
    }
    
    // Запрет копирования
    ManagedPtr(const ManagedPtr&) = delete;
    ManagedPtr& operator=(const ManagedPtr&) = delete;
    
    // Move семантика
    ManagedPtr(ManagedPtr&& other) noexcept 
        : ptr(other.ptr), auto_release(other.auto_release) {
        other.ptr = nullptr;
        other.auto_release = false;
    }
    
    ManagedPtr& operator=(ManagedPtr&& other) noexcept {
        if (this != &other) {
            if (auto_release && ptr) {
                delete ptr;
            }
            ptr = other.ptr;
            auto_release = other.auto_release;
            other.ptr = nullptr;
            other.auto_release = false;
        }
        return *this;
    }
    
    T* get() const { return ptr; }
    T* operator->() const { return ptr; }
    T& operator*() const { return *ptr; }
    
    T* release() {
        auto_release = false;
        return ptr;
    }
    
    void reset(T* new_ptr = nullptr) {
        if (auto_release && ptr) {
            delete ptr;
        }
        ptr = new_ptr;
        auto_release = true;
    }
    
    explicit operator bool() const { return ptr != nullptr; }
};

// StringPool удален - используется встроенная система MemoryManager::acquireString/releaseString

// --- Кольцевой буфер для эффективного управления данными ---
template<typename T, size_t Size>
class CircularBuffer {
private:
    T buffer[Size];
    size_t head;
    size_t tail;
    size_t count;
    
public:
    CircularBuffer() : head(0), tail(0), count(0) {}
    
    bool push(const T& item) {
        if (count >= Size) {
            return false; // Буфер полон
        }
        
        buffer[tail] = item;
        tail = (tail + 1) % Size;
        count++;
        return true;
    }
    
    bool pop(T& item) {
        if (count == 0) {
            return false; // Буфер пуст
        }
        
        item = buffer[head];
        head = (head + 1) % Size;
        count--;
        return true;
    }
    
    bool peek(T& item) const {
        if (count == 0) {
            return false;
        }
        item = buffer[head];
        return true;
    }
    
    size_t size() const { return count; }
    size_t capacity() const { return Size; }
    bool empty() const { return count == 0; }
    bool full() const { return count >= Size; }
    
    void clear() {
        head = tail = count = 0;
    }
};

// --- Макросы для удобного использования ---
#define MANAGED_STRING() MemoryManager::getInstance()->acquireString()
#define RELEASE_STRING(str) MemoryManager::getInstance()->releaseString(str)
#define MANAGED_BUFFER() MemoryManager::getInstance()->acquireBuffer()
#define RELEASE_BUFFER(buf) MemoryManager::getInstance()->releaseBuffer(buf)

// --- Глобальные переменные ---
extern MemoryManager* memoryManager;

// --- Утилиты для профилирования памяти ---
class MemoryProfiler {
private:
    unsigned long start_time;
    size_t start_heap;
    String operation_name;
    
public:
    explicit MemoryProfiler(const String& name);
    ~MemoryProfiler();
    
    void checkpoint(const String& description = "");
};

#define PROFILE_MEMORY(name) MemoryProfiler _prof(name)

#endif // MEMORY_MANAGER_H
