# AI-Based-Bread-Quality-Indicator-System

# Project Overview
- Developed an AI-Based Bread Quality Indicator System for real-time bread spoilage detection using ESP32-CAM and MQ-135 gas sensor.
- Implemented gas-based spoilage detection by monitoring ammonia, carbon dioxide, and volatile organic compounds released during bread decomposition.
- Integrated CNN-based image analysis using TensorFlow/Keras and MobileNetV2 for fresh vs moldy bread classification.
- Designed a hybrid embedded system combining gas sensing, AI image processing, Wi-Fi communication, and real-time monitoring.
- Built a Flask-based web application for live camera streaming, spoilage monitoring, and system status visualization.
- Implemented automated alert mechanisms using LEDs and buzzer indicators for immediate spoilage notification.
- Developed standalone edge-AI functionality enabling real-time inference without cloud dependency.
- Applied IoT, embedded systems, machine learning, computer vision, and sensor interfacing concepts for smart food quality monitoring.
- Configured the deep learning pipeline with:
  - Image Size: 128×128
  - Batch Size: 16
  - Epochs: 15
  - Transfer Learning Model: MobileNetV2

# Project Flowchart
1. Bread sample is placed near the monitoring system.
2. MQ-135 gas sensor continuously detects spoilage-related gases.
3. ESP32-CAM reads and processes sensor values.
4. Camera captures bread images for AI-based analysis.
5. CNN model classifies bread condition as:
   - Fresh
   - Moldy/Spoiled
6. Sensor readings and AI prediction are combined for final decision making.
7. If spoilage is detected:
   - Red LED turns ON
   - Buzzer alert activates
   - Web dashboard updates status
8. If bread is fresh:
   - Green LED remains ON
9. ESP32-CAM transmits monitoring data through Wi-Fi to Flask web application.
10. System continues real-time monitoring in connected or standalone mode.

# Results & Conclusion
- Successfully developed a working AI-Based Bread Quality Indicator System capable of detecting bread spoilage in real time.
- The MQ-135 gas sensor effectively detected harmful gases released during bread decomposition, including ammonia and volatile organic compounds.
- Observed that gas sensor readings increased significantly as bread changed from fresh condition to spoiled/moldy condition.
- The CNN-based image classification model successfully differentiated between fresh and moldy bread samples using image analysis techniques.
- Real-time spoilage alerts were achieved using LEDs, buzzer notifications, and a Flask-based web monitoring dashboard.
- The ESP32-CAM successfully handled image capture, Wi-Fi communication, sensor monitoring, and embedded system control operations.
- Demonstrated that combining gas sensing and AI-based image analysis improves detection reliability compared to using a single detection method.
- Achieved standalone edge-AI functionality where the system continued spoilage detection even without cloud dependency or continuous internet access.
- The project proved that low-cost embedded hardware and lightweight AI models can be used for practical smart food quality monitoring applications.
- The system can help reduce food wastage, improve food safety, and support smart monitoring applications in households, bakeries, supermarkets, and food storage environments.
