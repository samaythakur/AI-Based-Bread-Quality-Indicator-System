# AI-Based Bread Quality Indicator System
# FUNCTION: Receives image from ESP32-CAM → AI analyzes bread condition →returns result as FRESH or MOLDY.
# MAIN FEATURES:
# - Flask server for communication
# - MobileNetV2 transfer learning model
# - Thread-safe inference
# - Automatic model loading/training
# - REST API endpoints

import os
import io
import socket
import threading
import numpy as np
try:
    import tensorflow as tf
    from tensorflow import keras
    from tensorflow.keras import layers
    from tensorflow.keras.preprocessing.image import ImageDataGenerator
    from tensorflow.keras.callbacks import EarlyStopping
    from tensorflow.keras.applications import MobileNetV2
    from tensorflow.keras.optimizers import Adam
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from PIL import Image
    try:
        from flask import Flask, request, jsonify
        from flask_cors import CORS
    except ImportError:
        print("\nFlask or flask-cors is NOT installed.")
        print("Run: pip install flask flask-cors")
        exit(1)
except ImportError as e:
    print("\nMissing library:", e)
    print("Run: pip install flask flask-cors tensorflow pillow numpy matplotlib")
    exit(1)

# CONFIGURATION
DATASET_DIR =  r"C:\Users\samay\OneDrive\Desktop\COLLEGE\4-Project\python-projects\P2-AI-Based Bread Quality Indicator System\dataset_bread_indicator"
IMG_SIZE    = (128, 128)
BATCH_SIZE  = 16
EPOCHS      = 15
SERVER_PORT = 5000
MODEL_PATH  = "bread_model.h5"

# Prevents multiple requests from accessing model simultaneously
predict_lock = threading.Lock()

# MODEL TRAINING FUNCTION
def train_model():
    print("Training model...")
    # Data augmentation improves model generalization
    datagen = ImageDataGenerator(
        rescale=1.0/255,
        validation_split=0.2,
        rotation_range=10,
        width_shift_range=0.1,
        height_shift_range=0.1,
        horizontal_flip=True,
        zoom_range=0.1
    )
    # Training dataset
    train_gen = datagen.flow_from_directory(
        DATASET_DIR,
        target_size=IMG_SIZE,
        batch_size=BATCH_SIZE,
        class_mode='binary',
        subset='training'
    )
    # Validation dataset
    val_gen = datagen.flow_from_directory(
        DATASET_DIR,
        target_size=IMG_SIZE,
        batch_size=BATCH_SIZE,
        class_mode='binary',
        subset='validation'
    )
    # Pretrained CNN model for feature extraction
    base_model = MobileNetV2(
        input_shape=(128, 128, 3),
        include_top=False,
        weights='imagenet'
    )
    # Freeze pretrained layers
    base_model.trainable = False
    # Custom classification model
    model = keras.Sequential([
        base_model,
        layers.GlobalAveragePooling2D(),
        layers.Dense(64, activation='relu'),
        layers.Dropout(0.5),
        layers.Dense(1, activation='sigmoid')
    ])
    # Compile model
    model.compile(
        optimizer=Adam(learning_rate=0.0001),
        loss='binary_crossentropy',
        metrics=['accuracy']
    )
    # Stops training if validation does not improve
    early_stop = EarlyStopping(
        patience=3,
        restore_best_weights=True
    )
    # Train model
    history = model.fit(
        train_gen,
        validation_data=val_gen,
        epochs=EPOCHS,
        callbacks=[early_stop]
    )
    # Plot training accuracy and loss
    plt.figure(figsize=(10, 4))
    plt.subplot(1, 2, 1)
    plt.plot(history.history['accuracy'], label='Train')
    plt.plot(history.history['val_accuracy'], label='Validation')
    plt.title('Accuracy')
    plt.legend()
    plt.subplot(1, 2, 2)
    plt.plot(history.history['loss'], label='Train')
    plt.plot(history.history['val_loss'], label='Validation')
    plt.title('Loss')
    plt.legend()
    plt.tight_layout()
    plt.savefig("training_results.png")

    # Save trained model
    model.save(MODEL_PATH)
    print("Model saved to", MODEL_PATH)
    return model

# FLASK SERVER
app = Flask(__name__)
# Enables communication from browser and ESP32
CORS(app)
model = None

# IMAGE ANALYSIS API
# Receives image from ESP32-CAM using POST request
# Returns:- FRESH or MOLDY - Prediction confidence

@app.route('/analyse', methods=['POST'])
def analyse():
    try:
        # ===== IMAGE ACQUISITION =====

        # OPTION 1 → image uploaded from web app
        if 'image' in request.files:

            file = request.files['image']

            img = Image.open(file.stream).convert('RGB')

        # OPTION 2 → image from ESP32-CAM
        else:

            img_bytes = request.data

            if not img_bytes:
                return jsonify({
                    'status': 'error',
                    'message': 'No image received'
                }), 400

            # Convert received bytes into image
            img = Image.open(io.BytesIO(img_bytes)).convert('RGB')

        # ===== IMAGE PREPROCESSING =====

        img = img.resize(IMG_SIZE)

        x = np.array(img) / 255.0

        x = np.expand_dims(x, axis=0)

        # ===== AI PREDICTION =====

        with predict_lock:
            pred = float(model.predict(x, verbose=0)[0][0])

        # ===== RESULT CLASSIFICATION =====

        if pred > 0.25:

            result = "MOLDY"

            confidence = round(pred * 100, 1)

        else:

            result = "FRESH"

            confidence = round((1.0 - pred) * 100, 1)

        print(f"[analyse] pred={pred:.4f} → {result} ({confidence}%)")

        # ===== SEND RESPONSE =====

        return jsonify({
            'status': 'ok',
            'result': result,
            'confidence': confidence
        })

    except Exception as e:

        print("Error in /analyse:", e)

        return jsonify({
            'status': 'error',
            'message': str(e)
        }), 500

# SERVER STATUS CHECK
@app.route('/ping')
def ping():
    return jsonify({
        'status': 'ok'
    })

# HOME PAGE
@app.route('/')
def index():
    return "Bread AI Server Running — POST a JPEG to /analyse"

# MAIN PROGRAM
if __name__ == '__main__':
    print("Starting Bread Quality AI System...")

    # Load saved model if available
    if os.path.exists(MODEL_PATH):
        print("Loading saved model from", MODEL_PATH)
        model = tf.keras.models.load_model(MODEL_PATH)

        # Warmup inference to reduce first prediction delay
        dummy = np.zeros((1, 128, 128, 3), dtype=np.float32)
        model.predict(dummy, verbose=0)
        print("Model warmed up and ready.")
    else:
        # Train model if no saved model exists
        print("No saved model found. Training from dataset...")
        model = train_model()

    # Detect local IP address
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.connect(("8.8.8.8", 80))
    ip = s.getsockname()[0]
    s.close()
    print(f"\n{'='*50}")
    print(f" Flask server running at: http://{ip}:{SERVER_PORT}")
    print(f" Paste this IP into Arduino: LAPTOP_IP = \"{ip}\"")
    print(f"{'='*50}\n")

    # Start Flask server
    app.run(
        host='0.0.0.0',
        port=SERVER_PORT,
        threaded=True
    )