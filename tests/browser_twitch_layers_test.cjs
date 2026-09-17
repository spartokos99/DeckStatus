const assert=require('node:assert/strict');
const {withBrowser}=require('./browser_fixture.cjs');
let state={settings:{clientId:'',enabled:false,rules:[],revision:0},connection:'twitchDisabled',error:'',accounts:{},pending:{},subscriptions:{},log:[],canControl:true};
let scenes=[{id:'fixture',key:'read-key',revision:1,name:'Live set',width:1920,height:1080,background:'transparent',items:[{id:'master',name:'Now playing',type:'master',visible:true,x:30,y:30,width:700,height:400,opacity:1,options:{history:0}},{id:'wave',name:'Full width waveform',type:'waveform',visible:true,x:0,y:600,width:1920,height:400,opacity:1,options:{width:1600}},{id:'text',name:'Reward message',type:'text',visible:false,x:60,y:450,width:600,height:150,opacity:1,options:{text:'Hello'}}]}];
let tests=0,authorizations=0;
const automationState=()=>{const {clientId,...settings}=state.settings;const {accounts,pending,...result}=state;return {...result,settings,accountLinked:false};};
withBrowser((req,res,url)=>{
 if(!url.pathname.startsWith('/api/'))return false;res.setHeader('Content-Type','application/json');
 const send=value=>res.end(JSON.stringify(value));
 if(req.method==='POST'){let body='';req.on('data',v=>body+=v);req.on('end',()=>{const c=JSON.parse(body);if(url.pathname==='/api/admin/twitch'){
  if(c.action==='saveConnection'){assert.deepEqual(Object.keys(c.settings).sort(),['clientId','revision']);assert.equal(c.settings.revision,state.settings.revision);state.settings={...state.settings,...c.settings,revision:c.settings.revision+1};}
  if(c.action==='authorize'){authorizations++;state.pending[c.role]={code:'TESTCODE',url:'https://www.twitch.tv/activate?public=true&device-code=TESTCODE'};}
  if(c.action==='test'){tests++;return send({results:[{rule:state.settings.rules[0].name,action:'show',value:true}]});}
  return send(state);
 }if(url.pathname==='/api/admin/automations'){
  if(c.action==='save'){assert.deepEqual(Object.keys(c.settings).sort(),['enabled','revision','rules']);assert.equal(c.settings.revision,state.settings.revision);state.settings={...state.settings,...c.settings,revision:c.settings.revision+1};}
  if(c.action==='test'){tests++;return send({results:state.settings.rules[0].actions.map((a,i)=>({...a,rule:state.settings.rules[0].name,index:i+1}))});}
  return send(automationState());
 }if(url.pathname==='/api/scenes'){const s={...c.scene,revision:c.revision+1};scenes=[s];return send(s);}res.statusCode=404;send({});});return true;}
 if(url.pathname==='/api/auth/me')send({user:{id:'admin',username:'admin',role:'admin',mustChangePassword:false}});
 else if(url.pathname==='/api/app')send({mode:'rekordbox',canControl:true,obsBaseUrl:'http://127.0.0.1:18740',capabilities:{admin:true,sceneEditor:true,audioWaveform:true,deckOverlays:true,masterOverlay:true}});
 else if(url.pathname==='/api/admin/twitch')send(state);
 else if(url.pathname==='/api/admin/automations')send(automationState());
 else if(url.pathname==='/api/scenes')send({scenes});
 else if(url.pathname==='/api/presets')send({presets:[]});
 else if(url.pathname==='/api/media')send({media:[]});
 else if(url.pathname==='/api/admin/users')send({users:[]});
 else if(url.pathname==='/api/admin/ratings')send({tracks:[]});
 else if(url.pathname==='/api/admin/audio')send({settings:{deviceId:'',autoStart:false},state:{status:'stopped'},canControl:true});
 else if(url.pathname==='/api/audio/devices')send({devices:[]});
 else if(url.pathname==='/api/audio/state')send({status:'stopped',fresh:false});
 else if(url.pathname==='/api/master')send({current:null,history:[]});
 else{res.statusCode=404;send({});}return true;
},async({navigate,evaluate,until,delay,screenshot,call})=>{
 const click=async id=>evaluate(`document.getElementById(${JSON.stringify(id)}).click()`);
 const change=async(id,value,event='input')=>evaluate(`(()=>{const n=document.getElementById(${JSON.stringify(id)});n.value=${JSON.stringify(value)};n.dispatchEvent(new Event(${JSON.stringify(event)},{bubbles:true}));})()`);
 await navigate('/scenes?lang=en');await until(()=>evaluate('document.querySelectorAll(".scene-layer").length===3'),'Layers missing');
 await until(()=>evaluate('!document.getElementById("scene-save").disabled'),'Editor is still loading');
 assert.equal(await evaluate('document.querySelector(".scene-layer").dataset.id'),'text');
 await evaluate('document.querySelector(".scene-layer[data-id=text] .scene-layer-select").click()');
 assert.equal(await evaluate('document.querySelector(".scene-layer[data-id=text]").dataset.selected'),'true');
 await evaluate('document.querySelector(".scene-layer[data-id=text] .scene-layer-actions button:nth-child(3)").click()');
 assert.equal(await evaluate('document.querySelector(".scene-layer").dataset.id'),'wave','Move backwards failed');
 await evaluate('document.querySelector(".scene-layer[data-id=text] .scene-layer-actions button:nth-child(2)").click()');
 assert.equal(await evaluate('document.querySelector(".scene-layer").dataset.id'),'text','Move forwards failed');
 assert.equal(await evaluate('document.querySelector("[data-visibility=text]").getAttribute("aria-pressed")'),'false');
 await evaluate('document.querySelector("[data-visibility=text]").click()');
 assert.equal(await evaluate('document.querySelector(".scene-item[data-id=text]").hidden'),false);
 await evaluate('document.querySelector("[data-visibility=master]").click()');
 assert.equal(await evaluate('document.querySelector(".scene-item[data-id=master]").hidden'),true);
 assert.equal(await evaluate('document.querySelector(".scene-item[data-id=master] iframe").hasAttribute("src")'),false,'Hidden iframe still requests unauthorized resources');
 assert.equal(await evaluate('document.querySelectorAll(".scene-layer").length'),3,'Hiding deleted a layer');
 await click('scene-save');await until(()=>scenes[0].revision===2,'Scene did not save');await until(()=>evaluate('!document.getElementById("scene-save").disabled'),'Scene save still busy');
 assert.equal(scenes[0].items.find(i=>i.id==='master').visible,false);
 assert.equal(await evaluate('document.querySelector(".scene-layer .scene-layer-actions button:nth-child(2)").disabled'),true,'Save re-enabled out-of-bounds reorder');
 await screenshot('scene-layers-preview');

 await navigate('/admin?lang=en#twitch');await until(()=>evaluate('!document.querySelector("[data-pane=twitch]").hidden && !document.getElementById("twitch-save").disabled'),'Twitch panel missing');
 assert.equal(await evaluate('document.getElementById("twitch-new-rule")'),null,'Rule editor still in Admin');
 await change('twitch-client','fixtureclient123');await click('twitch-save');await until(()=>state.settings.clientId==='fixtureclient123','Connection did not save');await until(()=>evaluate('!document.getElementById("twitch-save").disabled'),'Save busy');
 await click('twitch-streamer-authorize');await until(()=>authorizations===1,'Authorization request missing');await until(()=>evaluate('document.getElementById("twitch-streamer-pending").textContent.includes("TESTCODE")'),'Device code missing');
 await screenshot('twitch-connection-preview');
 await navigate('/automations?lang=en');await evaluate('import("/automations.js").then(()=>true)');await until(()=>evaluate('!document.getElementById("twitch-new-rule").disabled'),'Automation page missing');
 assert.equal(await evaluate('document.getElementById("twitch-client")'),null,'Client ID still on automation page');
 assert.equal(await evaluate('document.querySelector("nav [data-i18n=navAutomations]")?.textContent'), 'Automations');
 await click('twitch-new-rule');await change('twitch-name','Reward lights');await change('twitch-match','reward-id');await change('twitch-scene','fixture','change');await change('twitch-item','wave','change');
 assert.equal(await evaluate('document.getElementById("twitch-duration").value'),'60');
 await click('automation-add-action');await change('twitch-action','audioOn');await change('twitch-duration','30');await change('twitch-scene','fixture','change');await change('twitch-item','master','change');
 assert.equal(await evaluate('document.getElementById("twitch-value-row").hidden'),true);
 await click('automation-add-action');await change('twitch-action','chat');await change('twitch-value','Hello {user}');
 assert.equal(await evaluate('document.getElementById("twitch-duration-row").hidden'),true);
 await delay(2200);assert.equal(await evaluate('document.getElementById("twitch-name").value'),'Reward lights','Polling overwrote draft');
 const actionButton=async(index,button)=>evaluate('document.querySelectorAll("#automation-actions .twitch-rule")['+index+'].querySelectorAll("button")['+button+'].click()');
 await actionButton(2,1); // Chat moves before audioOn.
 await actionButton(1,3); // Duplicate chat, then remove the copy.
 assert.equal(await evaluate('document.querySelectorAll("#automation-actions .twitch-rule").length'),4);
 await actionButton(2,4);await click('twitch-save');await until(()=>state.settings.rules.length===1,'Rule did not save');await until(()=>evaluate('!document.getElementById("twitch-save").disabled'),'Save busy');
 assert.deepEqual(state.settings.rules[0].actions.map(a=>a.action),['show','chat','audioOn']);
 assert.equal(state.settings.rules[0].actions[0].duration,60);assert.equal(state.settings.rules[0].actions[2].duration,30);assert.equal(state.settings.clientId,'fixtureclient123');
 assert.equal(await evaluate('document.querySelector("#automation-actions .twitch-rule button:nth-child(2)").disabled'),true,'Save re-enabled invalid reorder');
 await actionButton(2,0);await change('twitch-action','audioOff');await click('twitch-save');await until(()=>state.settings.rules[0].actions[2].action==='audioOff','Audio off did not save');await until(()=>evaluate('!document.getElementById("twitch-save").disabled'),'Save busy');
 await change('twitch-test-type','reward','change');await change('twitch-test-reward','reward-id');await click('twitch-test');await until(()=>tests===1,'Dry run missing');await until(()=>evaluate('document.getElementById("twitch-test-results").textContent.includes("3. Disable audio reactive")'),'Multiple test results missing');
 await screenshot('automations-preview');
 await evaluate('document.getElementById("automation-actions").scrollIntoView({block:"center"})');await screenshot('automation-actions-preview');
 await navigate('/automations?lang=de');await evaluate('import("/automations.js").then(()=>true)');await until(()=>evaluate('document.getElementById("twitch-new-rule").textContent==="Regel hinzufügen"'),'German translation missing');
 await until(()=>evaluate('document.querySelectorAll("#automation-actions .twitch-rule").length===3'),'Actions lost on reload');await actionButton(2,0);
 assert.equal(await evaluate('document.getElementById("twitch-action").selectedOptions[0].textContent'),'Audio reactive ausschalten');
 assert.equal(await evaluate('document.getElementById("twitch-duration").value'),'30');
 await call('Emulation.setDeviceMetricsOverride',{width:430,height:900,deviceScaleFactor:1,mobile:true});
 assert.ok(await evaluate('document.documentElement.scrollWidth<=window.innerWidth+1'),'Automation layout overflows mobile');
 state.canControl=false;await until(()=>evaluate('document.getElementById("twitch-save").disabled'),'Remote read-only bypassed');assert.equal(await evaluate('document.getElementById("automation-add-action").disabled'),true);
 await navigate('/admin?lang=de#twitch');await until(()=>evaluate('document.getElementById("twitch-client").value==="fixtureclient123"'),'Admin reload missing');
 await click('rating-refresh');await delay(100);assert.equal(await evaluate('document.getElementById("twitch-streamer-authorize").disabled'),true);
 assert.ok(await evaluate('document.documentElement.scrollWidth<=window.innerWidth+1'),'Admin layout overflows mobile');
 console.log('Layers and separate Twitch/Automations pages: multiple actions, order, duplicate/remove, timers, persistence, EN/DE, mobile, drafts, dry run and remote permissions passed.');
}).catch(error=>{console.error(error);process.exitCode=1;});
