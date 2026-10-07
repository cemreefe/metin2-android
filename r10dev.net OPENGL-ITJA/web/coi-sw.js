// coi-sw.js — make the page cross-origin isolated on plain static hosting.
// SharedArrayBuffer (the in-page server loopback) needs COOP + COEP headers
// that static hosts don't send; this service worker injects them on every
// same-origin response. Registered by local.html; first visit installs it
// and reloads once, afterwards all responses carry the headers.
self.addEventListener('install', function () { self.skipWaiting(); });
self.addEventListener('activate', function (e) { e.waitUntil(self.clients.claim()); });
self.addEventListener('fetch', function (e) {
  if (e.request.cache === 'only-if-cached' && e.request.mode !== 'same-origin') return;
  e.respondWith(
    fetch(e.request).then(function (r) {
      if (r.status === 0 || r.type === 'opaque') return r;
      var h = new Headers(r.headers);
      h.set('Cross-Origin-Embedder-Policy', 'require-corp');
      h.set('Cross-Origin-Opener-Policy', 'same-origin');
      h.set('Cross-Origin-Resource-Policy', 'cross-origin');
      return new Response(r.body, { status: r.status, statusText: r.statusText, headers: h });
    }).catch(function () { return fetch(e.request); })
  );
});
