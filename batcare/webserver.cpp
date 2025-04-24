#include "webserver.h"
#include "bomba.h"
#include <ESP8266WiFi.h>

ESP8266WebServer server(80);

void setupWebServer() {
  server.on("/", []() {
    String html = "<!DOCTYPE html>";
    html += "<html lang='es'>";
    html += "<head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Control de Bomba</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; background-color: #f4f4f9; margin: 0; padding: 0; }";
    html += ".container { max-width: 600px; margin: 50px auto; padding: 20px; background: #fff; border-radius: 8px; box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1); }";
    html += "h1 { text-align: center; color: #333; }";
    html += "p { font-size: 18px; margin: 10px 0; }";
    html += "button { display: block; width: 100%; padding: 15px; margin: 10px 0; font-size: 18px; color: #fff; background-color: #007bff; border: none; border-radius: 5px; cursor: pointer; }";
    html += "button:hover { background-color: #0056b3; }";
    html += "</style>";
    html += "</head>";
    html += "<body>";
    html += "<div class='container'>";
    html += "<h1>Estado del Sistema</h1>";

    // Datos del sistema
    html += "<p><strong>Voltaje:</strong> " + String(voltageActual) + " V</p>";
    html += "<p><strong>ADC:</strong> " + String(rawValueActual) + "</p>";
    html += "<p><strong>Bomba:</strong> " + String(bombaEncendida ? "ON" : "OFF") + "</p>";

    // Datos de la conexión Wi-Fi
    html += "<p><strong>Wi-Fi SSID:</strong> " + String(WiFi.SSID()) + "</p>";
    html += "<p><strong>Dirección IP:</strong> " + WiFi.localIP().toString() + "</p>";
    html += "<p><strong>Señal Wi-Fi (RSSI):</strong> " + String(WiFi.RSSI()) + " dBm</p>";

    // Botones para controlar la bomba
    html += "<p><button onclick=\"window.location.href='/on'\">Encender Bomba</button></p>";
    html += "<p><button onclick=\"window.location.href='/off'\">Apagar Bomba</button></p>";
    html += "</div>";
    html += "</body>";
    html += "</html>";
    server.send(200, "text/html", html);
  });

  server.on("/on", []() {
    encenderBombaManualmente();
    server.sendHeader("Location", "/");
    server.send(303); // Redirige a la página principal
  });

  server.on("/off", []() {
    apagarBombaManualmente();
    server.sendHeader("Location", "/");
    server.send(303); // Redirige a la página principal
  });

  server.begin();
  Serial.println("Servidor web iniciado");
}

void handleWebServer(float voltage, int rawValue, bool bombaEncendida) {
  server.handleClient();
}