#include "oled.h"
#include <ESP8266WiFi.h>

Adafruit_SSD1306 display(128, 32, &Wire, -1);

void setupOLED() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Fallo al iniciar la pantalla OLED"));
    for (;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
}

void mostrarPantallaWiFi() {
  // Pantalla 1: Estado del Wi-Fi y calidad de la señal
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("WiFi: ");
  display.println(WiFi.SSID());
  display.setCursor(0, 10);
  display.print("IP: ");
  display.println(WiFi.localIP());
  display.setCursor(0, 20);
  display.print("RSSI: ");
  display.print(WiFi.RSSI());
  display.print(" dBm");
  display.display();
}

void mostrarDatosEnOLED(float voltage, int rawValue, bool bombaEncendida, int hora, int minutos) {
  // Pantalla 2: Voltaje, valor ADC, estado de la bomba y hora actual
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("V:");
  display.print(voltage);
  display.print(" V");

  display.setCursor(0, 10);
  display.print("S:");
  display.print(rawValue);
  display.print("|");
  display.print(bombaEncendida ? "ON" : "OFF");

  display.setCursor(0, 20);
  display.print("Hora: ");
  display.print(hora);
  display.print(":");
  display.print(minutos);
  display.display();
}