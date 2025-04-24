#ifndef OLED_H
#define OLED_H

#include <Adafruit_SSD1306.h>

extern Adafruit_SSD1306 display;

void setupOLED();
void mostrarDatosEnOLED(float voltage, int rawValue, bool bombaEncendida, int hora, int minutos);
void mostrarPantallaWiFi(); // Añade esta línea

#endif