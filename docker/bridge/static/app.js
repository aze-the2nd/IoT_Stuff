/* Keller-Temperatur — Client gegen GET /keller_temp (Vertrag CONTRACT.md)
   Same-Origin, keine externen Abhängigkeiten. Tommy, 2026-09-23. */
"use strict";

var LIMIT = 2000;          // Punkte pro Abruf
var REFRESH_MS = 60000;    // Auto-Refresh alle 60 s
var MAX_POINTS = 800;      // maximale Punkte im Diagramm (Dezimierung)

var state = { values: [] };  // neueste zuerst, wie vom Server geliefert

function $(id) { return document.getElementById(id); }

function fmtTemp(v) { return v.toFixed(1); }
function fmtTime(t) {
  return new Date(t * 1000).toLocaleString("de-DE",
    { day: "2-digit", month: "2-digit", hour: "2-digit", minute: "2-digit" });
}
function ago(t) {
  var s = Math.max(0, Math.floor(Date.now() / 1000 - t));
  if (s < 60) return "vor " + s + " s";
  if (s < 3600) return "vor " + Math.floor(s / 60) + " min";
  return "vor " + Math.floor(s / 3600) + " h " + Math.floor((s % 3600) / 60) + " min";
}

function setStatus(text, isError) {
  var el = $("status");
  el.textContent = text;
  el.className = isError ? "error" : "";
}

function setHint(text) {
  var el = $("hint");
  if (text) { el.textContent = text; el.hidden = false; }
  else { el.hidden = true; }
}

function render() {
  var v = state.values;
  if (!v.length) {
    $("cur").textContent = "—";
    $("updated").textContent = "noch keine Daten";
    ["stat-min", "stat-avg", "stat-max"].forEach(function (id) { $(id).textContent = "—"; });
    drawChart([]);
    return;
  }
  var latest = v[0];
  $("cur").textContent = fmtTemp(latest.temp_c);
  $("updated").textContent = fmtTime(latest.t) + " Uhr (" + ago(latest.t) + ")";

  var temps = v.map(function (p) { return p.temp_c; });
  var min = Math.min.apply(null, temps);
  var max = Math.max.apply(null, temps);
  var avg = temps.reduce(function (a, b) { return a + b; }, 0) / temps.length;
  $("stat-min").textContent = fmtTemp(min) + " °C";
  $("stat-avg").textContent = fmtTemp(avg) + " °C";
  $("stat-max").textContent = fmtTemp(max) + " °C";

  drawChart(v);
}

/* Einfaches Liniendiagramm auf Canvas, ohne Bibliotheken. */
function drawChart(values) {
  var canvas = $("chart");
  var dpr = window.devicePixelRatio || 1;
  var cssW = canvas.clientWidth, cssH = canvas.clientHeight;
  if (!cssW) cssW = 500;
  canvas.width = Math.round(cssW * dpr);
  canvas.height = Math.round(cssH * dpr);
  var ctx = canvas.getContext("2d");
  ctx.scale(dpr, dpr);
  ctx.clearRect(0, 0, cssW, cssH);

  if (!values.length) {
    ctx.fillStyle = "#7f8ea3";
    ctx.font = "13px system-ui";
    ctx.textAlign = "center";
    ctx.fillText("noch keine Messwerte", cssW / 2, cssH / 2);
    return;
  }

  // älteste zuerst fürs Diagramm
  var pts = values.slice().reverse();
  if (pts.length > MAX_POINTS) {
    var k = Math.ceil(pts.length / MAX_POINTS);
    pts = pts.filter(function (_, i) { return i % k === 0; });
    if (pts.length < 2) pts = [values[values.length - 1], values[0]];
  }

  var padL = 44, padR = 12, padT = 10, padB = 24;
  var w = cssW - padL - padR, h = cssH - padT - padB;
  var t0 = pts[0].t, t1 = pts[pts.length - 1].t;
  var temps = pts.map(function (p) { return p.temp_c; });
  var lo = Math.floor(Math.min.apply(null, temps) - 0.5);
  var hi = Math.ceil(Math.max.apply(null, temps) + 0.5);
  if (hi - lo < 1) hi = lo + 1;

  function x(t) { return padL + (t1 === t0 ? w / 2 : ((t - t0) / (t1 - t0)) * w); }
  function y(temp) { return padT + (hi - temp) / (hi - lo) * h; }

  // Gitter + Y-Beschriftung
  ctx.strokeStyle = "#232c38";
  ctx.fillStyle = "#7f8ea3";
  ctx.font = "11px system-ui";
  ctx.textAlign = "right";
  var steps = 4;
  for (var s = 0; s <= steps; s++) {
    var gy = padT + (s / steps) * h;
    ctx.beginPath(); ctx.moveTo(padL, gy); ctx.lineTo(cssW - padR, gy); ctx.stroke();
    ctx.fillText(fmtTemp(hi - (s / steps) * (hi - lo)) + "°", padL - 6, gy + 3);
  }
  // X-Beschriftung (Anfang / Mitte / Ende)
  ctx.textAlign = "left";
  ctx.fillText(fmtTime(t0), padL, cssH - 6);
  ctx.textAlign = "center";
  ctx.fillText(fmtTime(Math.floor((t0 + t1) / 2)), padL + w / 2, cssH - 6);
  ctx.textAlign = "right";
  ctx.fillText(fmtTime(t1), cssW - padR, cssH - 6);

  // Fläche + Linie
  ctx.beginPath();
  pts.forEach(function (p, i) { i ? ctx.lineTo(x(p.t), y(p.temp_c)) : ctx.moveTo(x(p.t), y(p.temp_c)); });
  ctx.strokeStyle = "#6fa8d6";
  ctx.lineWidth = 1.5;
  ctx.lineJoin = "round";
  ctx.stroke();
  ctx.lineTo(x(pts[pts.length - 1].t), padT + h);
  ctx.lineTo(x(pts[0].t), padT + h);
  ctx.closePath();
  var grad = ctx.createLinearGradient(0, padT, 0, padT + h);
  grad.addColorStop(0, "rgba(111,168,214,0.18)");
  grad.addColorStop(1, "rgba(111,168,214,0)");
  ctx.fillStyle = grad;
  ctx.fill();

  // Letzter Punkt
  var lp = pts[pts.length - 1];
  ctx.beginPath();
  ctx.arc(x(lp.t), y(lp.temp_c), 3, 0, Math.PI * 2);
  ctx.fillStyle = "#6fa8d6";
  ctx.fill();
}

function load(initial) {
  return fetch("/keller_temp?limit=" + LIMIT)
    .then(function (r) {
      if (!r.ok) {
        return r.json().catch(function () { return {}; }).then(function (b) {
          throw new Error(r.status === 405
            ? "Lese-Endpunkt fehlt noch (405) — Bridge-Patch ist noch nicht deployed."
            : "Serverfehler " + r.status + (b.error ? ": " + b.error : ""));
        });
      }
      return r.json();
    })
    .then(function (d) {
      if (!d.ok || !Array.isArray(d.values)) throw new Error("unerwartete Antwort");
      state.values = d.values;
      render();
      setStatus("aktualisiert " + fmtTime(Math.floor(Date.now() / 1000)));
      setHint("");
    })
    .catch(function (e) {
      setStatus("Fehler", true);
      setHint(initial && !state.values.length
        ? "Keine Daten: " + e.message + " (Handy im Tailnet? Bridge erreichbar?)"
        : "Aktualisierung fehlgeschlagen: " + e.message);
    });
}

$("refresh").addEventListener("click", function () { load(false); });

window.addEventListener("resize", function () { render(); });

if ("serviceWorker" in navigator && location.protocol === "https:") {
  navigator.serviceWorker.register("/sw.js").catch(function () {});
}

load(true);
setInterval(function () { load(false); }, REFRESH_MS);
