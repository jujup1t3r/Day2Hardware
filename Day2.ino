const int   CHARGE_PIN = 21;
const int   ADC_PIN    = 1;
const int   BUTTON_PIN = 4;

const float VCC = 3.3;
const float R1  = 1000000.0;
const unsigned long T_CH_US = 25;
const int   N_AVG = 512;

const unsigned long COOLDOWN_MS = 1500;
unsigned long lastMeasureTime = 0;

// ===== ตารางคาลิเบรต =====
const int CAL_N = 3;
float CAL_RAW[CAL_N]   = { 21.59, 27.21, 28.91 };
float CAL_MOIST[CAL_N] = {  6.60, 11.50, 14.40 };

bool prevPressed = false;

void setup() {
  Serial.begin(9600);
  pinMode(BUTTON_PIN, INPUT);
  pinMode(CHARGE_PIN, OUTPUT);
  digitalWrite(CHARGE_PIN, LOW);
  analogReadResolution(12);
  analogSetPinAttenuation(ADC_PIN, ADC_11db);
  Serial.println("พร้อมวัด — กดสวิตช์ 1 ครั้งเพื่อวัด");
}

float measureRawCx(float &vcOut) {
  double sumVc = 0;
  for (int i = 0; i < N_AVG; i++) {
    pinMode(CHARGE_PIN, OUTPUT);
    digitalWrite(CHARGE_PIN, LOW);
    delayMicroseconds(300);
    noInterrupts();
    digitalWrite(CHARGE_PIN, HIGH);
    delayMicroseconds(T_CH_US);
    pinMode(CHARGE_PIN, INPUT);
    interrupts();
    sumVc += analogReadMilliVolts(ADC_PIN) / 1000.0;
  }
  float vc = sumVc / N_AVG;
  float ratio = 1.0 - (vc / VCC);
  if (ratio < 1e-4) ratio = 1e-4;
  float cx_F = -((double)T_CH_US * 1e-6) / (R1 * log(ratio));
  vcOut = vc;
  return cx_F * 1e12;
}

// วัด 5 รอบเอา median กัน outlier (ภายในการกด 1 ครั้ง)
float measureRobust(float &vcOut) {
  const int M = 5;
  float r[M], v;
  for (int k = 0; k < M; k++) r[k] = measureRawCx(v);
  for (int a = 0; a < M - 1; a++)
    for (int b = a + 1; b < M; b++)
      if (r[b] < r[a]) { float t = r[a]; r[a] = r[b]; r[b] = t; }
  vcOut = v;
  return r[M / 2];
}

float applyCal(float raw) {
  // เส้นยึด oven-dry truth (fit จาก CAL_RAW เมล็ดสด 21.59/27.21/28.91)
  return 0.113921f * raw * raw - 4.6872f * raw + 54.691f;
}

void loop() {
  bool pressed = (digitalRead(BUTTON_PIN) == LOW);
  unsigned long now = millis();

  if (pressed && !prevPressed && (now - lastMeasureTime >= COOLDOWN_MS)) {
    delay(30);
    if (digitalRead(BUTTON_PIN) == LOW) {
      float vc;
      float raw = measureRobust(vc);          // กดครั้งเดียว = 1 ค่า
      float moisture = applyCal(raw);

      Serial.printf("\"TGR-41\" | Vc = %.3f V | raw = %.2f pF | Moisture = %.2f %%\r\n", vc, raw, moisture);

      pinMode(CHARGE_PIN, OUTPUT);
      digitalWrite(CHARGE_PIN, LOW);
      lastMeasureTime = millis();
    }
  }
  prevPressed = pressed;
}