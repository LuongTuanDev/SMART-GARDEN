// --- ĐỐI TƯỢNG CẤU HÌNH & KẾT NỐI ---
let db;

// Các phần tử giao diện (DOM Elements)
const dbStatusDot = document.getElementById('db-status-dot');
const dbStatusText = document.getElementById('db-status-text');
const soilValue = document.getElementById('soil-value');
const soilGaugeBar = document.getElementById('soil-gauge-bar');
const soilStatusText = document.getElementById('soil-status-text');
const pumpStatusBadge = document.getElementById('pump-status-badge');
const pumpModeIndicator = document.getElementById('pump-mode-indicator');
const pumpDisplayContainer = document.getElementById('pump-display-container');

// Các nút điều khiển tác vụ
const modeSwitch = document.getElementById('mode-switch');
const pumpSwitch = document.getElementById('pump-switch');
const manualPumpItem = document.getElementById('manual-pump-item');
const thresholdSlider = document.getElementById('threshold-slider');
const thresholdVal = document.getElementById('threshold-val');

// Modal cài đặt
const openSettingsBtn = document.getElementById('open-settings-btn');
const closeSettingsBtn = document.getElementById('close-settings-btn');
const cancelSettingsBtn = document.getElementById('cancel-settings-btn');
const settingsOverlay = document.getElementById('settings-overlay');
const firebaseConfigForm = document.getElementById('firebase-config-form');

// Nhật ký hệ thống
const logsContainer = document.getElementById('logs-container');

// --- QUẢN LÝ NHẬT KÝ (LOGS) ---
function addLog(text, type = 'info') {
    const now = new Date();
    const timeStr = now.toTimeString().split(' ')[0];
    const li = document.createElement('li');
    li.className = `log-entry ${type}`;
    li.innerHTML = `
        <div class="log-meta">
            <span>${type === 'info' ? 'Hệ thống' : 'Firebase'}</span>
            <span class="log-time">${timeStr}</span>
        </div>
        <div class="log-text">${text}</div>
    `;
    logsContainer.insertBefore(li, logsContainer.firstChild);
}

// --- KHỞI TẠO BIỂU ĐỒ REALTIME (CHART.JS) ---
const ctx = document.getElementById('moisture-chart').getContext('2d');
const moistureChart = new Chart(ctx, {
    type: 'line',
    data: {
        labels: [],
        datasets: [{
            label: 'Độ ẩm đất (%)',
            data: [],
            borderColor: '#2ecc71',
            backgroundColor: 'rgba(46, 204, 113, 0.1)',
            borderWidth: 2,
            tension: 0.3,
            fill: true
        }]
    },
    options: {
        responsive: true,
        maintainAspectRatio: false,
        scales: {
            y: { min: 0, max: 100 }
        }
    }
});

function updateChart(value) {
    const now = new Date();
    const timeStr = now.toTimeString().split(' ')[0];
    if (moistureChart.data.labels.length > 10) {
        moistureChart.data.labels.shift();
        moistureChart.data.datasets[0].data.shift();
    }
    moistureChart.data.labels.push(timeStr);
    moistureChart.data.datasets[0].data.push(value);
    moistureChart.update();
}

// --- QUẢN LÝ MODAL THEO CLASS ACTIVE CỦA CSS ---
openSettingsBtn.addEventListener('click', () => {
    settingsOverlay.classList.add('active');
    // Khi mở cài đặt, hiển thị các giá trị hiệu chuẩn hiện tại từ Firebase
    if (db) {
        db.ref('/HeThongTuoi').once('value').then(snapshot => {
            const data = snapshot.val();
            if (data) {
                if (data.adc_kho !== undefined) document.getElementById('cfg-adc-kho').value = data.adc_kho;
                if (data.adc_uot !== undefined) document.getElementById('cfg-adc-uot').value = data.adc_uot;
            }
        });
    }
});
[closeSettingsBtn, cancelSettingsBtn].forEach(btn => btn.addEventListener('click', () => settingsOverlay.classList.remove('active')));

// Đóng modal khi nhấn phím Escape (ESC)
document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape' && settingsOverlay.classList.contains('active')) {
        settingsOverlay.classList.remove('active');
    }
});

firebaseConfigForm.addEventListener('submit', (e) => {
    e.preventDefault();
    const config = {
        databaseURL: document.getElementById('cfg-db-url').value.trim(),
        apiKey: document.getElementById('cfg-api-key').value.trim(),
        authDomain: document.getElementById('cfg-auth-domain').value.trim(),
        projectId: document.getElementById('cfg-project-id').value.trim(),
        appId: document.getElementById('cfg-app-id').value.trim()
    };
    localStorage.setItem('firebaseConfig', JSON.stringify(config));
    
    // Lưu thêm các giá trị hiệu chuẩn cảm biến lên Firebase trước khi reload
    if (db) {
        const adcKho = parseInt(document.getElementById('cfg-adc-kho').value) || 4095;
        const adcUot = parseInt(document.getElementById('cfg-adc-uot').value) || 1000;
        db.ref('/HeThongTuoi').update({
            adc_kho: adcKho,
            adc_uot: adcUot
        }).then(() => {
            addLog("Đã lưu cấu hình & thông số hiệu chuẩn cảm biến thành công!", "success");
            setTimeout(() => location.reload(), 1000);
        }).catch(err => {
            alert("Lỗi lưu hiệu chuẩn cảm biến: " + err.message);
            location.reload();
        });
    } else {
        addLog("Đã lưu cấu hình mới. Trình duyệt đang tải lại...", "info");
        setTimeout(() => location.reload(), 1000);
    }
});

// --- KẾT NỐI VÀ ĐỒNG BỘ DATA REALTIME ---
function initFirebase() {
    const savedConfig = localStorage.getItem('firebaseConfig');
    if (!savedConfig) {
        dbStatusDot.className = "status-dot";
        dbStatusText.innerText = "Chưa cấu hình Firebase";
        addLog("Vui lòng thiết lập cấu hình kết nối tới database.", "info");
        return;
    }

    const firebaseConfig = JSON.parse(savedConfig);

    document.getElementById('cfg-db-url').value = firebaseConfig.databaseURL || '';
    document.getElementById('cfg-api-key').value = firebaseConfig.apiKey || '';
    document.getElementById('cfg-auth-domain').value = firebaseConfig.authDomain || '';
    document.getElementById('cfg-project-id').value = firebaseConfig.projectId || '';
    document.getElementById('cfg-app-id').value = firebaseConfig.appId || '';

    // Khởi tạo Firebase SDK
    firebase.initializeApp(firebaseConfig);
    db = firebase.database();

    dbStatusDot.className = "status-dot connected";
    dbStatusText.innerText = "Đã kết nối trực tuyến";
    addLog("Kết nối Realtime Database thành công!", "success");

    // LẮNG NGHE SỰ THAY ĐỔI TỪ ESP32 ĐẨY LÊN
    db.ref('/HeThongTuoi').on('value', (snapshot) => {
        const data = snapshot.val();
        if (!data) return;

        // 1. Cập nhật độ ẩm đất hiển thị
        if (data.do_am_dat !== undefined) {
            soilValue.innerText = data.do_am_dat + "%";
            const percentage = data.do_am_dat;
            const offset = 527 - (527 * percentage) / 100;
            soilGaugeBar.style.strokeDashoffset = offset;

            if (data.do_am_dat < data.nguong_kho) {
                soilStatusText.innerText = "Trạng thái: Đất khô (Đang khát nước)";
                soilStatusText.style.color = "#e74c3c";
                soilGaugeBar.style.stroke = "#e74c3c"; // Đỏ khi khô
            } else {
                soilStatusText.innerText = "Trạng thái: Đất đủ ẩm";
                soilStatusText.style.color = "#2ecc71";
                soilGaugeBar.style.stroke = "#2ecc71"; // Xanh lá khi đủ ẩm
            }
            updateChart(data.do_am_dat);
        }

        // Cập nhật giá trị ADC thô hiển thị trong modal cài đặt
        if (data.raw_adc !== undefined) {
            const rawAdcEl = document.getElementById('cfg-raw-adc');
            if (rawAdcEl) rawAdcEl.value = data.raw_adc;
        }

        // 2. Cập nhật trạng thái máy bơm thực tế từ phần cứng
        if (data.trang_thai_bom !== undefined) {
            if (data.trang_thai_bom === 1) {
                pumpStatusBadge.innerText = "ĐANG BẬT";
                pumpStatusBadge.className = "status-pill pill-on";
                pumpDisplayContainer.classList.add('pump-active');
            } else {
                pumpStatusBadge.innerText = "ĐANG TẮT";
                pumpStatusBadge.className = "status-pill pill-off";
                pumpDisplayContainer.classList.remove('pump-active');
            }
        }

        // 3. ĐỒNG BỘ ĐỘC LẬP TRẠNG THÁI NÚT GẠT (ĐÃ SỬA LỖI ĐƠ KHI ĐANG FOCUS)
        if (data.che_do !== undefined) {
            modeSwitch.checked = (data.che_do === 0);
            pumpModeIndicator.innerText = `Chế độ: ${data.che_do === 0 ? 'Tự động (Auto)' : 'Thủ công (Manual)'}`;

            if (data.che_do === 0) {
                manualPumpItem.classList.add('disabled-item');
                pumpSwitch.disabled = true;
            } else {
                manualPumpItem.classList.remove('disabled-item');
                pumpSwitch.disabled = false;
            }
        }
        if (data.bom_thu_cong !== undefined) {
            pumpSwitch.checked = (data.bom_thu_cong === 1);
        }
        if (data.nguong_kho !== undefined) {
            thresholdSlider.value = data.nguong_kho;
            thresholdVal.innerText = data.nguong_kho + "%";
        }
    });
}

// --- 🔴 SỰ KIỆN ĐIỀU KHIỂN TỪ WEB ĐẨY XUỐNG FIREBASE ---

// 1. Thay đổi trạng thái Chế Độ (Auto = 0, Manual = 1)
modeSwitch.addEventListener('change', () => {
    if (!db) return;
    const isAuto = modeSwitch.checked;
    const numericMode = isAuto ? 0 : 1;

    db.ref('/HeThongTuoi').update({ che_do: numericMode })
        .then(() => {
            addLog(`Hạ lệnh chuyển chế độ: ${isAuto ? 'TỰ ĐỘNG' : 'THỦ CÔNG'}`, 'success');
            pumpModeIndicator.innerText = `Chế độ: ${isAuto ? 'Tự động (Auto)' : 'Thủ công (Manual)'}`;

            if (isAuto) {
                manualPumpItem.classList.add('disabled-item');
                pumpSwitch.disabled = true;
                pumpSwitch.checked = false;
                db.ref('/HeThongTuoi').update({ bom_thu_cong: 0 });
            } else {
                manualPumpItem.classList.remove('disabled-item');
                pumpSwitch.disabled = false;
            }
        })
        .catch(err => addLog(`Lỗi đồng bộ: ${err.message}`, 'warning'));
});

// 2. Bật/tắt Bơm Thủ Công
pumpSwitch.addEventListener('change', () => {
    if (!db) return;
    const isPumpOn = pumpSwitch.checked;
    const numericPumpState = isPumpOn ? 1 : 0;

    db.ref('/HeThongTuoi').update({ bom_thu_cong: numericPumpState })
        .then(() => {
            addLog(`Hạ lệnh điều khiển: ${isPumpOn ? 'BẬT BƠM' : 'TẮT BƠM'}`, 'success');
        })
        .catch(err => addLog(`Lỗi kích bơm: ${err.message}`, 'warning'));
});

// 3. Thay đổi Ngưỡng Tưới Tự Động
thresholdSlider.addEventListener('input', () => {
    thresholdVal.innerText = thresholdSlider.value + "%";
});

thresholdSlider.addEventListener('change', () => {
    if (!db) return;
    const newVal = parseInt(thresholdSlider.value);

    db.ref('/HeThongTuoi').update({ nguong_kho: newVal })
        .then(() => {
            addLog(`Cập nhật ngưỡng tưới mới: ${newVal}`, 'success');
        })
        .catch(err => addLog(`Lỗi đồng bộ ngưỡng: ${err.message}`, 'warning'));
});

window.onload = initFirebase;