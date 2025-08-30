#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// MQTT Broker settings
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* data_topic = "gyro/data";
const char* prediction_topic = "gyro/prediction";

// MPU6050 I2C address
const int MPU_ADDR = 0x68;

// Sensor calibration variables
float acc_x_cal, acc_y_cal, acc_z_cal;
float gyro_x_cal, gyro_y_cal, gyro_z_cal;

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe(prediction_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  
  // Initialize I2C communication
  Wire.begin();
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);  // PWR_MGMT_1 register
  Wire.write(0);     // set to zero (wakes up the MPU-6050)
  Wire.endTransmission(true);
  
  // Calibrate sensor
  calibrate_sensor();
  
  Serial.println("Setup complete");
}

void calibrate_sensor() {
  Serial.println("Calibrating sensor...");
  float acc_x_sum = 0, acc_y_sum = 0, acc_z_sum = 0;
  float gyro_x_sum = 0, gyro_y_sum = 0, gyro_z_sum = 0;
  
  // Take 500 samples for calibration
  for (int i = 0; i < 500; i++) {
    read_sensor();
    acc_x_sum += acc_x;
    acc_y_sum += acc_y;
    acc_z_sum += acc_z;
    gyro_x_sum += gyro_x;
    gyro_y_sum += gyro_y;
    gyro_z_sum += gyro_z;
    delay(2);
  }
  
  // Calculate average offsets
  acc_x_cal = acc_x_sum / 500;
  acc_y_cal = acc_y_sum / 500;
  acc_z_cal = acc_z_sum / 500 - 1.0; // Remove gravity from Z-axis
  gyro_x_cal = gyro_x_sum / 500;
  gyro_y_cal = gyro_y_sum / 500;
  gyro_z_cal = gyro_z_sum / 500;
  
  Serial.println("Calibration complete");
}

float acc_x, acc_y, acc_z;
float gyro_x, gyro_y, gyro_z;

void read_sensor() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);  // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14, true);  // request a total of 14 registers
  
  // Read accelerometer data
  acc_x = (Wire.read() << 8 | Wire.read()) / 16384.0;
  acc_y = (Wire.read() << 8 | Wire.read()) / 16384.0;
  acc_z = (Wire.read() << 8 | Wire.read()) / 16384.0;
  
  // Read temperature (not used but needed to advance pointer)
  Wire.read() << 8 | Wire.read();
  
  // Read gyroscope data
  gyro_x = (Wire.read() << 8 | Wire.read()) / 131.0;
  gyro_y = (Wire.read() << 8 | Wire.read()) / 131.0;
  gyro_z = (Wire.read() << 8 | Wire.read()) / 131.0;
  
  // Apply calibration
  acc_x -= acc_x_cal;
  acc_y -= acc_y_cal;
  acc_z -= acc_z_cal;
  gyro_x -= gyro_x_cal;
  gyro_y -= gyro_y_cal;
  gyro_z -= gyro_z_cal;
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  // Read sensor data
  read_sensor();
  
  // Create JSON payload
  String payload = "{";
  payload += "\"acc_x\":" + String(acc_x, 4) + ",";
  payload += "\"acc_y\":" + String(acc_y, 4) + ",";
  payload += "\"acc_z\":" + String(acc_z, 4) + ",";
  payload += "\"gyro_x\":" + String(gyro_x, 4) + ",";
  payload += "\"gyro_y\":" + String(gyro_y, 4) + ",";
  payload += "\"gyro_z\":" + String(gyro_z, 4);
  payload += "}";
  
  // Publish to MQTT
  client.publish(data_topic, payload.c_str());
  
  // Print to serial for debugging
  Serial.println(payload);
  
  // Delay to control sampling rate (adjust as needed)
  delay(100);
}
