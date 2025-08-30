#include <Wire.h>

const int MPU_ADDR = 0x68;

// Calibration and filtering variables
float acc_xcal, acc_ycal, acc_zcal;
float gyro_xcal, gyro_ycal, gyro_zcal;
float acc_x_out, acc_y_out, acc_z_out;
float gyro_x_out, gyro_y_out, gyro_z_out;
int16_t rawTemp;  // Added for temperature reading

// Button variables
int lastButtonState = HIGH;
bool loggingActive = false;
long int tempCount = 0;

void setup() {
  pinMode(0, INPUT_PULLUP);  // Use GPIO0 (D3 on NodeMCU)
  Wire.begin(4, 5);  // Specify I2C pins: SDA GPIO4 (D2), SCL GPIO5 (D1)
  Wire.setClock(400000);  // Set I2C clock to 400kHz
  writeByte(MPU_ADDR, 0x6B, 0);  // Wake up MPU-6050
  writeByte(MPU_ADDR, 0x1C, 0x00);  // Set ACCEL_CONFIG to ±2g
  writeByte(MPU_ADDR, 0x1B, 0x00);  // Set GYRO_CONFIG to ±250dps
  delay(100);  // Allow time for configuration
  Serial.begin(115200);  // Increased baud rate for faster data transfer

  // Initialize calibration variables
  acc_xcal = acc_ycal = acc_zcal = 0;
  gyro_xcal = gyro_ycal = gyro_zcal = 0;

  Serial.println("Starting calibration... Keep sensor flat and still!");
  delay(2000);

  // Calibration loop (500 samples)
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

  // Calculate average offsets
  gyro_xcal /= 500;
  gyro_ycal /= 500;
  gyro_zcal /= 500;
  acc_xcal /= 500;
  acc_ycal /= 500;
  acc_zcal /= 500;

  // Adjust for gravity (1g in Z-axis)
  acc_zcal -= 1.0;

  Serial.println("Calibration complete!");
}

void readAndConvert() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);  // Start with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14, true);  // Request 14 bytes

  // Read and convert accelerometer data
  acc_x_out = (Wire.read() << 8 | Wire.read()) / 16384.0;  // X-axis
  acc_y_out = (Wire.read() << 8 | Wire.read()) / 16384.0;  // Y-axis
  acc_z_out = (Wire.read() << 8 | Wire.read()) / 16384.0;  // Z-axis

  // Read temperature
  rawTemp = Wire.read() << 8 | Wire.read();

  // Read and convert gyroscope data
  gyro_x_out = (Wire.read() << 8 | Wire.read()) / 131.0;  // X-axis
  gyro_y_out = (Wire.read() << 8 | Wire.read()) / 131.0;  // Y-axis
  gyro_z_out = (Wire.read() << 8 | Wire.read()) / 131.0;  // Z-axis
}

void calibrate() {
  // Apply calibration
  acc_x_out -= acc_xcal;
  acc_y_out -= acc_ycal;
  acc_z_out -= acc_zcal;
  gyro_x_out -= gyro_xcal;
  gyro_y_out -= gyro_ycal;
  gyro_z_out -= gyro_zcal;

  // Apply low-pass filter
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
  int currentButtonState = digitalRead(0);  // Use GPIO0
  
  // Detect button press (falling edge)
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    loggingActive = true;
    tempCount = 0;  // Reset counter for new press
  }

  // Detect button release (rising edge)
  if (currentButtonState == HIGH && lastButtonState == LOW) {
    loggingActive = false;
    Serial.print("Total samples: ");
    Serial.println(tempCount);
  }

  // Continuous logging while button is pressed
  if (loggingActive) {
    readAndConvert();
    calibrate();
    
    // Calculate temperature in Celsius
    float tempC = rawTemp / 340.0 + 36.53;
    
    // Log data with timestamp
//    Serial.print(millis());
//    Serial.print(",");
Serial.print(acc_x_out, 4);
Serial.print(",");
Serial.print(acc_y_out, 4);
Serial.print(",");
Serial.print(acc_z_out, 4);
Serial.print(",");
Serial.print(gyro_x_out, 4);
Serial.print(",");
Serial.print(gyro_y_out, 4);
Serial.print(",");
Serial.println(gyro_z_out, 4);

 
    
    tempCount++;
  }

  lastButtonState = currentButtonState;
  delay(20);  // Maintain 50Hz sampling rate
}

void writeByte(uint8_t address, uint8_t reg, uint8_t data) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(data);
  Wire.endTransmission();
}
