/**
 * SMART GARDEN - ESP32 FIRMWARE (Cập nhật FirebaseClient mới nhất)
 * - Hệ thống tưới cây tự động đồng bộ Firebase Realtime Database qua ESP32.
 * * THƯ VIỆN CẦN CÀI ĐẶT:
 * 1. WiFiManager (by tzapu)
 * 2. FirebaseClient (by Mobizt) <-- Chỉ cần cài bản này qua Library Manager
 */

#define ENABLE_NO_AUTH
#define ENABLE_DATABASE

#include <WiFi.h>
#include <WiFiClientSecure.h>     // Thư viện mạng bảo mật
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>          
#include <FirebaseClient.h>       // Thư viện core FirebaseClient

// ==========================================================================
// 🔴 CẤU HÌNH THÔNG SỐ KẾT NỐI FIREBASE
// ==========================================================================
#define FIREBASE_HOST "https://smart-garden-eb27b-default-rtdb.firebaseio.com/"
#define API_KEY "AIzaSyCBLGHiOptA8PW_R5jUGV840SaM9OfLrGo"                                       

// --- CẤU HÌNH PHẦN CỨNG ESP32 ---
#define PIN_SOIL      36    // 
#define PIN_PUMP      26    // Relay điều khiển bơm (D26)
#define PIN_BUTTON    27    // Nút nhấn Reset WiFi (D27)
#define PIN_LED       2     // Đèn LED tích hợp (D2)

// --- CẤU HÌNH THÔNG SỐ VẬN HÀNH ---
#define HYSTERESIS     5     // Độ trễ ngắt bơm (5% ẩm)
#define SEND_INTERVAL  2000  

// --- ĐỐI TƯỢNG TOÀN CỤC CỦA FIREBASECLIENT ---
WiFiClientSecure ssl_client;      
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);

NoAuth noAuth;
FirebaseApp app;
RealtimeDatabase Database;
AsyncResult databaseResult;

// --- BIẾN ĐIỀU KHIỂN HỆ THỐNG ---
int do_am_dat = 0;
int nguong_kho = 40;     // Mặc định 40% độ ẩm
int che_do = 0;          
int bom_thu_cong = 0;    
int trang_thai_bom = 0;  

// Hiệu chuẩn cảm biến (đọc từ Firebase, mặc định 4095 khi khô và 1000 khi ướt)
int adc_kho = 4095;
int adc_uot = 1000;

// Biến kiểm soát lưu lượng gửi Firebase (Tránh tràn giới hạn ghi)
int last_uploaded_do_am_dat = -1;
int last_uploaded_trang_thai_bom = -1;
int last_uploaded_che_do = -1;
int last_uploaded_nguong_kho = -1;
int last_uploaded_bom_thu_cong = -1;
int last_uploaded_adc_kho = -1;
int last_uploaded_adc_uot = -1;
unsigned long lastHeartbeatTime = 0;
#define HEARTBEAT_INTERVAL 30000 // Tối thiểu 30 giây gửi 1 lần để giữ kết nối hoạt động

unsigned long lastSendTime = 0;
unsigned long buttonPressTime = 0;
bool isButtonPressed = false;

// --- HÀM KHỞI TẠO LED ---
void setupLED() {
  pinMode(PIN_LED, OUTPUT);
  analogWrite(PIN_LED, 0); // Tắt ban đầu
}

void setLEDDim() {
  analogWrite(PIN_LED, 15); // Sáng mờ (duty = 15/255)
}

void setLEDOff() {
  analogWrite(PIN_LED, 0);  // Tắt hẳn
}

// Nháy LED nhanh khi xóa cấu hình WiFi
void blinkLEDRapidly(int durationMs) {
  int elapsed = 0;
  while (elapsed < durationMs) {
    analogWrite(PIN_LED, 255); // Sáng tối đa
    delay(100);
    analogWrite(PIN_LED, 0);   // Tắt hẳn
    delay(100);
    elapsed += 200;
  }
}

// Cập nhật trạng thái đèn LED liên tục theo chế độ hoạt động
unsigned long lastLEDBlinkTime = 0;
bool ledState = false;

void updateLED() {
  // 1. Máy bơm đang chạy -> Nháy nhanh (150ms)
  if (trang_thai_bom == 1) {
    if (millis() - lastLEDBlinkTime >= 150) {
      lastLEDBlinkTime = millis();
      ledState = !ledState;
      analogWrite(PIN_LED, ledState ? 255 : 0);
    }
  } 
  // 2. Cảm biến đang đọc / Đồng bộ -> Nháy chậm (500ms) trong 1 giây đầu chu kỳ gửi
  else if (millis() - lastSendTime < 1000) {
    if (millis() - lastLEDBlinkTime >= 500) {
      lastLEDBlinkTime = millis();
      ledState = !ledState;
      analogWrite(PIN_LED, ledState ? 255 : 0);
    }
  } 
  // 3. Trạng thái bình thường -> Sáng mờ
  else {
    analogWrite(PIN_LED, 15); 
  }
}

// --- KHỞI TẠO FIREBASE ---
void initFirebase() {
  ssl_client.setInsecure(); // Bỏ qua SSL certificate để kết nối nhẹ và mượt
  
  Serial.println("[Firebase] Đang khởi tạo ứng dụng...");
  initializeApp(aClient, app, getAuth(noAuth));

  app.getApp<RealtimeDatabase>(Database);
  Database.url(FIREBASE_HOST);
}

// --- HÀM ĐIỀU KHIỂN BƠM ---
void controlPump(int state) {
  trang_thai_bom = state;
  digitalWrite(PIN_PUMP, state == 1 ? HIGH : LOW);
}

// --- HÀM PHỤ TRỢ BÓC TÁCH CHUỖI JSON TUYỆT ĐỐI ---
int getJsonValue(String payload, String key) {
  int index = payload.indexOf("\"" + key + "\"");
  if (index == -1) return -1;
  
  int colonIndex = payload.indexOf(":", index);
  if (colonIndex == -1) return -1;
  
  int commaIndex = payload.indexOf(",", colonIndex);
  int braceIndex = payload.indexOf("}", colonIndex);
  int endIndex = (commaIndex != -1 && commaIndex < braceIndex) ? commaIndex : braceIndex;
  
  String val = payload.substring(colonIndex + 1, endIndex);
  val.replace("\"", "");
  val.replace(" ", "");
  val.trim();
  
  return val.toInt();
}

// --- ĐỒNG BỘ DỮ LIỆU TỪ FIREBASE XUỐNG ESP32 ---
void syncWithFirebase() {
  if (app.ready()) {
    String payload = Database.get<String>(aClient, "/HeThongTuoi");
    
    if (aClient.lastError().code() == 0 && payload.length() > 0) {
      // Giải mã bằng bộ bóc tách chuỗi dọn sạch ký tự nhiễu
      int val_che_do = getJsonValue(payload, "che_do");
      int val_nguong_kho = getJsonValue(payload, "nguong_kho");
      int val_bom_thu_cong = getJsonValue(payload, "bom_thu_cong");
      int val_adc_kho = getJsonValue(payload, "adc_kho");
      int val_adc_uot = getJsonValue(payload, "adc_uot");

      if (val_che_do != -1) che_do = val_che_do;
      if (val_nguong_kho != -1) nguong_kho = val_nguong_kho;
      if (val_bom_thu_cong != -1) bom_thu_cong = val_bom_thu_cong;
      if (val_adc_kho != -1) adc_kho = val_adc_kho;
      if (val_adc_uot != -1) adc_uot = val_adc_uot;
      
      Serial.printf("[Firebase Read] Thành công! CheDo: %d, NguongKho: %d%%, Bom: %d, ADCKho: %d, ADCUot: %d\n", 
                    che_do, nguong_kho, bom_thu_cong, adc_kho, adc_uot);
    } else {
      Serial.printf("[Firebase Read] Lỗi: %s, Mã: %d\n", aClient.lastError().message().c_str(), aClient.lastError().code());
    }
  } else {
    Serial.println("[Firebase Read] App chưa sẵn sàng.");
  }
}

// --- 🔴 ĐÃ CẬP NHẬT: GỬI TRỌN BỘ 5 BIẾN ĐỒNG BỘ LÊN GIAO DIỆN ---
void uploadDataToFirebase(int rawADC) {
  if (app.ready()) {
    // Chỉ gửi lên Firebase khi có sự thay đổi giá trị hoặc tới hạn Heartbeat định kỳ
    bool hasMoistureChanged = abs(do_am_dat - last_uploaded_do_am_dat) >= 2; // Thay đổi >= 2% ẩm
    bool hasPumpStateChanged = (trang_thai_bom != last_uploaded_trang_thai_bom);
    bool hasModeChanged = (che_do != last_uploaded_che_do);
    bool hasThresholdChanged = (nguong_kho != last_uploaded_nguong_kho);
    bool hasManualPumpChanged = (bom_thu_cong != last_uploaded_bom_thu_cong);
    bool hasAdcKhoChanged = (adc_kho != last_uploaded_adc_kho);
    bool hasAdcUotChanged = (adc_uot != last_uploaded_adc_uot);
    bool isHeartbeat = (millis() - lastHeartbeatTime >= HEARTBEAT_INTERVAL);

    if (hasMoistureChanged || hasPumpStateChanged || hasModeChanged || hasThresholdChanged || hasManualPumpChanged || hasAdcKhoChanged || hasAdcUotChanged || isHeartbeat) {
      // Cập nhật mốc lưu giữ
      last_uploaded_do_am_dat = do_am_dat;
      last_uploaded_trang_thai_bom = trang_thai_bom;
      last_uploaded_che_do = che_do;
      last_uploaded_nguong_kho = nguong_kho;
      last_uploaded_bom_thu_cong = bom_thu_cong;
      last_uploaded_adc_kho = adc_kho;
      last_uploaded_adc_uot = adc_uot;
      lastHeartbeatTime = millis();

      String jsonStr = "{\"do_am_dat\":" + String(do_am_dat) + 
                       ",\"trang_thai_bom\":" + String(trang_thai_bom) + 
                       ",\"che_do\":" + String(che_do) + 
                       ",\"nguong_kho\":" + String(nguong_kho) + 
                       ",\"bom_thu_cong\":" + String(bom_thu_cong) + 
                       ",\"raw_adc\":" + String(rawADC) + 
                       ",\"adc_kho\":" + String(adc_kho) + 
                       ",\"adc_uot\":" + String(adc_uot) + "}";
      
      Database.update(aClient, "/HeThongTuoi", object_t(jsonStr), databaseResult); 
      Serial.printf("[Firebase Write] Gửi dữ liệu cập nhật: %s (Lý do: %s)\n", 
                    jsonStr.c_str(), 
                    hasPumpStateChanged ? "Thay đổi trạng thái bơm" : 
                    hasMoistureChanged ? "Độ ẩm biến động" : 
                    hasModeChanged ? "Chuyển chế độ" : 
                    hasThresholdChanged ? "Đổi ngưỡng tưới" : 
                    hasManualPumpChanged ? "Ấn nút bơm thủ công" : 
                    hasAdcKhoChanged ? "Thay đổi cấu hình ADC khô" :
                    hasAdcUotChanged ? "Thay đổi cấu hình ADC ướt" : "Heartbeat định kỳ");
    }
  } else {
    Serial.println("[Firebase Write] App chưa sẵn sàng.");
  }
}

// --- HÀM XỬ LÝ KẾT QUẢ ASYNC TRONG LOOP ---
void processFirebaseResult(AsyncResult &aResult) {
  if (!aResult.isResult()) return;

  if (aResult.isError()) {
    Serial.printf("[Firebase Async Error] Lỗi: %s\n", aResult.error().message().c_str());
  }
  if (aResult.available()) {
    Serial.printf("[Firebase Write Success] Đồng bộ lên giao diện thành công!\n");
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n--- HỆ THỐNG TƯỚI CÂY THÔNG MINH ESP32 ---");

  pinMode(PIN_PUMP, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP); 
  setupLED();
  
  controlPump(0); 
  setLEDDim();    

  // --- CẤU HÌNH WIFI QUA WIFIMANAGER ---
  WiFiManager wm;
  wm.setConfigPortalTimeout(180); 
  
  if (!wm.autoConnect("HeThongTuoi_CapNhatWiFi")) {
    Serial.println("[WiFi] Hết thời gian chờ cấu hình. Khởi động lại...");
    delay(3000);
    ESP.restart();
  }

  Serial.print("[WiFi] Đã kết nối! IP: ");
  Serial.println(WiFi.localIP());
  
  setLEDOff(); 
  initFirebase();
}

void loop() {
  // Duy trì kết nối token và xử lý tiến trình ngầm của thư viện Firebase
  app.loop();

  // Xử lý phản hồi từ các lệnh ghi dữ liệu Async
  processFirebaseResult(databaseResult);

  // --- XỬ LÝ NÚT NHẤN RESET WIFI GIỮ 5 GIÂY ---
  int buttonState = digitalRead(PIN_BUTTON);
  if (buttonState == LOW) { 
    if (!isButtonPressed) {
      buttonPressTime = millis();
      isButtonPressed = true;
    } else {
      if (millis() - buttonPressTime >= 5000) { 
        Serial.println("[Nút Nhấn] Đã giữ đủ 5 giây! Xóa cấu hình WiFi...");
        blinkLEDRapidly(3000); 
        WiFiManager wm;
        wm.resetSettings(); 
        ESP.restart();
      }
    }
  } else {
    isButtonPressed = false; 
  }

  // --- ĐỌC ĐỘ ẨM ĐẤT ---
  int rawADC = analogRead(PIN_SOIL);
  // Quy đổi sang phần trăm (0 - 100%) sử dụng giới hạn hiệu chuẩn adc_kho và adc_uot
  int percent = map(rawADC, adc_kho, adc_uot, 0, 100);
  do_am_dat = constrain(percent, 0, 100);
  
  // --- LOGIC VẬN HÀNH ĐỊNH KỲ 2 GIÂY ---
  if (millis() - lastSendTime >= SEND_INTERVAL) {
    lastSendTime = millis();
    
    Serial.printf("\n--- CHU KỲ ĐỒNG BỘ ---\n[Cảm biến] Raw ADC: %d, Quy đổi (0-100%%): %d%%, Ngưỡng: %d%%\n", rawADC, do_am_dat, nguong_kho);
    
    // 1. Đọc dữ liệu điều khiển từ Web xuống trước
    syncWithFirebase();

    // 2. Biện luận Logic tưới dựa trên chế độ vừa cập nhật
    if (che_do == 0) { // Chế độ Tự động (Auto)
      if (do_am_dat < nguong_kho) { // Độ ẩm tụt dưới ngưỡng tưới -> Bật bơm
        if (trang_thai_bom == 0) {
          Serial.printf("[Logic Auto] Đất khô (%d%% < %d%%) -> BẬT BƠM\n", do_am_dat, nguong_kho);
          controlPump(1);
        }
      } 
      else if (do_am_dat >= (nguong_kho + HYSTERESIS)) { // Đất ẩm vượt qua ngưỡng + độ trễ -> Tắt bơm
        if (trang_thai_bom == 1) {
          Serial.printf("[Logic Auto] Đất đủ ẩm (%d%% >= %d%%) -> TẮT BƠM\n", do_am_dat, nguong_kho + HYSTERESIS);
          controlPump(0);
        }
      }
    } 
    else { // Chế độ Thủ công (Manual)
      if (bom_thu_cong != trang_thai_bom) {
        Serial.printf("[Logic Manual] Trạng thái bơm -> %s\n", bom_thu_cong == 1 ? "BẬT" : "TẮT");
        controlPump(bom_thu_cong);
      }
    }

    // 3. Đẩy đồng bộ ngược lại toàn bộ 8 biến lên Firebase
    uploadDataToFirebase(rawADC);
  }

  // Cập nhật trạng thái nháy đèn LED liên tục (không chặn)
  updateLED();

  delay(10);
}