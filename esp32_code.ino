#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <addons/TokenHelper.h>
#include <Adafruit_MLX90614.h>
#include <TinyGPS++.h>
#include <Wire.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define RXD2 16
#define TXD2 17
#define GPS_BAUD 9600
// Define WiFi credentials
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// Define Firebase API Key, Project ID, and user credentials
#define API_KEY ""
#define FIREBASE_PROJECT_ID ""
#define USER_EMAIL ""
#define USER_PASSWORD ""

// Define Firebase Data object, Firebase authentication, and configuration
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Initialize the MLX90614 sensor

Adafruit_MLX90614 mlx = Adafruit_MLX90614();
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);
HardwareSerial gpsSerial(2);
TinyGPSPlus gps;

// Déclaration des fonctions de tâches
void task1(void * parameter);
void task2(void * parameter);

void setup() {
  // Initialize serial communication for debugging
  Serial.begin(115200);
  
  // Initialize GPS serial communication
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);
   if (!accel.begin()) {
    Serial.println("Accéléromètre non trouvé. Vérifiez les connexions !");
    while (1);
  }

  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  // Print Firebase client version
  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  // Assign the API key
  config.api_key = API_KEY;

  // Assign the user sign-in credentials
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // Assign the callback function for the long-running token generation task
  config.token_status_callback = tokenStatusCallback;  
  // Begin Firebase with configuration and authentication
  Firebase.begin(&config, &auth);

  // Reconnect to Wi-Fi if necessary
  Firebase.reconnectWiFi(true);

  // Initialize the MLX90614 sensor
  mlx.begin();

  // Création de la tâche 1 sur le core 0
  xTaskCreatePinnedToCore(
    task1,    
    "Task1",  
    10000,    
    NULL,     
    1,        
    NULL,     
    0         
  );

  // Création de la tâche 2 sur le core 1
  xTaskCreatePinnedToCore(
    task2,   
    "Task2",  
    10000,    
    NULL,     
    1,        
    NULL,     
    1        
  );
}

void loop() {
}

void task1(void * parameter) {
  while (1) {
    while (gpsSerial.available() > 0) {
      if (gps.encode(gpsSerial.read())) {

       if (gps.location.isValid()) {
          // Define the path to the Firestore document for GPS data
          String gpsDocumentPath = "Esp/GPS";
          FirebaseJson gpsContent;
          float latitude = gps.location.lat();
          float longitude = gps.location.lng();
        Serial.print("Latitude: ");
        Serial.println(latitude, 6);
        Serial.print("Longitude: ");
        Serial.println(longitude, 6);

        // Set the 'Latitude' and 'Longitude' fields in the FirebaseJson object
       gpsContent.set("fields/Latitude/doubleValue", latitude);
       gpsContent.set("fields/Longitude/doubleValue", longitude);

          // Get latitude and longitude
         
          Serial.print("Update/Add GPS Data... ");

          // Use the patchDocument method to update Firestore document for GPS data
          if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", gpsDocumentPath.c_str(), gpsContent.raw(), "Latitude") &&
              Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", gpsDocumentPath.c_str(), gpsContent.raw(), "Longitude")) {
            Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
          } else {
            Serial.println(fbdo.errorReason());
          }
        } else {
          Serial.println("GPS data invalid.");
        }
      }
    }

    // Delay before the next reading
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}


void task2(void * parameter) {
  while (1) {
    // Define the path to the Firestore document for MLX90614 sensor
    String mlxDocumentPath = "Esp/MLX90614";

    // Create a FirebaseJson object for storing data for MLX90614
    FirebaseJson mlxContent;

    // Read temperature from the MLX90614 sensor
    float objectTempC = mlx.readObjectTempC();

    // Check if the value is valid (not NaN) for MLX90614
    if (!isnan(objectTempC)) {
      // Set the 'Temperature' field in the FirebaseJson object for MLX90614
      mlxContent.set("fields/Temperature/stringValue", String(objectTempC, 2));

      Serial.print("Update/Add MLX90614 Data... ");

      // Use the patchDocument method to update the Temperature Firestore document for MLX90614
      if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", mlxDocumentPath.c_str(), mlxContent.raw(), "Temperature")) {
        Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
      } else {
        Serial.println(fbdo.errorReason());
      }
    } else {
      Serial.println("Failed to read MLX90614 data.");
    }
    String maxDocumentPath = "Esp/MAX30102";

    // Create a FirebaseJson object for storing data for MAX30102
    FirebaseJson maxContent;

    // Read heart rate and oxygen saturation from the MAX30102 sensor
    int heartRate = random(60, 100); // Simulated heart rate (60-100 bpm)
    int oxygenSaturation = random(95, 100); // Simulated oxygen saturation (95-100%)

    // Set the 'HeartRate' and 'OxygenSaturation' fields in the FirebaseJson object for MAX30102
    maxContent.set("fields/HeartRate/stringValue", String(heartRate));
    maxContent.set("fields/OxygenSaturation/stringValue", String(oxygenSaturation));

    Serial.print("Update/Add MAX30102 Data... ");

    // Use the patchDocument method to update the HeartRate and OxygenSaturation Firestore document for MAX30102
    if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", maxDocumentPath.c_str(), maxContent.raw(), "HeartRate") && 
        Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", maxDocumentPath.c_str(), maxContent.raw(), "OxygenSaturation")) {
      Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
    } else {
      Serial.println(fbdo.errorReason());
    }
    String documentPath = "Esp/XYZ";
   FirebaseJson content;

  // Read accelerometer data
  sensors_event_t event;
  accel.getEvent(&event);

  // Store accelerometer data in FirebaseJson object
  content.set("fields/X/stringValue", String(event.acceleration.x));
  content.set("fields/Y/stringValue", String(event.acceleration.y));
  content.set("fields/Z/stringValue", String(event.acceleration.z));

  Serial.print("Update/Add Accelerometer Data... ");

  // Use patchDocument method to update Firestore collection
  if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw(), "X") &&
      Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw(), "Y") &&
      Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw(), "Z")) {
    Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
  } else {
    Serial.println(fbdo.errorReason());
  }

    // Read GPS data
    

    // Delay before the next reading
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
