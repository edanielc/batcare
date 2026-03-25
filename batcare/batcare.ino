#include "globals.h"
#include "hardware.h"
#include "utils.h"
#include "google.h"
#include "web.h"

// Definiciones de variables globales (sin extern)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const uint8_t RELAY_PIN = D5;
const uint8_t ADC_PIN = A0;

const char* ntpServer = "time.google.com";
const long gmtOffset_sec = -21600;
const int daylightOffset_sec = 0;

Configuracion config;

const int EEPROM_SIZE = sizeof(Configuracion);
const int EEPROM_ADDR = 0;

Estado estado;

uint32_t tiempoInicioPausa = 0;
uint32_t tiempoUltimoEncendido = 0;
uint32_t tiempoTotalEncendido = 0;
uint32_t tiempoTotalEncendidoDia = 0;
uint16_t totalOperaciones = 0;
uint16_t totalOperacionesDia = 0;
float voltageActual = 0.0;
uint16_t rawValue = 0;

const uint8_t NUM_LECTURAS = 5;
uint16_t lecturasADC[NUM_LECTURAS];
uint8_t indiceLectura = 0;
uint32_t totalLecturas = 0;

const uint8_t MAX_LOG_ENTRIES = 4;
LogEntry logEntries[MAX_LOG_ENTRIES];
uint8_t logIndex = 0;

const uint8_t MAX_REGISTROS = 10;
RegistroOperacion registroOperaciones[MAX_REGISTROS];
uint8_t registroIndex = 0;
uint16_t totalOperacionesHist = 0;

ESP8266WebServer server(80);

WiFiClientSecure client;
uint32_t ultimoEnvioGoogle = 0;
const uint32_t INTERVALO_GOOGLE = 300000; // 5 minutos

// ========== FUNCIONES DE CONFIGURACIÓN (antes estaban en config.cpp) ==========
void cargarConfiguracion() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(EEPROM_ADDR, config);
  if (config.umbralAlto == 0xFFFF || config.umbralAlto == 0 || config.factorVoltaje == 0.0) {
    config.umbralAlto = 812;
    config.umbralBajo = 640;
    config.histeresis = 10;
    config.pausaMs = 900000;
    config.horaInicio = 12;
    config.horaFin = 22;
    config.factorVoltaje = 14.1 / 805.0;
    guardarConfiguracion();
  }
  EEPROM.end();
}

void guardarConfiguracion() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(EEPROM_ADDR, config);
  EEPROM.commit();
  EEPROM.end();
}
// =============================================================================

void setup() {
  delay(1000);
  Serial.begin(115200);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Error OLED"));
    for(;;);
  }
  // Ajustar contraste (brillo) a 80 (0-255, menor = menos brillo)
  display.ssd1306_command(0x81);   // Comando para control de contraste
  display.ssd1306_command(00);     // Valor de contraste
  display.display();
  delay(2000);
  display.clearDisplay();
  
  cargarConfiguracion();
  
  estado.bombaEncendida = false;
  estado.enPausa = false;
  estado.modoAutomatico = true;
  estado.errorWiFi = false;
  estado.errorReloj = false;

  agregarLog("Iniciando...");

  inicializarHardware();
  
  // Conectar WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  agregarLog("Conectando WiFi...");
  
  uint8_t intentos = 0;
  while(WiFi.status() != WL_CONNECTED && intentos < 20) {
    delay(500);
    intentos++;
  }
  
  if(WiFi.status() != WL_CONNECTED) {
    estado.errorWiFi = true;
    agregarLog("Error WiFi");
    actualizarOLED();
    delay(2000);
    ESP.restart();
  }
  
  agregarLog(("IP: " + WiFi.localIP().toString()).c_str());

  // Configurar tiempo
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  agregarLog("Sinc. hora...");
  
  struct tm timeinfo;
  uint8_t intentosNTP = 0;
  while(!getLocalTime(&timeinfo) && intentosNTP < 10) {
    delay(500);
    intentosNTP++;
  }
  
  if(intentosNTP >= 10) {
    estado.errorReloj = true;
    agregarLog("Error reloj");
  } else {
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
    agregarLog(timeStr);
  }

  configurarWebServer();
  
  server.begin();
  agregarLog("Servidor iniciado");
  
  reiniciarContadoresDiariosSiNecesario();
}

void loop() {
  server.handleClient();
  
  rawValue = leerADCfiltrado();
  voltageActual = rawValue * config.factorVoltaje;

  // Envío periódico a Google Sheets cada 5 minutos
  if (millis() - ultimoEnvioGoogle >= INTERVALO_GOOGLE) {
    ultimoEnvioGoogle = millis();
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      char fecha[11], hora[9];
      strftime(fecha, sizeof(fecha), "%Y-%m-%d", &timeinfo);
      strftime(hora, sizeof(hora), "%H:%M:%S", &timeinfo);
      String estadoBomba = estado.bombaEncendida ? "ON" : "OFF";
      enviarGoogleSheets(String(fecha), String(hora), estadoBomba, voltageActual, rawValue, "");
    } else {
      Serial.println("Google Sheets: No se pudo obtener hora para envío periódico");
    }
  }

  static uint32_t lastUpdate = 0;
  if(millis() - lastUpdate > 5000) {
    lastUpdate = millis();
    actualizarOLED();
  }

  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) {
    if(!estado.errorReloj) {
      estado.errorReloj = true;
      agregarLog("Error reloj");
    }
    return;
  } else if(estado.errorReloj) {
    estado.errorReloj = false;
    agregarLog("Reloj OK");
  }

  reiniciarContadoresDiariosSiNecesario();

  uint8_t horaActual = timeinfo.tm_hour;

  if(estado.modoAutomatico) {    
    if(estado.enPausa) {
      if(millis() - tiempoInicioPausa >= config.pausaMs) {
        estado.enPausa = false;
        agregarLog("Pausa completa");
        
        if(rawValue >= (config.umbralAlto - config.histeresis) && horaActual >= config.horaInicio && horaActual < config.horaFin) {
          encenderBomba();
        }
      }
    } 
    else {
      if(!estado.bombaEncendida && rawValue >= config.umbralAlto && horaActual >= config.horaInicio && horaActual < config.horaFin) {
        estado.enPausa = true;
        tiempoInicioPausa = millis();
        agregarLog("Iniciando pausa");
      }
      
      if(estado.bombaEncendida && (rawValue <= (config.umbralBajo + config.histeresis) || horaActual < config.horaInicio || horaActual >= config.horaFin)) {
        apagarBomba();
      }
    }
  }

  delay(1000);
}