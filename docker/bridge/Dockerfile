# Debian-based, not alpine: faster-whisper's dependencies (ctranslate2,
# onnxruntime) only ship manylinux (glibc) wheels, not musllinux — on alpine,
# pip would either fail outright or fall back to a from-source build that
# isn't realistic for these (C++/ONNX runtime) packages. Verified against
# PyPI's file listings before switching (2026-09-30).
FROM python:3.12-slim

WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY app.py .
COPY static ./static

EXPOSE 5005
CMD ["python", "app.py"]
