#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <WiFiClientSecure.h>
#include "wifi_config.h"   // <-- credenciales wifi
#include "google_config.h" // <-- URL de Google Apps Script

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
  uint16_t umbralAlto;      // Voltaje alto (ADC)
  uint16_t umbralBajo;      // Voltaje bajo (ADC)
  uint8_t  histeresis;      // Histéresis en unidades ADC
  uint32_t pausaMs;         // Tiempo de pausa en milisegundos
  uint8_t  horaInicio;      // Hora de inicio de operación (hora local)
  uint8_t  horaFin;         // Hora de fin de operación (hora local)
  float    factorVoltaje;    // Factor para convertir ADC a Voltaje real
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
uint32_t tiempoTotalEncendido = 0;     // Total histórico (no se reinicia)
uint32_t tiempoTotalEncendidoDia = 0;  // Total del día actual
uint16_t totalOperaciones = 0;         // Total histórico
uint16_t totalOperacionesDia = 0;      // Total de operaciones del día
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

// Registro de operaciones (hasta 10)
const uint8_t MAX_REGISTROS = 10;
struct RegistroOperacion {
  time_t timestamp;
  bool encendido;
  uint32_t duracion;        // Para APAGADO: tiempo que estuvo encendido; para ENCENDIDO: tiempo que estuvo apagado antes de este encendido
};
RegistroOperacion registroOperaciones[MAX_REGISTROS];
uint8_t registroIndex = 0;
uint16_t totalOperacionesHist = 0;  // historial total (hasta MAX_REGISTROS)

ESP8266WebServer server(80);

// Variables para Google Sheets
WiFiClientSecure client;
uint32_t ultimoEnvioGoogle = 0;
const uint32_t INTERVALO_GOOGLE = 300000; // 5 minutos

// Funciones de configuración EEPROM
void cargarConfiguracion() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(EEPROM_ADDR, config);
  // Si la EEPROM está vacía (todos bytes 0xFF), cargar valores por defecto
  if (config.umbralAlto == 0xFFFF || config.umbralAlto == 0 || config.factorVoltaje == 0.0) {
    config.umbralAlto = 812;
    config.umbralBajo = 640;
    config.histeresis = 10;
    config.pausaMs = 900000;   // 13 minutos
    config.horaInicio = 12;
    config.horaFin = 22;
    config.factorVoltaje = 14.1 / 805.0;   // factor por defecto (14.1V / 805 ADC)
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

String formatDate(time_t t) {
  struct tm *tm_info = localtime(&t);
  char buffer[11];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d", tm_info);
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
  if(totalOperacionesHist < MAX_REGISTROS) totalOperacionesHist++;
}

// Función para enviar datos a Google Sheets
void enviarGoogleSheets(String fecha, String hora, String estadoBomba, float voltaje, int adc, String comando) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Google Sheets: WiFi no conectado");
    return;
  }
  
  client.setInsecure(); // Para evitar problemas de certificados
  if (!client.connect("script.google.com", 443)) {
    Serial.println("Google Sheets: Error de conexión HTTPS");
    return;
  }
  
  String postData = "fecha=" + fecha + "&hora=" + hora + "&estado=" + estadoBomba +
                    "&voltaje=" + String(voltaje, 2) + "&valadc=" + String(adc) +
                    "&activacion=" + comando;
  
  String request = "POST " + String(GOOGLE_SCRIPT_URL) + " HTTP/1.1\r\n";
  request += "Host: script.google.com\r\n";
  request += "Content-Type: application/x-www-form-urlencoded\r\n";
  request += "Content-Length: " + String(postData.length()) + "\r\n";
  request += "Connection: close\r\n\r\n";
  request += postData;
  
  client.print(request);
  
  // Esperar respuesta (opcional)
  unsigned long timeout = millis() + 5000;
  while (client.available() == 0 && millis() < timeout);
  if (client.available()) {
    String resp = client.readString();
    if (resp.indexOf("OK") >= 0) {
      Serial.println("Google Sheets: Envío exitoso");
    } else {
      Serial.println("Google Sheets: Respuesta inesperada: " + resp.substring(0, 100));
    }
  } else {
    Serial.println("Google Sheets: Timeout esperando respuesta");
  }
  client.stop();
}

// Función para reiniciar contadores diarios si cambia el día
void reiniciarContadoresDiariosSiNecesario() {
  static int diaAnterior = -1;
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    int diaActual = timeinfo.tm_mday;
    if (diaAnterior == -1) {
      diaAnterior = diaActual;
    } else if (diaActual != diaAnterior) {
      // Reset diarios
      tiempoTotalEncendidoDia = 0;
      totalOperacionesDia = 0;
      diaAnterior = diaActual;
      agregarLog("Nuevo día - reset diario");
    }
  }
}

// ================== PÁGINA PRINCIPAL ESTILIZADA ==================
String generarPaginaWeb() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta http-equiv='refresh' content='5'>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Control de Carga Solar</title>";
  html += "<style>";
  html += "* { box-sizing: border-box; margin: 0; padding: 0; }";
  html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #f5f7fa; color: #333; padding: 20px; }";
  html += ".container { max-width: 1200px; margin: 0 auto; }";
  html += "h1 { text-align: center; margin-bottom: 20px; color: #2c3e50; font-weight: 300; }";
  html += ".panel { background: white; border-radius: 12px; box-shadow: 0 2px 10px rgba(0,0,0,0.05); padding: 20px; margin-bottom: 20px; }";
  html += "h2 { font-size: 1.3rem; color: #2c3e50; margin-bottom: 15px; border-left: 4px solid #3498db; padding-left: 12px; }";
  html += ".status-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; }";
  html += ".status-info { display: flex; flex-direction: column; gap: 10px; }";
  html += ".status-item { display: flex; justify-content: space-between; border-bottom: 1px solid #eee; padding: 8px 0; }";
  html += ".status-label { font-weight: 600; color: #7f8c8d; }";
  html += ".status-value { font-weight: 500; }";
  html += ".status-big { text-align: right; }";
  html += ".voltage-big { font-size: 3rem; font-weight: 600; color: #2c3e50; line-height: 1; }";
  html += ".adc-big { font-size: 2rem; font-weight: 500; color: #3498db; }";
  html += ".unit { font-size: 1rem; font-weight: normal; color: #7f8c8d; }";
  html += ".buttons { display: flex; flex-wrap: wrap; gap: 12px; margin-top: 10px; }";
  html += "button, .btn { padding: 10px 20px; border: none; border-radius: 30px; font-size: 1rem; cursor: pointer; transition: all 0.3s ease; font-weight: 500; text-decoration: none; display: inline-block; text-align: center; }";
  html += ".btn-primary { background: #3498db; color: white; }";
  html += ".btn-primary:hover { background: #2980b9; transform: translateY(-2px); box-shadow: 0 4px 8px rgba(0,0,0,0.1); }";
  html += ".btn-secondary { background: #95a5a6; color: white; }";
  html += ".btn-secondary:hover { background: #7f8c8d; }";
  html += ".btn-danger { background: #e74c3c; color: white; }";
  html += ".btn-danger:hover { background: #c0392b; }";
  html += ".btn-success { background: #2ecc71; color: white; }";
  html += ".btn-success:hover { background: #27ae60; }";
  html += ".btn-config { background: #f39c12; color: white; }";
  html += ".btn-config:hover { background: #e67e22; transform: scale(1.05); }";
  html += ".active-mode { background: #2ecc71; }";
  html += "table { width: 100%; border-collapse: collapse; margin-top: 10px; }";
  html += "th, td { padding: 10px; text-align: left; border-bottom: 1px solid #ddd; }";
  html += "th { background-color: #f8f9fa; font-weight: 600; }";
  html += "tr:hover { background-color: #f5f5f5; }";
  html += ".live { font-style: italic; color: #7f8c8d; margin-top: 10px; font-size: 0.9rem; }";
  html += ".formula { font-size: 0.8em; color: #7f8c8d; margin-top: 10px; }";
  html += "@media (max-width: 768px) { .status-grid { grid-template-columns: 1fr; } .status-big { text-align: left; margin-top: 15px; } }";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>☀️ Control de Carga Solar</h1>";
  
  // Panel estado
  html += "<div class='panel'>";
  html += "<h2>Estado Actual</h2>";
  html += "<div class='status-grid'>";
  html += "<div class='status-info'>";
  html += "<div class='status-item'><span class='status-label'>Modo:</span><span class='status-value'>" + String(estado.modoAutomatico ? "Automático" : "Manual") + "</span></div>";
  html += "<div class='status-item'><span class='status-label'>Bomba:</span><span class='status-value'>" + String(estado.bombaEncendida ? "ENCENDIDA" : "APAGADA") + "</span></div>";
  struct tm timeinfo;
  if(getLocalTime(&timeinfo)) {
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
    html += "<div class='status-item'><span class='status-label'>Hora:</span><span class='status-value'>" + String(timeStr) + "</span></div>";
  }
  html += "<div class='status-item'><span class='status-label'>Tiempo total activado (histórico):</span><span class='status-value'>" + formatDuration(tiempoTotalEncendido) + "</span></div>";
  html += "<div class='status-item'><span class='status-label'>Total operaciones (histórico):</span><span class='status-value'>" + String(totalOperaciones) + "</span></div>";
  html += "<div class='status-item'><span class='status-label'>Hoy activado:</span><span class='status-value'>" + formatDuration(tiempoTotalEncendidoDia) + "</span></div>";
  html += "<div class='status-item'><span class='status-label'>Hoy operaciones:</span><span class='status-value'>" + String(totalOperacionesDia) + "</span></div>";
  html += "<div class='status-item'><span class='status-label'>Factor calibración:</span><span class='status-value'>" + String(config.factorVoltaje, 6) + " V/ADC</span></div>";
  html += "</div>";
  html += "<div class='status-big'>";
  html += "<div>Voltaje</div>";
  html += "<div class='voltage-big'>" + String(voltageActual, 2) + "<span class='unit'> V</span></div>";
  html += "<div style='margin-top: 15px;'>ADC RAW</div>";
  html += "<div class='adc-big'>" + String(rawValue) + "</div>";
  html += "<div class='formula'>Fórmula: Voltaje = ADC × " + String(config.factorVoltaje, 6) + "</div>";
  html += "</div>";
  html += "</div>";
  html += "</div>";
  
  // Panel controles
  html += "<div class='panel'>";
  html += "<h2>Controles</h2>";
  html += "<div class='buttons'>";
  if(estado.modoAutomatico) {
    html += "<a href='/auto'><button class='btn btn-success active-mode'>Modo Automático</button></a>";
    html += "<a href='/manual'><button class='btn btn-secondary'>Modo Manual</button></a>";
  } else {
    html += "<a href='/auto'><button class='btn btn-secondary'>Modo Automático</button></a>";
    html += "<a href='/manual'><button class='btn btn-success active-mode'>Modo Manual</button></a>";
  }
  if(!estado.modoAutomatico) {
    html += "<a href='/on'><button class='btn btn-primary'>Encender Bomba</button></a>";
    html += "<a href='/off'><button class='btn btn-danger'>Apagar Bomba</button></a>";
  } else {
    html += "<button class='btn btn-secondary' disabled style='opacity:0.6;'>Encender Bomba (solo manual)</button>";
    html += "<button class='btn btn-secondary' disabled style='opacity:0.6;'>Apagar Bomba (solo manual)</button>";
  }
  html += "<a href='/config'><button class='btn btn-config'>⚙️ Configuración</button></a>";
  html += "</div>";
  html += "</div>";
  
  // Tabla de operaciones (con tiempo apagado)
  html += "<div class='panel'>";
  html += "<h2>Registro de Operaciones</h2>";
  html += "<table>";
  html += "<tr><th>Hora</th><th>Evento</th><th>Duración (ON)</th><th>Tiempo Apagado</th></tr>";
  
  // Calcular tiempos apagado (entre un APAGADO y el siguiente ENCENDIDO)
  // Recorremos los eventos en orden cronológico
  // Primero, determinar el orden de los índices
  int indices[MAX_REGISTROS];
  int count = 0;
  for (int i = 0; i < totalOperacionesHist; i++) {
    int idx = (registroIndex - totalOperacionesHist + i + MAX_REGISTROS) % MAX_REGISTROS;
    indices[count++] = idx;
  }
  
  // Para cada evento, calcular el tiempo hasta el próximo evento si es relevante
  for (int i = 0; i < count; i++) {
    int idx = indices[i];
    RegistroOperacion& reg = registroOperaciones[idx];
    String duracionOn = "";
    String tiempoApagado = "";
    
    if (reg.encendido) {
      // Buscar el APAGADO anterior para calcular el tiempo apagado
      int prevIdx = -1;
      for (int j = i-1; j >= 0; j--) {
        int pIdx = indices[j];
        if (!registroOperaciones[pIdx].encendido) {
          prevIdx = pIdx;
          break;
        }
      }
      if (prevIdx != -1) {
        // Tiempo apagado = diferencia entre este encendido y el apagado anterior
        uint32_t diff = reg.timestamp - registroOperaciones[prevIdx].timestamp;
        tiempoApagado = formatDuration(diff * 1000); // convertir a ms
      } else {
        tiempoApagado = "-";
      }
      // Para encendido, duración ON se mostrará cuando se apague, así que aquí va "-"
      duracionOn = "-";
    } else {
      // Apagado: duración ON es la que almacenamos
      duracionOn = formatDuration(reg.duracion);
      // Tiempo apagado se mostrará en el siguiente encendido, aquí no
      tiempoApagado = "-";
    }
    
    html += "<tr>";
    html += "<td>" + formatTime(reg.timestamp) + "</td>";
    html += "<td>" + String(reg.encendido ? "ENCENDIDO" : "APAGADO") + "</td>";
    html += "<td>" + duracionOn + "</td>";
    html += "<td>" + tiempoApagado + "</td>";
    html += "</tr>";
  }
  
  html += "</table>";
  html += "</div>";
  
  // Últimos eventos
  html += "<div class='panel'>";
  html += "<h2>Últimos Eventos</h2>";
  html += "<table>";
  for(uint8_t i = 0; i < MAX_LOG_ENTRIES; i++) {
    uint8_t idx = (logIndex + MAX_LOG_ENTRIES - i - 1) % MAX_LOG_ENTRIES;
    html += "<tr><td>" + String(logEntries[idx].mensaje) + "</td></tr>";
  }
  html += "</table>";
  html += "</div>";
  
  html += "</div></body></html>";
  return html;
}

// ================== PÁGINA DE CONFIGURACIÓN ESTILIZADA ==================
String generarPaginaConfig() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Configuración - Control Solar</title>";
  html += "<style>";
  html += "* { box-sizing: border-box; margin: 0; padding: 0; }";
  html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #f5f7fa; color: #333; padding: 20px; }";
  html += ".container { max-width: 800px; margin: 0 auto; }";
  html += "h1 { text-align: center; margin-bottom: 20px; color: #2c3e50; font-weight: 300; }";
  html += ".panel { background: white; border-radius: 12px; box-shadow: 0 2px 10px rgba(0,0,0,0.05); padding: 25px; margin-bottom: 20px; }";
  html += "h2 { font-size: 1.3rem; color: #2c3e50; margin-bottom: 20px; border-left: 4px solid #f39c12; padding-left: 12px; }";
  html += ".form-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; }";
  html += ".form-group { display: flex; flex-direction: column; }";
  html += "label { font-weight: 600; margin-bottom: 5px; color: #7f8c8d; }";
  html += "input { padding: 8px 12px; border: 1px solid #ddd; border-radius: 6px; font-size: 1rem; transition: border 0.3s; }";
  html += "input:focus { outline: none; border-color: #f39c12; }";
  html += ".note { font-size: 0.8em; color: #7f8c8d; margin-top: 4px; }";
  html += ".buttons { display: flex; gap: 12px; margin-top: 25px; flex-wrap: wrap; }";
  html += "button, .btn { padding: 10px 20px; border: none; border-radius: 30px; font-size: 1rem; cursor: pointer; transition: all 0.3s ease; font-weight: 500; text-decoration: none; display: inline-block; text-align: center; }";
  html += ".btn-primary { background: #f39c12; color: white; }";
  html += ".btn-primary:hover { background: #e67e22; transform: translateY(-2px); box-shadow: 0 4px 8px rgba(0,0,0,0.1); }";
  html += ".btn-secondary { background: #95a5a6; color: white; }";
  html += ".btn-secondary:hover { background: #7f8c8d; }";
  html += ".formula { background: #f8f9fa; padding: 12px; border-radius: 8px; margin-top: 20px; font-size: 0.9rem; color: #2c3e50; text-align: center; }";
  html += ".current-time { margin-top: 20px; text-align: center; font-size: 0.9rem; color: #7f8c8d; }";
  html += ".calib-info { background: #e8f4fd; border-left: 4px solid #3498db; padding: 12px; margin-top: 15px; border-radius: 6px; font-size: 0.85rem; }";
  html += "@media (max-width: 600px) { .form-grid { grid-template-columns: 1fr; } }";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>⚙️ Configuración Avanzada</h1>";
  html += "<div class='panel'>";
  html += "<h2>Parámetros de Control</h2>";
  html += "<form action='/saveconfig' method='POST'>";
  html += "<div class='form-grid'>";
  
  // Umbral Alto
  html += "<div class='form-group'>";
  html += "<label>Umbral Alto (ADC)</label>";
  html += "<input type='number' name='alto' value='" + String(config.umbralAlto) + "' required>";
  float voltajeAlto = config.umbralAlto * config.factorVoltaje;
  html += "<div class='note'>≈ " + String(voltajeAlto, 2) + " V</div>";
  html += "</div>";
  
  // Umbral Bajo
  html += "<div class='form-group'>";
  html += "<label>Umbral Bajo (ADC)</label>";
  html += "<input type='number' name='bajo' value='" + String(config.umbralBajo) + "' required>";
  float voltajeBajo = config.umbralBajo * config.factorVoltaje;
  html += "<div class='note'>≈ " + String(voltajeBajo, 2) + " V</div>";
  html += "</div>";
  
  // Histéresis
  html += "<div class='form-group'>";
  html += "<label>Histéresis (ADC)</label>";
  html += "<input type='number' name='histeresis' value='" + String(config.histeresis) + "' required>";
  html += "<div class='note'>Margen para evitar oscilaciones</div>";
  html += "</div>";
  
  // Pausa (minutos) - ahora permite 0
  html += "<div class='form-group'>";
  html += "<label>Pausa (minutos)</label>";
  html += "<input type='number' name='pausa' min='0' value='" + String(config.pausaMs / 60000) + "' required>";
  html += "<div class='note'>Tiempo de espera antes de nuevo encendido (0 = sin pausa)</div>";
  html += "</div>";
  
  // Hora inicio
  html += "<div class='form-group'>";
  html += "<label>Hora inicio (local)</label>";
  html += "<input type='number' name='horaInicio' min='0' max='23' value='" + String(config.horaInicio) + "' required>";
  html += "<div class='note'>0-23, hora Guatemala</div>";
  html += "</div>";
  
  // Hora fin
  html += "<div class='form-group'>";
  html += "<label>Hora fin (local)</label>";
  html += "<input type='number' name='horaFin' min='0' max='23' value='" + String(config.horaFin) + "' required>";
  html += "<div class='note'>0-23, hora Guatemala</div>";
  html += "</div>";
  
  // Factor de calibración
  html += "<div class='form-group' style='grid-column: span 2;'>";
  html += "<label>Factor de calibración (V/ADC)</label>";
  html += "<input type='number' step='0.000001' name='factor' value='" + String(config.factorVoltaje, 6) + "' required>";
  html += "<div class='note'>Fórmula: Voltaje = ADC × este factor</div>";
  html += "</div>";
  
  html += "</div>"; // cierra form-grid
  
  html += "<div class='buttons'>";
  html += "<button type='submit' class='btn btn-primary'>💾 Guardar Cambios</button>";
  html += "<a href='/' class='btn btn-secondary'>← Volver al inicio</a>";
  html += "</div>";
  html += "</form>";
  
  html += "<div class='calib-info'>";
  html += "📐 <strong>¿Cómo calibrar?</strong><br>";
  html += "1. Mide el voltaje real de la batería con un multímetro (ej. 12.6 V).<br>";
  html += "2. Anota el valor ADC RAW que muestra la página principal (ej. 720).<br>";
  html += "3. Calcula el factor: voltaje_real / ADC_raw (ej. 12.6 / 720 = 0.0175).<br>";
  html += "4. Ingresa ese factor aquí y guarda. ¡El voltaje mostrado será correcto!";
  html += "</div>";
  
  html += "<div class='formula'>";
  html += "<strong>Fórmula de conversión actual:</strong> Voltaje = ADC × " + String(config.factorVoltaje, 6);
  html += "</div>";
  
  // Mostrar hora actual
  struct tm timeinfo;
  if(getLocalTime(&timeinfo)) {
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
    html += "<div class='current-time'>🕒 Hora actual (Guatemala): " + String(timeStr) + "</div>";
  }
  
  html += "</div>"; // cierra panel
  html += "</div></body></html>";
  return html;
}

void encenderBomba() {
  digitalWrite(RELAY_PIN, LOW);
  estado.bombaEncendida = true;
  tiempoUltimoEncendido = millis();
  agregarRegistro(true);
  agregarLog("Bomba ENCENDIDA");
  
  // Enviar a Google Sheets con comando 1
  struct tm timeinfo;
  if(getLocalTime(&timeinfo)) {
    char fecha[11], hora[9];
    strftime(fecha, sizeof(fecha), "%Y-%m-%d", &timeinfo);
    strftime(hora, sizeof(hora), "%H:%M:%S", &timeinfo);
    enviarGoogleSheets(String(fecha), String(hora), "ON", voltageActual, rawValue, "1");
  } else {
    Serial.println("Error: No se pudo obtener hora para registro de encendido");
  }
}

void apagarBomba() {
  digitalWrite(RELAY_PIN, HIGH);
  if(estado.bombaEncendida) {
    uint32_t duracion = millis() - tiempoUltimoEncendido;
    tiempoTotalEncendido += duracion;
    tiempoTotalEncendidoDia += duracion;
    totalOperaciones++;
    totalOperacionesDia++;
    agregarRegistro(false, duracion);
    agregarLog("Bomba APAGADA");
    
    // Enviar a Google Sheets con comando 0
    struct tm timeinfo;
    if(getLocalTime(&timeinfo)) {
      char fecha[11], hora[9];
      strftime(fecha, sizeof(fecha), "%Y-%m-%d", &timeinfo);
      strftime(hora, sizeof(hora), "%H:%M:%S", &timeinfo);
      enviarGoogleSheets(String(fecha), String(hora), "OFF", voltageActual, rawValue, "0");
    } else {
      Serial.println("Error: No se pudo obtener hora para registro de apagado");
    }
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
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Error OLED"));
    for(;;);
  }
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

  for(uint8_t i=0; i<NUM_LECTURAS; i++) {
    lecturasADC[i] = analogRead(ADC_PIN);
    totalLecturas += lecturasADC[i];
  }
  agregarLog("ADC iniciado");

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  agregarLog("Hardware listo");

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

  // Rutas web
  server.on("/", []() { server.send(200, "text/html", generarPaginaWeb()); });
  server.on("/auto", []() { estado.modoAutomatico = true; agregarLog("Modo auto"); server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
  server.on("/manual", []() { estado.modoAutomatico = false; agregarLog("Modo manual"); server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
  server.on("/on", []() { estado.modoAutomatico = false; encenderBomba(); server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
  server.on("/off", []() { estado.modoAutomatico = false; apagarBomba(); server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
  server.on("/config", HTTP_GET, []() { server.send(200, "text/html", generarPaginaConfig()); });
  
  server.on("/saveconfig", HTTP_POST, []() {
    if (server.hasArg("alto") && server.hasArg("bajo") && server.hasArg("histeresis") &&
        server.hasArg("pausa") && server.hasArg("horaInicio") && server.hasArg("horaFin") &&
        server.hasArg("factor")) {
      
      config.umbralAlto = server.arg("alto").toInt();
      config.umbralBajo = server.arg("bajo").toInt();
      config.histeresis = server.arg("histeresis").toInt();
      config.pausaMs = server.arg("pausa").toInt() * 60000; // convertir minutos a ms
      config.horaInicio = server.arg("horaInicio").toInt();
      config.horaFin = server.arg("horaFin").toInt();
      config.factorVoltaje = server.arg("factor").toFloat();

      // Validaciones
      if (config.umbralAlto < config.umbralBajo) {
        uint16_t tmp = config.umbralAlto;
        config.umbralAlto = config.umbralBajo;
        config.umbralBajo = tmp;
      }
      if (config.horaInicio > 23) config.horaInicio = 0;
      if (config.horaFin > 23) config.horaFin = 23;
      if (config.histeresis > 50) config.histeresis = 50;
      if (config.pausaMs < 0) config.pausaMs = 0;  // permitir 0
      if (config.pausaMs > 3600000) config.pausaMs = 3600000;
      if (config.factorVoltaje <= 0.0) config.factorVoltaje = 14.1 / 805.0;

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
  
  // Inicializar contadores diarios con el día actual
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
      // comando vacío (cadena vacía) como se pide
      enviarGoogleSheets(String(fecha), String(hora), estadoBomba, voltageActual, rawValue, "");
    } else {
      Serial.println("Google Sheets: No se pudo obtener hora para envío periódico");
    }
  }

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

  // Reiniciar contadores diarios si cambió el día
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