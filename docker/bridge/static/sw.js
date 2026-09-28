/* Minimaler Service Worker — registriert sich nur im Secure Context (HTTPS).
   Über http://<Tailnet-IP> ohne TLS bleibt die App eine normale Web-App. */
"use strict";

var CACHE = "keller-temp-v1";
var SHELL = ["/", "/app.js", "/manifest.webmanifest", "/icon.svg"];

self.addEventListener("install", function (e) {
  e.waitUntil(caches.open(CACHE).then(function (c) { return c.addAll(SHELL); }));
});

self.addEventListener("activate", function (e) {
  e.waitUntil(
    caches.keys().then(function (keys) {
      return Promise.all(keys.filter(function (k) { return k !== CACHE; })
        .map(function (k) { return caches.delete(k); }));
    })
  );
});

/* App-Shell aus dem Cache, Messwerte immer live vom Netz. */
self.addEventListener("fetch", function (e) {
  var url = new URL(e.request.url);
  if (e.request.method !== "GET" || url.pathname === "/keller_temp") return;
  e.respondWith(
    caches.match(e.request).then(function (hit) {
      return hit || fetch(e.request).then(function (resp) {
        var copy = resp.clone();
        caches.open(CACHE).then(function (c) { c.put(e.request, copy); });
        return resp;
      });
    })
  );
});
