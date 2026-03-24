#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include "wifi_config.h"   // <-- credenciales wifi

// Configuración OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Configuración hardware
const uint8_t RELAY_PIN = D5;
const uint8_t ADC_PIN = A0;

// Configuración NTP (UTC-6 Guatemala)
const char* ntpServer = "time.google.com";
const long gmtOffset_sec = -21600;   // UTC-6
const int daylightOffset_sec = 0;

// Estructura de configuración (parámetros ajustables)
struct Configuracion {
  uint16_t umbralAlto;      // Voltaje alto (14.1V -> 812)
  uint16_t umbralBajo;      // Voltaje bajo (11V -> 640)
  uint8_t  histeresis;      // Histéresis en unidades ADC
  uint32_t pausaMs;         // Tiempo de pausa en milisegundos
  uint8_t  horaInicio;      // Hora de inicio de operación (hora local)
  uint8_t  horaFin;         // Hora de fin de operación (hora local)
};

Configuracion config;       // Variable global con los valores actuales

// Direcciones EEPROM
const int EEPROM_SIZE = sizeof(Configuracion);
const int EEPROM_ADDR = 0;

// Variables de estado
struct {
  bool bombaEncendida : 1;
  bool enPausa : 1;
  bool modoAutomatico : 1;
  bool errorWiFi : 1;
  bool errorReloj : 1;
} estado;

uint32_t tiempoInicioPausa = 0;
uint32_t tiempoUltimoEncendido = 0;
uint32_t tiempoTotalEncendido = 0;
float voltageActual = 0.0;
uint16_t rawValue = 0;

// Filtro ADC
const uint8_t NUM_LECTURAS = 5;
uint16_t lecturasADC[NUM_LECTURAS];
uint8_t indiceLectura = 0;
uint32_t totalLecturas = 0;

// Log de eventos y registros
const uint8_t MAX_LOG_ENTRIES = 4;
struct LogEntry {
  char mensaje[21];
};
LogEntry logEntries[MAX_LOG_ENTRIES];
uint8_t logIndex = 0;

// Registro de operaciones
const uint8_t MAX_REGISTROS = 10;
struct RegistroOperacion {
  time_t timestamp;
  bool encendido;
  uint32_t duracion;
};
RegistroOperacion registroOperaciones[MAX_REGISTROS];
uint8_t registroIndex = 0;
uint16_t totalOperaciones = 0;

ESP8266WebServer server(80);

// Funciones de configuración EEPROM
void cargarConfiguracion() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(EEPROM_ADDR, config);
  // Si la EEPROM está vacía (todos bytes 0xFF), cargar valores por defecto
  if (config.umbralAlto == 0xFFFF || config.umbralAlto == 0) {
    config.umbralAlto = 812;
    config.umbralBajo = 640;
    config.histeresis = 10;
    config.pausaMs = 900000;   // 13 minutos
    config.horaInicio = 12;
    config.horaFin = 22;
    guardarConfiguracion();    // Guardar valores por defecto
  }
  EEPROM.end();
}

void guardarConfiguracion() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(EEPROM_ADDR, config);
  EEPROM.commit();
  EEPROM.end();
}

// Funciones auxiliares
String formatDuration(uint32_t ms) {
  uint32_t seconds = ms / 1000;
  uint32_t minutes = seconds / 60;
  seconds %= 60;
  return String(minutes) + "m " + String(seconds) + "s";
}

String formatTime(time_t t) {
  struct tm *tm_info = localtime(&t);
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%H:%M:%S", tm_info);
  return String(buffer);
}

void actualizarOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Línea 1: Estado principal
  display.setCursor(0, 0);
  String linea1;
  if(estado.errorWiFi) {
    linea1 = "ERROR WiFi";
  } else if(estado.errorReloj) {
    linea1 = "ERROR Reloj";
  } else if(estado.bombaEncendida) {
    linea1 = "Bomba: ON";
  } else if(estado.enPausa) {
    uint32_t tiempoRestante = (config.pausaMs - (millis() - tiempoInicioPausa)) / 1000;
    linea1 = "Pausa: " + String(tiempoRestante) + "s";
  } else {
    linea1 = "Cargando...";
  }
  display.println(linea1);

  // Línea 2: Voltaje y modo
  display.setCursor(0, 8);
  String linea2 = String(voltageActual, 2) + "V ";
  linea2 += estado.modoAutomatico ? "(Auto)" : "(Manual)";
  display.println(linea2);

  // Líneas 3 y 4: Log de mensajes
  for(uint8_t i = 0; i < 2; i++) {
    uint8_t idx = (logIndex + MAX_LOG_ENTRIES - 2 + i) % MAX_LOG_ENTRIES;
    display.setCursor(0, 16 + (i * 8));
    display.println(logEntries[idx].mensaje);
  }
  
  display.display();
}

void agregarLog(const char* mensaje) {
  strncpy(logEntries[logIndex].mensaje, mensaje, sizeof(logEntries[0].mensaje)-1);
  logEntries[logIndex].mensaje[sizeof(logEntries[0].mensaje)-1] = '\0';
  logIndex = (logIndex + 1) % MAX_LOG_ENTRIES;
  
  Serial.println(mensaje);
  actualizarOLED();
}

void agregarRegistro(bool encendido, uint32_t duracion = 0) {
  time_t now;
  time(&now);
  
  registroOperaciones[registroIndex].timestamp = now;
  registroOperaciones[registroIndex].encendido = encendido;
  registroOperaciones[registroIndex].duracion = duracion;
  
  registroIndex = (registroIndex + 1) % MAX_REGISTROS;
  if(totalOperaciones < MAX_REGISTROS) totalOperaciones++;
}

String generarPaginaWeb() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta http-equiv='refresh' content='5'>";
  html += "<meta charset='UTF-8'>";
  html += "<title>Control de Carga Solar</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; }";
  html += "h1, h2 { color: #444; }";
  html += ".panel { border: 1px solid #ddd; padding: 15px; margin-bottom: 20px; border-radius: 5px; }";
  html += ".on { color: green; } .off { color: red; }";
  html += "button { padding: 8px 15px; margin: 5px; background: #4CAF50; color: white; border: none; border-radius: 4px; }";
  html += "table { width: 100%; border-collapse: collapse; }";
  html += "th, td { padding: 8px; text-align: left; border-bottom: 1px solid #ddd; }";
  html += "tr:hover { background-color: #f5f5f5; }";
  html += ".formula { font-size: 0.8em; color: #666; margin-top: 5px; }";
  html += "</style></head><body>";
  
  html += "<h1>Control de Carga Solar</h1>";
  
  // Panel de estado
  html += "<div class='panel'>";
  html += "<h2>Estado Actual</h2>";
  html += "<p><strong>Modo:</strong> " + String(estado.modoAutomatico ? "Automático" : "Manual") + "</p>";
  html += "<p><strong>Bomba:</strong> <span class='" + String(estado.bombaEncendida ? "on'>ENCENDIDA" : "off'>APAGADA") + "</span></p>";
  html += "<p><strong>Voltaje:</strong> " + String(voltageActual, 2) + "V</p>";
  html += "<p><strong>ADC RAW:</strong> " + String(rawValue) + "</p>";
  html += "<div class='formula'><strong>Fórmula de conversión:</strong> Voltaje = ADC × (14.1 / 805.0)</div>";
  
  struct tm timeinfo;
  if(getLocalTime(&timeinfo)) {
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
    html += "<p><strong>Hora (Guatemala):</strong> " + String(timeStr) + "</p>";
  }
  
  html += "<p><strong>Tiempo total activado:</strong> " + formatDuration(tiempoTotalEncendido) + "</p>";
  html += "<p><strong>Total operaciones:</strong> " + String(totalOperaciones) + "</p>";
  html += "</div>";
  
  // Panel de control
  html += "<div class='panel'>";
  html += "<h2>Controles</h2>";
  html += "<a href='/auto'><button>Modo Automático</button></a>";
  html += "<a href='/manual'><button>Modo Manual</button></a><br>";
  html += "<a href='/on'><button>Encender Bomba</button></a>";
  html += "<a href='/off'><button>Apagar Bomba</button></a>";
  html += "<a href='/config'><button>Configuración</button></a>";
  html += "</div>";
  
  // Panel de registros
  html += "<div class='panel'>";
  html += "<h2>Registro de Operaciones</h2>";
  html += "<table><tr><th>Hora</th><th>Evento</th><th>Duración</th></tr>";
  
  for(int i = 0; i < totalOperaciones; i++) {
    int idx = (registroIndex - totalOperaciones + i + MAX_REGISTROS) % MAX_REGISTROS;
    html += "<tr>";
    html += "<td>" + formatTime(registroOperaciones[idx].timestamp) + "</td>";
    html += "<td>" + String(registroOperaciones[idx].encendido ? "ENCENDIDO" : "APAGADO") + "</td>";
    html += "<td>" + (registroOperaciones[idx].encendido ? formatDuration(registroOperaciones[idx].duracion) : "-") + "</td>";
    html += "</tr>";
  }
  
  html += "</table>";
  html += "</div>";
  
  // Panel de eventos
  html += "<div class='panel'>";
  html += "<h2>Últimos Eventos</h2>";
  html += "<table>";
  
  for(uint8_t i = 0; i < MAX_LOG_ENTRIES; i++) {
    uint8_t idx = (logIndex + MAX_LOG_ENTRIES - i - 1) % MAX_LOG_ENTRIES;
    html += "<tr><td>" + String(logEntries[idx].mensaje) + "</td></tr>";
  }
  
  html += "</table>";
  html += "</div>";
  
  html += "</body></html>";
  
  return html;
}

void encenderBomba() {
  digitalWrite(RELAY_PIN, LOW);
  estado.bombaEncendida = true;
  tiempoUltimoEncendido = millis();
  agregarRegistro(true);
  agregarLog("Bomba ENCENDIDA");
}

void apagarBomba() {
  digitalWrite(RELAY_PIN, HIGH);
  if(estado.bombaEncendida) {
    uint32_t duracion = millis() - tiempoUltimoEncendido;
    tiempoTotalEncendido += duracion;
    agregarRegistro(false, duracion);
    agregarLog("Bomba APAGADA");
  }
  estado.bombaEncendida = false;
}

uint16_t leerADCfiltrado() {
  totalLecturas -= lecturasADC[indiceLectura];
  lecturasADC[indiceLectura] = analogRead(ADC_PIN);
  totalLecturas += lecturasADC[indiceLectura];
  indiceLectura = (indiceLectura + 1) % NUM_LECTURAS;
  return totalLecturas / NUM_LECTURAS;
}

void setup() {
  delay(1000);
  Serial.begin(115200);
  
  // Inicializar OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Error OLED"));
    for(;;);
  }
  display.display();
  delay(2000);
  display.clearDisplay();
  
  // Cargar configuración desde EEPROM
  cargarConfiguracion();
  
  // Estado inicial
  estado.bombaEncendida = false;
  estado.enPausa = false;
  estado.modoAutomatico = true;
  estado.errorWiFi = false;
  estado.errorReloj = false;

  agregarLog("Iniciando...");

  // Inicializar filtro ADC
  for(uint8_t i=0; i<NUM_LECTURAS; i++) {
    lecturasADC[i] = analogRead(ADC_PIN);
    totalLecturas += lecturasADC[i];
  }
  agregarLog("ADC iniciado");

  // Configurar hardware
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  agregarLog("Hardware listo");

  // Conectar WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
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

  // Configurar tiempo (UTC-6 Guatemala)
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

  // Configurar servidor web
  server.on("/", []() { server.send(200, "text/html", generarPaginaWeb()); });
  server.on("/auto", []() { estado.modoAutomatico = true; agregarLog("Modo auto"); server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
  server.on("/manual", []() { estado.modoAutomatico = false; agregarLog("Modo manual"); server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
  server.on("/on", []() { estado.modoAutomatico = false; encenderBomba(); server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
  server.on("/off", []() { estado.modoAutomatico = false; apagarBomba(); server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
  
  // Página de configuración (GET)
  server.on("/config", HTTP_GET, []() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<title>Configuración</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 20px; }";
    html += "h2 { color: #444; }";
    html += ".config-form { border: 1px solid #ddd; padding: 20px; border-radius: 5px; max-width: 500px; }";
    html += "label { display: inline-block; width: 150px; margin-bottom: 10px; }";
    html += "input { width: 100px; padding: 5px; margin-bottom: 10px; }";
    html += "button { background: #4CAF50; color: white; padding: 8px 15px; border: none; border-radius: 4px; margin-top: 10px; }";
    html += "a { display: inline-block; margin-top: 10px; }";
    html += ".note { font-size: 0.8em; color: #666; margin-left: 10px; }";
    html += ".formula { font-size: 0.8em; color: #666; margin-top: 15px; border-top: 1px solid #ddd; padding-top: 10px; }";
    html += "</style></head><body>";
    html += "<h2>Configuración de Parámetros</h2>";
    html += "<div class='config-form'>";
    html += "<form action='/saveconfig' method='POST'>";
    
    // Umbral Alto
    html += "<label>Umbral Alto (ADC):</label> <input type='number' name='alto' value='" + String(config.umbralAlto) + "'>";
    float voltajeAlto = config.umbralAlto * (14.1 / 805.0);
    html += "<span class='note'>≈ " + String(voltajeAlto, 2) + " V</span><br>";
    
    // Umbral Bajo
    html += "<label>Umbral Bajo (ADC):</label> <input type='number' name='bajo' value='" + String(config.umbralBajo) + "'>";
    float voltajeBajo = config.umbralBajo * (14.1 / 805.0);
    html += "<span class='note'>≈ " + String(voltajeBajo, 2) + " V</span><br>";
    
    // Histéresis
    html += "<label>Histéresis (ADC):</label> <input type='number' name='histeresis' value='" + String(config.histeresis) + "'><br>";
    
    // Pausa (en minutos)
    html += "<label>Pausa (minutos):</label> <input type='number' name='pausa' value='" + String(config.pausaMs / 60000) + "'><br>";
    
    // Hora inicio
    html += "<label>Hora inicio (local):</label> <input type='number' name='horaInicio' min='0' max='23' value='" + String(config.horaInicio) + "'><br>";
    
    // Hora fin
    html += "<label>Hora fin (local):</label> <input type='number' name='horaFin' min='0' max='23' value='" + String(config.horaFin) + "'><br>";
    
    html += "<button type='submit'>Guardar</button>";
    html += "</form>";
    html += "<div class='formula'><strong>Fórmula de conversión ADC → Voltaje:</strong> Voltaje = ADC × (14.1 / 805.0)</div>";
    html += "<a href='/'>Volver al inicio</a>";
    html += "</div>";
    
    // Mostrar hora actual
    struct tm timeinfo;
    if(getLocalTime(&timeinfo)) {
      char timeStr[20];
      strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
      html += "<p>Hora actual (Guatemala): " + String(timeStr) + "</p>";
    }
    
    html += "</body></html>";
    server.send(200, "text/html", html);
  });
  
  // Guardar configuración (POST)
  server.on("/saveconfig", HTTP_POST, []() {
    if (server.hasArg("alto") && server.hasArg("bajo") && server.hasArg("histeresis") &&
        server.hasArg("pausa") && server.hasArg("horaInicio") && server.hasArg("horaFin")) {
      
      config.umbralAlto = server.arg("alto").toInt();
      config.umbralBajo = server.arg("bajo").toInt();
      config.histeresis = server.arg("histeresis").toInt();
      config.pausaMs = server.arg("pausa").toInt() * 60000; // convertir minutos a ms
      config.horaInicio = server.arg("horaInicio").toInt();
      config.horaFin = server.arg("horaFin").toInt();

      // Validaciones simples
      if (config.umbralAlto < config.umbralBajo) {
        uint16_t tmp = config.umbralAlto;
        config.umbralAlto = config.umbralBajo;
        config.umbralBajo = tmp;
      }
      if (config.horaInicio > 23) config.horaInicio = 0;
      if (config.horaFin > 23) config.horaFin = 23;
      if (config.histeresis > 50) config.histeresis = 50;
      if (config.pausaMs < 60000) config.pausaMs = 60000; // mínimo 1 minuto
      if (config.pausaMs > 3600000) config.pausaMs = 3600000; // máximo 1 hora

      guardarConfiguracion();
      agregarLog("Config guardada");
      server.sendHeader("Location", "/config");
      server.send(302, "text/plain", "");
    } else {
      server.send(400, "text/plain", "Faltan parámetros");
    }
  });
  
  server.begin();
  agregarLog("Servidor iniciado");
}

void loop() {
  server.handleClient();
  
  // Leer y procesar ADC
  rawValue = leerADCfiltrado();
  voltageActual = rawValue * (14.1 / 805.0); // Calibración específica

  // Actualizar OLED cada 5 segundos
  static uint32_t lastUpdate = 0;
  if(millis() - lastUpdate > 5000) {
    lastUpdate = millis();
    actualizarOLED();
  }

  // Lógica de control automático
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