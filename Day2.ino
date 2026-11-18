const int   CHARGE_PIN = 21;
const int   ADC_PIN    = 1;
const int   BUTTON_PIN = 4;

const float VCC = 3.3;
const float R1  = 1000000.0;
const unsigned long T_CH_US = 25;
const int   N_AVG = 256;

// ===== ตารางคาลิเบรตจริง (เรียงจากน้อย -> มาก) =====
const int CAL_N = 2; 
float CAL_RAW[CAL_N]   = { 21.59, 28.91 };  // ค่าเฉลี่ย raw ที่วัดได้จริง
float CAL_MOIST[CAL_N] = {  6.60, 14.40 };  // % ความชื้นจริงจากซองตัวอย่าง
// ==================================================

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

// แปลง raw -> % ความชื้น ด้วย Linear Interpolation & Extrapolation
float applyCal(float raw) {
  if (raw <= CAL_RAW[0]) {
    return CAL_MOIST[0] + (raw - CAL_RAW[0]) * (CAL_MOIST[1] - CAL_MOIST[0]) / (CAL_RAW[1] - CAL_RAW[0]);
  }
  
  for (int i = 0; i < CAL_N - 1; i++) {
    if (raw <= CAL_RAW[i + 1]) {
      float t = (raw - CAL_RAW[i]) / (CAL_RAW[i + 1] - CAL_RAW[i]);
      return CAL_MOIST[i] + t * (CAL_MOIST[i + 1] - CAL_MOIST[i]);
    }
  }
  
  int i = CAL_N - 2;
  return CAL_MOIST[i] + (raw - CAL_RAW[i]) * (CAL_MOIST[i + 1] - CAL_MOIST[i]) / (CAL_RAW[i + 1] - CAL_RAW[i]);
}

void loop() {
  bool pressed = (digitalRead(BUTTON_PIN) == LOW);

  // ขอบขาลง = เพิ่งกด -> วัด 1 ครั้ง
  if (pressed && !prevPressed) {
    delay(20); // debounce กันสวิตช์เด้ง
    
    float vc;
    float raw = measureRawCx(vc);
    float moisture = applyCal(raw);
    
    Serial.printf("\"TGR-41\" | Vc = %.3f V | raw = %.2f pF | Moisture = %.2f %%\r\n", vc, raw, moisture);
    
    pinMode(CHARGE_PIN, OUTPUT);
    digitalWrite(CHARGE_PIN, LOW); // พักคายประจุ
  }
  
  prevPressed = pressed;
}