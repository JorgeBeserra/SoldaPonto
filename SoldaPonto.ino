/*********************************************
  Autor: Marlon Nardi Walendorff
  Projeto: Solda ponto para baterias de lítio com Arduino
  Detalhes do projeto:https://marlonnardi.com/2023/12/03/como-fazer-solda-ponto-profissional-para-baterias-18650-e-mais-construa-sua-propria-bicicleta-eletrica-1/
**********************************************/

/*
  Atualizaçao: Jorge Souza
*/

//============================================================
// ✅ Bibliotecas
//============================================================

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1331.h>
#include <SPI.h>
#include <RotaryEncoder.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <EEPROM.h>
#include <ESP8266httpUpdate.h>
#include <DNSServer.h>

//============================================================
// ⚙️ Mapeamento de Hardware e Parâmetros
//============================================================
#define FIRMWARE_VERSION "1.0.3"

// 🟦 Pinos
#define pin_Trigger 12 // D6
#define pin_Triac 3 // RX

#define pin_Encoder_CLK     0  // D3
#define pin_Encoder_DT      2  // TX
#define pin_Encoder_SW     16  // D0

const uint8_t   OLED_pin_scl_sck        = 13; // D7
const uint8_t   OLED_pin_sda_mosi       = 14; // D5
const uint8_t   OLED_pin_cs_ss          = 15; // D8
const uint8_t   OLED_pin_res_rst        = 5;  // D1
const uint8_t   OLED_pin_dc_rs          = 4;  // D2

// 🔧 Parâmetros de tempo
#define min_Time_ms 3
#define max_Time_ms 120
#define min_Cycle_ms 3
#define max_Cycle_ms 120

// 🧠 EEPROM
#define EEPROM_MAGIC_ADDR 500  // Posição na EEPROM
#define EEPROM_MAGIC_VALUE 0x42 // Valor fixo para validar dados
#define EEPROM_WIFI_CONFIG_START 0
#define EEPROM_CYCLE_MS_ADDR 100
#define EEPROM_TIME_MS_ADDR 102
#define EEPROM_COUNTER_ADDR 104

// 🖥️ Display
#define SCREEN_WIDTH 96 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// 🌐 DNS
const byte DNS_PORT = 53;

//============================================================
// 🎨 Cores OLED (RGB565)
//============================================================
const uint16_t  OLED_Color_Black        = 0x0000;
const uint16_t  OLED_Color_Blue         = 0x001F;
const uint16_t  OLED_Color_Red          = 0xF800;
const uint16_t  OLED_Color_Green        = 0x07E0;
const uint16_t  OLED_Color_Cyan         = 0x07FF;
const uint16_t  OLED_Color_Magenta      = 0xF81F;
const uint16_t  OLED_Color_Yellow       = 0xFFE0;
const uint16_t  OLED_Color_White        = 0xFFFF;

// The colors we actually want to use
uint16_t        OLED_Text_Color         = OLED_Color_Black;
uint16_t        OLED_Background_Color    = OLED_Color_Blue;

//============================================================
// 🔧 Objetos
//============================================================
Adafruit_SSD1331 display =  Adafruit_SSD1331(OLED_pin_cs_ss, OLED_pin_dc_rs, OLED_pin_sda_mosi, OLED_pin_scl_sck, OLED_pin_res_rst);
RotaryEncoder EncoderOne(pin_Encoder_CLK, pin_Encoder_DT);
RotaryEncoder *encoder = nullptr;

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
DNSServer dnsServer;

//============================================================
// 🔗 Wi-Fi
//============================================================
struct WifiConfig {
  char ssid[32];
  char pass[64];
};

WifiConfig wifiConfig;
bool configMode = false;

//============================================================
// 🔧 Estados
//============================================================
enum MenuState { NORMAL, EDITANDO_CICLO, EDITANDO_TEMPO };
MenuState menuState = NORMAL;

//============================================================
// 🔧 Variáveis Globais
//============================================================
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
static String last_wifi_status = "";

int16_t valorEncoder = 0;
const unsigned long WATCHDOG_TIMEOUT = 15000; // 15 segundos
volatile bool encoderFlag = false;

bool showValue = true;
bool lastButtonState = HIGH;
unsigned long lastButtonChange = 0;
const unsigned long debounceDelay = 150;


const char* githubAPI = "https://api.github.com/repos/JorgeBeserra/SoldaPonto/releases/latest";

byte aux2 = 0;

//============================================================
// 🔥 Protótipos de Funções
//============================================================

// 🧠 EEPROM
void checkEEPROM();
void loadWifiConfig();
void saveWifiConfig();
void saveSettings();
void loadSettings();
void saveCounter();
void loadCounter();

// 🌐 Wi-Fi
void connectWifi();
void startConfigPortal();
void monitorWifi();

// 🚀 OTA
void checkForUpdate();
void showOtaMessage(String msg);

// 🕹️ Encoder
void checkEncoderButton();
void atualizaValoresEncoder();
void atualizaBlink();

// 🔥 Watchdog
void watchdogCheck();
void watchdogReset();

// 🖥️ Display
void splashScreen();
void screenOne();
String getWifiStatus();

// 🛠️ Trigger
void trigger();

//============================================================
// 🚦 Interrupções
//============================================================
IRAM_ATTR void checkPosition()
{
  encoder->tick();
}

//============================================================
// 🚀 Setup
//============================================================
void setup()
{
  Serial.begin(74880);
  lastWatchdogReset = millis();
  Serial.println("🖥️ Display Iniciando...");
  display.begin();
  display.setFont();
  splashScreen();
  

  checkEEPROM();
  encoder = new RotaryEncoder(pin_Encoder_CLK, pin_Encoder_DT, RotaryEncoder::LatchMode::TWO03);
  //Configura pino como saída
  digitalWrite(pin_Triac, LOW);
  pinMode(pin_Triac, OUTPUT);
  //Configura pino como entrada PULL-UP
  pinMode(pin_Encoder_SW, INPUT_PULLUP);
  //Configura pino como entrada PULL-UP
  pinMode(pin_Trigger, INPUT_PULLUP);

  pinMode(pin_Encoder_CLK, INPUT_PULLUP);
  pinMode(pin_Encoder_DT, INPUT_PULLUP);

  loadWifiConfig();
  if (digitalRead(pin_Encoder_SW) == LOW) {
    startConfigPortal();
  } else {
    connectWifi();
  }

  loadSettings();
  loadCounter();

  //================= Interrupção Externa ========================//
  /* Vincula duas interrupções externas no pino 2 e 3 nas funções ISR0 e ISR1
     para garantir que o encoder sempre seja lido com prioridade.*/
  attachInterrupt(digitalPinToInterrupt(0), checkPosition, CHANGE);
  attachInterrupt(digitalPinToInterrupt(2), checkPosition, CHANGE);

  EncoderOne.setPosition(1);
  
  display.fillScreen(OLED_Background_Color);
}//endSetup --------------------------------------


bool isNewerVersion(String current, String latest) {
  current.replace(".", "");
  latest.replace(".", "");
  int currentNum = current.toInt();
  int latestNum = latest.toInt();
  return latestNum > currentNum;
}

void watchdogCheck() {
  if (millis() - lastWatchdogReset > WATCHDOG_TIMEOUT) {
    Serial.println("🚨 Watchdog ativado! Reiniciando o ESP...");
    ESP.restart();
  }
}

void watchdogReset() {
  lastWatchdogReset = millis();
}

void showOtaMessage(String msg) {
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
    Serial.println("❌ Sem conexão Wi-Fi. Cancelando verificação.");
    showOtaMessage("Sem Wi-Fi");
    delay(2000);
    return;
  }
  showOtaMessage("== Verificando ==");
  Serial.println("=======================================");
  Serial.println("🚀 Verificação de Atualização OTA");
  Serial.println("---------------------------------------");
  Serial.printf("🔧 Firmware atual: v%s\n", FIRMWARE_VERSION);
  Serial.println("🌐 Consultando GitHub...");
  // Testa DNS
  Serial.print("🔍 Testando DNS para api.github.com...");
  IPAddress githubIP;
  if (!WiFi.hostByName("api.github.com", githubIP)) {
    Serial.println("❌ Falha na resolução DNS.");
    return;
  }
  Serial.print("✔️ Resolvido IP: ");
  Serial.println(githubIP);

  // Conexão HTTPS
  WiFiClientSecure client;
  client.setInsecure(); // Ignora SSL

  HTTPClient https;
  const char* url = "https://api.github.com/repos/JorgeBeserra/SoldaPonto/releases/latest";

  Serial.println("🔗 Acessando GitHub API...");
  https.begin(client, url);
  https.addHeader("User-Agent", "ESP8266"); // GitHub exige

  int httpCode = https.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = https.getString();

    // Extrair a tag_name (versão)
    int tagIndex = payload.indexOf("\"tag_name\":\"v");
    if (tagIndex < 0) {
      showOtaMessage("[X] Sem versao");
      Serial.println("❌ Não encontrou a versão no JSON.");
      delay(2000);
      https.end();
      return;
    }
    int start = tagIndex + 13;
    int end = payload.indexOf("\"", start);
    String latestVersion = payload.substring(start, end);

    Serial.printf("✅ Última versão no GitHub: %s\n", latestVersion.c_str());
    Serial.printf("🔧 Minha versão atual: %s\n", FIRMWARE_VERSION);

    // Extrair browser_download_url (link direto do binário)
    int urlIndex = payload.indexOf("\"browser_download_url\":\"");
    if (urlIndex < 0) {
      Serial.println("❌ Não encontrou o link do binário no JSON.");
      showOtaMessage("[X] Sem binario");
      delay(2000);
      https.end();
      return;
    }
    int urlStart = urlIndex + strlen("\"browser_download_url\":\"");
    int urlEnd = payload.indexOf("\"", urlStart);
    String binUrl = payload.substring(urlStart, urlEnd);
    binUrl.trim();

    Serial.println("🗂️ Link do binário encontrado:");
    Serial.println(binUrl);

    https.end(); // Fecha a conexão HTTP
    Serial.println("🧠 Comparando versões...");
    if (isNewerVersion(FIRMWARE_VERSION, latestVersion)) {
      showOtaMessage("[...] Atualizando");
      Serial.println("🚀 Nova versão encontrada! Iniciando atualização OTA...");

      // 🔥 LINHAS FUNDAMENTAIS PARA SUPORTE A REDIRECT:
      ESPhttpUpdate.rebootOnUpdate(true);
      ESPhttpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

      t_httpUpdate_return result = ESPhttpUpdate.update(client, binUrl);

      switch (result) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("❌ OTA falhou. Erro (%d): %s\n",
                        ESPhttpUpdate.getLastError(),
                        ESPhttpUpdate.getLastErrorString().c_str());
          showOtaMessage("[X] Erro OTA");
          delay(3000);
          break;
        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("⚠️ Nenhuma atualização disponível.");
          showOtaMessage("- Sem atualizacao -");
          delay(2000);
          break;

        case HTTP_UPDATE_OK:
          Serial.println("✅ Atualização concluída. Reiniciando...");
          showOtaMessage("[OK] Atualizado!");
          delay(2000);
          break;
      }
    } else {
      Serial.println("👍 Firmware já está na última versão.");
      showOtaMessage("- Ja atual. -");
      delay(2000);
    }

  } else {
    Serial.printf("❌ Falha na conexão. HTTP Code: %d\n", httpCode);
    showOtaMessage("[X] HTTP FAIL");
    delay(2000);
    https.end();
  }
  Serial.println("=======================================");
}

void loadWifiConfig() {
  EEPROM.begin(512);
  EEPROM.get(EEPROM_WIFI_CONFIG_START, wifiConfig);

  if (wifiConfig.ssid[0] == 0xFF || wifiConfig.ssid[0] == '\0') {
    Serial.println("🚨 EEPROM inválida ou limpa. Aplicando valores padrão.");
    memset(&wifiConfig, 0, sizeof(WifiConfig)); // Zera a estrutura
    return;
  }

  Serial.println("✅ Wi-Fi carregado da EEPROM:");
  Serial.printf("🔸 SSID: %s\n", wifiConfig.ssid);
  Serial.printf("🔸 Senha: %s\n", wifiConfig.pass);
}


void saveWifiConfig() {

  if (strlen(wifiConfig.ssid) == 0 || strlen(wifiConfig.pass) == 0) {
    Serial.println("⚠️ Dados de Wi-Fi inválidos. Não salvando na EEPROM.");
    return;
  }

  EEPROM.begin(512);
  EEPROM.put(EEPROM_WIFI_CONFIG_START, wifiConfig);
  EEPROM.commit();
  Serial.println("💾 Configurações Wi-Fi salvas na EEPROM:");
  Serial.print("🔸 SSID: ");
  Serial.println(wifiConfig.ssid);
  Serial.print("🔸 Senha: ");
  Serial.println(wifiConfig.pass);
}

void saveSettings() {
  EEPROM.begin(512);
  EEPROM.put(EEPROM_CYCLE_MS_ADDR, cycle_ms);
  EEPROM.put(EEPROM_TIME_MS_ADDR, time_ms);
  EEPROM.commit();
  EEPROM.end();
}

void loadSettings() {
  EEPROM.begin(512);
  EEPROM.get(EEPROM_CYCLE_MS_ADDR, cycle_ms);
  EEPROM.get(EEPROM_TIME_MS_ADDR, time_ms);
  EEPROM.end();

  // Se os valores forem inválidos, aplica padrão seguro
  if (cycle_ms < min_Cycle_ms || cycle_ms > max_Cycle_ms) cycle_ms = 30;
  if (time_ms < min_Time_ms || time_ms > max_Time_ms) time_ms = 30;
}

void saveCounter() {
  EEPROM.begin(512);
  EEPROM.put(EEPROM_COUNTER_ADDR, soldaCount);
  EEPROM.commit();
  EEPROM.end();
}

void loadCounter() {
  EEPROM.begin(512);
  EEPROM.get(EEPROM_COUNTER_ADDR, soldaCount);
  EEPROM.end();
}

void connectWifi() {
  if (wifiConfig.ssid[0] == 0 || wifiConfig.ssid[0] == 0xFF || wifiConfig.ssid[0] == '\0') {
    Serial.println("❌ Nenhuma rede configurada.");
    return;
  }

  Serial.printf("🔗 Conectando no Wi-Fi SSID: %s...\n", wifiConfig.ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiConfig.ssid, wifiConfig.pass);

  unsigned long startAttemptTime = millis();
  const unsigned long wifiTimeout = 15000; // 15 segundos

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < wifiTimeout) {
    Serial.print(".");
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Wi-Fi conectado!");
    Serial.print("📡 IP: ");
    Serial.println(WiFi.localIP());
    checkForUpdate();
  } else {
    Serial.println("\n❌ Falha ao conectar no Wi-Fi.");
    Serial.println("⚠️ Mantendo operação offline.");
  }
}


void monitorWifi() {
  static unsigned long lastCheck = 0;

  if (millis() - lastCheck > 10000) {  // A cada 10 segundos
    lastCheck = millis();

    // 👉 Verifica se há rede configurada ANTES de tentar
    if ((wifiConfig.ssid[0] == 0) || (wifiConfig.ssid[0] == 0xFF) || (wifiConfig.ssid[0] == '\0')) {
      Serial.println("❌ Nenhuma rede configurada. Não tentando reconectar.");
      return;
    }

    if (WiFi.status() != WL_CONNECTED && !configMode) {
      Serial.println("⚠️ Wi-Fi desconectado. Tentando reconectar...");

      WiFi.disconnect();
      WiFi.begin(wifiConfig.ssid, wifiConfig.pass);

      unsigned long startReconnect = millis();
      const unsigned long reconnectTimeout = 10000;

      while (WiFi.status() != WL_CONNECTED && millis() - startReconnect < reconnectTimeout) {
        Serial.print(".");
        delay(500);
      }

      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✅ Reconectado ao Wi-Fi!");
        Serial.print("📡 IP: ");
        Serial.println(WiFi.localIP());
      } else {
        Serial.println("\n❌ Falha ao reconectar. Operando offline.");
      }
    }
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
  .container { background-color: #303134; padding: 20px; border-radius: 10px; display: inline-block; max-width: 400px; width: 90%; box-shadow: 0px 0px 10px rgba(0,0,0,0.7); }
  input, button { padding: 10px; margin: 8px 0; border-radius: 5px; border: none; width: 100%; box-sizing: border-box; }
  input { background-color: #3c4043; color: #e8eaed; }
  input::placeholder { color: #9aa0a6; }
  button { background-color: #4285f4; color: white; cursor: pointer; font-weight: bold; }
  button:hover { background-color: #3367d6; }
  hr { border: 0; border-top: 1px solid #5f6368; margin: 20px 0; }
  a { display: block; padding: 10px; background-color: #3c4043; border-radius: 5px; color: #8ab4f8; text-decoration: none; margin: 5px 0; transition: background-color 0.3s; }
  a:hover { background-color: #5f6368; }
  .footer { margin-top: 15px; color: #9aa0a6; font-size: 0.9em; }
</style>
</head>
<body>

<h2>🔧 Configuração SoldaPonto</h2>

<div class="container">
  <form action="/save" method="POST">
    <p><input type="text" name="ssid" placeholder="🔌 Nome da rede Wi-Fi" required></p>
    <p><input type="password" name="pass" placeholder="🔒 Senha da rede" required></p>
    <p><button type="submit">💾 Salvar e Reiniciar</button></p>
  </form>

  <hr>

  <a href="/update">🚀 Atualização OTA</a>
  <a href="/resetwifi">❌ Resetar Wi-Fi</a>
  <a href="/reseteeprom">🗑️ Resetar Configurações de Fábrica</a>

  <hr>

  <p><b>Dispositivo:</b> SoldaPonto v)rawliteral";
page += FIRMWARE_VERSION;
page += R"rawliteral(</p>
  <p><b>Modo:</b> Configuração (AP)</p>
  <p><b>IP do dispositivo:</b> )rawliteral";
page += ip;
page += R"rawliteral(</p>

  <div class="footer">
    🔧 Desenvolvido por Jorge Souza
  </div>
</div>

</body>
</html>
)rawliteral";


  server.send(200, "text/html", page);
});

  server.on("/save", HTTP_POST, []() {
    if (server.hasArg("ssid") && server.hasArg("pass")) {
      String s = server.arg("ssid");
      String p = server.arg("pass");
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
  Serial.println("⚠️ Resetando Wi-Fi...");
  memset(&wifiConfig, 0, sizeof(WifiConfig));
  saveWifiConfig();
  server.send(200, "text/plain", "Wi-Fi resetado. Reiniciando...");
  delay(1000);
  ESP.restart();
});

server.on("/reseteeprom", HTTP_GET, []() {
  Serial.println("⚠️ Resetando EEPROM...");
  EEPROM.begin(512);
  for (int i = 0; i < 512; i++) EEPROM.write(i, 0xFF);
  EEPROM.commit();
  server.send(200, "text/plain", "EEPROM apagada. Reiniciando...");
  delay(1000);
  ESP.restart();
});

  server.begin();
}

const unsigned long longPressDuration = 5000; // 5 segundos

void checaBotaoEncoder() {
  bool buttonState = digitalRead(pin_Encoder_SW);

  if (buttonState == LOW) { // Botão pressionado
    if (!buttonHeld) {
      buttonPressStart = millis();
      buttonHeld = true;
    } else {
      if (millis() - buttonPressStart >= longPressDuration) {
        Serial.println("🟢 Botão pressionado por 5s -> Verificando atualização OTA...");
        checkForUpdate();
        buttonHeld = false;  // Reseta após a atualização
      }
    }
  } else { // Botão solto
    if (buttonHeld) {
      if (millis() - buttonPressStart < longPressDuration) {
        // Pressão curta -> troca entre os modos de edição
        if (menuState == NORMAL) {
          menuState = EDITANDO_CICLO;
        } else if (menuState == EDITANDO_CICLO) {
          menuState = EDITANDO_TEMPO;
        } else if (menuState == EDITANDO_TEMPO) {
          menuState = NORMAL;
          saveSettings();
        }
      }
      buttonHeld = false; // Reseta estado do botão
    }
  }
}



void atualizaValoresEncoder() {
  encoder->tick();
  int newPos = encoder->getPosition();
  if (newPos != lastEncoderPos) {
    int delta = newPos - lastEncoderPos;
    if (menuState == EDITANDO_CICLO) {
      cycle_ms += delta;
      if (cycle_ms < min_Cycle_ms) cycle_ms = min_Cycle_ms;
      if (cycle_ms > max_Cycle_ms) cycle_ms = max_Cycle_ms;
    } else if (menuState == EDITANDO_TEMPO) {
      time_ms += delta;
      if (time_ms < min_Time_ms) time_ms = min_Time_ms;
      if (time_ms > max_Time_ms) time_ms = max_Time_ms;
    }
    lastEncoderPos = newPos;
  }
}


void atualizaBlink() {
  // Pisca só em modo edição
  if (menuState == EDITANDO_CICLO || menuState == EDITANDO_TEMPO) {
    if (millis() - lastBlink > 350) {
      showValue = !showValue;
      lastBlink = millis();
    }
  } else {
    showValue = true; // Sempre mostra tudo no modo normal
  }
}

void splashScreen() {
  display.fillScreen(OLED_Color_Black);
  display.setTextColor(OLED_Color_White);

  // Texto principal
  display.setTextSize(2);

  int16_t x1, y1;
  uint16_t w, h;

  display.getTextBounds("Solda", 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 10);
  display.print("Solda");

  display.getTextBounds("Ponto", 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 35);
  display.print("Ponto");

  // Versão
  display.setTextSize(1);
  String versao = "Versao: ";
  versao += FIRMWARE_VERSION;

  display.getTextBounds(versao, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 55);
  display.print(versao);

  delay(2000);
}

String getWifiStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    return "WiFi OK";
  } else {
    return "Sem WiFi";
  }
}

void checkEEPROM() {
  EEPROM.begin(512);
  if (EEPROM.read(EEPROM_MAGIC_ADDR) != EEPROM_MAGIC_VALUE) {
    Serial.println("🚨 EEPROM inválida ou limpa. Aplicando valores padrão.");

    // Resetando WiFi
    memset(&wifiConfig, 0, sizeof(WifiConfig));

    // Resetando configurações de ciclo e tempo
    cycle_ms = 30;
    time_ms = 30;

    // Resetando contador de soldas
    soldaCount = 0;

    // Salvando tudo na EEPROM
    saveWifiConfig();
    saveSettings();
    saveCounter();

    // Escrevendo o magic byte
    EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);
    EEPROM.commit();
    Serial.println("✅ EEPROM inicializada com sucesso.");
  } else {
    Serial.println("✔️ EEPROM válida.");
  }
  EEPROM.end();
}




void loop()
{
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
}//end_void_loop ----------------------



void trigger()
{

  if (digitalRead(pin_Trigger) ) //Se o botão está solto
  {
    aux2 = 1;
  }

  if (!digitalRead(pin_Trigger) && aux2 == 1) //Se o botão está pressionado
  {


    digitalWrite(pin_Triac, HIGH);
    Serial.println("Ativado");

    // Barra de progresso
    const int barX = 10;
    const int barY = 50;
    const int barWidth = 76;
    const int barHeight = 10;

    display.drawRect(barX - 1, barY - 1, barWidth + 2, barHeight + 2, OLED_Color_White);
    unsigned long start = millis();

    while (millis() - start < time_ms) {
      float progress = (float)(millis() - start) / (float)time_ms;
      if (progress > 1.0) progress = 1.0;
      int filled = progress * barWidth;

      display.fillRect(barX, barY, filled, barHeight, OLED_Color_Green);
      display.fillRect(barX + filled, barY, barWidth - filled, barHeight, OLED_Background_Color);

      delay(10);
    }

    digitalWrite(pin_Triac, LOW);
    Serial.println("Desativado");

    display.fillRect(barX - 1, barY - 1, barWidth + 2, barHeight + 2, OLED_Background_Color);
    soldaCount++;
    saveCounter();
    aux2 = 0;
    delay(500);
  }


}//----------------------- end_selecionaTela



void screenOne()
{
  
  String wifiStatus = (WiFi.status() == WL_CONNECTED) ? "[WiFi]" : "[ X ]";
  
  if (!digitalRead(pin_Encoder_SW) ) //Se o botão está solto
  {
    Serial.println("Rotary pressionado");
  }

  static int pos = 0;

  encoder->tick(); // just call tick() to check the state.

  int newPos = encoder->getPosition();

  //valorEncoder = EncoderOne.getPosition();//Captura o valor do encoder
  if (pos != newPos) {
    Serial.print("pos:");
    Serial.print(newPos);
    Serial.print(" dir:");
    Serial.println((int)(encoder->getDirection()));
    pos = newPos;
  } // if

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

  // Limpa só a área dos valores
  if(last_cycle_ms != cycle_ms){
    display.fillRect(35, 1, 45, 16, OLED_Background_Color);
    last_cycle_ms = cycle_ms;
  }

  if(last_time_ms != time_ms){
    display.fillRect(35, 25, 45, 16, OLED_Background_Color);
    last_time_ms = time_ms;
  }

    // Exibe ciclo (piscando só se editando)
  if (!(menuState == EDITANDO_CICLO && !showValue)) {
    
    if (cycle_ms <= 9) display.setCursor(67, 1);
    else if (cycle_ms >= 10 && cycle_ms <= 99) display.setCursor(55, 1);
    else if (cycle_ms >= 100) display.setCursor(43, 1);

    display.setTextSize(2);
    display.setTextColor(OLED_Text_Color);
    display.print(cycle_ms);
  }

  // Exibe tempo (piscando só se editando)
  if (!(menuState == EDITANDO_TEMPO && !showValue)) {

    if (time_ms <= 9) display.setCursor(67, 25);
    else if (time_ms >= 10 && time_ms <= 99) display.setCursor(55, 25);
    else if (time_ms >= 100) display.setCursor(43, 25);
    
    display.setTextSize(2);
    display.setTextColor(OLED_Text_Color);
    display.print(time_ms);
  }

   if (last_soldaCount != soldaCount || wifiStatus != last_wifi_status) {
    // Limpa área inferior
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

}//end_screenOne ----------------------