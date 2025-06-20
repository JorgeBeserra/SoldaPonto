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

//==================== Mapeamento de Hardware ==================//
//#define pin_Encoder_CLK 3
//#define pin_Encoder_DT 2
//#define pin_Encoder_SW 4

//#define pin_Trigger 5
//#define pin_Triac 12

#define min_Power_ms 3
#define max_Power_ms 120


#define SCREEN_WIDTH 96 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// ----- Novos ----- //

#define pin_Encoder_CLK     12  // D6
#define pin_Encoder_DT      0   // D3
#define pin_Encoder_SW      16  // D0

//#define OLED_CLK    12  // D5
//#define OLED_MOSI   13  // D7
//#define OLED_RESET  16   // D0
//#define OLED_DC     4   // D2
//#define OLED_CS     5  // D1

const uint8_t   OLED_pin_scl_sck        = 13;
const uint8_t   OLED_pin_sda_mosi       = 14;
const uint8_t   OLED_pin_cs_ss          = 15; // D7
const uint8_t   OLED_pin_res_rst        = 5;  // D1
const uint8_t   OLED_pin_dc_rs          = 4;  // D2


#define pin_Trigger 2 // D4
#define pin_Triac 3 // RX

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
uint16_t        OLED_Backround_Color    = OLED_Color_Blue;

//==================== Instânciando Objetos ====================//
Adafruit_SSD1331 display =  Adafruit_SSD1331(
        OLED_pin_cs_ss,
        OLED_pin_dc_rs,
        OLED_pin_sda_mosi,
        OLED_pin_scl_sck,
        OLED_pin_res_rst
     );

RotaryEncoder EncoderOne(pin_Encoder_CLK, pin_Encoder_DT);

//==================== Variáveis Globais ==================//
byte aux2 = 0;

int16_t valorEncoder = 0;
uint16_t time_ms = 0;

//------- Configuração WiFi -------//
struct WifiConfig {
  char ssid[32];
  char pass[64];
};

WifiConfig wifiConfig;
bool configMode = false;

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;


void loadWifiConfig() {
  EEPROM.begin(sizeof(WifiConfig));
  EEPROM.get(0, wifiConfig);
  if (wifiConfig.ssid[0] == 0xFF || wifiConfig.ssid[0] == '\0') {
    memset(&wifiConfig, 0, sizeof(WifiConfig));
  }
}

void saveWifiConfig() {
  EEPROM.put(0, wifiConfig);
  EEPROM.commit();
}

void connectWifi() {
  if (wifiConfig.ssid[0] == 0) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiConfig.ssid, wifiConfig.pass);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
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


void setup()
{

  Serial.begin(74880);
  //Configura pino como saída
  digitalWrite(pin_Triac, LOW);
  pinMode(pin_Triac, OUTPUT);

  //Configura pino como entrada PULL-UP
  pinMode(pin_Encoder_SW, INPUT_PULLUP);
  //Configura pino como entrada PULL-UP
  pinMode(pin_Trigger, INPUT_PULLUP);

  loadWifiConfig();
  if (digitalRead(pin_Trigger) == LOW) {
    startConfigPortal();
  } else {
    connectWifi();
  }

  //================= Interrupção Externa ========================//
  /* Vincula duas interrupções externas no pino 2 e 3 nas funções ISR0 e ISR1
     para garantir que o encoder sempre seja lido com prioridade.*/
  //attachInterrupt(digitalPinToInterrupt(2), ISR0, CHANGE);
  //attachInterrupt(digitalPinToInterrupt(3), ISR1, CHANGE);

  //EncoderOne.setPosition(25); // Energia inicial em 25%

  //Inicializa o OLED 128X64 0.96 INCH com endereço I2C 0x3C
   // Inicialize o display SPI
  
  
  Serial.println("Display Iniciado...");
  display.begin();
  display.setFont();
  display.fillScreen(OLED_Backround_Color);
  display.setTextColor(OLED_Text_Color);
  display.setTextSize(1);

  //Display.begin(SSD1306_SWITCHCAPVCC);
  //Display.clearDisplay();
  //Display.setTextColor(SSD1306_WHITE); //Define a cor do texto


  //Limpa o display, necessário para apagar a imagem inicial da adafruit
  //Display.clearDisplay();
  //Atualiza o display
  //Display.display();



  //Display.setTextSize(1); //Define o tamanho da fonte do texto
  //Posição Largura/Altura
  //Display.setCursor(0, 0);
  //Display.print("Acesse o projeto em:");

  //Display.setTextSize(1); //Define o tamanho da fonte do texto
  //Posição Largura/Altura
  //Display.setCursor(0, 25);
  //Display.print("marlonnardi.com");

  //Display.display();


  //delay(2000);

  //Display.clearDisplay();
  //Display.display();
  //Serial.println("Iniciado...");

}//endSetup --------------------------------------


void loop()
{
  if (configMode) {
    server.handleClient();
  }
  trigger();
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
    delay(120);
    digitalWrite(pin_Triac, LOW);
    Serial.println("Desativado");
    delay(3000);

    aux2 = 0;
  }


}//----------------------- end_selecionaTela


//================== ISRs Interrupções Externas =======================//
/* Caso qualquer pino do encoder envie sinal, o metodo .tick() sempre será
  chamado, atualizando o valor do encoder via sua biblioteca. */

void ISR0()// Função ligada a uma interrupção ISR logo não pode retornar valor e deve ser mais rápida possível
{
  EncoderOne.tick();// Começa a ler o valor do encoder
}//-------------------------endISR0

void ISR1()// Função ligada a uma interrupção ISR logo não pode retornar valor e deve ser mais rápida possível
{
  EncoderOne.tick();// Começa a ler o valor do encoder

}//------------------------endISR1


void screenOne()
{
  //display.clearDisplay();

  display.setTextSize(2); //Define o tamanho da fonte do texto
  //Posição Largura/Altura
  display.setCursor(27, 0);
  display.print("Energy:");


  if (valorEncoder <= 9)
  {
    display.setTextSize(4); //Define o tamanho da fonte do texto
    //Posição Largura/Altura
    display.setCursor(46, 25);
    display.print(valorEncoder);

    //Posição Largura/Altura
    display.setCursor(71, 25);
    display.print("%");

  }

  if (valorEncoder >= 10 && valorEncoder <= 99)
  {

    display.setTextSize(4); //Define o tamanho da fonte do texto
    //Posição Largura/Altura
    display.setCursor(31, 25);
    display.print(valorEncoder);

    //Posição Largura/Altura
    display.setCursor(81, 25);
    display.print("%");

  }

  if (valorEncoder >= 100)
  {

    display.setTextSize(4); //Define o tamanho da fonte do texto
    //Posição Largura/Altura
    display.setCursor(18, 25);
    display.print(valorEncoder);

    //Posição Largura/Altura
    display.setCursor(92, 25);
    display.print("%");

  }



/*
  valorEncoder = EncoderOne.getPosition();//Captura o valor do encoder

  if (EncoderOne.getPosition() < 1)
  {
    EncoderOne.setPosition(1);
    valorEncoder = 1;
  }

  if (EncoderOne.getPosition() > 100)
  {
    EncoderOne.setPosition(100);
    valorEncoder = 100;
  }


  time_ms = map(valorEncoder, 1, 100, min_Power_ms, max_Power_ms);

  Serial.println(time_ms);

*/
  //Display.display();

}//end_screenOne ----------------------