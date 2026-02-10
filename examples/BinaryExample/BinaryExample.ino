/**
 * @file BinaryExample.ino
 * @brief Raw binary protocol example using NimBLE-DataPipe.
 *
 * Demonstrates how to handle different binary modes synchronously
 * without any project-specific dependencies.
 */

#include <NimBLE-DataPipe.h>

NimBLE_DataPipe pipe("Generic-Data-Pipe", "4101", "3319");

// Example of a binary header for a custom protocol
struct MyHeader {
  uint8_t mode;
  uint8_t param1;
  uint8_t param2;
  uint16_t dataSize;
} __attribute__((packed));

void setup() {
  Serial.begin(115200);

  // Raw binary handler
  pipe.setOnBinary([](uint8_t type, const uint8_t *data, size_t len) {
    Serial.printf("[Pipe] Received Type: %d, Length: %d\n", type, len);

    if (type == 0x01) { // Example Mode 1
      if (len >= sizeof(MyHeader)) {
        MyHeader *h = (MyHeader *)data;
        Serial.printf("Processing mode 1: Param1=%d, Param2=%d\n", h->param1,
                      h->param2);
      }
    } else if (type == 0x02) { // Example Mode 2
      Serial.println("Processing raw data stream...");
    }
  });

  pipe.begin();
}

void loop() { delay(1000); }
