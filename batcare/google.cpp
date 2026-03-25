#include "google.h"
#include "globals.h"

void enviarGoogleSheets(String fecha, String hora, String estadoBomba, float voltaje, int adc, String comando) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Google Sheets: WiFi no conectado");
    return;
  }
  
  client.setInsecure();
  if (!client.connect("script.google.com", 443)) {
    Serial.println("Google Sheets: Error de conexión HTTPS");
    return;
  }
  
  String postData = "fecha=" + fecha + "&hora=" + hora + "&estado=" + estadoBomba +
                    "&voltaje=" + String(voltaje, 2) + "&valadc=" + String(adc) +
                    "&activacion=" + comando;
  
  String request = "POST " + String(GOOGLE_SCRIPT_URL) + " HTTP/1.1\r\n";
  request += "Host: script.google.com\r\n";
  request += "Content-Type: application/x-www-form-urlencoded\r\n";
  request += "Content-Length: " + String(postData.length()) + "\r\n";
  request += "Connection: close\r\n\r\n";
  request += postData;
  
  client.print(request);
  
  unsigned long timeout = millis() + 5000;
  while (client.available() == 0 && millis() < timeout);
  if (client.available()) {
    String resp = client.readString();
    if (resp.indexOf("OK") >= 0) {
      Serial.println("Google Sheets: Envío exitoso");
    } else {
      Serial.println("Google Sheets: Respuesta inesperada: " + resp.substring(0, 100));
    }
  } else {
    Serial.println("Google Sheets: Timeout esperando respuesta");
  }
  client.stop();
}