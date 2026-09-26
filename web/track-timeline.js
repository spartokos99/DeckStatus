import {broadcastUrl} from './broadcast.js';
import {t} from './i18n.js';

const timelines=new WeakMap(), active=new Set();
const reduced=matchMedia('(prefers-reduced-motion: reduce)');
let frame=0,lastFrame=0;
const clamp=(value,min,max)=>Math.min(max,Math.max(min,value));

function schedule(){if(!frame&&!document.hidden&&!reduced.matches&&[...active].some(s=>s.rate>0&&performance.now()-s.at<500))frame=requestAnimationFrame(tick);}
function tick(now){frame=0;if(document.hidden)return;if(now-lastFrame>=1000/30){lastFrame=now;for(const state of active){if(!state.element.isConnected){disposeTimeline(state.element);continue;}draw(state,now);}}schedule();}
document.addEventListener('visibilitychange',()=>{if(document.hidden){cancelAnimationFrame(frame);frame=0;}else schedule();});

export function disposeTimeline(element){const state=timelines.get(element);if(!state)return;state.controller?.abort();active.delete(state);state.canvas.remove();timelines.delete(element);}

function draw(state,now=performance.now()){
  const {element,canvas,options,wave}=state,style=options.timelineStyle;
  const waveform=style==='waveform'||style==='overview';
  const ready=!waveform||!!wave;
  canvas.hidden=!ready;
  element.dataset.timelineStyle=ready?style:'bar';
  element.dataset.waveformAvailable=String(!!wave);
  const status=element.querySelector('.timeline-status');
  status.hidden=ready;status.textContent=ready?'':t('trackWaveformUnavailable');
  if(!ready)return;
  const width=Math.min(1600,Math.round(element.clientWidth)),height=options.timelineHeight;
  if(width<1)return;
  const ratio=Math.min(devicePixelRatio||1,2);
  if(canvas.width!==Math.round(width*ratio)||canvas.height!==Math.round(height*ratio)){canvas.width=Math.round(width*ratio);canvas.height=Math.round(height*ratio);}
  canvas.style.height=height+'px';
  const ctx=canvas.getContext('2d');ctx.setTransform(ratio,0,0,ratio,0,0);ctx.clearRect(0,0,width,height);
  const elapsed=reduced.matches?0:clamp(now-state.at,0,500);
  const position=clamp(state.position+elapsed*state.rate,0,state.duration),progress=position/state.duration;
  ctx.lineWidth=5;
  if(style==='ring'){
    const radius=height/2-7,x=width/2,y=height/2;
    ctx.strokeStyle=options.mutedColor;ctx.globalAlpha=.25;ctx.beginPath();ctx.arc(x,y,radius,0,Math.PI*2);ctx.stroke();
    ctx.globalAlpha=1;ctx.strokeStyle=options.accent;ctx.beginPath();ctx.arc(x,y,radius,-Math.PI/2,-Math.PI/2+progress*Math.PI*2);ctx.stroke();
    ctx.fillStyle=options.textColor;ctx.font='12px "Segoe UI", sans-serif';ctx.textAlign='center';ctx.textBaseline='middle';ctx.fillText(Math.round(progress*100)+'%',x,y);return;
  }
  const duration=wave.durationMs,span=style==='waveform'?options.timelineWindow*1000:duration;
  const start=style==='waveform'?position-span/2:0,columns=Math.ceil(width/2),count=wave.samples.length;
  for(let x=0;x<columns;x++){
    const from=(start+x/columns*span)/duration*count,to=(start+(x+1)/columns*span)/duration*count;
    if(to<0||from>=count)continue;
    let peak=0;
    for(let i=Math.max(0,Math.floor(from)),end=Math.min(count,Math.max(i+1,Math.ceil(to)));i<end;i++){const sample=wave.samples[i];if(((sample>>2)&31)>=((peak>>2)&31))peak=sample;}
    const amplitude=((peak>>2)&31)/31*(height-6);
    ctx.fillStyle=options.timelineColor?`rgb(${((peak>>13)&7)*255/7} ${((peak>>10)&7)*255/7} ${((peak>>7)&7)*255/7})`:options.accent;
    ctx.globalAlpha=start+x/columns*span<position ? .55 : 1;
    ctx.fillRect(x/columns*width,(height-amplitude)/2,Math.max(1,width/columns-0.5),Math.max(1,amplitude));
  }
  ctx.globalAlpha=1;ctx.fillStyle=options.textColor;
  const cursor=style==='waveform'?width/2:clamp(position/duration,0,1)*width;
  ctx.fillRect(Math.round(cursor),0,2,height);
  ctx.beginPath();ctx.moveTo(cursor-4,0);ctx.lineTo(cursor+6,0);ctx.lineTo(cursor+1,5);ctx.fill();
}

async function loadWave(state,url,track){
  const controller=new AbortController();state.controller=controller;state.attempt=performance.now();
  try{
    const response=await fetch(broadcastUrl(url),{cache:'no-store',signal:AbortSignal.any([controller.signal,AbortSignal.timeout(5000)])});
    if(!response.ok)return;
    const value=await response.json();
    if(!controller.signal.aborted&&state.track===track&&value.trackId===track&&value.available===true&&value.format==='rgb5'&&
      Number.isFinite(value.durationMs)&&value.durationMs>0&&value.durationMs<=14400000&&Array.isArray(value.samples)&&value.samples.length>0&&value.samples.length<=30000&&value.samples.every(n=>Number.isInteger(n)&&n>=0&&n<=65535))state.wave=value;
  }catch{/* Unavailable analysis keeps the ordinary progress bar. */}
  finally{if(state.controller===controller){state.controller=null;if(active.has(state))draw(state);}}
}

export function updateTimelineDesign(element,item,options,url){
  const style=options.timelineStyle||'bar',custom=!['bar','segments'].includes(style);
  if(element.hidden||element.dataset.available!=='true'||!custom){disposeTimeline(element);element.dataset.timelineStyle=style;return;}
  let state=timelines.get(element);
  if(!state){const canvas=document.createElement('canvas');canvas.className='track-waveform';canvas.setAttribute('aria-hidden','true');element.append(canvas);state={element,canvas,position:null,track:null,attempt:-Infinity,at:0,rate:0,wave:null};timelines.set(element,state);}
  const now=performance.now(),delta=item.positionMs-state.position,dt=now-state.at;
  if(state.track!==item.trackId||state.url!==url){state.controller?.abort();state.controller=null;state.wave=null;state.attempt=-Infinity;state.position=null;state.track=item.trackId;state.url=url;}
  state.rate=state.position!==null&&dt>0&&delta>0&&delta<2000&&item.playing!==false?clamp(delta/dt,0,4):0;
  Object.assign(state,{options,position:item.positionMs,duration:item.durationMs,at:now});active.add(state);
  if(['waveform','overview'].includes(style)&&!state.wave&&!state.controller&&now-state.attempt>=5000&&url)void loadWave(state,url,item.trackId);
  draw(state,now);schedule();
}
