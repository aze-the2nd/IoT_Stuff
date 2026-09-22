# Tiny HTTP-to-MySQL bridge for the keller_temp sensor. The ESP32 firmware
# POSTs one JSON reading per stored sample; this just validates it and
# inserts a row. No auth beyond being on the LAN — matches the rest of this
# hobby project's threat model (see Config.h's OTA/WiFi credential notes).
import os
from datetime import datetime, timezone

import pymysql
from flask import Flask, jsonify, request

app = Flask(__name__)


def get_conn():
    return pymysql.connect(
        host=os.environ["DB_HOST"],
        user=os.environ["DB_USER"],
        password=os.environ["DB_PASSWORD"],
        database=os.environ["DB_NAME"],
        autocommit=True,
    )


@app.post("/keller_temp")
def insert_reading():
    data = request.get_json(force=True, silent=True) or {}
    epoch = data.get("epoch")
    temp_c = data.get("temp_c")
    if not isinstance(epoch, int) or not isinstance(temp_c, (int, float)):
        return jsonify(error="epoch (int) and temp_c (number) required"), 400

    recorded_at = datetime.fromtimestamp(epoch, tz=timezone.utc)
    conn = get_conn()
    try:
        with conn.cursor() as cur:
            cur.execute(
                "INSERT INTO keller_temp (recorded_at, temp_c) VALUES (%s, %s)",
                (recorded_at, temp_c),
            )
    finally:
        conn.close()
    return jsonify(status="ok"), 201


@app.get("/health")
def health():
    return jsonify(status="ok")


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5005)
