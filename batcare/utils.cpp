#include "utils.h"
#include "globals.h"

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

void agregarLog(const char* mensaje) {
  strncpy(logEntries[logIndex].mensaje, mensaje, sizeof(logEntries[0].mensaje)-1);
  logEntries[logIndex].mensaje[sizeof(logEntries[0].mensaje)-1] = '\0';
  logIndex = (logIndex + 1) % MAX_LOG_ENTRIES;
  
  Serial.println(mensaje);
  actualizarOLED();
}

void agregarRegistro(bool encendido, uint32_t duracion) {
  time_t now;
  time(&now);
  
  registroOperaciones[registroIndex].timestamp = now;
  registroOperaciones[registroIndex].encendido = encendido;
  registroOperaciones[registroIndex].duracion = duracion;
  
  registroIndex = (registroIndex + 1) % MAX_REGISTROS;
  if(totalOperacionesHist < MAX_REGISTROS) totalOperacionesHist++;
}

void actualizarOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
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

  display.setCursor(0, 8);
  String linea2 = String(voltageActual, 2) + "V ";
  linea2 += estado.modoAutomatico ? "(Auto)" : "(Manual)";
  display.println(linea2);

  for(uint8_t i = 0; i < 2; i++) {
    uint8_t idx = (logIndex + MAX_LOG_ENTRIES - 2 + i) % MAX_LOG_ENTRIES;
    display.setCursor(0, 16 + (i * 8));
    display.println(logEntries[idx].mensaje);
  }
  
  display.display();
}

void reiniciarContadoresDiariosSiNecesario() {
  static int diaAnterior = -1;
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    int diaActual = timeinfo.tm_mday;
    if (diaAnterior == -1) {
      diaAnterior = diaActual;
    } else if (diaActual != diaAnterior) {
      tiempoTotalEncendidoDia = 0;
      totalOperacionesDia = 0;
      diaAnterior = diaActual;
      agregarLog("Nuevo día - reset diario");
    }
  }
}