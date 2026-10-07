// M2PK archive loading — one compressed bundle instead of N file requests.
//
// Wire format (little-endian, gzip-compressed on the wire):
//   'M2PK' u32 bootCount u32 totalCount
//   per entry: u32 pathLen | path bytes | u64 size | data
// Boot-tier entries come first: opts.bootReady() fires once they are
// unpacked so main() can start while the rest streams in.
//
// Decompression is streaming (DecompressionStream 'gzip'). Whole packs are
// cached in IndexedDB keyed by URL+version — after the first download,
// boots are network-free.
//
// Runs on the page and inside workers (no DOM dependencies).

var M2PACK_IDB = 'm2pack-cache';

// Optional passphrase gate: encrypted packs are 'M2E1' | 12-byte nonce |
// AES-GCM ciphertext. The key is derived from the passphrase (PBKDF2) so
// nothing secret ships in the page — wrong passphrase just fails to open.
var m2packKey = null;

async function m2packSetPassphrase(pw) {
  var km = await crypto.subtle.importKey(
    'raw', new TextEncoder().encode(pw), 'PBKDF2', false, ['deriveKey']);
  m2packKey = await crypto.subtle.deriveKey(
    { name: 'PBKDF2', salt: new TextEncoder().encode('m2gate-v1'),
      iterations: 100000, hash: 'SHA-256' },
    km, { name: 'AES-GCM', length: 256 }, false, ['decrypt']);
}

// buf -> plaintext gzip bytes. Non-'M2E1' input passes through untouched
// so encrypted and plain packs can mix (e.g. local dev vs deployed).
async function m2packDecrypt(buf) {
  var b = new Uint8Array(buf);
  if (b.length < 20 || b[0] !== 0x4d || b[1] !== 0x32 || b[2] !== 0x45 || b[3] !== 0x31)
    return buf;
  if (!m2packKey) throw new Error('pack is encrypted: passphrase required');
  var plain = await crypto.subtle.decrypt(
    { name: 'AES-GCM', iv: b.subarray(4, 16) }, m2packKey, b.subarray(16));
  return plain;
}

function m2idbOpen() {
  return new Promise(function (resolve, reject) {
    var req = indexedDB.open(M2PACK_IDB, 1);
    req.onupgradeneeded = function () {
      req.result.createObjectStore('packs', { keyPath: 'url' });
    };
    req.onsuccess = function () { resolve(req.result); };
    req.onerror = function () { reject(req.error); };
  });
}

async function m2idbGet(url) {
  try {
    var db = await m2idbOpen();
    return await new Promise(function (resolve) {
      var tx = db.transaction('packs', 'readonly');
      var req = tx.objectStore('packs').get(url);
      req.onsuccess = function () { resolve(req.result || null); };
      req.onerror = function () { resolve(null); };
    });
  } catch (e) { return null; }
}

async function m2idbPut(url, ver, gz) {
  try {
    var db = await m2idbOpen();
    await new Promise(function (resolve) {
      var tx = db.transaction('packs', 'readwrite');
      tx.objectStore('packs').put({ url: url, ver: ver, gz: gz });
      tx.oncomplete = resolve;
      tx.onerror = resolve;
    });
  } catch (e) {}
}

// Fetch (or read IDB) the compressed pack bytes. Returns a Blob —
// Chrome file-backs large Blobs, so a ~150MB part doesn't sit on the
// JS heap, and IndexedDB stores it straight through.
async function m2packFetch(spec, onProgress) {
  var hit = await m2idbGet(spec.url);
  if (hit && hit.ver === spec.ver)
    return hit.gz instanceof Blob ? hit.gz : new Blob([hit.gz]);

  var resp = await fetch(spec.url);
  if (!resp.ok) throw new Error(spec.url + ': HTTP ' + resp.status);
  var total = +resp.headers.get('content-length') || 0;
  var rd = resp.body.getReader();
  var parts = [], len = 0;
  for (;;) {
    var c = await rd.read();
    if (c.done) break;
    parts.push(c.value);
    len += c.value.length;
    if (onProgress && total) onProgress(len / total);
  }
  var gz = new Blob(parts);
  m2idbPut(spec.url, spec.ver, gz);   // fire-and-forget
  return gz;
}

// Chunked reader over a ReadableStream of the decompressed pack.
function m2packReader(gz) {
  var stream = (gz instanceof Blob ? gz : new Blob([gz])).stream()
    .pipeThrough(new DecompressionStream('gzip'));
  var rd = stream.getReader();
  var chunks = [], avail = 0, done = false;
  async function fill(n) {
    while (avail < n && !done) {
      var c = await rd.read();
      if (c.done) { done = true; break; }
      chunks.push(c.value);
      avail += c.value.length;
    }
  }
  return {
    // consume exactly n bytes; returns null on truncation
    take: async function (n) {
      await fill(n);
      if (avail < n) return null;
      var out = new Uint8Array(n), off = 0;
      while (off < n) {
        var head = chunks[0];
        var k = Math.min(n - off, head.length);
        out.set(head.subarray(0, k), off);
        off += k;
        if (k === head.length) chunks.shift();
        else chunks[0] = head.subarray(k);
        avail -= k;
      }
      return out;
    },
  };
}

function m2mkdirP(FS, path) {
  var parts = path.split('/'), dir = '';
  for (var i = 0; i < parts.length; i++) {
    if (!parts[i]) continue;
    dir += '/' + parts[i];
    try { FS.mkdir(dir); } catch (e) {}
  }
}

async function m2idbDel(url) {
  try {
    var db = await m2idbOpen();
    db.transaction('packs', 'readwrite').objectStore('packs').delete(url);
  } catch (e) {}
}

// Walk the entries of one compressed archive.
// onEntry(path, data, i, total, bootCount) is called in order.
async function m2packEach(gz, url, onEntry) {
  var r = m2packReader(await m2packDecrypt(await gz.arrayBuffer()));
  var magic = await r.take(12);
  if (!magic || String.fromCharCode(magic[0], magic[1], magic[2], magic[3]) !== 'M2PK')
    throw new Error(url + ': bad magic');
  var dv = new DataView(magic.buffer, 4);
  var bootCount = dv.getUint32(0, true);
  var total = dv.getUint32(4, true);
  for (var i = 0; i < total; i++) {
    var hdr = await r.take(4);
    if (!hdr) throw new Error(url + ': truncated at entry ' + i);
    var plen = new DataView(hdr.buffer).getUint32(0, true);
    var path = new TextDecoder().decode(await r.take(plen));
    var sdv = new DataView((await r.take(8)).buffer);
    var size = sdv.getUint32(0, true) + sdv.getUint32(4, true) * 0x100000000;
    var data = await r.take(size);
    if (!data) throw new Error(url + ': truncated data ' + path);
    onEntry(path, data, i, total, bootCount);
  }
}

// Unpack archives into FS under dest (memfs path prefix).
// spec = {url, ver} (single archive) or {boot: {url,ver}, parts: [{url,ver}]}
// (split archives). With a boot archive, opts.bootReady fires once it's
// unpacked; with a single archive it fires at bootCount or the end.
// opts = {onProgress, bootReady, onFile}
async function m2LoadPack(FS, spec, dest, opts) {
  opts = opts || {};
  var specs = spec.parts ? [].concat(spec.boot ? [spec.boot] : [], spec.parts)
                         : [spec];
  var bootFired = !spec.boot;
  for (var si = 0; si < specs.length; si++) {
    var sp = specs[si];
    if (opts.onPart) opts.onPart(si, specs.length, si === 0 && spec.boot);
    var gz = await m2packFetch(sp, opts.onProgress);
    await m2packEach(gz, sp.url, function (path, data, i, total, bootCount) {
      m2mkdirP(FS, dest + '/' + path.split('/').slice(0, -1).join('/'));
      FS.writeFile(dest + '/' + path, data);
      if (opts.onFile) opts.onFile(path, i + 1, total);
      if (!bootFired && !spec.boot && i + 1 >= bootCount) {
        bootFired = true;
        if (opts.bootReady) opts.bootReady();
      }
    });
    if (!bootFired) {
      bootFired = true;
      if (opts.bootReady) opts.bootReady();
    }
  }
  if (opts.bootReady && !bootFired) opts.bootReady();
}

// Lazy archives: file data stays out of MEMFS. Each archive is unpacked
// once into a Blob of concatenated file data plus an index, stored in
// IndexedDB (so the Blob is disk-backed). Writes <dest>/.m2lazy, which the
// engine's lazy FS (platform/web/WebLazyFS.cpp) reads to copy single files
// into MEMFS on first open.
// opts = {onPart, onProgress, onFile}
async function m2LoadPackLazy(FS, specs, dest, opts) {
  opts = opts || {};
  var urls = [], lines = [];
  for (var si = 0; si < specs.length; si++) {
    var sp = specs[si];
    if (opts.onPart) opts.onPart(si, specs.length);
    var key = sp.url + '#raw';
    var rec = await m2idbGet(key);
    if (!rec || rec.ver !== sp.ver || !(rec.gz instanceof Blob) || !rec.index) {
      var gz = await m2packFetch(sp, opts.onProgress);
      var parts = [], index = [], off = 0;
      await m2packEach(gz, sp.url, function (path, data, i, total) {
        parts.push(data);
        index.push([path.toLowerCase(), off, data.length]);
        off += data.length;
        if (opts.onFile) opts.onFile(path, i + 1, total);
      });
      gz = null;
      var blob = new Blob(parts);
      parts = null;
      await new Promise(async function (resolve) {
        try {
          var db = await m2idbOpen();
          var tx = db.transaction('packs', 'readwrite');
          tx.objectStore('packs').put({ url: key, ver: sp.ver, gz: blob, index: index });
          tx.oncomplete = resolve;
          tx.onerror = resolve;
        } catch (e) { resolve(); }
      });
      // Re-read so the Blob handle is the disk-backed IndexedDB copy.
      rec = (await m2idbGet(key)) || { gz: blob, index: index };
      blob = null;
      m2idbDel(sp.url);
    }
    urls.push(URL.createObjectURL(rec.gz));
    for (var k = 0; k < rec.index.length; k++) {
      var e = rec.index[k];
      lines.push(si + '\t' + e[1] + '\t' + e[2] + '\t' + e[0]);
    }
  }
  FS.writeFile(dest + '/.m2lazy',
    urls.length + '\n' + urls.join('\n') + '\n' + lines.join('\n') + '\n');
}
