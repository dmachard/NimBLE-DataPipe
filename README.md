# NimBLE-DataPipe

**NimBLE-DataPipe** is a lightweight BLE transport layer for the ESP32. It allows you to "pipe" both **JSON** and **Binary** data over a single BLE characteristic with zero effort regarding MTU limits or fragmentation.

## Why DataPipe?

- **Automatic Fragmentation**: Large payloads (up to 64KB) are split and reassembled transparently.
- **Bi-modal Support**: Built-in support for `ArduinoJson` objects and raw `uint8_t` buffers.
- **Zero-config**: Automatically detects the best MTU for your connection.
- **Reliable Delivery**: Uses BLE Indications (GATT-level ACKs) for guaranteed delivery.

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

## Quick Start: ESP32 Side (C++)

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

## Quick Start: Web Side (JavaScript)

Use the [Web Bluetooth API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Bluetooth_API) to communicate with DataPipe from a browser.

```javascript
const SERVICE_UUID = "your-service-uuid";
const CHAR_UUID    = "your-char-uuid";

let device, characteristic;

// --- Connect ---
async function connect() {
  device = await navigator.bluetooth.requestDevice({
    filters: [{ services: [SERVICE_UUID] }]
  });
  const server = await device.gatt.connect();
  const service = await server.getPrimaryService(SERVICE_UUID);
  characteristic = await service.getCharacteristic(CHAR_UUID);

  // Listen for indications from the ESP32
  await characteristic.startNotifications();
  characteristic.addEventListener("characteristicvaluechanged", onReceive);
  console.log("Connected");
}

// --- Receive (chunk reassembly) ---
let rxBuffer = new Uint8Array(0);
let expectedLen = 0;
let expectedType = 0;
let headerReceived = false;

function onReceive(event) {
  const chunk = new Uint8Array(event.target.value.buffer);

  // Append chunk to buffer
  const tmp = new Uint8Array(rxBuffer.length + chunk.length);
  tmp.set(rxBuffer);
  tmp.set(chunk, rxBuffer.length);
  rxBuffer = tmp;

  // Parse header once we have 3 bytes
  if (!headerReceived && rxBuffer.length >= 3) {
    expectedType = rxBuffer[0];
    expectedLen = rxBuffer[1] | (rxBuffer[2] << 8);
    rxBuffer = rxBuffer.slice(3);
    headerReceived = true;
  }

  // Complete message?
  if (headerReceived && rxBuffer.length >= expectedLen) {
    const payload = rxBuffer.slice(0, expectedLen);

    if (expectedType === 0x00) {
      const json = JSON.parse(new TextDecoder().decode(payload));
      console.log("Received JSON:", json);
    } else {
      console.log(`Received Binary: type=${expectedType}, ${payload.length} bytes`);
    }

    // Reset for next message
    rxBuffer = new Uint8Array(0);
    headerReceived = false;
  }
}

// --- Send JSON ---
async function sendJson(obj) {
  const text = JSON.stringify(obj);
  const payload = new TextEncoder().encode(text);
  const len = payload.length;

  // Header: [TYPE=0x00][LEN_LO][LEN_HI] + payload
  const buffer = new Uint8Array(3 + len);
  buffer[0] = 0x00;
  buffer[1] = len & 0xFF;
  buffer[2] = (len >> 8) & 0xFF;
  buffer.set(payload, 3);

  await characteristic.writeValueWithResponse(buffer);
}

// --- Usage ---
await connect();
await sendJson({ cmd: "get_info" });
```

## Binary Mode

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
