# Hệ Thống Tưới Cây Tự Động Thông Minh (Smart Garden IoT)

Hệ thống giám sát độ ẩm đất và điều khiển máy bơm tự động/thủ công qua WiFi, đồng bộ hóa thời gian thực (Real-time) với Web Dashboard thông qua Firebase Realtime Database.

---

## 📁 Cấu Trúc Thư Mục Dự Án
```
d:\apptuoicaytudong\
├── firmware\
│   └── firmware.ino          # Mã nguồn nạp cho vi điều khiển ESP8266
├── index.html                # Trang giao diện Web Dashboard chính
├── style.css                 # Thiết kế giao diện (Glassmorphism, Dark mode, Animation)
├── app.js                    # Logic đồng bộ Firebase, vẽ biểu đồ & lưu cấu hình
└── README.md                 # Tài liệu hướng dẫn sử dụng này
```

---

## ⚙️ Hướng Dẫn Chuẩn Bị Trên Firebase Realtime Database

1. Truy cập trang quản trị [Firebase Console](https://console.firebase.google.com/).
2. Tạo một Project mới (ví dụ: `smart-garden-iot`).
3. Tạo **Realtime Database**:
   - Chọn khu vực (Region) thích hợp (ví dụ: Singapore - `asia-southeast1`).
   - Ở mục bảo mật (Rules Setup), chọn **Start in test mode** để bật quyền đọc/ghi tự do cho quá trình phát triển ban đầu.
   - Nhấp vào nút **Rules** và đảm bảo luật bảo mật có nội dung như sau:
     ```json
     {
       "rules": {
         ".read": "true",
         ".write": "true"
       }
     }
     ```
4. Khởi tạo cấu trúc dữ liệu ban đầu bằng cách import file JSON hoặc tạo thủ công trên giao diện Firebase với cấu trúc chính xác như sau:
   ```json
   {
     "HeThongTuoi": {
       "do_am_dat": 0,
       "nguong_kho": 600,
       "che_do": 0,
       "bom_thu_cong": 0,
       "trang_thai_bom": 0
     }
   }
   ```
5. Nhấy vào **Project Settings** (Biểu tượng bánh răng ở góc trên bên trái) -> Chọn tab **General** -> Đi tới mục **Your apps** -> Chọn biểu tượng Web (`</>`) để đăng ký ứng dụng Web và lấy các thông số `firebaseConfig` (Database URL, API Key, Auth Domain, Project ID).

---

## 🔌 Hướng Dẫn Nạp Chương Trình Cho ESP8266 (Firmware)

### 1. Sơ đồ nối dây linh kiện
| Linh kiện | Chân trên ESP8266 NodeMCU | Ghi chú |
| :--- | :--- | :--- |
| **Cảm biến độ ẩm đất** | **A0** (Analog Input) | Đọc giá trị tương tự từ 0 (ướt nhất) đến 1023 (khô nhất) |
| **Relay điều khiển Bơm** | **D1** (GPIO 5) | Kích mức HIGH để bật bơm (Có thể thay đổi trong code nếu Relay kích mức LOW) |
| **Nút nhấn Reset WiFi** | **D2** (GPIO 4) | Một chân nối D2, chân còn lại nối GND (Nút nhấn kéo xuống GND) |
| **Đèn LED Trạng thái** | **D4** (GPIO 2 - Built-in) | Đèn LED tích hợp sẵn trên mạch. Sáng mờ khi chưa cấu hình WiFi |

### 2. Cài đặt thư viện trên Arduino IDE
Mở Arduino IDE -> Đi tới **Tools** -> **Manage Libraries...** và cài đặt các thư viện sau:
1. `WiFiManager` (bởi *tzapu*)
2. `Firebase ESP Client` (bởi *Mobizt*)
3. `ArduinoJson` (bởi *Benoit Blanchon*)

### 3. Nạp code
- Mở file `firmware/firmware.ino` trong thư mục dự án bằng Arduino IDE.
- Cập nhật thông tin cấu hình Firebase của bạn ở phần khai báo:
  ```cpp
  #define FIREBASE_HOST "ten-du-an-default-rtdb.firebaseio.com" // Thay bằng Database URL của bạn (không chứa https://)
  #define API_KEY "AIzaSy..."                                   // Thay bằng API Key của bạn
  ```
- Kết nối mạch ESP8266 với máy tính, chọn đúng Cổng COM (Port) và Board tương ứng (ví dụ: `NodeMCU 1.0 (ESP-12E Module)`).
- Nhấn **Upload** (Mũi tên sang phải) để nạp code.

### 4. Vận hành lần đầu & Cấu hình WiFi (WiFi Provisioning)
- Khi nạp code lần đầu tiên hoặc khi không có mạng WiFi đã lưu, đèn LED trên mạch sẽ sáng mờ báo hiệu chế độ cấu hình.
- ESP8266 sẽ tự động phát ra một mạng WiFi tên là **`HeThongTuoi_CapNhatWiFi`** (không mật khẩu).
- Dùng điện thoại hoặc máy tính kết nối vào mạng WiFi này. Một giao diện web cấu hình sẽ tự động hiển thị (nếu không hiển thị, mở trình duyệt gõ địa chỉ `192.168.4.1`).
- Nhấp chọn WiFi nhà bạn, nhập mật khẩu WiFi và nhấn **Save**.
- ESP8266 sẽ tự động lưu mật khẩu, tắt đèn LED mờ và kết nối Internet để đồng bộ với Firebase.

### 5. Cách reset mạng WiFi (Nút nhấn 5 giây)
- Khi muốn thay đổi sang một mạng WiFi mới, trong lúc mạch đang hoạt động bình thường, hãy **nhấn và giữ nút nhấn ở chân D2 trong 5 giây**.
- Đèn LED trên mạch sẽ chớp nháy nhanh liên tục trong 3 giây để báo hiệu đã xóa bộ nhớ WiFi cũ.
- Chip sẽ tự động khởi động lại và mở lại cổng phát WiFi **`HeThongTuoi_CapNhatWiFi`** để bạn cấu hình mạng mới từ đầu.

---

## 🖥️ Cách Chạy Web Dashboard Điều Khiển

1. Không cần cài đặt Node.js hay chạy các lệnh phức tạp. Bạn chỉ cần **click đúp vào file `index.html`** để mở trang giao diện trực tiếp trên bất kỳ trình duyệt nào (Chrome, Edge, Safari, Firefox).
2. Lần chạy đầu tiên, giao diện sẽ hiển thị trạng thái "Chưa cấu hình" và tự động mở một bảng cài đặt **Cấu hình Firebase**.
3. Bạn nhập đầy đủ các thông số Firebase lấy được ở bước thiết lập Firebase Console (Database URL, API Key, Auth Domain, Project ID) rồi nhấn **Lưu cấu hình**.
4. Trang web sẽ tự động lưu cấu hình vào bộ nhớ trình duyệt (`localStorage`) và tự động kết nối trực tiếp đến Firebase của bạn. Từ lúc này, mọi tương tác của cảm biến và nút gạt sẽ đồng bộ thời gian thực:
   - **Độ ẩm đất** cập nhật liên tục dưới dạng vòng tròn tiến độ chuyển màu (Đỏ = Khô, Xanh lá = Đủ ẩm, Xanh dương = Quá ướt).
   - **Trạng thái bơm** hiển thị trực quan bằng hoạt ảnh vòi nước rung lắc và nhỏ giọt khi bơm đang hoạt động.
   - **Chế độ tự động/Thủ công** đồng bộ 2 chiều (Chuyển chế độ trên App sẽ làm thay đổi hành vi xử lý của ESP8266 ngay lập tức).
   - **Nút bật bơm thủ công** chỉ kích hoạt khi bạn tắt chế độ tự động (chuyển sang Thủ công).
   - **Thanh trượt Ngưỡng tưới** cho phép bạn thay đổi ngưỡng khô mà ESP8266 dùng để quyết định bật bơm ở chế độ Tự động.
   - **Biểu đồ đường thời gian thực** vẽ lại diễn biến độ ẩm đất cùng với đường ngưỡng đứt nét để bạn dễ quan sát xu hướng ẩm của đất.
   - **Nhật ký hoạt động** ghi nhận mọi tiến trình bật tắt máy bơm, đổi chế độ hay thay đổi ngưỡng cấu hình theo thời gian thực.
