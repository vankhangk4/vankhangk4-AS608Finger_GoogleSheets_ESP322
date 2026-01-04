#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <Keypad.h>
#include <Adafruit_Fingerprint.h>
#include <Wire.h>

// ===== PIN DEFINITIONS =====
// I2C LCD (SDA=21, SCL=22)
#define LCD_SDA 21
#define LCD_SCL 22
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Fingerprint sensor (UART2)
#define FINGERPRINT_RX 16
#define FINGERPRINT_TX 17
HardwareSerial fingerSerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fingerSerial);

// Keypad
#define ROW1 13
#define ROW2 12
#define ROW3 14
#define ROW4 27
#define COL1 26
#define COL2 25
#define COL3 33
#define COL4 32

// Relay
#define RELAY_PIN 4

// Light sensor (LDR)
#define LDR_ANALOG 34
#define LDR_DIGITAL 35

// Sound sensor
#define SOUND_SENSOR 5

// DHT11 (Temperature & Humidity)
#define DHT_PIN 15
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// LED
#define LED_PIN 2

// ===== KEYPAD SETUP =====
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

// ===== TEST STATES =====
enum TestMode {
  TEST_MENU,
  TEST_LCD,
  TEST_FINGERPRINT,
  TEST_KEYPAD,
  TEST_RELAY,
  TEST_LDR,
  TEST_SOUND,
  TEST_DHT11,
  TEST_LED
};

TestMode currentTest = TEST_MENU;

void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n\n========== SENSOR TEST PROGRAM ==========");
  Serial.println("Khởi tạo các chân và cảm biến...\n");
  
  // Initialize pins
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  
  pinMode(LDR_DIGITAL, INPUT);
  pinMode(SOUND_SENSOR, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Initialize LCD
  Wire.begin(LCD_SDA, LCD_SCL);
  lcd.init();
  lcd.backlight();
  lcd.print("ESP32 Test");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  
  // Initialize DHT
  dht.begin();
  
  // Initialize Fingerprint
  fingerSerial.begin(57600, SERIAL_8N1, FINGERPRINT_RX, FINGERPRINT_TX);
  delay(500);
  
  Serial.println("Khởi tạo xong! Chọn cảm biến để test:");
  Serial.println("1 - LCD I2C (16x2)");
  Serial.println("2 - Cảm biến vân tay AS608");
  Serial.println("3 - Keypad 4x4");
  Serial.println("4 - Relay");
  Serial.println("5 - Cảm biến ánh sáng (LDR)");
  Serial.println("6 - Cảm biến âm thanh");
  Serial.println("7 - DHT11 (Nhiệt độ & Độ ẩm)");
  Serial.println("8 - LED");
  Serial.println("0 - Menu");
  Serial.println("======================================\n");
  
  delay(2000);
  lcd.clear();
}

void loop() {
  // Đọc lựa chọn từ Serial
  if (Serial.available()) {
    char choice = Serial.read();
    Serial.println();
    
    switch(choice) {
      case '1':
        testLCD();
        break;
      case '2':
        testFingerprint();
        break;
      case '3':
        testKeypad();
        break;
      case '4':
        testRelay();
        break;
      case '5':
        testLDR();
        break;
      case '6':
        testSoundSensor();
        break;
      case '7':
        testDHT11();
        break;
      case '8':
        testLED();
        break;
      case '0':
        printMenu();
        break;
      default:
        if (choice >= 32) { // Chỉ in nếu là ký tự có thể in
          Serial.println("Lựa chọn không hợp lệ! Chọn 0-8");
        }
        break;
    }
  }
}

void printMenu() {
  Serial.println("\n========== MENU TEST SENSORS ==========");
  Serial.println("1 - LCD I2C (16x2)");
  Serial.println("2 - Cảm biến vân tay AS608");
  Serial.println("3 - Keypad 4x4");
  Serial.println("4 - Relay");
  Serial.println("5 - Cảm biến ánh sáng (LDR)");
  Serial.println("6 - Cảm biến âm thanh");
  Serial.println("7 - DHT11 (Nhiệt độ & Độ ẩm)");
  Serial.println("8 - LED");
  Serial.println("0 - Menu");
  Serial.println("======================================\n");
}

// ===== LCD TEST =====
void testLCD() {
  Serial.println("\n========== TEST LCD I2C ==========");
  Serial.println("Hiển thị text trên LCD...");
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("LCD Test");
  lcd.setCursor(0, 1);
  lcd.print("Connected OK!");
  
  Serial.println("✓ LCD hoạt động bình thường");
  Serial.println("Nhấn phím gì để quay lại menu\n");
}

// ===== FINGERPRINT TEST =====
void testFingerprint() {
  Serial.println("\n========== TEST CAM BIEN VAN TAY ==========");
  Serial.println("Kiểm tra kết nối với AS608...");
  
  if (finger.verifyPassword()) {
    Serial.println("✓ Cảm biến vân tay kết nối thành công!");
    Serial.print("ID cảm biến: 0x");
    Serial.println(finger.moduleVersion, HEX);
    
    lcd.clear();
    lcd.print("Fingerprint OK");
    
    Serial.println("\nĐặt ngón tay lên cảm biến để test...");
    Serial.println("(Bấm Ctrl+C để dừng)");
    
    unsigned long startTime = millis();
    while (millis() - startTime < 30000) { // Test 30 giây
      if (Serial.available() && Serial.read() == 3) break; // Ctrl+C
      
      int p = finger.getImage();
      if (p == FINGERPRINT_OK) {
        Serial.println("✓ Nhận dạng vân tay thành công!");
        lcd.clear();
        lcd.print("Fingerprint OK");
        delay(1000);
        break;
      }
    }
  } else {
    Serial.println("✗ Không thể kết nối với cảm biến vân tay!");
    Serial.println("Kiểm tra:");
    Serial.println("- TX cảm biến → GPIO 16");
    Serial.println("- RX cảm biến → GPIO 17");
    Serial.println("- GND và VCC 3.3V");
    
    lcd.clear();
    lcd.print("Fingerprint FAIL");
  }
  
  Serial.println("Nhấn phím gì để quay lại menu\n");
}

// ===== KEYPAD TEST =====
void testKeypad() {
  Serial.println("\n========== TEST KEYPAD 4x4 ==========");
  Serial.println("Nhấn các phím trên keypad...");
  Serial.println("(Bấm * để thoát)\n");
  
  lcd.clear();
  lcd.print("Keypad Test");
  
  bool exitKeypadTest = false;
  unsigned long lastPressTime = 0;
  
  while (!exitKeypadTest) {
    char key = keypad.getKey();
    
    if (key) {
      lastPressTime = millis();
      
      Serial.print("Phím nhấn: ");
      Serial.println(key);
      
      lcd.clear();
      lcd.print("Key: ");
      lcd.print(key);
      
      if (key == '*') {
        Serial.println("\n✓ Keypad hoạt động bình thường!");
        exitKeypadTest = true;
      }
    }
    
    // Timeout 30 giây
    if (millis() - lastPressTime > 30000) {
      Serial.println("✓ Keypad sẵn sàng (timeout 30s)");
      exitKeypadTest = true;
    }
  }
  
  Serial.println("Nhấn phím gì để quay lại menu\n");
}

// ===== RELAY TEST =====
void testRelay() {
  Serial.println("\n========== TEST RELAY ==========");
  Serial.println("Kiểm tra relay...");
  
  lcd.clear();
  lcd.print("Relay Test");
  
  // Bật relay
  Serial.println("Bật relay...");
  digitalWrite(RELAY_PIN, HIGH);
  lcd.setCursor(0, 1);
  lcd.print("ON - 2s");
  delay(2000);
  
  // Tắt relay
  Serial.println("Tắt relay...");
  digitalWrite(RELAY_PIN, LOW);
  lcd.clear();
  lcd.print("Relay Test");
  lcd.setCursor(0, 1);
  lcd.print("OFF");
  delay(2000);
  
  // Bật lại
  Serial.println("Bật relay lại...");
  digitalWrite(RELAY_PIN, HIGH);
  lcd.setCursor(0, 1);
  lcd.print("ON - 2s");
  delay(2000);
  
  digitalWrite(RELAY_PIN, LOW);
  Serial.println("✓ Relay hoạt động bình thường!");
  Serial.println("Nhấn phím gì để quay lại menu\n");
}

// ===== LDR TEST =====
void testLDR() {
  Serial.println("\n========== TEST CAM BIEN ANH SANG (LDR) ==========");
  Serial.println("Đọc giá trị từ cảm biến ánh sáng...");
  Serial.println("(Chạy trong 10 giây)\n");
  
  lcd.clear();
  lcd.print("LDR Test");
  
  unsigned long startTime = millis();
  int minValue = 4095, maxValue = 0;
  
  while (millis() - startTime < 10000) {
    int analogValue = analogRead(LDR_ANALOG);
    int digitalValue = digitalRead(LDR_DIGITAL);
    
    Serial.print("Analog: ");
    Serial.print(analogValue);
    Serial.print(" | Digital: ");
    Serial.println(digitalValue);
    
    minValue = min(minValue, analogValue);
    maxValue = max(maxValue, analogValue);
    
    lcd.setCursor(0, 1);
    lcd.print("A:");
    lcd.print(analogValue);
    lcd.print(" D:");
    lcd.print(digitalValue);
    
    delay(500);
  }
  
  Serial.print("\n✓ LDR hoạt động bình thường!");
  Serial.print(" (Min: ");
  Serial.print(minValue);
  Serial.print(", Max: ");
  Serial.println(maxValue);
  Serial.println("Nhấn phím gì để quay lại menu\n");
}

// ===== SOUND SENSOR TEST =====
void testSoundSensor() {
  Serial.println("\n========== TEST CAM BIEN AM THANH ==========");
  Serial.println("Phát hiện âm thanh...");
  Serial.println("(Chạy trong 10 giây)\n");
  
  lcd.clear();
  lcd.print("Sound Test");
  
  unsigned long startTime = millis();
  int soundCount = 0;
  
  while (millis() - startTime < 10000) {
    int soundValue = digitalRead(SOUND_SENSOR);
    
    if (soundValue == HIGH) {
      soundCount++;
      Serial.println("✓ Phát hiện âm thanh!");
      
      lcd.clear();
      lcd.print("Sound: ");
      lcd.print(soundCount);
      delay(500);
      
      // Đợi âm thanh kết thúc
      while (digitalRead(SOUND_SENSOR) == HIGH) {
        delay(10);
      }
      delay(500);
    }
    
    delay(100);
  }
  
  Serial.print("✓ Cảm biến âm thanh hoạt động! (Phát hiện ");
  Serial.print(soundCount);
  Serial.println(" lần)");
  Serial.println("Nhấn phím gì để quay lại menu\n");
}

// ===== DHT11 TEST =====
void testDHT11() {
  Serial.println("\n========== TEST DHT11 (NHIET DO VA DO AM) ==========");
  
  lcd.clear();
  lcd.print("DHT11 Test");
  delay(1000);
  
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("✗ Lỗi đọc từ DHT11!");
    Serial.println("Kiểm tra:");
    Serial.println("- Chân OUT → GPIO 15");
    Serial.println("- VCC → 3.3V");
    Serial.println("- GND → GND");
    
    lcd.clear();
    lcd.print("DHT11 ERROR");
  } else {
    Serial.print("✓ DHT11 hoạt động bình thường!");
    Serial.print(" Nhiệt độ: ");
    Serial.print(temperature);
    Serial.print("°C | Độ ẩm: ");
    Serial.print(humidity);
    Serial.println("%");
    
    lcd.clear();
    lcd.print("T:");
    lcd.print((int)temperature);
    lcd.print("C H:");
    lcd.print((int)humidity);
    lcd.print("%");
  }
  
  Serial.println("Nhấn phím gì để quay lại menu\n");
}

// ===== LED TEST =====
void testLED() {
  Serial.println("\n========== TEST LED ==========");
  Serial.println("Bật/tắt LED...");
  
  lcd.clear();
  lcd.print("LED Test");
  
  // Bật LED
  Serial.println("Bật LED...");
  digitalWrite(LED_PIN, HIGH);
  lcd.setCursor(0, 1);
  lcd.print("ON - 2s");
  delay(2000);
  
  // Tắt LED
  Serial.println("Tắt LED...");
  digitalWrite(LED_PIN, LOW);
  lcd.clear();
  lcd.print("LED Test");
  lcd.setCursor(0, 1);
  lcd.print("OFF - 2s");
  delay(2000);
  
  // Nhấp nháy 5 lần
  Serial.println("Nhấp nháy LED 5 lần...");
  for (int i = 0; i < 5; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(300);
    digitalWrite(LED_PIN, LOW);
    delay(300);
  }
  
  Serial.println("✓ LED hoạt động bình thường!");
  Serial.println("Nhấn phím gì để quay lại menu\n");
}
