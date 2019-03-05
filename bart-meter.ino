/*
 *  BartMeter 1.5
 * .ESP8266 queries JSON schedule from api.bart.gov
 *  Outputs to SSD1306 128x32 OLED .97" display https://robotzero.one/heltec-wifi-kit-8/
 *  and also drives an analog panel meter and WS2812B RBG LED depending on how
 *  many minutes are left at the configured station.  Also includes alert for BART system delays
 *  Lots of Serial.printf's for debugging, can be removed to save some memory
 */
#include <Arduino.h>
//ESP8266 Libraries
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
//FastLED Library
#include <noise.h>
#include <bitswap.h>
#include <fastspi_types.h>
#include <pixelset.h>
#include <fastled_progmem.h>
#include <led_sysdefs.h>
#include <hsv2rgb.h>
#include <fastled_delay.h>
#include <colorpalettes.h>
#include <color.h>
#include <fastspi_ref.h>
#include <fastspi_bitbang.h>
#include <controller.h>
#include <fastled_config.h>
#include <colorutils.h>
#include <chipsets.h>
#include <pixeltypes.h>
#include <fastspi_dma.h>
#include <fastpin.h>
#include <fastspi_nop.h>
#include <platforms.h>
#include <lib8tion.h>
#include <cpp_compat.h>
#include <fastspi.h>
#include <FastLED.h>
#include <dmx.h>
#include <power_mgt.h>
// Arduino JSON Parser
#include <ArduinoJson.h>
// U8g2 Display Library
#include <U8g2lib.h>
#ifdef u8g2_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef u8g2_HAVE_HW_I2C
#include <Wire.h>
#endif
//Parameters for the LED
#define LED_PIN 7
#define COLOR_ORDER GRB
#define CHIPSET WS2812B
#define BRIGHTNESS 250
#define METER_PIN D3
CRGB leds[1];
//Parameters for the OLED panel
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ 16, /* clock=*/ 5, /* data=*/ 4);
const char* ssid     = "SSID";
const char* password = "WIFI_PASSWORD";
//Parameters for talking to Bart
const char* host = "api.bart.gov";
const char* station = "POWL";  // Bart Station you're leaving from
const char* apiKey  = "API_KEY";  // API Key
const char* dir     = "N";  // Direction, Northbound (N) or Southbound (S)


const unsigned long updateInterval = 10 * 1000;  // update interval of 10 seconds  (10L * 1000L; 10 seconds delay for testing)

WiFiClient client;
void setup() {

  // Initialize all the things
  u8g2.begin();
  u8g2.setPowerSave(0);
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, 1).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness( BRIGHTNESS );
  leds[0] = CRGB::Black;
  FastLED.show();
  Serial.begin(115200);
  delay(10);
  // Clear display, set font
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tf);
  // We start by connecting to a WiFi network
  Serial.print(F("Connecting to "));
  Serial.println(ssid);
  u8g2.setCursor(0, 8);
  u8g2.print("Connecting to ");
  u8g2.setCursor(0, 20);
  u8g2.print("Wifi");
  u8g2.sendBuffer();
  /* Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
     would try to act as both a client and an access-point and could cause
     network-issues with your other WiFi-devices on your WiFi-network. */
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    
  }
  Serial.println("");
  Serial.println(F("WiFi connected"));  
  Serial.println(F("IP address: "));
  Serial.println(WiFi.localIP());
  //u8g2.clear();
  u8g2.clearBuffer();
  u8g2.setCursor(0, 8);
  u8g2.print("Wifi connected");
  u8g2.setCursor(0, 20);
  u8g2.print("IP Address");
  u8g2.setCursor(0, 32);
  u8g2.print(WiFi.localIP().toString().c_str());
  u8g2.sendBuffer();
  delay(5000);
}


void loop() {
  delay(3000);
  Serial.print(F("Connecting to "));
  Serial.println(host);
  u8g2.clearBuffer();
  u8g2.setCursor(0, 8);
  u8g2.print("Connecting to");
  u8g2.setCursor(0, 20);
  u8g2.print("BART");
  u8g2.sendBuffer();
  // Use WiFiClient class to create TCP connections
 
  const int httpPort = 80;
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    //u8g2.clear();
    u8g2.clearBuffer();
    u8g2.setCursor(0, 8);
    u8g2.print("Connection ");
    u8g2.setCursor(0, 20);
    u8g2.print("failed!");
    u8g2.sendBuffer();
    return;
  }
  
  // We now create a URI for the request
  String url = "http://";
  url += host;
  url += "/api/etd.aspx?cmd=etd&orig=";
  url += station;
  url += "&key=";
  url += apiKey;
  url += "&dir=";
  url += dir;
  url += "&json=y";
  
  Serial.print(F("Requesting URL: "));
  Serial.println(url);
  u8g2.clearBuffer();
  u8g2.setCursor(32, 8);
  u8g2.print("Checking train");
  u8g2.setCursor(32, 20);
  u8g2.print("schedules");
  u8g2.sendBuffer();
// Connect to Bart API and parse the response
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;  //Object of class HTTPClient
    http.begin(url);
    int httpCode = http.GET();
    //Check the returning code                                                                  
    if (httpCode > 0) {
       const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_ARRAY_SIZE(2) + 2*JSON_ARRAY_SIZE(3) + JSON_OBJECT_SIZE(1) + 2*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + 2*JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(6) + 6*JSON_OBJECT_SIZE(8) + 1020;
      DynamicJsonBuffer jsonBuffer(capacity);
      // FIND FIELDS IN JSON TREE
      
      JsonObject& root = jsonBuffer.parseObject(http.getString());
      if (root.success()) {
      // parseObject() succeeded
      Serial.println(F("Parsing Successful"));
      }     
      else {
      // parseObject() failed
      Serial.println(F("Parsing failed!"));
      }
      JsonObject& subroot = root["root"];
      if (!subroot.success()) {
        Serial.println(F("No subroot"));
      }
      JsonObject& station = subroot["station"][0];
      if (!station.success()) {
        Serial.println(F("No station"));
      }
      JsonObject& etd1 = station["etd"][1];
      if (!etd1.success()) {
        Serial.println(F("No etd1"));
      }
      const char* trainLine = etd1["destination"]; // "Dublin/Pleasanton" or whatever line you specify earlier
      JsonArray& etd1_estimate = etd1["estimate"];
      if (!etd1_estimate.success()) {
        Serial.println(F("No etd1_estimate"));
      }
      JsonObject& etd1_estimate0 = etd1_estimate[0];
      if (!etd1_estimate0.success()) {
        Serial.println(F("No etd1_stimate0"));
      }
      const char* estimate0_minutes = etd1_estimate0["minutes"];
      int arrivalTime = etd1_estimate0["minutes"];
      const int bartdelay = etd1_estimate0["delay"];
      if (bartdelay == 1) {
        delaywarning();
      }
      setMeter(arrivalTime);
      setLED(arrivalTime);
      Serial.print(trainLine);
      Serial.println(" arriving in");
      Serial.print(estimate0_minutes);
      Serial.println(" minutes");
      u8g2.setFont(u8g2_font_ncenB08_tf);
      u8g2.clearBuffer();
      u8g2.setCursor(0, 8);
      u8g2.print(trainLine);
      u8g2.setCursor(0, 20);
      u8g2.print("arriving in:"); 
      u8g2.setCursor(0, 32);   
      u8g2.print(estimate0_minutes);
      u8g2.setCursor(16, 32);
      u8g2.print("minutes.");
      u8g2.sendBuffer();
      
  }
  //Close out connection
  http.end();   //Close connection
  Serial.println();
  Serial.println("closing connection");
  // Wait our update interval before updating again
    delay(updateInterval);
  }

} 
void setMeter(int arrivalTime) {
  int meterVal;
  Serial.print(F("arrivalTime = "));
  Serial.println(arrivalTime);
  //Scale our meter output to a 15 - 1 minute range
  meterVal = map(arrivalTime, 15, 1, 0, 1024);
  Serial.print(F("Meter set to: "));
  Serial.println(meterVal);
  analogWrite(METER_PIN, meterVal);
}
void setLED(int arrivalTime) {
  if (arrivalTime > 10) {
    leds[0] = CRGB::Green;
    Serial.println(F("Setting LED to Green"));
  }
  else if ( arrivalTime > 5 && arrivalTime <= 10) {
    leds[0] = CRGB::Yellow;
    Serial.println(F("Setting LED to Yellow"));
  }
  else if ( arrivalTime <= 5 ) {
    leds[0] = CRGB::Red;
    Serial.println(F("Setting LED to Red"));
  }
  else {
    leds[0] = CRGB::Black;
    Serial.println(F("Setting LED to Off"));
  }
  FastLED.show();
  delay(100);
  }
// Function to flash a warning when we have a delay
void delaywarning (void) {
  Serial.println("There is currently a delay");
  u8g2.setFont(u8g2_font_open_iconic_embedded_4x_t);
  //u8g2.clear();
  u8g2.clearBuffer();
// Draw a warning sign
  u8g2.drawGlyph(0,32,71);
  u8g2.sendBuffer();
  delay(1000);
  u8g2.setFont(u8g2_font_crox5tb_tf);

  u8g2.clearBuffer();
  u8g2.setCursor(0, 16);
  u8g2.print("Warning");
  u8g2.sendBuffer();
  delay(1000);
  //u8g2.clear();
  u8g2.clearBuffer();
  u8g2.setCursor(0, 16);
  u8g2.print("Delay");
  u8g2.sendBuffer();
  delay(1000);
}
