#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Configuración OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Configuración WiFi
const char* ssid = "R-2G";
const char* password = "KRISANTEMOZ88";

// Configuración hardware
const uint8_t RELAY_PIN = D5;
const uint8_t ADC_PIN = A0;

// Umbrales
const uint16_t UMBRAL_ALTO = 805;    // 14.1V
const uint16_t UMBRAL_BAJO = 750;    // 11V
const uint8_t HISTERESIS = 10;
const uint32_t PAUSA_MS = 240000;    // 4 minutos en ms

// Configuración NTP
const char* ntpServer = "time.google.com";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

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

// Función para formatear tiempo
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
    uint32_t tiempoRestante = (PAUSA_MS - (millis() - tiempoInicioPausa)) / 1000;
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
  html += "<meta charset='UTF-8'>";  // 👈 Añade esta línea
  html += "<title>Control de Carga</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; }";
  html += "h1, h2 { color: #444; }";
  html += ".panel { border: 1px solid #ddd; padding: 15px; margin-bottom: 20px; border-radius: 5px; }";
  html += ".on { color: green; } .off { color: red; }";
  html += "button { padding: 8px 15px; margin: 5px; background: #4CAF50; color: white; border: none; border-radius: 4px; }";
  html += "table { width: 100%; border-collapse: collapse; }";
  html += "th, td { padding: 8px; text-align: left; border-bottom: 1px solid #ddd; }";
  html += "tr:hover { background-color: #f5f5f5; }";
  html += "</style></head><body>";
  
  html += "<h1>Control de Carga Solar</h1>";
  
  // Panel de estado
  html += "<div class='panel'>";
  html += "<h2>Estado Actual</h2>";
  html += "<p><strong>Modo:</strong> " + String(estado.modoAutomatico ? "Automático" : "Manual") + "</p>";
  html += "<p><strong>Bomba:</strong> <span class='" + String(estado.bombaEncendida ? "on'>ENCENDIDA" : "off'>APAGADA") + "</span></p>";
  html += "<p><strong>Voltaje:</strong> " + String(voltageActual, 2) + "V</p>";
  
  struct tm timeinfo;
  if(getLocalTime(&timeinfo)) {
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
    html += "<p><strong>Hora GMT+0:</strong> " + String(timeStr) + "</p>";
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

void setup() {
  Serial.begin(115200);
  
  // Inicializar OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Error OLED"));
    for(;;);
  }
  display.display();
  delay(2000);
  display.clearDisplay();
  
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

  // Configurar servidor web
  server.on("/", []() { server.send(200, "text/html", generarPaginaWeb()); });
  server.on("/auto", []() { estado.modoAutomatico = true; agregarLog("Modo auto"); server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
  server.on("/manual", []() { estado.modoAutomatico = false; agregarLog("Modo manual"); server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
  server.on("/on", []() { estado.modoAutomatico = false; encenderBomba(); server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
  server.on("/off", []() { estado.modoAutomatico = false; apagarBomba(); server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
  server.begin();
  agregarLog("Servidor iniciado");
}

void loop() {
  server.handleClient();
  
  // Leer y procesar ADC
  rawValue = leerADCfiltrado();
  voltageActual = rawValue * (14.1 / 805.0);

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
      if(millis() - tiempoInicioPausa >= PAUSA_MS) { // 2 minutos
        estado.enPausa = false;
        agregarLog("Pausa completa");
        
        if(rawValue >= (UMBRAL_ALTO-HISTERESIS) && horaActual >=12 && horaActual <22) {
          encenderBomba();
        }
      }
    } 
    else {
      if(!estado.bombaEncendida && rawValue >= UMBRAL_ALTO && horaActual >=12 && horaActual <22) {
        estado.enPausa = true;
        tiempoInicioPausa = millis();
        agregarLog("Iniciando pausa");
      }
      
      if(estado.bombaEncendida && (rawValue <= (UMBRAL_BAJO+HISTERESIS) || horaActual <12 || horaActual >=22)) {
        apagarBomba();
      }
    }
  }

  delay(1000);
}

uint16_t leerADCfiltrado() {
  totalLecturas -= lecturasADC[indiceLectura];
  lecturasADC[indiceLectura] = analogRead(ADC_PIN);
  totalLecturas += lecturasADC[indiceLectura];
  indiceLectura = (indiceLectura + 1) % NUM_LECTURAS;
  return totalLecturas / NUM_LECTURAS;
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