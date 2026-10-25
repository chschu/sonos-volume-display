#include "WebUI.h"

namespace Config {
namespace WebUI {

const char INDEX_HTML[] PROGMEM = R"HTMLDOC(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Sonos Volume Display</title>
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; }
  body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; margin: 0; padding: 0 1rem 4rem; background: #121317; color: #e6e6e6; }
  header { padding: 1rem 0 0.5rem; }
  h1 { font-size: 1.3rem; margin: 0 0 0.75rem; }
  nav { display: flex; gap: 0.5rem; flex-wrap: wrap; }
  nav button { flex: 1 1 auto; padding: 0.5rem 0.75rem; border: 1px solid #3a3d44; border-radius: 6px; background: #1c1e24; color: #e6e6e6; cursor: pointer; font-size: 0.9rem; }
  nav button.active { background: #3a7bd5; border-color: #3a7bd5; color: #fff; }
  section { display: none; background: #1c1e24; border-radius: 8px; padding: 1rem; margin-top: 1rem; box-shadow: 0 1px 2px rgba(0,0,0,0.4); }
  section.visible { display: block; }
  h2 { font-size: 1.05rem; margin: 0 0 0.75rem; }
  .row { margin-bottom: 0.75rem; }
  label { display: block; font-size: 0.8rem; color: #9aa0a6; margin-bottom: 0.25rem; }
  input[type=text], input[type=password], input[type=number], select { width: 100%; padding: 0.4rem 0.5rem; border: 1px solid #3a3d44; border-radius: 6px; background: #121317; color: #e6e6e6; font-size: 0.95rem; }
  input[type=checkbox] { transform: scale(1.2); margin-right: 0.4rem; }
  button.action { padding: 0.5rem 1rem; border: none; border-radius: 6px; background: #3a7bd5; color: #fff; cursor: pointer; font-size: 0.9rem; }
  button.action.secondary { background: #2a2d33; color: #e6e6e6; }
  .kv { width: 100%; border-collapse: collapse; font-size: 0.85rem; }
  .kv td { padding: 0.25rem 0.4rem; border-bottom: 1px solid #2a2d33; word-break: break-all; }
  .kv td:first-child { color: #9aa0a6; white-space: nowrap; }
  .kv tr:nth-child(even) { background: #202329; }
  ul.picker { list-style: none; margin: 0.5rem 0 0; padding: 0; max-height: 180px; overflow-y: auto; border: 1px solid #2a2d33; border-radius: 6px; }
  ul.picker li { padding: 0.5rem; border-bottom: 1px solid #2a2d33; cursor: pointer; font-size: 0.85rem; }
  ul.picker li:last-child { border-bottom: none; }
  ul.picker li:hover { background: #262a31; }
  .muted { color: #888; font-size: 0.8rem; }
  .hidden { display: none; }
  #toast { position: fixed; left: 50%; bottom: 1rem; transform: translateX(-50%) translateY(150%); opacity: 0; pointer-events: none; background: #2f855a; color: #fff; padding: 0.6rem 1rem; border-radius: 6px; font-size: 0.9rem; transition: transform 0.2s ease, opacity 0.2s ease; max-width: 90vw; text-align: center; }
  #toast.show { transform: translateX(-50%) translateY(0); opacity: 1; pointer-events: auto; }
  #toast.error { background: #c53030; }
</style>
</head>
<body>
<header>
  <h1>Sonos Volume Display</h1>
  <nav>
    <button data-section="status" class="active">Status</button>
    <button data-section="network">Network</button>
    <button data-section="sonos">Sonos Room</button>
    <button data-section="led">LED</button>
  </nav>
</header>
<main>
  <section id="status" class="visible">
    <h2>Status</h2>
    <table class="kv" id="statusTable"><tbody></tbody></table>
    <p><button class="action secondary" id="statusRefresh" type="button">Refresh</button></p>
  </section>

  <section id="network">
    <h2>Network</h2>
    <form id="networkForm">
      <div class="row">
        <label for="netSsid">SSID</label>
        <input type="text" id="netSsid" name="ssid">
      </div>
      <div class="row">
        <button type="button" class="action secondary" id="netScanBtn">Scan networks</button>
        <ul class="picker hidden" id="netScanList"></ul>
      </div>
      <div class="row">
        <label for="netPassphrase">Passphrase</label>
        <input type="password" id="netPassphrase" name="passphrase" placeholder="leave blank to keep current">
      </div>
      <div class="row">
        <label for="netHostname">Hostname</label>
        <input type="text" id="netHostname" name="hostname">
      </div>
      <button type="submit" class="action">Save</button>
    </form>
    <h2 style="margin-top:1.25rem">Current connection</h2>
    <table class="kv" id="netStatusTable"><tbody></tbody></table>
  </section>

  <section id="sonos">
    <h2>Sonos Room</h2>
    <form id="sonosForm">
      <div class="row">
        <label><input type="checkbox" id="sonosActive" name="active"> Enabled</label>
      </div>
      <div class="row">
        <button type="button" class="action secondary" id="sonosDiscoverBtn">Discover rooms</button>
        <ul class="picker hidden" id="sonosRoomList"></ul>
      </div>
      <div class="row">
        <label for="sonosRoomUuid">Room UUID</label>
        <input type="text" id="sonosRoomUuid" name="room-uuid">
      </div>
      <button type="submit" class="action">Save</button>
    </form>
  </section>

  <section id="led">
    <h2>LED</h2>
    <form id="ledForm">
      <div class="row">
        <label for="ledBrightness">Brightness (0-255)</label>
        <input type="number" id="ledBrightness" name="brightness" min="0" max="255">
      </div>
      <div class="row">
        <label for="ledTransform">Volume curve</label>
        <select id="ledTransform" name="transform"></select>
      </div>
      <button type="submit" class="action">Save</button>
    </form>
  </section>
</main>
<div id="toast"></div>
<script>
(function () {
  var toastTimer;
  function toast(msg, isError) {
    var el = document.getElementById('toast');
    el.textContent = msg;
    el.className = isError ? 'show error' : 'show';
    clearTimeout(toastTimer);
    toastTimer = setTimeout(function () { el.className = ''; }, 4000);
  }

  function getJSON(url) {
    return fetch(url).then(function (r) {
      return r.json().then(function (body) { return { ok: r.ok, status: r.status, body: body }; });
    });
  }

  function postForm(url, params) {
    return fetch(url, {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: new URLSearchParams(params).toString()
    }).then(function (r) {
      return r.json().then(function (body) { return { ok: r.ok, status: r.status, body: body }; });
    });
  }

  function kv(tbody, obj) {
    tbody.innerHTML = '';
    Object.keys(obj).forEach(function (k) {
      var tr = document.createElement('tr');
      var tdK = document.createElement('td');
      tdK.textContent = k;
      var tdV = document.createElement('td');
      tdV.textContent = String(obj[k]);
      tr.appendChild(tdK);
      tr.appendChild(tdV);
      tbody.appendChild(tr);
    });
  }

  function loadStatus() {
    Promise.all([getJSON('/api/info'), getJSON('/api/config/network')]).then(function (r) {
      var info = r[0].body || {};
      var status = (r[1].body && r[1].body.status) || {};
      kv(document.querySelector('#statusTable tbody'), Object.assign({}, info, status));
    }).catch(function () { toast('Failed to load status', true); });
  }

  function loadNetwork() {
    getJSON('/api/config/network').then(function (r) {
      var cfg = r.body;
      document.getElementById('netSsid').value = cfg.ssid || '';
      document.getElementById('netHostname').value = cfg.hostname || '';
      document.getElementById('netPassphrase').value = '';
      if (cfg.status) {
        kv(document.querySelector('#netStatusTable tbody'), cfg.status);
      }
    }).catch(function () { toast('Failed to load network config', true); });
  }

  function loadSonos() {
    getJSON('/api/config/sonos').then(function (r) {
      var cfg = r.body;
      document.getElementById('sonosActive').checked = !!cfg.active;
      document.getElementById('sonosRoomUuid').value = cfg['room-uuid'] || '';
    }).catch(function () { toast('Failed to load Sonos config', true); });
  }

  function loadLed() {
    getJSON('/api/config/led').then(function (r) {
      var cfg = r.body;
      document.getElementById('ledBrightness').value = cfg.brightness;
      var select = document.getElementById('ledTransform');
      select.innerHTML = '';
      var choices = (cfg.choices && cfg.choices.transform) || [];
      choices.forEach(function (c) {
        var opt = document.createElement('option');
        opt.value = c.id;
        opt.textContent = c.name + ' (' + c.formula + ')';
        select.appendChild(opt);
      });
      select.value = cfg.transform;
    }).catch(function () { toast('Failed to load LED config', true); });
  }

  var sectionLoaders = { status: loadStatus, network: loadNetwork, sonos: loadSonos, led: loadLed };

  function showSection(name) {
    document.querySelectorAll('main > section').forEach(function (s) {
      s.classList.toggle('visible', s.id === name);
    });
    document.querySelectorAll('nav button').forEach(function (b) {
      b.classList.toggle('active', b.dataset.section === name);
    });
    if (sectionLoaders[name]) sectionLoaders[name]();
  }

  document.querySelectorAll('nav button').forEach(function (b) {
    b.addEventListener('click', function () { showSection(b.dataset.section); });
  });

  document.getElementById('statusRefresh').addEventListener('click', loadStatus);

  document.getElementById('netScanBtn').addEventListener('click', function () {
    var list = document.getElementById('netScanList');
    list.classList.remove('hidden');
    list.innerHTML = '<li class="muted">Scanning...</li>';
    getJSON('/api/discover/networks').then(function (r) {
      if (!r.ok) { list.innerHTML = '<li class="muted">Scan failed</li>'; return; }
      list.innerHTML = '';
      if (!r.body.length) { list.innerHTML = '<li class="muted">No networks found</li>'; return; }
      r.body.forEach(function (n) {
        var li = document.createElement('li');
        li.textContent = n.ssid + ' (' + n.rssi + ' dBm' + (n.encrypted ? ', locked' : '') + ')';
        li.addEventListener('click', function () {
          document.getElementById('netSsid').value = n.ssid;
          list.classList.add('hidden');
        });
        list.appendChild(li);
      });
    }).catch(function () { list.innerHTML = '<li class="muted">Scan failed</li>'; });
  });

  document.getElementById('networkForm').addEventListener('submit', function (ev) {
    ev.preventDefault();
    if (!confirm('Saving will restart the device if anything changed. Continue?')) return;
    var params = { ssid: document.getElementById('netSsid').value, hostname: document.getElementById('netHostname').value };
    var pass = document.getElementById('netPassphrase').value;
    if (pass !== '') params.passphrase = pass;
    postForm('/api/config/network', params).then(function (r) {
      var cfg = r.body;
      document.getElementById('netSsid').value = cfg.ssid || '';
      document.getElementById('netHostname').value = cfg.hostname || '';
      document.getElementById('netPassphrase').value = '';
      if (cfg.status) kv(document.querySelector('#netStatusTable tbody'), cfg.status);
      if (r.ok) {
        toast('Saved. Device is rebooting if anything changed.');
      } else {
        toast('Save failed - values were not accepted.', true);
      }
    }).catch(function () { toast('Save failed - device unreachable (it may be rebooting).', true); });
  });

  document.getElementById('sonosDiscoverBtn').addEventListener('click', function () {
    var list = document.getElementById('sonosRoomList');
    list.classList.remove('hidden');
    list.innerHTML = '<li class="muted">Discovering...</li>';
    getJSON('/api/discover/rooms').then(function (r) {
      if (!r.ok) { list.innerHTML = '<li class="muted">' + (r.body && r.body.error ? r.body.error : 'Discovery failed') + '</li>'; return; }
      list.innerHTML = '';
      if (!r.body.length) { list.innerHTML = '<li class="muted">No rooms found</li>'; return; }
      r.body.forEach(function (room) {
        var li = document.createElement('li');
        li.textContent = room.name + ' (' + room.ip + ')';
        li.addEventListener('click', function () {
          document.getElementById('sonosRoomUuid').value = room.uuid;
          list.classList.add('hidden');
        });
        list.appendChild(li);
      });
    }).catch(function () { list.innerHTML = '<li class="muted">Discovery failed</li>'; });
  });

  document.getElementById('sonosForm').addEventListener('submit', function (ev) {
    ev.preventDefault();
    if (!confirm('Saving will restart the device if anything changed. Continue?')) return;
    var params = {
      active: document.getElementById('sonosActive').checked ? 'true' : 'false',
      'room-uuid': document.getElementById('sonosRoomUuid').value
    };
    postForm('/api/config/sonos', params).then(function (r) {
      var cfg = r.body;
      document.getElementById('sonosActive').checked = !!cfg.active;
      document.getElementById('sonosRoomUuid').value = cfg['room-uuid'] || '';
      if (r.ok) {
        toast('Saved. Device is rebooting if anything changed.');
      } else {
        toast('Save failed - values were not accepted.', true);
      }
    }).catch(function () { toast('Save failed - device unreachable (it may be rebooting).', true); });
  });

  document.getElementById('ledForm').addEventListener('submit', function (ev) {
    ev.preventDefault();
    if (!confirm('Saving will restart the device if anything changed. Continue?')) return;
    var params = {
      brightness: document.getElementById('ledBrightness').value,
      transform: document.getElementById('ledTransform').value
    };
    postForm('/api/config/led', params).then(function (r) {
      var cfg = r.body;
      document.getElementById('ledBrightness').value = cfg.brightness;
      var select = document.getElementById('ledTransform');
      if (select.querySelector('option[value="' + cfg.transform + '"]')) {
        select.value = cfg.transform;
      }
      if (r.ok) {
        toast('Saved. Device is rebooting if anything changed.');
      } else {
        toast('Save failed - values were not accepted.', true);
      }
    }).catch(function () { toast('Save failed - device unreachable (it may be rebooting).', true); });
  });

  showSection('status');
})();
</script>
</body>
</html>
)HTMLDOC";

} // namespace WebUI
} // namespace Config
