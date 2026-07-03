/*********************************************
  Autor: Marlon Nardi Walendorff
  Projeto: Solda ponto para baterias de lítio com Arduino
  Atualização: Jorge Souza
**********************************************/

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1331.h>
#include <SPI.h>
#include <RotaryEncoder.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecureBearSSL.h>
#include <EEPROM.h>
#include <DNSServer.h>

#define FIRMWARE_VERSION "1.0.10"

// Pinos
#define pin_Trigger 12       // D6
#define pin_Triac 3          // RX / GPIO3
#define pin_Encoder_CLK 0    // D3 / GPIO0
#define pin_Encoder_DT 2     // D4 / GPIO2
#define pin_Encoder_SW 16    // D0 / GPIO16

// Rede elétrica
#define MAINS_HZ 60
#define HALF_CYCLE_US (1000000UL / (2UL * MAINS_HZ))

const uint8_t OLED_pin_scl_sck  = 13; // D7
const uint8_t OLED_pin_sda_mosi = 14; // D5
const uint8_t OLED_pin_cs_ss    = 15; // D8
const uint8_t OLED_pin_res_rst  = 5;  // D1
const uint8_t OLED_pin_dc_rs    = 4;  // D2

// Parâmetros de tempo
#define min_Time_ms 3
#define max_Time_ms 120
#define min_Cycle_ms 1
#define max_Cycle_ms 50

// EEPROM
#define EEPROM_SIZE 512
#define EEPROM_MAGIC_ADDR 500
#define EEPROM_MAGIC_VALUE 0x42
#define EEPROM_WIFI_CONFIG_START 0
#define EEPROM_CYCLE_MS_ADDR 100
#define EEPROM_TIME_MS_ADDR 102
#define EEPROM_COUNTER_ADDR 104

// Display
#define SCREEN_WIDTH 96
#define SCREEN_HEIGHT 64

const byte DNS_PORT = 53;

const uint16_t OLED_Color_Black   = 0x0000;
const uint16_t OLED_Color_Blue    = 0x001F;
const uint16_t OLED_Color_Red     = 0xF800;
const uint16_t OLED_Color_Green   = 0x07E0;
const uint16_t OLED_Color_Cyan    = 0x07FF;
const uint16_t OLED_Color_Magenta = 0xF81F;
const uint16_t OLED_Color_Yellow  = 0xFFE0;
const uint16_t OLED_Color_White   = 0xFFFF;

uint16_t OLED_Text_Color       = OLED_Color_Black;
uint16_t OLED_Background_Color = OLED_Color_Blue;

Adafruit_SSD1331 display = Adafruit_SSD1331(
  OLED_pin_cs_ss,
  OLED_pin_dc_rs,
  OLED_pin_sda_mosi,
  OLED_pin_scl_sck,
  OLED_pin_res_rst
);

RotaryEncoder *encoder = nullptr;
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
DNSServer dnsServer;

struct WifiConfig {
  char ssid[32];
  char pass[64];
};

WifiConfig wifiConfig;
bool configMode = false;

enum MenuState { NORMAL, EDITANDO_CICLO, EDITANDO_TEMPO };
MenuState menuState = NORMAL;

uint16_t cycle_ms = 0;
uint16_t time_ms = 0;
uint32_t soldaCount = 0;

bool buttonHeld = false;
unsigned long buttonPressStart = 0;
unsigned long lastBlink = 0;
unsigned long lastWatchdogReset = 0;

int lastEncoderPos = 0;
uint16_t last_cycle_ms = 0;
uint16_t last_time_ms = 0;
uint32_t last_soldaCount = 0;
String last_wifi_status = "";

bool showValue = true;
const unsigned long WATCHDOG_TIMEOUT = 30000; // 30 segundos
const unsigned long longPressDuration = 5000;
byte aux2 = 0;

const char* githubAPI = "https://api.github.com/repos/JorgeBeserra/SoldaPonto/releases/latest";

void checkEEPROM();
void loadWifiConfig();
void saveWifiConfig(bool allowEmpty = false);
void saveSettings();
void loadSettings();
void saveCounter();
void loadCounter();
void connectWifi();
void startConfigPortal();
void monitorWifi();
void checkForUpdate();
void showOtaMessage(const String& msg);
void checaBotaoEncoder();
void atualizaValoresEncoder();
void atualizaBlink();
void watchdogCheck();
void watchdogReset();
void splashScreen();
void screenOne();
String getWifiStatus();
static inline uint32_t roundToHalfCycles(uint32_t us);
void delayMicrosFeed(uint32_t us);
void trigger();
String extractJsonString(const String& json, const String& key);
bool isNewerVersion(String current, String latest);

IRAM_ATTR void checkPosition() {
  if (encoder != nullptr) {
    encoder->tick();
  }
}

void setup() {
  Serial.begin(74880);
  lastWatchdogReset = millis();

  Serial.println("Display iniciando...");
  display.begin();
  display.setFont();
  splashScreen();

  encoder = new RotaryEncoder(pin_Encoder_CLK, pin_Encoder_DT, RotaryEncoder::LatchMode::TWO03);

  digitalWrite(pin_Triac, LOW);
  pinMode(pin_Triac, OUTPUT);
  pinMode(pin_Encoder_SW, INPUT_PULLUP);
  pinMode(pin_Trigger, INPUT_PULLUP);
  pinMode(pin_Encoder_CLK, INPUT_PULLUP);
  pinMode(pin_Encoder_DT, INPUT_PULLUP);

  checkEEPROM();
  loadWifiConfig();
  loadSettings();
  loadCounter();

  lastEncoderPos = encoder->getPosition();

  attachInterrupt(digitalPinToInterrupt(pin_Encoder_CLK), checkPosition, CHANGE);
  attachInterrupt(digitalPinToInterrupt(pin_Encoder_DT), checkPosition, CHANGE);

  if (digitalRead(pin_Encoder_SW) == LOW) {
    startConfigPortal();
  } else {
    connectWifi();
  }

  display.fillScreen(OLED_Background_Color);
}

bool isNewerVersion(String current, String latest) {
  current.replace("v", "");
  current.replace("V", "");
  latest.replace("v", "");
  latest.replace("V", "");

  int currentParts[3] = {0, 0, 0};
  int latestParts[3] = {0, 0, 0};

  for (int i = 0; i < 3; i++) {
    int dot = current.indexOf('.');
    String part = (dot >= 0) ? current.substring(0, dot) : current;
    currentParts[i] = part.toInt();
    if (dot < 0) break;
    current = current.substring(dot + 1);
  }

  for (int i = 0; i < 3; i++) {
    int dot = latest.indexOf('.');
    String part = (dot >= 0) ? latest.substring(0, dot) : latest;
    latestParts[i] = part.toInt();
    if (dot < 0) break;
    latest = latest.substring(dot + 1);
  }

  for (int i = 0; i < 3; i++) {
    if (latestParts[i] > currentParts[i]) return true;
    if (latestParts[i] < currentParts[i]) return false;
  }

  return false;
}

String extractJsonString(const String& json, const String& key) {
  String token = "\"" + key + "\"";
  int keyIndex = json.indexOf(token);
  if (keyIndex < 0) return "";

  int colonIndex = json.indexOf(':', keyIndex + token.length());
  if (colonIndex < 0) return "";

  int firstQuote = json.indexOf('"', colonIndex + 1);
  if (firstQuote < 0) return "";

  int secondQuote = json.indexOf('"', firstQuote + 1);
  if (secondQuote < 0) return "";

  return json.substring(firstQuote + 1, secondQuote);
}

void watchdogCheck() {
  if (millis() - lastWatchdogReset > WATCHDOG_TIMEOUT) {
    Serial.println("Watchdog ativado. Reiniciando o ESP...");
    ESP.restart();
  }
}

void watchdogReset() {
  lastWatchdogReset = millis();
}

void showOtaMessage(const String& msg) {
  display.fillScreen(OLED_Color_Black);
  display.setTextColor(OLED_Color_White);
  display.setTextSize(1);

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(msg, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2);
  display.print(msg);
}

void checkForUpdate() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Sem Wi-Fi. Cancelando verificacao OTA.");
    showOtaMessage("Sem Wi-Fi");
    delay(2000);
    return;
  }

  showOtaMessage("Verificando");
  Serial.println("=======================================");
  Serial.println("Verificacao de atualizacao OTA");
  Serial.printf("Firmware atual: v%s\n", FIRMWARE_VERSION);

  IPAddress githubIP;
  Serial.print("Testando DNS api.github.com...");
  if (!WiFi.hostByName("api.github.com", githubIP)) {
    Serial.println("Falha DNS.");
    showOtaMessage("Falha DNS");
    delay(2000);
    return;
  }
  Serial.println(githubIP);

  BearSSL::WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  if (!https.begin(client, githubAPI)) {
    Serial.println("Falha ao iniciar HTTPS.");
    showOtaMessage("HTTPS FAIL");
    delay(2000);
    return;
  }

  https.addHeader("User-Agent", "ESP8266-SoldaPonto");
  int httpCode = https.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("Falha na conexao. HTTP Code: %d\n", httpCode);
    showOtaMessage("HTTP FAIL");
    https.end();
    delay(2000);
    return;
  }

  String payload = https.getString();
  String latestVersion = extractJsonString(payload, "tag_name");
  String binUrl = extractJsonString(payload, "browser_download_url");
  https.end();

  latestVersion.replace("v", "");
  latestVersion.replace("V", "");
  latestVersion.trim();
  binUrl.trim();

  if (latestVersion.length() == 0) {
    Serial.println("Nao encontrou tag_name no JSON.");
    showOtaMessage("Sem versao");
    delay(2000);
    return;
  }

  if (binUrl.length() == 0) {
    Serial.println("Nao encontrou browser_download_url no JSON.");
    showOtaMessage("Sem binario");
    delay(2000);
    return;
  }

  Serial.printf("Ultima versao no GitHub: %s\n", latestVersion.c_str());
  Serial.println(binUrl);

  if (!isNewerVersion(FIRMWARE_VERSION, latestVersion)) {
    Serial.println("Firmware ja esta atualizado.");
    showOtaMessage("Ja atual");
    delay(2000);
    return;
  }

  showOtaMessage("Atualizando");
  Serial.println("Nova versao encontrada. Iniciando OTA...");

  BearSSL::WiFiClientSecure updateClient;
  updateClient.setInsecure();

  ESPhttpUpdate.rebootOnUpdate(true);
  ESPhttpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  watchdogReset();
  yield();
  t_httpUpdate_return result = ESPhttpUpdate.update(updateClient, binUrl);
  watchdogReset();
  yield();

  switch (result) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("OTA falhou. Erro (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      showOtaMessage("Erro OTA");
      delay(3000);
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("Nenhuma atualizacao disponivel.");
      showOtaMessage("Sem atualizacao");
      delay(2000);
      break;
    case HTTP_UPDATE_OK:
      Serial.println("Atualizacao concluida.");
      showOtaMessage("Atualizado");
      delay(2000);
      break;
  }
}

void loadWifiConfig() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(EEPROM_WIFI_CONFIG_START, wifiConfig);
  EEPROM.end();

  if (wifiConfig.ssid[0] == 0xFF || wifiConfig.ssid[0] == '\0') {
    memset(&wifiConfig, 0, sizeof(WifiConfig));
    Serial.println("Nenhuma rede Wi-Fi salva.");
    return;
  }

  wifiConfig.ssid[sizeof(wifiConfig.ssid) - 1] = '\0';
  wifiConfig.pass[sizeof(wifiConfig.pass) - 1] = '\0';

  Serial.println("Wi-Fi carregado da EEPROM:");
  Serial.printf("SSID: %s\n", wifiConfig.ssid);
}

void saveWifiConfig(bool allowEmpty) {
  if (!allowEmpty && strlen(wifiConfig.ssid) == 0) {
    Serial.println("SSID vazio. Nao salvando Wi-Fi.");
    return;
  }

  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(EEPROM_WIFI_CONFIG_START, wifiConfig);
  EEPROM.commit();
  EEPROM.end();

  if (strlen(wifiConfig.ssid) == 0) {
    Serial.println("Wi-Fi limpo na EEPROM.");
  } else {
    Serial.println("Wi-Fi salvo na EEPROM.");
    Serial.printf("SSID: %s\n", wifiConfig.ssid);
  }
}

void saveSettings() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(EEPROM_CYCLE_MS_ADDR, cycle_ms);
  EEPROM.put(EEPROM_TIME_MS_ADDR, time_ms);
  EEPROM.commit();
  EEPROM.end();
}

void loadSettings() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(EEPROM_CYCLE_MS_ADDR, cycle_ms);
  EEPROM.get(EEPROM_TIME_MS_ADDR, time_ms);
  EEPROM.end();

  if (cycle_ms < min_Cycle_ms || cycle_ms > max_Cycle_ms) cycle_ms = 20;
  if (time_ms < min_Time_ms || time_ms > max_Time_ms) time_ms = 20;
}

void saveCounter() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(EEPROM_COUNTER_ADDR, soldaCount);
  EEPROM.commit();
  EEPROM.end();
}

void loadCounter() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(EEPROM_COUNTER_ADDR, soldaCount);
  EEPROM.end();

  if (soldaCount == 0xFFFFFFFF) {
    soldaCount = 0;
  }
}

void connectWifi() {
  if (wifiConfig.ssid[0] == 0 || wifiConfig.ssid[0] == 0xFF || wifiConfig.ssid[0] == '\0') {
    Serial.println("Nenhuma rede configurada.");
    return;
  }

  Serial.printf("Conectando no Wi-Fi SSID: %s...\n", wifiConfig.ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiConfig.ssid, wifiConfig.pass);

  unsigned long startAttemptTime = millis();
  const unsigned long wifiTimeout = 15000;

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < wifiTimeout) {
    Serial.print(".");
    delay(250);
    yield();
    watchdogReset();
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi conectado.");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    checkForUpdate();
  } else {
    Serial.println("\nFalha ao conectar. Operando offline.");
  }
}

void monitorWifi() {
  static unsigned long lastCheck = 0;

  if (millis() - lastCheck <= 10000) return;
  lastCheck = millis();

  if (configMode) return;

  if (wifiConfig.ssid[0] == 0 || wifiConfig.ssid[0] == 0xFF || wifiConfig.ssid[0] == '\0') {
    return;
  }

  if (WiFi.status() == WL_CONNECTED) return;

  Serial.println("Wi-Fi desconectado. Tentando reconectar...");
  WiFi.disconnect();
  WiFi.begin(wifiConfig.ssid, wifiConfig.pass);

  unsigned long startReconnect = millis();
  const unsigned long reconnectTimeout = 10000;

  while (WiFi.status() != WL_CONNECTED && millis() - startReconnect < reconnectTimeout) {
    Serial.print(".");
    delay(250);
    yield();
    watchdogReset();
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nReconectado ao Wi-Fi.");
  } else {
    Serial.println("\nFalha ao reconectar. Operando offline.");
  }
}

void startConfigPortal() {
  configMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("SoldaPontoConfig");
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  httpUpdater.setup(&server, "/update");

  server.on("/", HTTP_GET, []() {
    String ip = WiFi.softAPIP().toString();
    String page = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-br">
<head>
<meta charset="UTF-8">
<title>SoldaPonto Config</title>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
  body { font-family: Arial; background-color: #202124; color: #e8eaed; text-align: center; margin: 0; padding: 20px; }
  h2 { color: #fbbc04; }
  .container { background-color: #303134; padding: 20px; border-radius: 10px; display: inline-block; max-width: 400px; width: 90%; box-shadow: 0 0 10px rgba(0,0,0,0.7); }
  input, button { padding: 10px; margin: 8px 0; border-radius: 5px; border: none; width: 100%; box-sizing: border-box; }
  input { background-color: #3c4043; color: #e8eaed; }
  button { background-color: #4285f4; color: white; cursor: pointer; font-weight: bold; }
  a { display: block; padding: 10px; background-color: #3c4043; border-radius: 5px; color: #8ab4f8; text-decoration: none; margin: 5px 0; }
  hr { border: 0; border-top: 1px solid #5f6368; margin: 20px 0; }
  .footer { margin-top: 15px; color: #9aa0a6; font-size: 0.9em; }
</style>
</head>
<body>
<h2>Configuração SoldaPonto</h2>
<div class="container">
  <form action="/save" method="POST">
    <p><input type="text" name="ssid" placeholder="Nome da rede Wi-Fi" required></p>
    <p><input type="password" name="pass" placeholder="Senha da rede"></p>
    <p><button type="submit">Salvar e Reiniciar</button></p>
  </form>
  <hr>
  <a href="/update">Atualização OTA</a>
  <a href="/resetwifi">Resetar Wi-Fi</a>
  <a href="/reseteeprom">Resetar Configurações de Fábrica</a>
  <hr>
  <p><b>Dispositivo:</b> SoldaPonto v)rawliteral";
    page += FIRMWARE_VERSION;
    page += R"rawliteral(</p>
  <p><b>Modo:</b> Configuração (AP)</p>
  <p><b>IP do dispositivo:</b> )rawliteral";
    page += ip;
    page += R"rawliteral(</p>
  <div class="footer">Desenvolvido por Jorge Souza</div>
</div>
</body>
</html>
)rawliteral";

    server.send(200, "text/html", page);
  });

  server.on("/save", HTTP_POST, []() {
    if (server.hasArg("ssid")) {
      String s = server.arg("ssid");
      String p = server.hasArg("pass") ? server.arg("pass") : "";
      memset(&wifiConfig, 0, sizeof(WifiConfig));
      s.toCharArray(wifiConfig.ssid, sizeof(wifiConfig.ssid));
      p.toCharArray(wifiConfig.pass, sizeof(wifiConfig.pass));
      saveWifiConfig();
      server.send(200, "text/plain", "Salvo. Reiniciando...");
      delay(1000);
      ESP.restart();
    } else {
      server.send(400, "text/plain", "Falha nos parametros");
    }
  });

  server.on("/resetwifi", HTTP_GET, []() {
    Serial.println("Resetando Wi-Fi...");
    memset(&wifiConfig, 0, sizeof(WifiConfig));
    saveWifiConfig(true);
    server.send(200, "text/plain", "Wi-Fi resetado. Reiniciando...");
    delay(1000);
    ESP.restart();
  });

  server.on("/reseteeprom", HTTP_GET, []() {
    Serial.println("Resetando EEPROM...");
    EEPROM.begin(EEPROM_SIZE);
    for (int i = 0; i < EEPROM_SIZE; i++) EEPROM.write(i, 0xFF);
    EEPROM.commit();
    EEPROM.end();
    server.send(200, "text/plain", "EEPROM apagada. Reiniciando...");
    delay(1000);
    ESP.restart();
  });

  server.begin();
}

void checaBotaoEncoder() {
  bool buttonState = digitalRead(pin_Encoder_SW);

  if (buttonState == LOW) {
    if (!buttonHeld) {
      buttonPressStart = millis();
      buttonHeld = true;
    } else if (millis() - buttonPressStart >= longPressDuration) {
      Serial.println("Botao pressionado por 5s. Verificando OTA...");
      checkForUpdate();
      buttonHeld = false;
    }
  } else {
    if (buttonHeld && millis() - buttonPressStart < longPressDuration) {
      if (menuState == NORMAL) {
        menuState = EDITANDO_CICLO;
        Serial.println("Editando ciclo.");
      } else if (menuState == EDITANDO_CICLO) {
        menuState = EDITANDO_TEMPO;
        Serial.println("Editando tempo.");
      } else {
        menuState = NORMAL;
        Serial.println("Modo normal. Salvando configuracoes.");
        saveSettings();
      }
    }
    buttonHeld = false;
  }
}

void atualizaValoresEncoder() {
  if (encoder == nullptr) return;

  encoder->tick();
  int newPos = encoder->getPosition();
  if (newPos == lastEncoderPos) return;

  int delta = newPos - lastEncoderPos;

  if (menuState == EDITANDO_CICLO) {
    int novo = (int)cycle_ms + delta;
    if (novo < min_Cycle_ms) novo = min_Cycle_ms;
    if (novo > max_Cycle_ms) novo = max_Cycle_ms;
    cycle_ms = novo;
  } else if (menuState == EDITANDO_TEMPO) {
    int novo = (int)time_ms + delta;
    if (novo < min_Time_ms) novo = min_Time_ms;
    if (novo > max_Time_ms) novo = max_Time_ms;
    time_ms = novo;
  }

  lastEncoderPos = newPos;
}

void atualizaBlink() {
  if (menuState == EDITANDO_CICLO || menuState == EDITANDO_TEMPO) {
    if (millis() - lastBlink > 350) {
      showValue = !showValue;
      lastBlink = millis();
    }
  } else {
    showValue = true;
  }
}

void splashScreen() {
  display.fillScreen(OLED_Color_Black);
  display.setTextColor(OLED_Color_White);
  display.setTextSize(2);

  int16_t x1, y1;
  uint16_t w, h;

  display.getTextBounds("Solda", 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 10);
  display.print("Solda");

  display.getTextBounds("Ponto", 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 35);
  display.print("Ponto");

  display.setTextSize(1);
  String versao = "Versao: ";
  versao += FIRMWARE_VERSION;
  display.getTextBounds(versao, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 55);
  display.print(versao);
  delay(2000);
}

String getWifiStatus() {
  return (WiFi.status() == WL_CONNECTED) ? "WiFi OK" : "Sem WiFi";
}

void checkEEPROM() {
  EEPROM.begin(EEPROM_SIZE);

  if (EEPROM.read(EEPROM_MAGIC_ADDR) != EEPROM_MAGIC_VALUE) {
    Serial.println("EEPROM invalida ou limpa. Aplicando padrao.");

    memset(&wifiConfig, 0, sizeof(WifiConfig));
    cycle_ms = 20;
    time_ms = 20;
    soldaCount = 0;

    EEPROM.put(EEPROM_WIFI_CONFIG_START, wifiConfig);
    EEPROM.put(EEPROM_CYCLE_MS_ADDR, cycle_ms);
    EEPROM.put(EEPROM_TIME_MS_ADDR, time_ms);
    EEPROM.put(EEPROM_COUNTER_ADDR, soldaCount);
    EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);
    EEPROM.commit();

    Serial.println("EEPROM inicializada com sucesso.");
  } else {
    Serial.println("EEPROM valida.");
  }

  EEPROM.end();
}

void loop() {
  watchdogCheck();

  if (configMode) {
    dnsServer.processNextRequest();
    server.handleClient();
  }

  monitorWifi();
  trigger();
  checaBotaoEncoder();
  atualizaValoresEncoder();
  atualizaBlink();
  screenOne();
  watchdogReset();
}

static inline uint32_t roundToHalfCycles(uint32_t us) {
  uint32_t n = (us + (HALF_CYCLE_US / 2)) / HALF_CYCLE_US;
  if (n < 1) n = 1;
  return n * HALF_CYCLE_US;
}

void delayMicrosFeed(uint32_t us) {
  uint32_t start = micros();
  while ((micros() - start) < us) {
    yield();
    watchdogReset();
  }
}

void trigger() {
  static const uint16_t intervalo_ms = 40;

  if (digitalRead(pin_Trigger)) {
    aux2 = 1;
  }

  if (!digitalRead(pin_Trigger) && aux2 == 1) {
    uint32_t pulso_us = roundToHalfCycles((uint32_t)time_ms * 1000UL);
    const int barX = 10, barY = 50, barW = 76, barH = 10;

    display.drawRect(barX - 1, barY - 1, barW + 2, barH + 2, OLED_Color_White);

    for (uint16_t p = 0; p < cycle_ms; ++p) {
      digitalWrite(pin_Triac, HIGH);
      Serial.printf("Pulso %u/%u ON\n", p + 1, cycle_ms);

      uint32_t start = micros();
      while ((micros() - start) < pulso_us) {
        float progress = (float)(micros() - start) / (float)pulso_us;
        if (progress > 1.0f) progress = 1.0f;
        int filled = (int)(progress * barW);
        display.fillRect(barX, barY, filled, barH, OLED_Color_Green);
        display.fillRect(barX + filled, barY, barW - filled, barH, OLED_Background_Color);
        yield();
        watchdogReset();
      }

      digitalWrite(pin_Triac, LOW);
      Serial.println("Pulso OFF");
      display.fillRect(barX, barY, barW, barH, OLED_Background_Color);

      soldaCount++;
      saveCounter();

      if (p + 1 < cycle_ms) {
        delayMicrosFeed((uint32_t)intervalo_ms * 1000UL);
      }
    }

    aux2 = 0;
    delay(200);
  }
}

void screenOne() {
  String wifiStatus = (WiFi.status() == WL_CONNECTED) ? "[WiFi]" : "[ X ]";

  static int pos = 0;
  if (encoder != nullptr) {
    encoder->tick();
    int newPos = encoder->getPosition();
    if (pos != newPos) {
      Serial.print("pos:");
      Serial.print(newPos);
      Serial.print(" dir:");
      Serial.println((int)(encoder->getDirection()));
      pos = newPos;
    }
  }

  static bool desenhouTitulos = false;
  if (!desenhouTitulos) {
    display.fillScreen(OLED_Background_Color);
    display.setTextColor(OLED_Text_Color);
    display.setTextSize(1);
    display.setCursor(1, 1);
    display.print("Ciclo:");
    display.setCursor(80, 1);
    display.print("cl");
    display.setCursor(1, 25);
    display.print("Tempo:");
    display.setCursor(80, 25);
    display.print("ms");
    display.setCursor(40, 55);
    display.print(getWifiStatus());
    desenhouTitulos = true;
  }

  if (last_cycle_ms != cycle_ms || (menuState == EDITANDO_CICLO && !showValue)) {
    display.fillRect(35, 1, 45, 16, OLED_Background_Color);
    last_cycle_ms = cycle_ms;
  }

  if (last_time_ms != time_ms || (menuState == EDITANDO_TEMPO && !showValue)) {
    display.fillRect(35, 25, 45, 16, OLED_Background_Color);
    last_time_ms = time_ms;
  }

  if (!(menuState == EDITANDO_CICLO && !showValue)) {
    if (cycle_ms <= 9) display.setCursor(67, 1);
    else if (cycle_ms <= 99) display.setCursor(55, 1);
    else display.setCursor(43, 1);

    display.setTextSize(2);
    display.setTextColor(OLED_Text_Color);
    display.print(cycle_ms);
  }

  if (!(menuState == EDITANDO_TEMPO && !showValue)) {
    if (time_ms <= 9) display.setCursor(67, 25);
    else if (time_ms <= 99) display.setCursor(55, 25);
    else display.setCursor(43, 25);

    display.setTextSize(2);
    display.setTextColor(OLED_Text_Color);
    display.print(time_ms);
  }

  if (last_soldaCount != soldaCount || wifiStatus != last_wifi_status) {
    display.fillRect(0, 50, SCREEN_WIDTH, 14, OLED_Background_Color);
    display.setTextSize(1);
    display.setCursor(1, 55);
    display.print(wifiStatus);
    display.setCursor(60, 55);
    display.print("S:");
    display.print(soldaCount);
    last_soldaCount = soldaCount;
    last_wifi_status = wifiStatus;
  }
}
