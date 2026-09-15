// Synthetic signals only. This test never opens a Windows audio device.
const assert=require('node:assert/strict');
const {withBrowser}=require('./browser_fixture.cjs');
let active=false, stale=false, failure=false, sequence=0, deviceId='',posts=[];
const signal=Array.from({length:1024},(_,i)=>Math.sin(2*Math.PI*i*13/1024)*.4+Math.sin(2*Math.PI*i*43/1024)*.2);
const devices=[{id:'input-1',name:'Studio interface · synthetic fixture',kind:'input'},{id:'loop-1',name:'Monitor output · synthetic fixture',kind:'loopback'},{id:'unsafe-name',name:'<img src=x onerror=alert(1)>',kind:'input'}];
function state(){return {status:failure?'error':active?'capturing':'stopped',error:failure?'audioDeviceLost':null,deviceId,deviceName:devices.find(d=>d.id===deviceId)?.name||'',fresh:active&&!stale&&!failure,sampleAgeMs:stale?500:active?5:null,sampleRate:48000,sequence:++sequence,left:active?signal:Array(1024).fill(0),right:active?signal:Array(1024).fill(0)};}
withBrowser((req,res,url)=>{
  if(!url.pathname.startsWith('/api/audio/'))return false;
  res.setHeader('Content-Type','application/json');
  if(url.pathname==='/api/audio/devices')res.end(JSON.stringify({devices,error:null}));
  else if(url.pathname==='/api/audio/state')res.end(JSON.stringify(state()));
  else if(url.pathname==='/api/audio/source'){let body='';req.on('data',chunk=>body+=chunk);req.on('end',()=>{const parsed=JSON.parse(body);posts.push(parsed);deviceId=parsed.deviceId;active=!!deviceId;failure=false;res.end(JSON.stringify(state()));});}
  else{res.statusCode=404;res.end();}return true;
},async({evaluate,navigate,until,delay,screenshot,root})=>{
  const change=async(id,value)=>evaluate(`(()=>{const el=document.getElementById(${JSON.stringify(id)});el.value=${JSON.stringify(String(value))};el.dispatchEvent(new Event('input',{bubbles:true}));el.dispatchEvent(new Event('change',{bubbles:true}));})()`);
  const pixels=()=>evaluate(`(()=>{const c=document.querySelector('canvas');const d=c.getContext('2d').getImageData(0,0,c.width,c.height).data;let n=0;for(let i=3;i<d.length;i+=4)if(d[i])n++;return n;})()`);
  await navigate('/waveform/settings');await until(()=>evaluate('document.getElementById("device").options.length===4'),'Device selection missing');
  assert.equal(await evaluate('document.documentElement.lang'),'en');
  assert.equal(await evaluate('document.querySelector("main select").id'),'device','Audio input must be first setting');
  assert.equal(await evaluate('document.querySelectorAll("#device img").length'),0,'Device names interpreted as HTML');
  assert.equal(posts.length,0,'Loading settings opened audio capture');
  await change('device','input-1');assert.equal(posts.length,0,'Choosing an input must not start recording');
  await evaluate('document.getElementById("start").click()');
  await until(()=>posts.length===1,'Start did not call capture API');assert.deepEqual(posts[0],{deviceId:'input-1'});
  await until(()=>evaluate('document.getElementById("preview").contentDocument?.querySelector("canvas")?.dataset.signal==="live"'),'Live waveform missing');
  assert.ok(!(await evaluate('document.getElementById("url").value')).includes('input-1'),'Source ID leaked into visual URL');
  await change('preset','neon');await change('gain','2.7');await change('height','300');
  await navigate('/waveform/settings');await until(()=>evaluate('document.getElementById("gain")?.value==="2.7"'),'Settings not persisted');
  assert.equal(await evaluate('document.getElementById("height").value'),'300');
  await evaluate('document.querySelector("[data-language]").value="de";document.querySelector("[data-language]").dispatchEvent(new Event("change"))');
  assert.equal(await evaluate('document.documentElement.lang'),'de');
  assert.equal(await evaluate('document.getElementById("start").textContent'),'Start / Quelle wechseln');
  assert.ok((await evaluate('document.getElementById("url").value')).includes('lang=de'));
  // Inspect the actual rendered settings, using only labelled synthetic input.
  await change('preset','mint');await delay(650);await screenshot('waveform-settings');
  // Public README images are generated separately with a mandatory English locale.
  const url=await evaluate('document.getElementById("url").value');
  for(const mode of ['line','fill','bars','mirror','radial','history']){
    await navigate('/waveform?mode='+mode+'&hideSilent=1&gain=2&glow=0&smoothing=0');
    await until(()=>evaluate('document.querySelector("canvas").dataset.signal==="live"'),'Mode not live: '+mode);await delay(mode==='history'?400:100);
    assert.ok(await pixels()>100,'Mode did not paint signal: '+mode);await screenshot('waveform-'+mode);
  }
  // Core math: a bin-centred sine must peak at the matching frequency.
  const math=await evaluate(`(async()=>{const {spectrum}=await import('/waveform-renderer.js');const {normalize}=await import('/waveform-options.js');const s=Float32Array.from({length:1024},(_,i)=>Math.sin(2*Math.PI*32*i/1024));const f=spectrum(s);return {peak:f.indexOf(Math.max(...f)),amplitude:f[32],safe:normalize({width:1e9,height:-1,color:'url(evil)',mode:'__proto__',gain:'NaN',minHz:9000,maxHz:100})};})()`);
  assert.equal(math.peak,32);assert.ok(Math.abs(math.amplitude-1)<.01);assert.equal(math.safe.width,2560);assert.equal(math.safe.height,80);assert.equal(math.safe.mode,'line');assert.equal(math.safe.gain,1.5);assert.equal(math.safe.maxHz,9100);assert.equal(math.safe.color,'#a8eccf');
  const rendering=await evaluate(`(async()=>{const {WaveformRenderer}=await import('/waveform-renderer.js');const {normalize}=await import('/waveform-options.js');
    const canvas=document.createElement('canvas'),r=new WaveformRenderer(canvas,normalize({mode:'line',width:200,height:100,opacity:50,trails:90,glow:0,smoothing:0,gain:1}));
    const left=Array(1024).fill(.5),right=Array(1024).fill(-.5);
    const state={status:'capturing',fresh:true,sampleAgeMs:0,sampleRate:48000,left,right};
    r.update(state,100);const cancelled=[...r.signal].every(x=>x===0);r.options.channel='left';
    for(let time=100;time<2100;time+=20){r.update({...state,sequence:time},time);r.draw(time);}
    const alpha=canvas.getContext('2d').getImageData(0,0,1,1).data[3],selected=r.signal[0];
    r.options.gate=.6;r.update(state,2120);return {cancelled,selected,alpha,gated:[...r.signal].every(x=>x===0)};})()`);
  assert.equal(rendering.cancelled,true,'Stereo mix must combine both channels');assert.equal(rendering.selected,.5,'Left channel selection ignored');
  assert.ok(rendering.alpha>=127&&rendering.alpha<=128,'Trails accumulated background opacity');assert.equal(rendering.gated,true,'Noise gate ignored');
  // Stale data and device loss must erase samples and history, including trails.
  stale=true;await until(()=>evaluate('document.querySelector("canvas").dataset.signal==="idle"'),'Stale audio stayed live');assert.equal(await pixels(),0);
  stale=false;failure=true;await navigate('/waveform/settings?lang=de');await until(()=>evaluate('document.getElementById("audio-status").textContent.includes("Audiogerät nicht verfügbar")'),'Device-loss translation missing');
  failure=false;await change('device','loop-1');await evaluate('document.getElementById("start").click()');await until(()=>deviceId==='loop-1','Switch to loopback failed');
  await evaluate('document.getElementById("stop").click()');await until(()=>!active,'Stop capture failed');
  assert.deepEqual(posts.at(-1),{deviceId:''});
  await navigate(new URL(url).pathname+new URL(url).search);await delay(500);assert.equal(await pixels(),0,'Stopped capture painted audio');
  // An active but silent input remains transparent; no synthesized activity.
  active=true;signal.fill(0);await delay(400);assert.equal(await pixels(),0,'Silence fabricated audio');
  console.log('Waveform UI: input consent, source switching/stop, 6 render modes, FFT, bounds, persistence, EN/DE, stale/error/silent handling passed.');
}).catch(error=>{console.error(error);process.exitCode=1;});
