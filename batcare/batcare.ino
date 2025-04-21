#define ANALOG_PIN A0
#define LED_PIN D4  // Pin GPIO2 (D4) para un LED indicador
#define UMBRAL_ADC 805  // Valor ADC correspondiente a 11V

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);  // Apaga el LED inicialmente
}

void loop() {
  // Leer el valor analógico (0-1023)
  int rawValue = analogRead(ANALOG_PIN);

  // Convertir el valor analógico a voltaje real
  float voltage = rawValue * (14.0 / 1023.0);  // Ajusta 14.0 según tu rango máximo

  // Mostrar el voltaje en el monitor serial
  Serial.print("Voltaje de la batería: ");
  Serial.println(voltage);

  // Verificar si el voltaje está por debajo de 11V
  if (rawValue < UMBRAL_ADC) {
    Serial.println("¡Alerta! Voltaje por debajo de 11V");
    digitalWrite(LED_PIN, HIGH);  // Enciende el LED
  } else {
    digitalWrite(LED_PIN, LOW);  // Apaga el LED
  }

  delay(1000);  // Esperar 1 segundo antes de la próxima lectura
}
