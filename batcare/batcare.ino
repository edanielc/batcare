#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128    // Ancho de la pantalla OLED
#define SCREEN_HEIGHT 32    // Alto de la pantalla OLED

// Crear objeto para la pantalla OLED
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define ANALOG_PIN A0
#define RELAY_PIN D5        // Pin GPIO14 (D5) para controlar el relay
#define UMBRAL_ALTO_ADC 805 // Valor ADC correspondiente a ~14.1V
#define UMBRAL_BAJO_ADC 635 // Valor ADC correspondiente a ~11V

bool bombaEncendida = false; // Estado actual de la bomba

void setup() {
  Serial.begin(115200); // Inicializar comunicación serial

  // Inicializar la pantalla OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Dirección I2C 0x3C
    Serial.println(F("Fallo al iniciar la pantalla OLED"));
    for (;;); // Detener el programa si falla
  }

  // Limpiar la pantalla y configurar texto
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Apaga el relay inicialmente (invierte HIGH/LOW según tu relé)
}

void loop() {
  // Leer el valor analógico (0-1023)
  int rawValue = analogRead(ANALOG_PIN);

  // Convertir el valor analógico a voltaje real
  float voltage = rawValue * (14.1 / 805.0); // Ajusta 14.0 según tu rango máximo

  // Control de la bomba con histéresis
  if (!bombaEncendida && rawValue >= UMBRAL_ALTO_ADC) {
    // Encender la bomba si está apagada y el voltaje supera 14V
    digitalWrite(RELAY_PIN, LOW); // Activa el relay (cambia LOW/HIGH según tu relé)
    bombaEncendida = true;
  } else if (bombaEncendida && rawValue <= UMBRAL_BAJO_ADC) {
    // Apagar la bomba si está encendida y el voltaje cae por debajo de 11V
    digitalWrite(RELAY_PIN, HIGH); // Desactiva el relay (cambia HIGH/LOW según tu relé)
    bombaEncendida = false;
  }

  // Mostrar datos en el monitor serial
  Serial.print("Voltaje: ");
  Serial.print(voltage);
  Serial.print(" V, ADC: ");
  Serial.print(rawValue);
  Serial.print(", Bomba: ");
  Serial.println(bombaEncendida ? "ON" : "OFF");

  // Mostrar datos en la pantalla OLED
  display.clearDisplay(); // Limpiar la pantalla antes de escribir nuevos datos
  display.setCursor(0, 0); // Posición inicial (fila 0)
//
  display.print("V:");
  display.print(voltage);

  display.setCursor(0, 19); // Segunda línea
  display.print("S:");
  display.print(rawValue);
  display.print("|");
  display.print(bombaEncendida ? "ON" : "OFF");

  display.display(); // Actualizar la pantalla para mostrar los datos

  delay(1000); // Esperar 1 segundo antes de la próxima lectura
}