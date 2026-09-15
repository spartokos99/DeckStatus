import {normalize} from './waveform-options.js';
import {broadcastUrl} from './broadcast.js';
import {WaveformRenderer} from './waveform-renderer.js';
import {t} from './i18n.js';
const canvas = document.getElementById('waveform');
canvas.setAttribute('aria-label',t('waveLive'));
const options = normalize(Object.fromEntries(new URLSearchParams(location.search)));
const renderer = new WaveformRenderer(canvas,options);
let disposed = false, timer;
async function poll() {
  try {
    const response = await fetch(broadcastUrl('/api/audio/state'),{cache:'no-store',signal:AbortSignal.timeout(1500)});
    if(!response.ok)throw Error('Audio unavailable');
    renderer.update(await response.json());
  } catch { renderer.clearSignal(); }
  if(!disposed)timer=setTimeout(poll,40);
}
function draw(now){if(disposed)return;renderer.draw(now);requestAnimationFrame(draw);}
window.addEventListener('pagehide',()=>{disposed=true;clearTimeout(timer);});
poll();requestAnimationFrame(draw);
