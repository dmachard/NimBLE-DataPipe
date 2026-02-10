/**
 * @file SelfTest.ino
 * @brief Simple self-test for NimBLE-DataPipe.
 *
 * This sketch demonstrates how to verify the library functionality.
 * You can use a BLE scanner (like nRF Connect) to write values to the
 * characteristic and see them reassembled and parsed in the Serial Monitor.
 */

#include <NimBLE-DataPipe.h>

NimBLE_DataPipe pipe("DataPipe-Test", "BEEF", "CAFE");

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("--- NimBLE-DataPipe Self Test ---");

  // 1. Test JSON handling
  pipe.setOnJson([](const JsonDocument &doc) {
    Serial.println("[Test] Received JSON:");
    serializeJsonPretty(doc, Serial);
    Serial.println();

    // Echo back a response
    JsonDocument response;
    response["status"] = "received";
    response["echo"] = doc["cmd"] | "unknown";
    pipe.sendJson(response);
  });

  // 2. Test Binary handling
  pipe.setOnBinary([](uint8_t type, const uint8_t *data, size_t len) {
    Serial.printf("[Test] Received Binary Type: 0x%02X, Len: %d\n", type, len);
    // Simple verification check
    bool valid = true;
    for (size_t i = 0; i < len; i++) {
      if (data[i] != (uint8_t)i) {
        valid = false;
        break;
      }
    }
    Serial.println(valid ? "Data Integrity: PASS" : "Data Integrity: FAIL");
  });

  pipe.begin();
  Serial.println("Advertising started. Connect with a BLE tool to test.");
}

void loop() {
  // Every 10 seconds, print MTU if connected
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck > 10000) {
    lastCheck = millis();
    if (pipe.isConnected()) {
      Serial.printf("Current MTU: %d\n", pipe.getMTU());
    }
  }
  delay(100);
}
