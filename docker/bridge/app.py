# Tiny HTTP-to-MySQL bridge for the keller_temp sensor. The ESP32 firmware
# POSTs one JSON reading per stored sample; this just validates it and
# inserts a row. No auth beyond being on the LAN/Tailnet — matches the rest
# of this hobby project's threat model (see Config.h's OTA/WiFi credential
# notes).
#
# Read endpoint + static PWA client added 2026-09-23: API contract from
# Tommy (see NC iot-keller/CONTRACT.md), adapted to this file's existing
# pymysql/get_conn pattern by Aurora.
#
# Whisper transcription endpoint added 2026-09-30: contract + reference
# patch from Tommy (Synesis feature/transcribe, see CONTRACT.md section
# "Whisper-Transkription"), adapted to this file's existing style (uses
# @app.post like the rest of the file, rather than add_url_rule) by Aurora.
import os
import tempfile
from datetime import datetime, timezone

import av.error
import pymysql
import pymysql.cursors
from flask import Flask, jsonify, request, send_from_directory

app = Flask(__name__)

CLIENT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "static")

GET_LIMIT_DEFAULT = 500
GET_LIMIT_MAX = 5000

WHISPER_MODEL_SIZE = os.environ.get("WHISPER_MODEL_SIZE", "small")
WHISPER_MODEL_PATH = os.environ.get("WHISPER_MODEL_PATH")  # None = library default cache dir
WHISPER_MAX_AUDIO_BYTES = 25 * 1024 * 1024
WHISPER_ALLOWED_CONTENT_TYPES = {"audio/mp4", "audio/m4a", "audio/wav", "audio/x-wav", "audio/webm"}

# Lazy-loaded on first request, not at import time: the model is ~460MB and
# loading it shouldn't block container startup or /health.
_whisper_model = None


def get_whisper_model():
    global _whisper_model
    if _whisper_model is None:
        from faster_whisper import WhisperModel

        _whisper_model = WhisperModel(
            WHISPER_MODEL_SIZE,
            device="cpu",
            compute_type="int8",
            download_root=WHISPER_MODEL_PATH,
        )
    return _whisper_model


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


@app.post("/transcripts/whisper")
def transcripts_whisper():
    content_type = (request.headers.get("Content-Type") or "").split(";")[0].strip().lower()
    if content_type not in WHISPER_ALLOWED_CONTENT_TYPES:
        return jsonify(ok=False, error="content-type must be audio/mp4"), 400

    length = request.content_length or 0
    if length <= 0:
        return jsonify(ok=False, error="empty audio"), 400
    if length > WHISPER_MAX_AUDIO_BYTES:
        return jsonify(ok=False, error="audio too large (max 25 MB)"), 413

    audio = request.get_data()
    if not audio:
        return jsonify(ok=False, error="empty audio"), 400

    tmp_path = None
    try:
        with tempfile.NamedTemporaryFile(suffix=".m4a", delete=False) as tmp:
            tmp.write(audio)
            tmp_path = tmp.name

        model = get_whisper_model()
        segments, info = model.transcribe(tmp_path, language="de", beam_size=1)
        text = " ".join(segment.text.strip() for segment in segments).strip()
        if not text:
            return jsonify(ok=False, error="no speech detected"), 422

        return jsonify(
            ok=True,
            text=text,
            model=f"whisper-{WHISPER_MODEL_SIZE}",
            language=info.language or "de",
        )
    except av.error.InvalidDataError:
        # Bytes with an allowed Content-Type that still aren't decodable
        # audio (corrupt upload, wrong format despite the header) — a client
        # error, not a whisper/infra failure. Verified against hardware:
        # this exact exception is what av.open() raises via faster-whisper's
        # decode_audio() for undecodable input (found by Tommy's e2e probe).
        return jsonify(ok=False, error="invalid audio"), 400
    except Exception:
        app.logger.exception("whisper transcription failed")
        return jsonify(ok=False, error="whisper unavailable"), 503
    finally:
        if tmp_path:
            try:
                os.remove(tmp_path)
            except OSError:
                pass


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
