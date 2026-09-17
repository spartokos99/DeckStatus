// Synthetic audio only: verifies the real rendering modules, without opening a device.
const assert=require('node:assert/strict');
const {withBrowser}=require('./browser_fixture.cjs');
let sequence=0,amplitude=0,stale=false,posts=0,polls=0;
withBrowser((req,res,url)=>{
 if(url.pathname!=='/api/audio/state')return false;
 if(req.method!=='GET')posts++;
 polls++;res.setHeader('Content-Type','application/json');
 const samples=Array.from({length:1024},(_,i)=>amplitude*Math.sin(i*2*Math.PI*93.75/48000));
 res.end(JSON.stringify({status:'capturing',fresh:!stale,sampleAgeMs:stale?600:0,sequence:++sequence,sampleRate:48000,left:samples,right:samples}));return true;
},async({navigate,evaluate,until,delay,screenshot})=>{
 await navigate('/component/text?text=LIVE%20SESSION&audioEnabled=true&audioGain=2&audioThreshold=0.1&audioAttack=0&audioRelease=0&reactScale=0.5&reactX=40&reactY=-20&reactRotation=15&reactOpacity=-0.4');
 await until(()=>evaluate('document.querySelector(".creative-element")?.textContent==="LIVE SESSION"'),'Text missing');
 await until(()=>polls>0,'Reactive component did not request audio');
 assert.equal(await evaluate('document.querySelector("#component").style.transform'),'translate(0px, 0px) rotate(0deg) scale(1)');
 amplitude=1;
 await until(()=>evaluate('document.querySelector("#component").style.transform.includes("scale(1.5)")'),'Audio scaling missing');
 assert.equal(await evaluate('document.querySelector("#component").style.transform'),'translate(40px, -20px) rotate(15deg) scale(1.5)');
 assert.equal(await evaluate('document.querySelector("#component").style.opacity'),'0.6');
 stale=true;await until(()=>evaluate('document.querySelector("#component").style.transform.includes("scale(1)")'),'Stale audio did not reset motion');
 const checks=await evaluate(`(async()=>{
  const {AudioSignal,Envelope}=await import('/audio-reactivity.js');
  const signal=new AudioSignal(),samples=Array.from({length:1024},(_,i)=>Math.sin(i*2*Math.PI*93.75/48000));
  const state={status:'capturing',fresh:true,sampleAgeMs:0,sequence:10,sampleRate:48000,left:samples,right:samples};
  signal.update(state,0);const rms=signal.value('rms',1),bass=signal.value('bass',1),high=signal.value('high',1);
  signal.value('rms',400);signal.update(state,410);const stalled=signal.value('rms',411);
  signal.update({...state,sequence:11},420);const recovered=signal.value('rms',421);
  signal.update({...state,sequence:12,left:samples.map(()=>NaN)},430);const finite=Number.isFinite(signal.value('rms',431));
  const env=new Envelope(),o={audioEnabled:true,audioGain:1,audioThreshold:0,audioAttack:100,audioRelease:200};
  const attack=env.update(1,o,0),release=env.update(0,o,100);
  return {rms,bass,high,stalled,recovered,finite,attack,release};
 })()`);
 assert.ok(checks.rms>.7&&checks.rms<.71);assert.ok(checks.bass>checks.high*10);assert.equal(checks.stalled,0);assert.ok(checks.recovered>.7&&checks.finite);assert.ok(checks.release>0&&checks.release<checks.attack&&checks.attack<1);
 assert.equal(await evaluate(`(async()=>{const {boundedText}=await import('/creative-options.js');return boundedText('😀'.repeat(100),159);})()`),'😀'.repeat(39),'UTF-8 truncation split a character');
 // Use fixed timestamps to verify edge triggering, cooldown, decay and stale clearing.
 const flash=await evaluate(`(async()=>{
  const {CreativeRenderer}=await import('/creative-renderer.js');const node=document.createElement('div');node.style.cssText='width:64px;height:64px';document.body.append(node);
  const r=new CreativeRenderer(node,'fx');r.update({effect:'flash',audioEnabled:true,audioGain:1,audioThreshold:.2,intensity:1,flashDuration:100,cooldown:400,color:'#ffffff'});
  const alpha=()=>r.element.getContext('2d').getImageData(32,32,1,1).data[3];
  r.draw(1,1,0,true);const start=alpha();r.draw(1,1,110,true);const continuous=alpha();
  r.draw(0,0,150,true);r.draw(1,1,200,true);const limited=alpha();r.draw(0,0,450,true);r.draw(1,1,500,true);const next=alpha();
  r.draw(1,1,510,false);const stale=alpha();node.remove();return {start,continuous,limited,next,stale};
 })()`);
 assert.deepEqual(flash,{start:255,continuous:0,limited:0,next:255,stale:0});
 stale=false;await navigate('/component/fx?effect=fog&width=1280&height=720&audioEnabled=true&audioGain=2&audioThreshold=0&audioAttack=0&audioRelease=0&density=32');
 const pixels='[...document.querySelector("canvas").getContext("2d").getImageData(0,0,1280,720).data].some((v,i)=>i%4===3&&v>0)';
 await until(()=>evaluate('!!document.querySelector("canvas") && '+pixels),'Fog did not render');await screenshot('creative-fog-synthetic');
 stale=true;await until(async()=>!await evaluate(pixels),'Disconnected fog did not clear');
 await navigate('/component/text?text=%3Cimg%20src=x%20onerror=alert(1)%3E');
 await until(()=>evaluate('document.querySelector(".creative-element")?.textContent.startsWith("<img")'),'Literal text missing');
 assert.equal(await evaluate('document.querySelectorAll("img").length'),0,'Text became executable markup');
 await delay(100);const before=polls;await delay(250);assert.equal(polls,before,'Disabled audio still polls');assert.equal(posts,0,'Renderer started capture');
 // Whole-layer transformations must also wrap the existing track/waveform renderers.
 await evaluate(`(async()=>{
  const {renderScene}=await import('/scene-shared.js');const stage=document.createElement('div');stage.id='test-stage';stage.style.position='relative';document.body.append(stage);
  const item=(id,type,options)=>({id,type,x:0,y:0,width:640,height:240,opacity:1,rotation:5,visible:true,options:{...options,audioEnabled:true,audioGain:2,audioThreshold:0,audioAttack:0,audioRelease:0,reactScale:.25}});
  window.testScene={width:1280,height:720,background:'transparent',items:[item('text','text',{text:'Scene reaction'}),item('deck','deck',{deck:1}),item('wave','waveform',{})]};
  renderScene(stage,window.testScene);window.originalFrame=stage.querySelector('iframe');
 })()`);
 stale=false;amplitude=1;
 await until(()=>evaluate('[...document.querySelectorAll("#test-stage .scene-item")].every(n=>n.style.transform.includes("scale(1.25)"))'),'Reaction missing on legacy layer');
 await evaluate(`(async()=>{const {renderScene}=await import('/scene-shared.js');window.testScene.items[1].x=200;renderScene(document.querySelector('#test-stage'),window.testScene);})()`);
 assert.equal(await evaluate('window.originalFrame===document.querySelector("#test-stage iframe")'),true,'Geometry restarted legacy renderer');
 console.log('Creative synthetic browser tests passed: bands, attack/release, stale and stalled audio, transforms, fog, flash edges/cooldown, safe text, retained legacy frames and no capture commands.');
}).catch(error=>{console.error(error);process.exitCode=1;});
