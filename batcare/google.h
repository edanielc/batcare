#ifndef GOOGLE_H
#define GOOGLE_H

#include <Arduino.h>

void enviarGoogleSheets(String fecha, String hora, String estadoBomba, float voltaje, int adc, String comando);

#endif