#include "webserver.h"
#include "bomba.h"

ESP8266WebServer server(80);

void setupWebServer() {
  server.on("/", []() {
    String html = "<html><body>";
    html += "<h1>Estado del Sistema</h1>";
    html += "<p>Voltaje: " + String(voltageActual) + " V</p>";
    html += "<p>ADC: " + String(rawValueActual) + "</p>";
    html += "<p>Bomba: " + String(bombaEncendida ? "ON" : "OFF") + "</p>";
    html += "<p><a href='/on'><button>Encender Bomba</button></a></p>";
    html += "<p><a href='/off'><button>Apagar Bomba</button></a></p>";
    html += "</body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/on", []() {
    encenderBombaManualmente();
    server.send(200, "text/plain", "Bomba encendida");
  });

  server.on("/off", []() {
    apagarBombaManualmente();
    server.send(200, "text/plain", "Bomba apagada");
  });

  server.begin();
  Serial.println("Servidor web iniciado");
}

void handleWebServer(float voltage, int rawValue, bool bombaEncendida) {
  server.handleClient();
}