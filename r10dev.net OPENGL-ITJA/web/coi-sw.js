// Only documents and worker entrypoints need isolation headers. Packs,
// wasm and ordinary scripts use the browser's normal same-origin fetch.
self.addEventListener('install', function (e) { e.waitUntil(self.skipWaiting()); });
self.addEventListener('activate', function (e) {
  e.waitUntil((async function () {
    if (self.registration.navigationPreload) {
      try { await self.registration.navigationPreload.enable(); } catch (err) {}
    }
    await self.clients.claim();
  })());
});
self.addEventListener('fetch', function (e) {
  if (e.request.cache === 'only-if-cached' && e.request.mode !== 'same-origin') return;
  if (new URL(e.request.url).origin !== self.location.origin) return;
  var document = e.request.mode === 'navigate';
  var worker = e.request.destination === 'worker' || e.request.destination === 'sharedworker';
  if (!document && !worker) return;
  e.respondWith((async function () {
    var r;
    if (document) {
      try { r = await e.preloadResponse; } catch (err) {}
    }
    if (!r) r = await fetch(e.request);
    if (r.status === 0 || r.type === 'opaque') return r;
    var h = new Headers(r.headers);
    h.set('Cross-Origin-Embedder-Policy', 'require-corp');
    if (document)
      h.set('Cross-Origin-Opener-Policy', 'same-origin');
    h.set('Cross-Origin-Resource-Policy', 'same-origin');
    return new Response(r.body, { status: r.status, statusText: r.statusText, headers: h });
  })());
});
