#include <WiFi.h>
#include <PubSubClient.h>
#include "DHTesp.h"

// --- Pin Definitionen ---
const int DHT_PIN = 15;
const int SWITCH_PIN = 4;
const int LED_PIN = 2;

DHTesp dhtSensor;

// --- Netzwerk & MQTT ---
// Wokwi stellt ein virtuelles Gast-WLAN ohne Passwort zur Verfügung
const char* ssid = "Wokwi-GUEST"; 
const char* password = "";
const char* mqtt_server = "broker.hivemq.com"; // Öffentlicher Test-Broker

WiFiClient espClient;
PubSubClient client(espClient);

// --- Globale Variablen (für Task-Kommunikation) ---
float currentTemp = 0.0;
bool emergencyStop = false;
SemaphoreHandle_t dataMutex; // Mutex schützt die Variablen vor gleichzeitigem Zugriff

// --- FreeRTOS Task Definitionen ---
void TaskReadSensors(void *pvParameters);
void TaskMQTT(void *pvParameters);
void TaskAlarmControl(void *pvParameters);

void setup() {
  Serial.begin(115200);
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);

  // WLAN verbinden
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  client.setServer(mqtt_server, 1883);

  // FreeRTOS Mutex erstellen
  dataMutex = xSemaphoreCreateMutex();

  // FreeRTOS Tasks starten
  // xTaskCreate(Funktion, Name, Stack-Größe, Parameter, Priorität, Task-Handle)
  xTaskCreate(TaskReadSensors, "SensorTask", 2048, NULL, 1, NULL);
  xTaskCreate(TaskMQTT, "MQTTTask", 4096, NULL, 2, NULL);
  xTaskCreate(TaskAlarmControl, "AlarmTask", 1024, NULL, 3, NULL);
}

void loop() {
  // Der Loop bleibt leer, FreeRTOS übernimmt die Steuerung
}

// ---------------------------------------------------------
// Task 1: Sensoren auslesen
void TaskReadSensors(void *pvParameters) {
  for (;;) {
    TempAndHumidity data = dhtSensor.getTempAndHumidity();
    bool swState = digitalRead(SWITCH_PIN) == LOW;

    // Mutex sperren, Daten updaten, Mutex freigeben
    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
      currentTemp = data.temperature;
      emergencyStop = swState;
      xSemaphoreGive(dataMutex);
    }
    vTaskDelay(2000 / portTICK_PERIOD_MS); // 2 Sekunden warten
  }
}

// ---------------------------------------------------------
// Task 2: MQTT Kommunikation (Senden an die Leitstelle)
void TaskMQTT(void *pvParameters) {
  for (;;) {
    if (!client.connected()) {
      client.connect("WokwiESP32_Hikmat_Gateway"); // Eindeutige ID vergeben
    }
    client.loop();

    float tempToSend;
    bool stopToSend;

    // Daten sicher auslesen
    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
      tempToSend = currentTemp;
      stopToSend = emergencyStop;
      xSemaphoreGive(dataMutex);
    }

    // JSON String bauen und senden
    String payload = "{\"Temperature\": " + String(tempToSend) + ", \"EmergencyStop\": " + String(stopToSend) + "}";
    client.publish("hikmat/factory/conveyor1/status", payload.c_str());
    Serial.println("Gesendet: " + payload);

    vTaskDelay(5000 / portTICK_PERIOD_MS); // Alle 5 Sekunden senden
  }
}

// ---------------------------------------------------------
// Task 3: Lokale Alarmsteuerung in Echtzeit
void TaskAlarmControl(void *pvParameters) {
  for (;;) {
    float checkTemp;
    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
      checkTemp = currentTemp;
      xSemaphoreGive(dataMutex);
    }

    // Wenn Temperatur über 30 Grad, Alarm-LED an
    if (checkTemp > 30.0) {
      digitalWrite(LED_PIN, HIGH);
    } else {
      digitalWrite(LED_PIN, LOW);
    }
    vTaskDelay(500 / portTICK_PERIOD_MS); // Sehr schnelle Reaktionszeit (500ms)
  }
}
