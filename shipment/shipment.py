import paho.mqtt.client as mqtt
import numpy as np
import torch
import torch.nn as nn
from sklearn.preprocessing import StandardScaler
import joblib
import os

# Define the model class
class ShipmentClassifier(nn.Module):
    def __init__(self, input_size, num_classes):
        super(ShipmentClassifier, self).__init__()
        self.fc1 = nn.Linear(input_size, 64)
        self.fc2 = nn.Linear(64, 32)
        self.fc3 = nn.Linear(32, num_classes)

    def forward(self, x):
        x = torch.relu(self.fc1(x))
        x = torch.relu(self.fc2(x))
        x = self.fc3(x) 
        return x

# Check if model file exists
model_path = 'shipment_model.pth'  # Adjust filename if necessary
if not os.path.exists(model_path):
    print(f"Model file {model_path} not found.")
    exit(1)

# Load the trained model
input_size = 12
num_classes = 5
model = ShipmentClassifier(input_size, num_classes)
try:
    # Use weights_only=False for compatibility (ensure you trust the model file)
    model.load_state_dict(torch.load(model_path, map_location='cpu', weights_only=False))
    model.eval()
except Exception as e:
    print(f"Error loading model: {e}")
    exit(1)

# Check if scaler file exists
scaler_path = 'scaler.pkl'
if not os.path.exists(scaler_path):
    print(f"Scaler file {scaler_path} not found.")
    exit(1)

# Load the scaler
try:
    scaler = joblib.load(scaler_path)
except Exception as e:
    print(f"Error loading scaler: {e}")
    exit(1)

# Buffer to store incoming sensor rows
buffer = []

# MQTT callback for when a message is received
def on_message(client, userdata, msg):
    global buffer
    payload = msg.payload.decode('utf-8').strip()
    try:
        values = list(map(float, payload.split(',')))
        if len(values) == 6:
            buffer.append(values)
            if len(buffer) == 50:
                # Extract features from the window
                data_array = np.array(buffer)
                acc = data_array[:, :3]
                gyro = data_array[:, 3:]
                features = np.concatenate([
                    acc.mean(axis=0), acc.std(axis=0),
                    gyro.mean(axis=0), gyro.std(axis=0)
                ])
                # Scale features
                features_scaled = scaler.transform([features])
                # Convert to tensor
                tensor = torch.tensor(features_scaled, dtype=torch.float32)
                # Inference
                with torch.no_grad():
                    output = model(tensor)
                    _, pred = torch.max(output, 1)
                    predicted_class = pred.item()
                # Print the prediction
                print(f"Predicted class: {predicted_class}")
                # Publish the prediction to another topic
                client.publish("sensor/mpu6050/prediction", str(predicted_class))
                # Clear the buffer
                buffer = []
        else:
            print("Invalid data length")
    except ValueError:
        print("Invalid data format")

# Set up MQTT client
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.on_message = on_message

# Connect to the broker
client.connect("broker.hivemq.com", 1883, 60)

# Subscribe to the topic
client.subscribe("sensor/mpu6050/data")

# Start the loop to listen for messages
client.loop_forever()