#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <TTBOUNCE.h>
#include <SSD1306Brzo.h>
#include <pg.h> // PostgreSQL client library

// Defines
#define VERSION "v1.0"
#define NAME "SonoffGrinder"
#define SONOFFBASIC // Use Sonoff Basic default Pins

#define ON LOW    // LOW is LED ON
#define OFF HIGH  // HIGH is LED OFF

// Configuration
const char* cSSID     = "coffee";
const char* cPASSWORD = "coffeecoffee";

// PostgreSQL Server Configuration
const char* db_host = "your-database-ip"; // Replace with your actual database IP
const int db_port = 5432; // Replace with your actual database port
const char* db_user = "your-username"; // Replace with your actual database username
const char* db_pass = "your-password"; // Replace with your actual database password
const char* db_name = "your-database"; // Replace with your actual database name

const bool bFlipDisplay = true;

#ifdef SONOFFBASIC
const int pGRINDER = 12; // Relay Pin of Sonoff
const int pLED = 13; // LED Pin of Sonoff
const int pBUTTON = 14; // GPIO14, last Pin on Header

const int pSCL = 1; // Tx Pin on Sonoff
const int pSDA = 3; // Rx Pin on Sonoff
#else // Custom or Development Board
const int pGRINDER = 12; // Relay Pin of Sonoff
const int pLED = 16; // LED Pin of Sonoff
const int pBUTTON = 0; // GPIO14, last Pin on Header

const int pSCL = 5; // Tx Pin on Sonoff
const int pSDA = 2; // Rx Pin on Sonoff
#endif

const unsigned long tMAX = 60000; // Maxmimum Time for grinding
const unsigned long tOVERLAY = 60000; // show additional information for this period
const unsigned long tDEBOUNCE = 50; // Debounce Time for Button
const unsigned long tPRESS = 300; // Time for Button Press Detection

char htmlResponse[3000];

bool bClick = false;
bool bDoubleClick = false;
bool bPress = false;

bool bWifiConnected = true; // initialize true to start Connection
bool bShowOverlay = true; // initialize true

int tSingleShot = 5000;
int tDualShot = 10000;

int tGrindDuration = 0;
int tGrindPeriod = 0;
unsigned long tGrindStart = 0;

os_timer_t timerGRINDER;

ESP8266WebServer server (80);
TTBOUNCE button = TTBOUNCE(pBUTTON);
SSD1306Brzo  display(0x3c, pSDA, pSCL); // ADDRESS, SDA, SCL

PGconn* conn;

void logGrindEvent(const char* event) {
  if (conn) {
    String query = String("INSERT INTO grind_log (event, duration, timestamp) VALUES ('") + event + "', " + String(tGrindDuration) + ", NOW());";
    PGresult* res = PQexec(conn, query.c_str());
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
      Serial.println("Insert failed");
    }
    PQclear(res);
  } else {
    Serial.println("No database connection");
  }
}

void click() {
  if (digitalRead(pGRINDER) == LOW) {
    digitalWrite(pGRINDER, HIGH);  // turn Relais ON
    os_timer_arm(&timerGRINDER, tSingleShot, false);
    bClick = true;
    tGrindPeriod = tSingleShot;
    tGrindStart = millis();
    logGrindEvent("click_start");
    Serial.println("Clicked");
    Serial.println("Relais " + String(tSingleShot, DEC) + " ms ON");
    digitalWrite(pLED, ON);
  } else {
    digitalWrite(pGRINDER, LOW);
    os_timer_disarm(&timerGRINDER);
    bClick = false;
    bDoubleClick = false;
    logGrindEvent("click_stop");
    Serial.println("Abort!");
    Serial.println("Relais OFF");
    digitalWrite(pLED, OFF);
  }
}

void doubleClick() {
  if (digitalRead(pGRINDER) == LOW) {
    digitalWrite(pGRINDER, HIGH);  // turn Relais ON
    os_timer_arm(&timerGRINDER, tDualShot, false);
    bDoubleClick = true;
    tGrindPeriod = tDualShot;
    tGrindStart = millis();
    logGrindEvent("double_click_start");
    Serial.println("DoubleClicked");
    Serial.println("Relais " + String(tDualShot, DEC) + " ms ON");
    digitalWrite(pLED, ON);
  } else {
    digitalWrite(pGRINDER, LOW);
    os_timer_disarm(&timerGRINDER);
    bClick = false;
    bDoubleClick = false;
    logGrindEvent("double_click_stop");
    Serial.println("Abort!");
    Serial.println("Relais OFF");
    digitalWrite(pLED, OFF);
  }
}

void press() {
  if(digitalRead(pGRINDER) == LOW || (bClick == true || bDoubleClick == true)) {
    digitalWrite(pGRINDER, HIGH);  // turn Relais ON
    bPress = true;
    tGrindPeriod = tMAX;
    if (bClick == false && bDoubleClick == false) {
      os_timer_arm(&timerGRINDER, tMAX, false);
      tGrindStart = millis(); // only initialize if manual Mode
    } else {
      os_timer_arm(&timerGRINDER, tMAX - (millis() - tGrindStart), false);
    }
    logGrindEvent("press_start");
    Serial.println("Pressed");
    Serial.println("Relais ON");
    digitalWrite(pLED, ON);
  } else {
    digitalWrite(pGRINDER, LOW);
    os_timer_disarm(&timerGRINDER);
    bPress = false;
    logGrindEvent("press_stop");
    Serial.println("Abort!");
    Serial.println("Relais OFF");
    digitalWrite(pLED, OFF);
  }
}

void timerCallback(void *pArg) {
  // start of timerCallback
  digitalWrite(pGRINDER, LOW);
  logGrindEvent("timer_expired");
  Serial.println("Timer expired");
  Serial.println("Relais OFF");
  digitalWrite(pLED, OFF);
  os_timer_disarm(&timerGRINDER);
  bClick = false;
  bDoubleClick = false;
  bPress = false;
}

void setup() {
  Serial.begin(115200); // Start serial

  pinMode(pGRINDER, OUTPUT);      // define Grinder output Pin
  digitalWrite(pGRINDER, LOW);    // turn Relais OFF
  pinMode(pLED, OUTPUT);
  digitalWrite(pLED, ON);         // turn LED ON at start

  pinMode(pBUTTON, INPUT_PULLUP);        // Bugfix ttbounce button.enablePullup(); not working

  handleWifi();

  display.init();
  if(bFlipDisplay == true) {display.flipScreenVertically();}
  handleDisplay();

  server.on("/", handleRoot );
  server.on("/save", handleSave);
  server.begin();
  //Serial.println ( "HTTP Server started" );

  os_timer_setfn(&timerGRINDER, timerCallback, NULL);

  button.setActiveLow(); button.enablePullup();   // Button from GPIO directly to GND, ebable internal pullup
  button.setDebounceInterval(tDEBOUNCE);
  button.setPressInterval(tPRESS);
  button.attachClick(click);                      // attach the Click Method to the Click Event
  button.attachDoubleClick(doubleClick);          // attach the double Click Method to the double Click Event
  button.attachPress(press);                      // attach the Press Method to the Press Event

  EEPROM.begin(8);  // Initialize EEPROM
  tSingleShot = eeGetInt(0);
  tDualShot = eeGetInt(4);

  // Connect to PostgreSQL database
  conn = PQconnectdb("host=" + String(db_host) + " port=" + String(db_port) + " user=" + String(db_user) + " password=" + String(db_pass) + " dbname=" + String(db_name));
  if (PQstatus(conn) != CONNECTION_OK) {
    Serial.println("Connection to database failed");
  } else {
    Serial.println("Connected to database");
  }

  //======OTA PART=========================================================
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
    digitalWrite(pLED, ON);
    os_timer_arm(&timerGRINDER, 0, false); // instantly fire Timer
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_16);
    display.drawString(64, 10, "OTA Finsihed");
    display.drawProgressBar(0, 32, 127, 10, 100);
    display.drawString(64, 45, "rebooting..");
    display.display();
    // eeWriteInt(0, 1000); // for testing
    // eeWriteInt(4, 2000); // for testing
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_16);
    display.drawString(64, 10, "OTA Update");
    display.drawProgressBar(0, 32, 127, 10, progress / (total / 100));
    display.drawString(64, 45, String(progress / (total / 100)) + " %");
    display.display();
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_16);
    display.drawString(64, 10, "OTA Error");
    display.drawProgressBar(0, 32, 127, 10, 100);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
      display.drawString(64, 45, "Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
      display.drawString(64, 45, "Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
      display.drawString(64, 45, "Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
      display.drawString(64, 45, "Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
      display.drawString(64, 45, "End Failed");
    }
    display.display();
    delay(1000);
  });
  ArduinoOTA.begin();

  digitalWrite(pLED, OFF);        // turn LED OFF after Setup
}

void loop() {
  ArduinoOTA.handle();

  server.handleClient();
  
  button.update();

  if(bPress == true) {
    // Press was detected
    if(button.read() == LOW) { // wait for Button release
      tGrindDuration = millis() - tGrindStart;
      os_timer_arm(&timerGRINDER, 0, false); // instantly fire Timer
      bPress = false;
      if (bClick == true) {
        eeWriteInt(0, tGrindDuration); // safe single Shot Time
        bClick = false;
        tSingleShot = tGrindDuration;
        logGrindEvent("single_shot_set");
        Serial.println("Single Shot set to " + String(tGrindDuration, DEC) + " ms");
      }
      else if (bDoubleClick == true) {
        eeWriteInt(4, tGrindDuration); // safe double Shot Time
        bDoubleClick = false;
        tDualShot = tGrindDuration;
        logGrindEvent("double_shot_set");
        Serial.println("Double Shot set to " + String(tGrindDuration, DEC) + " ms");
      }
    }
  }

  handleWifi();
  handleDisplay();
}
