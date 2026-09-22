// Extracted-package check. No production injection, device connection or audio capture.
const assert=require('node:assert/strict'),path=require('node:path'),net=require('node:net');
const {spawn}=require('node:child_process');
const {isolatedStore,authenticate}=require('./exe_auth.cjs');
const appVersion=require('../tools/version.cjs');
const root=path.resolve(process.argv[2]||'build/Release');
const delay=ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function port(){const server=net.createServer();await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));const value=server.address().port;await new Promise(resolve=>server.close(resolve));return value;}
async function run(args,test){
 const number=await port(),base='http://127.0.0.1:'+number,store=isolatedStore('portable smoke');
 const child=spawn(path.join(root,'DeckStatus.exe'),[...args,'--port',String(number),'--data-dir',store.data,'--network-config',path.join(store.folder,'network.json')],{cwd:root,windowsHide:true,stdio:['ignore','pipe','pipe']});
 let log='',cookie='';child.stdout.on('data',chunk=>log+=chunk);child.stderr.on('data',chunk=>log+=chunk);
 const closed=new Promise(resolve=>child.on('close',resolve));let error;child.on('error',e=>{error=e;});
 async function request(route,body){const response=await fetch(base+route,{signal:AbortSignal.timeout(4000),headers:{Cookie:cookie,...(body?{'Content-Type':'application/json'}:{})},...(body?{method:'POST',body:JSON.stringify(body)}:{})});const set=response.headers.getSetCookie().find(value=>value.startsWith('deckstatus_session='));if(set)cookie=set.split(';')[0];const data=response.headers.get('content-type')?.includes('application/json')?await response.json():await response.text();return {status:response.status,data};}
 try{
  let ready=false;for(let i=0;i<100;i++){if(error)throw error;if(child.exitCode!==null)throw Error('Test server exited during startup');try{await request('/api/auth/me');ready=true;break;}catch{await delay(50);}}
  assert.ok(ready,'Server did not start');await authenticate(request,log);await test(request,base);
 }finally{if(child.exitCode===null)child.kill();await closed;store.cleanup();}
}
(async()=>{
 for(const language of ['en','de'])await run(['--demo','--lang',language],async(request)=>{
  const app=(await request('/api/app')).data;assert.equal(app.mode,'rekordbox');assert.equal(app.version,appVersion);
  assert.equal((await request('/automations')).status,200);assert.equal((await request('/automations.js')).status,200);
  assert.deepEqual((await request('/api/admin/automations')).data.settings,{enabled:false,rules:[],revision:0});
  assert.deepEqual((await request('/api/public/twitch')).data,{available:false,user:null,pending:null});
  assert.equal((await request('/api/public/rating',{track:'fake',stars:5})).status,401);
  assert.equal((await request('/api/history')).data.canViewRatings,true);
  const state=(await request('/api/state')).data;assert.equal(state.decks[0].title,'Night Drive "Live"');assert.equal(state.decks[1].artist,'Studio North');
  assert.equal(state.decks[0].bpm,128);assert.equal(state.decks[0].originalBpm,126);assert.equal(state.decks[0].durationMs,240000);
  assert.equal((await request('/api/prolink/devices')).status,409);assert.equal((await request('/api/prolink/control',{action:'discover'})).status,409);
  assert.equal((await request('/api/audio/state')).data.status,'stopped');assert.ok((await request('/api/history')).data.total>=1);
 });
 await run(['--mode','prolink'],async(request,base)=>{
  const app=(await request('/api/app')).data;assert.equal(app.mode,'prolink');assert.equal(app.capabilities.rekordboxSetup,false);
  let setup=(await request('/api/prolink/devices')).data;assert.equal(setup.runtimeAvailable,true);assert.equal(setup.status,'stopped');
  assert.equal((await request('/api/rekordbox/status')).status,409);assert.equal((await request('/api/health')).status,503);
  assert.equal((await request('/api/prolink/control',{action:'play'})).status,400);
  assert.equal((await request('/api/prolink/control',{action:'connect',players:[1,1]})).status,400);
  for(const route of ['/','/prolink/settings','/rekordbox/settings','/overlay/settings','/master-overlay/settings','/history','/waveform/settings','/navigation.js','/navigation.css','/admin','/scenes','/automations','/automations.js','/api/admin/automations'])assert.equal((await request(route)).status,200);
  if(process.env.DECKSTATUS_TEST_DISCOVERY==='1'){
  // Passive discovery exercises the actual bundled JVM. No virtual device joins the network.
  assert.equal((await request('/api/prolink/control',{action:'discover'})).status,202);
  for(let i=0;i<100;i++){setup=(await request('/api/prolink/devices')).data;if(['discovering','error'].includes(setup.status))break;await delay(50);}
  assert.ok(setup.status==='discovering'||setup.message==='prolinkPortsBusy','Real helper startup failed: '+JSON.stringify(setup));
  assert.equal((await request('/api/state')).data.version,'PRO DJ LINK · Beat Link 8.0.0','Helper output must preserve UTF-8 across Windows pipes');
  console.log('Real ProLink helper discovery: '+setup.status+(setup.status==='error'?' (ports already occupied)':''));
  assert.equal((await request('/api/prolink/control',{action:'disconnect'})).status,202);
  for(let i=0;i<100;i++){setup=(await request('/api/prolink/devices')).data;if(setup.status==='stopped')break;await delay(50);}
  assert.equal(setup.status,'stopped');assert.equal((await request('/api/audio/state')).data.status,'stopped');
  }
  assert.equal((await request('/api/state')).data.decks.some(d=>d.loaded),false);
 });
 console.log('Portable smoke passed: authenticated EN/DE demo, default mode, idle ProLink/runtime, UI routes, mode/API gates and audio off. Discovery runs only with DECKSTATUS_TEST_DISCOVERY=1.');
})().catch(error=>{console.error(error);process.exitCode=1;});
