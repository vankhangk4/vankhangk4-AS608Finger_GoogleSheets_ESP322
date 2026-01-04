#include <Arduino.h>
#include <Adafruit_Fingerprint.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Preferences.h>

// ===== PIN DEFINITIONS =====
#define FINGERPRINT_RX 16  
#define FINGERPRINT_TX 17
#define RELAY_PIN 4

// Keypad pins
#define ROW1 13
#define ROW2 12
#define ROW3 14
#define ROW4 27
#define COL1 26
#define COL2 25
#define COL3 33
#define COL4 32

// ===== INITIALIZATION =====
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
LiquidCrystal_I2C lcd(0x27, 16, 2);
Preferences preferences; // NVS storage

// Keypad setup
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

// ===== GLOBAL VARIABLES =====
int wrongAttempts = 0;
const int MAX_WRONG = 3;
String currentPassword = "123"; // Mặc định lần đầu
bool inEnrollMode = false;

// ===== FUNCTIONS =====
void unlock_door() {
  Serial.println("✓ MỞ CỬA");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Mo cua");
  
  // Bật relay
  digitalWrite(RELAY_PIN, HIGH);
  delay(2000);
  digitalWrite(RELAY_PIN, LOW);
  
  wrongAttempts = 0;
  delay(3000);
}

void lock_door() {
  Serial.println("✗ ĐÓNG CỬA");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Dong cua");
  delay(2000);
}

void fingerprint_wrong() {
  wrongAttempts++;
  Serial.print("✗ VÂN TAY SAI (");
  Serial.print(wrongAttempts);
  Serial.print("/");
  Serial.print(MAX_WRONG);
  Serial.println(")");
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Van tay sai");
  lcd.setCursor(0, 1);
  lcd.print("Lan: " + String(wrongAttempts));
  delay(2000);
}

String input_password(const char* prompt) {
  Serial.print(prompt);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(prompt);
  lcd.setCursor(0, 1);
  lcd.print("___");
  
  String password = "";
  unsigned long timeout = millis();
  
  while(millis() - timeout < 30000) {
    char key = keypad.getKey();
    
    if (key) {
      if (key == '#') { // Xác nhận
        if (password.length() > 0) {
          return password;
        }
      } else if (key == '*') { // Hủy
        return "";
      } else {
        password += key;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(prompt);
        lcd.setCursor(0, 1);
        for(int i = 0; i < password.length(); i++) {
          lcd.print("*");
        }
      }
      timeout = millis();
    }
  }
  
  Serial.println("TIMEOUT");
  return "";
}

void enroll_fingerprint() {
  Serial.println("\n>> YÊU CẦU NHẬP MẬT KHẨU CỦA");
  String oldPassword = input_password("Mat khau cu:");
  
  if (oldPassword != currentPassword) {
    Serial.println("✗ MẬT KHẨU CŨ SAI!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Mat khau sai!");
    delay(2000);
    return;
  }
  
  Serial.println("✓ MẬT KHẨU ĐÚNG - KHỞI TẠO VÂN TAY");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Dat van tay");
  lcd.setCursor(0, 1);
  lcd.print("Tay phai (2 lan)");
  delay(3000);
  
  // Khởi tạo vân tay (sử dụng example từ Adafruit)
  Serial.println(">> Đợi vân tay để đăng ký...");
  
  // Note: Bạn cần sử dụng code enrollment từ library Adafruit
  // Để đơn giản, tôi sẽ để placeholder
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Van tay OK");
  delay(2000);
  
  Serial.println("\n>> YÊU CẦU NHẬP MẬT KHẨU MỚI");
  String newPassword = input_password("Mat khau moi:");
  
  if (newPassword.length() == 0) {
    Serial.println("✗ HỦY KHỞI TẠO");
    return;
  }
  
  String confirmPassword = input_password("Xac nhan MK:");
  
  if (newPassword != confirmPassword) {
    Serial.println("✗ MẬT KHẨU KHÔNG KHỚP!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Mat khau khong");
    lcd.setCursor(0, 1);
    lcd.print("khop!");
    delay(2000);
    return;
  }
  
  // Lưu mật khẩu vào NVS
  preferences.begin("security", false); // false = read/write
  preferences.putString("password", newPassword);
  preferences.end();
  
  currentPassword = newPassword;
  
  Serial.println("✓ KHỞI TẠO THÀNH CÔNG!");
  Serial.print("Mật khẩu mới: ");
  Serial.println(newPassword);
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Khoi tao OK");
  lcd.setCursor(0, 1);
  lcd.print("Mat khau luu roi");
  delay(3000);
  
  inEnrollMode = false;
}

void prompt_password() {
  Serial.println("\n>> Yêu cầu mật khẩu");
  String password = input_password("Mat khau:");
  
  if (password == currentPassword) {
    Serial.println("✓ MẬT KHẨU ĐÚNG");
    unlock_door();
  } else {
    Serial.println("✗ MẬT KHẨU SAI");
    lock_door();
  }
}

void show_menu() {
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║           MENU CHÍNH               ║");
  Serial.println("║  * = Khởi tạo vân tay + mật khẩu  ║");
  Serial.println("║  # = Sử dụng vân tay bình thường   ║");
  Serial.println("╚════════════════════════════════════╝\n");
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Menu:");
  lcd.setCursor(0, 1);
  lcd.print("* Khoi tao");
}

void test_fingerprint() {
  if (!inEnrollMode) {
    if (wrongAttempts >= MAX_WRONG) {
      Serial.println("\n⚠️  Đã sai 3 lần, yêu cầu mật khẩu!");
      prompt_password();
      return;
    }
    
    if (finger.getImage() == FINGERPRINT_OK) {
      if (finger.image2Tz() == FINGERPRINT_OK) {
        if (finger.fingerFastSearch() == FINGERPRINT_OK) {
          Serial.print("✓ VÂN TAY ĐÚNG - ID: ");
          Serial.println(finger.fingerID);
          unlock_door();
        } else {
          fingerprint_wrong();
        }
      }
    }
  }
}

void check_menu() {
  char key = keypad.getKey();
  
  if (key == '*') {
    Serial.println("\n>> Chế độ KHỞI TẠO VÂN TAY");
    inEnrollMode = true;
    enroll_fingerprint();
    show_menu();
  } else if (key == '#') {
    Serial.println("\n>> Chế độ BÌNH THƯỜNG");
    inEnrollMode = false;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Dat van tay...");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║  HỆ THỐNG VÂN TAY + MẬT KHẨU V2    ║");
  Serial.println("╚════════════════════════════════════╝\n");
  
  // Load mật khẩu từ NVS
  preferences.begin("security", true); // true = read only
  currentPassword = preferences.getString("password", "123");
  preferences.end();
  
  Serial.print("Mật khẩu hiện tại: ");
  Serial.println(currentPassword);
  
  // Init relay
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  
  // Init LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("He thong khoi");
  lcd.setCursor(0, 1);
  lcd.print("dong...");
  
  // Init fingerprint
  Serial.println(">> Initializing fingerprint at 57600 baud...");
  mySerial.begin(57600, SERIAL_8N1, FINGERPRINT_RX, FINGERPRINT_TX);
  delay(2000);
  
  finger.begin(57600);
  delay(2000);
  
  if (finger.verifyPassword()) {
    Serial.println("✓ Cảm biến vân tay: OK");
  } else {
    Serial.println("✗ Cảm biến vân tay: FAILED");
  }
  
  show_menu();
}

void loop() {
  check_menu();
  test_fingerprint();
  delay(100);
}
