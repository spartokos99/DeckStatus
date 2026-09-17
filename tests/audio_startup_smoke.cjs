// Persist a deliberately nonexistent input in a disposable store: never opens hardware.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),net=require('node:net'),{spawn}=require('node:child_process');
const {isolatedStore,authenticate}=require('./exe_auth.cjs');
const root=path.resolve(process.env.DECKSTATUS_TEST_ROOT||'build/Release'),store=isolatedStore('audio startup');let child;
const delay=ms=>new Promise(r=>setTimeout(r,ms));
async function stop(){if(child&&child.exitCode===null){const closed=new Promise(r=>child.once('close',r));child.kill();await closed;}child=null;}
async function run(test){
 const reservation=net.createServer();await new Promise(r=>reservation.listen(0,'127.0.0.1',r));const port=reservation.address().port;await new Promise(r=>reservation.close(r));
 let log='',cookie='',error;child=spawn(path.join(root,'DeckStatus.exe'),['--demo','--port',String(port),'--data-dir',store.data,'--network-config',path.join(store.folder,'network.json')],{cwd:root,windowsHide:true,stdio:['ignore','pipe','pipe']});child.on('error',e=>error=e);child.stdout.on('data',b=>log+=b);child.stderr.on('data',b=>log+=b);
 const request=async(route,body)=>{const response=await fetch('http://127.0.0.1:'+port+route,{headers:{Cookie:cookie,...(body?{'Content-Type':'application/json'}:{})},...(body?{method:'POST',body:JSON.stringify(body)}:{})});const set=response.headers.getSetCookie().find(c=>c.startsWith('deckstatus_session='));if(set)cookie=set.split(';')[0];return {status:response.status,data:await response.json()};};
 let ready=false;for(let i=0;i<120;i++){if(error)throw error;try{await request('/api/auth/me');ready=true;break;}catch{}await delay(50);}assert.ok(ready,'Startup failed');await authenticate(request,log);await test(request);await stop();
}
(async()=>{try{
 await run(async request=>{const response=await request('/api/admin/audio');assert.equal(response.status,200);assert.equal(response.data.settings.autoStart,false);assert.equal(response.data.state.status,'stopped');});
 const file=path.join(store.data,'portal.json'),json=JSON.parse(fs.readFileSync(file));json.audioSettings={deviceId:'DeckStatus-nonexistent-test-endpoint',deviceName:'Disconnected test input',autoStart:true};fs.writeFileSync(file,JSON.stringify(json));
 await run(async request=>{const response=(await request('/api/admin/audio')).data;assert.equal(response.controlError,'audioDeviceLost');assert.equal(response.state.status,'stopped');assert.deepEqual(response.settings,json.audioSettings);
  assert.equal((await request('/api/admin/audio',{action:'save',deviceId:json.audioSettings.deviceId,autoStart:false})).status,200);
 });
 await run(async request=>{const response=(await request('/api/admin/audio')).data;assert.equal(response.controlError,null);assert.equal(response.settings.deviceId,json.audioSettings.deviceId);assert.equal(response.settings.autoStart,false);assert.equal(response.state.status,'stopped');});
 console.log('Real EXE audio startup smoke passed: defaults off, missing-device autostart error without fallback, settings retained, disabled-autostart restart. No audio device opened.');
}finally{await stop();store.cleanup();}})().catch(error=>{console.error(error);process.exitCode=1;});
