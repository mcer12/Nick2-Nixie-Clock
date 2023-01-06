
void ICACHE_RAM_ATTR shiftSetValue(uint8_t pin, bool value) {
  //(value) ? bitSet(bytes[pinsToRegisterMap[pin]], pinsToBitsMap[pin]) : bitClear(bytes[pinsToRegisterMap[pin]], pinsToBitsMap[pin]);
  (value) ? bitSet(bytes[pin / 8], pin % 8) : bitClear(bytes[pin / 8], pin % 8);
}


/*
  void ICACHE_RAM_ATTR shiftSetAll(bool value) {
  for (int i = 0; i < registersCount * 8; i++) {
    //(value) ? bitSet(bytes[pinsToRegisterMap[i]], pinsToBitsMap[i]) : bitClear(bytes[pinsToRegisterMap[i]], pinsToBitsMap[i]);
    (value) ? bitSet(bytes[i / 8], i % 8) : bitClear(bytes[i / 8], i % 8);
  }
  }
*/
void ICACHE_RAM_ATTR shiftWriteBytes(volatile byte *data) {

  for (int i; i < bytesToShift; i++) {
    SPI.transfer(data[bytesToShift - 1 - i]);
  }

  // set gpio through register manipulation, fast!
  GPOS = 1 << LATCH;
  GPOC = 1 << LATCH;
}

void ICACHE_RAM_ATTR TimerHandler()
{

  // Only one ISR timer is available so if we want the dots to not glitch during wifi connection, we need to put it here...
  // speed of the dots depends on refresh frequency of the display
  /*
    if (enableDotsAnimation) {

    int stepCount = dotsAnimationSteps / registersCount;

    for (int i = 0; i < registersCount; i++) {
    if (dotsAnimationState >= stepCount * i && dotsAnimationState < stepCount * (i + 1)) {
      #if defined(CLOCK_VERSION_IV12)
      segmentBrightness[i][4] = bri_vals_separate[bri][i];
      #else
      segmentBrightness[i][7] = bri_vals_separate[bri][i];
      #endif
    } else {
      #if defined(CLOCK_VERSION_IV12)
      segmentBrightness[i][4] = 0;
      #else
      segmentBrightness[i][7] = 0;
      #endif
    }
    }
    dotsAnimationState++;
    if (dotsAnimationState >= dotsAnimationSteps) dotsAnimationState = 0;

    }
  */

  if (!isPoweredOn) {
    for (int i = 0; i < digitsCount; i++) {
      for (int ii = 0; ii < cathodeCount; ii++) {
        shiftSetValue(digitPins[i][ii], false);
      }
    }
    shiftWriteBytes(bytes); // Digits are reversed (first shift register = last digit etc.)
    return;
  }


  // Normal PWM
  for (int i = 0; i < digitsCount; i++) {
    for (int ii = 0; ii < cathodeCount; ii++) {
      // logic is inverted because we need to sink current!
      if (
        (shiftedDutyState[i] < bri_vals_separate[bri][i] && currentCathode[i] != targetCathode[i] && crossFadeState[i] <= bri_vals_separate[bri][i]) &&
        (
          (shiftedDutyState[i] > crossFadeState[i] && digitPins[i][ii] == currentCathode[i] && currentCathode > 0) ||
          (shiftedDutyState[i] <= crossFadeState[i] && digitPins[i][ii] == targetCathode[i] && targetCathode > 0)
        ) ||
        ( shiftedDutyState[i] < bri_vals_separate[bri][i] && currentCathode[i] == targetCathode[i] && targetCathode[i] == digitPins[i][ii])
      ) {
        shiftSetValue(digitPins[i][ii], true);
      }
      else {
        shiftSetValue(digitPins[i][ii], false);
      }
    }
  }

  if (
    (shiftedDutyState[0] < currentNeonBrightness && shiftedDutyState[0] < bri_vals_separate[bri][0])
  ) {
    // set gpio through register manipulation, fast!
    GPOS = 1 << COLON_PIN;
  }
  else {
    GPOC = 1 << COLON_PIN;
  }


  shiftWriteBytes(bytes); // Digits are reversed (first shift register = last digit etc.)

  for (int i = 0; i < digitsCount; i++) {
    shiftedDutyState[i]++;
    if (shiftedDutyState[i] >= pwmResolution) {
      shiftedDutyState[i] = 0;
    }
  }

  //if (dutyState > pwmResolution) dutyState = 0;
  //else dutyState++;
}

void initScreen() {
  pinMode(DATA, INPUT);
  pinMode(CLOCK, OUTPUT);
  pinMode(LATCH, OUTPUT);
  digitalWrite(LATCH, LOW);

  bri = json["bri"].as<int>();
  setupBriBalance();
  crossFadeTime = json["fade"].as<int>();
  setupPhaseShift();
  setupCrossFade();

  SPI.begin();
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);
  SPI.setClockDivider(SPI_CLOCK_DIV16);

  ITimer.attachInterruptInterval(TIMER_INTERVAL_uS, TimerHandler);

  blankAllDigits();
}

void enableScreen() {
  ITimer.attachInterruptInterval(TIMER_INTERVAL_uS, TimerHandler);
}
void disableScreen() {
  ITimer.detachInterrupt();
}

void setupBriBalance() {
  if (json["bal_enable"].as<int>() == 0) return;
  for (int i = 0; i < digitsCount; i++) {
    bri_vals_separate[0][i] = json["bal"]["low"][i].as<int>(); // low bri balance
    bri_vals_separate[1][i] = json["bal"]["high"][i].as<int>(); // medium brightness is ignored and set to high
    bri_vals_separate[2][i] = json["bal"]["high"][i].as<int>(); // high bri balance
  }
}

void setupCrossFade() {
  if (crossFadeTime > 0) {
    int fadeMillis = crossFadeTime / pwmResolution;
    fade_animation_ticker.attach_ms(fadeMillis, handleFade); // handle dimming animation
  }
}
void handleFade() {
  for (int i = 0; i < digitsCount; i++) {
    if (targetCathode[i] != currentCathode[i] && fadeIterator % (pwmResolution / bri_vals_separate[bri][i]) == 0) {
      //crossFadeState[i] += bri_vals_separate[bri][i] / 8;
      crossFadeState[i] += 1;

      if (crossFadeState[i] >= bri_vals_separate[bri][i]) {
        currentCathode[i] = targetCathode[i];
        crossFadeState[i] = 0;
      }
    }
  }
  if (currentNeonBrightness < targetNeonBrightness && currentNeonBrightness < bri_vals_separate[bri][0] && fadeIterator % (pwmResolution / bri_vals_separate[bri][0]) == 0) currentNeonBrightness++;
  if (currentNeonBrightness > targetNeonBrightness && currentNeonBrightness > 0 && fadeIterator % (pwmResolution / bri_vals_separate[bri][0]) == 0) currentNeonBrightness--;

  fadeIterator++;
}

void setNeon(bool state) {
  if (state) targetNeonBrightness = bri_vals_separate[bri][0];
  else targetNeonBrightness = 0;
}

void setDigit(uint8_t digit, uint8_t value) {
  if (value < 0) {
    targetCathode[digit] = -1;
    //targetBrightness[digit] = 0;
  } else {
    targetCathode[digit] = digitPins[digit][value];
  }
  crossFadeState[digit] = 0;

  // skip crossfade it's  disabled or brightness is below minimal crossfade threshold
  if (crossFadeTime == 0 || bri_vals_separate[bri][digit] < MINIMAL_CROSSFADE_BRIGHTNESS) {
    currentCathode[digit] = targetCathode[digit];
  }
}

void setAllDigitsTo(uint16_t value) {
  for (int i = 0; i < digitsCount; i++) {
    setDigit(i, value);
  }
}

void blankDigit(uint8_t digit) {
  setDigit(digit, -1);
}

void blankAllDigits() {
  for (int i = 0; i < digitsCount; i++) {
    blankDigit(i);
  }
}

void setDot(uint8_t digit, bool enable) {

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

  /*
    Serial.print(hours);
    Serial.print(":");
    Serial.print(minute());
    Serial.print(":");
    Serial.println(second());
  */
  for (int i = 0; i < digitsCount ; i++) {
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

  blankAllDigits();

  if (digitsCount < 6) {
    if ((ip_addr[3] / 100) % 10 != 0) {
      setDigit(1, (ip_addr[3] / 100) % 10);
    }
    if ((ip_addr[3] / 10) % 10 != 0) {
      setDigit(2, (ip_addr[3] / 10) % 10);
    }
    setDigit(3, (ip_addr[3]) % 10);
  } else {
    if ((ip_addr[3] / 100) % 10 != 0) {
      setDigit(3, (ip_addr[3] / 100) % 10);
    }
    if ((ip_addr[3] / 10) % 10 != 0 || (ip_addr[3] / 100) % 10 != 0) {
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

void setupPhaseShift() {
  // for some reason ZM1040 is singing more with phase shift
/*
#ifndef CLOCK_VERSION_ZM1040
  disableScreen();
  uint8_t shiftSteps = floor(pwmResolution / digitsCount);
  if (shiftSteps)
    for (int i = 0; i < digitsCount; i++) {
      shiftedDutyState[i] = i * shiftSteps;
    }
  enableScreen();
#endif
*/
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
