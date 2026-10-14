# 🦾 Microservizio: `mar-firmware`

**Sottosistema di Controllo Basso Livello & Attuazione Fisica (Microcontrollore)**  
*Parte dell'architettura a microservizi M.A.R. (Multi-legged Autonomous Robot)*

---

## 🎯 Ruolo nell'Architettura a Microservizi

Nell'architettura distribuita di **M.A.R.**, il componente `mar-firmware` è l'unico microservizio che gira su hardware microcontrollore dedicato (Arduino Mega 2560, Arduino GIGA o RP2040) ed è separato computazionalmente dal Raspberry Pi 5.

I suoi compiti principali sono:
1. **Controllo Fisico e Attuazione**:
   - Gestione dei driver motori H-bridge (PWM/DIR) per flessione/estensione dell'anca e cinghie dentate dei ginocchi.
   - Controllo dei servomotori dedicati all'adduzione/abduzione laterale (apertura carreggiata).
   - Cinematica Inversa (IK) e generazione delle andature (*Trot, Crawl, Stand, Sit*).
2. **Campionamento Sensori Hardware**:
   - Lettura continua sensori di prossimità/ultrasuoni per sicurezza immediata.
   - Monitoraggio tensione batteria (BMS) e correnti assorbite.
   - Stima dell'orientamento tramite IMU (Roll/Pitch).
3. **Safety Watchdog & Fail-Safe Hardware**:
   - Timer watchdog hardware a **500–1000 ms**: se la connessione con `mar-serial-bridge` su Raspberry Pi 5 si interrompe o crasha, il firmware entra istantaneamente in `EMERGENCY_STANDSTILL`, disabilitando tutti i motori per evitare collisioni incontrollate.

---

## 🔌 Interfaccia di Comunicazione Seriale

Il firmware comunica esclusivamente con il microservizio `mar-serial-bridge` tramite connessione seriale USB (115200 baud, 8N1).

### Comandi in Ingresso (Downlink da `mar-serial-bridge`)
* `CMD:STOP\n`: Arresto immediato motori e stabilizzazione.
* `CMD:FORWARD,<velocità_0_100>\n`: Avanzamento con andatura impostata.
* `CMD:BACKWARD,<velocità_0_100>\n`: Retromarcia.
* `CMD:TURN_L,<gradi_o_vel>\n`: Rotazione sul posto a sinistra.
* `CMD:TURN_R,<gradi_o_vel>\n`: Rotazione sul posto a destra.
* `CMD:STAND,<altezza_mm>\n`: Assetto eretto all'altezza desiderata.
* `CMD:SIT\n`: Modalità di riposo a terra.
* `CMD:HIP_WIDTH,<angolo_deg>\n`: Regolazione apertura servomotori dell'anca.
* `CMD:PING\n`: Heartbeat periodico per azzerare il watchdog.

### Telemetria in Uscita (Uplink verso `mar-serial-bridge`)
* `TLM:STATUS=<OK|WARN|FAULT>,BATT=<volts>,CURR=<amps>,PITCH=<deg>,ROLL=<deg>\n`

---

## 📂 Struttura della Cartella

```text
mar-firmware/
├── README.md                                          # Questo documento
├── platformio.ini                                     # Configurazione per PlatformIO (Mega2560, Uno, ecc.)
├── ARDUINO_SETUP.md                                   # Guida dettagliata al cablaggio e pinout hardware
└── mar_stereovision_controller/
    └── mar_stereovision_controller.ino                # Sketch C++ principale con parser comandi e watchdog
```

---

## 🚀 Compilazione e Caricamento

### Opzione 1: PlatformIO (Consigliata)
```bash
# Compilazione e upload su Arduino Mega 2560
pio run --target upload -e mega2560

# Monitor seriale
pio device monitor -b 115200
```

### Opzione 2: Arduino IDE
1. Apri il file `mar_stereovision_controller/mar_stereovision_controller.ino`.
2. Seleziona la scheda (**Arduino Mega 2560** o compatibile).
3. Seleziona la porta seriale USB corretta.
4. Clicca su **Carica** (Ctrl+U / Cmd+U).
