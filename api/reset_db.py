"""
Run this ONCE to fix the sqlite3.OperationalError: table predictions has no column named top_feature
It deletes the old DB and recreates it with the correct schema.
"""
import os, sqlite3

DB_PATH = "predictions.db"

if os.path.exists(DB_PATH):
    os.remove(DB_PATH)
    print(f"Deleted old {DB_PATH}")

with sqlite3.connect(DB_PATH) as conn:
    conn.execute("""
        CREATE TABLE predictions (
            id               INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp        TEXT NOT NULL,
            air_quality      REAL,
            temperature      REAL,
            humidity         REAL,
            spo2             REAL,
            heart_rate       REAL,
            risk_level       INTEGER,
            risk_probability REAL,
            top_feature      TEXT
        )
    """)
    conn.commit()

print("Done — predictions.db recreated with correct schema.")
print("Now start Flask normally: python app_api.py")
