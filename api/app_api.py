"""
Asthma Risk Predictor — CLEAN WORKING VERSION
"""

import joblib
import numpy as np
import sqlite3
import datetime
import os
from flask import Flask, request, jsonify, render_template

# ── LOAD MODEL ─────────────────────────────────────────────
MODEL_PATH = os.path.abspath("asthma_risk_model.pkl")
print("Loading model from:", MODEL_PATH)

model = joblib.load(MODEL_PATH)
print("Model type:", type(model))

# ── FEATURE ORDER (must match training)
FEATURE_NAMES = ["air_quality", "temperature", "humidity", "spo2", "heart_rate"]

# ── APP SETUP ─────────────────────────────────────────────
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
app = Flask(
    __name__,
    template_folder=os.path.join(BASE_DIR, "templates"),
    static_folder=os.path.join(BASE_DIR, "static")
)

DB_PATH = "predictions.db"

# ── INIT DATABASE ─────────────────────────────────────────
def init_db():
    with sqlite3.connect(DB_PATH) as conn:
        conn.execute("""
            CREATE TABLE IF NOT EXISTS predictions (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TEXT NOT NULL,
                air_quality REAL,
                temperature REAL,
                humidity REAL,
                spo2 REAL,
                heart_rate REAL,
                risk_level INTEGER,
                risk_probability REAL,
                top_feature TEXT
            )
        """)
        conn.commit()

init_db()

# ── PREDICT ENDPOINT ──────────────────────────────────────
@app.route("/predict", methods=["POST"])
def predict():

    data = request.get_json(silent=True)
    if not data:
        return jsonify({"error": "No JSON received"}), 400

    print("RECEIVED:", data)

    try:
        features = {f: float(data[f]) for f in FEATURE_NAMES}
    except Exception as e:
        return jsonify({"error": str(e)}), 400

    # ── MODEL INFERENCE ───────────────────────────────────
    # Training data air_quality range was 0–10.
    # ESP32 MQ135 sends raw ADC 0–4095 → map it down before prediction.
    features["air_quality"] = (features["air_quality"] / 4095) * 10

    feature_values = [features[f] for f in FEATURE_NAMES]
    X = np.array(feature_values).reshape(1, -1)

    print("X:", X)

    risk_level = int(model.predict(X)[0])

    if hasattr(model, "predict_proba"):
        proba = model.predict_proba(X)[0]
        high_risk_prob   = float(proba[1])           # P(HIGH RISK) — for dashboard
        risk_probability = float(proba[risk_level])  # confidence of predicted class — for LCD
    else:
        high_risk_prob   = float(risk_level)
        risk_probability = float(risk_level)

    top_feature = FEATURE_NAMES[int(np.argmax(feature_values))]

    print("Prediction:", risk_level)
    print("Confidence:", risk_probability)

    # ── SAVE TO DB ───────────────────────────────────────
    timestamp = datetime.datetime.utcnow().isoformat()

    with sqlite3.connect(DB_PATH) as conn:
        conn.execute("""
            INSERT INTO predictions
            (timestamp, air_quality, temperature, humidity, spo2, heart_rate,
             risk_level, risk_probability, top_feature)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            timestamp,
            features["air_quality"],
            features["temperature"],
            features["humidity"],
            features["spo2"],
            features["heart_rate"],
            risk_level,
            high_risk_prob,
            top_feature
        ))
        conn.commit()

    # ── RESPONSE (MATCH ESP32) ────────────────────────────
    return jsonify({
        "risk_level":       risk_level,
        "risk_probability": round(high_risk_prob, 4),    # P(HIGH RISK) — for dashboard
        "risk_label":       "HIGH RISK" if risk_level == 1 else "LOW RISK",
        "confidence":       round(risk_probability, 4),  # confidence of predicted class — for LCD
        "drift_flag":       0
    }), 200


# ── DASHBOARD DATA ───────────────────────────────────────
@app.route("/data", methods=["GET"])
def data():
    with sqlite3.connect(DB_PATH) as conn:
        conn.row_factory = sqlite3.Row
        row = conn.execute(
            "SELECT * FROM predictions ORDER BY id DESC LIMIT 1"
        ).fetchone()

    if row is None:
        return jsonify({"error": "No data yet"}), 404

    r = dict(row)
    return jsonify(r)


# ── HOME ────────────────────────────────────────────────
@app.route("/")
def home():
    return render_template("index.html")


# ── RUN ────────────────────────────────────────────────
if __name__ == "__main__":
    app.run(debug=True, host="0.0.0.0", port=5000)