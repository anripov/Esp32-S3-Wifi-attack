# ESP32 Evil Twin v2.0 - Advanced WiFi Security Testing Framework

[🇷🇺 Русская версия](#русская-версия) | [🇺🇸 English Version](#english-version)

---

## 🇺🇸 English Version

## 🛡️ Overview

ESP32 Evil Twin v2.0 is a comprehensive WiFi security testing framework designed for educational and authorized penetration testing purposes. This advanced version features a modular architecture, enhanced security, real-time monitoring, and optimized performance.

## ⚠️ Legal Disclaimer

**IMPORTANT**: This tool is intended for educational purposes and authorized security testing only. Users are responsible for complying with all applicable laws and regulations. Unauthorized use of this tool against networks you do not own or have explicit permission to test is illegal and unethical.

## 🚀 ESP32-S3 Support

**Optimized for ESP32-S3 DevKitC-1N16R8** with automatic hardware detection and dynamic configuration:
- **16MB Flash + 8MB PSRAM** support with automatic optimization
- **USB-C Native** programming at 921600 baud
- **Dynamic Configuration** - automatically detects hardware and applies optimal settings
- **Enhanced Performance** - up to 3x improvement on ESP32-S3 with PSRAM

## ✨ Features

### Core Functionality
- **Evil Twin Attack**: Create fake access points to capture credentials
- **Deauthentication Attacks**: Disconnect clients from target networks
- **Client Discovery**: Scan and identify connected devices
- **Real-time Monitoring**: Live system metrics and attack statistics
- **Credential Capture**: Secure logging of captured data

### Advanced Features
- **Modular Architecture**: Clean separation of concerns with dedicated managers
- **Memory Optimization**: Advanced memory management with object pooling
- **Security Hardening**: Input validation, buffer overflow protection, CRC verification
- **Performance Monitoring**: Real-time system health and performance metrics
- **Modern Web Interface**: Responsive design with real-time updates
- **Comprehensive Logging**: Multi-level logging with rotation and persistence

## 🏗️ Architecture

### Core Components

```
src/
├── main.cpp              # Main application entry point
├── config.h/cpp          # Configuration management
├── wifi_attack.h/cpp     # WiFi attack implementations
├── web_server.h/cpp      # Web interface management
├── monitoring.h/cpp      # System monitoring and logging
└── memory_manager.h/cpp  # Advanced memory management
```

### Data Flow
```
User Interface → Web Server → Attack Manager → WiFi Hardware
                     ↓
              System Monitor ← Memory Manager
```

## 🚀 Quick Start

### Prerequisites
- **ESP32-S3 DevKitC-1N16R8** (recommended) or ESP32 development board
- PlatformIO IDE
- USB-C cable for ESP32-S3 or USB cable for ESP32

### Quick Setup

```bash
# Install dependencies
pio lib install

# For ESP32-S3 (recommended)
pio run -e esp32s3 --target uploadfs
pio run -e esp32s3 --target upload

# For ESP32 (legacy)
pio run -e esp32dev --target uploadfs
pio run -e esp32dev --target upload
```

### Usage

1. **Connect to WiFi**: `EvilTwin_Config`
2. **Open browser**: `192.168.4.1`
3. **Configure and launch attack**

## 📄 License

This project is licensed under the MIT License.

---

**Remember**: Always use this tool responsibly and only on networks you own or have explicit permission to test.

---

## 🇷🇺 Русская версия

## 🛡️ Обзор

ESP32 Evil Twin v2.0 - это комплексная платформа для тестирования безопасности WiFi, предназначенная для образовательных целей и авторизованного пентестинга. Эта продвинутая версия включает модульную архитектуру, улучшенную безопасность, мониторинг в реальном времени и оптимизированную производительность.

## ⚠️ Правовая оговорка

**ВАЖНО**: Этот инструмент предназначен только для образовательных целей и авторизованного тестирования безопасности. Пользователи несут ответственность за соблюдение всех применимых законов и правил. Несанкционированное использование этого инструмента против сетей, которыми вы не владеете или не имеете явного разрешения на тестирование, является незаконным и неэтичным.

## 🚀 Поддержка ESP32-S3

**Оптимизировано для ESP32-S3 DevKitC-1N16R8** с автоматическим определением оборудования и динамической конфигурацией:
- **16MB Flash + 8MB PSRAM** поддержка с автоматической оптимизацией
- **USB-C Native** программирование на скорости 921600 бод
- **Динамическая конфигурация** - автоматически определяет оборудование и применяет оптимальные настройки
- **Улучшенная производительность** - до 3x улучшения на ESP32-S3 с PSRAM

## ✨ Возможности

### Основной функционал
- **Evil Twin атака**: Создание поддельных точек доступа для захвата учетных данных
- **Деаутентификационные атаки**: Отключение клиентов от целевых сетей
- **Обнаружение клиентов**: Сканирование и идентификация подключенных устройств
- **Мониторинг в реальном времени**: Живые системные метрики и статистика атак
- **Захват учетных данных**: Безопасное логирование захваченных данных

### Продвинутые возможности
- **Модульная архитектура**: Четкое разделение задач с выделенными менеджерами
- **Оптимизация памяти**: Продвинутое управление памятью с пулингом объектов
- **Усиление безопасности**: Валидация ввода, защита от переполнения буфера, CRC верификация
- **Мониторинг производительности**: Метрики состояния системы и производительности в реальном времени
- **Современный веб-интерфейс**: Адаптивный дизайн с обновлениями в реальном времени
- **Комплексное логирование**: Многоуровневое логирование с ротацией и сохранением

## 🏗️ Архитектура

### Основные компоненты

```
src/
├── main.cpp              # Главная точка входа приложения
├── config.h/cpp          # Управление конфигурацией
├── wifi_attack.h/cpp     # Реализация WiFi атак
├── web_server.h/cpp      # Управление веб-интерфейсом
├── monitoring.h/cpp      # Системный мониторинг и логирование
└── memory_manager.h/cpp  # Продвинутое управление памятью
```

### Поток данных
```
Пользовательский интерфейс → Веб-сервер → Менеджер атак → WiFi оборудование
                                ↓
                         Системный монитор ← Менеджер памяти
```

## 🚀 Быстрый старт

### Требования
- **ESP32-S3 DevKitC-1N16R8** (рекомендуется) или плата разработки ESP32
- PlatformIO IDE
- USB-C кабель для ESP32-S3 или USB кабель для ESP32

### Быстрая настройка

```bash
# Установка зависимостей
pio lib install

# Для ESP32-S3 (рекомендуется)
pio run -e esp32s3 --target uploadfs
pio run -e esp32s3 --target upload

# Для ESP32 (устаревший)
pio run -e esp32dev --target uploadfs
pio run -e esp32dev --target upload
```

### Использование

1. **Подключитесь к WiFi**: `EvilTwin_Config`
2. **Откройте браузер**: `192.168.4.1`
3. **Настройте и запустите атаку**

## 📄 Лицензия

Этот проект лицензирован под лицензией MIT.

---

**Помните**: Всегда используйте этот инструмент ответственно и только в сетях, которыми вы владеете или имеете явное разрешение на тестирование.
