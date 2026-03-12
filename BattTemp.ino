#define BLYNK_TEMPLATE_ID    "Legacy"
#define BLYNK_TEMPLATE_NAME "Legacy"
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiManager.h> 
#include <BlynkSimpleEsp32.h> 
#include <ArduinoOTA.h>
#include <HTTPUpdateServer.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h> 
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Update.h> 
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
// หมายเหตุ: นำ esp_task_wdt.h ออกเนื่องจากไม่ได้ใช้ระบบ Config WDT แบบเต็มตัว

// --- [ ส่วนที่ 1: การตั้งค่าบอร์ด ] ---
String boardList[] = {"4948", "4A90", "59EC", "G7H8", "I9J0", "K1L2", "M3N4", "O5P6"};
int myID = 0; 
String myMacSuffix = ""; 
char auth[] = "yXTSHdXVnNyDihE9zrzxQGE2JosxAQaa";

// --- [ ส่วนที่ 2: ระบบ Update & Web ] ---
float currentVersion = 1.1; 
const String versionURL = "https://raw.githubusercontent.com/thanormsinm-byte/BattTemp/main/version.json";
const String firmwareURL = "https://raw.githubusercontent.com/thanormsinm-byte/BattTemp/main/Batt.bin";
WebServer server(80);
HTTPUpdateServer httpUpdater;
bool isOnline = false; 

// --- [ ส่วนที่ 3: ฮาร์ดแวร์ ] ---
const int oneWireBus = 5;       
const int LedHeartBeat = 2; 
const int ledPin = 4;        
const int configButton = 0; 
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);
LiquidCrystal_I2C lcd(0x27, 20, 4); 

unsigned long previousMillis = 0, blinkMillis = 0, blynkMillis = 0, buttonPressTime = 0;
int displayPage = 0; 
bool blinkState = false; 
String fullHostname = "";
String macAddrStr = "";

// --- [ ส่วนที่ 4: ฟังก์ชันเสริม (Helper Functions) ] ---

String getEfuseMac() {
  uint64_t mac = ESP.getEfuseMac(); char macBuf[18];
  sprintf(macBuf, "%02X:%02X:%02X:%02X:%02X:%02X", (uint8_t)(mac >> 0), (uint8_t)(mac >> 8), (uint8_t)(mac >> 16), (uint8_t)(mac >> 24), (uint8_t)(mac >> 32), (uint8_t)(mac >> 40));
  return String(macBuf);
}

String getSharedHTML(bool isUpdatePage) {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body { font-family: sans-serif; text-align: center; color: #333; margin: 0; padding-top: 50px; }";
  html += ".container { max-width: 400px; margin: auto; padding: 20px; } h2 { font-size: 16px; font-weight: bold; }";
  html += ".upload-area { border: 1.5px solid #e0e0e0; border-radius: 30px; padding: 8px 25px; cursor: pointer; display: inline-flex; }";
  html += "table { width: 100%; margin-top: 30px; border-collapse: collapse; } td { padding: 10px; border-bottom: 1px solid #eee; text-align: left; font-size: 13px; }";
  html += ".val { text-align: right; font-weight: bold; color: #f39c12; } </style></head><body>";
  html += "<div class='container'><h2>" + String(isUpdatePage ? "UPDATE FIRMWARE" : "SYSTEM DASHBOARD") + "</h2>";
  html += "<table><tr><td>SSID</td><td class='val'>" + WiFi.SSID() + "</td></tr><tr><td>IP</td><td class='val'>" + WiFi.localIP().toString() + "</td></tr>";
  html += "<tr><td>ID</td><td class='val'>" + fullHostname + "</td></tr><tr><td>Ver</td><td class='val'>" + String(currentVersion, 1) + "</td></tr></table></div></body></html>";
  return html;
}

// --- [ ส่วนที่ปรับปรุง: แก้ไข Error WDT และ Progress Bar ] ---
void update_progress(int cur, int total) {
  static int dotCount = 0;
  static unsigned long lastUpdate = 0;
  
  yield(); // คืนเวลาให้ระบบจัดการ WiFi/Network ป้องกันการหลุดขณะโหลด

  if (millis() - lastUpdate > 500) {
    lastUpdate = millis();
    dotCount++;
    if (dotCount > 11) dotCount = 0; 
    
    // ล้างเฉพาะพื้นที่หลังคำว่า Updating (ตำแหน่งที่ 8-20)
    lcd.setCursor(8, 3); 
    lcd.print("            "); 
    
    lcd.setCursor(8, 3);
    String dots = "";
    for (int i = 0; i < dotCount; i++) {
      dots += ".";
    }
    lcd.print(dots);
  }
}

void checkGitHubUpdate() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure client; 
  client.setInsecure(); 
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(client, versionURL);
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) { 
    String payload = http.getString();
    
    // ปรับการหาตำแหน่งให้เป็นอิสระต่อกัน (ไม่โดนบังคับลำดับ)
    int verKeyPos = payload.indexOf("\"version\":");
    int buildKeyPos = payload.indexOf("\"Build\":");
    
    if (verKeyPos != -1) {
      int firstQuote = payload.indexOf("\"", verKeyPos + 10); 
      int secondQuote = payload.indexOf("\"", firstQuote + 1);
      String newVerStr = payload.substring(firstQuote + 1, secondQuote);
      float newVersion = newVerStr.toFloat();
   
      String buildDate = "N/A";
      if (buildKeyPos != -1) {
        int bFirstQuote = payload.indexOf("\"", buildKeyPos + 8);
        int bSecondQuote = payload.indexOf("\"", bFirstQuote + 1);
        buildDate = payload.substring(bFirstQuote + 1, bSecondQuote);
      }

      Serial.print("\n[OTA] Curr("); Serial.print(currentVersion, 1);
      Serial.print(") <--> Last("); Serial.print(newVersion, 1);
      Serial.println(")");

      if (newVersion > currentVersion) {
        Serial.println("[OTA] >>> Found New Fw. <<<");
        
        lcd.clear();
        lcd.setCursor(0, 0); lcd.print("    ----OTA----");
        lcd.setCursor(0, 1); lcd.print("Ver.:" + String(newVersion, 1));
        lcd.setCursor(0, 2); lcd.print("Bld.:" + buildDate);
        lcd.setCursor(0, 3); lcd.print("Updating"); 

        httpUpdate.onProgress(update_progress);
        httpUpdate.rebootOnUpdate(true);
        
        t_httpUpdate_return ret = httpUpdate.update(client, firmwareURL);
      
        if (ret == HTTP_UPDATE_FAILED) {
          Serial.printf("[OTA] Update Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
          lcd.setCursor(0, 3); lcd.print("Update Failed!      ");
        }
      }
    }
  }
  http.end();
}

// --- [ ส่วนที่ 5: ฟังก์ชันหลักของระบบ ] ---

void startWiFiManager() {
  server.stop(); 
  delay(500); 

  lcd.clear(); 
  lcd.setCursor(0, 0); lcd.print("--- CHANGE WIFI ---");
  lcd.setCursor(0, 1); lcd.print("1.Connect WiFi Name");
  lcd.setCursor(0, 2); lcd.print(">> " + fullHostname); 

  for(int i=0; i<3; i++) {
    lcd.setCursor(0, 3); lcd.print("2.Open: 192.168.4.1"); delay(500);
    lcd.setCursor(8, 3); lcd.print("           "); delay(500);
  }
  lcd.setCursor(0, 3); lcd.print("2.Open: 192.168.4.1");
  
  WiFiManager wm; 
  wm.setConfigPortalTimeout(180); 
  
  if (!wm.startConfigPortal(fullHostname.c_str())) {
    delay(1000);
  }
  ESP.restart(); 
}

void setup() {
  Serial.begin(115200);
  Serial.println("--- SYSTEM BOOT ---");
  
  lcd.begin(); 
  lcd.clear();
  lcd.backlight(); 
  lcd.setCursor(0, 0); lcd.print("--- SYSTEM BOOT ---");
  lcd.setCursor(0, 1); lcd.print("    Hardware Init.");
  
  pinMode(LedHeartBeat, OUTPUT); 
  pinMode(ledPin, OUTPUT); 
  pinMode(configButton, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  WiFi.begin(); 
  
  int retry = 0;
  while (WiFi.SSID() == "" && retry < 10) { 
    delay(100); 
    retry++; 
  }

  macAddrStr = getEfuseMac();
  uint64_t mac = ESP.getEfuseMac();
  char hexStr[5];
  sprintf(hexStr, "%02X%02X", (uint8_t)(mac >> 32), (uint8_t)(mac >> 40)); 
  myMacSuffix = String(hexStr);
  
  for (int i = 0; i < 8; i++) { if (myMacSuffix.equalsIgnoreCase(boardList[i])) { myID = i + 1; break; } }
  
  fullHostname = (myID > 0 ? String(myID) : "N/A") + "-Batt-" + myMacSuffix;
  
  lcd.setCursor(0, 2); lcd.print("Connect to WiFi...");

  lcd.setCursor(0, 3);
  String targetSSID = WiFi.SSID(); 
  if (targetSSID != "") {
    lcd.print("SSID: " + targetSSID.substring(0, 14));
  } else {
    lcd.print("SSID: Not Found!");
  }
  
  WiFiManager wm;
  wm.setConfigPortalTimeout(60); 
  
  wm.setAPCallback([](WiFiManager *myWiFiManager) {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("--- CONFIG MODE ---");
    lcd.setCursor(0, 1); lcd.print("1.Connect WiFi Name");
    lcd.setCursor(0, 2); lcd.print(">> " + fullHostname); 
    
    for(int i=0; i<3; i++) {
      lcd.setCursor(0, 3); lcd.print("2.Open: 192.168.4.1"); delay(500);
      lcd.setCursor(8, 3); lcd.print("           "); delay(500);
    }
    lcd.setCursor(0, 3); lcd.print("2.Open: 192.168.4.1");
  });

  if (wm.autoConnect(fullHostname.c_str())) {
    isOnline = true;
    ArduinoOTA.setHostname(fullHostname.c_str()); ArduinoOTA.begin();
    Blynk.config(auth, "myplant.ddns.net", 8080); Blynk.connect(); 

    server.on("/", HTTP_GET, []() { server.send(200, "text/html", getSharedHTML(false)); });
    server.on("/update", HTTP_GET, []() { server.send(200, "text/html", getSharedHTML(true)); });
    httpUpdater.setup(&server, "/update"); 
    server.begin();
    checkGitHubUpdate();
  }
  sensors.begin(); 
  lcd.clear();
}

void loop() {
  if (digitalRead(configButton) == LOW) {
    if (buttonPressTime == 0) buttonPressTime = millis();
    if (millis() - buttonPressTime > 1500) { startWiFiManager(); } 
  } else { buttonPressTime = 0; }

  if (WiFi.status() == WL_CONNECTED) {
    if (!isOnline) { isOnline = true; }
    Blynk.run(); ArduinoOTA.handle(); server.handleClient(); 
  } else { isOnline = false; }

  sensors.requestTemperatures(); 
  float temperatureC = sensors.getTempCByIndex(0);
  unsigned long currentMillis = millis();

  if (isOnline && (currentMillis - blynkMillis >= 2000)) {
    blynkMillis = currentMillis;
    if (Blynk.connected() && myID > 0) {
      Blynk.virtualWrite(myID, temperatureC); 
      Blynk.virtualWrite(myID + 20, fullHostname); 
    }
  }

  if (currentMillis - blinkMillis >= 500) {
    blinkMillis = currentMillis; blinkState = !blinkState; digitalWrite(LedHeartBeat, blinkState); 
  }

  unsigned long displayInterval = (displayPage == 1) ? 15000 : 3000;
  if (currentMillis - previousMillis >= displayInterval) {
    previousMillis = currentMillis; 
    displayPage = (displayPage > 1) ? 0 : displayPage + 1;
    lcd.clear(); 
  }

  if (displayPage == 0) { 
    lcd.setCursor(0, 0); lcd.print("Temp. Monitor V" + String(currentVersion, 1));
    lcd.setCursor(0, 1); lcd.print("      ----------      ");
    lcd.setCursor(0, 2); lcd.print("By  : Teerawat.C");
    lcd.setCursor(0, 3); lcd.print("Tel.: 085-003 7446");
  } else if (displayPage == 1) { 
    lcd.setCursor(0, 0); lcd.print("Dev.  : "); lcd.print(fullHostname);
    lcd.setCursor(0, 1); lcd.print("Temp  : ");
    if(temperatureC == -127.00) { 
      lcd.print("Error"); 
    } else { 
      lcd.print(temperatureC, 1); lcd.print((char)223); lcd.print("C");
      lcd.setCursor(0, 2);
      if (temperatureC >= 10.0 && temperatureC <= 34.9) {
        lcd.print("Status: Normal");
        lcd.setCursor(0, 3); lcd.print("Range : 10.0-35.0");
      } else if (temperatureC >= 35.0 && temperatureC <= 50.0) {
        lcd.print("Status: Warm");
        lcd.setCursor(0, 3); lcd.print("Range : 35.0-50.0");
      } else if (temperatureC > 50.0) {
        lcd.print("Status: OverHeat");
        lcd.setCursor(0, 3); lcd.print("Range : Temp. > 50.0");
      } else {
        lcd.print("Status: Low Temp");
      }
    }
  } else if (displayPage == 2) { 
    lcd.setCursor(0, 0); lcd.print("Blynk:"); lcd.print((isOnline && Blynk.connected()) ? "Connected" : " Error");
    if (isOnline) {
      lcd.setCursor(0, 1); lcd.print("WiFi :"); lcd.print(WiFi.SSID().substring(0,12));
      lcd.setCursor(0, 2); lcd.print("IP   :"); lcd.print(WiFi.localIP().toString());
    } else {
      lcd.setCursor(0, 1); lcd.print("WiFi : Disconnected");
    }
    lcd.setCursor(0, 3); lcd.print("MC:"); lcd.print(macAddrStr); 
  }
}
