/*
   GNU General Public License v3.0
   Copyright (c) 2022 Martin Cerny
*/

#include <FS.h>
#include <ArduinoJson.h>
#include <math.h>

#include "ESP8266TimerInterrupt.h"
#include "SPI.h"

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <Ticker.h>

#include <NeoPixelBus.h>
#include <NeoPixelAnimator.h>

#include <TimeLib.h>
#include <Timezone.h>

// Pick a clock version below!
#define CLOCK_VERSION_IN16

#if defined(CLOCK_VERSION_IN16)
#define CLOCK_6_DIGIT
#endif

#if !defined(CLOCK_VERSION_IN16)
#error "You have to select a clock version! Line 25"
#endif

#define AP_NAME "NICK2_"
#define FW_NAME "NICK2"
#define FW_VERSION "1.0.0"
#define CONFIG_TIMEOUT 300000 // 300000 = 5 minutes

// ONLY CHANGE DEFINES BELOW IF YOU KNOW WHAT YOU'RE DOING!
#define NORMAL_MODE 0
#define OTA_MODE 1
#define CONFIG_MODE 2
#define CONFIG_MODE_LOCAL 3
#define CONNECTION_FAIL 4
#define UPDATE_SUCCESS 1
#define UPDATE_FAIL 2
#define DATA 13
#define CLOCK 14
#define LATCH 15
#define TIMER_INTERVAL_uS 300 // screen pwm interval in microseconds, 300 = safe value for 6 digits. You can go down to 150-200 for 4-digit one. Going too low will cause crashes.
#define MINIMAL_CROSSFADE_BRIGHTNESS 8 // crossfade will be disabled below this brightness because there's not enough steps for smooth transition

// User global vars
const char* dns_name = "nick2"; // only for AP mode
const char* update_path = "/update";
const char* update_username = "nick2";
const char* update_password = "nick2";
const char* ntpServerName = "pool.ntp.org";

const int dotsAnimationSteps = 2000; // dotsAnimationSteps * TIMER_INTERVAL_uS = one animation cycle time in microseconds

const uint8_t PixelCount = 14; // Addressable LED count

HsbColor red[] = {
  HsbColor(RgbColor(100, 0, 0)), // LOW
  HsbColor(RgbColor(150, 0, 0)), // MEDIUM
  HsbColor(RgbColor(200, 0, 0)), // HIGH
};
HsbColor green[] = {
  HsbColor(RgbColor(0, 100, 0)), // LOW
  HsbColor(RgbColor(0, 150, 0)), // MEDIUM
  HsbColor(RgbColor(0, 200, 0)), // HIGH
};
HsbColor blue[] = {
  HsbColor(RgbColor(0, 0, 100)), // LOW
  HsbColor(RgbColor(0, 0, 150)), // MEDIUM
  HsbColor(RgbColor(0, 0, 200)), // HIGH
};
HsbColor yellow[] = {
  HsbColor(RgbColor(100, 100, 0)), // LOW
  HsbColor(RgbColor(150, 150, 0)), // MEDIUM
  HsbColor(RgbColor(200, 200, 0)), // HIGH
};
HsbColor purple[] = {
  HsbColor(RgbColor(100, 0, 100)), // LOW
  HsbColor(RgbColor(150, 0, 150)), // MEDIUM
  HsbColor(RgbColor(200, 0, 200)), // HIGH
};
HsbColor azure[] = {
  HsbColor(RgbColor(0, 100, 100)), // LOW
  HsbColor(RgbColor(0, 150, 150)), // MEDIUM
  HsbColor(RgbColor(0, 200, 200)), // HIGH
};

#if defined(CLOCK_VERSION_IV6) || defined(CLOCK_VERSION_IV6_V2)
HsbColor colonColorDefault[] = {
  HsbColor(RgbColor(30, 70, 50)), // LOW
  HsbColor(RgbColor(50, 100, 80)), // MEDIUM
  HsbColor(RgbColor(80, 130, 100)), // HIGH
};
#else
HsbColor colonColorDefault[] = {
  HsbColor(RgbColor(30, 70, 50)), // LOW
  HsbColor(RgbColor(50, 100, 80)), // MEDIUM
  HsbColor(RgbColor(120, 220, 140)), // HIGH
};
/*
  RgbColor colonColorDefault[] = {
  RgbColor(30, 70, 50), // LOW
  RgbColor(50, 100, 80), // MEDIUM
  RgbColor(100, 200, 120), // HIGH
  };
*/
#endif

/*
  RgbColor colonColorDefault[] = {
  RgbColor(30, 6, 1), // LOW
  RgbColor(38, 8, 2), // MEDIUM
  RgbColor(50, 10, 2), // HIGH
  };
*/
RgbColor currentColor = RgbColor(0, 0, 0);
//RgbColor colonColorDefault = RgbColor(90, 27, 7);
//RgbColor colonColorDefault = RgbColor(38, 12, 2);

// MAX6921 has 20 outputs, we have 3 of them,
// closest to that is 64 (8x8)
const uint8_t digitsCount = 6; // number of digits
const uint8_t cathodeCount = 10; // number of cathodes per digit
const uint8_t bytesToShift = 8; // we have 60 outputs, 60 / 8 = 8 bytes
const uint8_t digitPins[digitsCount][cathodeCount] = {
  {
    14,  //0
    1, //1
    3, //2
    2, //3
    4, //4
    6, //5
    5, //6
    7, //7
    0, //8
    15,  //9
  },
  {
    21, //0
    13, //1
    20, //2
    11, //3
    10, //4
    23, //5
    22, //6
    12, //7
    19, //8
    18, //9
  },
  {
    24, //0
    16, //1
    30, //2
    27, //3
    28, //4
    29, //5
    31, //6
    17, //7
    26, //8
    25, //9
  },
  {
    42, //0
    39, //1
    33, //2
    37, //3
    38, //4
    35, //5
    34, //6
    36, //7
    32, //8
    41, //9
  },

  {
    54, //0
    46, //1
    43, //2
    45, //3
    55, //4
    48, //5
    44, //6
    47, //7
    62, //8
    49, //9
  },
  {
    57, //0
    53, //1 // 56 = dot
    60, //2
    51, //3
    50, //4
    63, //5
    61, //6
    52, //7
    59, //8
    58, //9
  },
};


/*
  struct Digits {
  uint8_t current_cathode;
  uint8_t target_cathode;
  uint8_t current_brightness;
  uint8_t target_brightness;
  uint8_t cf_state;
  };
*/
volatile uint8_t currentCathode[digitsCount];
volatile uint8_t targetCathode[digitsCount];
volatile uint8_t crossFadeState[digitsCount];
volatile uint8_t currentBrightness[digitsCount];
volatile uint8_t targetBrightness[digitsCount];

// 32 steps of brightness * 200uS => 6.4ms for full refresh => 160Hz... pretty good!
// 48 steps => 100hz
volatile uint8_t shiftedDutyState[digitsCount];
const uint8_t pwmResolution = 48; // should be in the multiples of dimmingSteps to enable smooth crossfade
const uint8_t dimmingSteps = 2;

// Cathode poisoning prevention pattern --> circle through least used digits, prioritize number 7
uint8_t healPattern[6][10] = {
  {0, 6, 9, 3, 4, 5, 6, 7, 8, 9},
  {0, 1, 2, 3, 4, 5, 6, 7, 8, 9},
  {7, 3, 4, 7, 5, 6, 7, 8, 7, 9},
  {4, 5, 6, 7, 8, 9, 6, 7, 8, 9},
  {6, 7, 8, 7, 9, 6, 7, 8, 9, 7},
  {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}
};

// MAX BRIGHTNESS PER DIGIT
// These need to be multiples of 8 to enable crossfade! Must be less or equal as pwmResolution.
// Set maximum brightness for reach digit separately. This can be used to normalize brightness between new and burned out tubes.
// Last two values are ignored in 4-digit clock
uint8_t bri_vals_separate[3][6] = {
  {8, 8, 8, 8, 8, 8}, // Low brightness
  {24, 24, 24, 24, 24, 24}, // Medium brightness
  {48, 48, 48, 48, 48, 48}, // High brightness
};


// Better left alone global vars
volatile bool isPoweredOn = true;
unsigned long configStartMillis, prevDisplayMillis;
volatile int activeDot;
uint8_t deviceMode = NORMAL_MODE;
bool timeUpdateFirst = true;
volatile bool toggleSeconds;
bool breatheState;
byte mac[6];
volatile int dutyState = 0;
//volatile uint8_t digitsCache[] = {0, 0, 0, 0};
volatile byte bytes[bytesToShift];
volatile byte prevBytes[bytesToShift];
volatile uint8_t bri = 0;
volatile int crossFadeTime = 0;
volatile uint8_t fadeIterator;
uint8_t healIterator;
uint8_t timeUpdateStatus = 0; // 0 = no update, 1 = update success, 2 = update fail,
uint8_t failedAttempts = 0;
volatile bool enableDotsAnimation;
volatile unsigned short dotsAnimationState;
RgbColor colonColor;
IPAddress ip_addr;

TimeChangeRule EDT = {"EDT", Last, Sun, Mar, 1, 120};  //UTC + 2 hours
TimeChangeRule EST = {"EST", Last, Sun, Oct, 1, 60};  //UTC + 1 hours
Timezone TZ(EDT, EST);
NeoPixelBus<NeoGrbFeature, NeoWs2813InvertedMethod> strip(PixelCount);
NeoGamma<NeoGammaTableMethod> colorGamma;
NeoPixelAnimator animations(PixelCount);
DynamicJsonDocument json(2048); // config buffer
Ticker fade_animation_ticker;
Ticker onceTicker;
Ticker colonTicker;
ESP8266Timer ITimer;
DNSServer dnsServer;
ESP8266WebServer server(80);
WiFiUDP Udp;
ESP8266HTTPUpdateServer httpUpdateServer;
unsigned int localPort = 8888;  // local port to listen for UDP packets


// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  Serial.begin(115200);
  Serial.println("");

  if (!SPIFFS.begin()) {
    Serial.println("[CONF] Failed to mount file system");
  }
  readConfig();

  initStrip();
  initRgbColon();
  initScreen();

  WiFi.macAddress(mac);

  const char* ssid = json["ssid"].as<const char*>();
  const char* pass = json["pass"].as<const char*>();
  const char* ip = json["ip"].as<const char*>();
  const char* gw = json["gw"].as<const char*>();
  const char* sn = json["sn"].as<const char*>();

  if (ssid != NULL && pass != NULL && ssid[0] != '\0' && pass[0] != '\0') {
    Serial.println("[WIFI] Connecting to: " + String(ssid));
    WiFi.mode(WIFI_STA);

    if (ip != NULL && gw != NULL && sn != NULL && ip[0] != '\0' && gw[0] != '\0' && sn[0] != '\0') {
      IPAddress ip_address, gateway_ip, subnet_mask;
      if (!ip_address.fromString(ip) || !gateway_ip.fromString(gw) || !subnet_mask.fromString(sn)) {
        Serial.println("[WIFI] Error setting up static IP, using auto IP instead. Check your configuration.");
      } else {
        WiFi.config(ip_address, gateway_ip, subnet_mask);
      }
    }
    // serializeJson(json, Serial);

    enableDotsAnimation = true; // Start the dots animation

    updateColonColor(yellow[bri]);
    strip_show();

    WiFi.hostname(AP_NAME + macLastThreeSegments(mac));
    WiFi.begin(ssid, pass);

    //startBlinking(200, colorWifiConnecting);

    for (int i = 0; i < 1000; i++) {
      if (WiFi.status() != WL_CONNECTED) {
        if (i > 200) { // 20s timeout
          enableDotsAnimation = false;
          deviceMode = CONFIG_MODE;
          updateColonColor(red[bri]);
          strip_show();
          Serial.print("[WIFI] Failed to connect to: " + String(ssid) + ", going into config mode.");
          delay(500);
          break;
        }
        delay(100);
      } else {
        updateColonColor(green[bri]);
        enableDotsAnimation = false;
        strip_show();
        Serial.print("[WIFI] Successfully connected to: ");
        Serial.println(WiFi.SSID());
        Serial.print("[WIFI] Mac address: ");
        Serial.println(WiFi.macAddress());
        Serial.print("[WIFI] IP address: ");
        Serial.println(WiFi.localIP());
        delay(1000);
        break;
      }
    }
  } else {
    deviceMode = CONFIG_MODE;
    Serial.println("[CONF] No credentials set, going to config mode.");
  }

  if (deviceMode == CONFIG_MODE || deviceMode == CONNECTION_FAIL) {
    startConfigPortal(); // Blocking loop
  } else {
    ndp_setup();
    startServer();
  }

  //initScreen();

  if (json["rst_cycle"].as<unsigned int>() == 1) {
    cycleDigits();
    delay(500);
  }

  if (json["rst_ip"].as<unsigned int>() == 1) {
    showIP(5000);
    delay(500);
  }

  /*
    if (!MDNS.begin(dns_name)) {
      Serial.println("[ERROR] MDNS responder did not setup");
    } else {
      Serial.println("[INFO] MDNS setup is successful!");
      MDNS.addService("http", "tcp", 80);
    }
  */
}

// the loop function runs over and over again forever
void loop() {

  if (timeUpdateFirst == true && timeUpdateStatus == UPDATE_FAIL || deviceMode == CONNECTION_FAIL) {
    setAllDigitsTo(0);
    updateColonColor(red[bri]); // red
    strip_show();
    delay(10);
    return;
  }

  if (millis() - prevDisplayMillis >= 1000) { //update the display only if time has changed
    prevDisplayMillis = millis();
    toggleNightMode();

    if (
      (json["cathode"].as<int>() == 1 && (hour() >= 2 && hour() <= 6) && minute() < 10) ||
      (json["cathode"].as<int>() == 2 && (((hour() >= 2 && hour() <= 6) && minute() < 10) || minute() < 1))
    ) {
      healingCycle(); // do healing loop if the time is right :)
    } else {
      
      if (timeUpdateStatus) {
        if (timeUpdateStatus == UPDATE_SUCCESS) {
          setTemporaryColonColor(5, green[bri]);
        }
        if (timeUpdateStatus == UPDATE_FAIL) {
          if (failedAttempts > 2) {
            colonColor = red[bri];
          } else {
            setTemporaryColonColor(5, red[bri]);
          }
        }
        timeUpdateStatus = 0;
      }

      handleColon();
      showTime();

    }

    //setAllDigitsTo(8);

    /*
        int i = 0;
        for (;;) {
          i++;
          if (i > 9) i = 0;
          Serial.println(i);
          setDigit(0, i);
          setDigit(1, i);
          setDigit(2, i);
          setDigit(3, i);
          setDigit(4, i);
          setDigit(5, i);
          delay(2000);
        }
    */

  }

  animations.UpdateAnimations();
  strip_show();

  //MDNS.update();
  server.handleClient();
  delay(1); // Simple delay doesn't look like much but it keeps the ESP cold and saves power

}
