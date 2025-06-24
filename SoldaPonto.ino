/*********************************************
  Autor: Marlon Nardi Walendorff
  Projeto: Solda ponto para baterias de lítio com Arduino
  Detalhes do projeto:https://marlonnardi.com/2023/12/03/como-fazer-solda-ponto-profissional-para-baterias-18650-e-mais-construa-sua-propria-bicicleta-eletrica-1/
**********************************************/

/*
  Atualizaçao: Jorge Souza
*/

//==================== Inclusão de Bibliotecas =================//
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1331.h>
#include <SPI.h>
#include <RotaryEncoder.h>
//--- WiFi e atualização OTA ---//
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <EEPROM.h>
#include <ESP8266httpUpdate.h>

#define FIRMWARE_VERSION "1.0.0"

//==================== Mapeamento de Hardware ==================//

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

#define min_Time_ms 3
#define max_Time_ms 120

#define min_Cycle_ms 3
#define max_Cycle_ms 120



#define SCREEN_WIDTH 96 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define EEPROM_WIFI_CONFIG_START 0
#define EEPROM_CYCLE_MS_ADDR 100
#define EEPROM_TIME_MS_ADDR 102
#define EEPROM_COUNTER_ADDR 104

volatile bool encoderFlag = false;

// SSD1331 color definitions
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

enum MenuState { NORMAL, EDITANDO_CICLO, EDITANDO_TEMPO };
MenuState menuState = NORMAL;

unsigned long lastBlink = 0;
bool showValue = true;

bool lastButtonState = HIGH;
unsigned long lastButtonChange = 0;
const unsigned long debounceDelay = 150;





//==================== Instânciando Objetos ====================//
Adafruit_SSD1331 display =  Adafruit_SSD1331(
        OLED_pin_cs_ss,
        OLED_pin_dc_rs,
        OLED_pin_sda_mosi,
        OLED_pin_scl_sck,
        OLED_pin_res_rst
     );

RotaryEncoder EncoderOne(pin_Encoder_CLK, pin_Encoder_DT);

RotaryEncoder *encoder = nullptr;

//==================== Variáveis Globais ==================//
byte aux2 = 0;

int16_t valorEncoder = 0;
uint16_t time_ms = 0;
uint16_t last_time_ms = 0;
uint16_t cycle_ms = 0;
uint16_t last_cycle_ms = 0;
uint32_t soldaCount = 0;
uint32_t last_soldaCount = 0;
static String last_wifi_status = "";

//------- Configuração WiFi -------//
struct WifiConfig {
  char ssid[32];
  char pass[64];
};

WifiConfig wifiConfig;
bool configMode = false;

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

const char* firmwareUrl = "https://github.com/USERNAME/SoldaPonto/releases/latest/download/SoldaPonto.bin";

void checkForUpdate() {
  WiFiClient client;
  t_httpUpdate_return result = ESPhttpUpdate.update(client, firmwareUrl);
  switch (result) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("Update failed (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("No update available");
      break;
    case HTTP_UPDATE_OK:
      Serial.println("Update successful");
      break;
  }
}


void loadWifiConfig() {
  EEPROM.begin(sizeof(WifiConfig));
  EEPROM.get(EEPROM_WIFI_CONFIG_START, wifiConfig);
  if (wifiConfig.ssid[0] == 0xFF || wifiConfig.ssid[0] == '\0') {
    memset(&wifiConfig, 0, sizeof(WifiConfig));
  }
}

void saveWifiConfig() {
  EEPROM.put(EEPROM_WIFI_CONFIG_START, wifiConfig);
  EEPROM.commit();
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
  if (wifiConfig.ssid[0] == 0) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiConfig.ssid, wifiConfig.pass);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
  }
  if (WiFi.status() == WL_CONNECTED) {
    checkForUpdate();
  }
}

void startConfigPortal() {
  configMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("SoldaPontoConfig");
  httpUpdater.setup(&server, "/update");
  server.on("/", HTTP_GET, []() {
    String page = "<form method='POST' action='/save'>SSID:<input name='ssid'><br>Senha:<input name='pass' type='password'><br><input type='submit' value='Salvar'></form><p>OTA: <a href='/update'>/update</a></p>";
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
  server.begin();
}

//================== ISRs Interrupções Externas =======================//
/* Caso qualquer pino do encoder envie sinal, o metodo .tick() sempre será
  chamado, atualizando o valor do encoder via sua biblioteca. */

IRAM_ATTR void checkPosition()// Função ligada a uma interrupção ISR logo não pode retornar valor e deve ser mais rápida possível
{
  encoder->tick();
}//-------------------------endISR0

void checaBotaoEncoder() {
  bool buttonState = digitalRead(pin_Encoder_SW);
  if (buttonState == LOW && lastButtonState == HIGH && millis() - lastButtonChange > debounceDelay) {
    // Botão pressionado
    if (menuState == NORMAL) {
      menuState = EDITANDO_CICLO;
    } else if (menuState == EDITANDO_CICLO) {
      menuState = EDITANDO_TEMPO;
    } else if (menuState == EDITANDO_TEMPO){
      menuState = NORMAL;
      saveSettings();
    }
    lastButtonChange = millis();
  }
  lastButtonState = buttonState;
}

int lastEncoderPos = 0;

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

void setup()
{
  Serial.begin(74880);

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
  if (digitalRead(pin_Trigger) == LOW) {
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
  
  Serial.println("Display Iniciado...");
  display.begin();
  display.setFont();
  splashScreen();
  display.fillScreen(OLED_Background_Color);
}//endSetup --------------------------------------


void loop()
{
  if (configMode) {
    server.handleClient();
  }

  trigger();
  checaBotaoEncoder();
  atualizaValoresEncoder();
  atualizaBlink();
  screenOne();
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