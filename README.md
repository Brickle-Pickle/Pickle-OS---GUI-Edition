# Pickle OS - GUI Edition

A full-featured graphical operating system for the ESP32-2432S028 development board with touchscreen interface, modular hardware expansion, and wireless connectivity.

**Successor to [Pickle OS (Terminal Edition)](https://github.com/Brickle-Pickle/Pickle-OS)** - evolved from CLI to full GUI with LVGL.

## Overview

Pickle OS GUI Edition transforms the compact ESP32-2432S028 into a portable mini-PC with:

- **Native touchscreen UI** powered by LVGL 8/9
- **WiFi connectivity** with OTA firmware updates
- **Bluetooth BLE** for external gamepad controller
- **Hot-swappable hardware modules** via CN1/P3 connectors (RFID, temperature/humidity sensors)
- **Local chat server** accessible from mobile devices on the same network
- **SD card storage** for apps, configuration, and user data
- **Built-in apps**: clock, notepad, settings, sensor dashboard, games, and more

This project serves as a professional portfolio piece demonstrating embedded systems architecture, real-time OS integration, hardware abstraction layers, and full-stack IoT development.

## Hardware Requirements

### Main Device
- **ESP32-2432S028** development board
  - ESP32-WROOM-32 module
  - 2.8" TFT LCD display (ST7789, 320x240 px)
  - Resistive touchscreen (XPT2046)
  - MicroSD card slot
  - USB-C programming interface

### Optional Hardware Modules

| Module | Function | Connection | Protocol |
|--------|----------|------------|----------|
| RFID/NFC Reader | Read/write Mifare tags | CN1 pins | SPI (RC522) |
| Temperature & Humidity | Environmental monitoring | P3 pins | GPIO/I2C (DHT22/SHT30) |
| External Gamepad | Wireless joystick + buttons | Bluetooth | BLE (second ESP32) |

### Pin Configuration

**CN1 Connector**: 5V, GND, IO35, IO22, IO21  
**P3 Connector**: 3.3V, GND, IO1, IO3

Note: SPI and I2C buses can be software-remapped. Recommended to design a 3D-printed adapter board for secure module mounting.

## Software Architecture

### Key Technologies

- **Framework**: PlatformIO (Arduino-ESP32)
- **RTOS**: FreeRTOS (native ESP32 support)
- **Graphics**: LVGL 8/9 with DMA-accelerated rendering
- **Networking**: ESP-IDF WiFi stack, AsyncWebServer, WebSockets, MQTT
- **Storage**: SD card (FAT32), SPIFFS for system files
- **Bluetooth**: ESP32 BLE GATT server/client

## Current Development Status

**Current Phase**: Pre-development  
**Latest Version**: Not yet released  
**Next Milestone**: Phase 1 - Foundation (Months 1-2)

## Development Roadmap

### Phase 1: Foundation (Months 1-2)
**Status**: In progress 
**Target**: Bootable touchscreen OS with basic navigation

**Infrastructure**
- [X] Configure PlatformIO + LVGL 8/9 development environment
- [X] Integrate ST7789_DRIVER display driver (correct pinout for ESP32-2432S028)
- [X] Calibrate XPT2046 touchscreen for accurate input
- [X] Implement double-buffering with FreeRTOS for 60 FPS rendering
- [X] Create window/screen manager with navigation stack

**User Interface**
- [X] Design app launcher with touch-friendly icon grid
- [X] Build on-screen keyboard (QWERTY + numeric layouts)
- [ ] Implement toast notification system
- [ ] Create custom visual theme (colors, fonts, icons)

**Milestone**: Device boots with fluid touchscreen UI. First demonstrable version for portfolio.

---

### Phase 2: Connectivity & Storage (Months 3-4)
**Status**: Not started  
**Target**: WiFi-enabled OS with OTA updates and persistent storage

**WiFi & Network**
- [ ] WiFi manager app (scan, connect, save credentials to SD)
- [ ] Over-the-air (OTA) firmware update system
- [ ] NTP client for automatic time synchronization
- [ ] HTTP/HTTPS client library integration

**Storage & Apps**
- [ ] SD card FAT32 file system support
- [ ] Settings app with persistent configuration
- [ ] Clock app (digital/analog with calendar view)
- [ ] Notepad app (text editor with SD save/load)

**Milestone**: OS connects to WiFi, supports OTA updates, and has functional SD storage with productivity apps.

---

### Phase 3: Server & Gamepad (Months 5-6)
**Status**: Not started  
**Target**: Local web server + wireless BLE gamepad support

**Web Server**
- [ ] Embedded HTTP server (serve SPA from SD card)
- [ ] WebSocket server for real-time communication
- [ ] Local chat application (multi-client on same network)
- [ ] Web-based control panel for OS monitoring

**BLE Gamepad**
- [ ] Custom BLE GATT protocol for gamepad events
- [ ] Virtual gamepad API abstraction layer
- [ ] Firmware for secondary ESP32 (joystick + 4 buttons)
- [ ] BLE pairing UI in settings

**Milestone**: Chat accessible from mobile browser + physical gamepad working via Bluetooth. High "wow factor" for demonstrations.

---

### Phase 4: Modular Hardware System (Months 7-8)
**Status**: Not started  
**Target**: Hot-swap module architecture with RFID and environmental sensors

**Module Architecture**
- [ ] Hardware abstraction layer (HAL) for modules
- [ ] Auto-detection system (identify connected module at boot)
- [ ] Dynamic app loading based on active modules

**RFID/NFC Module**
- [ ] RC522 driver (SPI on CN1 pins)
- [ ] RFID manager app (read/write tags, store history on SD)

**Temperature/Humidity Module**
- [ ] DHT22/SHT30 driver (GPIO/I2C on P3 pins)
- [ ] Environmental sensor app (real-time graphs, SD logging)

**Hardware Design**
- [ ] 3D-printable adapter board for CN1/P3 modules
- [ ] PCB design for RFID module breakout
- [ ] PCB design for sensor module breakout

**Milestone**: OS automatically detects and initializes hardware modules. RFID and temperature sensors fully operational.

---

### Phase 5: Advanced Applications (Months 9-10)
**Status**: Not started  
**Target**: IoT integration, games, and polished UX

**Data & IoT**
- [ ] Multi-sensor dashboard with live LVGL charts
- [ ] MQTT client (publish sensor data, subscribe to topics)
- [ ] RSS/news feed reader

**Games & Entertainment**
- [ ] Snake or Breakout game (touchscreen + gamepad support)
- [ ] High score tracking (stored on SD card)

**User Experience**
- [ ] Dark/light theme toggle
- [ ] Adaptive brightness control (optional photoresistor)
- [ ] Lock screen with PIN code entry
- [ ] Power saving mode with deep sleep

**Milestone**: Feature-complete system with useful apps, games, and professional polish.

---

## Quick Start

### Prerequisites

- [PlatformIO](https://platformio.org/) installed (CLI or VSCode extension)
- ESP32-2432S028 development board
- MicroSD card (optional but recommended)
- USB-C cable for programming

### Installation

```bash
# Clone the repository
git clone https://github.com/YOUR_USERNAME/Pickle-OS-GUI.git
cd Pickle-OS-GUI

# Build the firmware
pio run

# Upload to ESP32-2432S028
pio run --target upload

# Monitor serial output
pio device monitor
```

### OTA Update (WiFi)

Once WiFi is configured on the device:

```bash
# Upload firmware over the air
pio run --target upload --upload-port DEVICE_IP_ADDRESS
```

### Flash Pre-compiled Binary

Download the latest `.bin` file from [Releases](../../releases) and flash with esptool:

```bash
esptool.py --port /dev/ttyUSB0 write_flash 0x10000 pickle-os-gui-v1.0.0.bin
```

## Hardware Setup Notes

### Display driver configuration

The `User_Setup.h` file for TFT_eSPI must be copied manually into the library folder after installing dependencies: 

```bash
.pio/libdeps/esp32dev/TFT_eSPI/User_Setup.h
```

The reference file is located at `include/User_Setup.h` in this repository.
This board uses an ST7789 driver despite being documented as ILI9341.
Hardware reset is required on pin 4 before display initialization.

## Module Development

### Creating a New Hardware Module

1. Implement the `Module` interface in `src/hal/modules/`:

```cpp
class MyModule : public Module {
public:
    void init() override;
    void update() override;
    String getName() override { return "My Module"; }
    ModuleType getType() override { return MODULE_CUSTOM; }
};
```

2. Register the module in `src/hal/module_manager.cpp`
3. Create the corresponding app in `src/apps/mymodule/`
4. The OS will auto-detect and load it when connected

### Supported Module Interfaces

- **SPI**: High-speed peripherals (RFID, LoRa)
- **I2C**: Sensors (temperature, humidity, pressure)
- **GPIO**: Simple digital I/O (relays, buttons)
- **UART**: GPS modules, external MCUs

## WiFi Configuration

First boot will show WiFi setup wizard. Alternatively, create `wifi.json` on SD card:

```json
{
  "ssid": "YourNetworkName",
  "password": "YourPassword"
}
```

## BLE Gamepad Setup

Flash the gamepad firmware to a second ESP32:

```bash
cd gamepad-firmware/
pio run --target upload
```

Pair via **Settings > Bluetooth > Add Gamepad** on Pickle OS.

## Contributing

Contributions welcome. Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit changes with clear messages
4. Push to your fork
5. Open a Pull Request

See [CONTRIBUTING.md](CONTRIBUTING.md) for coding standards and conventions.

See [Screen Making Guide](docs/screen_making_tutorial.md) for best practices on designing LVGL screens.

## Known Limitations

- **RAM**: ESP32 has ~320 KB total. LVGL double-buffering is memory-intensive. Optimize buffer size vs FPS.
- **Pin Count**: CN1/P3 provide limited pins. SPI bus sharing requires careful CS pin management.
- **Touchscreen**: Resistive (XPT2046) has lower precision than capacitive. Calibration recommended.
- **SD Card**: SPI bus contention with display. Use proper CS toggling and respect timing.

## Technical Decisions

### Why LVGL?
- Industry-standard embedded GUI library
- Hardware-accelerated rendering on ESP32
- Rich widget set (charts, keyboards, animations)
- Active community and documentation

### Why FreeRTOS?
- Native ESP32 support with optimized scheduler
- Efficient task management for UI, network, and sensors
- Industry-proven real-time capabilities

### Why Modular HAL?
- Clean separation of concerns
- Easy to add new hardware without modifying core OS
- Supports hot-swapping modules at runtime
- Simplifies testing and debugging

## Performance Benchmarks

Target metrics (to be updated after Phase 1):

- **UI Framerate**: 60 FPS sustained with LVGL animations
- **Touch Latency**: < 50ms from touch to visual response
- **WiFi Reconnect**: < 3 seconds after signal loss
- **OTA Update**: Full firmware flash in < 60 seconds
- **Boot Time**: < 5 seconds from power-on to launcher

## License

This project is licensed under the MIT License - see [LICENSE](LICENSE) file for details.

## Acknowledgments

- Original **Pickle OS (Terminal)** project: [github.com/Brickle-Pickle/Pickle-OS](https://github.com/Brickle-Pickle/Pickle-OS)
- LVGL graphics library: [lvgl.io](https://lvgl.io)
- ESP32 community and Espressif documentation
- PlatformIO development platform

---

**Note**: This is a portfolio project demonstrating embedded systems expertise. Not intended for production use without further hardening and testing.

Building with determination, caffeine, and a touch of madness.