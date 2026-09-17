// Optional rendering/integration test: Node 22+ and a locally installed Chromium.
// Run: node tests/browser_master_test.cjs [PATH_TO_CHROME_OR_EDGE]
// Uses only a private headless browser profile and a synthetic local HTTP fixture.
const fs = require('node:fs');
const path = require('node:path');
const http = require('node:http');
const { spawn } = require('node:child_process');
const assert = require('node:assert/strict');
const root = path.resolve(__dirname, '..');
const browserExe = process.argv[2] || 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe';
const output = path.join(root, 'build', 'test-artifacts');
fs.mkdirSync(output, { recursive: true });
const profile = fs.mkdtempSync(path.join(output, 'browser-master-'));
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
const track = (entryId, trackId, title, artist = 'Orbit & Friends') => ({
  entryId, trackId, id: 1, title, artist, album: 'After Hours', key: '8A', bpm: 128.5, originalBpm: 126.25,
  positionMs: 65000, durationMs: 240000, metadataAvailable: true, loaded: true, coverUrl: '/api/master/covers/' + trackId
});
let current = track(4, 104, 'Night Drive “Live”');
let history = [track(3, 103, 'First Light', 'Studio North'), track(2, 102, 'Between the Lines'), track(1, 101, 'New Beginnings')];
let status = 'connected';
let coverCalls = 0;
let deckData = { ...track(1, 101, 'Deck Overlay'), coverUrl: '/api/decks/1/cover?trackId=101' };
const routes = new Map([
  ['/master-overlay', 'master-overlay.html'], ['/master-overlay/settings', 'master-settings.html'],
  ['/master-overlay.js', 'master-overlay.js'], ['/master-options.js', 'master-options.js'], ['/overlay', 'overlay.html']
]);
for (const asset of ['navigation.js', 'navigation.css', 'icon.svg', 'deck-overlay.js', 'overlay-shared.js', 'overlay.css', 'settings.css', 'settings.js', 'i18n.js', 'storage.js', 'locales/en.json', 'locales/de.json']) routes.set('/' + asset, asset);
routes.set('/overlay/settings', 'master-settings.html');
routes.set('/', 'index.html');
for(const asset of ['auth.js','broadcast.js','component-presets.js','component-presets.css'])routes.set('/'+asset,asset);
const fixture = http.createServer((request, response) => {
  if(request.url==='/api/app'){response.setHeader('Content-Type','application/json');response.end(JSON.stringify({version:'2.1.0',mode:'rekordbox',canControl:true,capabilities:{dashboard:true,history:true,deckOverlays:true,masterOverlay:true,audioWaveform:true,rekordboxSetup:true,prolinkSetup:false,networkSettings:true}}));return;}
  const url = new URL(request.url, 'http://localhost');
  response.setHeader('Cache-Control', 'no-store');
  response.setHeader('Content-Security-Policy', "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; img-src 'self'; connect-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'self'; form-action 'none'");
  if (url.pathname === '/api/master') {
    response.setHeader('Content-Type', 'application/json');
    return response.end(JSON.stringify({ schemaVersion: 1, status, demo: false, current, history, historyLimit: 50 }));
  }
  if (url.pathname === '/api/state') {
    response.setHeader('Content-Type', 'application/json');
    return response.end(JSON.stringify({ status: 'connected', demo: false, decks: [deckData, ...[2,3,4].map(id => ({ ...deckData, id, trackId: 100 + id, title: 'Deck ' + id, coverUrl: '/api/decks/' + id + '/cover?trackId=' + (100 + id) }))] }));
  }
  if (url.pathname.startsWith('/api/master/covers/')) { coverCalls++; response.statusCode = 404; return response.end(); }
  const asset = routes.get(url.pathname);
  if (!asset) { response.statusCode = 404; return response.end(); }
  response.setHeader('Content-Type', asset.endsWith('.svg') ? 'image/svg+xml' : asset.endsWith('.js') ? 'text/javascript' : asset.endsWith('.css') ? 'text/css' : asset.endsWith('.json') ? 'application/json' : 'text/html; charset=utf-8');
  response.end(fs.readFileSync(path.join(root, 'web', asset)));
});

async function until(fn, message, timeout = 6000) {
  const start = Date.now();
  while (Date.now() - start < timeout) { if (await fn()) return; await delay(40); }
  throw new Error(message);
}

(async () => {
  let browser, socket;
  try {
    await new Promise(resolve => fixture.listen(0, '127.0.0.1', resolve));
    const base = 'http://127.0.0.1:' + fixture.address().port;
    browser = spawn(browserExe, ['--headless=new', '--no-first-run', '--no-default-browser-check',
      '--disable-background-networking', '--remote-debugging-port=0', '--user-data-dir=' + profile, 'about:blank'],
      { windowsHide: true, stdio: 'ignore' });
    let launchError;
    browser.on('error', error => { launchError = error; });
    const portFile = path.join(profile, 'DevToolsActivePort');
    await until(() => { if (launchError) throw launchError; return fs.existsSync(portFile); }, 'Browser did not start', 15000);
    const port = fs.readFileSync(portFile, 'utf8').split('\n')[0];
    const targets = await (await fetch('http://127.0.0.1:' + port + '/json/list')).json();
    socket = new WebSocket(targets.find(target => target.type === 'page').webSocketDebuggerUrl);
    await new Promise((resolve, reject) => { socket.onopen = resolve; socket.onerror = reject; });
    let sequence = 0;
    const pending = new Map(), exceptions = [];
    socket.onmessage = ({ data }) => {
      const message = JSON.parse(data);
      if (message.id) { const job = pending.get(message.id); pending.delete(message.id); if (job) message.error ? job.reject(new Error(JSON.stringify(message.error))) : job.resolve(message.result); }
      if (message.method === 'Runtime.exceptionThrown') exceptions.push(message.params.exceptionDetails);
    };
    const call = (method, params = {}) => new Promise((resolve, reject) => {
      const id = ++sequence; pending.set(id, { resolve, reject }); socket.send(JSON.stringify({ id, method, params }));
    });
    const evaluate = async expression => {
      const result = await call('Runtime.evaluate', { expression, returnByValue: true, awaitPromise: true });
      if (result.exceptionDetails) throw new Error(JSON.stringify(result.exceptionDetails));
      return result.result.value;
    };
    await call('Page.enable'); await call('Runtime.enable');
    // Keep animation checks independent of the Windows accessibility preference.
    // Reduced motion is explicitly exercised later in this suite.
    await call('Emulation.setEmulatedMedia', { features: [{ name: 'prefers-reduced-motion', value: 'no-preference' }] });
    await call('Emulation.setDeviceMetricsOverride', { width: 1440, height: 1120, deviceScaleFactor: 1, mobile: false });
    const navigate = async route => {
      const url = new URL(route, base);
      await call('Page.navigate', { url: url.href });
      await until(() => evaluate('document.readyState === "complete" && location.pathname === ' + JSON.stringify(url.pathname)), 'Page did not load');
    };
    const rows = () => evaluate('[...document.querySelectorAll(".track:not([aria-hidden])")].map(c => ({entry: Number(c.dataset.entryId), current: c.dataset.current, text:c.textContent}))');
    const screenshot = async name => {
      const shot = await call('Page.captureScreenshot', { format: 'png', captureBeyondViewport: false });
      fs.writeFileSync(path.join(output, name + '.png'), Buffer.from(shot.data, 'base64'));
    };

    await navigate('/master-overlay/settings');
    await until(() => evaluate('document.querySelector("h1").textContent === "One track leads. Your history follows."'), 'English must be the default');
    assert.equal(await evaluate('document.documentElement.lang'), 'en');
    await evaluate('document.querySelector("[data-language]").value="de"; document.querySelector("[data-language]").dispatchEvent(new Event("change"))');
    await until(() => evaluate('document.querySelector("iframe").contentDocument?.querySelectorAll(".track").length === 4'), 'Settings preview missing tracks');
    await delay(750);
    await screenshot('master-settings');
    await evaluate('document.getElementById("history").value="2"; document.getElementById("history").dispatchEvent(new Event("input",{bubbles:true})); document.querySelector("input[value=album]").click()');
    await until(() => evaluate('document.getElementById("url").value.includes("history=2") && !new URL(document.getElementById("url").value).searchParams.get("fields").includes("album")'), 'Settings did not update OBS URL');
    await navigate('/master-overlay/settings');
    assert.equal(await evaluate('document.getElementById("history").value'), '2', 'Settings did not survive reload');
    await evaluate('document.getElementById("historyScale").value="0.65"; document.getElementById("historyScale").dispatchEvent(new Event("input", {bubbles:true})); document.getElementById("align").value="right"; document.getElementById("align").dispatchEvent(new Event("input", {bubbles:true}))');
    await until(() => evaluate('document.getElementById("url").value.includes("historyScale=0.65&align=right")'), 'Scale/alignment missing from OBS URL');
    assert.equal(await evaluate('document.getElementById("scale-value").textContent'), '0,65×');
    await navigate('/master-overlay/settings');
    assert.deepEqual(await evaluate('[document.getElementById("historyScale").value, document.getElementById("align").value]'), ['0.65', 'right'], 'Scale/alignment settings not persisted');
    await delay(850);
    await screenshot('master-settings-scaled');

    // Measure actual painted cards and their flow slots, not just CSS declarations.
    const geometry = () => evaluate(`(() => {
      const rect = el => { const r = el.getBoundingClientRect(); return {left:r.left, right:r.right, top:r.top, bottom:r.bottom, width:r.width, height:r.height}; };
      return [...document.querySelectorAll('.track:not([aria-hidden])')].map(card => ({slot:rect(card), card:rect(card.querySelector('.card')), cover:rect(card.querySelector('.art'))}));
    })()`);
    for (const align of ['left', 'center', 'right']) {
      await navigate('/master-overlay?history=2&duration=0&historyScale=0.65&align=' + align);
      await until(async () => (await rows()).length === 3, 'Scaled history missing');
      const [master, previous, older] = await geometry();
      assert.ok(Math.abs(previous.card.width / master.card.width - .65) < .002, 'History card width not scaled uniformly');
      assert.ok(Math.abs(previous.cover.width / master.cover.width - .65) < .002, 'Cover not scaled with card');
      assert.ok(Math.abs(previous.card.height / master.card.height - .65) < .01, 'History card height not scaled');
      assert.ok(Math.abs(older.slot.top - previous.slot.bottom - 10) < 1, 'Scaling left an unscaled empty slot');
      const anchor = rect => align === 'left' ? rect.left : align === 'right' ? rect.right : (rect.left + rect.right) / 2;
      assert.ok(Math.abs(anchor(master.card) - anchor(previous.card)) < 1, 'History alignment does not match master');
      const viewport = await evaluate('innerWidth');
      assert.ok(Math.abs(anchor(master.card) - (align === 'left' ? 8 : align === 'right' ? viewport - 8 : viewport / 2)) < 1, 'Whole overlay alignment ignored');
      assert.equal(await evaluate('document.querySelector("[data-current=true] [data-value=bpm]").textContent'), '128,5');
      assert.equal(await evaluate('document.querySelector("[data-current=true] [data-value=originalBpm]").textContent'), '126,25');
      if (align === 'right') await screenshot('master-overlay-scaled-right');
    }
    for (const [input, expected] of [['0.01', .2], ['0.20', .2], ['1.00', 1], ['4', 1], ['invalid', 1]]) {
      await navigate('/master-overlay?history=1&duration=0&historyScale=' + input);
      await until(async () => (await rows()).length === 2, 'Scale boundary setup failed');
      const [master, previous] = await geometry();
      assert.ok(Math.abs(previous.card.width / master.card.width - expected) < .002, 'Scale bounds/default not respected: ' + input);
    }

    await navigate('/master-overlay?history=2&duration=650&historyScale=0.4&align=center');
    await until(async () => (await rows()).length === 3, 'Scaled animation setup failed');
    await delay(700);
    await evaluate('window.scalingCard = document.querySelector(".track[data-current=true]")');
    history = [current, ...history]; current = track(20, 120, 'Skalierter Übergang');
    await until(() => evaluate('document.querySelector(".track[data-current=true]")?.dataset.entryId === "20"'), 'Scaled transition not started');
    assert.ok(await evaluate('window.scalingCard.getAnimations().length > 0'), 'Master did not animate into smaller history card: '+JSON.stringify(await evaluate('({reduced:matchMedia("(prefers-reduced-motion: reduce)").matches,connected:window.scalingCard.isConnected,id:window.scalingCard.dataset.entryId,animations:document.getAnimations().map(a=>({state:a.playState,time:a.currentTime,duration:a.effect.getTiming().duration}))})')));
    const intermediate = await evaluate('window.scalingCard.querySelector(".card").getBoundingClientRect().width');
    assert.ok(intermediate > 720 * .4 && intermediate <= 720.5, 'Scaling transition jumped directly to its final width');
    await delay(700);
    const [scaledMaster, scaledPrevious] = await geometry();
    assert.ok(Math.abs(scaledPrevious.card.width / scaledMaster.card.width - .4) < .002, 'Scaled transition ended at wrong width');
    current.bpm = 133.75;
    await until(() => evaluate('document.querySelector("[data-current=true] [data-value=bpm]").textContent === "133,75"'), 'Current tempo did not update independently');
    assert.equal(await evaluate('document.querySelector("[data-current=true] [data-value=originalBpm]").textContent'), '126,25', 'Original tempo changed with current tempo');
    assert.equal(await evaluate('document.querySelector("[data-current=false] [data-value=bpm]").textContent'), '128,5', 'History tempo no longer shows captured value');
    current.originalBpm = null;
    await until(() => evaluate('document.querySelector("[data-current=true] [data-value=originalBpm]").textContent === "—"'), 'Missing original tempo must show a dash');

    // Restore the original sequence for the existing transition/regression checks.
    current = track(4, 104, 'Night Drive “Live”');
    history = [track(3, 103, 'First Light', 'Studio North'), track(2, 102, 'Between the Lines'), track(1, 101, 'New Beginnings')];

    await navigate('/master-overlay?history=3&fields=title,artist,album,key,bpm,cover&duration=650&width=720');
    await until(async () => (await rows()).length === 4, 'Master overlay missing initial rows');
    await delay(700);
    await screenshot('master-overlay');
    await evaluate('window.savedCard = document.querySelector(".track[data-current=true]")');
    history = [current, ...history]; current = track(5, 105, 'Neuer Master');
    await until(() => evaluate('document.querySelector(".track[data-current=true]")?.dataset.entryId === "5"'), 'Master change not rendered');
    assert.ok(await evaluate('document.getAnimations().length > 0'), 'No smooth transition running');
    assert.ok(await evaluate('window.savedCard === [...document.querySelectorAll(".track")].find(card => card.dataset.entryId === "4")'), 'Previous card was rebuilt instead of moving into history');
    await delay(750);
    assert.deepEqual((await rows()).map(row => row.entry), [5, 4, 3, 2]);
    assert.equal(await evaluate('document.querySelectorAll(".track").length'), 4, 'Outgoing card leaked');
    current.bpm = 129.75;
    await until(() => evaluate('document.querySelector(".track[data-current=true] [data-field=bpm]").textContent.includes("129,75")'), 'Live BPM not updated');
    assert.equal(await evaluate('document.getAnimations().length'), 0, 'BPM update restarted animation');
    current.title = '<img src=x onerror="window.injectionExecuted=true">';
    await until(() => evaluate('document.querySelector("h1").textContent.startsWith("<img")'), 'Title not shown as text');
    assert.equal(await evaluate('Boolean(window.injectionExecuted) || document.querySelectorAll("h1 img").length > 0'), false, 'Track metadata executed markup');
    const saved = current; current = null;
    await until(() => evaluate('!document.querySelector(".track[data-current=true]:not([aria-hidden])")'), 'Unknown master remained current');
    current = saved;
    await until(() => evaluate('document.querySelector(".track[data-current=true]:not([aria-hidden])")?.dataset.entryId === "5"'), 'Master did not recover during fade');
    await delay(750);
    assert.equal(await evaluate('document.querySelectorAll(".track").length'), 4, 'Recovery leaked cards');

    await navigate('/master-overlay?history=0&fields=title&duration=0');
    await until(async () => (await rows()).length === 1, 'History zero still renders history');
    const callsBefore = coverCalls;
    history = [current, ...history]; current = track(6, 106, 'Nur Titel');
    await until(() => evaluate('document.querySelector("h1").textContent === "Nur Titel"'), 'Title-only mode not updated');
    assert.equal(coverCalls, callsBefore, 'Hidden cover triggered a request');
    assert.ok(await evaluate('["artist","album","key","bpm","cover"].every(f => document.querySelector(`[data-field="${f}"]`).hidden)'), 'Field choices ignored');
    assert.equal(await evaluate('document.getAnimations().length'), 0, 'Zero duration still animates');

    await navigate('/master-overlay?history=2&duration=2000&historyScale=0.2&align=right');
    await until(async () => (await rows()).length === 3, 'Rapid transition setup failed');
    for (let id = 7; id <= 10; ++id) {
      history = [current, ...history]; current = track(id, 100 + id, 'Track ' + id);
      await until(() => evaluate(`document.querySelector('.track[data-current=true]:not([aria-hidden])')?.dataset.entryId === '${id}'`), 'Rapid track change lost');
    }
    await delay(2100);
    assert.deepEqual((await rows()).map(row => row.entry), [10, 9, 8]);
    assert.equal(await evaluate('document.querySelectorAll(".track").length'), 3, 'Rapid transitions leaked outgoing cards');
    await call('Emulation.setEmulatedMedia', { features: [{ name: 'prefers-reduced-motion', value: 'reduce' }] });
    history = [current, ...history]; current = track(11, 111, 'Weniger Bewegung');
    await until(() => evaluate('document.querySelector("h1").textContent === "Weniger Bewegung"'), 'Reduced-motion transition failed');
    assert.equal(await evaluate('document.getAnimations().length'), 0, 'Reduced motion ignored');
    status = 'disconnected';
    await until(() => evaluate('!document.querySelector(".track[data-current=true]:not([aria-hidden])")'), 'Disconnect kept current master');
    assert.deepEqual((await rows()).map(row => row.entry), [10, 9]);
    await navigate('/overlay?deck=1');
    await until(() => evaluate('document.getElementById("bpm")?.textContent === "128,5"'), 'Deck overlay current BPM missing');
    assert.equal(await evaluate('document.getElementById("original-bpm").textContent'), '126,25', 'Deck overlay original BPM missing');
    deckData.bpm = 135;
    await until(() => evaluate('document.getElementById("bpm").textContent === "135"'), 'Deck overlay current BPM did not update');
    assert.equal(await evaluate('document.getElementById("original-bpm").textContent'), '126,25', 'Deck overlay original BPM changed with current tempo');
    deckData.originalBpm = null;
    await until(() => evaluate('document.getElementById("original-bpm").textContent === "—"'), 'Deck overlay missing original BPM fallback broken');
    await call('Emulation.setDeviceMetricsOverride', { width: 360, height: 700, deviceScaleFactor: 1, mobile: false });
    await screenshot('deck-overlay-bpm-mobile');

    await call('Emulation.setDeviceMetricsOverride', { width: 1440, height: 1000, deviceScaleFactor: 1, mobile: false });
    status = 'connected';
    current = track(30, 130, 'Timeline test');
    await navigate('/master-overlay?timeline=1&history=2&duration=0&historyScale=0.5');
    await until(() => evaluate('document.querySelector(".timeline:not([hidden]) [data-time=position]")?.textContent === "1:05"'), 'Master timeline missing');
    assert.equal(await evaluate('document.querySelectorAll(".timeline:not([hidden])").length'), 1, 'Timeline shown on history');
    assert.equal(await evaluate('document.querySelector(".timeline:not([hidden]) [data-time=duration]").textContent'), '4:00');
    current.positionMs = 120000;
    await until(() => evaluate('document.querySelector(".time-bar").getAttribute("aria-valuenow") === "50.00"'), 'Live progress not updated');
    current.positionMs = -1200;
    await until(() => evaluate('document.querySelector("[data-time=position]").textContent === "−0:01"'), 'Negative preroll not displayed');
    assert.equal(await evaluate('document.querySelector(".time-bar").getAttribute("aria-valuenow")'), '0.00');
    current.positionMs = 300000;
    await until(() => evaluate('document.querySelector(".time-bar").getAttribute("aria-valuenow") === "100.00"'), 'Progress overflow not clamped');
    current.positionMs = null;
    await until(() => evaluate('document.querySelector(".timeline").dataset.available === "false"'), 'Missing timeline presented as known');
    assert.equal(await evaluate('document.querySelector(".time-bar").hasAttribute("aria-valuenow")'), false);
    current.positionMs = 65000;
    await until(() => evaluate('document.querySelector("[data-time=position]").textContent === "1:05"'), 'Timeline did not recover');
    await delay(650);
    assert.equal(await evaluate('document.querySelector("[data-time=position]").textContent'), '1:05', 'Paused timeline advanced without a sample');
    history = [current, ...history]; current = track(31, 131, 'Next timeline');
    await until(() => evaluate('document.querySelector("[data-current=true]")?.dataset.trackId === "131"'), 'Timeline transition failed');
    assert.equal(await evaluate('document.querySelectorAll(".timeline:not([hidden])").length'), 1);
    await screenshot('master-timeline');

    await navigate('/overlay/settings?deck=2');
    await until(() => evaluate('document.getElementById("deck").value === "2" && document.getElementById("url").value.includes("deck=2")'), 'Deck settings not ready');
    assert.equal(await evaluate('document.getElementById("history").closest("[data-mode]").hidden'), true);
    await evaluate('document.getElementById("timeline").click(); document.getElementById("preset").value="light"; document.getElementById("preset").dispatchEvent(new Event("change")); document.getElementById("fontSize").value="32"; document.getElementById("fontSize").dispatchEvent(new Event("change"))');
    await until(() => evaluate('new URL(document.getElementById("url").value).searchParams.get("timeline") === "1"'), 'Timeline missing from deck URL');
    await evaluate('document.getElementById("deck").value="3"; document.getElementById("deck").dispatchEvent(new Event("change"))');
    assert.equal(await evaluate('document.getElementById("fontSize").value'), '23', 'Deck settings leaked into another deck');
    await evaluate('document.getElementById("deck").value="2"; document.getElementById("deck").dispatchEvent(new Event("change"))');
    assert.equal(await evaluate('document.getElementById("fontSize").value'), '32', 'Deck settings were not retained');
    await evaluate('document.getElementById("all-decks").click()');
    assert.deepEqual(await evaluate('[1,2,3,4].map(id => JSON.parse(localStorage.getItem("deckstatus.deck.options."+id)).fontSize)'), [32,32,32,32]);
    await evaluate('document.querySelector("[data-language]").value="en"; document.querySelector("[data-language]").dispatchEvent(new Event("change"))');
    assert.equal(await evaluate('document.querySelector("h1").textContent'), 'Your decks. Your style.');
    assert.equal(await evaluate('new URL(document.getElementById("url").value).searchParams.get("lang")'), 'en');
    await until(() => evaluate('document.getElementById("preview").contentDocument?.querySelector("[data-time=position]")?.textContent === "1:05"'), 'Deck preview timeline missing');
    const customUrl = await evaluate('document.getElementById("open").getAttribute("href")');
    await screenshot('deck-settings');
    await navigate(customUrl);
    await until(() => evaluate('document.querySelector("[data-field=title]")?.textContent === "Deck 2"'), 'Configured deck not rendered');
    assert.deepEqual(await evaluate('(() => { const card = document.querySelector(".card"); return [getComputedStyle(card).backgroundColor, getComputedStyle(document.querySelector("h1")).fontSize, document.querySelector(".timeline").hidden, document.documentElement.lang]; })()'), ['rgba(243, 247, 248, 0.96)', '32px', false, 'en']);
    await screenshot('deck-custom-timeline');
    await navigate('/overlay?deck=1&timeline=1&layout=stacked&font=mono&coverSize=140&opacity=0&border=0&radius=0&badges=0&shadow=0');
    await until(() => evaluate('document.querySelector(".card") !== null'), 'Stacked overlay missing');
    assert.equal(await evaluate('getComputedStyle(document.querySelector(".card")).flexDirection'), 'column');
    assert.equal(await evaluate('document.querySelector(".tag").hidden'), true);
    assert.equal(await evaluate('Math.round(document.querySelector(".art").getBoundingClientRect().width)'), 140);
    await navigate('/overlay?deck=5');
    await until(() => evaluate('!document.getElementById("error").hidden'), 'Invalid deck not rejected');
    assert.equal(await evaluate('document.querySelectorAll(".track").length'), 0);
    await navigate('/overlay?deck=1&deck=2');
    await until(() => evaluate('!document.getElementById("error").hidden'), 'Duplicate deck not rejected');
    await navigate('/');
    await until(() => evaluate('document.querySelectorAll(".deck").length === 4 && document.querySelector("h1").textContent === "Live from your decks."'), 'Dashboard English missing');
    await evaluate('document.querySelector("[data-language]").value="de"; document.querySelector("[data-language]").dispatchEvent(new Event("change"))');
    assert.equal(await evaluate('document.querySelector("h1").textContent'), 'Live aus deinen Decks.');
    await navigate('/');
    await until(() => evaluate('document.documentElement.lang === "de" && document.querySelector("h1").textContent === "Live aus deinen Decks."'), 'Language did not persist');
    await screenshot('dashboard-de');

    await navigate('/overlay?deck=1&lang=constructor&background=invalid&opacity=999');
    await until(() => evaluate('document.querySelector(".card") !== null'), 'Unknown language fallback failed');
    assert.equal(await evaluate('document.documentElement.lang'), 'en');
    assert.equal(await evaluate('getComputedStyle(document.querySelector(".card")).backgroundColor'), 'rgb(17, 33, 34)');
    // Upgrade old browser settings without overwriting newer DeckStatus choices.
    await navigate('/overlay/settings?deck=2');
    await until(() => evaluate('document.getElementById("url").value.includes("deck=2")'), 'Migration setup failed');
    await evaluate('localStorage.clear(); localStorage.setItem("rb.language","de"); localStorage.setItem("rb.deck.options.2", JSON.stringify({ fontSize: 35, timeline: true, accent: "#ffcc00" })); localStorage.setItem("rb.master.options", JSON.stringify({ history: 3, historyScale: 0.6 }));');
    await navigate('/overlay/settings?deck=2');
    await until(() => evaluate('document.getElementById("fontSize").value === "35" && document.documentElement.lang === "de"'), 'Legacy deck settings/language not migrated');
    assert.equal(await evaluate('JSON.parse(localStorage.getItem("deckstatus.deck.options.2")).timeline'), true);
    await evaluate('document.getElementById("fontSize").value="29"; document.getElementById("fontSize").dispatchEvent(new Event("change"))');
    await navigate('/overlay/settings?deck=2');
    await until(() => evaluate('document.getElementById("fontSize").value === "29"'), 'Legacy settings overwrote new values');
    await navigate('/master-overlay/settings');
    await until(() => evaluate('document.getElementById("history").value === "3" && document.getElementById("historyScale").value === "0.6"'), 'Legacy master settings not migrated');
    await navigate('/');
    await until(() => evaluate('document.querySelectorAll(".deck").length === 4'), 'DeckStatus dashboard not ready');
    assert.equal(await evaluate('document.title'), 'DeckStatus');
    assert.equal(await evaluate('document.querySelector(".brand strong").textContent'), 'DeckStatus');
    assert.equal(exceptions.length, 0, JSON.stringify(exceptions));
    console.log('Browser passed: deck settings/persistence/apply-all, EN/DE/default/fallback, appearance, timeline/current-only/history/seek/preroll/paused/missing, scale limits and uniform geometry, all alignments, scaled transitions, both BPM values in both overlays, settings/persistence, history, fields, rapid changes, XSS, recovery, reduced motion and disconnect.');
    await call('Browser.close');
  } finally {
    if (socket) socket.close();
    if (browser && browser.exitCode === null) browser.kill();
    fixture.closeAllConnections();
    await new Promise(resolve => fixture.close(resolve));
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
