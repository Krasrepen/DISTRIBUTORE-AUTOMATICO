#include <SPI.h>
#include <MFRC522.h>
#include <HX711.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

// ============ PIN  ============
#define RST_PIN     15
#define IRQ_PIN     2   
#define MISO_PIN    4   
#define SCK_PIN     17  
#define MOSI_PIN    16  
#define SS_PIN      5   
#define NEXTION_RX  35
#define NEXTION_TX  14
#define MOTOR_A_ENABLE  18
#define MOTOR_A_STEP    19
#define MOTOR_A_DIR     21
#define MOTOR_B_ENABLE  22
#define MOTOR_B_STEP    23
#define MOTOR_B_DIR     32

// PIN CELLE CORRETTI
#define LOADCELL_A_DOUT 33  // Cassetto A (Rondelle)
#define LOADCELL_A_SCK  25  // Cassetto A (Rondelle)
#define LOADCELL_B_DOUT 26  // Cassetto B (Viti M8)
#define LOADCELL_B_SCK  27  // Cassetto B (Viti M8)

// ============ EEPROM LAYOUT ============
#define EEPROM_SIZE         512
#define ADDR_CALIBRATION_A  0   
#define ADDR_CALIBRATION_B  4   
#define ADDR_OFFSET_A       8   
#define ADDR_OFFSET_B       12  
#define ADDR_INVENTARIO     16  

// ============ CONFIGURAZIONE ============
#define STEPS_PER_REVOLUTION 200
#define MICROSTEP 16              
#define STEPS_TOTALI (STEPS_PER_REVOLUTION * MICROSTEP)  
#define GIRI_APERTURA 1.5
#define STEPS_APERTURA (int)(STEPS_TOTALI * GIRI_APERTURA)  
#define VELOCITA_DELAY 800        
#define TIMEOUT_AUTENTICAZIONE 120000

// ============ WIFI ============
const char* ssid = "ZAGOTRAM";
const char* password = "Coccodrillo1965";
const char* POWER_AUTOMATE_WEBHOOK = "https://prod-03.northeurope.logic.azure.com:443/workflows/ae3270c48d93463dbcd42e71d0d0e2c0/triggers/manual/paths/invoke?api-version=2016-06-01&sp=%2Ftriggers%2Fmanual%2Frun&sv=1.0&sig=ZxFl9I0Qwg17ifVM2IEF8HaPRoEThduHrXn2ncmqFcY";

// ============ STRUTTURE ============
struct Prodotto {
  String nome;
  int quantita;
  float pesoUnitario;
  int sogliaMinima;
};

struct Utente {
  byte uid[4];
  String nome;
  String cognome;
  String codiceBadge;
  bool autorizzato;
};

enum TipoOperazione { PRELIEVO, RIENTRO, RIFORNIMENTO, CHIUSURA_MANUALE };
enum StatoSistema { NON_AUTENTICATO, SELEZIONE_OPERAZIONE, SELEZIONE_PRODOTTO, OPERAZIONE_IN_CORSO, COMPLETATO };

// ============ VARIABILI GLOBALI ============
MFRC522 rfid(SS_PIN, RST_PIN);
HX711 scalaA, scalaB;

// *** FATTORI CALIBRAZIONE VERI ***
float fattoreCalibrazioneA = 1.0;
float fattoreCalibrazioneB = 1.0;
long offsetA = 0;
long offsetB = 0;
bool calibrazioneValidaA = false;
bool calibrazioneValidaB = false;

StatoSistema statoCorrente = NON_AUTENTICATO;
TipoOperazione operazioneCorrente;
bool utenteAutenticato = false;
unsigned long tempoAutenticazione = 0;
String nomeUtenteCorrente = "";
String cognomeUtenteCorrente = "";
String codiceBadgeCorrente = "";
int prodottoSelezionato = -1;
bool cassetto[2] = {false, false}; 
int cassettoCorrente = -1;
volatile int posizioneCorrente[2] = {0, 0};
volatile bool motoreAttivo = false;

// Pesi
float pesoA = 0, pesoB = 0;
float pesoIniziale = 0;

// *** VARIABILI PER WARNING E DISABILITAZIONE BOTTONE ***
bool bottoneConfermaDisabilitato = false;
unsigned long ultimoWarning = 0;

// Database
Prodotto prodotti[2] = {
  {"Rondella", 50, 13.0, 5},
  {"Vite M8", 100, 29.0, 3}
};

Utente utentiAutorizzati[] = {
  {{0xAC, 0xE8, 0x2C, 0x23}, "Michele", "Zago", "BADGE-001", true},
  {{0xB3, 0x8B, 0x1D, 0x40}, "Pietro", "Zago", "BADGE-002", true},
  {{0xAA, 0xBB, 0xCC, 0xDD}, "Admin", "System", "BADGE-000", true}
};

bool nextionDisponibile = false;

// ============================================================================
// 🚀 SETUP CON CARICAMENTO FATTORI VERI DA EEPROM
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  EEPROM.begin(EEPROM_SIZE);
  configuraPinMotori();
  inizializzaRFID();
  
  caricaFattoriCalibrazioneVeri();
  inizializzaCelleConFattoriVeri();
  
  inizializzaNextion();
  setupWiFi();
  
  testSistemaConFattoriVeri();
  
  delay(2000);
  inviaInventarioInizialeAPowerApps();
}

// ============================================================================
// 🔧 CARICA FATTORI VERI DA EEPROM
// ============================================================================

void caricaFattoriCalibrazioneVeri() {
  float tempFactorA, tempFactorB;
  long tempOffsetA, tempOffsetB;
  
  EEPROM.get(ADDR_CALIBRATION_A, tempFactorA);
  EEPROM.get(ADDR_CALIBRATION_B, tempFactorB);
  EEPROM.get(ADDR_OFFSET_A, tempOffsetA);
  EEPROM.get(ADDR_OFFSET_B, tempOffsetB);
  
  // Verifica validità Cassetto A
  if (!isnan(tempFactorA) && tempFactorA > 0.1 && tempFactorA < 100000) {
    fattoreCalibrazioneA = tempFactorA;
    offsetA = tempOffsetA;
    calibrazioneValidaA = true;
  } else {
    calibrazioneValidaA = true;
    fattoreCalibrazioneA = -522.15f; 
    offsetA = 169002; 
  }
  
  // Verifica validità Cassetto B
  if (!isnan(tempFactorB) && tempFactorB > 0.1 && tempFactorB < 100000) {
    fattoreCalibrazioneB = tempFactorB;
    offsetB = tempOffsetB;
    calibrazioneValidaB = true;
  } else {
    calibrazioneValidaB = true;
    fattoreCalibrazioneB = -518.97f; 
    offsetB = 293020; 
  }
}

void inizializzaCelleConFattoriVeri() {
  scalaA.begin(LOADCELL_A_DOUT, LOADCELL_A_SCK);
  scalaB.begin(LOADCELL_B_DOUT, LOADCELL_B_SCK);
  
  delay(1000);
  
  bool connessioneA = scalaA.is_ready();
  bool connessioneB = scalaB.is_ready();
  
  if (connessioneA) {
    scalaA.set_scale(fattoreCalibrazioneA);
    scalaA.set_offset(offsetA);
  }
  
  if (connessioneB) {
    scalaB.set_scale(fattoreCalibrazioneB);
    scalaB.set_offset(offsetB);
  }
}

void testSistemaConFattoriVeri() {
  if (scalaA.is_ready()) {
    float pesoTestA = scalaA.get_units(3);
  }
  
  if (scalaB.is_ready()) {
    float pesoTestB = scalaB.get_units(3);
  }
}

// ============================================================================
// ⚖️ LETTURA PESI CON FATTORI VERI
// ============================================================================

void aggiornaPesi() {
  static unsigned long ultimoAggiornamento = 0;
  
  if (millis() - ultimoAggiornamento > 1000) {
    
    // Cassetto A
    if (scalaA.is_ready()) {
      float peso_raw = scalaA.get_units(2);
      
      if (abs(peso_raw) < 10000 && !isnan(peso_raw) && !isinf(peso_raw)) {
        pesoA = peso_raw;
      } else {
        pesoA = 0;
      }
      
      if (abs(pesoA) < 0.5) pesoA = 0;
    } else {
      pesoA = 0;
    }
    
    // Cassetto B
    if (scalaB.is_ready()) {
      float peso_raw = scalaB.get_units(2);
      
      if (abs(peso_raw) < 10000 && !isnan(peso_raw) && !isinf(peso_raw)) {
        pesoB = peso_raw;
      } else {
        pesoB = 0;
      }
      
      if (abs(pesoB) < 0.5) pesoB = 0;
    } else {
      pesoB = 0;
    }
    
    ultimoAggiornamento = millis();
  }
}

// ============================================================================
// 🔧 CALCOLO PEZZI CON SEGNO CORRETTO
// ============================================================================

int calcolaPezziConSegno(float pesoDifferenza, int prodottoIndex, TipoOperazione operazione) {
  if (abs(pesoDifferenza) < 0.1) return 0;
  
  int pezziBase = (int)round(abs(pesoDifferenza) / prodotti[prodottoIndex].pesoUnitario);
  
  switch (operazione) {
    case PRELIEVO:
      return (pesoDifferenza < 0) ? pezziBase : -pezziBase;
      
    case RIENTRO:
    case RIFORNIMENTO:
      return (pesoDifferenza > 0) ? pezziBase : -pezziBase;
      
    default:
      return 0;
  }
}

// ============================================================================
// 🚨 CONTROLLO CONTINUO OPERAZIONI AL CONTRARIO CON DISABILITAZIONE BOTTONE
// ============================================================================

void controllaOperazioneAlContrario() {
  if (statoCorrente != OPERAZIONE_IN_CORSO || prodottoSelezionato == -1 || cassettoCorrente == -1) {
    return;
  }
  
  float pesoCorrente = (cassettoCorrente == 0) ? pesoA : pesoB;
  float pesoDifferenza = pesoCorrente - pesoIniziale;
  bool calibrazioneValida = (cassettoCorrente == 0) ? calibrazioneValidaA : calibrazioneValidaB;
  
  if (!calibrazioneValida) return;
  
  float sogliaMinima = prodotti[prodottoSelezionato].pesoUnitario * 0.2;
  
  bool operazioneAlContrario = false;
  String messaggioWarning = "";
  
  if (abs(pesoDifferenza) >= sogliaMinima) {
    int componentiModificati = calcolaPezziConSegno(pesoDifferenza, prodottoSelezionato, operazioneCorrente);
    
    switch(operazioneCorrente) {
      case PRELIEVO:
        if (componentiModificati < 0) {
          operazioneAlContrario = true;
          messaggioWarning = "AVVISO: STAI AGGIUNGENDO!";
        }
        break;
        
      case RIENTRO:
        if (componentiModificati < 0) {
          operazioneAlContrario = true;
          messaggioWarning = "AVVISO: STAI TOGLIENDO!";
        }
        break;
        
      case RIFORNIMENTO:
        if (componentiModificati < 0) {
          operazioneAlContrario = true;
          messaggioWarning = "AVVISO: STAI TOGLIENDO!";
        }
        break;
    }
  }
  
  if (operazioneAlContrario && !bottoneConfermaDisabilitato) {
    bottoneConfermaDisabilitato = true;
    ultimoWarning = millis();
    
    inviaComandoNextion("b0.en=0");
    inviaComandoNextion("b0.bco=63488");
    
    inviaComandoNextion("t1.txt=\"" + messaggioWarning + "\"");
    inviaComandoNextion("t1.pco=63488");
    
  } else if (!operazioneAlContrario && bottoneConfermaDisabilitato) {
    bottoneConfermaDisabilitato = false;
    
    inviaComandoNextion("b0.en=1");
    inviaComandoNextion("b0.bco=2016");
    
    inviaComandoNextion("t1.txt=\"" + prodotti[prodottoSelezionato].nome + "\"");
    inviaComandoNextion("t1.pco=65535");
  }
  
  if (bottoneConfermaDisabilitato && (millis() - ultimoWarning > 3000)) {
    ultimoWarning = millis();
    inviaComandoNextion("t1.txt=\"" + messaggioWarning + "\"");
  }
}

// ============================================================================
// 🔧 LOOP PRINCIPALE
// ============================================================================

void loop() {
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    gestisciRFID();
  }
  
  if (Serial2.available()) {
    gestisciComandoNextion();
  }
  
  aggiornaPesi();
  
  if (utenteAutenticato) {
    verificaTimeout();
  }
  
  static unsigned long ultimoDisplayUpdate = 0;
  if (millis() - ultimoDisplayUpdate > 1000) {
    if (statoCorrente == OPERAZIONE_IN_CORSO) {
      aggiornaDisplayNextion();
    }
    ultimoDisplayUpdate = millis();
  }
  
  delay(50);
}

// ============ INIZIALIZZAZIONE COMPONENTI ============

void configuraPinMotori() {
  pinMode(MOTOR_A_ENABLE, OUTPUT);
  pinMode(MOTOR_A_STEP, OUTPUT);
  pinMode(MOTOR_A_DIR, OUTPUT);
  digitalWrite(MOTOR_A_ENABLE, HIGH);
  
  pinMode(MOTOR_B_ENABLE, OUTPUT);
  pinMode(MOTOR_B_STEP, OUTPUT);
  pinMode(MOTOR_B_DIR, OUTPUT);
  digitalWrite(MOTOR_B_ENABLE, HIGH);
}

void inizializzaRFID() {
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  rfid.PCD_Init();
}

void inizializzaNextion() {
  Serial2.begin(9600, SERIAL_8N1, NEXTION_RX, NEXTION_TX);
  delay(500);
  
  Serial2.print("rest");
  Serial2.write(0xff);
  Serial2.write(0xff);
  Serial2.write(0xff);
  delay(3000);
  
  inviaComandoNextion("page 0");
  inviaComandoNextion("t0.txt=\"DISTRIBUTORE AUTOMATICO\"");
  inviaComandoNextion("t1.txt=\"Avvicina carta RFID\"");
  inviaComandoNextion("b0.val=0");
  inviaComandoNextion("b1.val=0");
  
  nextionDisponibile = true;
}

void setupWiFi() {
  WiFi.begin(ssid, password);
  
  int tentativi = 0;
  while (WiFi.status() != WL_CONNECTED && tentativi < 8) {
    delay(1000);
    tentativi++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    inviaComandoNextion("b1.val=1");
  }
}

void inviaComandoNextion(String comando) {
  if (!nextionDisponibile) return;
  
  Serial2.print(comando);
  Serial2.write(0xff);
  Serial2.write(0xff);
  Serial2.write(0xff);
  delay(50);
}

// ============ NEXTION ============

void gestisciComandoNextion() {
  String comando = "";
  
  unsigned long timeout = millis() + 100;
  while (Serial2.available() && millis() < timeout) {
    char c = Serial2.read();
    if (c == 0xFF) {
      if (Serial2.available() >= 2) {
        char c1 = Serial2.read();
        char c2 = Serial2.read();
        if (c1 == 0xFF && c2 == 0xFF) break;
      }
    } else if (c >= 32 && c <= 126) {
      comando += c;
    }
  }
  
  if (comando.length() > 0) {
    processaComandoNextion(comando);
  }
}

void processaComandoNextion(String cmd) {
  if (cmd == "op0") selezionaOperazione(0);
  else if (cmd == "op1") selezionaOperazione(1);
  else if (cmd == "op2") selezionaOperazione(2);
  else if (cmd == "op3") selezionaOperazione(3);
  else if (cmd == "prod0") selezionaProdotto(0);
  else if (cmd == "prod1") selezionaProdotto(1);
  else if (cmd == "conferma") confermaOperazione();
  else if (cmd == "annulla") annullaOperazione();
  else if (cmd == "logout") logout();
  else if (cmd == "b4") logout();
  else if (cmd == "back") {
    if (statoCorrente == SELEZIONE_PRODOTTO) {
      statoCorrente = SELEZIONE_OPERAZIONE;
      aggiornaDisplayLogin();
    }
  }
}

// ============ RFID ============

void gestisciRFID() {
  static unsigned long ultimaAutenticazione = 0;
  
  if (millis() - ultimaAutenticazione < 1000) {
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    return;
  }
  
  Utente* utente = verificaUtente();
  
  if (utenteAutenticato && utente != nullptr) {
    if (statoCorrente == NON_AUTENTICATO) {
      utenteAutenticato = false;
      nomeUtenteCorrente = "";
      cognomeUtenteCorrente = "";
      codiceBadgeCorrente = "";
      tempoAutenticazione = 0;
    }
  }
  
  if (utente != nullptr && !utenteAutenticato) {  
    utenteAutenticato = true;
    tempoAutenticazione = millis();
    ultimaAutenticazione = millis();
    nomeUtenteCorrente = utente->nome;
    cognomeUtenteCorrente = utente->cognome;
    codiceBadgeCorrente = utente->codiceBadge;
    statoCorrente = SELEZIONE_OPERAZIONE;
    
    inviaComandoNextion("page 1");
    delay(1000);
    
    inviaComandoNextion("b0.val=1");
    delay(100);
    
    inviaComandoNextion("t0.txt=\"Benvenuto " + nomeUtenteCorrente + "\"");
    delay(100);
    
    inviaComandoNextion("t1.txt=\"Seleziona operazione\"");
    delay(100);
    
  } else if (utente != nullptr && utenteAutenticato) {
    tempoAutenticazione = millis();
    ultimaAutenticazione = millis();
    
  } else {
    ultimaAutenticazione = millis();
    inviaComandoNextion("t0.txt=\"ACCESSO NEGATO\"");
    inviaComandoNextion("t1.txt=\"Badge non autorizzato\"");
    delay(2000);
    inviaComandoNextion("t0.txt=\"DISTRIBUTORE AUTOMATICO\"");
    inviaComandoNextion("t1.txt=\"Avvicina carta RFID\"");
  }
  
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

Utente* verificaUtente() {
  for (int i = 0; i < sizeof(utentiAutorizzati) / sizeof(utentiAutorizzati[0]); i++) {
    bool uidCorrispondente = true;
    
    for (byte j = 0; j < rfid.uid.size && j < 4; j++) {
      if (rfid.uid.uidByte[j] != utentiAutorizzati[i].uid[j]) {
        uidCorrispondente = false;
        break;
      }
    }
    
    if (uidCorrispondente && utentiAutorizzati[i].autorizzato) {
      return &utentiAutorizzati[i];
    }
  }
  return nullptr;
}

void verificaTimeout() {
  if (utenteAutenticato && (millis() - tempoAutenticazione > TIMEOUT_AUTENTICAZIONE)) {
    logout();
  }
}

void logout() {
  utenteAutenticato = false;
  nomeUtenteCorrente = "";
  cognomeUtenteCorrente = "";
  codiceBadgeCorrente = "";
  statoCorrente = NON_AUTENTICATO;
  tempoAutenticazione = 0;
  
  bottoneConfermaDisabilitato = false;
  ultimoWarning = 0;
  
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(100);
  
  for (int i = 0; i < 2; i++) {
    if (cassetto[i]) {
      chiudiCassetto(i);
    }
  }
  
  prodottoSelezionato = -1;
  cassettoCorrente = -1;
  
  inviaComandoNextion("page 0");
  delay(1000);
  
  inviaComandoNextion("t0.txt=\"DISTRIBUTORE AUTOMATICO\"");
  delay(200);
  
  inviaComandoNextion("t1.txt=\"Avvicina carta RFID\"");
  delay(200);
  
  inviaComandoNextion("b0.val=0");
  delay(200);
  
  delay(1000);
}

// ============ OPERAZIONI ============

void selezionaOperazione(int op) {
  if (!utenteAutenticato) return;
  
  operazioneCorrente = (TipoOperazione)op;
  String nomiOperazioni[] = {"PRELIEVO", "RIENTRO", "RIFORNIMENTO", "CHIUSURA MANUALE"};
  
  if (operazioneCorrente == CHIUSURA_MANUALE) {
    for (int i = 0; i < 2; i++) {
      if (cassetto[i]) {
        chiudiCassetto(i);
      }
    }
    inviaComandoNextion("t0.txt=\"Cassetti chiusi\"");
    delay(2000);
    aggiornaDisplayLogin();
  } else {
    statoCorrente = SELEZIONE_PRODOTTO;
    inviaComandoNextion("page 2");
    delay(500);
    inviaComandoNextion("t0.txt=\"" + nomiOperazioni[op] + "\"");
    inviaComandoNextion("t1.txt=\"Seleziona prodotto\"");
    inviaComandoNextion("b2.txt=\"" + prodotti[0].nome + " (" + String(prodotti[0].quantita) + ")\"");
    inviaComandoNextion("b3.txt=\"" + prodotti[1].nome + " (" + String(prodotti[1].quantita) + ")\"");
  }
}

void selezionaProdotto(int prod) {
  if (!utenteAutenticato || prod < 0 || prod >= 2) return;
  
  prodottoSelezionato = prod;
  cassettoCorrente = prod;
  
  bottoneConfermaDisabilitato = false;
  ultimoWarning = 0;
  
  aggiornaPesi();
  delay(1000);
  
  pesoIniziale = (cassettoCorrente == 0) ? pesoA : pesoB;
  
  apriCassetto(cassettoCorrente);
  statoCorrente = OPERAZIONE_IN_CORSO;
  
  inviaComandoNextion("page 3");
  delay(500);
  
  String istruzioni = "";
  switch (operazioneCorrente) {
    case PRELIEVO: istruzioni = "PRELIEVO IN CORSO"; break;
    case RIENTRO: istruzioni = "RIENTRO IN CORSO"; break;
    case RIFORNIMENTO: istruzioni = "RIFORNIMENTO IN CORSO"; break;
  }
  
  inviaComandoNextion("t0.txt=\"" + istruzioni + "\"");
  inviaComandoNextion("t1.txt=\"" + prodotti[prod].nome + "\"");
  inviaComandoNextion("t2.txt=\"Peso: " + String(pesoIniziale, 1) + "g\"");
  inviaComandoNextion("t3.txt=\"0 (0)\"");
  inviaComandoNextion("j0.val=0");
  
  inviaComandoNextion("b0.en=1");
  inviaComandoNextion("b0.bco=2016");
}

void confermaOperazione() {
  if (!utenteAutenticato || prodottoSelezionato == -1 || cassettoCorrente == -1) return;
  
  if (bottoneConfermaDisabilitato) {
    inviaComandoNextion("t0.txt=\"CORREGGI OPERAZIONE\"");
    delay(1000);
    
    String istruzioni = "";
    switch (operazioneCorrente) {
      case PRELIEVO: istruzioni = "PRELIEVO IN CORSO"; break;
      case RIENTRO: istruzioni = "RIENTRO IN CORSO"; break;
      case RIFORNIMENTO: istruzioni = "RIFORNIMENTO IN CORSO"; break;
    }
    inviaComandoNextion("t0.txt=\"" + istruzioni + "\"");
    return;
  }
  
  aggiornaPesi();  
  delay(500);
  aggiornaPesi();  
  
  float pesoFinale = (cassettoCorrente == 0) ? pesoA : pesoB;
  float pesoDifferenza = pesoFinale - pesoIniziale;
  
  int componentiModificati = 0;
  bool operazioneValida = false;
  
  if ((cassettoCorrente == 0 && calibrazioneValidaA) || (cassettoCorrente == 1 && calibrazioneValidaB)) {
    
    float sogliaMinima = prodotti[prodottoSelezionato].pesoUnitario * 0.2;
    
    if (abs(pesoDifferenza) >= sogliaMinima) {
      componentiModificati = calcolaPezziConSegno(pesoDifferenza, prodottoSelezionato, operazioneCorrente);
      
      if (componentiModificati != 0) {
        operazioneValida = true;
        
        if (componentiModificati > 0) {
          if (operazioneCorrente == PRELIEVO) {
            prodotti[prodottoSelezionato].quantita -= componentiModificati;
          } else {
            prodotti[prodottoSelezionato].quantita += componentiModificati;
          }
        } else {
          if (operazioneCorrente == PRELIEVO) {
            prodotti[prodottoSelezionato].quantita -= componentiModificati;
          } else {
            prodotti[prodottoSelezionato].quantita += componentiModificati;
          }
        }
      }
    }
  }
  
  if (prodotti[prodottoSelezionato].quantita < 0) {
    prodotti[prodottoSelezionato].quantita = 0;
  }
  
  if (WiFi.status() == WL_CONNECTED && operazioneValida && componentiModificati != 0) {
    String tipoOp = "";
    switch (operazioneCorrente) {
      case PRELIEVO: tipoOp = "PRELIEVO"; break;
      case RIENTRO: tipoOp = "RIENTRO"; break;
      case RIFORNIMENTO: tipoOp = "RIFORNIMENTO"; break;
    }
    
    inviaAPowerAutomate(tipoOp, codiceBadgeCorrente, prodotti[prodottoSelezionato].nome, abs(componentiModificati), abs(pesoDifferenza));
  }
  
  chiudiCassetto(cassettoCorrente);
  
  inviaComandoNextion("page 1");
  if (abs(componentiModificati) > 0) {
    String messaggioFinale = "";
    if (componentiModificati > 0 && 
        ((operazioneCorrente == PRELIEVO) || 
         (operazioneCorrente == RIENTRO || operazioneCorrente == RIFORNIMENTO))) {
      String operazioneRichiesta = "";
      switch(operazioneCorrente) {
        case PRELIEVO: operazioneRichiesta = "PRELIEVO"; break;
        case RIENTRO: operazioneRichiesta = "RIENTRO"; break; 
        case RIFORNIMENTO: operazioneRichiesta = "RIFORNIMENTO"; break;
      }
      messaggioFinale = "OK " + operazioneRichiesta + ": " + String(abs(componentiModificati)) + " " + prodotti[prodottoSelezionato].nome;
    } else {
      messaggioFinale = "ERRORE: " + String(abs(componentiModificati)) + " " + prodotti[prodottoSelezionato].nome + " (inaspettato)";
    }
    inviaComandoNextion("t0.txt=\"Operazione completata\"");
    inviaComandoNextion("t1.txt=\"" + messaggioFinale + "\"");
  } else {
    inviaComandoNextion("t0.txt=\"Nessuna modifica\"");
    inviaComandoNextion("t1.txt=\"Peso invariato\"");
  }
  
  prodottoSelezionato = -1;
  cassettoCorrente = -1;
  statoCorrente = COMPLETATO;
  
  delay(3000);
  statoCorrente = SELEZIONE_OPERAZIONE;
  aggiornaDisplayLogin();
}

void annullaOperazione() {
  if (!utenteAutenticato || prodottoSelezionato == -1) return;
  
  if (cassettoCorrente >= 0) {
    chiudiCassetto(cassettoCorrente);
  }
  
  bottoneConfermaDisabilitato = false;
  ultimoWarning = 0;
  
  prodottoSelezionato = -1;
  cassettoCorrente = -1;
  statoCorrente = SELEZIONE_OPERAZIONE;
  
  inviaComandoNextion("page 1");
  delay(500);
  inviaComandoNextion("t0.txt=\"Operazione annullata\"");
  delay(2000);
  aggiornaDisplayLogin();
}

void aggiornaDisplayLogin() {
  inviaComandoNextion("page 1");
  delay(500);
  inviaComandoNextion("b0.val=1");
  inviaComandoNextion("t0.txt=\"Benvenuto " + nomeUtenteCorrente + "\"");
  inviaComandoNextion("t1.txt=\"Seleziona operazione\"");
}

// ============================================================================
// 🖥️ DISPLAY AGGIORNATO CON CONTROLLO OPERAZIONI AL CONTRARIO
// ============================================================================

void aggiornaLedStato() {
  if (nextionDisponibile) {
    inviaComandoNextion("b0.val=" + String(utenteAutenticato ? 1 : 0));
    inviaComandoNextion("b1.val=" + String(WiFi.status() == WL_CONNECTED ? 1 : 0));
  }
}

void aggiornaDisplayNextion() {
  static unsigned long ultimoAggiornamento = 0;
  
  if (millis() - ultimoAggiornamento > 1000) {
    if (nextionDisponibile) {
      aggiornaLedStato();
      
      if (statoCorrente == OPERAZIONE_IN_CORSO && prodottoSelezionato >= 0 && cassettoCorrente >= 0) {
        
        controllaOperazioneAlContrario();
        
        float pesoCorrente = (cassettoCorrente == 0) ? pesoA : pesoB;
        bool calibrazioneValida = (cassettoCorrente == 0) ? calibrazioneValidaA : calibrazioneValidaB;
        
        String pesoTesto = "ERR";
        int pezziModificati = 0;
        int pezziPresenti = 0;
        
        if (calibrazioneValida && abs(pesoCorrente) < 5000) {
          pesoTesto = String(pesoCorrente, 1) + "g";
          
          float pesoDifferenza = pesoCorrente - pesoIniziale;
          pezziModificati = calcolaPezziConSegno(pesoDifferenza, prodottoSelezionato, operazioneCorrente);
          
          if (abs(pesoCorrente) > 1.0) {
            pezziPresenti = (int)round(abs(pesoCorrente) / prodotti[prodottoSelezionato].pesoUnitario);
          } else {
            pezziPresenti = 0;
          }
          
          if (!bottoneConfermaDisabilitato) {
            inviaComandoNextion("t2.txt=\"Peso: " + pesoTesto + "\"");
          }
          
        } else if (!calibrazioneValida) {
          pesoTesto = "NO CAL";
          if (!bottoneConfermaDisabilitato) {
            inviaComandoNextion("t2.txt=\"Peso: " + pesoTesto + "\"");
          }
        } else {
          pesoTesto = "ERRORE";
          if (!bottoneConfermaDisabilitato) {
            inviaComandoNextion("t2.txt=\"Peso: " + pesoTesto + "\"");
          }
        }
        
        String counterDisplay = String(pezziPresenti) + " (" + String(pezziModificati) + ")";
        inviaComandoNextion("t3.txt=\"" + counterDisplay + "\"");
        
        int percentuale = min(100, (pezziPresenti * 100) / 30);
        inviaComandoNextion("j0.val=" + String(percentuale));
      }
    }
    
    ultimoAggiornamento = millis();
  }
}

// ============ MOTORI ============

void muoviMotore(int cassetto, int passi, bool direzione) {
  if (passi == 0 || cassetto < 0 || cassetto > 1) return;
  
  motoreAttivo = true;
  
  int enablePin = (cassetto == 0) ? MOTOR_A_ENABLE : MOTOR_B_ENABLE;
  int stepPin = (cassetto == 0) ? MOTOR_A_STEP : MOTOR_B_STEP;
  int dirPin = (cassetto == 0) ? MOTOR_A_DIR : MOTOR_B_DIR;
  
  digitalWrite(enablePin, LOW);
  digitalWrite(dirPin, direzione ? HIGH : LOW);
  delayMicroseconds(1);
  
  for(int i = 0; i < abs(passi); i++) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(VELOCITA_DELAY);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(VELOCITA_DELAY);
    
    posizioneCorrente[cassetto] += direzione ? 1 : -1;
    if (posizioneCorrente[cassetto] < 0) {
      posizioneCorrente[cassetto] = 0;
      break;
    }
  }
  
  digitalWrite(enablePin, HIGH);
  motoreAttivo = false;
}

void apriCassetto(int numeroCassetto) {
  if (numeroCassetto < 0 || numeroCassetto > 1 || cassetto[numeroCassetto]) return;
  
  String nomeCassetto = (numeroCassetto == 0) ? "A (Rondelle)" : "B (Viti M8)";
  inviaComandoNextion("t1.txt=\"Apertura " + nomeCassetto + "...\"");
  
  muoviMotore(numeroCassetto, STEPS_APERTURA, true);
  
  cassetto[numeroCassetto] = true;
  cassettoCorrente = numeroCassetto;
  inviaComandoNextion("t1.txt=\"Cassetto " + nomeCassetto + " aperto\"");
}

void chiudiCassetto(int numeroCassetto) {
  if (numeroCassetto == -1) numeroCassetto = cassettoCorrente;
  if (numeroCassetto < 0 || numeroCassetto > 1 || !cassetto[numeroCassetto]) return;
  
  String nomeCassetto = (numeroCassetto == 0) ? "A (Rondelle)" : "B (Viti M8)";
  inviaComandoNextion("t1.txt=\"Chiusura " + nomeCassetto + "...\"");
  
  muoviMotore(numeroCassetto, posizioneCorrente[numeroCassetto], false);
  posizioneCorrente[numeroCassetto] = 0;
  
  cassetto[numeroCassetto] = false;
  if (cassettoCorrente == numeroCassetto) cassettoCorrente = -1;
  inviaComandoNextion("t1.txt=\"Cassetto " + nomeCassetto + " chiuso\"");
}

// ============ POWER AUTOMATE ============

void inviaInventarioInizialeAPowerApps() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  
  aggiornaPesi();
  delay(1000);
  aggiornaPesi();
  
  int quantitaRealeA = 0, quantitaRealeB = 0;
  
  if (calibrazioneValidaA && scalaA.is_ready() && abs(pesoA) > 1.0) {
    quantitaRealeA = (int)round(abs(pesoA) / prodotti[0].pesoUnitario);
  }
  
  if (calibrazioneValidaB && scalaB.is_ready() && abs(pesoB) > 1.0) {
    quantitaRealeB = (int)round(abs(pesoB) / prodotti[1].pesoUnitario);
  }
  
  prodotti[0].quantita = quantitaRealeA;
  prodotti[1].quantita = quantitaRealeB;
  
  HTTPClient http;
  http.begin(POWER_AUTOMATE_WEBHOOK);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);
  
  StaticJsonDocument<400> doc;
  doc["operazione"] = "INVENTARIO_INIZIALE";
  doc["utente"] = "SISTEMA";
  doc["componente"] = "SINCRONIZZAZIONE";
  doc["quantita"] = 0;
  doc["peso"] = 0;
  doc["sorgente"] = "ESP32_AVVIO";
  doc["quantita_rondelle"] = quantitaRealeA;
  doc["quantita_viti_m8"] = quantitaRealeB;
  doc["peso_cassetto_a"] = pesoA;
  doc["peso_cassetto_b"] = pesoB;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  int httpResponseCode = http.POST(jsonString);
  
  http.end();
}

void inviaAPowerAutomate(String operazione, String utente, String componente, int quantita, float peso) {
  if (WiFi.status() != WL_CONNECTED) return;
  
  HTTPClient http;
  http.begin(POWER_AUTOMATE_WEBHOOK);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);
  
  StaticJsonDocument<400> doc;
  doc["operazione"] = operazione;
  doc["utente"] = utente;
  doc["componente"] = componente;
  doc["quantita"] = quantita;
  doc["peso"] = peso;
  doc["sorgente"] = "ESP32_CALIBRATO";
  doc["quantita_rondelle"] = prodotti[0].quantita;
  doc["quantita_viti_m8"] = prodotti[1].quantita;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  http.POST(jsonString);
  http.end();
}