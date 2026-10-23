#include <Arduino.h>
#include <WiFiManager.h> 
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>

// ---------------------------------------------------------
// ⚠️ 1. ตั้งค่า LINE Token
// ---------------------------------------------------------
String lineToken = "pz3ShA29h+6rFZUlYpKx3co28e+WAJYpb12Sw8BeKQ0ZSPowJHNROxIDy1dANzmSs7lf7FAK6fjOb7R/ZxHUDCsgVd1uSLeM10idYrX4VtJoB0UiM8uJ306DsdUtC69lN2DOvPihe+xFCa+cC+Sm7QdB04t89/1O/w1cDnyilFU="; 

// ---------------------------------------------------------
// ⚠️ 2. ตั้งค่า OTA (นี่คือบรรทัดที่บอกว่าอยู่เวอร์ชันไหน)
// ---------------------------------------------------------
const float currentVersion = 1.0; 

// เปลี่ยน 192.168.X.X เป็น IP Address ของคอมพิวเตอร์คุณ Zen 
String jsonUrl = "http://172.20.10.4:8000/version.json";
String binUrl = "http://172.20.10.4:8000/firmware.bin";

int countA2 = 0;

// ---------------------------------------------------------
// ฟังก์ชันส่ง LINE
// ---------------------------------------------------------
void sendLineMessage(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("https://api.line.me/v2/bot/message/broadcast");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + lineToken);
    
    String payload = "{\"messages\":[{\"type\":\"text\",\"text\":\"" + message + "\"}]}";
    int httpResponseCode = http.POST(payload);
    
    if (httpResponseCode == 200) {
      Serial.println("LINE Message ส่งสำเร็จ!");
    } else {
      Serial.print("ส่ง LINE พลาด Error code: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  }
}

// ---------------------------------------------------------
// ฟังก์ชันสำหรับดาวน์โหลดและอัปเดต Firmware (OTA)
// ---------------------------------------------------------
void performOTA() {
  Serial.println("กำลังดาวน์โหลด Firmware ใหม่...");
  HTTPClient http;
  http.begin(binUrl);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    int contentLength = http.getSize();
    bool canBegin = Update.begin(contentLength);
    if (canBegin) {
      Serial.println("เริ่มเขียน Firmware...");
      size_t written = Update.writeStream(http.getStream());
      
      if (written == contentLength) {
        Serial.println("เขียน Firmware สำเร็จ! ระบบจะรีสตาร์ท...");
        if (Update.end()) {
          ESP.restart(); // รีสตาร์ทบอร์ด
        }
      } else {
        Serial.println("เขียน Firmware ไม่ครบ!");
      }
    }
  } else {
    Serial.println("ดาวน์โหลดไฟล์ .bin ล้มเหลว");
  }
  http.end();
}

// ---------------------------------------------------------
// FreeRTOS Tasks
// ---------------------------------------------------------
void TaskA1(void *pvParameters) {
  for (;;) {
    Serial.println("A1");
    vTaskDelay(pdMS_TO_TICKS(2000)); 
  }
}

void TaskA2(void *pvParameters) {
  for (;;) {
    Serial.println("A2");
    countA2++; 
    if (countA2 == 5) {
      Serial.println("ครบ 5 ครั้งแล้ว! (กำลังส่ง LINE...)");
      sendLineMessage("แจ้งเตือน: ตรวจพบการทำงานของ A2 ครบ 5 ครั้ง");
      countA2 = 0; 
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

void TaskOTA(void *pvParameters) {
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("ตรวจสอบอัปเดต OTA...");
      HTTPClient http;
      http.begin(jsonUrl);
      int httpCode = http.GET();
      
      if (httpCode == 200) {
        String payload = http.getString();
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, payload);
        float newVersion = doc["version"];
        
        Serial.print("เวอร์ชันปัจจุบัน: "); Serial.println(currentVersion);
        Serial.print("เวอร์ชันบนเซิร์ฟเวอร์: "); Serial.println(newVersion);
        
        if (newVersion > currentVersion) {
          Serial.println("พบเวอร์ชันใหม่! เริ่มกระบวนการ OTA...");
          performOTA();
        } else {
          Serial.println("เฟิร์มแวร์เป็นเวอร์ชันล่าสุดแล้ว");
        }
      } else {
        Serial.println("ไม่สามารถโหลดไฟล์ JSON ได้ (เช็กเว็บเซิร์ฟเวอร์)");
      }
      http.end();
    }
    vTaskDelay(pdMS_TO_TICKS(30000)); // เช็กทุก 30 วินาที
  }
}

void setup() {
  Serial.begin(115200);

  WiFiManager wm;
  Serial.println("กำลังเชื่อมต่อ Wi-Fi...");
  bool res = wm.autoConnect("Zen_ESP32_Setup");

  if (!res) {
    Serial.println("เชื่อมต่อไม่สำเร็จ รีสตาร์ท...");
    ESP.restart();
  }

  Serial.println("เชื่อมต่อ Wi-Fi สำเร็จแล้ว!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  xTaskCreatePinnedToCore(TaskA1, "Task_A1", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(TaskA2, "Task_A2", 8192, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(TaskOTA, "Task_OTA", 8192, NULL, 1, NULL, 1);
}

void loop() {
}