#include <Arduino.h>
#include <Adafruit_Fingerprint.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
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
Preferences preferences;

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
String savedPassword = "";
bool hasFingerprint = false;
int fingerCount = 0;  // Số lượng vân tay đã lưu
const int MAX_FINGERPRINTS = 10;  // Tối đa 10 vân tay

// ===== FORWARD DECLARATIONS =====
void singleBeep();
void successBeep();
void errorBeep();
void show_main_menu();
void enroll_fingerprint();
String input_password(const char* prompt);
void setup_password();
void unlock_door();
void fingerprint_wrong();
void prompt_password();
void test_fingerprint();
void check_password_or_fingerprint();
void check_menu();

// ===== BUZZER FUNCTIONS =====
#define BUZZER_PIN 19

void singleBeep() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(150);
  digitalWrite(BUZZER_PIN, LOW);
}

void successBeep() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(150);
  digitalWrite(BUZZER_PIN, LOW);
  delay(100);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(150);
  digitalWrite(BUZZER_PIN, LOW);
}

void errorBeep() {
  for(int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(50);
    digitalWrite(BUZZER_PIN, LOW);
    delay(50);
  }
}

// Reset tất cả dữ liệu (dùng khi xóa sạch vân tay)
void resetAllData() {
  preferences.begin("security", false);
  preferences.clear();  // Xóa tất cả
  preferences.end();
  
  fingerCount = 0;
  hasFingerprint = false;
  savedPassword = "";
  wrongAttempts = 0;
  
  Serial.println("✓ Đã reset tất cả dữ liệu!");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Da reset du lieu");
  successBeep();
  delay(2000);
}

// ===== FUNCTIONS =====
void show_main_menu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Menu: A/B");
  lcd.setCursor(0, 1);
  lcd.print("A:Khoi tao VT");
}

void enroll_fingerprint() {
  if (fingerCount >= MAX_FINGERPRINTS) {
    Serial.println("✗ Đã đạt tối đa số lượng vân tay!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Toi da van tay!");
    errorBeep();
    delay(2000);
    return;
  }
  
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║     KHỞI TẠO VÂN TAY (ENROLL)      ║");
  Serial.println("╚════════════════════════════════════╝\n");
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Dat tay...");
  lcd.setCursor(0, 1);
  lcd.print("Cho den het...");
  
  Serial.println(">> Đặt ngón tay lên cảm biến...");
  
  // Chờ để người dùng đặt tay
  while (finger.getImage() != FINGERPRINT_OK) {
    delay(100);
  }
  
  if (finger.image2Tz() != FINGERPRINT_OK) {
    Serial.println("✗ Lỗi xử lý ảnh");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Loi xu ly anh");
    errorBeep();
    delay(2000);
    return;
  }
  
  // KIỂM TRA TRÙNG LẶP: Tìm kiếm xem vân tay này có tồn tại không
  if (finger.fingerFastSearch() == FINGERPRINT_OK) {
    Serial.print("✗ VÂN TAY ĐÃ TỒN TẠI (ID: ");
    Serial.print(finger.fingerID);
    Serial.println(")");
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Van tay da ton");
    lcd.setCursor(0, 1);
    lcd.print("tai (ID:" + String(finger.fingerID) + ")");
    errorBeep();
    delay(3000);
    return;
  }
  
  // Vân tay mới - tìm slot trống và lưu
  int slot = fingerCount + 1;
  
  if (finger.storeModel(slot) != FINGERPRINT_OK) {
    Serial.println("✗ Không thể lưu vân tay");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Luu van tay loi");
    errorBeep();
    delay(2000);
    return;
  }
  
  fingerCount++;
  hasFingerprint = true;
  
  Serial.print("✓ KHỞI TẠO VÂN TAY #");
  Serial.print(slot);
  Serial.println(" THÀNH CÔNG!");
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Van tay #" + String(slot) + " OK!");
  lcd.setCursor(0, 1);
  lcd.print("Nhan A-tiep/B-MK");
  
  successBeep();
  
  // Lưu trạng thái
  preferences.begin("security", false);
  preferences.putBool("hasFingerprint", true);
  preferences.putInt("fingerCount", fingerCount);
  preferences.end();
  
  Serial.println(">> Nhấn A để thêm vân tay, B để thiết lập mật khẩu\n");
  
  delay(3000);
}

String input_password(const char* prompt) {
  Serial.print(prompt);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(prompt);
  
  String password = "";
  unsigned long timeout = millis();
  
  while(millis() - timeout < 30000) {
    char key = keypad.getKey();
    
    if (key) {
      if (key == '#') { // Xác nhận
        if (password.length() > 0) {
          Serial.println(password);
          return password;
        }
      } else if (key == '*') { // Hủy
        Serial.println("HUY");
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
    delay(100);
  }
  
  Serial.println("TIMEOUT");
  return "";
}

void setup_password() {
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║      THIẾT LẬP MẬT KHẨU           ║");
  Serial.println("╚════════════════════════════════════╝\n");
  
  String newPassword = input_password("Mat khau moi:");
  
  if (newPassword.length() == 0) {
    Serial.println("✗ HỦY THIẾT LẬP");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Huy thiet lap");
    delay(2000);
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
  
  // Lưu mật khẩu
  preferences.begin("security", false);
  preferences.putString("password", newPassword);
  preferences.end();
  
  savedPassword = newPassword;
  
  Serial.println("✓ THIẾT LẬP MẬT KHẨU THÀNH CÔNG!");
  Serial.print("Mật khẩu: ");
  Serial.println(newPassword);
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Mat khau OK!");
  lcd.setCursor(0, 1);
  lcd.print("He thong san san");
  
  delay(3000);
}

void unlock_door() {
  Serial.println("✓ MỞ CỬA");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Mo cua!");
  
  // Bật relay 2 giây
  digitalWrite(RELAY_PIN, HIGH);
  delay(500);
  digitalWrite(RELAY_PIN, LOW);
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Relay tam mo");
  
  wrongAttempts = 0;
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

void prompt_password() {
  Serial.println("\n>> Yêu cầu mật khẩu");
  String password = input_password("Mat khau:");
  
  if (password == savedPassword) {
    Serial.println("✓ MẬT KHẨU ĐÚNG");
    unlock_door();
  } else {
    Serial.println("✗ MẬT KHẨU SAI");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Mat khau SAI!");
    delay(2000);
  }
}

void test_fingerprint() {
  if (!hasFingerprint) return;
  
  if (wrongAttempts >= MAX_WRONG) {
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

void check_menu() {
  char key = keypad.getKey();
  
  if (key == 'A') {
    // Luôn cho phép thêm vân tay mới (tối đa 10)
    enroll_fingerprint();
    show_main_menu();
  } 
  else if (key == 'B') {
    if (hasFingerprint) {
      setup_password();
      show_main_menu();
    } else {
      Serial.println("⚠️  Chưa khởi tạo vân tay!");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Chua co van tay");
      errorBeep();
      delay(2000);
      show_main_menu();
    }
  }
  else if (key == 'D') {
    // Phím D để reset tất cả dữ liệu
    resetAllData();
    show_main_menu();
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║  HỆ THỐNG VÂN TAY + MẬT KHẨU V3    ║");
  Serial.println("╚════════════════════════════════════╝\n");
  
  // Load cấu hình từ NVS
  preferences.begin("security", true);
  hasFingerprint = preferences.getBool("hasFingerprint", false);
  fingerCount = preferences.getInt("fingerCount", 0);
  savedPassword = preferences.getString("password", "");
  preferences.end();
  
  Serial.print("Số vân tay đã lưu: ");
  Serial.println(fingerCount);
  Serial.print("Mật khẩu: ");
  Serial.println(savedPassword.length() > 0 ? "CÓ" : "CHƯA");
  
  // Init relay
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  
  // Init buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Init LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("He thong khoi");
  lcd.setCursor(0, 1);
  lcd.print("dong...");
  
  // Init fingerprint
  Serial.println("\n>> Initializing fingerprint at 57600 baud...");
  mySerial.begin(57600, SERIAL_8N1, FINGERPRINT_RX, FINGERPRINT_TX);
  delay(2000);
  
  finger.begin(57600);
  delay(2000);
  
  if (finger.verifyPassword()) {
    Serial.println("✓ Cảm biến vân tay: OK");
  } else {
    Serial.println("✗ Cảm biến vân tay: FAILED");
  }
  
  show_main_menu();
  
  Serial.println("\n✓ Hệ thống sẵn sàng!");
  Serial.println("Hướng dẫn:");
  Serial.println("  A = Thêm vân tay mới (tối đa 10)");
  Serial.println("  B = Thiết lập mật khẩu");
  Serial.println("  Sử dụng = Đặt vân tay hoặc nhập mật khẩu\n");
  
  delay(2000);
}

void loop() {
  if (hasFingerprint) {
    test_fingerprint();
  }
  
  check_menu();
  delay(100);
}
