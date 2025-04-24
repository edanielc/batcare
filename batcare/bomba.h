#ifndef BOMBA_H
#define BOMBA_H

extern bool bombaEncendida;
extern float voltageActual;
extern int rawValueActual;

void setupBomba();
void actualizarBomba(int rawValue, int horaActual);
void encenderBombaManualmente();
void apagarBombaManualmente();

#endif