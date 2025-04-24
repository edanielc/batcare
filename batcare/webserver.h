#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <ESP8266WebServer.h>

extern ESP8266WebServer server;

void setupWebServer();
void handleWebServer(float voltage, int rawValue, bool bombaEncendida);

#endif