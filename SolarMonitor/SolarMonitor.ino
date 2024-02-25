// For the LilyGo T-Display S3 based ESP32S3 with ST7789 170 x 320 TFT
#include <ArduinoJson.h>  // https://github.com/bblanchon/ArduinoJson NOTE: *** MUST BE Version-6 or above ***
#include <Wire.h>
#include <WiFi.h>              // Built-in
#include <WiFiClientSecure.h>  // Built-in
#include "time.h"              // Built-in
#include "credentials.h"
#include "TFT_eSPI.h"
#include <TFT_eWidget.h>  // Widget library
#include <TFT_eFEX.h>

const char* ssid = ssid1;
const char* password = password1;


//################ PROGRAM VARIABLES and OBJECTS ################

float Production[96] = { 0 };

float LifeTimeEnergy = 0;
float Revenue = 0;
float LastYearEnergy = 0;
float LastMonthEnergy = 0;
float LastDayEnergy = 0;
float CurrentPower = 0;

WiFiClientSecure client;  // wifi client object

const uint16_t port = 443;

#define PIN_POWER_ON 15  // LCD and battery Power Enable
#define PIN_LCD_BL 38    // BackLight enable pin
#define PIN_BTN1 14
#define PIN_BTN2 0

TFT_eSPI tft = TFT_eSPI();
MeterWidget currentPowerMeter = MeterWidget(&tft);
GraphWidget dailyPowerGraph = GraphWidget(&tft);
TraceWidget tr1 = TraceWidget(&dailyPowerGraph);  // Graph trace 1
TFT_eFEX eFex = TFT_eFEX(&tft);

const uint32_t DISPLAY_PERIOD = 5 * 60 * 1000;  // Display updates every 5 minutes
uint32_t DISPLAY_LAST = 0;

const uint32_t GRAPH_PERIOD = 15 * 60 * 1000;  // Display updates every 5 minutes
uint32_t GRAPH_LAST = 0;

uint32_t COUNTDOWN_PERIOD = 1 * 1000;  // Display updates every 1 second
uint32_t COUNTDOWN_LAST = 0;

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -5 * 3600;
const int daylightOffset_sec = 3600;

bool IsGraph = false;

void setup() {
  Serial.begin(115200);

  pinMode(PIN_POWER_ON, OUTPUT);  //triggers the LCD backlight
  pinMode(PIN_LCD_BL, OUTPUT);    // BackLight enable pin
  pinMode(PIN_BTN1, INPUT);       //Right button proven to be pulled up, push = 0
  pinMode(PIN_BTN2, INPUT);       //Left button proven to be pulled up, push = 0

  digitalWrite(PIN_POWER_ON, HIGH);
  digitalWrite(PIN_LCD_BL, HIGH);

  // We start by connecting to a WiFi network

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  Serial.println("\nConnecting");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  DISPLAY_LAST = millis() - DISPLAY_PERIOD;
  GRAPH_LAST = millis() - GRAPH_PERIOD;

  IsGraph = false;
  SetPowerMeter();

  Serial.println("");
  Serial.println("Display Config Complete.");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  Serial.println("");
  Serial.println("Time Config Complete.");
}

//#########################################################################################
void loop() {
  String GETRequest = "";

  if (digitalRead(PIN_BTN1) == LOW && IsGraph == true) {
    IsGraph = false;
    SetPowerMeter();
    UpdateMeter();
  }

  if (digitalRead(PIN_BTN2) == LOW && IsGraph == false) {
    IsGraph = true;
    SetPowerGraph();
    UpdateGraph();
  }


  //Power Meter Update
  if (millis() > DISPLAY_LAST + DISPLAY_PERIOD) {
    Serial.println("Data Update");
    DISPLAY_LAST = millis();
    GETRequest = "GET /site/" + SiteNumber + "/overview?api_key=" + apikey;
    if (Obtain_Energy_Reading(GETRequest, 0) && IsGraph == false) {
      UpdateMeter();
    }
  }

  //Graph Update
  if (millis() > GRAPH_LAST + GRAPH_PERIOD) {
    Serial.println("Graph Update");
    char queryday[30], querymonth[30], queryyear[30];

    GRAPH_LAST = millis();
    time_t now;
    struct tm timeinfo;
    while (!getLocalTime(&timeinfo, 5000)) {
      Serial.println(F("Failed to obtain time"));
    }

    strftime(queryday, 30, "%d", &timeinfo);
    strftime(querymonth, 30, "%m", &timeinfo);
    strftime(queryyear, 30, "%Y", &timeinfo);

    GETRequest = "GET /site/" + SiteNumber + "/power?api_key=" + apikey + "&startTime=" + String(queryyear) + "-" + String(querymonth) + "-" + String(queryday) + "%20" + "00:00:00&endTime=" + String(queryyear) + "-" + String(querymonth) + "-" + String(queryday) + "%20" + "23:59:00";

    if (Obtain_Energy_Reading(GETRequest, 1) && IsGraph == true) {
      UpdateGraph();
    }
  }

  if (millis() > COUNTDOWN_LAST + COUNTDOWN_PERIOD) {
    COUNTDOWN_LAST = millis();
    float RemainingTime = (DISPLAY_LAST + DISPLAY_PERIOD) - millis();
    float RemainingPercent = (RemainingTime / DISPLAY_PERIOD) * 100.0;
    uint8_t percent = 100 - RemainingPercent;
    eFex.drawProgressBar(0, 160, 320, 10, percent, TFT_GREY, TFT_BLUE);
  }
}

void SetPowerMeter() {
  tft.fillScreen(TFT_BLACK);
  //                       --Red-- -Org-  -Yell-   -Grn-
  currentPowerMeter.setZones(0, 25, 25, 50, 50, 75, 75, 100);
  // Meter is 239 pixels wide and 126 pixels high
  currentPowerMeter.analogMeter(40, 30, 13.2, "kW", "0", "3.25", "6.5", "9.75", "13.12");  // Draw analogue meter at 0, 0
}

void UpdateMeter() {
  if (IsGraph == false) {
    currentPowerMeter.updateNeedle(CurrentPower / 1000.00, 0);
  }
}

void SetPowerGraph() {
  tft.fillScreen(TFT_BLACK);
  // Graph area is 200 pixels wide, 150 high, dark grey background
  dailyPowerGraph.createGraph(300, 150, tft.color565(5, 5, 5));
  dailyPowerGraph.setGraphScale(0, 96, 0, 13200);
  dailyPowerGraph.drawGraph(20, 0);

  // Draw the y axis scale
  tft.setTextDatum(MR_DATUM);  // Middle right text datum
  tft.drawNumber(0, dailyPowerGraph.getPointX(0.0), dailyPowerGraph.getPointY(0.0));
  tft.drawNumber(6.5, dailyPowerGraph.getPointX(0.0), dailyPowerGraph.getPointY(6500.0));
  tft.drawNumber(13, dailyPowerGraph.getPointX(0.0), dailyPowerGraph.getPointY(12000.0));

  tr1.startTrace(TFT_GREEN);
}

void UpdateGraph() {
  if (IsGraph == true) {
    tr1.startTrace(TFT_GREEN);
    for (int index = 0; index < 96; index++) {
      tr1.addPoint(index, Production[index]);
    }
  }
}

bool Obtain_Energy_Reading(String Request, int Type) {
  String rxtext = "";
  Serial.println("Connecting to server for data");
  client.stop();  // close connection before sending a new request
  client.setInsecure();
  if (client.connect(server, 443)) {  // if the connection succeeds
    Serial.println("Connecting...");
    // send the HTTP PUT request:
    client.println(Request);
    client.println("Host: monitoringapi.solaredge.com");
    client.println("User-Agent: ESP Energy Receiver/1.1");
    client.println("Connection: close");
    client.println();
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 10000) {  // Server can be slow so give it time
        Serial.println(">>> Client Timeout !");
        client.stop();
        return false;
      }
    }
    char c = 0;
    bool startJson = false;
    int jsonend = 0;
    bool Reading = true;
    while (client.available()) {
      c = client.read();
      //Serial.print(c);
      // JSON formats contain an equal number of open and close curly brackets,
      // so check that JSON is received correctly by counting open and close brackets
      if (c == '{') {
        startJson = true;  // set true to indicate JSON message has started
        jsonend++;
      }
      if (c == '}') {
        jsonend--;
      }
      if (startJson == true) {
        rxtext += c;
      }
      // if jsonend = 0 then we have have received equal number of curly braces
      if (jsonend == 0 && startJson == true) {
        Serial.println("");
        Serial.println("Received OK...");
        Serial.println(rxtext);
        if (Type == 0)
          if (!DecodeEnergyData(rxtext)) return false;
        if (Type == 1)
          if (!DecodeEnergyGraph(rxtext)) return false;

        client.stop();
        Reading = false;
        return true;
      }
    }
  } else {
    // if no connection was made:
    Serial.println("connection failed");
    return false;
  }

  return true;
}

bool DecodeEnergyData(String json) {
  Serial.print(F("Creating object...and "));
  DynamicJsonDocument doc(40 * 1024);
  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, json);
  // Test if parsing succeeds.
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return false;
  }
  // convert it to a JsonObject
  JsonObject root = doc.as<JsonObject>();
  Serial.println("Decoding data");
  // All Serial.println statements are for diagnostic purposes and not required, remove if not needed
  JsonObject overview = root["overview"];
  const char* overview_lastUpdateTime = overview["lastUpdateTime"];           // "2018-09-20 10:48:56"
  long overview_lifeTimeData_energy = overview["lifeTimeData"]["energy"];     // 350820
  float overview_lifeTimeData_revenue = overview["lifeTimeData"]["revenue"];  // 22.95513
  long overview_lastYearData_energy = overview["lastYearData"]["energy"];     // 350460
  long overview_lastMonthData_energy = overview["lastMonthData"]["energy"];   // 225261
  int overview_lastDayData_energy = overview["lastDayData"]["energy"];        // 680
  float overview_currentPower_power = overview["currentPower"]["power"];      // 264
  const char* overview_measuredBy = overview["measuredBy"];                   // "METER"
  LifeTimeEnergy = overview_lifeTimeData_energy;
  Revenue = overview_lifeTimeData_revenue;
  LastYearEnergy = overview_lastYearData_energy;
  LastMonthEnergy = overview_lastMonthData_energy;
  LastDayEnergy = overview_lastDayData_energy;
  CurrentPower = overview_currentPower_power;

  return true;
}

bool DecodeEnergyGraph(String json) {
  Serial.print(F("Creating object...and "));
  DynamicJsonDocument doc(40 * 1024);
  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, json);
  // Test if parsing succeeds.
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return false;
  }
  // convert it to a JsonObject
  JsonObject root = doc.as<JsonObject>();
  Serial.println("Decoding graph data");

  JsonObject power = root["power"];
  JsonArray power_values = power["values"];
  for (int index = 0; index < 96; index++) {
    String arrayValue = power_values[index]["value"];
    Serial.println(arrayValue);
    if (arrayValue == "null") {
      Production[index] = 0.0;
    } else {
      Production[index] = float(power_values[index]["value"]);
    }
  }
  return true;
}
