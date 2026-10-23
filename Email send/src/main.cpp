#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <ESP_Mail_Client.h>

// ==================== ตั้งค่า WiFi ====================
#define WIFI_SSID "Zen"
#define WIFI_PASSWORD "0954686204zen25"

// ==================== ตั้งค่า Email (SMTP) ====================
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465 // ใช้ SSL

// อีเมลผู้ส่ง (ต้องใช้ App Password ของ Gmail)
#define AUTHOR_EMAIL "worldzornno.1@gmail.com"
#define AUTHOR_PASSWORD "dlqp ghpc vjfu gsbn" 

// อีเมลผู้รับ
#define RECIPIENT_EMAIL "qwerty258uujv@gmail.com"

// ==================== ตั้งค่า เวลา (NTP) ====================
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600; // เวลาประเทศไทย (UTC+7)
const int daylightOffset_sec = 0;

// ==================== โครงสร้างข้อมูล ====================
struct SensorData {
  String dateTime;
  int value1;
  int value2;
};

const int MAX_SAMPLES = 10;
SensorData dataLog[MAX_SAMPLES];
int sampleCount = 0;

// ตัวแปรจับเวลา
unsigned long previousMillis = 0;
const long interval = 2000; // 2 วินาที

// Object สำหรับส่ง Email
SMTPSession smtp;

// ----------------- ประกาศฟังก์ชัน (Function Prototypes) -----------------
String getDateTime();
void sendEmailWithData();

void setup() {
  Serial.begin(115200);
  
  // สุ่มเมล็ดสุ่มค่าแบบสุ่มจริงจากขา Analog ที่ลอยอยู่
  randomSeed(analogRead(0));

  // เชื่อมต่อ WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  // ซิงค์เวลาจาก NTP Server
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Waiting for NTP time sync...");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nTime Synchronized!");
}

void loop() {
  unsigned long currentMillis = millis();

  // ทำงานทุกๆ 2 วินาที
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    if (sampleCount < MAX_SAMPLES) {
      // 1. สุ่มค่า 2 ค่า
      int val1 = random(10, 100);
      int val2 = random(100, 500);
      String currentTime = getDateTime();

      // 2. เก็บลง Array
      dataLog[sampleCount].dateTime = currentTime;
      dataLog[sampleCount].value1 = val1;
      dataLog[sampleCount].value2 = val2;

      Serial.printf("[%d/%d] Saved: %s | Val1: %d | Val2: %d\n", 
                    sampleCount + 1, MAX_SAMPLES, 
                    currentTime.c_str(), val1, val2);

      sampleCount++;
    }

    // 3. เมื่อครบ 10 ค่า ให้ส่งอีเมลและรีเซ็ต
    if (sampleCount >= MAX_SAMPLES) {
      Serial.println("\nครบ 10 ค่าแล้ว กำลังส่ง Email...");
      sendEmailWithData();

      // รีเซ็ตเพื่อเริ่มรอบใหม่
      sampleCount = 0;
      Serial.println("รีเซ็ตการเก็บข้อมูล เริ่มต้นรอบใหม่...\n");
    }
  }
}

// ----------------- นิยามฟังก์ชัน -----------------

String getDateTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "N/A";
  }
  char timeStringBuff[50];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeStringBuff);
}

void sendEmailWithData() {
  smtp.debug(0); // ถ้าต้องการดู log ว่าอีเมลติดปัญหาอะไร ให้เปลี่ยนเป็น 1

  ESP_Mail_Session session;
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_EMAIL;
  session.login.password = AUTHOR_PASSWORD;
  session.login.user_domain = "";

  SMTP_Message message;
  message.sender.name = "ESP32 Data Logger";
  message.sender.email = AUTHOR_EMAIL;
  message.subject = "รายงานข้อมูล ESP32 (สะสมครบ 10 ค่า)";
  message.addRecipient("User", RECIPIENT_EMAIL);

  // สร้างตาราง HTML ในเนื้อหา Email
  String emailBody = "<h2>รายงานข้อมูลจาก ESP32</h2>";
  emailBody += "<table border='1' cellpadding='5' style='border-collapse: collapse;'>";
  emailBody += "<tr><th>#</th><th>Date Time</th><th>Value 1</th><th>Value 2</th></tr>";

  for (int i = 0; i < MAX_SAMPLES; i++) {
    emailBody += "<tr>";
    emailBody += "<td>" + String(i + 1) + "</td>";
    emailBody += "<td>" + dataLog[i].dateTime + "</td>";
    emailBody += "<td>" + String(dataLog[i].value1) + "</td>";
    emailBody += "<td>" + String(dataLog[i].value2) + "</td>";
    emailBody += "</tr>";
  }
  emailBody += "</table>";

  message.html.content = emailBody.c_str();
  message.text.content = ""; 

  if (!smtp.connect(&session)) {
    Serial.printf("Connection error, Status Code: %d, Error Code: %d, Reason: %s\n", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
    return;
  }

  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.printf("Error sending Email, Status Code: %d, Error Code: %d, Reason: %s\n", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
  } else {
    Serial.println(">>> ส่ง Email สำเร็จเรียบร้อย! <<<");
  }
}