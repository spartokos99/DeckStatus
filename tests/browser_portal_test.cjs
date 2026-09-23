// End-to-end Chromium test against the real EXE, with an isolated demo store.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),net=require('node:net');
const {spawn}=require('node:child_process');const {withBrowser}=require('./browser_fixture.cjs');
const {httpsProxy}=require('./https_proxy_fixture.cjs');
const root=path.resolve(process.env.DECKSTATUS_TEST_ROOT||'build/Release'),output=path.resolve('build/test-artifacts');fs.mkdirSync(output,{recursive:true});
const directory=fs.mkdtempSync(path.join(output,'portal browser ')),data=path.join(directory,'DeckStatus.data');
const delay=ms=>new Promise(resolve=>setTimeout(resolve,ms));
let child,proxy,log='',sceneId,sceneKey,ratingId,savedAudioId='';const password='Browser test passphrase 42!';
async function launch(port){log='';child=spawn(path.join(root,'DeckStatus.exe'),['--demo','--port',String(port),'--data-dir',data,'--network-config',path.join(directory,'network.json')],{cwd:root,windowsHide:true,stdio:['ignore','pipe','pipe']});let launchError;child.on('error',error=>launchError=error);child.stdout.on('data',chunk=>log+=chunk);child.stderr.on('data',chunk=>log+=chunk);for(let i=0;i<150;i++){if(launchError)throw launchError;if(child.exitCode!==null)throw Error('Test EXE exited during startup');try{if((await fetch('http://127.0.0.1:'+port+'/api/auth/me')).ok)return;}catch{}await delay(50);}throw Error('Test EXE did not start');}
async function stop(){if(!child||child.exitCode!==null)return;const closed=new Promise(resolve=>child.on('close',resolve));child.kill();await closed;child=null;}
(async()=>{
 try{
  const reservation=net.createServer();await new Promise(resolve=>reservation.listen(0,'127.0.0.1',resolve));const port=reservation.address().port;await new Promise(resolve=>reservation.close(resolve));
  if(process.env.DECKSTATUS_TEST_PROXY==='1'){
   proxy=await httpsProxy(port,directory);
   fs.writeFileSync(path.join(directory,'network.json'),JSON.stringify({bind:'127.0.0.1',port,allowRemoteControl:false,publicDomain:proxy.domain}));
  }
  await launch(port);
  const temporary=log.match(/Temporary password: ([a-f0-9]+)/)?.[1];assert.ok(temporary,'Missing initial credentials');
  const fixture=()=>false;fixture.baseUrl=proxy?.baseUrl||'http://127.0.0.1:'+port;fixture.browserArgs=proxy?.browserArgs;
  await withBrowser(fixture,async({navigate,evaluate,until,call,screenshot})=>{
   // Trust only inside this disposable browser session; never change system trust.
   if(proxy)await call('Security.setIgnoreCertificateErrors',{ignore:true});
   const fill=async(id,value)=>evaluate(`document.getElementById(${JSON.stringify(id)}).value=${JSON.stringify(value)}`);
   const click=async selector=>evaluate(`document.querySelector(${JSON.stringify(selector)}).click()`);
   const change=async(id,value)=>{await fill(id,value);await evaluate(`document.getElementById(${JSON.stringify(id)}).dispatchEvent(new Event('change'))`);};
   const presetPage=async url=>{await navigate(url);await until(()=>evaluate('document.getElementById("component-presets")?.getAttribute("aria-busy")==="false"'),'Preset library missing');};
   const savePreset=async name=>{await fill('preset-name',name);await evaluate('document.getElementById("preset-name").dispatchEvent(new Event("input"))');await click('#preset-create');await until(()=>evaluate('document.getElementById("preset-message").textContent.includes("Preset saved")'),'Preset save failed');return evaluate('document.getElementById("saved-preset").value');};
   const checkOverlayUrl=async()=>{
    const localUrl=await evaluate('document.getElementById("url").value');
    assert.equal(new URL(localUrl).origin,'http://127.0.0.1:'+port,'Overlay link lost localhost with public domain enabled');
    assert.equal((await fetch(localUrl)).status,200,'Local OBS overlay failed without browser session');
    assert.equal(await evaluate('new URL(document.getElementById("preview").src).origin'),fixture.baseUrl,'Preview did not stay on the current origin');
   };
   const signIn=async secret=>{await navigate('/login?lang=en');await evaluate('import("/auth.js").then(()=>true)');await fill('username','admin');await fill('password',secret);await evaluate('document.getElementById("login-form").requestSubmit()');};
   await signIn(temporary);await until(()=>evaluate('location.pathname==="/account/password"'),'Forced password page missing');
   await evaluate('import("/auth.js").then(()=>true)');
   assert.equal(await evaluate('(async()=> (await fetch("/api/scenes")).status)()'),403);
   await fill('current-password',temporary);await fill('new-password',password);await fill('confirm-password',password);await evaluate('document.getElementById("password-form").requestSubmit()');
   try{await until(()=>evaluate('location.pathname==="/login"'),'Password redirect missing');}catch(error){throw Error(error.message+'; '+await evaluate('JSON.stringify({path:location.pathname,message:document.getElementById("auth-message")?.textContent})'));}await signIn(password);await until(()=>evaluate('location.pathname==="/"'),'Dashboard login failed');
   const cookies=(await call('Storage.getCookies')).cookies;
   await until(()=>evaluate('document.querySelectorAll("#decks [data-field=overlay]").length===4'),'Dashboard deck links missing');
   for(const url of await evaluate('[...document.querySelectorAll("#decks [data-field=overlay]")].map(a=>a.href)')){
    assert.equal(new URL(url).origin,'http://127.0.0.1:'+port);assert.equal((await fetch(url)).status,200,'Dashboard local overlay link failed');
   }
   assert.equal(cookies.find(c=>c.name==='deckstatus_session')?.secure,Boolean(proxy),'Session Secure flag does not match access mode');
   if(proxy){
    const access=await evaluate('(async()=> ({app:await (await fetch("/api/app")).json(),network:await (await fetch("/api/network")).json()}))()');
    assert.equal(access.app.canControl,false);assert.equal(access.network.canConfigure,false,'HTTPS proxy inherited local permissions');
   }
   await navigate('/admin?lang=en');await until(()=>evaluate('document.getElementById("users-list").children.length===1'),'Admin list missing');
   await click('[data-tab=audio]');
   const audio=await evaluate('(async()=> (await fetch("/api/admin/audio")).json())()');assert.equal(audio.settings.autoStart,false);assert.equal(audio.state.status,'stopped');
   if(proxy){
    assert.equal(audio.canControl,false);assert.equal(await evaluate('(async()=> (await fetch("/api/admin/audio",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({action:"stop"})})).status)()'),403);
   }else{
    const devices=await evaluate('(async()=> (await (await fetch("/api/audio/devices")).json()).devices)()');
    if(devices.length){
     savedAudioId=devices[0].id;await until(()=>evaluate('!document.querySelector("#admin-audio-device").disabled'),'Audio controls missing');
     await change('admin-audio-device',savedAudioId);await click('#admin-audio-save');
     await until(()=>evaluate('document.querySelector("#admin-audio-message").textContent.includes("Audio settings saved")'),'Audio selection save failed');
     assert.equal(await evaluate('(async()=> (await (await fetch("/api/audio/state")).json()).status)()'),'stopped','Saving input opened a real device');
    }
   }
   await click('[data-tab=users]');await fill('user-name','helper');await fill('user-password','Temporary helper passphrase');await evaluate('document.getElementById("user-form").requestSubmit()');await until(()=>evaluate('document.getElementById("users-list").children.length===2'),'User creation failed');
   await presetPage('/master-overlay/settings?history=2&historyScale=0.55&align=right&timeline=1&width=640&lang=en');
   const trackOption=async(name,value)=>evaluate(`{const input=document.querySelector('[data-track-option="${name}"]');if(input.type==='checkbox')input.checked=${JSON.stringify(value)};else input.value=${JSON.stringify(value)};input.dispatchEvent(new Event('change'));}`);
   await trackOption('bpmInteger',true);await trackOption('hideMissing',true);await trackOption('historySameFields',false);
   await evaluate(`for(const input of document.querySelectorAll('[data-history-field]')){input.checked=['title','label'].includes(input.value);input.dispatchEvent(new Event('change'));}`);
   await trackOption('coverPosition','right');await trackOption('coverShape','round');await trackOption('coverSpin',true);
   await evaluate(`{const input=document.querySelector('[data-track-option="font"]');input.value='impact';input.dispatchEvent(new Event('input'));}`);
   for(const [name,value] of [['fontSize','46'],['fontStyle','italic'],['fontWeight','800']])await evaluate(`{const input=document.querySelector('[data-track-option="${name}"]');input.value=${JSON.stringify(value)};input.dispatchEvent(new Event('input'));}`);
   await evaluate(`document.querySelectorAll('.track-extras details').forEach(d=>d.open=true);document.querySelector('.track-extras').scrollIntoView();`);
   await screenshot('track-settings-en');
   await call('Emulation.setDeviceMetricsOverride',{width:390,height:844,deviceScaleFactor:1,mobile:false});
   assert.equal(await evaluate('document.documentElement.scrollWidth<=innerWidth'),true,'Expanded track settings overflow on mobile');
   await call('Emulation.setDeviceMetricsOverride',{width:1440,height:1120,deviceScaleFactor:1,mobile:false});
   const masterPreset=await savePreset('Studio <Master>');
   await checkOverlayUrl();
   assert.equal(await evaluate('document.getElementById("saved-preset").selectedOptions[0].textContent'),'Studio <Master>','Preset name became markup');
   assert.equal(await evaluate('document.getElementById("app-mode").hidden'),false,'Overlay settings hid the current mode');
   await change('history',0);await click('#preset-load');assert.equal(await evaluate('document.getElementById("history").value'),'2','Master preset did not restore history');
   assert.equal(await evaluate(`document.querySelector('[data-track-option="font"]').value`),'impact','Individual font was lost on preset load');
   for(const [name,value] of [['fontSize','46'],['fontStyle','italic'],['fontWeight','800']])assert.equal(await evaluate(`document.querySelector('[data-track-option="${name}"]').value`),value,'Individual typography was lost on preset load');
   assert.deepEqual(await evaluate(`[...document.querySelectorAll('[data-history-field]:checked')].map(i=>i.value)`),['title','label'],'History fields were lost on preset load');
   assert.equal(await evaluate('document.getElementById("historyScale").value'),'0.55');assert.equal(await evaluate('document.getElementById("align").value'),'right');
   await fill('preset-name','Studio master');await change('history',3);await click('#preset-update');await until(()=>evaluate('document.getElementById("preset-message").textContent.includes("Preset saved")'),'Preset update failed');
   await presetPage('/overlay/settings?deck=2&timeline=1&fields=title,artist,bpm&fontSize=30&lang=en');
   assert.equal(await evaluate('document.getElementById("saved-preset").options.length'),1,'Deck library contains master presets');
   const deckPreset=await savePreset('Studio deck');await change('deck',1);await click('#preset-load');
   await checkOverlayUrl();
   assert.equal(await evaluate('[...document.querySelectorAll("#deck-links a")].every(a=>new URL(a.href).hostname==="127.0.0.1")'),true);
   assert.equal(await evaluate('document.getElementById("deck").value'),'2','Deck choice not restored');
   assert.equal(await evaluate('document.getElementById("fontSize").value'),'30','Deck design not restored');
   await presetPage('/waveform/settings?lang=en');await change('mode','bars');await evaluate('document.getElementById("mode").dispatchEvent(new Event("input"))');
   await fill('gain',2.3);await evaluate('document.getElementById("gain").dispatchEvent(new Event("input"))');
   const wavePreset=await savePreset('Studio waveform');
   await until(()=>evaluate('document.getElementById("preview").getAttribute("src")?.startsWith("/waveform?")'),'Waveform preview missing');await checkOverlayUrl();
   await fill('gain',4);await evaluate('document.getElementById("gain").dispatchEvent(new Event("input"))');await click('#preset-load');assert.equal(await evaluate('document.getElementById("gain").value'),'2.3','Waveform preset did not restore signal settings');
   assert.equal(await evaluate('(async()=> (await (await fetch("/api/audio/state")).json()).status)()'),'stopped','Preset started audio capture');
   await savePreset('Disposable waveform');await evaluate('window.confirm=()=>true');await click('#preset-delete');await until(()=>evaluate('document.getElementById("preset-message").textContent.includes("Preset deleted")'),'Preset deletion failed');
   await call('Emulation.setDeviceMetricsOverride',{width:390,height:844,deviceScaleFactor:1,mobile:false});
   assert.equal(await evaluate('document.documentElement.scrollWidth<=innerWidth'),true,'Preset library overflows on mobile');
   await call('Emulation.setDeviceMetricsOverride',{width:1440,height:1120,deviceScaleFactor:1,mobile:false});
   await navigate('/scenes?lang=en');await until(()=>evaluate('document.getElementById("scene-component").options.length===4 && !document.getElementById("scene-presets-refresh").disabled'),'Scene preset library missing');
   for(const preset of [masterPreset,wavePreset,deckPreset]){await change('scene-component',preset);await click('#scene-add-preset');}
   assert.equal(await evaluate('document.getElementById("option-deck").value'),'2');
   assert.equal(await evaluate('document.getElementById("item-design").disabled'),true,'Linked design should be edited through its preset');
   assert.equal(await evaluate('document.getElementById("option-deck").matches(":disabled")'),false,'Linked deck selection is disabled');
   await change('option-deck',1);
   await change('item-source-preset','');
   await fill('option-fontSize',31);await evaluate('document.getElementById("option-fontSize").dispatchEvent(new Event("input"))');
   await fill('scene-name','Studio scene');await evaluate('document.getElementById("scene-name").dispatchEvent(new Event("input"))');
   await fill('item-x','420');await evaluate('document.getElementById("item-x").dispatchEvent(new Event("input"))');await fill('item-y','560');await evaluate('document.getElementById("item-y").dispatchEvent(new Event("input"))');
   await until(()=>evaluate('document.querySelectorAll(".scene-item iframe").length===3'),'Scene layers missing');
   const rect=await evaluate('(()=>{const r=document.querySelector(".scene-item[data-selected=true]").getBoundingClientRect();return {x:r.x+40,y:r.y+40};})()');
   await call('Input.dispatchMouseEvent',{type:'mousePressed',x:rect.x,y:rect.y,button:'left',clickCount:1});await call('Input.dispatchMouseEvent',{type:'mouseMoved',x:rect.x+35,y:rect.y+25,button:'left',buttons:1});await call('Input.dispatchMouseEvent',{type:'mouseReleased',x:rect.x+35,y:rect.y+25,button:'left',clickCount:1});
   assert.ok(Number(await evaluate('document.getElementById("item-x").value'))>420,'Dragging did not move layer');
   await click('#scene-save');await until(()=>evaluate('document.getElementById("scene-url").value.includes("key=")'),'Scene save failed');
   const saved=await evaluate('(async()=> (await (await fetch("/api/scenes")).json()).scenes[0])()');sceneId=saved.id;sceneKey=saved.key;assert.equal(saved.items.length,3);
   const sceneUrl=await evaluate('document.getElementById("scene-url").value');
   assert.equal(new URL(sceneUrl).origin,'http://127.0.0.1:'+port,'Scene OBS URL lost localhost');
   assert.equal((await fetch(sceneUrl)).status,200,'Local scene key failed');
   assert.equal(saved.items[0].name,'Studio master');assert.equal(saved.items[0].options.history,3);assert.equal(saved.items[0].options.historyScale,.55);assert.equal(saved.items[0].options.align,'right');
   assert.equal(saved.items[2].options.deck,1,'Deck override was not saved');
   assert.equal(saved.items[0].options.fieldStyles.title.fontSize,46);assert.equal(saved.items[0].options.fieldStyles.title.fontStyle,'italic');assert.equal(saved.items[0].options.fieldStyles.title.fontWeight,800);
   assert.equal(saved.items[0].options.fieldStyles.title.font,'impact');assert.equal(saved.items[0].options.coverPosition,'right');assert.equal(saved.items[0].options.bpmInteger,true);assert.deepEqual(saved.items[0].options.historyFields,['title','label']);
   assert.equal(saved.items[1].options.gain,2.3);assert.equal(saved.items[1].options.mode,'bars');assert.equal(saved.items[2].options.fontSize,31);assert.deepEqual(saved.items[2].options.fields,['title','artist','bpm','currentBpm']);
   assert.equal(saved.items[0].presetId,masterPreset);assert.equal(saved.items[1].presetId,wavePreset);assert.equal(saved.items[2].presetId,'');
   const savedPresets=await evaluate('(async()=> (await (await fetch("/api/presets")).json()).presets)()');
   assert.equal(savedPresets.find(p=>p.id===deckPreset).options.fontSize,30,'Editing a scene modified the preset');
   assert.equal(savedPresets.find(p=>p.id===masterPreset).revision,2,'Preset update did not keep its identity');
   await evaluate(`(async()=>{const p=(await (await fetch('/api/presets')).json()).presets.find(p=>p.id===${JSON.stringify(masterPreset)});p.options.history=7;await fetch('/api/presets',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:'save',id:p.id,revision:p.revision,preset:p})});})()`);
   assert.equal(await evaluate('(async()=> (await (await fetch("/api/scenes")).json()).scenes[0].items[0].options.history)()'),7,'Updating a preset did not update the linked scene');
   const refreshed=await evaluate('(async()=> (await (await fetch("/api/scenes")).json()).scenes[0])()');assert.equal(refreshed.key,saved.key);assert.equal(refreshed.items[0].x,saved.items[0].x);assert.equal(refreshed.items[0].width,saved.items[0].width);assert.ok(refreshed.revision>saved.revision);
   await until(()=>evaluate(`new URL(document.querySelector('.scene-item iframe').src).searchParams.get('history')==='7'`),'Clean editor did not refresh the linked design');
   await screenshot('scene-editor-en');
   await call('Emulation.setDeviceMetricsOverride',{width:390,height:844,deviceScaleFactor:1,mobile:false});assert.equal(await evaluate('document.documentElement.scrollWidth<=innerWidth'),true,'Editor overflow on mobile');
   await call('Emulation.setDeviceMetricsOverride',{width:1440,height:1120,deviceScaleFactor:1,mobile:false});
   await evaluate('document.querySelector("[data-language]").value="de";document.querySelector("[data-language]").dispatchEvent(new Event("change"))');assert.equal(await evaluate('document.getElementById("scene-new").textContent'),'Neue Szene');
   await click('#nav-logout');await until(()=>evaluate('location.pathname==="/login"'),'Logout failed');
   await navigate('/history?lang=en');await until(()=>evaluate('document.querySelector("[data-stars]")?.disabled'),'Anonymous stars not disabled');
   assert.equal(await evaluate('document.querySelector("nav").hidden'),true,'Anonymous history shows admin navigation');
   assert.equal(await evaluate('document.getElementById("history-ratings-link").hidden'),true);
   assert.equal(await evaluate('(async()=> (await fetch("/api/public/rating",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({track:"fake",stars:5})})).status)()'),401,'Anonymous vote accepted');
   await screenshot('public-history-en');
   await navigate('/scene?scene='+sceneId+'&key='+sceneKey);await until(()=>evaluate('document.querySelectorAll("iframe").length===3'),'Anonymous OBS scene missing');
   await until(()=>evaluate('[...document.querySelectorAll("iframe")].filter(f=>f.src.includes("overlay")).every(f=>f.contentDocument?.querySelector(".track"))'),'OBS child overlays failed authentication');
   assert.equal(await evaluate('(async()=> (await fetch("/api/admin/users")).status)()'),401);
   assert.equal(await evaluate('(async()=> (await fetch("/api/presets"+location.search)).status)()'),401,'OBS key exposed the preset library');
   await evaluate(`(async()=>{await fetch('/api/auth/login',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({username:'admin',password:${JSON.stringify(password)}})});const s=(await (await fetch('/api/scenes')).json()).scenes[0];window.originalSceneFrame=document.querySelector('iframe');s.items[0].x=230;await fetch('/api/scenes',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:'save',id:s.id,revision:s.revision,scene:s})});await fetch('/api/auth/logout',{method:'POST',headers:{'Content-Type':'application/json'},body:'{}'});})()`);
   await until(()=>evaluate('document.querySelector(".scene-item").style.left==="230px"'),'Saved geometry did not update the open OBS scene');
   assert.equal(await evaluate('window.originalSceneFrame===document.querySelector("iframe")'),true,'Geometry update restarted the renderer');
   await signIn(password);await until(()=>evaluate('location.pathname==="/"'),'Relogin failed');await navigate('/admin?lang=en');
   await until(()=>evaluate('!document.getElementById("ratings-empty").hidden'),'Empty ratings not shown');
   await screenshot('admin-ratings-en');
   // A second editor changes the scene; stale saves must not overwrite it.
   const conflict=await evaluate(`(async()=>{const s=(await (await fetch('/api/scenes')).json()).scenes[0];const body={action:'save',id:s.id,revision:s.revision,scene:{...s,name:'Updated scene'}};await fetch('/api/scenes',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});return (await fetch('/api/scenes',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})).status;})()`);assert.equal(conflict,409);
   await stop();await launch(port);assert.ok(!log.includes('Temporary password:'),'Bootstrap reappeared after restart');
   assert.equal(await evaluate('(async()=> (await fetch("/api/admin/ratings")).status)()'),401,'Session survived restart');
   await signIn(password);await until(()=>evaluate('location.pathname==="/"'),'Login after restart failed');
   const restoredAudio=await evaluate('(async()=> (await fetch("/api/admin/audio")).json())()');assert.equal(restoredAudio.settings.deviceId,savedAudioId);assert.equal(restoredAudio.settings.autoStart,false);assert.equal(restoredAudio.state.status,'stopped');
   const persisted=await evaluate('(async()=> ({ratings:(await (await fetch("/api/admin/ratings")).json()).tracks,scenes:(await (await fetch("/api/scenes")).json()).scenes}))()');assert.equal(persisted.scenes[0].name,'Updated scene');assert.equal(persisted.ratings.length,0);
   const persistedPresets=await evaluate('(async()=> (await (await fetch("/api/presets")).json()).presets)()');assert.equal(persistedPresets.length,3);assert.equal(persistedPresets.find(p=>p.id===masterPreset).options.history,7);assert.equal(persisted.scenes[0].items[0].options.history,7);
   // A different browser preference state still has the same server presets.
   await evaluate('localStorage.clear()');await presetPage('/overlay/settings?lang=de');await change('saved-preset',deckPreset);await click('#preset-load');
   assert.equal(await evaluate('document.getElementById("deck").value'),'2');assert.equal(await evaluate('document.getElementById("preset-create").textContent'),'Als neu speichern');
   if(proxy){
    await evaluate('document.getElementById("open").target="_self";document.getElementById("open").click()');
    await until(()=>evaluate('location.hostname==="127.0.0.1"&&document.querySelector(".track")'),'Opening a local overlay from the domain was blocked');
    await navigate('/scenes?lang=en');await until(()=>evaluate('document.getElementById("scene-url").value.includes("key=")'),'Saved scene missing');
    await evaluate('document.getElementById("scene-open").target="_self";document.getElementById("scene-open").click()');
    await until(()=>evaluate('location.hostname==="127.0.0.1"&&document.querySelectorAll("iframe").length===3'),'Opening a local scene from the domain was blocked');
    await until(()=>evaluate('[...document.querySelectorAll("iframe")].filter(f=>f.src.includes("overlay")).every(f=>f.contentDocument?.querySelector(".track"))'),'Local scene child renderers did not load');
   }
   console.log('Real EXE browser tests passed: authentication, users, Twitch-required voting, component preset CRUD/EN-DE/mobile/restart, track design controls, linked scene updates and detached copies, drag/save/OBS tokens and conflicts.');
   if(proxy){
    assert.ok(proxy.requests.some(r=>r.method==='POST'&&r.path==='/api/public/rating'&&r.status===401),'HTTPS anonymous vote was not rejected');
    for(const route of ['/api/auth/login','/api/auth/password','/api/auth/logout','/api/scenes','/api/presets'])
     assert.ok(proxy.requests.some(r=>r.method==='POST'&&r.path===route&&r.host===proxy.domain&&r.origin===proxy.baseUrl&&r.status===200),'Missing successful HTTPS request: '+route);
    console.log('HTTPS reverse-proxy browser checks passed: original domain/Origin headers, secure cookies, remote permissions, localhost OBS links, same-origin previews and restart.');
   }
  });
 }finally{await stop();await proxy?.close();const resolved=path.resolve(directory);assert.equal(path.dirname(resolved),output,'Unexpected test cleanup path');fs.rmSync(resolved,{recursive:true,force:true});}
})().catch(error=>{console.error(error);process.exitCode=1;});
