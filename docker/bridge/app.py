# Tiny HTTP-to-MySQL bridge for the keller_temp sensor. The ESP32 firmware
# POSTs one JSON reading per stored sample; this just validates it and
# inserts a row. No auth beyond being on the LAN/Tailnet — matches the rest
# of this hobby project's threat model (see Config.h's OTA/WiFi credential
# notes).
#
# Read endpoint + static PWA client added 2026-09-23: API contract from
# Tommy (see NC iot-keller/CONTRACT.md), adapted to this file's existing
# pymysql/get_conn pattern by Aurora.
import os
from datetime import datetime, timezone

import pymysql
import pymysql.cursors
from flask import Flask, jsonify, request, send_from_directory

app = Flask(__name__)

CLIENT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "static")

GET_LIMIT_DEFAULT = 500
GET_LIMIT_MAX = 5000


def get_conn():
    return pymysql.connect(
        host=os.environ["DB_HOST"],
        user=os.environ["DB_USER"],
        password=os.environ["DB_PASSWORD"],
        database=os.environ["DB_NAME"],
        autocommit=True,
    )


def db_query(sql, params):
    conn = get_conn()
    try:
        with conn.cursor(pymysql.cursors.DictCursor) as cur:
            cur.execute(sql, params)
            return cur.fetchall()
    finally:
        conn.close()


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


@app.get("/keller_temp")
def keller_temp_read():
    since = request.args.get("since", type=int)
    limit = request.args.get("limit", default=GET_LIMIT_DEFAULT, type=int)

    if limit is None or not (1 <= limit <= GET_LIMIT_MAX):
        return jsonify(ok=False, error="limit must be an integer between 1 and 5000"), 400
    if since is not None and since < 0:
        return jsonify(ok=False, error="since must be a non-negative unix timestamp"), 400

    sql = "SELECT UNIX_TIMESTAMP(recorded_at) AS t, temp_c FROM keller_temp"
    params = []
    if since is not None:
        sql += " WHERE recorded_at >= FROM_UNIXTIME(%s)"
        params.append(since)
    sql += " ORDER BY id DESC LIMIT %s"
    params.append(limit)

    try:
        rows = db_query(sql, params)
    except Exception:
        app.logger.exception("keller_temp read failed")
        return jsonify(ok=False, error="db error"), 500

    values = [{"t": int(r["t"]), "temp_c": float(r["temp_c"])} for r in rows]
    return jsonify(ok=True, count=len(values), values=values)


@app.get("/health")
def health():
    return jsonify(status="ok")


# Static PWA client (same origin as the API — no CORS needed). Flask/Werkzeug
# matches the fixed rules above (/keller_temp, /health) before this catch-all,
# so the API stays reachable regardless of route registration order.
@app.route("/")
def client_index():
    return send_from_directory(CLIENT_DIR, "index.html")


@app.route("/<path:filename>")
def client_static(filename):
    return send_from_directory(CLIENT_DIR, filename)


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5005)
