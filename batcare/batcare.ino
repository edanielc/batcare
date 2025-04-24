#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "webserver.h"
#include "oled.h"
#include "bomba.h"

// Configuración de Wi-Fi
//const char* ssid = "R-2G";       // Cambia por el nombre de tu red Wi-Fi
//const char* password = "KRISANTEMOZ88"; // Cambia por la contraseña de tu red Wi-Fi
const char* ssid = "BaristaLovers";       // Cambia por el nombre de tu red Wi-Fi
const char* password = "Baristalovers"; // Cambia por la contraseña de tu red Wi-Fi

// Configuración NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -6 * 3600); // GMT-6 (ajusta según tu zona horaria)

unsigned long lastSwitchTime = 0; // Última vez que se cambió de pantalla
bool mostrarWiFi = true;          // Controla qué pantalla se muestra

void setup() {
  Serial.begin(115200); // Inicializar comunicación serial

  // Inicializar Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Conectando a Wi-Fi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado a Wi-Fi");

  // Inicializar cliente NTP
  timeClient.begin();

  // Inicializar la pantalla OLED
  setupOLED();

  // Inicializar el servidor web
  setupWebServer();

  // Inicializar la lógica de la bomba
  setupBomba();
}

void loop() {
  // Actualizar la hora
  timeClient.update();

  // Leer el valor analógico (0-1023)
  int rawValue = analogRead(A0);

  // Convertir el valor analógico a voltaje real
  float voltage = rawValue * (14.1 / 805.0); // Ajusta según tu rango máximo

  // Control de la bomba
  actualizarBomba(rawValue, timeClient.getHours());

  // Log de consola
  Serial.print("Hora actual: ");
  Serial.print(timeClient.getHours());
  Serial.print(":");
  Serial.print(timeClient.getMinutes());
  Serial.print(", Voltaje: ");
  Serial.print(voltage);
  Serial.print(" V, ADC: ");
  Serial.print(rawValue);
  Serial.print(", Bomba: ");
  Serial.println(bombaEncendida ? "ON" : "OFF");

  // Alternar entre las dos pantallas cada 5 segundos
  unsigned long currentTime = millis();
  if (currentTime - lastSwitchTime >= 5000) { // Cambiar cada 5 segundos
    mostrarWiFi = !mostrarWiFi; // Alternar entre las pantallas
    lastSwitchTime = currentTime;
  }

  // Mostrar datos en la pantalla OLED
  if (mostrarWiFi) {
    mostrarPantallaWiFi();
  } else {
    mostrarDatosEnOLED(voltage, rawValue, bombaEncendida, timeClient.getHours(), timeClient.getMinutes());
  }

  // Manejar solicitudes del servidor web
  handleWebServer(voltage, rawValue, bombaEncendida);

  delay(1000); // Esperar 1 segundo antes de la próxima lectura
}