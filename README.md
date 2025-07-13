Distributore Automatico Intelligente - ESP32

📝 Descrizione del Progetto

Sistema automatizzato per la gestione e distribuzione di componenti industriali con controllo accessi RFID, monitoraggio inventario in tempo reale e integrazione Power Apps.

🎯 Caratteristiche Principali

Autenticazione RFID: Accesso controllato tramite badge personalizzati
Gestione Inventario: Tracciamento automatico componenti con celle di carico
Interfaccia Touch: Display Nextion 3.5" per controllo intuitivo
Automazione Cassetti: Apertura/chiusura motorizzata con stepper motors
Cloud Integration: Sincronizzazione dati con Microsoft Power Apps
Operazioni Multiple: Prelievo, rientro, rifornimento e gestione manuale

🛠️ Hardware Utilizzato
ESP32 DevKit V1
Display Nextion NX4832K035_011
RFID Reader RC522
2x NEMA 17 + TB6600
4x Celle di carico + HX711

📦 Prodotti Gestiti

Cassetto A: Rondelle (peso unitario: 13g)
Cassetto B: Viti M8 (peso unitario: 29g)

🚀 Funzionalità del Sistema
1. Autenticazione e Sicurezza

Badge RFID univoci per ogni utente autorizzato
Timeout automatico sessioni (2 minuti)
Controllo accessi centralizzato

2. Operazioni Supportate

PRELIEVO: Ritiro componenti con aggiornamento automatico inventario
RIENTRO: Restituzione componenti precedentemente prelevati
RIFORNIMENTO: Aggiunta nuovi stock al magazzino
CHIUSURA MANUALE: Controllo manuale apertura cassetti

3. Monitoraggio Intelligente

Conteggio automatico tramite peso (precisione ±1 pezzo)
Calcolo quantità disponibili in tempo reale
Soglie minime personalizzabili per ogni prodotto

4. Integrazione Cloud

Sincronizzazione bidirezionale con Power Apps
Tracciabilità completa operazioni (chi, cosa, quando)
Dashboard analytics e reporting

📋 Schema Database Power Apps
Tabelle Principali:

Componenti: Anagrafica prodotti e quantità
Utenti: Gestione badge e autorizzazioni
Prelievo: Log operazioni di ritiro
Rientro: Log operazioni di restituzione
Rifornimento: Log operazioni di ricarica stock
Fornitori: Anagrafica fornitori componenti

🔧 Setup e Installazione
Prerequisiti

Arduino IDE 1.8.19+
Librerie: MFRC522, HX711, WiFi, HTTPClient, ArduinoJson
Account Microsoft Power Platform

Configurazione Hardware

Collegare componenti secondo pinout fornito
Calibrare celle di carico per ogni cassetto
Configurare display Nextion con file HMI

Configurazione Software

Compilare e caricare sketch Arduino
Configurare credenziali WiFi nel codice
Impostare webhook Power Automate
Registrare badge RFID utenti autorizzati

📊 Pinout ESP32

// RFID RC522
RST: 15, IRQ: 2, MISO: 4, SCK: 17, MOSI: 16, SS: 5

// MOTORI STEPPER  
Motore A: Enable=18, PUL=19, Dir=21
Motore B: Enable=22, PUL=23, Dir=32

// DISPLAY NEXTION
RX=35, TX=14

// CELLE DI CARICO
Cassetto A: DT=33, SCK=25  
Cassetto B: DT=26, SCK=27

🎮 Interfaccia Utente
Flusso Operativo:

Schermata Benvenuto: Richiesta badge RFID
Menu Operazioni: Selezione tipo operazione
Selezione Prodotto: Scelta componente da gestire
Operazione in Corso: Monitoraggio real-time peso/quantità
Conferma: Completamento e aggiornamento inventario

Indicatori Stato:

🟢 LED Verde: Utente autenticato
🔵 LED Blu: Connessione WiFi attiva
📊 Progress Bar: Quantità disponibili (0-30 pezzi)
⚖️ Display Peso: Peso attuale cassetto (±0.1g)

💡 Applicazioni Pratiche

Magazzini Industriali: Gestione componenti elettronici/meccanici
Laboratori R&D: Controllo materiali e strumentazione
Officine Meccaniche: Distribuzione utensili e ricambi
Ambiente Produttivo: Tracciabilità materiali per qualità

🛡️ Sicurezza e Affidabilità

Calibrazione automatica celle di carico con backup EEPROM
Gestione errori e valori anomali sensori
Retry automatico comunicazioni Power Apps
Protezione da sovraccarichi motori

📈 Vantaggi Business

Riduzione Sprechi: Tracciabilità completa prelievi
Ottimizzazione Stock: Monitoraggio automatico soglie minime
Controllo Accessi: Solo personale autorizzato
Audit Trail: Log completo per conformità ISO
ROI Rapido: Automazione processo manuale

🔮 Sviluppi Futuri

 Supporto multi-cassetto (espandibile a 8 cassetti)
 App mobile companion per gestione remota
 Integrazione bilancia di precisione esterna
 Sistema di backup locale su SD card
 Notifiche push per soglie minime raggiunte

📞 Supporto
Per questioni tecniche o richieste di personalizzazione, contattare il team di sviluppo.

Versione: 2.1.0
Ultimo aggiornamento: Gennaio 2025
Licenza: MIT
Autore: Michele Zago
