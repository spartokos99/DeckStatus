// End-to-end Chromium test against the real EXE, with an isolated demo store.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),net=require('node:net');
const {spawn}=require('node:child_process');const {withBrowser}=require('./browser_fixture.cjs');
const root=path.resolve(process.env.DECKSTATUS_TEST_ROOT||'build/Release'),output=path.resolve('build/test-artifacts');fs.mkdirSync(output,{recursive:true});
const directory=fs.mkdtempSync(path.join(output,'portal browser ')),data=path.join(directory,'DeckStatus.data');
const delay=ms=>new Promise(resolve=>setTimeout(resolve,ms));
let child,log='',sceneId,sceneKey,ratingId;const password='Browser test passphrase 42!';
async function launch(port){log='';child=spawn(path.join(root,'DeckStatus.exe'),['--demo','--port',String(port),'--data-dir',data,'--network-config',path.join(directory,'network.json')],{cwd:root,windowsHide:true,stdio:['ignore','pipe','pipe']});let launchError;child.on('error',error=>launchError=error);child.stdout.on('data',chunk=>log+=chunk);child.stderr.on('data',chunk=>log+=chunk);for(let i=0;i<150;i++){if(launchError)throw launchError;if(child.exitCode!==null)throw Error('Test EXE exited during startup');try{if((await fetch('http://127.0.0.1:'+port+'/api/auth/me')).ok)return;}catch{}await delay(50);}throw Error('Test EXE did not start');}
async function stop(){if(!child||child.exitCode!==null)return;const closed=new Promise(resolve=>child.on('close',resolve));child.kill();await closed;child=null;}
(async()=>{
 try{
  const reservation=net.createServer();await new Promise(resolve=>reservation.listen(0,'127.0.0.1',resolve));const port=reservation.address().port;await new Promise(resolve=>reservation.close(resolve));await launch(port);
  const temporary=log.match(/Temporary password: ([a-f0-9]+)/)?.[1];assert.ok(temporary,'Missing initial credentials');
  const fixture=()=>false;fixture.baseUrl='http://127.0.0.1:'+port;
  await withBrowser(fixture,async({navigate,evaluate,until,call,screenshot})=>{
   const fill=async(id,value)=>evaluate(`document.getElementById(${JSON.stringify(id)}).value=${JSON.stringify(value)}`);
   const click=async selector=>evaluate(`document.querySelector(${JSON.stringify(selector)}).click()`);
   const change=async(id,value)=>{await fill(id,value);await evaluate(`document.getElementById(${JSON.stringify(id)}).dispatchEvent(new Event('change'))`);};
   const presetPage=async url=>{await navigate(url);await until(()=>evaluate('document.getElementById("component-presets")?.getAttribute("aria-busy")==="false"'),'Preset library missing');};
   const savePreset=async name=>{await fill('preset-name',name);await evaluate('document.getElementById("preset-name").dispatchEvent(new Event("input"))');await click('#preset-create');await until(()=>evaluate('document.getElementById("preset-message").textContent.includes("Preset saved")'),'Preset save failed');return evaluate('document.getElementById("saved-preset").value');};
   const signIn=async secret=>{await navigate('/login?lang=en');await fill('username','admin');await fill('password',secret);await evaluate('document.getElementById("login-form").requestSubmit()');};
   await signIn(temporary);await until(()=>evaluate('location.pathname==="/account/password"'),'Forced password page missing');
   assert.equal(await evaluate('(async()=> (await fetch("/api/scenes")).status)()'),403);
   await fill('current-password',temporary);await fill('new-password',password);await fill('confirm-password',password);await evaluate('document.getElementById("password-form").requestSubmit()');
   await until(()=>evaluate('location.pathname==="/login"'),'Password redirect missing');await signIn(password);await until(()=>evaluate('location.pathname==="/"'),'Dashboard login failed');
   await navigate('/admin?lang=en');await until(()=>evaluate('document.getElementById("users-list").children.length===1'),'Admin list missing');
   await click('[data-tab=users]');await fill('user-name','helper');await fill('user-password','Temporary helper passphrase');await evaluate('document.getElementById("user-form").requestSubmit()');await until(()=>evaluate('document.getElementById("users-list").children.length===2'),'User creation failed');
   await presetPage('/master-overlay/settings?history=2&historyScale=0.55&align=right&timeline=1&width=640&lang=en');
   const masterPreset=await savePreset('Studio <Master>');
   assert.equal(await evaluate('document.getElementById("saved-preset").selectedOptions[0].textContent'),'Studio <Master>','Preset name became markup');
   assert.equal(await evaluate('document.getElementById("app-mode").hidden'),false,'Overlay settings hid the current mode');
   await change('history',0);await click('#preset-load');assert.equal(await evaluate('document.getElementById("history").value'),'2','Master preset did not restore history');
   assert.equal(await evaluate('document.getElementById("historyScale").value'),'0.55');assert.equal(await evaluate('document.getElementById("align").value'),'right');
   await fill('preset-name','Studio master');await change('history',3);await click('#preset-update');await until(()=>evaluate('document.getElementById("preset-message").textContent.includes("Preset saved")'),'Preset update failed');
   await presetPage('/overlay/settings?deck=2&timeline=1&fields=title,artist,bpm&fontSize=30&lang=en');
   assert.equal(await evaluate('document.getElementById("saved-preset").options.length'),1,'Deck library contains master presets');
   const deckPreset=await savePreset('Studio deck');await change('deck',1);await click('#preset-load');
   assert.equal(await evaluate('document.getElementById("deck").value'),'2','Deck choice not restored');
   assert.equal(await evaluate('document.getElementById("fontSize").value'),'30','Deck design not restored');
   await presetPage('/waveform/settings?lang=en');await change('mode','bars');await evaluate('document.getElementById("mode").dispatchEvent(new Event("input"))');
   await fill('gain',2.3);await evaluate('document.getElementById("gain").dispatchEvent(new Event("input"))');
   const wavePreset=await savePreset('Studio waveform');
   await fill('gain',4);await evaluate('document.getElementById("gain").dispatchEvent(new Event("input"))');await click('#preset-load');assert.equal(await evaluate('document.getElementById("gain").value'),'2.3','Waveform preset did not restore signal settings');
   assert.equal(await evaluate('(async()=> (await (await fetch("/api/audio/state")).json()).status)()'),'stopped','Preset started audio capture');
   await savePreset('Disposable waveform');await evaluate('window.confirm=()=>true');await click('#preset-delete');await until(()=>evaluate('document.getElementById("preset-message").textContent.includes("Preset deleted")'),'Preset deletion failed');
   await call('Emulation.setDeviceMetricsOverride',{width:390,height:844,deviceScaleFactor:1,mobile:false});
   assert.equal(await evaluate('document.documentElement.scrollWidth<=innerWidth'),true,'Preset library overflows on mobile');
   await call('Emulation.setDeviceMetricsOverride',{width:1440,height:1120,deviceScaleFactor:1,mobile:false});
   await navigate('/scenes?lang=en');await until(()=>evaluate('document.getElementById("scene-component").options.length===4 && !document.getElementById("scene-presets-refresh").disabled'),'Scene preset library missing');
   for(const preset of [masterPreset,wavePreset,deckPreset]){await change('scene-component',preset);await click('#scene-add-preset');}
   assert.equal(await evaluate('document.getElementById("option-deck").value'),'2');
   await fill('option-fontSize',31);await evaluate('document.getElementById("option-fontSize").dispatchEvent(new Event("input"))');
   await fill('scene-name','Studio scene');await evaluate('document.getElementById("scene-name").dispatchEvent(new Event("input"))');
   await fill('item-x','420');await evaluate('document.getElementById("item-x").dispatchEvent(new Event("input"))');await fill('item-y','560');await evaluate('document.getElementById("item-y").dispatchEvent(new Event("input"))');
   await until(()=>evaluate('document.querySelectorAll(".scene-item iframe").length===3'),'Scene layers missing');
   const rect=await evaluate('(()=>{const r=document.querySelector(".scene-item[data-selected=true]").getBoundingClientRect();return {x:r.x+40,y:r.y+40};})()');
   await call('Input.dispatchMouseEvent',{type:'mousePressed',x:rect.x,y:rect.y,button:'left',clickCount:1});await call('Input.dispatchMouseEvent',{type:'mouseMoved',x:rect.x+35,y:rect.y+25,button:'left',buttons:1});await call('Input.dispatchMouseEvent',{type:'mouseReleased',x:rect.x+35,y:rect.y+25,button:'left',clickCount:1});
   assert.ok(Number(await evaluate('document.getElementById("item-x").value'))>420,'Dragging did not move layer');
   await click('#scene-save');await until(()=>evaluate('document.getElementById("scene-url").value.includes("key=")'),'Scene save failed');
   const saved=await evaluate('(async()=> (await (await fetch("/api/scenes")).json()).scenes[0])()');sceneId=saved.id;sceneKey=saved.key;assert.equal(saved.items.length,3);
   assert.equal(saved.items[0].name,'Studio master');assert.equal(saved.items[0].options.history,3);assert.equal(saved.items[0].options.historyScale,.55);assert.equal(saved.items[0].options.align,'right');
   assert.equal(saved.items[1].options.gain,2.3);assert.equal(saved.items[1].options.mode,'bars');assert.equal(saved.items[2].options.fontSize,31);assert.deepEqual(saved.items[2].options.fields,['title','artist','bpm']);
   const savedPresets=await evaluate('(async()=> (await (await fetch("/api/presets")).json()).presets)()');
   assert.equal(savedPresets.find(p=>p.id===deckPreset).options.fontSize,30,'Editing a scene modified the preset');
   assert.equal(savedPresets.find(p=>p.id===masterPreset).revision,2,'Preset update did not keep its identity');
   await evaluate(`(async()=>{const p=(await (await fetch('/api/presets')).json()).presets.find(p=>p.id===${JSON.stringify(masterPreset)});p.options.history=7;await fetch('/api/presets',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:'save',id:p.id,revision:p.revision,preset:p})});})()`);
   assert.equal(await evaluate('(async()=> (await (await fetch("/api/scenes")).json()).scenes[0].items[0].options.history)()'),3,'Updating a preset changed a saved scene');
   await screenshot('scene-editor-en');
   await call('Emulation.setDeviceMetricsOverride',{width:390,height:844,deviceScaleFactor:1,mobile:false});assert.equal(await evaluate('document.documentElement.scrollWidth<=innerWidth'),true,'Editor overflow on mobile');
   await call('Emulation.setDeviceMetricsOverride',{width:1440,height:1120,deviceScaleFactor:1,mobile:false});
   await evaluate('document.querySelector("[data-language]").value="de";document.querySelector("[data-language]").dispatchEvent(new Event("change"))');assert.equal(await evaluate('document.getElementById("scene-new").textContent'),'Neue Szene');
   await click('#nav-logout');await until(()=>evaluate('location.pathname==="/login"'),'Logout failed');
   await navigate('/history?lang=en');await until(()=>evaluate('document.querySelector("[data-stars=\\"5\\"]")&&!document.querySelector("[data-stars=\\"5\\"]").disabled'),'Public history rating missing');
   assert.equal(await evaluate('document.querySelector("nav").hidden'),true,'Anonymous history shows admin navigation');
   ratingId=await evaluate('document.querySelector("tbody tr").dataset.ratingId');await click('tbody tr [data-stars="5"]');await until(()=>evaluate('document.getElementById("rating-feedback").textContent.includes("saved")'),'Rating not saved');
   await click('tbody tr [data-stars="3"]');await until(()=>evaluate('document.querySelector("tbody tr [data-stars=\\"3\\"]").getAttribute("aria-pressed")==="true"'),'Vote update failed');
   await screenshot('public-history-en');
   await navigate('/scene?scene='+sceneId+'&key='+sceneKey);await until(()=>evaluate('document.querySelectorAll("iframe").length===3'),'Anonymous OBS scene missing');
   await until(()=>evaluate('[...document.querySelectorAll("iframe")].filter(f=>f.src.includes("overlay")).every(f=>f.contentDocument?.querySelector(".track"))'),'OBS child overlays failed authentication');
   assert.equal(await evaluate('(async()=> (await fetch("/api/admin/users")).status)()'),401);
   assert.equal(await evaluate('(async()=> (await fetch("/api/presets"+location.search)).status)()'),401,'OBS key exposed the preset library');
   await evaluate(`(async()=>{await fetch('/api/auth/login',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({username:'admin',password:${JSON.stringify(password)}})});const s=(await (await fetch('/api/scenes')).json()).scenes[0];window.originalSceneFrame=document.querySelector('iframe');s.items[0].x=230;await fetch('/api/scenes',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:'save',id:s.id,revision:s.revision,scene:s})});await fetch('/api/auth/logout',{method:'POST',headers:{'Content-Type':'application/json'},body:'{}'});})()`);
   await until(()=>evaluate('document.querySelector(".scene-item").style.left==="230px"'),'Saved geometry did not update the open OBS scene');
   assert.equal(await evaluate('window.originalSceneFrame===document.querySelector("iframe")'),true,'Geometry update restarted the renderer');
   await signIn(password);await until(()=>evaluate('location.pathname==="/"'),'Relogin failed');await navigate('/admin?lang=en');
   await until(()=>evaluate('document.querySelectorAll("#rating-rows tr").length>0'),'Persistent ratings missing in admin');
   const ratings=await evaluate('(async()=> (await (await fetch("/api/admin/ratings")).json()).tracks)()');assert.equal(ratings.find(row=>row.id===ratingId).count,1);assert.equal(ratings.find(row=>row.id===ratingId).average,3);
   await screenshot('admin-ratings-en');
   // A second editor changes the scene; stale saves must not overwrite it.
   const conflict=await evaluate(`(async()=>{const s=(await (await fetch('/api/scenes')).json()).scenes[0];const body={action:'save',id:s.id,revision:s.revision,scene:{...s,name:'Updated scene'}};await fetch('/api/scenes',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});return (await fetch('/api/scenes',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})).status;})()`);assert.equal(conflict,409);
   await stop();await launch(port);assert.ok(!log.includes('Temporary password:'),'Bootstrap reappeared after restart');
   assert.equal(await evaluate('(async()=> (await fetch("/api/admin/ratings")).status)()'),401,'Session survived restart');
   await signIn(password);await until(()=>evaluate('location.pathname==="/"'),'Login after restart failed');
   const persisted=await evaluate('(async()=> ({ratings:(await (await fetch("/api/admin/ratings")).json()).tracks,scenes:(await (await fetch("/api/scenes")).json()).scenes}))()');assert.equal(persisted.scenes[0].name,'Updated scene');assert.equal(persisted.ratings.find(row=>row.id===ratingId).average,3);
   const persistedPresets=await evaluate('(async()=> (await (await fetch("/api/presets")).json()).presets)()');assert.equal(persistedPresets.length,3);assert.equal(persistedPresets.find(p=>p.id===masterPreset).options.history,7);assert.equal(persisted.scenes[0].items[0].options.history,3);
   // A different browser preference state still has the same server presets.
   await evaluate('localStorage.clear()');await presetPage('/overlay/settings?lang=de');await change('saved-preset',deckPreset);await click('#preset-load');
   assert.equal(await evaluate('document.getElementById("deck").value'),'2');assert.equal(await evaluate('document.getElementById("preset-create").textContent'),'Als neu speichern');
   console.log('Real EXE browser tests passed: authentication, users, public voting, all component preset CRUD/EN-DE/mobile/restart, scene insertion and independent snapshots, drag/save/OBS tokens and conflicts.');
  });
 }finally{await stop();const resolved=path.resolve(directory);assert.equal(path.dirname(resolved),output,'Unexpected test cleanup path');fs.rmSync(resolved,{recursive:true,force:true});}
})().catch(error=>{console.error(error);process.exitCode=1;});
