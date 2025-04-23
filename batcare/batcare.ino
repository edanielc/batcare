#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define SCREEN_WIDTH 128    // Ancho de la pantalla OLED
#define SCREEN_HEIGHT 32    // Alto de la pantalla OLED

// Crear objeto para la pantalla OLED
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define ANALOG_PIN A0
#define RELAY_PIN D5        // Pin GPIO14 (D5) para controlar el relay
#define UMBRAL_ALTO_ADC 805 // Valor ADC correspondiente a ~14.1V
#define UMBRAL_BAJO_ADC 635 // Valor ADC correspondiente a ~11V

bool bombaEncendida = false; // Estado actual de la bomba

// Configuración de Wi-Fi
const char* ssid = "R-2G";       // Cambia por el nombre de tu red Wi-Fi
const char* password = "KRISANTEMOZ88"; // Cambia por la contraseña de tu red Wi-Fi

// Configuración NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -6 * 3600); // GMT-6 (ajusta según tu zona horaria)

unsigned long lastSwitchTime = 0; // Última vez que se cambió de pantalla
bool mostrarWiFi = true;          // Controla qué pantalla se muestra

void setup() {
  Serial.begin(115200); // Inicializar comunicación serial

  // Inicializar la pantalla OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Dirección I2C 0x3C
    Serial.println(F("Fallo al iniciar la pantalla OLED"));
    for (;;); // Detener el programa si falla
  }

  // Limpiar la pantalla y configurar texto
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Apaga el relay inicialmente

  // Conectar al Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Conectando a Wi-Fi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado a Wi-Fi");

  // Inicializar cliente NTP
  timeClient.begin();
}

void loop() {
  // Leer el valor analógico (0-1023)
  int rawValue = analogRead(ANALOG_PIN);

  // Convertir el valor analógico a voltaje real
  float voltage = rawValue * (14.1 / 805.0); // Ajusta según tu rango máximo

  // Obtener la hora actual
  timeClient.update();
  int horaActual = timeClient.getHours(); // Hora en formato 24 horas

  // Mostrar datos en el monitor serial
  Serial.print("Hora actual: ");
  Serial.print(horaActual);
  Serial.print(":");
  Serial.print(timeClient.getMinutes());
  Serial.print(", Voltaje: ");
  Serial.print(voltage);
  Serial.print(" V, ADC: ");
  Serial.print(rawValue);
  Serial.print(", Bomba: ");
  Serial.println(bombaEncendida ? "ON" : "OFF");

  // Control de la bomba con histéresis y validación de horario
  if (!bombaEncendida && rawValue >= UMBRAL_ALTO_ADC && horaActual >= 6 && horaActual < 16) {
    digitalWrite(RELAY_PIN, LOW); // Enciende el relay
    bombaEncendida = true;
  } else if (bombaEncendida && (rawValue <= UMBRAL_BAJO_ADC || horaActual < 6 || horaActual >= 16)) {
    digitalWrite(RELAY_PIN, HIGH); // Apaga el relay
    bombaEncendida = false;
  }

  // Alternar entre las dos pantallas cada 5 segundos
  unsigned long currentTime = millis();
  if (currentTime - lastSwitchTime >= 5000) { // Cambiar cada 5 segundos
    mostrarWiFi = !mostrarWiFi; // Alternar entre las pantallas
    lastSwitchTime = currentTime;
  }

  // Mostrar datos en la pantalla OLED
  display.clearDisplay();
  if (mostrarWiFi) {
    mostrarPantallaWiFi();
  } else {
    mostrarPantallaDatos(voltage, rawValue, bombaEncendida, horaActual);
  }
  display.display();

  delay(1000); // Esperar 1 segundo antes de la próxima lectura
}

void mostrarPantallaWiFi() {
  // Pantalla 1: Estado del Wi-Fi y calidad de la señal
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
}

void mostrarPantallaDatos(float voltage, int rawValue, bool bombaEncendida, int horaActual) {
  // Pantalla 2: Voltaje, valor ADC, estado de la bomba y hora actual
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
  display.print(horaActual);
  display.print(":");
  display.print(timeClient.getMinutes());
}