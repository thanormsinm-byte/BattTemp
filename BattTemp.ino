#define BLYNK_TEMPLATE_ID   "Legacy"
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

// --- [ ส่วนที่ 1: รายชื่อ 8 บอร์ด ] ---
String boardList[] = {"4948", "4A90", "59EC", "G7H8", "I9J0", "K1L2", "M3N4", "O5P6"};
int myID = 0; 
String myMacSuffix = ""; 

// --- [ ส่วนที่ 2: ข้อมูลการเชื่อมต่อ ] ---
char auth[] = "yXTSHdXVnNyDihE9zrzxQGE2JosxAQaa";
WebServer server(80);
HTTPUpdateServer httpUpdater;
bool isOnline = false; 

// --- [ ส่วนที่ 3: GitHub Update Configuration ] ---
float currentVersion = 1.2; 
const String versionURL = "https://raw.githubusercontent.com/thanormsinm-byte/BattTemp/main/version.json";
const String firmwareURL = "https://raw.githubusercontent.com/thanormsinm-byte/BattTemp/main/Batt.bin";

// --- [ ส่วนที่ 4: ฮาร์ดแวร์ ] ---
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

// --- [ หน้าเว็บ UI V5.0: Shared UI for Root & Update ] ---
String getSharedHTML(bool isUpdatePage) {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { font-family: sans-serif; text-align: center; color: #333; background-color: #fff; margin: 0; padding-top: 50px; }";
  html += ".container { max-width: 400px; margin: auto; padding: 20px; }";
  html += ".header { display: flex; align-items: center; justify-content: center; margin-bottom: 35px; }";
  html += ".logo { width: 32px; height: 32px; background: #000; position: relative; margin-right: 12px; }";
  html += ".logo::before { content: ''; position: absolute; top: 4px; left: 4px; right: 4px; bottom: 4px; border: 1px solid #fff; }";
  html += "h2 { font-size: 16px; font-weight: bold; margin: 0; letter-spacing: 1px; }";
  html += ".upload-area { border: 1.5px solid #e0e0e0; border-radius: 30px; padding: 8px 25px; margin: 0 auto 20px auto; display: inline-flex; align-items: center; justify-content: center; cursor: pointer; font-size: 14px; }";
  html += ".btn-submit { background: #007bff; color: #fff; border: none; padding: 10px 30px; border-radius: 20px; cursor: pointer; font-size: 13px; display: none; margin: 10px auto 25px auto; font-weight: bold; }";
  html += ".progress-container { width: 85%; background-color: #f3f3f3; border-radius: 25px; margin: 25px auto 10px auto; display: none; padding: 0px; height: 24px; position: relative; overflow: hidden; }";
  html += ".progress-bar { width: 0%; height: 100%; background-color: #007bff; border-radius: 25px; transition: width 0.1s; }";
  html += ".progress-text { position: absolute; width: 100%; top: 0; left: 0; line-height: 24px; font-size: 12px; font-weight: bold; color: #333; }";
  html += "#status-text { font-size: 12px; color: #007bff; margin-bottom: 35px; display: none; font-weight: bold; }";
  html += ".settings-group { margin-top: 50px; }"; 
  html += ".settings-title { font-size: 10px; color: #000; font-weight: bold; margin-bottom: 15px; display: block; letter-spacing: 1px; }";
  html += "table { width: 100%; border-collapse: collapse; }";
  html += "td { padding: 12px 5px; border-bottom: 1px solid #cccccc; text-align: left; font-size: 13px; color: #333; }"; 
  html += ".val { text-align: right; font-weight: bold; color: #f39c12; }"; 
  html += "input[type='file'] { display: none; }";
  html += "</style></head><body>";
  
  html += "<div class='container'>";
  html += "<div class='header'><div class='logo'></div><h2>" + String(isUpdatePage ? "UPDATE FIRMWARE" : "SYSTEM DASHBOARD") + "</h2></div>";
  
  html += "<form method='POST' action='/update' enctype='multipart/form-data' id='upload_form' onsubmit='startUpdate(event)'>";
  html += "<label for='file_input' class='upload-area'><span class='upload-icon'>📄</span> " + String(isUpdatePage ? "Select Bin File" : "Choose New Firmware") + "</label>";
  
  // ระบุ accept='.bin' เพื่อให้ Filter ตั้งต้นเป็นไฟล์ bin
  html += "<input type='file' name='update' id='file_input' accept='.bin' onchange='fileSelected()'>";
  
  html += "<input type='submit' class='btn-submit' id='submit_btn' value='Start Update Now'></form>";
  
  html += "<div class='progress-container' id='prg_container'><div class='progress-bar' id='prg_bar'></div><div class='progress-text' id='prg_percent'>0%</div></div>";
  html += "<div id='status-text'>Uploading...</div>";
  
  html += "<div class='settings-group'><span class='settings-title'>SETTINGS</span><table>";
  html += "<tr><td>SSID</td><td class='val'>" + WiFi.SSID() + "</td></tr>";
  html += "<tr><td>IP Address</td><td class='val'>" + WiFi.localIP().toString() + "</td></tr>";
  html += "<tr><td>Hardware ID</td><td class='val'>" + fullHostname + "</td></tr>";
  html += "<tr><td>Firmware Version</td><td class='val'>" + String(currentVersion, 1) + "</td></tr>";
  html += "</table></div></div>";
  
  html += "<script>";
  // ส่วนตรวจสอบนามสกุลไฟล์ที่เลือก
  html += "function fileSelected() {";
  html += "  var fileInput = document.getElementById('file_input');";
  html += "  var filePath = fileInput.value;";
  html += "  var allowedExtensions = /(\\.bin)$/i;"; // กำหนดให้รับแค่ .bin เท่านั้น
  html += "  if(!allowedExtensions.exec(filePath)) {";
  html += "    alert('กรุณาเลือกไฟล์นามสกุล .bin เท่านั้น!');";
  html += "    fileInput.value = '';";
  html += "    document.getElementById('submit_btn').style.display = 'none';";
  html += "    return false;";
  html += "  } else {";
  html += "    if(fileInput.files.length > 0) document.getElementById('submit_btn').style.display = 'block';";
  html += "  }";
  html += "}";

  html += "function startUpdate(e) { e.preventDefault(); var form = document.getElementById('upload_form'); var data = new FormData(form); var xhr = new XMLHttpRequest();";
  html += "document.getElementById('submit_btn').style.display = 'none'; document.getElementById('prg_container').style.display = 'block'; document.getElementById('status-text').style.display = 'block';";
  html += "xhr.upload.addEventListener('progress', function(evt) { if (evt.lengthComputable) { var per = Math.round((evt.loaded / evt.total) * 100);";
  html += "document.getElementById('prg_bar').style.width = per + '%'; document.getElementById('prg_percent').innerText = per + '%';";
  html += "if(per >= 100) { document.getElementById('status-text').innerText = 'Processing...'; }";
  html += "} }, false);";
  html += "xhr.onload = function() { if(xhr.status === 200) { document.getElementById('status-text').innerHTML = 'Update Success! Restarting...'; setTimeout(function() { window.location.href='/'; }, 5000); } else { document.getElementById('status-text').innerText = 'Update Failed!'; } };";
  html += "xhr.open('POST', '/update'); xhr.send(data); }</script></body></html>";
  return html;
}

// ฟังก์ชันเดิมของ GitHub Update (คงไว้ตามเดิม)
void checkGitHubUpdate() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure client; client.setInsecure(); 
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(client, versionURL);
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) { 
    String payload = http.getString();
    int verKeyPos = payload.indexOf("\"version\":");
    if (verKeyPos != -1) {
      int firstQuote = payload.indexOf("\"", verKeyPos + 10); 
      int secondQuote = payload.indexOf("\"", firstQuote + 1);
      String newVerStr = payload.substring(firstQuote + 1, secondQuote);
      float newVersion = newVerStr.toFloat();
      
      if (newVersion > currentVersion) {
        Serial.println("[OTA] Found New Fw.");
        lcd.clear(); lcd.setCursor(0, 0); lcd.print("   --- OTA ---");
        httpUpdate.rebootOnUpdate(true);
        httpUpdate.update(client, firmwareURL);
      }
    }
  }
  http.end();
}

String getEfuseMac() {
  uint64_t mac = ESP.getEfuseMac(); char macBuf[18];
  sprintf(macBuf, "%02X:%02X:%02X:%02X:%02X:%02X", (uint8_t)(mac >> 0), (uint8_t)(mac >> 8), (uint8_t)(mac >> 16), (uint8_t)(mac >> 24), (uint8_t)(mac >> 32), (uint8_t)(mac >> 40));
  return String(macBuf);
}

void startWiFiManager() {
  lcd.clear(); lcd.setCursor(0, 0); lcd.print("--- CONFIG MODE ---");
  WiFiManager wm; wm.setCaptivePortalEnable(true); wm.setConfigPortalTimeout(180); 
  server.stop();
  if (!wm.startConfigPortal(fullHostname.c_str())) { delay(500); }
  ESP.restart();
}

void setup() {
  Serial.begin(115200);
  
  Serial.println("--- SYSTEM BOOT ---");
  
  lcd.begin(); 
  lcd.backlight(); 
  lcd.setCursor(0, 0); lcd.print("--- SYSTEM BOOT ---");
  lcd.setCursor(0, 1); lcd.print("   Hardwar Int.");
  
  pinMode(LedHeartBeat, OUTPUT); 
  pinMode(ledPin, OUTPUT); 
  pinMode(configButton, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  WiFi.begin();
  delay(300);

  macAddrStr = getEfuseMac();
  uint64_t mac = ESP.getEfuseMac(); char hexStr[5];
  sprintf(hexStr, "%02X%02X", (uint8_t)(mac >> 32), (uint8_t)(mac >> 40)); 
  myMacSuffix = String(hexStr);

  for (int i = 0; i < 8; i++) {
    if (myMacSuffix.equalsIgnoreCase(boardList[i])) { myID = i + 1; break; }
  }
  
  fullHostname = (myID > 0 ? String(myID) : "N/A") + "-Batt-" + myMacSuffix;
  lcd.setCursor(0, 2); lcd.print("Connect to WiFi...");
  Serial.println("Connect to WiFi");

  // บรรทัดที่ 4 ดึงชื่อ WiFi มาแสดง (ถ้ามี)
  lcd.setCursor(0, 3);
  String targetSSID = WiFi.SSID(); 
  if (targetSSID != "") {
    lcd.print("SSID: " + targetSSID.substring(0, 14)); // ตัดชื่อไม่ให้ยาวเกินหน้าจอ
    Serial.println("SSID: " + targetSSID.substring(0, 14));
  } else {
    lcd.print("SSID: Not Found!"); // กรณีไม่เคยตั้งค่า WiFi เลย
    Serial.println("SSID: Not Found!"); // กรณีไม่เคยตั้งค่า WiFi เลย
  }
 
  WiFiManager wm;
  wm.setConfigPortalTimeout(60); 

  // Callback เมื่อต้องเข้าโหมดตั้งค่า
  wm.setAPCallback([](WiFiManager *myWiFiManager) {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("--- CONFIG MODE ---");
    lcd.setCursor(0, 1); lcd.print("SSID: " + String(myWiFiManager->getConfigPortalSSID()));
    lcd.setCursor(0, 2); lcd.print("IP  : 192.168.4.1");
    lcd.setCursor(0, 3); lcd.print("Setup WiFi Now!");
    
    Serial.println("--- CONFIG MODE ---");
  });

  if (wm.autoConnect(fullHostname.c_str())) {
    isOnline = true;
    ArduinoOTA.setHostname(fullHostname.c_str()); ArduinoOTA.begin();
    Blynk.config(auth, "myplant.ddns.net", 8080); Blynk.connect(); 

    // --- [ ส่วนสำคัญ: ปรับแต่ง Web Server ] ---
    // หน้าหลักใช้ UI แบบใหม่
    server.on("/", HTTP_GET, []() {
      server.send(200, "text/html", getSharedHTML(false));
    });

    // หน้าอัปเดต ใช้ UI แบบใหม่เหมือนกัน (ส่ง true ไปบอกว่าเป็นหน้า Update)
    server.on("/update", HTTP_GET, []() {
      server.send(200, "text/html", getSharedHTML(true));
    });

    // ให้ httpUpdater จัดการเฉพาะฝั่งรับไฟล์ (POST)
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
    if (millis() - buttonPressTime > 3000) { startWiFiManager(); }
  } else { buttonPressTime = 0; }

  if (WiFi.status() == WL_CONNECTED) {
    if (!isOnline) { isOnline = true; checkGitHubUpdate(); }
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
    displayPage++;
    if (displayPage > 2) displayPage = 0; 
    if (displayPage == 2 && isOnline) { checkGitHubUpdate(); }
    lcd.clear(); 
  }

 // แสดงผลหน้าจอ

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
    lcd.setCursor(0, 0); lcd.print("Blynk:"); lcd.print((isOnline && Blynk.connected()) ? "Connected" : "Error");
    if (isOnline) {
      lcd.setCursor(0, 1); lcd.print("WiFi :"); lcd.print(WiFi.SSID().substring(0,12));
      lcd.setCursor(0, 2); lcd.print("IP   :"); lcd.print(WiFi.localIP().toString());
    } else {
      lcd.setCursor(0, 1); lcd.print("WiFi : Disconnected");
    }
    lcd.setCursor(0, 3); lcd.print("MC:"); lcd.print(macAddrStr); 
  }

}
