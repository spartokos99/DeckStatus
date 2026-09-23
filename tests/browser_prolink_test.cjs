const assert=require('node:assert/strict');
const {withBrowser,appVersion}=require('./browser_fixture.cjs');
let mode='rekordbox',phase='stopped',devices=[],players=[],posts=[],failed=false;
let mapping=[],preferences={autoConnect:true,devices:[]};
const source=[
 {number:1,name:'CDJ-3000',address:'192.0.2.11',kind:'player',supported:true,selectable:true},
 {number:2,name:'CDJ-3000X',address:'192.0.2.12',kind:'player',supported:true,selectable:true,guidance:'prolink3000xHelp'},
 {number:3,name:'XDJ-AZ',address:'192.0.2.13',kind:'player',supported:true,selectable:true,guidance:'prolinkAzHelp'},
 {number:4,name:'XDJ-AZ',address:'192.0.2.13',kind:'player',supported:true,selectable:true,guidance:'prolinkAzHelp'},
 {number:33,name:'DJM-900NXS2',address:'192.0.2.33',kind:'mixer',supported:true,selectable:false,guidance:'prolinkMixerHelp'},
 {number:34,name:'DJM-A9',address:'192.0.2.34',kind:'mixer',supported:true,selectable:false,guidance:'prolinkMixerHelp'},
 {number:9,name:'<img src=x onerror=alert(1)>',address:'192.0.2.99',kind:'player',supported:false,selectable:false}
];
withBrowser((req,res,url)=>{
 if(!url.pathname.startsWith('/api/'))return false;res.setHeader('Content-Type','application/json');
 if(url.pathname==='/api/app')res.end(JSON.stringify({version:appVersion,mode,canControl:true,capabilities:{dashboard:true,history:true,deckOverlays:true,masterOverlay:true,audioWaveform:true,rekordboxSetup:mode==='rekordbox',prolinkSetup:mode==='prolink',networkSettings:true}}));
 else if(url.pathname==='/api/prolink/devices'){
  if(failed){res.statusCode=503;res.end('{}');return true;}
  res.end(JSON.stringify({status:phase,message:phase==='connected'?'prolinkConnected':'prolinkStopped',runtimeAvailable:true,devices:devices.map(d=>({...d,selected:players.includes(d.number),playing:phase==='connected'&&d.number===1,synced:phase==='connected',onAir:phase==='connected'&&d.number===1,master:d.number===1,firmware:'test fixture',bpm:128})),players,mapping,localAddress:phase==='connected'?'192.0.2.100':'',networkInterface:'Ethernet · synthetic fixture',virtualPlayer:phase==='connected'?7:null}));
 }else if(url.pathname==='/api/prolink/settings'){
  if(req.method==='POST'){let body='';req.on('data',chunk=>body+=chunk);req.on('end',()=>{preferences=JSON.parse(body);res.end(JSON.stringify(preferences));});}else res.end(JSON.stringify(preferences));
 }else if(url.pathname==='/api/prolink/control'){
  let body='';req.on('data',chunk=>body+=chunk);req.on('end',()=>{const command=JSON.parse(body);posts.push(command);if(command.action==='discover'){phase='discovering';devices=source;}if(command.action==='connect'){phase='connected';mapping=command.mapping;players=mapping.map(d=>d.player);}if(command.action==='disconnect'){phase='stopped';players=[];mapping=[];devices=[];}res.statusCode=202;res.end('{"accepted":true}');});
 }else if(url.pathname==='/api/state'||url.pathname==='/api/rekordbox/status')res.end(JSON.stringify({mode,status:'disconnected',decks:[],message:'prolinkStopped'}));
 else {res.statusCode=404;res.end('{}');}return true;
},async({evaluate,navigate,until,delay,screenshot,call})=>{
 await navigate('/prolink/settings?lang=en');await until(()=>evaluate('document.querySelector("#app-mode")?.textContent.includes("Rekordbox")'),'Default mode missing');
 assert.equal(await evaluate('document.querySelector("#prolink-content").hidden'),true);
 assert.equal(await evaluate('document.querySelector("[data-capability=prolinkSetup]").getAttribute("aria-disabled")'),'true');
 assert.equal(await evaluate('document.querySelector("[data-capability=prolinkSetup]").hasAttribute("href")'),false);
 assert.equal(await evaluate('document.querySelectorAll(".nav-group").length'),3);
 assert.equal(await evaluate('document.querySelectorAll(".nav-link").length'),15);
 assert.deepEqual(await evaluate('[...document.querySelectorAll(".nav-group-label")].map(el=>el.textContent)'),['Start','Stream','Connections']);
 assert.equal(await evaluate('document.querySelector(".nav-standalone[data-capability=admin]").parentElement.matches("nav")'),true,'Admin must stand alone');
 assert.equal(await evaluate('document.querySelector("[data-i18n=navAutomations]").closest(".nav-group")===document.querySelector("[data-capability=history]").closest(".nav-group")'),true,'Automations must belong to Stream');
 assert.deepEqual(await evaluate('[...document.querySelectorAll(".nav-components-menu a")].map(a=>a.dataset.href)'),['/overlay/settings','/master-overlay/settings','/waveform/settings','/components/text','/components/image','/components/fx']);
 assert.equal(await evaluate('document.querySelector("[data-capability=scenes]").closest(".nav-group")===document.querySelector("[data-capability=history]").closest(".nav-group")'),true);
 await evaluate('document.querySelector(".nav-components summary").focus()');
 await call('Input.dispatchKeyEvent',{type:'keyDown',key:'ArrowDown',windowsVirtualKeyCode:40});
 assert.equal(await evaluate('document.querySelector(".nav-components").open && document.activeElement.dataset.capability==="deckOverlays"'),true,'Dropdown keyboard open failed');
 await call('Input.dispatchKeyEvent',{type:'keyDown',key:'Escape',windowsVirtualKeyCode:27});
 assert.equal(await evaluate('!document.querySelector(".nav-components").open && document.activeElement.matches(".nav-components summary")'),true,'Dropdown Escape failed');
 assert.equal(posts.length,0,'Opening setup started networking');
 mode='prolink';await navigate('/prolink/settings?lang=en');await until(()=>evaluate('!document.querySelector("#prolink-content").hidden'),'ProLink settings missing');
 assert.equal(await evaluate('document.querySelector("[data-capability=rekordboxSetup]").hasAttribute("href")'),false);
 assert.equal(await evaluate('document.querySelector("[data-capability=audioWaveform]").getAttribute("aria-disabled")'),'false','Windows audio should stay available');
 await until(()=>evaluate('!document.getElementById("discover").disabled'),'Initial settings did not load');
 await evaluate('document.getElementById("discover").click()');await until(()=>evaluate('document.querySelectorAll(".device-card").length===7'),'Discovery list missing');
 assert.equal(await evaluate('document.querySelectorAll("#devices input[type=checkbox]").length'),4,'Mixer became selectable or shared-IP players collapsed');
 assert.ok(await evaluate('document.getElementById("devices").textContent.includes("OneLibrary") && document.getElementById("devices").textContent.includes("Connect to CDJ/XDJ/DJM")'),'New device guidance missing');
 assert.ok(await evaluate('document.querySelector("[data-i18n=prolinkMetadataHelp]").textContent.includes("fallback is disabled")'),'Metadata limitation missing');
 assert.equal(await evaluate('document.querySelectorAll(".device-card img").length'),0,'Device name was interpreted as HTML');
 assert.equal(posts.length,1,'Discovery connected automatically');
 for(const player of [1,2])await evaluate(`document.querySelector('[data-player="${player}"]').click()`);
 await evaluate(`{const select=document.querySelector('[data-deck-player="1"]');select.value='3';select.dispatchEvent(new Event('change'));}`);
 assert.equal(await evaluate(`Array.from(document.querySelector('[data-deck-player="2"]').options).some(o=>o.value==='3')`),false,'Occupied deck offered twice');
 await evaluate(`{const select=document.querySelector('[data-deck-player="2"]');select.value='4';select.dispatchEvent(new Event('change'));}`);
 await evaluate('document.getElementById("connect").click()');await until(()=>evaluate('document.getElementById("prolink-status").textContent.includes("Connected")'),'Players did not connect');
 assert.deepEqual(posts.at(-1),{action:'connect',mapping:[{player:1,deck:3},{player:2,deck:4}]});
 assert.deepEqual(preferences,{autoConnect:true,devices:[{player:1,deck:3,name:'CDJ-3000'},{player:2,deck:4,name:'CDJ-3000X'}]});
 await navigate('/prolink/settings?lang=en');await until(()=>evaluate(`document.querySelector('[data-deck-player="1"]')?.value==='3'`),'Saved mapping lost on reload');
 assert.equal(await evaluate('document.getElementById("connect").disabled'),true);
 assert.ok(await evaluate('document.querySelector("#devices").textContent.includes("On-Air")'));
 await screenshot('prolink-connected-en');
 await call('Emulation.setDeviceMetricsOverride',{width:390,height:844,deviceScaleFactor:1,mobile:false});
 assert.equal(await evaluate('document.documentElement.scrollWidth<=window.innerWidth'),true,'Mobile navigation overflows');
 await evaluate('document.querySelector(".nav-components summary").click()');
 assert.equal(await evaluate('document.querySelector(".nav-components").open && document.documentElement.scrollWidth<=innerWidth'),true,'Open mobile dropdown overflows');
 await evaluate('document.querySelector("[data-language]").value="de";document.querySelector("[data-language]").dispatchEvent(new Event("change"))');
 assert.equal(await evaluate('document.getElementById("discover").textContent'),'Geräte suchen');
 assert.equal(await evaluate('document.querySelector("[data-i18n=navComponents]").textContent'),'Szenen-Komponenten');
 assert.ok(await evaluate('document.getElementById("devices").textContent.includes("Vierdeck-Modus") && document.getElementById("devices").textContent.includes("Playernummer von 1–4")'),'Device guidance did not switch to German');
 await evaluate('document.querySelector("h1").click()');
 assert.equal(await evaluate('document.querySelector(".nav-components").open'),false,'Outside click did not close dropdown');
 assert.ok(await evaluate('document.getElementById("app-mode").textContent.startsWith("Modus")'));
 failed=true;await until(()=>evaluate('!document.getElementById("connection-error").hidden'),'Connection failure not shown');
 assert.equal(await evaluate('document.getElementById("connect").disabled'),true);failed=false;
 await evaluate('document.getElementById("disconnect").click()');await until(()=>evaluate('document.getElementById("disconnect").disabled && !document.getElementById("discover").disabled && document.querySelectorAll(".device-card").length===2'),'Disconnect not applied');
 assert.equal(await evaluate('document.getElementById("connect").disabled'),true,'Missing saved devices became connectable');
 assert.equal(posts.at(-1).action,'disconnect');
 await evaluate('document.getElementById("discover").click()');await until(()=>evaluate('document.querySelectorAll(".device-card").length===7'),'Rediscovery failed');
 // Explicitly choose only the two XDJ-AZ endpoints, which share one address.
 await evaluate('for(let i=0;i<4;i++){const input=document.querySelector("#devices input:checked");if(!input)break;input.click();}');
 for(const player of [3,4])await evaluate(`document.querySelector('[data-player="${player}"]').click()`);
 await evaluate('document.getElementById("connect").click()');await until(()=>posts.at(-1)?.action==='connect','AZ connection not submitted');
 assert.deepEqual(posts.at(-1),{action:'connect',mapping:[{player:3,deck:1},{player:4,deck:2}]});
 await until(()=>evaluate(`document.getElementById('connect').disabled && !document.getElementById('disconnect').disabled && document.querySelector('[data-player="3"]')?.disabled`),'AZ connection not rendered');
 assert.ok(await evaluate(`document.querySelector('[data-deck-player="3"]').value==='1' && document.querySelector('[data-deck-player="4"]').value==='2'`),'AZ deck mapping wrong');
 await evaluate('document.getElementById("disconnect").click()');await until(()=>evaluate('document.getElementById("disconnect").disabled && document.querySelectorAll(".device-card").length===2'),'AZ disconnect not applied');
 await navigate('/rekordbox/settings?lang=en');await until(()=>evaluate('!document.getElementById("mode-unavailable").hidden'),'Direct inactive page not gated');
 assert.equal(await evaluate('document.getElementById("rekordbox-content").hidden'),true);
 console.log('ProLink UI passed: both mode gates, grouped navigation, no automatic network start, discovery/selection/connect/disconnect, safe device names, status, mobile layout and EN/DE.');
}).catch(error=>{console.error(error);process.exitCode=1;});
