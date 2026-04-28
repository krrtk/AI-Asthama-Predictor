import joblib, numpy as np
model = joblib.load("asthma_risk_model.pkl")

# Raw ADC value like ESP32 sends
X1 = np.array([[871, 33, 70, 99, 90]])
print("ADC raw:  ", model.predict_proba(X1))

# Mapped AQI value  
X2 = np.array([[106, 33, 70, 99, 90]])
print("AQI mapped:", model.predict_proba(X2))

# High risk AQI
X3 = np.array([[380, 33, 70, 92, 110]])
print("High AQI:  ", model.predict_proba(X3))