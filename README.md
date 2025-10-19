# ESP32-P4 RTSP Camera Example

This Visual Studio Code project targets **ESP-IDF v6.0** and the **Waveshare Pico P4** (ESP32-P4) development board equipped with an **OV5647** CSI camera module. The firmware captures frames via the CSI peripheral, encodes them with the ESP32-P4 hardware H.264 engine, and publishes the bitstream over an RTSP-like TCP socket for experimentation.

## Project Layout

```
esp-idf-pico-p4/
├── CMakeLists.txt
├── sdkconfig.defaults
├── components/
│   ├── camera_driver/        # OV5647 CSI acquisition
│   ├── connectivity/         # Wi-Fi/Ethernet bring-up + RTSP socket
│   └── image_processing/     # Hardware H.264 encoding helpers
├── main/
│   ├── CMakeLists.txt
│   └── main_app.c            # Application entry point
└── .vscode/                  # VS Code + ESP-IDF extension configuration
```

Each component exposes a dedicated API so that camera acquisition, H.264 encoding, and networking remain decoupled.

## Getting Started

1. Install the [ESP-IDF v6.0 toolchain](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32p4/get-started/) and export the environment variables (`. ./export.sh`).
2. Open the `esp-idf-pico-p4` folder in VS Code and allow it to install the recommended extensions.
3. Use the command palette (`Ctrl+Shift+P`) to run **ESP-IDF: Select Port to Use** and **ESP-IDF: Select Device Target** (`esp32p4`).
4. Build, flash, and monitor the firmware via the provided tasks (e.g. `ESP-IDF: Build`).

## Notes

* The RTSP implementation is a lightweight placeholder that forwards H.264 packets on a TCP socket. Integrate a complete RTSP/RTP stack (e.g. Live555 or GStreamer RTSP server) for production use.
* Adjust the pin mapping inside `camera_driver_default_config()` to match your OV5647 ribbon wiring.
* Update Wi-Fi credentials in `connectivity_default_transport_config()` or override them at runtime.
