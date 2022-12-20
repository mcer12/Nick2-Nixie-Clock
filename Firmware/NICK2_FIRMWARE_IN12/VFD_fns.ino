

void initScreen() {

  for (int i = 0; i < sizeof(anodes_pins) / sizeof(anodes_pins[0]); i++) {
    pinMode(anodes_pins[i], OUTPUT);
    digitalWrite(anodes_pins[i], LOW);    // turn the LED off by making the voltage LOW
  }

  for (int i = 0; i < sizeof(SN7414_pins) / sizeof(SN7414_pins[0]); i++) {
    pinMode(SN7414_pins[i], OUTPUT);
    digitalWrite(SN7414_pins[i], LOW);    // turn the LED off by making the voltage LOW
  }

  bri = json["bri"].as<int>();

  xTaskCreatePinnedToCore(
    handleDisplay,   /* Task function. */
    "handleDisplay",     /* name of task. */
    10000,       /* Stack size of task */
    NULL,        /* parameter of the task */
    10,           /* priority of the task */
    &Task1,      /* Task handle to keep track of created task */
    1);          /* pin task to core 0 */

  blankAllDigits();
}


void handleDisplay(void * pvParameters) { // this is time critical, pin to separate core or run as a ISR task.
  Serial.print("Display running on core ");
  Serial.println(xPortGetCoreID());

  while (1) {
    /*
    for (int i = 0; i < sizeof(anodes_pins) / sizeof(anodes_pins[0]); i++) {
      digitalWrite(anodes_pins[i], LOW);    // turn the LED off by making the voltage LOW
    }
    delay(4);
    */
    
    for (int a = 0; a < digitsCount; a++) {
      for (int i = 0; i < digitsCount; i++) {
        digitalWrite(anodes_pins[i], LOW);
      }
      delayMicroseconds(bri_delay_blank[bri]);
      for (int i = 0; i < (sizeof(SN7414_pins) / sizeof(SN7414_pins[0])); i++) {
        digitalWrite(SN7414_pins[i], numbers[ targetDigits[a] ][i]);
      }
      digitalWrite(anodes_pins[a], HIGH);
      delayMicroseconds(bri_delay_active[bri]);
    }
    
  }
}

void setDigit(uint8_t digit, uint8_t value) {
  targetDigits[digit] = value;
}

void setAllDigitsTo(uint16_t value) {
  for (int i = 0; i < digitsCount; i++) {
    setDigit(i, value);
  }
}

void blankDigit(uint8_t digit) {
  targetDigits[digit] = 10; // out of range values (> 9) will blank the digits
}

void blankAllDigits() {
  for (int i = 0; i < digitsCount; i++) {
    blankDigit(i);
  }
}

void showTime() {

  int hours = hour();
  if (hours > 12 && json["t_format"].as<int>() == 0) { // 12 / 24 h format
    hours -= 12;
  } else if (hours == 0 && json["t_format"].as<int>() == 0) {
    hours = 12;
  }

  int splitTime[6] = {
    (hours / 10) % 10,
    hours % 10,
    (minute() / 10) % 10,
    minute() % 10,
    (second() / 10) % 10,
    second() % 10,
  };

  for (int i = 0; i < digitsCount; i++) {
    if (i == 0 && splitTime[0] == 0 && json["zero"].as<int>() == 0) {
      blankDigit(i);
      continue;
    }
    setDigit(i, splitTime[i]);
  }

}

void cycleDigits() {
  updateColonColor(azure[bri]);
  strip.Show();

  for (int i = 0; i < 10; i++) {
    for (int ii = 0; ii < digitsCount; ii++) {
      setDigit(ii, i);
    }
    delay(1000);
  }

  strip.ClearTo(RgbColor(0, 0, 0));
  strip.Show();
}

void showIP(int delay_ms) {
  IPAddress ip_addr = WiFi.localIP();

  if (digitsCount < 6) {
    blankDigit(0);
    if ((ip_addr[3] / 100) % 10 == 0) {
      blankDigit(1);
    } else {
      setDigit(1, (ip_addr[3] / 100) % 10);
    }
    if ((ip_addr[3] / 10) % 10 == 0) {
      blankDigit(2);
    } else {
      setDigit(2, (ip_addr[3] / 10) % 10);
    }
    setDigit(3, (ip_addr[3]) % 10);
  } else {
    setDigit(0, 1);
    blankDigit(2);
    if ((ip_addr[3] / 100) % 10 == 0) {
      blankDigit(3);
    } else {
      setDigit(3, (ip_addr[3] / 100) % 10);
    }
    if ((ip_addr[3] / 10) % 10 == 0 && (ip_addr[3] / 100) % 10 == 0) {
      blankDigit(4);
    } else {
      setDigit(4, (ip_addr[3] / 10) % 10);
    }
    setDigit(5, (ip_addr[3]) % 10);
  }

  updateColonColor(azure[bri]);
  strip_show();
  delay(delay_ms);
  strip.ClearTo(RgbColor(0, 0, 0));
  strip_show();
}


void toggleNightMode() {
  if (json["nmode"].as<int>() == 0) return;
  if (hour() >= 22 || hour() <= 6) {
    bri = 0;
  } else {
    bri = json["bri"].as<int>();
  }
}

void healingCycle() {
  strip.ClearTo(RgbColor(0, 0, 0)); // red
  strip.Show();
  for (int i = 0; i < digitsCount; i++) {
    setDigit(i, healPattern[i][healIterator]);
  }
  healIterator++;
  if (healIterator > 9) healIterator = 0;
}
