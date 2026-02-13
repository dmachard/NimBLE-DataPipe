#include "NimBLE-DataPipe.h"

class DataPipeServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override {
    DATAPIPE_LOG("[NimBLE-DataPipe] Client Connected");
  };
  void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo,
                    int reason) override {
    DATAPIPE_LOG("[NimBLE-DataPipe] Client Disconnected: %d", reason);
    NimBLEDevice::startAdvertising();
  }
};

NimBLE_DataPipe::NimBLE_DataPipe(const char *deviceName,
                                 const char *serviceUuid, const char *charUuid)
    : _deviceName(deviceName), _serviceUuid(serviceUuid), _charUuid(charUuid) {
  _rxBuffer.reserve(2048); // Initial capacity
}

void NimBLE_DataPipe::begin() {
  NimBLEDevice::init(_deviceName);
  NimBLEDevice::setMTU(517); // Allow large attribute values (up to 512 payload)
  _pServer = NimBLEDevice::createServer();
  _pServer->setCallbacks(new DataPipeServerCallbacks());

  uint32_t properties = NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE |
                        NIMBLE_PROPERTY::INDICATE;

  NimBLEService *pService = _pServer->createService(_serviceUuid);
  _pChar = pService->createCharacteristic(_charUuid, properties, 514);

  _pChar->setCallbacks(this);
  pService->start();

  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(_serviceUuid);
  pAdvertising->enableScanResponse(true);
  pAdvertising->start();

  DATAPIPE_LOG("[NimBLE-DataPipe] Initialized (Indicate mode)");
}

void NimBLE_DataPipe::stop() {
  NimBLEDevice::deinit(true);
  DATAPIPE_LOG("[NimBLE-DataPipe] Stopped");
}

bool NimBLE_DataPipe::isConnected() {
  return _pServer && _pServer->getConnectedCount() > 0;
}

uint16_t NimBLE_DataPipe::getMTU() {
  if (!_pServer || !isConnected())
    return 23;

  auto peers = _pServer->getPeerDevices();
  if (peers.empty())
    return 23;

  return _pServer->getPeerMTU(peers[0]);
}

void NimBLE_DataPipe::sendJson(const JsonDocument &doc) {
  if (!isConnected())
    return;
  String payload;
  serializeJson(doc, payload);
  sendInternal(TYPE_JSON, (const uint8_t *)payload.c_str(), payload.length());
}

void NimBLE_DataPipe::sendBinary(uint8_t type, const uint8_t *data,
                                 size_t len) {
  sendInternal(type, data, len);
}

void NimBLE_DataPipe::sendInternal(uint8_t type, const uint8_t *payload,
                                   size_t len) {
  if (!_pChar || !isConnected())
    return;

  uint16_t rawMTU = getMTU();
  if (rawMTU < 5)
    return;
  size_t maxPayload = rawMTU - 4; // MTU - ATT overhead (3) - 1
  size_t totalLen = len + 3;      // Payload + Header (Type + LenL + LenH)

  uint8_t *buffer = new uint8_t[totalLen];
  buffer[0] = type;
  buffer[1] = len & 0xFF;
  buffer[2] = (len >> 8) & 0xFF;
  memcpy(buffer + 3, payload, len);

  DATAPIPE_LOG("[DP-TX] Sending: Type=%d, PayloadLen=%d, TotalLen=%d, MTU=%d",
               type, len, totalLen, rawMTU);

  if (totalLen <= maxPayload) {
    _pChar->indicate(buffer, totalLen);
  } else {
    // Large message: chunk and send with delay between each
    for (size_t i = 0; i < totalLen; i += maxPayload) {
      size_t chunkSize = min(maxPayload, totalLen - i);
      if (!isConnected())
        break;

      DATAPIPE_LOG("[DP-TX] Chunk: offset=%d, size=%d", i, chunkSize);

      bool ok = _pChar->indicate(buffer + i, chunkSize);
      if (!ok)
        break;
    }
  }

  delete[] buffer;
  DATAPIPE_LOG("[DP-TX] ✅ Complete (%d bytes)", totalLen);
}

void NimBLE_DataPipe::onWrite(NimBLECharacteristic *pCharacteristic,
                              NimBLEConnInfo &connInfo) {
  NimBLEAttValue val = pCharacteristic->getValue();
  const uint8_t *data = val.data();
  size_t len = val.length();

  if (len == 0)
    return;

  // 1. Handle Header
  if (!_headerReceived) {
    _rxBuffer.insert(_rxBuffer.end(), data, data + len);
    if (_rxBuffer.size() >= 3) {
      _expectedType = _rxBuffer[0];
      _expectedLen = _rxBuffer[1] | (_rxBuffer[2] << 8);
      _rxBuffer.erase(_rxBuffer.begin(), _rxBuffer.begin() + 3);
      _headerReceived = true;
    } else {
      return; // Need more bytes for header
    }
  } else {
    // Accumulate payload data
    _rxBuffer.insert(_rxBuffer.end(), data, data + len);
  }

  // 2. Check if complete — call handler directly
  if (_rxBuffer.size() >= _expectedLen && _headerReceived) {
    if (_expectedType == TYPE_JSON && _jsonHandler) {
      JsonDocument doc;
      DeserializationError error =
          deserializeJson(doc, _rxBuffer.data(), _rxBuffer.size());
      if (!error)
        _jsonHandler(doc);
      else
        DATAPIPE_LOG("[NimBLE-DataPipe] JSON Error: %s", error.c_str());
    } else if (_binaryHandler) {
      _binaryHandler(_expectedType, _rxBuffer.data(), _rxBuffer.size());
    }

    _headerReceived = false;
    _rxBuffer.clear();
  }
}
