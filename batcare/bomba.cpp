#include "bomba.h"
#include <Arduino.h>

#define RELAY_PIN 14 // GPIO14 (equivalente a D5 en NodeMCU)
#define UMBRAL_ALTO_ADC 805
#define UMBRAL_BAJO_ADC 635

bool bombaEncendida = false;
float voltageActual = 0;
int rawValueActual = 0;

void setupBomba() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Apaga el relay inicialmente
}

void actualizarBomba(int rawValue, int horaActual) {
  rawValueActual = rawValue;
  voltageActual = rawValue * (14.1 / 805.0);

  if (!bombaEncendida && rawValue >= UMBRAL_ALTO_ADC && horaActual >= 6 && horaActual < 16) {
    digitalWrite(RELAY_PIN, LOW); // Enciende el relay
    bombaEncendida = true;
  } else if (bombaEncendida && (rawValue <= UMBRAL_BAJO_ADC || horaActual < 6 || horaActual >= 16)) {
    digitalWrite(RELAY_PIN, HIGH); // Apaga el relay
    bombaEncendida = false;
  }
}

void encenderBombaManualmente() {
  digitalWrite(RELAY_PIN, LOW);
  bombaEncendida = true;
}

void apagarBombaManualmente() {
  digitalWrite(RELAY_PIN, HIGH);
  bombaEncendida = false;
}