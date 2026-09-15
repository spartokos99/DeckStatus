// Optional artwork build step; normal C++ builds use the committed ICO.
// node tools/generate-icon.cjs [path/to/msedge.exe]
const fs = require('node:fs');
const path = require('node:path');
const { spawn } = require('node:child_process');
const root = path.resolve(__dirname, '..');
const output = path.join(root, 'build', 'icon-tools');
fs.mkdirSync(output, { recursive: true });
const profile = fs.mkdtempSync(path.join(output, 'edge-'));
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
(async () => {
  let browser, socket;
  try {
    browser = spawn(process.argv[2] || 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
      ['--headless=new', '--no-first-run', '--disable-background-networking', '--remote-debugging-port=0', '--user-data-dir=' + profile, 'about:blank'],
      { windowsHide: true, stdio: 'ignore' });
    let failure; browser.on('error', e => { failure = e; });
    const portFile = path.join(profile, 'DevToolsActivePort');
    for (let i = 0; !fs.existsSync(portFile); i++) { if (failure) throw failure; if (i > 300) throw Error('Browser startup timeout'); await delay(50); }
    const port = fs.readFileSync(portFile, 'utf8').split('\n')[0];
    const pages = await (await fetch('http://127.0.0.1:' + port + '/json/list')).json();
    socket = new WebSocket(pages.find(p => p.type === 'page').webSocketDebuggerUrl);
    await new Promise((resolve, reject) => { socket.onopen = resolve; socket.onerror = reject; });
    let sequence = 0; const pending = new Map();
    socket.onmessage = ({data}) => { const m = JSON.parse(data); const p = pending.get(m.id); if (p) { pending.delete(m.id); m.error ? p.reject(Error(JSON.stringify(m.error))) : p.resolve(m.result); } };
    const call = (method, params = {}) => new Promise((resolve, reject) => { const id = ++sequence; pending.set(id, {resolve, reject}); socket.send(JSON.stringify({id, method, params})); });
    await call('Emulation.setDefaultBackgroundColorOverride', { color: {r:0, g:0, b:0, a:0} });
    const svg = fs.readFileSync(path.join(root, 'web', 'icon.svg'), 'utf8');
    const sizes = [16, 20, 24, 32, 40, 48, 64, 128, 256];
    const pictures = [];
    for (const size of sizes) {
      await call('Emulation.setDeviceMetricsOverride', {width:size, height:size, deviceScaleFactor:1, mobile:false});
      await call('Runtime.evaluate', {expression: 'document.documentElement.innerHTML=' + JSON.stringify('<head><style>html,body{margin:0;background:transparent}svg{display:block;width:' + size + 'px;height:' + size + 'px}</style></head><body>' + svg + '</body>')});
      const shot = await call('Page.captureScreenshot', {format:'png', clip:{x:0,y:0,width:size,height:size,scale:1}});
      pictures.push(Buffer.from(shot.data, 'base64'));
    }
    const header = Buffer.alloc(6 + sizes.length * 16); header.writeUInt16LE(1, 2); header.writeUInt16LE(sizes.length, 4);
    let offset = header.length;
    sizes.forEach((size, index) => { const p = 6 + index * 16; header[p] = header[p+1] = size === 256 ? 0 : size; header.writeUInt16LE(1, p+4); header.writeUInt16LE(32, p+6); header.writeUInt32LE(pictures[index].length, p+8); header.writeUInt32LE(offset, p+12); offset += pictures[index].length; });
    fs.mkdirSync(path.join(root, 'assets'), {recursive:true});
    fs.writeFileSync(path.join(root, 'assets', 'DeckStatus.ico'), Buffer.concat([header, ...pictures]));
    fs.writeFileSync(path.join(root, 'docs', 'images', 'deckstatus-icon.png'), pictures.at(-1));
    console.log('Created DeckStatus.ico: 9 sizes, 16–256 px. Source: web/icon.svg');
    await call('Browser.close');
  } finally { socket?.close(); browser?.kill(); }
})().catch(e => { console.error(e); process.exitCode = 1; });
