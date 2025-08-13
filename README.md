# ESP32 Evil Twin v2.0 - Advanced WiFi Security Testing Framework

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
