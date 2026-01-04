/*
 * HỆ THỐNG AN NINH ĐA LỚP VÀ GIÁM SÁT MÔI TRƯỜNG THÔNG MINH
 * Multi-Layer Security & Smart Environment Monitoring System
 * ============================================================
 * 
 * Chức năng:
 * A. Kiểm soát An ninh & Ra vào
 *    - Khóa cửa điện tử (Relay)
 *    - Xác thực mật khẩu (Keypad 4x4)
 *    - Xác thực vân tay (AS608)
 *    - Chế độ thường: Vân tay HOẶC Mật khẩu
 *    - Chế độ bảo mật cao (2FA): Vân tay VÀ Mật khẩu
 *    - Chống dò mật mã (khóa sau 3 lần sai)
 * 
 * B. Tự động hóa & Chiếu sáng thông minh
 *    - Tự động bật đèn khi trời tối (LDR)
 *    - Phát hiện âm thanh -> bật đèn/thông báo "Có khách"
 * 
 * C. Giám sát Môi trường
 *    - Đo nhiệt độ/độ ẩm liên tục (DHT11)
 *    - Cảnh báo quá nhiệt (>40°C) -> ngắt thiết bị
 * 
 * D. Giao diện người dùng (LCD 16x2)
 * 
 * Hướng dẫn sử dụng phím:
 *    A: Chuyển chế độ Thường/Bảo mật cao (2FA)
 *    B: Xem thông tin cảm biến
 *    C: Xóa mật khẩu đang nhập
 *    D: Đổi mật khẩu (nhập mật khẩu cũ trước, rồi nhấn D)
 *    #: Xác nhận mật khẩu
 *    *: Quay lại màn hình chính / VÀO ADMIN MENU (sau khi nhập đúng MK)
 *    0-9: Nhập mật khẩu
 * 
 * ADMIN MENU (nhập đúng mật khẩu + nhấn *):
 *    1: Thêm vân tay mới
 *    2: Xóa vân tay theo ID
 *    3: Xóa TẤT CẢ vân tay
 *    4: Xem số vân tay đã lưu
 *    *: Thoát Admin Menu
 */

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Adafruit_Fingerprint.h>
#include <DHT.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ==================== WIFI & GOOGLE SHEETS ====================
// Thay đổi thông tin WiFi của bạn
const char* WIFI_SSID = "the7w";       // Tên WiFi
const char* WIFI_PASSWORD = "Dk@17092004";  // Mật khẩu WiFi

// Google Apps Script Web App URL 
const char* GOOGLE_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbxpWsXbtsYM9pqvpQ1TKvVOsREGitNTL8hjFoy099yIT25H9sNSvytg11tf-HpvJYTo/exec";

// ==================== PIN DEFINITIONS ====================
// LCD I2C
#define LCD_SDA 21
#define LCD_SCL 22

// Fingerprint Sensor (UART2)
#define FINGER_RX 16
#define FINGER_TX 17

// Keypad 4x4
#define ROW1 13
#define ROW2 12
#define ROW3 14
#define ROW4 27
#define COL1 26
#define COL2 25
#define COL3 33
#define COL4 32

// Relay (Door Lock) - Hiện không dùng vì chỉ có 1 relay
#define RELAY_PIN 4

// LDR Light Sensor
#define LDR_ANALOG 34
#define LDR_DIGITAL 35

// Sound Sensor
#define SOUND_PIN 5

// DHT11
#define DHT_PIN 15
#define DHT_TYPE DHT11

// LED (Đèn chiếu sáng tự động)
#define LED_PIN 18

// LED CỬA (Sáng khi mở cửa)
#define DOOR_LED_PIN 19

// LED ÂM THANH (Sáng khi có âm thanh)
#define SOUND_LED_PIN 23

// FAN/MOTOR (Quạt làm mát) - Relay nối vào D4 (cùng relay cửa)
#define FAN_PIN 4

// ==================== SYSTEM SETTINGS ====================
// Mật khẩu mặc định
#define DEFAULT_ADMIN_PASSWORD "1234"
#define DEFAULT_USER_PASSWORD "0000"

// Ngưỡng cảnh báo nhiệt độ (°C)
#define TEMP_WARNING_THRESHOLD 40.0

// Ngưỡng bật quạt làm mát (°C)
#define TEMP_FAN_THRESHOLD 30.0

// Ngưỡng ánh sáng (giá trị thấp = sáng, cao = tối)
#define LDR_DARK_THRESHOLD 2500

// Số lần nhập sai tối đa
#define MAX_WRONG_ATTEMPTS 3

// Thời gian khóa sau khi nhập sai (ms)
#define LOCKOUT_TIME 30000

// Thời gian mở cửa (ms)
#define DOOR_OPEN_TIME 5000

// Thời gian đèn sáng khi có âm thanh (ms)
#define SOUND_LIGHT_DURATION 10000

// ==================== OBJECTS ====================
LiquidCrystal_I2C lcd(0x27, 16, 2);

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {ROW1, ROW2, ROW3, ROW4};
byte colPins[COLS] = {COL1, COL2, COL3, COL4};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

HardwareSerial fingerSerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fingerSerial);

DHT dht(DHT_PIN, DHT_TYPE);

// ==================== SYSTEM VARIABLES ====================
// Mật khẩu hiện tại
String adminPassword = DEFAULT_ADMIN_PASSWORD;
String userPassword = DEFAULT_USER_PASSWORD;
String inputPassword = "";

// Trạng thái hệ thống
bool highSecurityMode = false;      // Chế độ bảo mật cao (2FA)
bool doorUnlocked = false;          // Trạng thái cửa
bool systemLocked = false;          // Hệ thống bị khóa do nhập sai
bool overheated = false;            // Trạng thái quá nhiệt
bool guestDetected = false;         // Phát hiện có khách

// Xác thực 2FA
bool passwordVerified = false;      // Đã xác thực mật khẩu (cho 2FA)
bool fingerprintVerified = false;   // Đã xác thực vân tay (cho 2FA)

// Đếm số lần sai
int wrongAttempts = 0;              // Số lần nhập mật khẩu sai
int wrongFingerprintAttempts = 0;   // Số lần quét vân tay sai
bool fingerprintLocked = false;     // Vân tay bị khóa sau 3 lần sai

// Thời gian
unsigned long lockoutStartTime = 0;
unsigned long doorOpenStartTime = 0;
unsigned long soundLightStartTime = 0;
unsigned long lastSensorReadTime = 0;
unsigned long lastDisplayUpdateTime = 0;
unsigned long twoFactorStartTime = 0;

// Dữ liệu cảm biến
float temperature = 0;
float humidity = 0;
int lightLevel = 0;
bool isDark = false;
bool soundLightOn = false;
bool fanRunning = false;         // Trạng thái quạt

// Trạng thái hiển thị
enum DisplayState {
  DISPLAY_WELCOME,
  DISPLAY_ENTER_PASSWORD,
  DISPLAY_SCAN_FINGER,
  DISPLAY_2FA_PASSWORD,
  DISPLAY_2FA_FINGER,
  DISPLAY_ACCESS_GRANTED,
  DISPLAY_ACCESS_DENIED,
  DISPLAY_SYSTEM_LOCKED,
  DISPLAY_OVERHEAT_WARNING,
  DISPLAY_GUEST_DETECTED,
  DISPLAY_SENSOR_INFO
};
DisplayState currentDisplay = DISPLAY_WELCOME;

// ==================== FUNCTION PROTOTYPES ====================
void initSystem();
void readSensors();
void handleKeypad();
void handleFingerprint();
void handleAutomation();
void handleOverheatProtection();
void updateDisplay();
void unlockDoor();
void lockDoor();
void resetAuthentication();
void switchSecurityMode();
void showMessage(const char* line1, const char* line2, int delayMs = 2000);

// Admin functions
void adminMenu();
bool enrollFingerprint(uint8_t id);
bool deleteFingerprint(uint8_t id);
void deleteAllFingerprints();
void showFingerprintCount();

// WiFi & Google Sheets functions
void connectWiFi();
void sendToGoogleSheets(String event, String method, String user, String status);

// ==================== INITIALIZATION ====================
void initSystem() {
  Serial.begin(115200);
  Serial.println("\n");
  Serial.println("╔════════════════════════════════════════════════════════╗");
  Serial.println("║   HỆ THỐNG AN NINH ĐA LỚP ESP32                        ║");
  Serial.println("║   Multi-Layer Security System                          ║");
  Serial.println("╚════════════════════════════════════════════════════════╝");
  Serial.println();
  
  // Khởi tạo I2C và LCD
  Wire.begin(LCD_SDA, LCD_SCL);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Security System");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  
  // Khởi tạo GPIO
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(DOOR_LED_PIN, OUTPUT);
  pinMode(SOUND_LED_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(LDR_DIGITAL, INPUT);
  pinMode(SOUND_PIN, INPUT);
  
  // Đảm bảo cửa khóa, đèn tắt, quạt tắt
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(DOOR_LED_PIN, LOW);
  digitalWrite(SOUND_LED_PIN, LOW);
  digitalWrite(FAN_PIN, LOW);
  
  // Khởi tạo DHT11
  dht.begin();
  
  // Kết nối WiFi
  connectWiFi();
  
  // Khởi tạo cảm biến vân tay
  fingerSerial.begin(57600, SERIAL_8N1, FINGER_RX, FINGER_TX);
  delay(100);
  
  if (finger.verifyPassword()) {
    Serial.println("✓ Cảm biến vân tay: OK");
    finger.getTemplateCount();
    Serial.print("  Số vân tay đã lưu: ");
    Serial.println(finger.templateCount);
  } else {
    Serial.println("✗ Cảm biến vân tay: Không tìm thấy!");
  }
  
  delay(1000);
  
  // Hiển thị hướng dẫn
  Serial.println("\n╔═══════════════ HƯỚNG DẪN SỬ DỤNG ═══════════════╗");
  Serial.println("║ Phím A: Chuyển chế độ Thường/Bảo mật cao (2FA)  ║");
  Serial.println("║ Phím B: Xem thông tin cảm biến                  ║");
  Serial.println("║ Phím C: Xóa mật khẩu đang nhập                  ║");
  Serial.println("║ Phím D: Đổi mật khẩu (nhập MK cũ trước)         ║");
  Serial.println("║ Phím #: Xác nhận mật khẩu                       ║");
  Serial.println("║ Phím *: Quay lại / VÀO ADMIN (sau khi nhập MK)  ║");
  Serial.println("║ 0-9  : Nhập mật khẩu                            ║");
  Serial.println("╠═══════════════════════════════════════════════════╣");
  Serial.println("║ ADMIN MENU (nhập đúng MK + nhấn *):              ║");
  Serial.println("║   1: Thêm vân tay | 2: Xóa vân tay               ║");
  Serial.println("║   3: Xóa tất cả   | 4: Xem số vân tay đã lưu     ║");
  Serial.println("╚═════════════════════════════════════════════════╝");
  Serial.println("\n╔═══════════════ MẬT KHẨU MẶC ĐỊNH ════════════════╗");
  Serial.println("║ Admin: " + String(DEFAULT_ADMIN_PASSWORD) + " (mở cửa + vào Admin Menu)       ║");
  Serial.println("║ User:  " + String(DEFAULT_USER_PASSWORD) + " (chỉ mở cửa)                    ║");
  Serial.println("╚═════════════════════════════════════════════════════╝");
  Serial.println("Chế độ hiện tại: THƯỜNG (Vân tay HOẶC Mật khẩu)\n");
  
  currentDisplay = DISPLAY_WELCOME;
}

// ==================== SENSOR READING ====================
void readSensors() {
  if (millis() - lastSensorReadTime < 2000) return;
  lastSensorReadTime = millis();
  
  // Đọc DHT11
  float newTemp = dht.readTemperature();
  float newHum = dht.readHumidity();
  
  if (!isnan(newTemp) && !isnan(newHum)) {
    temperature = newTemp;
    humidity = newHum;
  }
  
  // Đọc LDR
  lightLevel = analogRead(LDR_ANALOG);
  isDark = (lightLevel > LDR_DARK_THRESHOLD);
  
  // In ra Serial (mỗi 2 giây)
  Serial.printf("[Sensors] Temp: %.1f°C | Hum: %.0f%% | Light: %d (%s) | Fan: %s\n", 
                temperature, humidity, lightLevel, isDark ? "Tối" : "Sáng", fanRunning ? "ON" : "OFF");
}

// ==================== KEYPAD HANDLING ====================
void handleKeypad() {
  char key = keypad.getKey();
  if (!key) return;
  
  Serial.print("[Keypad] Phím: ");
  Serial.println(key);
  
  // Nếu hệ thống bị khóa
  if (systemLocked) {
    if (millis() - lockoutStartTime >= LOCKOUT_TIME) {
      systemLocked = false;
      wrongAttempts = 0;
      showMessage("System Unlocked", "Try again");
      currentDisplay = DISPLAY_WELCOME;
    } else {
      int remainingSec = (LOCKOUT_TIME - (millis() - lockoutStartTime)) / 1000;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("System Locked!");
      lcd.setCursor(0, 1);
      lcd.print("Wait ");
      lcd.print(remainingSec);
      lcd.print(" sec");
    }
    return;
  }
  
  // Nếu đang quá nhiệt
  if (overheated) {
    showMessage("OVERHEAT!", "System disabled");
    return;
  }
  
  // Xử lý phím chức năng
  switch (key) {
    case 'A':  // Chuyển đổi chế độ bảo mật
      switchSecurityMode();
      return;
      
    case 'B':  // Hiển thị thông tin cảm biến
      currentDisplay = DISPLAY_SENSOR_INFO;
      return;
      
    case 'C':  // Xóa mật khẩu đang nhập
      inputPassword = "";
      if (highSecurityMode && passwordVerified) {
        currentDisplay = DISPLAY_2FA_FINGER;
      } else if (highSecurityMode) {
        currentDisplay = DISPLAY_2FA_PASSWORD;
      } else {
        currentDisplay = DISPLAY_ENTER_PASSWORD;
      }
      showMessage("Password cleared", "", 1000);
      return;
      
    case 'D':  // Đổi mật khẩu (chỉ Admin mới đổi được)
      if (inputPassword == adminPassword) {
        showMessage("Change which?", "1:Admin 2:User");
        
        // Chờ chọn loại mật khẩu
        unsigned long waitStart = millis();
        char choice = 0;
        while (millis() - waitStart < 5000) {
          choice = keypad.getKey();
          if (choice == '1' || choice == '2') break;
        }
        
        if (choice != '1' && choice != '2') {
          showMessage("Timeout!", "");
          inputPassword = "";
          currentDisplay = DISPLAY_WELCOME;
          return;
        }
        
        bool changingAdmin = (choice == '1');
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(changingAdmin ? "New ADMIN pass:" : "New USER pass:");
        
        String newPass = "";
        unsigned long startTime = millis();
        
        while (millis() - startTime < 15000) {
          char k = keypad.getKey();
          if (k) {
            if (k == '#') {
              if (newPass.length() >= 4) {
                if (changingAdmin) {
                  adminPassword = newPass;
                  Serial.println("[System] Mật khẩu ADMIN đã đổi thành: " + adminPassword);
                } else {
                  userPassword = newPass;
                  Serial.println("[System] Mật khẩu USER đã đổi thành: " + userPassword);
                }
                showMessage("Password changed", "Success!");
              } else {
                showMessage("Too short!", "Min 4 digits");
              }
              break;
            } else if (k == '*') {
              showMessage("Cancelled", "");
              break;
            } else if (k >= '0' && k <= '9') {
              newPass += k;
              lcd.setCursor(0, 1);
              for (unsigned int i = 0; i < newPass.length(); i++) lcd.print('*');
              lcd.print("        ");
            }
          }
        }
        inputPassword = "";
        currentDisplay = DISPLAY_WELCOME;
      } else if (inputPassword == userPassword) {
        showMessage("No permission!", "Need Admin pass");
        inputPassword = "";
      } else {
        showMessage("Enter Admin pass", "first, then D");
      }
      return;
      
    case '*':  // Quay lại màn hình chính HOẶC vào Admin Menu
      if (inputPassword == adminPassword) {
        // Mật khẩu ADMIN đúng -> vào Admin Menu
        Serial.println("[Admin] Vào Admin Menu với quyền ADMIN...");
        inputPassword = "";
        adminMenu();
        currentDisplay = DISPLAY_WELCOME;
      } else if (inputPassword == userPassword) {
        // Mật khẩu USER -> KHÔNG cho vào Admin
        Serial.println("[Auth] Mật khẩu User không có quyền Admin!");
        showMessage("No Admin access", "User password!", 2000);
        inputPassword = "";
        currentDisplay = DISPLAY_WELCOME;
      } else {
        // Quay lại màn hình chính
        inputPassword = "";
        resetAuthentication();
        currentDisplay = DISPLAY_WELCOME;
        guestDetected = false;
      }
      return;
      
    case '#':  // Xác nhận mật khẩu
      if (inputPassword.length() > 0) {
        // Kiểm tra mật khẩu Admin hoặc User
        bool isAdmin = (inputPassword == adminPassword);
        bool isUser = (inputPassword == userPassword);
        
        if (isAdmin || isUser) {
          Serial.printf("[Auth] ✓ Mật khẩu %s đúng!\n", isAdmin ? "ADMIN" : "USER");
          
          // Nếu vân tay bị khóa -> mở cửa bằng mật khẩu
          if (fingerprintLocked) {
            fingerprintLocked = false;
            wrongFingerprintAttempts = 0;
            wrongAttempts = 0;
            Serial.println("[Auth] ✓ Vân tay bị khóa - Mở cửa bằng mật khẩu!");
            sendToGoogleSheets("DOOR_OPEN", "PASSWORD", isAdmin ? "ADMIN" : "USER", "SUCCESS_AFTER_FINGER_LOCK");
            unlockDoor();  // Mở cửa (đèn sáng)
          } else if (highSecurityMode) {
            // Chế độ 2FA: cần thêm vân tay
            passwordVerified = true;
            twoFactorStartTime = millis();
            showMessage("Password OK!", "Scan finger...");
            currentDisplay = DISPLAY_2FA_FINGER;
          } else {
            // Chế độ thường: mở cửa ngay
            wrongAttempts = 0;
            sendToGoogleSheets("DOOR_OPEN", "PASSWORD", isAdmin ? "ADMIN" : "USER", "SUCCESS");
            unlockDoor();
          }
        } else {
          // Mật khẩu sai (không phải Admin cũng không phải User)
          Serial.println("[Auth] ✗ Mật khẩu sai!");
          wrongAttempts++;
          
          // Gửi log thất bại
          sendToGoogleSheets("DOOR_OPEN", "PASSWORD", "Unknown", "FAILED");
          
          if (wrongAttempts >= MAX_WRONG_ATTEMPTS) {
            systemLocked = true;
            lockoutStartTime = millis();
            showMessage("3 wrong tries!", "Locked 30 sec");
            currentDisplay = DISPLAY_SYSTEM_LOCKED;
            Serial.println("[Security] !!! HỆ THỐNG BỊ KHÓA 30 GIÂY !!!");
            sendToGoogleSheets("SYSTEM_LOCKED", "PASSWORD", "Unknown", "LOCKED_3_ATTEMPTS");
          } else {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Wrong password!");
            lcd.setCursor(0, 1);
            lcd.print("Attempts: ");
            lcd.print(wrongAttempts);
            lcd.print("/3");
            delay(2000);
            currentDisplay = highSecurityMode ? DISPLAY_2FA_PASSWORD : DISPLAY_ENTER_PASSWORD;
          }
        }
        inputPassword = "";
      }
      return;
      
    default:  // Phím số 0-9
      if (key >= '0' && key <= '9') {
        if (currentDisplay == DISPLAY_WELCOME || currentDisplay == DISPLAY_SENSOR_INFO) {
          currentDisplay = highSecurityMode ? DISPLAY_2FA_PASSWORD : DISPLAY_ENTER_PASSWORD;
        }
        
        if (inputPassword.length() < 10) {
          inputPassword += key;
          
          // Hiển thị dấu *
          lcd.setCursor(0, 1);
          lcd.print("Pass: ");
          for (unsigned int i = 0; i < inputPassword.length(); i++) {
            lcd.print('*');
          }
          lcd.print("      ");
        }
      }
      return;
  }
}

// ==================== FINGERPRINT HANDLING ====================
void handleFingerprint() {
  // Không xử lý nếu hệ thống bị khóa hoặc quá nhiệt
  if (systemLocked || overheated) return;
  
  // Không xử lý nếu vân tay bị khóa (cần nhập mật khẩu)
  if (fingerprintLocked) return;
  
  // Kiểm tra timeout 2FA (30 giây)
  if (highSecurityMode && (passwordVerified || fingerprintVerified)) {
    if (millis() - twoFactorStartTime > 30000) {
      showMessage("2FA Timeout!", "Try again");
      resetAuthentication();
      currentDisplay = DISPLAY_WELCOME;
      Serial.println("[2FA] Timeout - đã hết thời gian xác thực");
      return;
    }
  }
  
  // Đọc vân tay
  int result = finger.getImage();
  if (result != FINGERPRINT_OK) return;
  
  result = finger.image2Tz();
  if (result != FINGERPRINT_OK) return;
  
  result = finger.fingerSearch();
  
  if (result == FINGERPRINT_OK) {
    Serial.printf("[Auth] ✓ Vân tay khớp! ID: %d | Độ tin cậy: %d\n", 
                  finger.fingerID, finger.confidence);
    
    // Reset số lần sai vân tay
    wrongFingerprintAttempts = 0;
    
    if (highSecurityMode) {
      if (passwordVerified) {
        // 2FA hoàn tất - đã có mật khẩu, giờ có vân tay
        fingerprintVerified = true;
        wrongAttempts = 0;
        Serial.println("[2FA] ✓ Xác thực 2 lớp hoàn tất!");
        sendToGoogleSheets("DOOR_OPEN", "2FA", "Finger_ID_" + String(finger.fingerID), "SUCCESS");
        unlockDoor();
      } else {
        // Chưa nhập mật khẩu - quét vân tay trước
        fingerprintVerified = true;
        twoFactorStartTime = millis();
        showMessage("Finger OK!", "Enter password");
        currentDisplay = DISPLAY_2FA_PASSWORD;
        Serial.println("[2FA] Vân tay OK, chờ mật khẩu...");
      }
    } else {
      // Chế độ thường: mở cửa ngay
      wrongAttempts = 0;
      sendToGoogleSheets("DOOR_OPEN", "FINGERPRINT", "Finger_ID_" + String(finger.fingerID), "SUCCESS");
      unlockDoor();
    }
  } else if (result == FINGERPRINT_NOTFOUND) {
    Serial.println("[Auth] ✗ Vân tay không khớp!");
    
    // Tăng số lần sai vân tay
    wrongFingerprintAttempts++;
    Serial.printf("[Auth] Số lần sai vân tay: %d/3\n", wrongFingerprintAttempts);
    
    // Gửi log thất bại
    sendToGoogleSheets("DOOR_OPEN", "FINGERPRINT", "Unknown", "FAILED");
    
    // Khóa vân tay nếu sai quá 3 lần
    if (wrongFingerprintAttempts >= 3) {
      fingerprintLocked = true;
      Serial.println("[Security] !!! VÂN TAY BỊ KHÓA - NHẬP MẬT KHẨU ĐỂ MỞ !!!");
      sendToGoogleSheets("FINGER_LOCKED", "FINGERPRINT", "Unknown", "LOCKED_3_ATTEMPTS");
      showMessage("Finger LOCKED!", "Enter password", 2000);
      currentDisplay = DISPLAY_ENTER_PASSWORD;
    } else {
      // Hiển thị thông báo
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Wrong finger!");
      lcd.setCursor(0, 1);
      lcd.print("Attempts: ");
      lcd.print(wrongFingerprintAttempts);
      lcd.print("/3");
      delay(1500);
    }
  }
}

// ==================== AUTOMATION HANDLING ====================
void handleAutomation() {
  // === Tự động bật đèn khi trời tối ===
  if (isDark && !soundLightOn && !overheated) {
    digitalWrite(LED_PIN, HIGH);
  } else if (!isDark && !soundLightOn) {
    digitalWrite(LED_PIN, LOW);
  }
  
  // === Điều khiển quạt theo nhiệt độ ===
  if (temperature >= TEMP_FAN_THRESHOLD && !fanRunning && !overheated) {
    fanRunning = true;
    digitalWrite(FAN_PIN, HIGH);
    Serial.printf("[Auto] 🌀 Bật quạt làm mát (Nhiệt độ: %.1f°C >= %.1f°C)\n", 
                  temperature, TEMP_FAN_THRESHOLD);
  } else if (temperature < (TEMP_FAN_THRESHOLD - 2) && fanRunning) {
    // Tắt quạt khi nhiệt độ giảm 2 độ dưới ngưỡng (tránh bật/tắt liên tục)
    fanRunning = false;
    digitalWrite(FAN_PIN, LOW);
    Serial.printf("[Auto] 🌀 Tắt quạt (Nhiệt độ: %.1f°C < %.1f°C)\n", 
                  temperature, TEMP_FAN_THRESHOLD - 2);
  }
  
  // === Phát hiện âm thanh ===
  int soundDetected = digitalRead(SOUND_PIN);
  
  if (soundDetected == HIGH) {
    Serial.println("[Auto] 🔔 Phát hiện âm thanh!");
    
    // Bật LED âm thanh
    digitalWrite(SOUND_LED_PIN, HIGH);
    
    guestDetected = true;
    soundLightOn = true;
    soundLightStartTime = millis();
    
    // Bật thêm LED chính nếu trời tối
    if (isDark) {
      digitalWrite(LED_PIN, HIGH);
    }
    
    // Hiển thị thông báo có khách
    if (currentDisplay == DISPLAY_WELCOME || currentDisplay == DISPLAY_SENSOR_INFO) {
      currentDisplay = DISPLAY_GUEST_DETECTED;
    }
  }
  
  // Tắt đèn sau thời gian cài đặt
  if (soundLightOn && (millis() - soundLightStartTime >= SOUND_LIGHT_DURATION)) {
    soundLightOn = false;
    
    // Tắt LED âm thanh
    digitalWrite(SOUND_LED_PIN, LOW);
    
    if (!isDark) {
      digitalWrite(LED_PIN, LOW);
    }
    if (currentDisplay == DISPLAY_GUEST_DETECTED) {
      guestDetected = false;
      currentDisplay = DISPLAY_WELCOME;
    }
  }
}

// ==================== OVERHEAT PROTECTION ====================
void handleOverheatProtection() {
  if (temperature >= TEMP_WARNING_THRESHOLD && !overheated) {
    overheated = true;
    Serial.println("[Safety] 🔥 CẢNH BÁO: NHIỆT ĐỘ QUÁ CAO!");
    Serial.println("[Safety] Ngắt tất cả thiết bị điện (trừ quạt)!");
    
    // Ngắt Relay và LED, nhưng giữ quạt chạy để làm mát
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
    // Bật quạt để làm mát khi quá nhiệt
    digitalWrite(FAN_PIN, HIGH);
    fanRunning = true;
    doorUnlocked = false;
    
    currentDisplay = DISPLAY_OVERHEAT_WARNING;
  }
  
  // Reset khi nhiệt độ giảm xuống dưới ngưỡng an toàn (40-5=35°C)
  if (overheated && temperature < (TEMP_WARNING_THRESHOLD - 5)) {
    overheated = false;
    Serial.println("[Safety] ✓ Nhiệt độ đã an toàn, hệ thống hoạt động lại");
    showMessage("Temp normal", "System resumed");
    currentDisplay = DISPLAY_WELCOME;
  }
}

// ==================== DOOR CONTROL ====================
void unlockDoor() {
  Serial.println("[Door] 🔓 MỞ CỬA!");
  doorUnlocked = true;
  doorOpenStartTime = millis();
  
  // Bật LED cửa
  digitalWrite(DOOR_LED_PIN, HIGH);
  
  currentDisplay = DISPLAY_ACCESS_GRANTED;
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ACCESS GRANTED!");
  lcd.setCursor(0, 1);
  lcd.print("Door unlocked");
}

void lockDoor() {
  Serial.println("[Door] 🔒 KHÓA CỬA!");
  doorUnlocked = false;
  
  // Tắt LED cửa
  digitalWrite(DOOR_LED_PIN, LOW);
  
  resetAuthentication();
  currentDisplay = DISPLAY_WELCOME;
}

// ==================== AUTHENTICATION RESET ====================
void resetAuthentication() {
  passwordVerified = false;
  fingerprintVerified = false;
  inputPassword = "";
}

// ==================== SECURITY MODE SWITCH ====================
void switchSecurityMode() {
  highSecurityMode = !highSecurityMode;
  resetAuthentication();
  
  if (highSecurityMode) {
    Serial.println("[Mode] Chế độ: BẢO MẬT CAO (2FA) - Cần Vân tay VÀ Mật khẩu");
  } else {
    Serial.println("[Mode] Chế độ: THƯỜNG - Chỉ cần Vân tay HOẶC Mật khẩu");
  }
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Security Mode:");
  lcd.setCursor(0, 1);
  if (highSecurityMode) {
    lcd.print("HIGH (2FA)");
  } else {
    lcd.print("NORMAL");
  }
  delay(2000);
  currentDisplay = DISPLAY_WELCOME;
}

// ==================== DISPLAY UPDATE ====================
void updateDisplay() {
  // Kiểm tra nếu cửa đang mở và hết thời gian
  if (doorUnlocked && (millis() - doorOpenStartTime >= DOOR_OPEN_TIME)) {
    lockDoor();
    return;
  }
  
  // Cập nhật màn hình định kỳ
  if (millis() - lastDisplayUpdateTime < 500) return;
  lastDisplayUpdateTime = millis();
  
  switch (currentDisplay) {
    case DISPLAY_WELCOME:
      lcd.setCursor(0, 0);
      if (highSecurityMode) {
        lcd.print("[2FA] Welcome!  ");
      } else {
        lcd.print("Welcome!        ");
      }
      lcd.setCursor(0, 1);
      lcd.print("T:");
      lcd.print(temperature, 1);
      lcd.print("C H:");
      lcd.print(humidity, 0);
      lcd.print("%  ");
      break;
      
    case DISPLAY_ENTER_PASSWORD:
      lcd.setCursor(0, 0);
      lcd.print("Enter Password: ");
      break;
      
    case DISPLAY_SCAN_FINGER:
      lcd.setCursor(0, 0);
      lcd.print("Scan your finger");
      lcd.setCursor(0, 1);
      lcd.print("or press key    ");
      break;
      
    case DISPLAY_2FA_PASSWORD:
      lcd.setCursor(0, 0);
      if (fingerprintVerified) {
        lcd.print("[2FA] Finger OK!");
      } else {
        lcd.print("[2FA] Password: ");
      }
      break;
      
    case DISPLAY_2FA_FINGER:
      lcd.setCursor(0, 0);
      lcd.print("[2FA] Pass OK!  ");
      lcd.setCursor(0, 1);
      lcd.print("Scan finger now ");
      break;
      
    case DISPLAY_ACCESS_GRANTED:
      // Đã hiển thị trong unlockDoor()
      break;
      
    case DISPLAY_ACCESS_DENIED:
      lcd.setCursor(0, 0);
      lcd.print("ACCESS DENIED!  ");
      lcd.setCursor(0, 1);
      lcd.print("Try again       ");
      break;
      
    case DISPLAY_SYSTEM_LOCKED:
      {
        int remainingSec = (LOCKOUT_TIME - (millis() - lockoutStartTime)) / 1000;
        if (remainingSec <= 0) {
          systemLocked = false;
          wrongAttempts = 0;
          currentDisplay = DISPLAY_WELCOME;
        } else {
          lcd.setCursor(0, 0);
          lcd.print("SYSTEM LOCKED!  ");
          lcd.setCursor(0, 1);
          lcd.print("Wait ");
          lcd.print(remainingSec);
          lcd.print(" seconds  ");
        }
      }
      break;
      
    case DISPLAY_OVERHEAT_WARNING:
      lcd.setCursor(0, 0);
      lcd.print("!! OVERHEAT !!  ");
      lcd.setCursor(0, 1);
      lcd.print("Temp: ");
      lcd.print(temperature, 1);
      lcd.print("C     ");
      break;
      
    case DISPLAY_GUEST_DETECTED:
      lcd.setCursor(0, 0);
      lcd.print("** CO KHACH! ** ");
      lcd.setCursor(0, 1);
      lcd.print("Sound detected  ");
      break;
      
    case DISPLAY_SENSOR_INFO:
      lcd.setCursor(0, 0);
      lcd.print("T:");
      lcd.print(temperature, 1);
      lcd.print("C H:");
      lcd.print(humidity, 0);
      lcd.print("%  ");
      lcd.setCursor(0, 1);
      lcd.print("L:");
      lcd.print(lightLevel);
      lcd.print(isDark ? " DK" : " BR");
      lcd.print(" F:");
      lcd.print(fanRunning ? "ON " : "OFF");
      break;
  }
}

// ==================== HELPER FUNCTIONS ====================
void showMessage(const char* line1, const char* line2, int delayMs) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
  if (delayMs > 0) {
    delay(delayMs);
  }
}

// ==================== ADMIN MENU ====================
void adminMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("=== ADMIN MENU ===");
  lcd.setCursor(0, 1);
  lcd.print("1Add 2Del 3All 4#");
  
  Serial.println("\n╔═══════════════ ADMIN MENU ═══════════════╗");
  Serial.println("║ 1: Thêm vân tay mới                      ║");
  Serial.println("║ 2: Xóa vân tay theo ID                   ║");
  Serial.println("║ 3: Xóa TẤT CẢ vân tay                    ║");
  Serial.println("║ 4: Xem số vân tay đã lưu                 ║");
  Serial.println("║ *: Thoát Admin Menu                      ║");
  Serial.println("╚══════════════════════════════════════════╝\n");
  
  while (true) {
    char key = keypad.getKey();
    if (!key) continue;
    
    switch (key) {
      case '1': {
        // Thêm vân tay mới
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Enter ID (1-127)");
        lcd.setCursor(0, 1);
        lcd.print("ID: ");
        
        Serial.println("[Admin] Nhập ID vân tay (1-127):");
        
        String idStr = "";
        unsigned long startTime = millis();
        
        while (millis() - startTime < 10000) {
          char k = keypad.getKey();
          if (k) {
            if (k == '#') {
              int id = idStr.toInt();
              if (id >= 1 && id <= 127) {
                if (enrollFingerprint(id)) {
                  showMessage("Enroll Success!", "ID saved", 2000);
                }
              } else {
                showMessage("Invalid ID!", "Use 1-127", 2000);
              }
              break;
            } else if (k == '*') {
              break;
            } else if (k >= '0' && k <= '9') {
              idStr += k;
              lcd.setCursor(4, 1);
              lcd.print(idStr);
              lcd.print("   ");
            }
          }
        }
        
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("=== ADMIN MENU ===");
        lcd.setCursor(0, 1);
        lcd.print("1Add 2Del 3All 4#");
        break;
      }
      
      case '2': {
        // Xóa vân tay theo ID
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Delete ID:");
        lcd.setCursor(0, 1);
        lcd.print("ID: ");
        
        Serial.println("[Admin] Nhập ID vân tay cần xóa:");
        
        String idStr = "";
        unsigned long startTime = millis();
        
        while (millis() - startTime < 10000) {
          char k = keypad.getKey();
          if (k) {
            if (k == '#') {
              int id = idStr.toInt();
              if (id >= 1 && id <= 127) {
                if (deleteFingerprint(id)) {
                  showMessage("Deleted!", "", 2000);
                } else {
                  showMessage("Delete failed!", "", 2000);
                }
              } else {
                showMessage("Invalid ID!", "", 2000);
              }
              break;
            } else if (k == '*') {
              break;
            } else if (k >= '0' && k <= '9') {
              idStr += k;
              lcd.setCursor(4, 1);
              lcd.print(idStr);
              lcd.print("   ");
            }
          }
        }
        
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("=== ADMIN MENU ===");
        lcd.setCursor(0, 1);
        lcd.print("1Add 2Del 3All 4#");
        break;
      }
      
      case '3': {
        // Xóa tất cả vân tay
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Delete ALL?");
        lcd.setCursor(0, 1);
        lcd.print("#=Yes *=No");
        
        Serial.println("[Admin] Xóa TẤT CẢ vân tay? # = Có, * = Không");
        
        while (true) {
          char k = keypad.getKey();
          if (k == '#') {
            deleteAllFingerprints();
            showMessage("All deleted!", "", 2000);
            break;
          } else if (k == '*') {
            showMessage("Cancelled", "", 1000);
            break;
          }
        }
        
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("=== ADMIN MENU ===");
        lcd.setCursor(0, 1);
        lcd.print("1Add 2Del 3All 4#");
        break;
      }
      
      case '4': {
        // Xem số vân tay đã lưu
        showFingerprintCount();
        delay(3000);
        
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("=== ADMIN MENU ===");
        lcd.setCursor(0, 1);
        lcd.print("1Add 2Del 3All 4#");
        break;
      }
      
      case '*':
        // Thoát Admin Menu
        Serial.println("[Admin] Thoát Admin Menu");
        showMessage("Exit Admin", "", 1000);
        return;
    }
  }
}

// ==================== ENROLL FINGERPRINT ====================
bool enrollFingerprint(uint8_t id) {
  Serial.printf("[Enroll] Bắt đầu đăng ký vân tay ID: %d\n", id);
  
  // Bước 1: Quét lần 1
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Place finger...");
  lcd.setCursor(0, 1);
  lcd.print("(1st scan)");
  
  Serial.println("[Enroll] Đặt ngón tay lên cảm biến (lần 1)...");
  
  int p = -1;
  unsigned long startTime = millis();
  
  while (p != FINGERPRINT_OK && millis() - startTime < 10000) {
    p = finger.getImage();
    if (p == FINGERPRINT_OK) {
      Serial.println("[Enroll] ✓ Đã chụp ảnh lần 1");
    }
    delay(50);
  }
  
  if (p != FINGERPRINT_OK) {
    showMessage("Timeout!", "Try again", 2000);
    return false;
  }
  
  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    showMessage("Image error!", "", 2000);
    return false;
  }
  
  // Bước 2: Nhấc ngón tay
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Remove finger...");
  Serial.println("[Enroll] Nhấc ngón tay ra...");
  
  delay(2000);
  
  // Chờ ngón tay được nhấc ra
  while (finger.getImage() != FINGERPRINT_NOFINGER) {
    delay(100);
  }
  
  // Bước 3: Quét lần 2
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Place same");
  lcd.setCursor(0, 1);
  lcd.print("finger again...");
  
  Serial.println("[Enroll] Đặt CÙNG ngón tay lên lần nữa...");
  
  p = -1;
  startTime = millis();
  
  while (p != FINGERPRINT_OK && millis() - startTime < 10000) {
    p = finger.getImage();
    if (p == FINGERPRINT_OK) {
      Serial.println("[Enroll] ✓ Đã chụp ảnh lần 2");
    }
    delay(50);
  }
  
  if (p != FINGERPRINT_OK) {
    showMessage("Timeout!", "Try again", 2000);
    return false;
  }
  
  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    showMessage("Image error!", "", 2000);
    return false;
  }
  
  // Bước 4: Tạo model
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Creating model..");
  
  Serial.println("[Enroll] Đang tạo model...");
  
  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    if (p == FINGERPRINT_ENROLLMISMATCH) {
      showMessage("Fingers not", "match! Retry", 2000);
      Serial.println("[Enroll] ✗ Hai lần quét không khớp!");
    } else {
      showMessage("Model error!", "", 2000);
    }
    return false;
  }
  
  // Bước 5: Lưu vào bộ nhớ
  Serial.printf("[Enroll] Đang lưu vào ID %d...\n", id);
  
  p = finger.storeModel(id);
  if (p != FINGERPRINT_OK) {
    showMessage("Store failed!", "", 2000);
    Serial.println("[Enroll] ✗ Lưu thất bại!");
    return false;
  }
  
  Serial.printf("[Enroll] ✓ Đăng ký thành công! ID: %d\n", id);
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Success!");
  lcd.setCursor(0, 1);
  lcd.print("ID: ");
  lcd.print(id);
  
  return true;
}

// ==================== DELETE FINGERPRINT ====================
bool deleteFingerprint(uint8_t id) {
  Serial.printf("[Admin] Xóa vân tay ID: %d\n", id);
  
  int p = finger.deleteModel(id);
  
  if (p == FINGERPRINT_OK) {
    Serial.printf("[Admin] ✓ Đã xóa vân tay ID %d\n", id);
    return true;
  } else {
    Serial.printf("[Admin] ✗ Không thể xóa ID %d\n", id);
    return false;
  }
}

// ==================== DELETE ALL FINGERPRINTS ====================
void deleteAllFingerprints() {
  Serial.println("[Admin] Xóa TẤT CẢ vân tay...");
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Deleting all...");
  
  int p = finger.emptyDatabase();
  
  if (p == FINGERPRINT_OK) {
    Serial.println("[Admin] ✓ Đã xóa tất cả vân tay!");
  } else {
    Serial.println("[Admin] ✗ Lỗi khi xóa!");
  }
}

// ==================== SHOW FINGERPRINT COUNT ====================
void showFingerprintCount() {
  finger.getTemplateCount();
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Stored prints:");
  lcd.setCursor(0, 1);
  lcd.print(finger.templateCount);
  lcd.print(" / 127");
  
  Serial.printf("[Admin] Số vân tay đã lưu: %d / 127\n", finger.templateCount);
}

// ==================== MAIN FUNCTIONS ====================
void setup() {
  initSystem();
}

void loop() {
  // Đọc cảm biến
  readSensors();
  
  // Xử lý bảo vệ quá nhiệt (ưu tiên cao nhất)
  handleOverheatProtection();
  
  // Xử lý tự động hóa (đèn, âm thanh)
  handleAutomation();
  
  // Xử lý keypad
  handleKeypad();
  
  // Xử lý vân tay
  handleFingerprint();
  
  // Cập nhật màn hình
  updateDisplay();
  
  // Delay nhỏ để ổn định
  delay(10);
}

// ==================== WIFI CONNECTION ====================
void connectWiFi() {
  Serial.println("\n[WiFi] Đang kết nối WiFi...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    lcd.setCursor(attempts % 16, 1);
    lcd.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] ✓ Đã kết nối!");
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected!");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
    delay(2000);
  } else {
    Serial.println("\n[WiFi] ✗ Không kết nối được!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Failed!");
    lcd.setCursor(0, 1);
    lcd.print("Continue offline");
    delay(2000);
  }
}

// ==================== GOOGLE SHEETS LOGGING ====================
void sendToGoogleSheets(String event, String method, String user, String status) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Sheets] WiFi không kết nối - bỏ qua gửi log");
    return;
  }
  
  HTTPClient http;
  
  // Tạo URL với parameters
  String url = String(GOOGLE_SCRIPT_URL);
  url += "?event=" + event;
  url += "&method=" + method;
  url += "&user=" + user;
  url += "&status=" + status;
  url += "&temp=" + String(temperature, 1);
  url += "&humidity=" + String(humidity, 0);
  
  Serial.println("[Sheets] Đang gửi log...");
  Serial.println("[Sheets] URL: " + url);
  
  http.begin(url);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  
  int httpCode = http.GET();
  
  if (httpCode > 0) {
    Serial.printf("[Sheets] ✓ Gửi thành công! HTTP Code: %d\n", httpCode);
    String response = http.getString();
    Serial.println("[Sheets] Response: " + response);
  } else {
    Serial.printf("[Sheets] ✗ Lỗi: %s\n", http.errorToString(httpCode).c_str());
  }
  
  http.end();
}
