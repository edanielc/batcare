#include "hardware.h"
#include "globals.h"
#include "utils.h"
#include "google.h"

void inicializarHardware() {
  for(uint8_t i=0; i<NUM_LECTURAS; i++) {
    lecturasADC[i] = analogRead(ADC_PIN);
    totalLecturas += lecturasADC[i];
  }
  agregarLog("ADC iniciado");

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  agregarLog("Hardware listo");
}

uint16_t leerADCfiltrado() {
  totalLecturas -= lecturasADC[indiceLectura];
  lecturasADC[indiceLectura] = analogRead(ADC_PIN);
  totalLecturas += lecturasADC[indiceLectura];
  indiceLectura = (indiceLectura + 1) % NUM_LECTURAS;
  return totalLecturas / NUM_LECTURAS;
}

void encenderBomba() {
  digitalWrite(RELAY_PIN, HIGH); //Penciente pin D5
  estado.bombaEncendida = true;
  tiempoUltimoEncendido = millis();
  agregarRegistro(true);
  agregarLog("Bomba ENCENDIDA");
  
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
  digitalWrite(RELAY_PIN, LOW); //Apaga pin D5
  if(estado.bombaEncendida) {
    uint32_t duracion = millis() - tiempoUltimoEncendido;
    tiempoTotalEncendido += duracion;
    tiempoTotalEncendidoDia += duracion;
    totalOperaciones++;
    totalOperacionesDia++;
    agregarRegistro(false, duracion);
    agregarLog("Bomba APAGADA");
    
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