/*
  =============================================================================
  M.A.R. (Multi-legged Autonomous Robot) — Stereovision Low-Level Motor Controller
  =============================================================================
  Firmware per Arduino Uno / Mega / Nano.
  Gestisce la ricezione dei comandi seriali dal Raspberry Pi 5 (Stereovisione),
  il controllo dei motori DC (driver L298N), e la lettura continua del sensore
  a ultrasuoni HC-SR04 con fail-safe di emergenza e heartbeat watchdog.

  Supporta sia il Protocollo M.A.R. strutturato sia i comandi legacy a singolo carattere.

  Autore: Team M.A.R. Robotics
  Data: 2026
  =============================================================================
*/

// ============================================================================
// CONFIGURAZIONE PIN HARDWARE
// ============================================================================

// Motore 1 (Sinistro - Driver L298N)
const int MOTOR_LEFT_IN1 = 8;
const int MOTOR_LEFT_IN2 = 7;
const int MOTOR_LEFT_ENA = 11; // Segnale PWM velocità

// Motore 2 (Destro - Driver L298N)
const int MOTOR_RIGHT_IN3 = 9;
const int MOTOR_RIGHT_IN4 = 10;
const int MOTOR_RIGHT_ENB = 12; // Segnale PWM velocità

// Sensore Ultrasuoni HC-SR04 (Supporta sia modalità single-pin che 2-pin)
const int ULTRASONIC_TRIG_PIN = 52; // Pin Trigger (o pin condiviso)
const int ULTRASONIC_ECHO_PIN = 52; // Impostare uguale al TRIG per single-pin, o separato

// Configurazione Sicurezza e Watchdog
const unsigned long WATCHDOG_TIMEOUT_MS = 1000;   // Timeout arresto se cade la comunicazione
const unsigned long ULTRASONIC_INTERVAL_MS = 60;  // Intervallo di campionamento sensore
const float EMERGENCY_DISTANCE_CM = 15.0;         // Distanza minima di arresto immediato

// ============================================================================
// STATO GLOBALE
// ============================================================================
int currentSpeed = 160;               // Velocità PWM base (0 - 255)
unsigned long lastCommandTime = 0;    // Timestamp per watchdog
unsigned long lastUltrasonicTime = 0; // Timestamp campionamento ultrasuoni
float lastDistanceCm = -1.0;
String inputBuffer = "";
bool isEmergencyStop = false;

// ============================================================================
// DICHIARAZIONE FUNZIONI
// ============================================================================
void stopMotors();
void moveForward(int speed);
void moveBackward(int speed);
void turnLeft(int speed);
void turnRight(int speed);
float readUltrasonicCm();
void parseSerialCommand(String cmd);

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(9600); // 9600 o 115200 a seconda della configurazione host

  // Configurazione pin motori
  pinMode(MOTOR_LEFT_IN1, OUTPUT);
  pinMode(MOTOR_LEFT_IN2, OUTPUT);
  pinMode(MOTOR_LEFT_ENA, OUTPUT);

  pinMode(MOTOR_RIGHT_IN3, OUTPUT);
  pinMode(MOTOR_RIGHT_IN4, OUTPUT);
  pinMode(MOTOR_RIGHT_ENB, OUTPUT);

  // Inizializza pin ultrasuoni
  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);

  // Arresto di sicurezza iniziale
  stopMotors();
  lastCommandTime = millis();

  // Messaggio di sincronizzazione
  Serial.println("ARDUINO_READY");
  Serial.println("MAR_STEREO_CONTROLLER:ONLINE");
}

// ============================================================================
// LOOP PRINCIPALE
// ============================================================================
void loop() {
  unsigned long now = millis();

  // 1. Lettura buffer seriale da Raspberry Pi 5
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        parseSerialCommand(inputBuffer);
        inputBuffer = "";
      }
    } else {
      inputBuffer += c;
      if (inputBuffer.length() > 64) {
        inputBuffer = ""; // Reset in caso di overflow
      }
    }
  }

  // 2. Campionamento periodico sensore ad ultrasuoni
  if (now - lastUltrasonicTime >= ULTRASONIC_INTERVAL_MS) {
    lastUltrasonicTime = now;
    lastDistanceCm = readUltrasonicCm();

    // Invia telemetria
    if (lastDistanceCm > 0) {
      Serial.print("ULTRASONIC:");
      Serial.print(lastDistanceCm, 1);
      Serial.println("cm");
    }

    // 3. FAIL-SAFE OSTACOLO IMMINENTE
    if (lastDistanceCm > 0 && lastDistanceCm <= EMERGENCY_DISTANCE_CM) {
      if (!isEmergencyStop) {
        stopMotors();
        isEmergencyStop = true;
        Serial.print("EMERGENCY_STOP:");
        Serial.print(lastDistanceCm, 1);
        Serial.println("cm");
      }
    } else {
      isEmergencyStop = false;
    }
  }

  // 4. HEARTBEAT SAFETY WATCHDOG
  if (now - lastCommandTime > WATCHDOG_TIMEOUT_MS) {
    // Se non si ricevono comandi per più di 1s, ferma i motori per sicurezza
    stopMotors();
  }

  delay(5);
}

// ============================================================================
// PARSER PROTOCOLLO SERIALE
// ============================================================================
void parseSerialCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  lastCommandTime = millis();

  // --- Supporto comandi a singolo carattere legacy ---
  if (cmd.length() == 1) {
    char c = cmd.charAt(0);
    switch (c) {
      case 'F': moveForward(currentSpeed); break;
      case 'B': moveBackward(currentSpeed); break;
      case 'L': turnLeft(currentSpeed); break;
      case 'R': turnRight(currentSpeed); break;
      case 'S': stopMotors(); break;
      case '?':
        Serial.print("TLM:STATUS=OK,SPEED=");
        Serial.print(currentSpeed);
        Serial.print(",US=");
        Serial.println(lastDistanceCm);
        break;
    }
    return;
  }

  // --- Supporto Comandi Diretti Robot Quadrupede (AVANTI, STOP, STOP_TERRA, GIRA_DX, GIRA_SX) ---
  if (cmd.equalsIgnoreCase("AVANTI")) {
    moveForward(currentSpeed);
    return;
  }
  else if (cmd.equalsIgnoreCase("STOP")) {
    stopMotors();
    return;
  }
  else if (cmd.equalsIgnoreCase("STOP_TERRA")) {
    stopMotors();
    Serial.println("TLM:ALERT=GROUND_OBSTACLE");
    return;
  }
  else if (cmd.equalsIgnoreCase("GIRA_DX")) {
    turnRight(currentSpeed);
    return;
  }
  else if (cmd.equalsIgnoreCase("GIRA_SX")) {
    turnLeft(currentSpeed);
    return;
  }

  // --- Supporto Modifica Periodo di Trotto Float (es. 1.20, 2.50) ---
  char firstC = cmd.charAt(0);
  if ((firstC >= '0' && firstC <= '9') || firstC == '.') {
    float periodSec = cmd.toFloat();
    if (periodSec > 0.1 && periodSec < 10.0) {
      // Periodo breve = trotto veloce (PWM alto), periodo lungo = trotto lento (PWM basso)
      currentSpeed = constrain(map(long(periodSec * 100), 100, 300, 240, 100), 80, 255);
      Serial.print("TLM:GAIT_PERIOD=");
      Serial.print(periodSec, 2);
      Serial.print("s,PWM=");
      Serial.println(currentSpeed);
      return;
    }
  }

  // --- Supporto Protocollo Strutturato M.A.R. (CMD:...) ---
  if (cmd.startsWith("CMD:")) {
    String payload = cmd.substring(4);
    payload.trim();

    if (payload.startsWith("STOP_TERRA")) {
      stopMotors();
      Serial.println("TLM:ALERT=GROUND_OBSTACLE");
    }
    else if (payload.startsWith("STOP")) {
      stopMotors();
    }
    else if (payload.startsWith("FORWARD")) {
      int speed = currentSpeed;
      int comma = payload.indexOf(',');
      if (comma != -1) {
        int pct = payload.substring(comma + 1).toInt();
        speed = map(constrain(pct, 0, 100), 0, 100, 0, 255);
      }
      moveForward(speed);
    }
    else if (payload.startsWith("BACKWARD")) {
      int speed = currentSpeed;
      int comma = payload.indexOf(',');
      if (comma != -1) {
        int pct = payload.substring(comma + 1).toInt();
        speed = map(constrain(pct, 0, 100), 0, 100, 0, 255);
      }
      moveBackward(speed);
    }
    else if (payload.startsWith("TURN_L")) {
      int speed = currentSpeed;
      int comma = payload.indexOf(',');
      if (comma != -1) {
        int pct = payload.substring(comma + 1).toInt();
        speed = map(constrain(pct, 0, 100), 0, 100, 0, 255);
      }
      turnLeft(speed);
    }
    else if (payload.startsWith("TURN_R")) {
      int speed = currentSpeed;
      int comma = payload.indexOf(',');
      if (comma != -1) {
        int pct = payload.substring(comma + 1).toInt();
        speed = map(constrain(pct, 0, 100), 0, 100, 0, 255);
      }
      turnRight(speed);
    }
    else if (payload.startsWith("STATUS") || payload.startsWith("PING")) {
      Serial.print("TLM:STATUS=OK,US=");
      Serial.println(lastDistanceCm);
    }
  }
}

// ============================================================================
// FUNZIONI DI MOVIMENTO (DRIVER L298N)
// ============================================================================

void stopMotors() {
  digitalWrite(MOTOR_LEFT_IN1, LOW);
  digitalWrite(MOTOR_LEFT_IN2, LOW);
  analogWrite(MOTOR_LEFT_ENA, 0);

  digitalWrite(MOTOR_RIGHT_IN3, LOW);
  digitalWrite(MOTOR_RIGHT_IN4, LOW);
  analogWrite(MOTOR_RIGHT_ENB, 0);
}

void moveForward(int speed) {
  if (isEmergencyStop) {
    stopMotors();
    return;
  }
  // Motore Sinistro Avanti
  digitalWrite(MOTOR_LEFT_IN1, HIGH);
  digitalWrite(MOTOR_LEFT_IN2, LOW);
  analogWrite(MOTOR_LEFT_ENA, speed);

  // Motore Destro Avanti
  digitalWrite(MOTOR_RIGHT_IN3, HIGH);
  digitalWrite(MOTOR_RIGHT_IN4, LOW);
  analogWrite(MOTOR_RIGHT_ENB, speed);
}

void moveBackward(int speed) {
  // Motore Sinistro Indietro
  digitalWrite(MOTOR_LEFT_IN1, LOW);
  digitalWrite(MOTOR_LEFT_IN2, HIGH);
  analogWrite(MOTOR_LEFT_ENA, speed);

  // Motore Destro Indietro
  digitalWrite(MOTOR_RIGHT_IN3, LOW);
  digitalWrite(MOTOR_RIGHT_IN4, HIGH);
  analogWrite(MOTOR_RIGHT_ENB, speed);
}

void turnLeft(int speed) {
  // Ruota sul posto verso sinistra (Sinistro indietro, Destro avanti)
  digitalWrite(MOTOR_LEFT_IN1, LOW);
  digitalWrite(MOTOR_LEFT_IN2, HIGH);
  analogWrite(MOTOR_LEFT_ENA, speed);

  digitalWrite(MOTOR_RIGHT_IN3, HIGH);
  digitalWrite(MOTOR_RIGHT_IN4, LOW);
  analogWrite(MOTOR_RIGHT_ENB, speed);
}

void turnRight(int speed) {
  // Ruota sul posto verso destra (Sinistro avanti, Destro indietro)
  digitalWrite(MOTOR_LEFT_IN1, HIGH);
  digitalWrite(MOTOR_LEFT_IN2, LOW);
  analogWrite(MOTOR_LEFT_ENA, speed);

  digitalWrite(MOTOR_RIGHT_IN3, LOW);
  digitalWrite(MOTOR_RIGHT_IN4, HIGH);
  analogWrite(MOTOR_RIGHT_ENB, speed);
}

// ============================================================================
// LETTURA SENSORE ULTRASUONI HC-SR04
// ============================================================================
float readUltrasonicCm() {
  if (ULTRASONIC_TRIG_PIN == ULTRASONIC_ECHO_PIN) {
    // Modalità single-pin
    pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);

    pinMode(ULTRASONIC_TRIG_PIN, INPUT);
    long duration = pulseIn(ULTRASONIC_TRIG_PIN, HIGH, 25000); // 25ms timeout (~4 metri)
    if (duration == 0) return -1.0;
    return (float)duration * 0.0343 / 2.0;
  } else {
    // Modalità standard 2 pin
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);

    long duration = pulseIn(ULTRASONIC_ECHO_PIN, HIGH, 25000);
    if (duration == 0) return -1.0;
    return (float)duration * 0.0343 / 2.0;
  }
}
