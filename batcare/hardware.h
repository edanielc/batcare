#ifndef HARDWARE_H
#define HARDWARE_H

#include <Arduino.h>

void inicializarHardware();
uint16_t leerADCfiltrado();
void encenderBomba();
void apagarBomba();

#endif