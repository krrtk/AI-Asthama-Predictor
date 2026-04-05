# Asthma Risk Predictor

A cloud-connected IoT wearable that predicts short-term asthma risk in real time using live sensor data and a machine learning ensemble model.

---

## What it does

A small wearable device reads your surrounding air quality and your body vitals every 5 seconds. It sends that data wirelessly to a cloud server, which runs an ML model and decides if you are at low or high risk of an asthma episode. The result shows up instantly on a tiny screen on the device and on a web dashboard you can open from any browser.

No laptop needed. No USB cable. Just wear it.

---

## How it works

```
Sensors → NodeMCU (WiFi) → Flask API → ML Model → Prediction → OLED Display + Web Dashboard
```

The device collects five values:

- Air quality (MQ-135 sensor)
- Temperature and humidity (DHT11 sensor)
- SpO2 and heart rate (MAX30102 sensor)

These are sent as a JSON request to the Flask API. The API runs the prediction, logs the result to a SQLite database, and sends back the risk level, probability, and the main reason for the prediction. The web dashboard reads from this database and updates every 5 seconds.

---

## Dataset

We did not collect medical data directly. The training dataset was built by combining two publicly available datasets and aligning them to match our sensor outputs.

### Source 1 — Asthma Disease Dataset
Contains patient records with asthma diagnosis labels (asthma / no asthma) along with pollution-related features such as air quality index and particulate matter levels. This dataset provided our output labels and the environmental side of the feature set.

### Source 2 — Human Vital Signs Dataset
Contains physiological readings from real patients including SpO2 (blood oxygen saturation) and heart rate values across different health conditions. This dataset provided the body vitals side of the feature set.

### How we merged them

Both datasets were cleaned individually to remove inconsistencies and missing values. Since they did not share a common patient ID, we could not do a direct row-level join. Instead we aligned them by health condition category — records labeled as asthma-positive were matched with vital sign readings from patients in respiratory distress, and records labeled as asthma-negative were matched with vital sign readings from healthy patients.

Temperature and humidity values were not present in either dataset. Since these are environmental factors that influence asthma risk, we generated synthetic but medically reasonable values for each record based on known ranges from published asthma research — higher humidity and temperature mapped to higher risk profiles.

The final merged dataset has approximately 2392 samples with 5 features and 1 binary label. It is a prototype dataset built specifically to match our sensor inputs. It has not been clinically validated.

---

## ML Model

We use an ensemble of three classifiers:

- Random Forest
- XGBoost
- Logistic Regression

Their predictions are combined using soft voting with calibrated probabilities (Platt scaling) so the output probability is reliable, not just a raw score.

Every prediction also includes a SHAP explanation — which sensor value pushed the risk up or down the most. An Isolation Forest runs as an anomaly gate before the ensemble, rejecting any sensor reading that is physically impossible.

---

## Project Structure

```
asthma-risk-predictor/
├── README.md
├── .gitignore
├── ml/
│   ├── train_asthma_model_v2.ipynb
│   ├── asthma_model_v2.pkl
│   └── dataset/
│       └── asthma_data.csv
├── api/
│   ├── app.py
│   └── requirements.txt
├── dashboard/
│   ├── index.html
│   ├── css/
│   │   └── style.css
│   └── js/
│       └── dashboard.js
└── firmware/
    └── asthma_predictor/
        └── asthma_predictor.ino
```

---

## Team

| Person | Role |
|--------|------|
| Person 1 | Hardware setup, sensor firmware, ML model training |
| Person 2 | Flask API, SQLite logging, drift detection |
| Person 3 | Web dashboard frontend |

---

## Limitations

- Dataset is partially synthetic and not clinically validated
- No temporal modelling — each prediction is based on a single 5-second reading, not a time series
- Sensor accuracy depends on hardware quality and placement
- Not intended for medical diagnosis
