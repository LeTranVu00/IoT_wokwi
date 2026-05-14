#include <WiFi.h>
#include <PubSubClient.h>
#include <DHTesp.h>
#include <ESP32Servo.h>

// =========================================
// WIFI
// =========================================

const char* ssid = "Wokwi-GUEST";
const char* password = "";

// =========================================
// MQTT
// =========================================

const char* mqtt_server = "broker.hivemq.com";

WiFiClient espClient;
PubSubClient client(espClient);

// =========================================
// DHT
// =========================================

DHTesp dhtSensor;

// =========================================
// SERVO
// =========================================

Servo gateServo;

// =========================================
// LED PINS
// =========================================

const int bedroomLight1 = 25;
const int bedroomLight2 = 26;

const int livingroomLight1 = 32;

const int kitchenLight1 = 33;

const int outdoorLight1 = 4;

// =========================================
// BUTTON PINS
// =========================================

const int btnBedroom1 = 21;
const int btnBedroom2 = 5;

const int btnLivingroom = 16;

const int btnKitchen = 17;

const int btnOutdoor = 23;

// =========================================
// SENSOR PINS
// =========================================

const int DHT_PIN = 27;

const int PIR_PIN = 14;

const int GAS_PIN = 34;

const int LDR_PIN = 35;

// HC-SR04

const int TRIG_PIN = 18;
const int ECHO_PIN = 19;

// Buzzer

const int BUZZER_PIN = 12;

// Servo

const int SERVO_PIN = 13;

// =========================================
// STATES
// =========================================

bool bedroom1State = false;
bool bedroom2State = false;

bool livingroomState = false;

bool kitchenState = false;

bool outdoorState = false;

// button states

bool lastBtnBedroom1 = HIGH;
bool lastBtnBedroom2 = HIGH;

bool lastBtnLivingroom = HIGH;

bool lastBtnKitchen = HIGH;

bool lastBtnOutdoor = HIGH;

// =========================================
// LAST SENSOR VALUES
// =========================================

float lastTemp = -999;
float lastHumidity = -999;

int lastGas = -1;
int lastLdr = -1;

float lastDistance = -1;

bool lastMotion = false;

// =========================================
// TIMING
// =========================================

unsigned long lastSensorTime = 0;
const unsigned long SENSOR_INTERVAL = 500;

unsigned long lastDebounceTime1 = 0;
unsigned long lastDebounceTime2 = 0;
unsigned long lastDebounceTime3 = 0;
unsigned long lastDebounceTime4 = 0;
unsigned long lastDebounceTime5 = 0;
const unsigned long DEBOUNCE_DELAY = 50;

// =========================================
// AUTOMATION
// =========================================

// B. PIR - auto tắt đèn phòng khách sau 10 giây
unsigned long pirTriggerTime = 0;
bool pirAutoOn = false;
const unsigned long PIR_TIMEOUT = 10000; // 10 giây

// C. Gas threshold
const int GAS_THRESHOLD = 2000;
bool gasAlertActive = false;

// A. LDR threshold
const int LDR_DARK_THRESHOLD = 500;

// =========================================
// MANUAL OVERRIDE FLAGS
// =========================================

// Tắt còi báo từ app (reset khi gas về an toàn)
bool buzzerMuted = false;

// Override cổng thủ công từ app (30 giây)
bool manualGateActive = false;
unsigned long manualGateTime = 0;
const unsigned long MANUAL_GATE_TIMEOUT = 30000;

// Chống lệnh cổng rác gửi lại sau khi reconnect
unsigned long lastGateCmdTime = 0;
const unsigned long GATE_CMD_DEBOUNCE = 2000; // 2 giây
String lastGateCmd = "";

// Trạng thái cổng hiện tại (tránh ghi servo & publish liên tục)
bool gateIsOpen = false;

// Hysteresis: mở khi < OPEN_DIST, đóng khi > CLOSE_DIST
const float GATE_OPEN_DIST  = 20.0;  // cm - có vật → mở
const float GATE_CLOSE_DIST = 25.0;  // cm - không còn vật → đóng

// Servo angles for open/close (degrees)
// NOTE: self-test ends at 90° → dùng 0° = MỞ để chuyển động rõ ràng
const int GATE_OPEN_ANGLE  = 0;  // mở cửa: về 0°
const int GATE_CLOSE_ANGLE = 90; // đóng cửa: xoay 90°

// Timer: bắt đầu đếm khi distance > 25cm, đủ 3s thì đóng
unsigned long distanceAboveThresholdSince = 0;
const unsigned long GATE_CLOSE_DELAY = 3000; // 3 giây

// Override đèn sân thủ công từ app
bool manualOutdoorOverride = false;

// HC-SR04 tự động: bật/tắt chế độ auto
bool gateAutoDetectEnabled = true;

// =========================================
// WIFI CONNECT
// =========================================

void setup_wifi() {

  delay(10);

  Serial.println();
  Serial.print("Connecting WiFi");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);

    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
}

// =========================================
// MQTT PUBLISH
// =========================================

void publishMessage(const char* topic, String message) {

  client.publish(topic, message.c_str());
}

// =========================================
// UPDATE DEVICE
// =========================================

void updateDevice(
  int pin,
  bool &stateVar,
  bool state,
  const char* statusTopic
) {

  stateVar = state;

  digitalWrite(pin, state);

  publishMessage(
    statusTopic,
    state ? "ON" : "OFF"
  );

  Serial.print(statusTopic);
  Serial.print(" -> ");

  Serial.println(state ? "ON" : "OFF");
}

// =========================================
// TOGGLE DEVICE
// =========================================

void toggleDevice(
  int pin,
  bool &stateVar,
  const char* statusTopic
) {

  stateVar = !stateVar;

  digitalWrite(pin, stateVar);

  publishMessage(
    statusTopic,
    stateVar ? "ON" : "OFF"
  );
}

// =========================================
// MQTT CALLBACK
// =========================================

void callback(char* topic, byte* payload, unsigned int length) {

  String message = "";

  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  message.toUpperCase();

  String topicStr = String(topic);

  Serial.println("==============");
  Serial.println(topicStr);
  Serial.println(message);

  // =====================================
  // BEDROOM LIGHT 1
  // =====================================

  if (topicStr == "smarthome/bedroom/light1") {

    updateDevice(
      bedroomLight1,
      bedroom1State,
      message == "ON",
      "smarthome/bedroom/light1/status"
    );
  }

  // =====================================
  // BEDROOM LIGHT 2
  // =====================================

  else if (topicStr == "smarthome/bedroom/light2") {

    updateDevice(
      bedroomLight2,
      bedroom2State,
      message == "ON",
      "smarthome/bedroom/light2/status"
    );
  }

  // =====================================
  // LIVING ROOM
  // =====================================

  else if (topicStr == "smarthome/livingroom/light1") {

    updateDevice(
      livingroomLight1,
      livingroomState,
      message == "ON",
      "smarthome/livingroom/light1/status"
    );
  }

  // =====================================
  // KITCHEN
  // =====================================

  else if (topicStr == "smarthome/kitchen/light1") {

    updateDevice(
      kitchenLight1,
      kitchenState,
      message == "ON",
      "smarthome/kitchen/light1/status"
    );
  }

  // =====================================
  // OUTDOOR
  // =====================================

  else if (topicStr == "smarthome/outdoor/light1" || topicStr == "smarthome/outdoor/light1/control") {

    if (message == "AUTO") {
      // App chọn chế độ tự động → bật lại auto LDR
      manualOutdoorOverride = false;
      Serial.println("[MANUAL] Outdoor light -> AUTO mode");
    } else {
      // App điều khiển thủ công (ON/OFF) → tắt auto LDR
      manualOutdoorOverride = true;

      updateDevice(
        outdoorLight1,
        outdoorState,
        message == "ON",
        "smarthome/outdoor/light1/status"
      );
    }
  }

  // =====================================
  // SERVO GATE
  // =====================================

  else if (topicStr == "smarthome/outdoor/gate") {

    // Bỏ qua lệnh trùng lặp trong vòng 2 giây (chặn message cũ từ broker)
    unsigned long now_cb = millis();
    if (message == lastGateCmd && (now_cb - lastGateCmdTime < GATE_CMD_DEBOUNCE)) {
      Serial.println("[GATE] Duplicate command ignored");
      return;
    }
    lastGateCmd = message;
    lastGateCmdTime = now_cb;

    // App điều khiển thủ công → tắt auto HC-SR04 trong 30 giây
    manualGateActive = true;
    manualGateTime = millis();

    if (message == "OPEN") {
      gateIsOpen = true;
      Serial.print("[MANUAL] gateServo.write(");
      Serial.print(GATE_OPEN_ANGLE);
      Serial.println(") -> OPEN");
      gateServo.write(GATE_OPEN_ANGLE);

      publishMessage(
        "smarthome/outdoor/gate/status",
        "OPEN"
      );

      Serial.println("[MANUAL] Gate OPEN from app");
    }

    else if (message == "CLOSE") {
      gateIsOpen = false;
      Serial.print("[MANUAL] gateServo.write(");
      Serial.print(GATE_CLOSE_ANGLE);
      Serial.println(") -> CLOSE");
      gateServo.write(GATE_CLOSE_ANGLE);

      publishMessage(
        "smarthome/outdoor/gate/status",
        "CLOSE"
      );

      Serial.println("[MANUAL] Gate CLOSE from app");
    }
  }

  // =====================================
  // GATE AUTO-DETECT TOGGLE
  // =====================================

  else if (topicStr == "smarthome/outdoor/gate/auto") {

    if (message == "ON") {
      gateAutoDetectEnabled = true;
      publishMessage(
        "smarthome/outdoor/gate/auto/status",
        "ENABLED"
      );
      Serial.println("[INFO] Gate auto-detect ENABLED");
    }

    else if (message == "OFF") {
      gateAutoDetectEnabled = false;
      publishMessage(
        "smarthome/outdoor/gate/auto/status",
        "DISABLED"
      );
      Serial.println("[INFO] Gate auto-detect DISABLED");
    }
  }

  // =====================================
  // BUZZER MUTE
  // =====================================

  else if (topicStr == "smarthome/kitchen/buzzer") {

    if (message == "OFF") {

      buzzerMuted = true;
      noTone(BUZZER_PIN);

      publishMessage(
        "smarthome/kitchen/buzzer/status",
        "MUTED"
      );

      Serial.println("[INFO] Buzzer muted from app.");
    }

    else if (message == "ON") {

      buzzerMuted = false;
      
      if (gasAlertActive) {
        tone(BUZZER_PIN, 1000);
      }

      publishMessage(
        "smarthome/kitchen/buzzer/status",
        "ACTIVE"
      );

      Serial.println("[INFO] Buzzer unmuted.");
    }
  }
}

// =========================================
// MQTT RECONNECT
// =========================================

void reconnect() {

  while (!client.connected()) {

    Serial.print("Connecting MQTT...");

    // cleanSession=true: xóa sạch session cũ, tránh nhận message đọc từ broker
    if (client.connect("SmartHomeESP32", nullptr, nullptr,
                       nullptr, 0, false, nullptr, true)) {

      Serial.println("connected");

      client.subscribe("smarthome/bedroom/light1");
      client.subscribe("smarthome/bedroom/light2");

      client.subscribe("smarthome/livingroom/light1");

      client.subscribe("smarthome/kitchen/light1");

      client.subscribe("smarthome/outdoor/light1");
      client.subscribe("smarthome/outdoor/light1/control");

      client.subscribe("smarthome/outdoor/gate");
      client.subscribe("smarthome/outdoor/gate/auto");

      client.subscribe("smarthome/kitchen/buzzer");

      Serial.println("All topics subscribed");

      // Reattach servo sau khi MQTT connect để làm mới PWM
      // (tránh WiFi/MQTT cướp mất timer PWM của servo)
      gateServo.detach();
      delay(50);
      gateServo.attach(SERVO_PIN, 500, 2400);
      gateServo.write(gateIsOpen ? GATE_OPEN_ANGLE : GATE_CLOSE_ANGLE);
      Serial.println("[SERVO] Reattached after MQTT connect -> PWM refreshed");
    }

    else {

      Serial.print("failed rc=");
      Serial.println(client.state());

      delay(2000);
    }
  }
}

// =========================================
// READ HC-SR04
// =========================================

float readDistance() {

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // timeout 30ms

  float distance = duration * 0.034 / 2;

  return distance;
}

// =========================================
// SETUP
// =========================================

void setup() {

  Serial.begin(115200);

  // =====================================
  // LED OUTPUT
  // =====================================

  pinMode(bedroomLight1, OUTPUT);
  pinMode(bedroomLight2, OUTPUT);

  pinMode(livingroomLight1, OUTPUT);

  pinMode(kitchenLight1, OUTPUT);

  pinMode(outdoorLight1, OUTPUT);

  // Tắt tất cả đèn khi khởi động
  digitalWrite(bedroomLight1, LOW);
  digitalWrite(bedroomLight2, LOW);
  digitalWrite(livingroomLight1, LOW);
  digitalWrite(kitchenLight1, LOW);
  digitalWrite(outdoorLight1, LOW);

  // =====================================
  // BUTTON INPUT
  // =====================================

  pinMode(btnBedroom1, INPUT_PULLUP);
  pinMode(btnBedroom2, INPUT_PULLUP);

  pinMode(btnLivingroom, INPUT_PULLUP);

  pinMode(btnKitchen, INPUT_PULLUP);

  pinMode(btnOutdoor, INPUT_PULLUP);

  // Đọc trạng thái thực của button để tránh trigger giả khi boot
  delay(50); // Chờ pull-up ổn định
  lastBtnBedroom1 = digitalRead(btnBedroom1);
  lastBtnBedroom2 = digitalRead(btnBedroom2);
  lastBtnLivingroom = digitalRead(btnLivingroom);
  lastBtnKitchen = digitalRead(btnKitchen);
  lastBtnOutdoor = digitalRead(btnOutdoor);

  // =====================================
  // SENSOR INPUT
  // =====================================

  pinMode(PIR_PIN, INPUT);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // =====================================
  // BUZZER
  // =====================================

  pinMode(BUZZER_PIN, OUTPUT);

  // =====================================
  // DHT
  // =====================================

  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);

  // =====================================
  // SERVO
  // =====================================

  // -------------------------------------------------------
  // QUAN TRỌNG: WiFi ESP32 dùng Timer 0 & Timer 1 nội bộ.
  // Chỉ cấp Timer 2 & 3 cho servo để tránh xung đột PWM.
  // -------------------------------------------------------
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  gateServo.setPeriodHertz(50);           // 50Hz - tiêu chuẩn servo
  gateServo.attach(SERVO_PIN, 500, 2400); // pulse: 500µs(0°) ~ 2400µs(180°)

  // Đặt servo về ĐÓNG (90°) ban đầu, self-test nhanh: 90->0->90
  gateServo.write(GATE_CLOSE_ANGLE);
  delay(800);
  gateServo.write(GATE_OPEN_ANGLE);
  delay(800);
  gateServo.write(GATE_CLOSE_ANGLE);
  delay(800);

  // =====================================
  // WIFI
  // =====================================

  setup_wifi();

  // Sau khi WiFi connect, reattach servo để làm mới tín hiệu PWM
  // (WiFi có thể làm gián đoạn timer trong quá trình kết nối)
  gateServo.detach();
  delay(100);
  gateServo.attach(SERVO_PIN, 500, 2400);
  gateServo.write(GATE_CLOSE_ANGLE); // giữ ở ĐÓNG(90°)
  Serial.println("[SERVO] Reattached after WiFi -> CLOSE(90 deg)");

  // =====================================
  // MQTT
  // =====================================

  client.setServer(mqtt_server, 1883);

  client.setCallback(callback);
}

// =========================================
// LOOP
// =========================================

void loop() {

  if (!client.connected()) {
    reconnect();
  }

  client.loop();

  unsigned long now = millis();

  // =====================================
  // BUTTONS (check every loop - no delay)
  // =====================================

  bool btn1 = digitalRead(btnBedroom1);
  bool btn2 = digitalRead(btnBedroom2);
  bool btn3 = digitalRead(btnLivingroom);
  bool btn4 = digitalRead(btnKitchen);
  bool btn5 = digitalRead(btnOutdoor);

  if (btn1 == LOW && lastBtnBedroom1 == HIGH &&
      (now - lastDebounceTime1 > DEBOUNCE_DELAY)) {
    lastDebounceTime1 = now;
    Serial.println("[BTN] Bedroom Light 1 pressed");
    toggleDevice(
      bedroomLight1,
      bedroom1State,
      "smarthome/bedroom/light1/status"
    );
  }

  if (btn2 == LOW && lastBtnBedroom2 == HIGH &&
      (now - lastDebounceTime2 > DEBOUNCE_DELAY)) {
    lastDebounceTime2 = now;
    Serial.println("[BTN] Bedroom Light 2 pressed");
    toggleDevice(
      bedroomLight2,
      bedroom2State,
      "smarthome/bedroom/light2/status"
    );
  }

  if (btn3 == LOW && lastBtnLivingroom == HIGH &&
      (now - lastDebounceTime3 > DEBOUNCE_DELAY)) {
    lastDebounceTime3 = now;
    Serial.println("[BTN] Livingroom Light pressed");
    // Reset PIR auto-on để tránh PIR override ngay sau khi nhấn button
    pirAutoOn = false;
    toggleDevice(
      livingroomLight1,
      livingroomState,
      "smarthome/livingroom/light1/status"
    );
  }

  if (btn4 == LOW && lastBtnKitchen == HIGH &&
      (now - lastDebounceTime4 > DEBOUNCE_DELAY)) {
    lastDebounceTime4 = now;
    Serial.println("[BTN] Kitchen Light pressed");
    toggleDevice(
      kitchenLight1,
      kitchenState,
      "smarthome/kitchen/light1/status"
    );
  }

  if (btn5 == LOW && lastBtnOutdoor == HIGH &&
      (now - lastDebounceTime5 > DEBOUNCE_DELAY)) {
    lastDebounceTime5 = now;
    Serial.println("[BTN] Outdoor Light pressed");
    // Set manual override để LDR không ghi đè ngay lập tức
    manualOutdoorOverride = true;
    toggleDevice(
      outdoorLight1,
      outdoorState,
      "smarthome/outdoor/light1/status"
    );
  }

  lastBtnBedroom1 = btn1;
  lastBtnBedroom2 = btn2;
  lastBtnLivingroom = btn3;
  lastBtnKitchen = btn4;
  lastBtnOutdoor = btn5;

  // =====================================
  // SENSORS (check every 500ms)
  // =====================================

  if (now - lastSensorTime >= SENSOR_INTERVAL) {

    lastSensorTime = now;

    // -----------------------------------
    // DHT22
    // -----------------------------------

    TempAndHumidity data =
        dhtSensor.getTempAndHumidity();

    float temperature = data.temperature;
    float humidity = data.humidity;

    if (!isnan(temperature)) {
      if (temperature != lastTemp) {
        publishMessage(
          "smarthome/bedroom/temperature",
          String(temperature)
        );
        lastTemp = temperature;
      }
    }

    if (!isnan(humidity)) {
      if (humidity != lastHumidity) {
        publishMessage(
          "smarthome/bedroom/humidity",
          String(humidity)
        );
        lastHumidity = humidity;
      }
    }

    // -----------------------------------
    // C. GAS SENSOR + BUZZER ALERT
    // -----------------------------------

    int gasValue = analogRead(GAS_PIN);

    if (gasValue != lastGas) {
      publishMessage(
        "smarthome/kitchen/gas",
        String(gasValue)
      );
      lastGas = gasValue;
    }

    if (gasValue > GAS_THRESHOLD) {

      // Chỉ kêu nếu chưa bị tắt từ app
      if (!buzzerMuted) {
        tone(BUZZER_PIN, 1000); // 1000 Hz
      }

      // Publish cảnh báo cho Flutter nếu chưa alert
      if (!gasAlertActive) {
        gasAlertActive = true;
        publishMessage(
          "smarthome/kitchen/gas/alert",
          "DANGER"
        );
        Serial.println("[ALERT] Gas leakage detected!");
      }

    } else {

      noTone(BUZZER_PIN); // Tắt âm thanh

      // Reset mute và alert khi gas về bình thường
      if (buzzerMuted) {
        buzzerMuted = false;
        publishMessage("smarthome/kitchen/buzzer/status", "ACTIVE");
      }

      if (gasAlertActive) {
        gasAlertActive = false;
        publishMessage(
          "smarthome/kitchen/gas/alert",
          "SAFE"
        );
        Serial.println("[INFO] Gas level normal.");
      }
    }

    // -----------------------------------
    // A. LDR - TỰ ĐỘNG ĐÈN SÂN
    // -----------------------------------

    int ldrValue = analogRead(LDR_PIN);

    if (ldrValue != lastLdr) {
      publishMessage(
        "smarthome/bedroom/lightlevel",
        String(ldrValue)
      );
      lastLdr = ldrValue;
    }

    // Trời tối → bật đèn sân (chỉ khi không có override thủ công)
    if (ldrValue < LDR_DARK_THRESHOLD && !outdoorState && !manualOutdoorOverride) {
      updateDevice(
        outdoorLight1,
        outdoorState,
        true,
        "smarthome/outdoor/light1/status"
      );
      Serial.println("[AUTO] LDR dark -> Outdoor light ON");
    }

    // Trời sáng → tắt đèn sân (chỉ khi không có override thủ công)
    else if (ldrValue >= LDR_DARK_THRESHOLD && outdoorState && !manualOutdoorOverride) {
      updateDevice(
        outdoorLight1,
        outdoorState,
        false,
        "smarthome/outdoor/light1/status"
      );
      Serial.println("[AUTO] LDR bright -> Outdoor light OFF");
    }

    // -----------------------------------
    // B. PIR - TỰ ĐỘNG ĐÈN PHÒNG KHÁCH
    // -----------------------------------

    bool motion = digitalRead(PIR_PIN);

    if (motion != lastMotion) {
      publishMessage(
        "smarthome/livingroom/motion",
        motion ? "DETECTED" : "CLEAR"
      );
      lastMotion = motion;
    }

    // Phát hiện chuyển động → bật đèn phòng khách
    if (motion && !livingroomState) {
      updateDevice(
        livingroomLight1,
        livingroomState,
        true,
        "smarthome/livingroom/light1/status"
      );
      pirTriggerTime = now;
      pirAutoOn = true;
      Serial.println("[AUTO] PIR motion -> Livingroom light ON");
    }

    // Có chuyển động tiếp → reset timer
    if (motion && pirAutoOn) {
      pirTriggerTime = now;
    }

    // -----------------------------------
    // D. HC-SR04 - CỔNG THÔNG MINH
    // -----------------------------------

    float distance = readDistance();

    // Publish khoảng cách lên topic riêng (không xung đột lệnh gate)
    if (abs(distance - lastDistance) > 1) {
      publishMessage(
        "smarthome/outdoor/distance",
        String(distance)
      );
      Serial.print("[SENSOR] Distance: ");
      Serial.print(distance);
      Serial.print(" cm  gateIsOpen=");
      Serial.println(gateIsOpen);
      lastDistance = distance;
    }

    // Reset manual override sau 30 giây
    if (manualGateActive &&
        (now - manualGateTime >= MANUAL_GATE_TIMEOUT)) {
      manualGateActive = false;
      Serial.println("[INFO] Manual gate override expired.");
    }

    // Auto HC-SR04 chỉ khi không có override thủ công VÀ tính năng auto được bật
    if (!manualGateActive && gateAutoDetectEnabled) {

      // --- MỞ CỔNG: khoảng cách < 25cm ---
      if (!gateIsOpen && distance > 0 && distance < 25.0) {

        gateIsOpen = true;
        distanceAboveThresholdSince = 0; // reset timer đóng
        Serial.print("[SERVO] write(");
        Serial.print(GATE_OPEN_ANGLE);
        Serial.println(") -> OPEN");
        gateServo.write(GATE_OPEN_ANGLE); // 0°
        publishMessage("smarthome/outdoor/gate/status", "OPEN");
        Serial.println("[AUTO] Gate OPEN (distance < 25cm) -> servo 0 deg");

      }

      // --- ĐÓNG CỔNG: khoảng cách >= 25cm, chờ 3 giây ---
      else if (gateIsOpen) {

        if (distance >= 25.0 || distance == 0) {
          // Bắt đầu / tiếp tục đếm timer
          if (distanceAboveThresholdSince == 0) {
            distanceAboveThresholdSince = now;
            Serial.println("[AUTO] distance >= 25cm: bat dau dem 3s de dong cong...");
          }

          // Đủ 3 giây → đóng cổng
          if (now - distanceAboveThresholdSince >= GATE_CLOSE_DELAY) {
            gateIsOpen = false;
            distanceAboveThresholdSince = 0;
            Serial.print("[SERVO] write(");
            Serial.print(GATE_CLOSE_ANGLE);
            Serial.println(") -> CLOSE");
            gateServo.write(GATE_CLOSE_ANGLE); // 90°
            publishMessage("smarthome/outdoor/gate/status", "CLOSE");
            Serial.println("[AUTO] Gate CLOSED (distance >= 25cm kept 3s) -> servo 90 deg");
          }

        } else {
          // Khoảng cách về lại < 25cm → huỷ timer đóng
          if (distanceAboveThresholdSince != 0) {
            Serial.println("[AUTO] Close cancelled: distance < 25cm tro lai");
            distanceAboveThresholdSince = 0;
          }
        }
      }
    }
  }
  
  // =====================================
  // B. PIR TIMEOUT - Tắt đèn sau 10 giây
  // =====================================

  if (pirAutoOn && livingroomState &&
      (now - pirTriggerTime >= PIR_TIMEOUT)) {

    updateDevice(
      livingroomLight1,
      livingroomState,
      false,
      "smarthome/livingroom/light1/status"
    );

    pirAutoOn = false;

    Serial.println("[AUTO] PIR timeout -> Livingroom light OFF");
  }
}