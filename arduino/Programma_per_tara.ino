#include <HX711.h>
#include <EEPROM.h>

// ============ CONFIGURAZIONE PIN ============
// Cassetto A: HX711 con 2 celle di carico collegate
#define LOADCELL_DOUT_A   33
#define LOADCELL_SCK_A    25

// Cassetto B: HX711 con 2 celle di carico collegate  
#define LOADCELL_DOUT_B   26
#define LOADCELL_SCK_B    27

// ============ CONFIGURAZIONE EEPROM ============
#define EEPROM_SIZE       512
#define ADDR_CALIBRATION_A  0   // Indirizzo per fattore calibrazione A
#define ADDR_CALIBRATION_B  4   // Indirizzo per fattore calibrazione B
#define ADDR_OFFSET_A      8   // Indirizzo per offset A
#define ADDR_OFFSET_B      12  // Indirizzo per offset B

// ============ VARIABILI GLOBALI ============
HX711 scalaA;  // Bilancia cassetto A (2 celle collegate)
HX711 scalaB;  // Bilancia cassetto B (2 celle collegate)

float fattoreCalibrazioneA = 1.0;
float fattoreCalibrazioneB = 1.0;
long offsetA = 0;
long offsetB = 0;

bool sistemaPronto = false;
bool calibrazioneCompletaA = false;
bool calibrazioneCompletaB = false;

// ============ SETUP ============
void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n╔══════════════════════════════════════════════════════════╗");
  Serial.println("║           🔧 CALIBRAZIONE CELLE DI CARICO ESP32          ║");
  Serial.println("║                    DISTRIBUTORE AUTOMATICO               ║");
  Serial.println("╠══════════════════════════════════════════════════════════╣");
  Serial.println("║  📦 Cassetto A: DT=33, SCK=25 (2 celle → 1 HX711)       ║");
  Serial.println("║  📦 Cassetto B: DT=26, SCK=27 (2 celle → 1 HX711)       ║");
  Serial.println("╚══════════════════════════════════════════════════════════╝\n");
  
  // Inizializza EEPROM
  EEPROM.begin(EEPROM_SIZE);
  Serial.println("💾 EEPROM inizializzata (" + String(EEPROM_SIZE) + " bytes)");
  
  // Inizializza le bilance
  Serial.println("🔄 Inizializzazione HX711...");
  
  scalaA.begin(LOADCELL_DOUT_A, LOADCELL_SCK_A);
  scalaB.begin(LOADCELL_DOUT_B, LOADCELL_SCK_B);
  
  delay(1000);
  
  // Verifica connessione
  Serial.print("📡 Test connessione Cassetto A... ");
  if (scalaA.is_ready()) {
    Serial.println("✅ OK");
  } else {
    Serial.println("❌ ERRORE - Verifica collegamenti!");
    return;
  }
  
  Serial.print("📡 Test connessione Cassetto B... ");
  if (scalaB.is_ready()) {
    Serial.println("✅ OK");
  } else {
    Serial.println("❌ ERRORE - Verifica collegamenti!");
    return;
  }
  
  // Carica calibrazioni salvate
  caricaCalibrazioni();
  
  sistemaPronto = true;
  
  Serial.println("\n✅ Sistema pronto per la calibrazione!");
  mostraMenu();
}

// ============ LOOP PRINCIPALE ============
void loop() {
  if (!sistemaPronto) {
    Serial.println("❌ Sistema non pronto. Riavvia ESP32.");
    delay(5000);
    return;
  }
  
  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();
    
    eseguiComando(comando);
  }
  
  delay(100);
}

// ============ GESTIONE COMANDI ============
void eseguiComando(String cmd) {
  cmd.toLowerCase();
  
  if (cmd == "menu") {
    mostraMenu();
  }
  else if (cmd == "status") {
    mostraStatus();
  }
  else if (cmd == "raw") {
    mostraValoriRaw();
  }
  else if (cmd == "peso") {
    mostraPesi();
  }
  else if (cmd == "tara_a") {
    taraCassettoA();
  }
  else if (cmd == "tara_b") {
    taraCassettoB();
  }
  else if (cmd == "tara_all") {
    taraTutto();
  }
  else if (cmd == "cal_a") {
    calibraCassettoA();
  }
  else if (cmd == "cal_b") {
    calibraCassettoB();
  }
  else if (cmd == "test_a") {
    testContinuoA();
  }
  else if (cmd == "test_b") {
    testContinuoB();
  }
  else if (cmd == "test_all") {
    testContinuoEntrambi();
  }
  else if (cmd == "save") {
    salvaCalibrazioni();
  }
  else if (cmd == "load") {
    caricaCalibrazioni();
  }
  else if (cmd == "reset") {
    resetCalibrazioni();
  }
  else if (cmd == "info") {
    mostraInfo();
  }
  else {
    Serial.println("❓ Comando non riconosciuto. Digita 'menu' per vedere i comandi disponibili.");
  }
}

// ============ MENU E STATUS ============
void mostraMenu() {
  Serial.println("\n╔══════════════════════════════════════════════════════════╗");
  Serial.println("║                    📋 MENU COMANDI                       ║");
  Serial.println("╠══════════════════════════════════════════════════════════╣");
  Serial.println("║  🔧 CALIBRAZIONE                                         ║");
  Serial.println("║    tara_a      - Tara Cassetto A (azzera)                ║");
  Serial.println("║    tara_b      - Tara Cassetto B (azzera)                ║");
  Serial.println("║    tara_all    - Tara entrambi i cassetti                ║");
  Serial.println("║    cal_a       - Calibra Cassetto A (peso noto)          ║");
  Serial.println("║    cal_b       - Calibra Cassetto B (peso noto)          ║");
  Serial.println("║                                                          ║");
  Serial.println("║  📊 MONITORAGGIO                                         ║");
  Serial.println("║    status      - Stato calibrazioni                      ║");
  Serial.println("║    raw         - Valori raw ADC                          ║");
  Serial.println("║    peso        - Pesi calibrati                          ║");
  Serial.println("║    test_a      - Test continuo Cassetto A (ESC per stop) ║");
  Serial.println("║    test_b      - Test continuo Cassetto B (ESC per stop) ║");
  Serial.println("║    test_all    - Test continuo entrambi (ESC per stop)   ║");
  Serial.println("║                                                          ║");
  Serial.println("║  💾 GESTIONE DATI                                        ║");
  Serial.println("║    save        - Salva calibrazioni in EEPROM            ║");
  Serial.println("║    load        - Carica calibrazioni da EEPROM           ║");
  Serial.println("║    reset       - Reset tutte le calibrazioni             ║");
  Serial.println("║    info        - Informazioni tecniche                   ║");
  Serial.println("╚══════════════════════════════════════════════════════════╝");
}

void mostraStatus() {
  Serial.println("\n📊 === STATUS CALIBRAZIONI ===");
  
  Serial.println("📦 CASSETTO A:");
  Serial.println("   🔗 Pin: DT=" + String(LOADCELL_DOUT_A) + ", SCK=" + String(LOADCELL_SCK_A));
  Serial.println("   📡 Connesso: " + String(scalaA.is_ready() ? "✅ SÌ" : "❌ NO"));
  Serial.println("   ⚖️ Fattore calibrazione: " + String(fattoreCalibrazioneA, 2));
  Serial.println("   📏 Offset: " + String(offsetA));
  Serial.println("   ✅ Calibrato: " + String(calibrazioneCompletaA ? "SÌ" : "NO"));
  
  Serial.println("\n📦 CASSETTO B:");
  Serial.println("   🔗 Pin: DT=" + String(LOADCELL_DOUT_B) + ", SCK=" + String(LOADCELL_SCK_B));
  Serial.println("   📡 Connesso: " + String(scalaB.is_ready() ? "✅ SÌ" : "❌ NO"));
  Serial.println("   ⚖️ Fattore calibrazione: " + String(fattoreCalibrazioneB, 2));
  Serial.println("   📏 Offset: " + String(offsetB));
  Serial.println("   ✅ Calibrato: " + String(calibrazioneCompletaB ? "SÌ" : "NO"));
  
  Serial.println("\n🔧 STATO GENERALE:");
  Serial.println("   Sistema: " + String(sistemaPronto ? "✅ PRONTO" : "❌ NON PRONTO"));
  Serial.println("   Memoria libera: " + String(ESP.getFreeHeap()) + " bytes");
}

void mostraValoriRaw() {
  Serial.println("\n🔢 === VALORI RAW ADC ===");
  
  if (scalaA.is_ready()) {
    long rawA = scalaA.read();
    Serial.println("📦 Cassetto A: " + String(rawA));
  } else {
    Serial.println("📦 Cassetto A: ❌ NON DISPONIBILE");
  }
  
  if (scalaB.is_ready()) {
    long rawB = scalaB.read();
    Serial.println("📦 Cassetto B: " + String(rawB));
  } else {
    Serial.println("📦 Cassetto B: ❌ NON DISPONIBILE");
  }
}

void mostraPesi() {
  Serial.println("\n⚖️ === PESI CALIBRATI ===");
  
  if (scalaA.is_ready()) {
    float pesoA = scalaA.get_units(5);  // Media di 5 letture
    Serial.println("📦 Cassetto A: " + String(pesoA, 2) + " g");
  } else {
    Serial.println("📦 Cassetto A: ❌ NON DISPONIBILE");
  }
  
  if (scalaB.is_ready()) {
    float pesoB = scalaB.get_units(5);  // Media di 5 letture
    Serial.println("📦 Cassetto B: " + String(pesoB, 2) + " g");
  } else {
    Serial.println("📦 Cassetto B: ❌ NON DISPONIBILE");
  }
}

// ============ OPERAZIONI DI TARA ============
void taraCassettoA() {
  Serial.println("\n🔄 === TARA CASSETTO A ===");
  Serial.println("⚠️ Assicurati che il cassetto A sia VUOTO!");
  Serial.println("Premi INVIO per confermare o 'n' per annullare...");
  
  while (!Serial.available()) {
    delay(100);
  }
  
  String conferma = Serial.readStringUntil('\n');
  conferma.trim();
  conferma.toLowerCase();
  
  if (conferma == "n") {
    Serial.println("❌ Tara annullata");
    return;
  }
  
  if (!scalaA.is_ready()) {
    Serial.println("❌ Cassetto A non disponibile!");
    return;
  }
  
  Serial.println("🔄 Esecuzione tara...");
  scalaA.tare(10);  // Media di 10 letture per la tara
  offsetA = scalaA.get_offset();
  
  Serial.println("✅ Tara Cassetto A completata!");
  Serial.println("📏 Nuovo offset: " + String(offsetA));
}

void taraCassettoB() {
  Serial.println("\n🔄 === TARA CASSETTO B ===");
  Serial.println("⚠️ Assicurati che il cassetto B sia VUOTO!");
  Serial.println("Premi INVIO per confermare o 'n' per annullare...");
  
  while (!Serial.available()) {
    delay(100);
  }
  
  String conferma = Serial.readStringUntil('\n');
  conferma.trim();
  conferma.toLowerCase();
  
  if (conferma == "n") {
    Serial.println("❌ Tara annullata");
    return;
  }
  
  if (!scalaB.is_ready()) {
    Serial.println("❌ Cassetto B non disponibile!");
    return;
  }
  
  Serial.println("🔄 Esecuzione tara...");
  scalaB.tare(10);  // Media di 10 letture per la tara
  offsetB = scalaB.get_offset();
  
  Serial.println("✅ Tara Cassetto B completata!");
  Serial.println("📏 Nuovo offset: " + String(offsetB));
}

void taraTutto() {
  Serial.println("\n🔄 === TARA ENTRAMBI I CASSETTI ===");
  Serial.println("⚠️ Assicurati che ENTRAMBI i cassetti siano VUOTI!");
  Serial.println("Premi INVIO per confermare o 'n' per annullare...");
  
  while (!Serial.available()) {
    delay(100);
  }
  
  String conferma = Serial.readStringUntil('\n');
  conferma.trim();
  conferma.toLowerCase();
  
  if (conferma == "n") {
    Serial.println("❌ Tara annullata");
    return;
  }
  
  taraCassettoA();
  delay(1000);
  taraCassettoB();
  
  Serial.println("\n✅ Tara di entrambi i cassetti completata!");
}

// ============ CALIBRAZIONE ============
void calibraCassettoA() {
  Serial.println("\n🎯 === CALIBRAZIONE CASSETTO A ===");
  
  if (!scalaA.is_ready()) {
    Serial.println("❌ Cassetto A non disponibile!");
    return;
  }
  
  // Prima esegui la tara
  Serial.println("1️⃣ Prima devo fare la tara (cassetto vuoto)");
  Serial.println("   Svuota il cassetto A e premi INVIO...");
  
  while (!Serial.available()) {
    delay(100);
  }
  Serial.readStringUntil('\n');  // Consuma input
  
  scalaA.tare(10);
  Serial.println("✅ Tara completata");
  
  // Richiedi peso noto
  Serial.println("\n2️⃣ Ora inserisci un peso NOTO nel cassetto A");
  Serial.print("   Inserisci il peso in grammi (es: 100.5): ");
  
  while (!Serial.available()) {
    delay(100);
  }
  
  float pesoNoto = Serial.parseFloat();
  Serial.readStringUntil('\n');  // Consuma resto della linea
  
  if (pesoNoto <= 0) {
    Serial.println("❌ Peso non valido! Calibrazione annullata.");
    return;
  }
  
  Serial.println("   Peso inserito: " + String(pesoNoto, 2) + " g");
  Serial.println("   Attendi stabilizzazione... (5 secondi)");
  
  delay(5000);
  
  // Leggi valore e calcola fattore
  long valoreRaw = scalaA.get_value(10);  // Media di 10 letture
  fattoreCalibrazioneA = (float)valoreRaw / pesoNoto;
  
  scalaA.set_scale(fattoreCalibrazioneA);
  
  Serial.println("\n📊 RISULTATI CALIBRAZIONE CASSETTO A:");
  Serial.println("   Valore raw: " + String(valoreRaw));
  Serial.println("   Peso noto: " + String(pesoNoto, 2) + " g");
  Serial.println("   Fattore calcolato: " + String(fattoreCalibrazioneA, 2));
  
  // Test calibrazione
  delay(1000);
  float pesoTest = scalaA.get_units(5);
  Serial.println("   Test lettura: " + String(pesoTest, 2) + " g");
  Serial.println("   Errore: " + String(abs(pesoTest - pesoNoto), 2) + " g");
  
  calibrazioneCompletaA = true;
  Serial.println("\n✅ Calibrazione Cassetto A COMPLETATA!");
  Serial.println("💡 Ricordati di salvare con il comando 'save'");
}

void calibraCassettoB() {
  Serial.println("\n🎯 === CALIBRAZIONE CASSETTO B ===");
  
  if (!scalaB.is_ready()) {
    Serial.println("❌ Cassetto B non disponibile!");
    return;
  }
  
  // Prima esegui la tara
  Serial.println("1️⃣ Prima devo fare la tara (cassetto vuoto)");
  Serial.println("   Svuota il cassetto B e premi INVIO...");
  
  while (!Serial.available()) {
    delay(100);
  }
  Serial.readStringUntil('\n');  // Consuma input
  
  scalaB.tare(10);
  Serial.println("✅ Tara completata");
  
  // Richiedi peso noto
  Serial.println("\n2️⃣ Ora inserisci un peso NOTO nel cassetto B");
  Serial.print("   Inserisci il peso in grammi (es: 100.5): ");
  
  while (!Serial.available()) {
    delay(100);
  }
  
  float pesoNoto = Serial.parseFloat();
  Serial.readStringUntil('\n');  // Consuma resto della linea
  
  if (pesoNoto <= 0) {
    Serial.println("❌ Peso non valido! Calibrazione annullata.");
    return;
  }
  
  Serial.println("   Peso inserito: " + String(pesoNoto, 2) + " g");
  Serial.println("   Attendi stabilizzazione... (5 secondi)");
  
  delay(5000);
  
  // Leggi valore e calcola fattore
  long valoreRaw = scalaB.get_value(10);  // Media di 10 letture
  fattoreCalibrazioneB = (float)valoreRaw / pesoNoto;
  
  scalaB.set_scale(fattoreCalibrazioneB);
  
  Serial.println("\n📊 RISULTATI CALIBRAZIONE CASSETTO B:");
  Serial.println("   Valore raw: " + String(valoreRaw));
  Serial.println("   Peso noto: " + String(pesoNoto, 2) + " g");
  Serial.println("   Fattore calcolato: " + String(fattoreCalibrazioneB, 2));
  
  // Test calibrazione
  delay(1000);
  float pesoTest = scalaB.get_units(5);
  Serial.println("   Test lettura: " + String(pesoTest, 2) + " g");
  Serial.println("   Errore: " + String(abs(pesoTest - pesoNoto), 2) + " g");
  
  calibrazioneCompletaB = true;
  Serial.println("\n✅ Calibrazione Cassetto B COMPLETATA!");
  Serial.println("💡 Ricordati di salvare con il comando 'save'");
}

// ============ TEST CONTINUI ============
void testContinuoA() {
  Serial.println("\n📊 === TEST CONTINUO CASSETTO A ===");
  Serial.println("Premi ESC per uscire...\n");
  
  unsigned long ultimaLettura = 0;
  
  while (true) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == 27) {  // ESC
        Serial.println("\n🔚 Test interrotto");
        while (Serial.available()) Serial.read();  // Pulisci buffer
        return;
      }
    }
    
    if (millis() - ultimaLettura > 500) {
      if (scalaA.is_ready()) {
        float peso = scalaA.get_units(3);
        long raw = scalaA.read();
        Serial.println("📦 A: " + String(peso, 2) + " g  (raw: " + String(raw) + ")");
      } else {
        Serial.println("📦 A: ❌ Non disponibile");
      }
      ultimaLettura = millis();
    }
    
    delay(50);
  }
}

void testContinuoB() {
  Serial.println("\n📊 === TEST CONTINUO CASSETTO B ===");
  Serial.println("Premi ESC per uscire...\n");
  
  unsigned long ultimaLettura = 0;
  
  while (true) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == 27) {  // ESC
        Serial.println("\n🔚 Test interrotto");
        while (Serial.available()) Serial.read();  // Pulisci buffer
        return;
      }
    }
    
    if (millis() - ultimaLettura > 500) {
      if (scalaB.is_ready()) {
        float peso = scalaB.get_units(3);
        long raw = scalaB.read();
        Serial.println("📦 B: " + String(peso, 2) + " g  (raw: " + String(raw) + ")");
      } else {
        Serial.println("📦 B: ❌ Non disponibile");
      }
      ultimaLettura = millis();
    }
    
    delay(50);
  }
}

void testContinuoEntrambi() {
  Serial.println("\n📊 === TEST CONTINUO ENTRAMBI I CASSETTI ===");
  Serial.println("Premi ESC per uscire...\n");
  
  unsigned long ultimaLettura = 0;
  
  while (true) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == 27) {  // ESC
        Serial.println("\n🔚 Test interrotto");
        while (Serial.available()) Serial.read();  // Pulisci buffer
        return;
      }
    }
    
    if (millis() - ultimaLettura > 1000) {
      Serial.print("📦 A: ");
      if (scalaA.is_ready()) {
        float pesoA = scalaA.get_units(3);
        Serial.print(String(pesoA, 2) + " g");
      } else {
        Serial.print("❌ N/A");
      }
      
      Serial.print("  |  📦 B: ");
      if (scalaB.is_ready()) {
        float pesoB = scalaB.get_units(3);
        Serial.print(String(pesoB, 2) + " g");
      } else {
        Serial.print("❌ N/A");
      }
      
      Serial.println();
      ultimaLettura = millis();
    }
    
    delay(50);
  }
}

// ============ GESTIONE EEPROM ============
void salvaCalibrazioni() {
  Serial.println("\n💾 === SALVATAGGIO CALIBRAZIONI ===");
  
  EEPROM.put(ADDR_CALIBRATION_A, fattoreCalibrazioneA);
  EEPROM.put(ADDR_CALIBRATION_B, fattoreCalibrazioneB);
  EEPROM.put(ADDR_OFFSET_A, offsetA);
  EEPROM.put(ADDR_OFFSET_B, offsetB);
  
  if (EEPROM.commit()) {
    Serial.println("✅ Calibrazioni salvate in EEPROM");
    Serial.println("   📦 Cassetto A - Fattore: " + String(fattoreCalibrazioneA, 2) + ", Offset: " + String(offsetA));
    Serial.println("   📦 Cassetto B - Fattore: " + String(fattoreCalibrazioneB, 2) + ", Offset: " + String(offsetB));
  } else {
    Serial.println("❌ Errore nel salvataggio!");
  }
}

void caricaCalibrazioni() {
  Serial.println("\n📁 === CARICAMENTO CALIBRAZIONI ===");
  
  float tempFactorA, tempFactorB;
  long tempOffsetA, tempOffsetB;
  
  EEPROM.get(ADDR_CALIBRATION_A, tempFactorA);
  EEPROM.get(ADDR_CALIBRATION_B, tempFactorB);
  EEPROM.get(ADDR_OFFSET_A, tempOffsetA);
  EEPROM.get(ADDR_OFFSET_B, tempOffsetB);
  
  // Verifica se i valori sono ragionevoli
  if (!isnan(tempFactorA) && tempFactorA > 0.1 && tempFactorA < 100000) {
    fattoreCalibrazioneA = tempFactorA;
    offsetA = tempOffsetA;
    scalaA.set_scale(fattoreCalibrazioneA);
    scalaA.set_offset(offsetA);
    calibrazioneCompletaA = true;
    Serial.println("✅ Cassetto A: Fattore=" + String(fattoreCalibrazioneA, 2) + ", Offset=" + String(offsetA));
  } else {
    Serial.println("⚠️ Cassetto A: Nessuna calibrazione valida trovata");
    calibrazioneCompletaA = false;
  }
  
  if (!isnan(tempFactorB) && tempFactorB > 0.1 && tempFactorB < 100000) {
    fattoreCalibrazioneB = tempFactorB;
    offsetB = tempOffsetB;
    scalaB.set_scale(fattoreCalibrazioneB);
    scalaB.set_offset(offsetB);
    calibrazioneCompletaB = true;
    Serial.println("✅ Cassetto B: Fattore=" + String(fattoreCalibrazioneB, 2) + ", Offset=" + String(offsetB));
  } else {
    Serial.println("⚠️ Cassetto B: Nessuna calibrazione valida trovata");
    calibrazioneCompletaB = false;
  }
}

void resetCalibrazioni() {
  Serial.println("\n🔄 === RESET CALIBRAZIONI ===");
  Serial.println("⚠️ Questo cancellerà TUTTE le calibrazioni salvate!");
  Serial.println("Digita 'CONFERMA' per procedere o qualsiasi altra cosa per annullare:");
  
  while (!Serial.available()) {
    delay(100);
  }
  
  String conferma = Serial.readStringUntil('\n');
  conferma.trim();
  conferma.toUpperCase();  // Per confronto con "CONFERMA"
  
  if (conferma == "CONFERMA") {
    // Reset valori
    fattoreCalibrazioneA = 1.0;
    fattoreCalibrazioneB = 1.0;
    offsetA = 0;
    offsetB = 0;
    calibrazioneCompletaA = false;
    calibrazioneCompletaB = false;
    
    // Reset HX711
    scalaA.set_scale(1.0);
    scalaB.set_scale(1.0);
    scalaA.set_offset(0);
    scalaB.set_offset(0);
    
    // Cancella EEPROM
    for (int i = 0; i < 16; i++) {
      EEPROM.write(i, 0xFF);
    }
    EEPROM.commit();
    
    Serial.println("✅ Reset completato! Tutte le calibrazioni sono state cancellate.");
  } else {
    Serial.println("❌ Reset annullato");
  }
}

// ============ INFORMAZIONI ============
void mostraInfo() {
  Serial.println("\n📋 === INFORMAZIONI TECNICHE ===");
  Serial.println("🔧 Hardware:");
  Serial.println("   ESP32 Dev Module");
  Serial.println("   2x HX711 (24-bit ADC per celle di carico)");
  Serial.println("   4x Celle di carico (2 per cassetto, cablate in parallelo)");
  
  Serial.println("\n📌 Pin Configuration:");
  Serial.println("   Cassetto A: DT=" + String(LOADCELL_DOUT_A) + ", SCK=" + String(LOADCELL_SCK_A));
  Serial.println("   Cassetto B: DT=" + String(LOADCELL_DOUT_B) + ", SCK=" + String(LOADCELL_SCK_B));
  
  Serial.println("\n💾 EEPROM Layout:");
  Serial.println("   Indirizzo " + String(ADDR_CALIBRATION_A) + ": Fattore calibrazione A");
  Serial.println("   Indirizzo " + String(ADDR_CALIBRATION_B) + ": Fattore calibrazione B");
  Serial.println("   Indirizzo " + String(ADDR_OFFSET_A) + ": Offset A");
  Serial.println("   Indirizzo " + String(ADDR_OFFSET_B) + ": Offset B");
  
  Serial.println("\n⚖️ Risoluzione:");
  Serial.println("   HX711: 24-bit (16.777.216 valori)");
  Serial.println("   Precisione tipica: 0.1g - 1g (dipende dalla cella)");
  
  Serial.println("\n🔬 Note tecniche:");
  Serial.println("   - Ogni cassetto ha 2 celle collegate in parallelo ad 1 HX711");
  Serial.println("   - La calibrazione va fatta con cassetti vuoti + peso noto");
  Serial.println("   - Temperature stabili migliorano la precisione");
  Serial.println("   - Evitare vibrazioni durante le misurazioni");
  
  Serial.println("\n📊 Capacità sistema:");
  Serial.println("   Frequenza campionamento: ~10-80 Hz");
  Serial.println("   Stabilizzazione: 3-5 secondi dopo carico");
  Serial.println("   Deriva termica: <0.1g per °C (tipica)");
}