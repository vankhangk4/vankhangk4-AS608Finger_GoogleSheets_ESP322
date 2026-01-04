/*
 * GOOGLE APPS SCRIPT - Nhận log từ ESP32
 * =======================================
 * 
 * HƯỚNG DẪN CÀI ĐẶT:
 * 
 * 1. Tạo Google Sheets mới tại: https://sheets.google.com
 * 
 * 2. Đặt tên các cột ở hàng 1:
 *    A1: Timestamp
 *    B1: Event
 *    C1: Method
 *    D1: User
 *    E1: Status
 *    F1: Temperature
 *    G1: Humidity
 * 
 * 3. Vào menu: Extensions > Apps Script
 * 
 * 4. Xóa code mặc định và paste toàn bộ code này vào
 * 
 * 5. Nhấn nút Save (Ctrl+S)
 * 
 * 6. Deploy:
 *    - Nhấn "Deploy" > "New deployment"
 *    - Type: "Web app"
 *    - Execute as: "Me"
 *    - Who has access: "Anyone"
 *    - Nhấn "Deploy"
 * 
 * 7. Copy URL được tạo ra (dạng: https://script.google.com/macros/s/xxx/exec)
 * 
 * 8. Paste URL đó vào file main.cpp (thay YOUR_SCRIPT_ID)
 * 
 * 9. Cũng thay đổi WIFI_SSID và WIFI_PASSWORD trong main.cpp
 */

function doGet(e) {
  try {
    // Lấy parameters từ URL
    var event = e.parameter.event || "UNKNOWN";
    var method = e.parameter.method || "UNKNOWN";
    var user = e.parameter.user || "UNKNOWN";
    var status = e.parameter.status || "UNKNOWN";
    var temp = e.parameter.temp || "N/A";
    var humidity = e.parameter.humidity || "N/A";
    
    // Lấy timestamp Việt Nam (UTC+7)
    var now = new Date();
    var vnTime = new Date(now.getTime() + (7 * 60 * 60 * 1000));
    var timestamp = Utilities.formatDate(vnTime, "GMT", "dd/MM/yyyy HH:mm:ss");
    
    // Mở spreadsheet hiện tại
    var sheet = SpreadsheetApp.getActiveSpreadsheet().getActiveSheet();
    
    // Thêm dữ liệu vào hàng mới
    sheet.appendRow([
      timestamp,
      event,
      method,
      user,
      status,
      temp + "°C",
      humidity + "%"
    ]);
    
    // Trả về response thành công
    return ContentService.createTextOutput(JSON.stringify({
      "status": "success",
      "message": "Data logged successfully",
      "timestamp": timestamp
    })).setMimeType(ContentService.MimeType.JSON);
    
  } catch (error) {
    // Trả về lỗi nếu có
    return ContentService.createTextOutput(JSON.stringify({
      "status": "error",
      "message": error.toString()
    })).setMimeType(ContentService.MimeType.JSON);
  }
}

// Hàm test (chạy thử trong Apps Script)
function testLog() {
  var e = {
    parameter: {
      event: "TEST",
      method: "SCRIPT",
      user: "TEST_USER",
      status: "SUCCESS",
      temp: "28.5",
      humidity: "65"
    }
  };
  
  var result = doGet(e);
  Logger.log(result.getContent());
}
