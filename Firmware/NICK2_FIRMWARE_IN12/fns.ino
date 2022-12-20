String macToStr(const uint8_t* mac) {
  String result;
  for (int i = 0; i < 6; ++i) {
    if (mac[i] < 0x10) result += "0";
    result += String(mac[i], HEX);
    if (i < 5)
      result += ':';
  }
  result.toUpperCase();
  return result;
}

String macLastThreeSegments(const uint8_t* mac) {
  String result;
  for (int i = 3; i < 6; ++i) {
    if (mac[i] < 0x10) result += "0";
    result += String(mac[i], HEX);
  }
  result.toUpperCase();
  return result;
}

// SPIFFS

bool readConfig() {
  File configFile = LittleFS.open("/config.json");
  if (!configFile) {
    Serial.println("[CONF] Failed to read config file... first run?");
    Serial.println("[CONF] Creating new file...");
    saveConfig();
    return false;
  }
  DeserializationError error = deserializeJson(json, configFile.readString());
  configFile.close();
  return true;
}

bool saveConfig() {
  File configFile = LittleFS.open("/config.json", FILE_WRITE);
  if (!configFile) {
    Serial.println("[CONF] Failed to open config file for writing");
    return false;
  }
  
  serializeJson(json, configFile);
  //serializeJson(json, Serial);
  configFile.close();
  return true;
}
