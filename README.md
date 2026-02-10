# NimBLE-DataPipe

**NimBLE-DataPipe** is a lightweight BLE transport layer for the ESP32. It allows you to "pipe" both **JSON** and **Binary** data over a single BLE characteristic with zero effort regarding MTU limits or fragmentation.

## Why DataPipe?

- **Automatic Fragmentation**: Large payloads (up to 64KB) are split and reassembled transparently.
- **Bi-modal Support**: Built-in support for `ArduinoJson` objects and raw `uint8_t` buffers.
- **Zero-config**: Automatically detects the best MTU for your connection.

## Installation

### PlatformIO
```ini
lib_deps =
    h2zero/NimBLE-Arduino
    bblanchon/ArduinoJson
    NimBLE-DataPipe
```

## Protocol Header (3 bytes)
Each message is prefixed with a 3-byte technical header:
`[TYPE (1 byte)][LENGTH (2 bytes LE)]`

| Type | Name | Description |
|------|------|-------------|
| `0x00`      | **JSON**   | Structured document (ArduinoJson compatible) |
| `0x01-0xFF` | **Binary** | Custom application modes|

## Quick Start: WiFi & NTP Config (JSON)

This example shows how to build a complete configuration interface.

```cpp
#include <NimBLE_DataPipe.h>

NimBLE_DataPipe bleDataPipe("ESP32-Config-Demo", "SERVICE-UUID", "CHAR-UUID");

void setup() {
  Serial.begin(115200);

  bleDataPipe.setOnJson([](const JsonDocument &doc) {
    String cmd = doc["cmd"] | "";

    if (cmd == "wifi_save") {
      String ssid = doc["ssid"] | "";
      String pass = doc["pass"] | "";
      Serial.printf("Saving WiFi: %s\n", ssid.c_str());
      
      JsonDocument res;
      res["status"] = "ok";
      bleDataPipe.sendJson(res);
    } 
    else if (cmd == "get_info") {
      JsonDocument res;
      res["type"] = "device_info";
      res["version"] = "1.0.2";
      res["free_heap"] = ESP.getFreeHeap();
      bleDataPipe.sendJson(res);
    }
  });

  bleDataPipe.begin();
}
```

## Quick Start (Binary)

```cpp
bleDataPipe.setOnBinary([](uint8_t type, const uint8_t *data, size_t len) {
  if (type == 0x01) { 
     // Handle your custom binary mode
  }
});

// Sending raw binary
uint8_t myBuffer[128] = { ... };
bleDataPipe.sendBinary(0x01, myBuffer, 128);
```


## Logging

By default, the library prints status messages to `Serial`. You can silence all output by defining `DATAPIPE_SILENT` **before** including the library:

```cpp
#define DATAPIPE_SILENT
#include <NimBLE_DataPipe.h>
```

## Reliability (Notify vs Indicate)

By default, **DataPipe** uses **Notifications** for maximum speed. You can switch to **Indications** for 100% reliable delivery (GATT ACKs):

```cpp
// Before calling begin()
bleDataPipe.setUseIndication(true); 
bleDataPipe.begin();
```

| Mode | Reliability | Speed | Use Case |
|------|-------------|-------|----------|
| **Notify** (Default) | High (Link Layer ACKs) | Fast | Real-time streams, Small payloads |
| **Indicate** | Total (GATT Layer ACKs) | Slower | Critical configs, Firmware updates |
