#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>

String formatDuration(uint32_t ms);
String formatTime(time_t t);
String formatDate(time_t t);
void agregarLog(const char* mensaje);
void agregarRegistro(bool encendido, uint32_t duracion = 0);
void actualizarOLED();
void reiniciarContadoresDiariosSiNecesario();

#endif