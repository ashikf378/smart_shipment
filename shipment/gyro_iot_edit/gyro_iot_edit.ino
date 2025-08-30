#include <Wire.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// WiFi and MQTT Configuration
const char* ssid = "As";
const char* password = "Ashikf378@!";
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_topic = "sensor/mpu6050/data";

WiFiClient espClient;
PubSubClient client(espClient);

// MPU Configuration
const int MPU_ADDR = 0x68;
float acc_xcal, acc_ycal, acc_zcal;
float gyro_xcal, gyro_ycal, gyro_zcal;
float acc_x_out, acc_y_out, acc_z_out;
float gyro_x_out, gyro_y_out, gyro_z_out;
int16_t rawTemp;

// Button variables
int lastButtonState = HIGH;
bool loggingActive = false;
long int tempCount = 0;

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
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  pinMode(0, INPUT_PULLUP);
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  
  Wire.begin(4, 5);
  Wire.setClock(400000);
  writeByte(MPU_ADDR, 0x6B, 0);
  writeByte(MPU_ADDR, 0x1C, 0x00);
  writeByte(MPU_ADDR, 0x1B, 0x00);
  delay(100);

  // Calibration initialization
  acc_xcal = acc_ycal = acc_zcal = 0;
  gyro_xcal = gyro_ycal = gyro_zcal = 0;

  Serial.println("Starting calibration... Keep sensor flat and still!");
  delay(2000);

  for (int i = 0; i < 500; i++) {
    readAndConvert();
    gyro_xcal += gyro_x_out;
    gyro_ycal += gyro_y_out;
    gyro_zcal += gyro_z_out;
    acc_xcal += acc_x_out;
    acc_ycal += acc_y_out;
    acc_zcal += acc_z_out;
    delay(10);
  }

  gyro_xcal /= 500;
  gyro_ycal /= 500;
  gyro_zcal /= 500;
  acc_xcal /= 500;
  acc_ycal /= 500;
  acc_zcal /= 500;
  acc_zcal -= 1.0;

  Serial.println("Calibration complete!");
}

void readAndConvert() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14, true);

  acc_x_out = (Wire.read() << 8 | Wire.read()) / 16384.0;
  acc_y_out = (Wire.read() << 8 | Wire.read()) / 16384.0;
  acc_z_out = (Wire.read() << 8 | Wire.read()) / 16384.0;
  rawTemp = Wire.read() << 8 | Wire.read();
  gyro_x_out = (Wire.read() << 8 | Wire.read()) / 131.0;
  gyro_y_out = (Wire.read() << 8 | Wire.read()) / 131.0;
  gyro_z_out = (Wire.read() << 8 | Wire.read()) / 131.0;
}

void calibrate() {
  acc_x_out -= acc_xcal;
  acc_y_out -= acc_ycal;
  acc_z_out -= acc_zcal;
  gyro_x_out -= gyro_xcal;
  gyro_y_out -= gyro_ycal;
  gyro_z_out -= gyro_zcal;

  static float filtered_acc_x, filtered_acc_y, filtered_acc_z;
  static float filtered_gyro_x, filtered_gyro_y, filtered_gyro_z;
  
  filtered_acc_x = 0.3 * acc_x_out + 0.7 * filtered_acc_x;
  filtered_acc_y = 0.3 * acc_y_out + 0.7 * filtered_acc_y;
  filtered_acc_z = 0.3 * acc_z_out + 0.7 * filtered_acc_z;
  filtered_gyro_x = 0.3 * gyro_x_out + 0.7 * filtered_gyro_x;
  filtered_gyro_y = 0.3 * gyro_y_out + 0.7 * filtered_gyro_y;
  filtered_gyro_z = 0.3 * gyro_z_out + 0.7 * filtered_gyro_z;
  
  acc_x_out = filtered_acc_x;
  acc_y_out = filtered_acc_y;
  acc_z_out = filtered_acc_z;
  gyro_x_out = filtered_gyro_x;
  gyro_y_out = filtered_gyro_y;
  gyro_z_out = filtered_gyro_z;
}
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  int currentButtonState = digitalRead(0);
  
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    loggingActive = true;
    tempCount = 0;
  }

  if (currentButtonState == HIGH && lastButtonState == LOW) {
    loggingActive = false;
    Serial.print("Total samples: ");
    Serial.println(tempCount);
  }

  if (loggingActive) {
    readAndConvert();
    calibrate();
    
    // Create comma-separated numeric values
    String payload = String(acc_x_out, 4) + "," +
                    String(acc_y_out, 4) + "," +
                    String(acc_z_out, 4) + "," +
                    String(gyro_x_out, 4) + "," +
                    String(gyro_y_out, 4) + "," +
                    String(gyro_z_out, 4);

    // Publish to MQTT
    client.publish(mqtt_topic, payload.c_str());
    
    tempCount++;
  }

  lastButtonState = currentButtonState;
  delay(20);
}

void writeByte(uint8_t address, uint8_t reg, uint8_t data) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(data);
  Wire.endTransmission();
}
