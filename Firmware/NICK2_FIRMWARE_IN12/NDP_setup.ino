
void ndp_setup() {
  TimeChangeRule EDT2 = {"EDT", (uint8_t)json["dst_week"].as<int>(), (uint8_t)json["dst_day"].as<int>(), (uint8_t)json["dst_month"].as<int>(), (uint8_t)json["dst_hour"].as<int>(), (uint8_t)json["dst_offset"].as<int>()};
  TimeChangeRule EST2 = {"EST", (uint8_t)json["std_week"].as<int>(), (uint8_t)json["std_day"].as<int>(), (uint8_t)json["std_month"].as<int>(), (uint8_t)json["std_hour"].as<int>(), (uint8_t)json["std_offset"].as<int>()};

  if (json["dst_enable"].as<int>() == 1) {
    TZ.setRules(EDT2, EST2);
  } else {
    TZ.setRules(EST2, EST2);
  }

  Udp.begin(localPort);
  setSyncProvider(getNtpLocalTime);
  setSyncInterval(3600);
}
