// Synthetic administration fixture for the server-wide master hold time.
// Run: node tests/browser_admin_master_test.cjs [PATH_TO_CHROME_OR_EDGE]
const assert=require('node:assert/strict'),{withBrowser}=require('./browser_fixture.cjs');
let holdMs=4000,canControl=true,failure=false,posts=[];
const describe=()=>({holdMs,defaultHoldMs:4000,maxHoldMs:30000,available:true,canControl});
withBrowser((req,res,url)=>{
 if(!url.pathname.startsWith('/api/'))return false;res.setHeader('Content-Type','application/json');
 if(url.pathname==='/api/app')res.end(JSON.stringify({mode:'rekordbox',canControl,capabilities:{admin:true,scenes:true,audioWaveform:true},user:{username:'admin'}}));
 else if(url.pathname==='/api/admin/users')res.end('{"users":[]}');
 else if(url.pathname==='/api/admin/ratings')res.end('{"tracks":[]}');
 else if(url.pathname==='/api/audio/devices')res.end('{"devices":[],"error":null}');
 else if(url.pathname==='/api/admin/audio')res.end(JSON.stringify({settings:{deviceId:'',deviceName:'',autoStart:false},state:{status:'stopped'},canControl,controlError:null}));
 else if(url.pathname==='/api/admin/master'){
  if(failure){res.statusCode=503;res.end('{"error":"masterSettingsUnavailable"}');return true;}
  if(req.method==='GET')res.end(JSON.stringify(describe()));
  else{let body='';req.on('data',chunk=>body+=chunk);req.on('end',()=>{
   const command=JSON.parse(body);posts.push(command);
   if(!Number.isInteger(command.holdMs)||command.holdMs<0||command.holdMs>30000){res.statusCode=400;return res.end('{"error":"masterSettingsInvalid"}');}
   holdMs=command.holdMs;res.end(JSON.stringify(describe()));});}
 }else{res.statusCode=404;res.end('{}');}return true;
},async({navigate,evaluate,until,delay,call,screenshot})=>{
 const $=id=>'document.getElementById('+JSON.stringify('admin-master-'+id)+')';
 const drag=async value=>evaluate(`(()=>{const input=${$('hold')};input.value=${JSON.stringify(String(value))};input.dispatchEvent(new Event('input'));})()`);
 const ready=()=>until(()=>evaluate($('hold')+'?.disabled===false'),'Master hold control stayed disabled');
 // Each visit needs its own URL: navigating to an identical address only moves the fragment.
 let visit=0;const open=lang=>navigate('/admin?lang='+lang+'&visit='+(++visit)+'#master');
 await open('en');await ready();
 await until(()=>evaluate('!document.querySelector("[data-pane=master]").hidden'),'Master deep link did not select the tab');
 assert.equal(await evaluate($('hold')+'.value'),'4','Server hold time was not loaded');
 assert.equal(await evaluate($('value')+'.textContent'),'4 s');
 assert.equal(await evaluate($('save')+'.disabled'),true,'Unchanged hold time was savable');
 assert.deepEqual(posts,[],'Opening the tab wrote to the server');

 // A draft stays local until it is confirmed with Save.
 await drag(8);await delay(650);
 assert.equal(await evaluate($('hold')+'.value'),'8','Draft was not retained');
 assert.equal(await evaluate($('pending')+'.hidden'),false,'Unsaved change was not announced');
 assert.equal(holdMs,4000,'Dragging saved without confirmation');
 await evaluate($('save')+'.click()');await until(()=>holdMs===8000,'Hold time was not saved');
 assert.deepEqual(posts,[{holdMs:8000}],'Unexpected save payload');
 await until(()=>evaluate($('message')+'.textContent.length>0'),'Save gave no feedback');
 assert.equal(await evaluate($('pending')+'.hidden'),true,'Saved change still marked unsaved');
 await open('en');await ready();
 assert.equal(await evaluate($('hold')+'.value'),'8','Saved hold time was lost on reload');
 await screenshot('admin-master-en');

 // Zero is the documented opt-out and must read as such, not as "0 s".
 await drag(0);assert.equal(await evaluate($('value')+'.textContent'),'Off (immediate)');
 await evaluate($('save')+'.click()');await until(()=>holdMs===0,'Opt-out was not saved');
 await ready(); // Wait for the response to re-enable controls before clicking Reset.
 await evaluate($('reset')+'.click()');
 assert.equal(await evaluate($('hold')+'.value'),'4','Default button did not restore 4 seconds');
 assert.equal(await evaluate($('reset')+'.disabled'),true,'Default button stayed active at the default');
 await evaluate($('save')+'.click()');await until(()=>holdMs===4000,'Default was not saved');

 await evaluate('document.querySelector("[data-language]").value="de";document.querySelector("[data-language]").dispatchEvent(new Event("change"))');
 assert.equal(await evaluate('document.querySelector("[data-tab=master]").textContent'),'Master-Erkennung');
 assert.equal(await evaluate($('value')+'.textContent'),'4 s');
 await call('Emulation.setDeviceMetricsOverride',{width:390,height:844,deviceScaleFactor:1,mobile:false});
 assert.equal(await evaluate('document.documentElement.scrollWidth<=innerWidth'),true,'Admin master overflows mobile');

 // Remote policy and outages disable the control instead of failing silently.
 canControl=false;await open('de');
 await until(()=>evaluate($('hold')+'.disabled&&'+$('readonly')+'.hidden===false'),'Remote policy ignored');
 canControl=true;failure=true;await open('de');
 await until(()=>evaluate($('message')+'.textContent.length>0&&'+$('hold')+'.disabled'),'Outage not shown');
 failure=false;await open('de');await ready();
 assert.equal(holdMs,4000,'Recovery changed the stored hold time');
 console.log('Admin master detection UI passed: server value, draft preservation, explicit save, opt-out label, default restore, EN/DE, mobile, remote policy and outage recovery.');
}).catch(error=>{console.error(error);process.exitCode=1;});
