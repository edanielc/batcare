#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <WiFiClientSecure.h>
#include "wifi_config.h"
#include "google_config.h"

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
extern Adafruit_SSD1306 display;

// Hardware
extern const uint8_t RELAY_PIN;
extern const uint8_t ADC_PIN;

// NTP
extern const char* ntpServer;
extern const long gmtOffset_sec;
extern const int daylightOffset_sec;

// Configuración EEPROM
struct Configuracion {
  uint16_t umbralAlto;
  uint16_t umbralBajo;
  uint8_t  histeresis;
  uint32_t pausaMs;
  uint8_t  horaInicio;
  uint8_t  horaFin;
  float    factorVoltaje;
};
extern Configuracion config;
extern const int EEPROM_SIZE;
extern const int EEPROM_ADDR;

// Estado
struct Estado {
  bool bombaEncendida : 1;
  bool enPausa : 1;
  bool modoAutomatico : 1;
  bool errorWiFi : 1;
  bool errorReloj : 1;
};
extern Estado estado;

// Variables temporales
extern uint32_t tiempoInicioPausa;
extern uint32_t tiempoUltimoEncendido;
extern uint32_t tiempoTotalEncendido;
extern uint32_t tiempoTotalEncendidoDia;
extern uint16_t totalOperaciones;
extern uint16_t totalOperacionesDia;
extern float voltageActual;
extern uint16_t rawValue;

// Filtro ADC
extern const uint8_t NUM_LECTURAS;
extern uint16_t lecturasADC[];
extern uint8_t indiceLectura;
extern uint32_t totalLecturas;

// Log
extern const uint8_t MAX_LOG_ENTRIES;
struct LogEntry {
  char mensaje[21];
};
extern LogEntry logEntries[];
extern uint8_t logIndex;

// Registro de operaciones
extern const uint8_t MAX_REGISTROS;
struct RegistroOperacion {
  time_t timestamp;
  bool encendido;
  uint32_t duracion;
};
extern RegistroOperacion registroOperaciones[];
extern uint8_t registroIndex;
extern uint16_t totalOperacionesHist;

// Web server
extern ESP8266WebServer server;

// Google
extern WiFiClientSecure client;
extern uint32_t ultimoEnvioGoogle;
extern const uint32_t INTERVALO_GOOGLE;

#endif