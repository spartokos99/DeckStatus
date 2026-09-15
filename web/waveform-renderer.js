const N = 1024;
const clamp = (value, min, max) => Math.min(max, Math.max(min, value));
export function spectrum(samples) {
  const real = new Float64Array(N), imaginary = new Float64Array(N);
  for (let i = 0, j = 0; i < N; i++) {
    if (i) { let bit = N >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; }
    real[j] = samples[i] * (0.5 - 0.5 * Math.cos(2 * Math.PI * i / (N - 1)));
  }
  for (let size = 2; size <= N; size <<= 1) {
    const half = size >> 1;
    for (let base = 0; base < N; base += size) for (let j = 0; j < half; j++) {
      const angle = -2 * Math.PI * j / size, cos = Math.cos(angle), sin = Math.sin(angle);
      const a = base + j, b = a + half;
      const re = cos * real[b] - sin * imaginary[b], im = sin * real[b] + cos * imaginary[b];
      real[b] = real[a] - re; imaginary[b] = imaginary[a] - im; real[a] += re; imaginary[a] += im;
    }
  }
  return Float32Array.from(real.subarray(0, N/2), (value, i) => Math.hypot(value, imaginary[i]) * 4 / N);
}
export class WaveformRenderer {
  constructor(canvas, options) {
    this.canvas = canvas; this.options = options;
    this.layer = document.createElement('canvas'); this.context = this.layer.getContext('2d');
    this.layer.width = options.width; this.layer.height = options.height;
    canvas.width = options.width; canvas.height = options.height;
    this.signal = new Float32Array(N); this.smooth = new Float32Array(N); this.levels = new Float32Array(N/2);
    this.history = []; this.lastDraw = 0; this.fresh = false; this.lastData = 0; this.sampleRate = 48000;
  }
  update(state, now = performance.now()) {
    this.fresh = state?.status === 'capturing' && state.fresh === true &&
      Number.isFinite(state.sampleAgeMs) && state.sampleAgeMs <= 250 && state.sampleAgeMs >= 0 &&
      Array.isArray(state.left) && state.left.length === N && Array.isArray(state.right) && state.right.length === N;
    if (!this.fresh) { this.clearSignal(); return; }
    this.lastData = now;
    this.sampleRate = clamp(Number(state.sampleRate) || 48000, 8000, 384000);
    let sum = 0;
    for (let i = 0; i < N; i++) {
      const l = Number.isFinite(state.left[i]) ? clamp(state.left[i],-1,1) : 0;
      const r = Number.isFinite(state.right[i]) ? clamp(state.right[i],-1,1) : 0;
      const v = this.options.channel === 'left' ? l : this.options.channel === 'right' ? r : (l+r)/2;
      this.signal[i] = v; sum += v*v;
    }
    const rms = Math.sqrt(sum/N), gated = rms < this.options.gate;
    for (let i = 0; i < N; i++) this.signal[i] = gated ? 0 : clamp(this.signal[i] * this.options.gain,-1,1);
    this.silent = gated || rms === 0;
    if (state.sequence !== this.sequence) {
      this.history.push({time:now, value:gated ? 0 : clamp(rms * this.options.gain * 2,0,1)});
      this.sequence = state.sequence;
    }
    this.history = this.history.filter(point => now-point.time <= this.options.historySeconds*1000);
    this.targetLevels = spectrum(this.signal);
  }
  clearSignal() {
    this.fresh = false; this.silent = true; this.signal.fill(0); this.smooth.fill(0); this.levels.fill(0);
    this.targetLevels = null; this.history = []; this.sequence = undefined;
  }
  draw(now = performance.now()) {
    const o = this.options, ctx = this.context, w = o.width, h = o.height;
    const elapsed = this.lastDraw ? now-this.lastDraw : 1000/60;
    if (elapsed < 1000/Number(o.fps) - 1) return;
    this.lastDraw = now;
    if (now-this.lastData > 300) this.clearSignal();
    const visible = this.fresh && !(o.hideSilent && this.silent);
    if (!visible || !o.trails) ctx.clearRect(0,0,w,h);
    else {
      ctx.save(); ctx.globalCompositeOperation = 'destination-out';
      ctx.fillStyle = `rgba(0,0,0,${1-Math.pow(o.trails/100,elapsed/16.67)})`;
      ctx.fillRect(0,0,w,h); ctx.restore();
    }
    if (visible) {
      const alpha = 1-Math.pow(o.smoothing/100,elapsed/16.67);
      for (let i=0;i<N;i++) this.smooth[i] += (this.signal[i]-this.smooth[i])*alpha;
      for (let i=0;i<N/2;i++) this.levels[i] += ((this.targetLevels?.[i] || 0)-this.levels[i])*alpha;
      if (o.grid || o.centerLine) {
        ctx.save(); ctx.strokeStyle=o.color; ctx.globalAlpha=.15; ctx.lineWidth=1; ctx.beginPath();
        if(o.grid) { for(let x=w/8;x<w;x+=w/8){ctx.moveTo(x,0);ctx.lineTo(x,h);} for(let y=h/4;y<h;y+=h/4){ctx.moveTo(0,y);ctx.lineTo(w,y);} }
        if(o.centerLine){ctx.moveTo(0,h/2);ctx.lineTo(w,h/2);} ctx.stroke();ctx.restore();
      }
      ctx.save();
      const gradient = ctx.createLinearGradient(0,0,w,h); gradient.addColorStop(0,o.color); gradient.addColorStop(1,o.color2);
      ctx.strokeStyle=ctx.fillStyle=o.gradient ? gradient : o.color; ctx.lineWidth=o.lineWidth;
      ctx.lineCap='round';ctx.lineJoin='round';ctx.shadowBlur=o.glow;ctx.shadowColor=o.color;
      const margin = Math.max(o.lineWidth, o.glow/2, 4), amplitude=h/2-margin;
      if(o.mode==='line' || o.mode==='fill') {
        ctx.beginPath();
        for(let i=0;i<N;i++){const x=margin+i/(N-1)*(w-2*margin),y=h/2-this.smooth[i]*amplitude; i ? ctx.lineTo(x,y) : ctx.moveTo(x,y);}
        if(o.mode==='fill'){ctx.lineTo(w-margin,h/2);ctx.lineTo(margin,h/2);ctx.closePath();ctx.globalAlpha=.65;ctx.fill();}else ctx.stroke();
      } else if(o.mode==='history') {
        const points=this.history.filter(p=>now-p.time<=o.historySeconds*1000);
        if(points.length>1){ctx.beginPath();points.forEach((p,i)=>{const x=w*(1-(now-p.time)/(o.historySeconds*1000));const y=h/2-p.value*amplitude;i?ctx.lineTo(x,y):ctx.moveTo(x,y);});
          for(const p of [...points].reverse())ctx.lineTo(w*(1-(now-p.time)/(o.historySeconds*1000)),h/2+p.value*amplitude);
          ctx.closePath();ctx.fill();}
      } else {
        const nyquist=this.sampleRate/2,low=Math.min(o.minHz,nyquist*.9),high=Math.min(o.maxHz,nyquist);
        const level=index=>{
          const start=Math.max(1,Math.floor(low*Math.pow(high/low,index/o.bars)/nyquist*(N/2)));
          const end=Math.min(N/2,Math.max(start+1,Math.ceil(low*Math.pow(high/low,(index+1)/o.bars)/nyquist*(N/2))));
          let peak=0; for(let bin=start;bin<end;bin++)peak=Math.max(peak,this.levels[bin]);
          return clamp(Math.log10(1+peak*9),0,1);
        };
        for(let i=0;i<o.bars;i++){
          const value=level(i); if(value<.001)continue;
          if(o.mode==='radial'){
            const angle=i/o.bars*Math.PI*2-Math.PI/2, radius=Math.min(w,h)*.23, length=value*Math.max(1,Math.min(w,h)*.23-margin);
            ctx.beginPath();ctx.moveTo(w/2+Math.cos(angle)*radius,h/2+Math.sin(angle)*radius);ctx.lineTo(w/2+Math.cos(angle)*(radius+length),h/2+Math.sin(angle)*(radius+length));ctx.stroke();
          }else{
            const step=(w-2*margin)/o.bars, bw=step*(1-o.gap/100), bh=value*(o.mode==='mirror'?amplitude:h-2*margin);
            const x=margin+i*step+(step-bw)/2, y=o.mode==='mirror'?h/2-bh:h-margin-bh;
            const height=o.mode==='mirror'?bh*2:bh;ctx.beginPath();
            ctx.roundRect(x,y,bw,height,Math.min(o.rounding,bw/2,height/2));ctx.fill();
          }
        }
      }
      ctx.restore();
    }
    // Compose on a fresh surface: trails must never accumulate background opacity.
    const output=this.canvas.getContext('2d');output.clearRect(0,0,w,h);
    if(o.opacity){output.save();output.globalAlpha=o.opacity/100;output.fillStyle=o.background;output.fillRect(0,0,w,h);output.restore();}
    output.drawImage(this.layer,0,0);
    this.canvas.dataset.signal = visible ? 'live' : 'idle';
  }
}
