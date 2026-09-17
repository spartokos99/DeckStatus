import {spectrum} from './waveform-renderer.js';
import {broadcastUrl} from './broadcast.js';
import {reactionOptions} from './creative-options.js';
const clamp=v=>Math.max(0,Math.min(1,v));
export class AudioSignal {
 constructor(){this.clear();}
 clear(){this.levels={rms:0,peak:0,bass:0,mid:0,high:0};this.lastData=-Infinity;this.sequence=null;this.fresh=false;}
 update(state,now=performance.now()){
  if(state?.status!=='capturing'||state.fresh!==true||!Number.isFinite(state.sampleAgeMs)||state.sampleAgeMs<0||state.sampleAgeMs>250||state.left?.length!==1024||state.right?.length!==1024){this.clear();return;}
  // Repeated/stalled buffers must not keep an effect alive forever.
  if(state.sequence===this.sequence)return;
  this.sequence=state.sequence;this.lastData=now;this.fresh=true;
  const samples=new Float32Array(1024);let sum=0,peak=0;
  for(let i=0;i<1024;i++){const l=Number.isFinite(state.left[i])?Math.max(-1,Math.min(1,state.left[i])):0,r=Number.isFinite(state.right[i])?Math.max(-1,Math.min(1,state.right[i])):0;samples[i]=(l+r)/2;sum+=(l*l+r*r)/2;peak=Math.max(peak,Math.abs(l),Math.abs(r));}
  const fft=spectrum(samples),rate=Number.isFinite(state.sampleRate)?Math.max(8000,Math.min(384000,state.sampleRate)):48000;
  const band=(low,high)=>{let result=0;for(let i=Math.max(1,Math.ceil(low*1024/rate));i<Math.min(512,Math.ceil(high*1024/rate));i++)result=Math.max(result,fft[i]);return clamp(result);};
  this.levels={rms:clamp(Math.sqrt(sum/1024)),peak:clamp(peak),bass:band(20,250),mid:band(250,4000),high:band(4000,20000)};
 }
 value(band,now=performance.now()){if(now-this.lastData>300){this.levels={rms:0,peak:0,bass:0,mid:0,high:0};this.fresh=false;return 0;}return this.levels[band]||0;}
}
export class Envelope {
 constructor(){this.value=0;this.last=null;}
 update(level,raw,now){const o=reactionOptions(raw),target=o.audioEnabled?clamp((level*o.audioGain-o.audioThreshold)/Math.max(.01,1-o.audioThreshold)):0,dt=this.last===null?16:Math.max(0,Math.min(100,now-this.last));this.last=now;const tau=target>this.value?o.audioAttack:o.audioRelease;this.value+=(target-this.value)*(tau?1-Math.exp(-dt/tau):1);if(this.value<.0001)this.value=0;return this.value;}
}
export function transform(node,item,level){const o=reactionOptions(item.options),v=o.audioEnabled?level:0;node.style.transform=`translate(${v*o.reactX}px,${v*o.reactY}px) rotate(${(item.rotation||0)+v*o.reactRotation}deg) scale(${Math.max(.1,1+v*o.reactScale)})`;node.style.transformOrigin='center';node.style.opacity=String(clamp((item.opacity??1)+v*o.reactOpacity));}
export class ReactiveLoop {
 constructor(read,draw,key,scene){this.read=read;this.draw=draw;this.key=key;this.scene=scene;this.signal=new AudioSignal();this.alive=true;this.pending=false;this.next=0;this.frame=0;this.abort=null;this.tick=now=>{if(!this.alive)return;const needed=this.read();if(!needed)this.signal.clear();else if(!this.pending&&now>=this.next)this.poll();this.draw(this.signal,now);this.frame=requestAnimationFrame(this.tick);};this.frame=requestAnimationFrame(this.tick);}
 async poll(){this.pending=true;this.abort=new AbortController();const timeout=setTimeout(()=>this.abort?.abort(),1500);try{const response=await fetch(broadcastUrl('/api/audio/state',this.key,this.scene),{cache:'no-store',signal:this.abort.signal});if(!response.ok)throw Error('Audio unavailable');const data=await response.json();if(this.alive)this.signal.update(data);}catch{this.signal.clear();}finally{clearTimeout(timeout);this.pending=false;this.next=performance.now()+40;}}
 stop(){this.alive=false;cancelAnimationFrame(this.frame);this.abort?.abort();this.signal.clear();}
}
