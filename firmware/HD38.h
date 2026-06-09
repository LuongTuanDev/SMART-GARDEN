#ifndef HD38_H
#define HD38_H

#include <Arduino.h>

// Hàm lọc nhiễu trung vị đọc từ cảm biến HD38
int readSoilMoisture(int pin) {
  int samples[5];
  
  // Đọc cảm biến liên tiếp 5 lần cách nhau 20ms
  for (int i = 0; i < 5; i++) {
    int raw = analogRead(pin);
    Serial.printf("[Debug Phần Cứng] Đọc raw trực tiếp từ chân pin: %d\n", raw);
    samples[i] = map(raw, 0, 4095, 0, 1023); // Khớp dải đo với ESP32 và Web
    delay(20);
  }

  // Sắp xếp nổi bọt từ nhỏ đến lớn
  for (int i = 0; i < 4; i++) {
    for (int j = i + 1; j < 5; j++) {
      if (samples[i] > samples[j]) {
        int temp = samples[i];
        samples[i] = samples[j];
        samples[j] = temp;
      }
    }
  }

  // Trả về giá trị ở giữa để loại bỏ nhiễu đỉnh
  return samples[2];
}

#endif
