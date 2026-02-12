# NimBLE-DataPipe

**NimBLE-DataPipe** is a lightweight BLE transport layer for the ESP32. It allows you to "pipe" both **JSON** and **Binary** data over a single BLE characteristic with zero effort regarding MTU limits or fragmentation.

## Why DataPipe?

- **Automatic Fragmentation**: Large payloads (up to 64KB) are split and reassembled transparently.
- **Bi-modal Support**: Built-in support for `ArduinoJson` objects and raw `uint8_t` buffers.
- **Zero-config**: Automatically detects the best MTU for your connection.

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
|--------------|-------------------------|--------|-----------------------------------|

## Web side (JavaScript / Web Bluetooth)

Using **NimBLE-DataPipe** from a browser is easy. Here is a minimal implementation to handle reassembly and fragmentation.

### Receiving Data (Reassembly)

```javascript
let rxBuffer = new Uint8Array(0);
let expectedType = 0;
let expectedLen = 0;
let headerReceived = false;

function onCharacteristicValueChanged(event) {
  const value = new Uint8Array(event.target.value.buffer);
  let offset = 0;

  if (!headerReceived) {
    if (value.length >= 3) {
      expectedType = value[0];
      expectedLen = value[1] | (value[2] << 8);
      headerReceived = true;
      rxBuffer = new Uint8Array(0);
      offset = 3;
    }
  }

  // Append data
  const chunk = value.slice(offset);
  const newBuffer = new Uint8Array(rxBuffer.length + chunk.length);
  newBuffer.set(rxBuffer);
  newBuffer.set(chunk, rxBuffer.length);
  rxBuffer = newBuffer;

  // Check if complete
  if (headerReceived && rxBuffer.length >= expectedLen) {
    if (expectedType === 0) { // JSON
      const jsonStr = new TextDecoder().decode(rxBuffer);
      const doc = JSON.parse(jsonStr);
      console.log("Received JSON:", doc);
    } else {
      console.log("Received Binary type", expectedType, rxBuffer);
    }
    headerReceived = false;
  }
}
```

### Sending Data (Fragmentation)

To send data, you must prefix it with the 3-byte header and split it into chunks matching the MTU.

```javascript
async function sendData(characteristic, type, payload) {
  // 1. Prepare header
  const header = new Uint8Array(3);
  header[0] = type;
  header[1] = payload.length & 0xFF;
  header[2] = (payload.length >> 8) & 0xFF;

  // 2. Combine Header + Payload
  const fullMessage = new Uint8Array(3 + payload.length);
  fullMessage.set(header);
  fullMessage.set(payload, 3);

  // 3. Send in chunks (e.g., 20 bytes for safety if MTU is unknown)
  const MTU = 20; 
  for (let i = 0; i < fullMessage.length; i += MTU) {
    const chunk = fullMessage.slice(i, i + MTU);
    await characteristic.writeValueWithResponse(chunk);
  }
}

// Usage for JSON
const myData = { cmd: "wifi_save", ssid: "MyHome", pass: "12345" };
const encoded = new TextEncoder().encode(JSON.stringify(myData));
await sendData(characteristic, 0, encoded);
```
