// node tools/readme-screenshots.cjs [path/to/msedge.exe]
// Set DECKSTATUS_SCREENSHOT_ONLY to one image name (for example scene-editor).
// Real UI, English only, synthetic data. Never connects to Rekordbox/audio devices.
const assert = require('node:assert/strict');
const fs = require('node:fs'), path = require('node:path');
const { withBrowser, appVersion } = require('../tests/browser_fixture.cjs');
const only=process.env.DECKSTATUS_SCREENSHOT_ONLY||'';
const titles = ['Night Drive','Ocean Lights','Afterglow','Echo Park','Slow Motion','Open Skies','Midnight Radio','First Light'];
const artists = ['Studio North','Lunar Avenue','The Signals','Harbour Club'];
const track = (index, id = index % 4 + 1) => ({entryId:index+1, trackId:index+1001, id, loaded:true, metadataAvailable:true,
  title:titles[index % titles.length], artist:artists[index % artists.length], album:'After Hours', genre:'House', label:'North Records',
  key:['8A','9A','10A','11A'][index%4], bpm:128.5, originalBpm:126, positionMs:65000+index%4*12000, durationMs:240000,
  startedAt:Date.UTC(2026,8,15,20,index*4), endedAt:Date.UTC(2026,8,15,20,index*4+4), isMaster:false});
const entries=Array.from({length:8},(_,i)=>({...track(i),coverUrl:'/api/history/covers/'+(i+1001)}));
entries.at(-1).isMaster=true;entries.at(-1).endedAt=null;
const decks=[1,2,3,4].map((id,i)=>({...track(i,id),isMaster:id===2,coverUrl:'/api/decks/'+id+'/cover?trackId='+(i+1001)}));
const current={...track(0,2),isMaster:true,coverUrl:'/api/master/covers/1001'};
const history=[1,2].map(i=>({...track(i),coverUrl:'/api/master/covers/'+(i+1001)}));
const signal=Array.from({length:1024},(_,i)=>Math.sin(2*Math.PI*13*i/1024)*.36+Math.sin(2*Math.PI*31*i/1024)*.16);
let sequence=0, screenshotMode='rekordbox';
const demoUser={id:'synthetic-admin',username:'admin',role:'admin',mustChangePassword:false};
const demoScene={id:'synthetic-scene',key:'synthetic-example-key',revision:1,name:'Late-night studio',width:1920,height:1080,background:'transparent',items:[
 {id:'master',type:'master',x:280,y:72,width:1360,height:720,opacity:1,visible:true,options:{width:1344,history:2,historyScale:.8,align:'center',timeline:true,fontSize:36,coverSize:116,padding:24,gap:16,radius:18,accent:'#6ae9b3',lang:'en'}},
 {id:'wave',type:'waveform',x:0,y:830,width:1920,height:250,opacity:1,visible:true,options:{mode:'line',width:1920,height:250,color:'#a8eccf',color2:'#48bce8',lineWidth:4,glow:12,lang:'en'}}]};
const demoPresets=[...demoScene.items,{id:'deck',type:'deck',options:{deck:2,width:660,timeline:true,fontSize:24,lang:'en'}}].map(item=>({id:'synthetic-preset-'+item.id,revision:1,name:{master:'Studio master',deck:'Clean deck',waveform:'Mint waveform'}[item.type],type:item.type,options:item.options}));
for(const [i,entry]of entries.entries()){entry.ratingId='synthetic-rating-'+i;entry.rating={count:16+i*3,average:(68+i*15)/(16+i*3),mine:4};}
const colours=['#56ae99','#597fb4','#b071a0','#bd9b64'];
function cover(index){const colour=colours[index%4];return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 300 300"><rect width="300" height="300" fill="#142c32"/><circle cx="180" cy="145" r="130" fill="${colour}"/><circle cx="180" cy="145" r="88" fill="#142c32"/><circle cx="180" cy="145" r="35" fill="${colour}"/><path d="M0 250L300 60V110L0 300Z" fill="#cceadd" opacity=".45"/></svg>`;}
withBrowser((req,res,url)=>{
  if(url.pathname==='/api/app'){res.setHeader('Content-Type','application/json');res.end(JSON.stringify({version:appVersion,mode:screenshotMode,user:demoUser,canControl:true,capabilities:{dashboard:true,history:true,deckOverlays:true,masterOverlay:true,audioWaveform:true,rekordboxSetup:screenshotMode==='rekordbox',prolinkSetup:screenshotMode==='prolink',networkSettings:true,scenes:true,admin:true}}));return true;}
  if(url.pathname==='/api/prolink/devices'){res.setHeader('Content-Type','application/json');res.end(JSON.stringify({status:'connected',message:'prolinkConnected',runtimeAvailable:true,players:[1,2],networkInterface:'Ethernet · synthetic fixture',localAddress:'192.0.2.100',virtualPlayer:7,devices:[{number:1,name:'CDJ-3000',address:'192.0.2.11',selectable:true,supported:true,selected:true,playing:true,onAir:true,synced:true,master:true,bpm:128.5},{number:2,name:'CDJ-3000',address:'192.0.2.12',selectable:true,supported:true,selected:true,playing:true,onAir:false,synced:true,master:false,bpm:128.5},{number:33,name:'DJM-A9',address:'192.0.2.33',selectable:false,supported:true}]}));return true;}
  if(!url.pathname.startsWith('/api/'))return false;
  if(url.pathname==='/api/network'){res.setHeader('Content-Type','application/json');const settings={bind:'0.0.0.0',port:18740,allowRemoteControl:false};res.end(JSON.stringify({active:settings,saved:settings,canConfigure:true,restartRequired:false,commandLineOverrides:false,interfaces:[{name:'Ethernet · synthetic fixture',address:'192.0.2.100'}],urls:['http://127.0.0.1:18740','http://192.0.2.100:18740']}));return true;}
  if(url.pathname.includes('/cover')){const index=Number(url.searchParams.get('trackId')||url.pathname.split('/').at(-1))||1001;res.setHeader('Content-Type','image/svg+xml');res.end(cover(index-1001));return true;}
  res.setHeader('Content-Type','application/json');
  if(url.pathname==='/api/scenes'){res.end(JSON.stringify({scenes:[demoScene]}));return true;}
  if(url.pathname==='/api/media'){res.end(JSON.stringify({media:[]}));return true;}
  if(url.pathname==='/api/presets'){res.end(JSON.stringify({presets:demoPresets}));return true;}
  if(url.pathname==='/api/scene'){res.end(JSON.stringify(demoScene));return true;}
  if(url.pathname==='/api/broadcast'){res.end(JSON.stringify({deck:'synthetic-deck-key',master:'synthetic-master-key',waveform:'synthetic-wave-key'}));return true;}
  if(url.pathname==='/api/auth/me'){res.end(JSON.stringify({user:null}));return true;}
  if(url.pathname==='/api/admin/users'){res.end(JSON.stringify({users:[demoUser]}));return true;}
  if(url.pathname==='/api/admin/ratings'){res.end(JSON.stringify({tracks:entries.map((entry,i)=>({...entry,...entry.rating,distribution:[0,1,2,5,8+i*3]}))}));return true;}
  if(url.pathname==='/api/state')res.end(JSON.stringify({status:'connected',demo:false,message:'Connected',version:'7.2.18.0',updatedAt:Date.UTC(2026,8,15,20,32),sampleAgeMs:24,artworkStatus:'Ready',masterDeckId:2,decks}));
  else if(url.pathname==='/api/master')res.end(JSON.stringify({status:'connected',demo:false,current,history,historyLimit:50}));
  else if(url.pathname==='/api/history')res.end(JSON.stringify({status:'connected',demo:false,entries:[...entries].reverse(),total:entries.length,nextBefore:null}));
  else if(url.pathname==='/api/audio/devices')res.end(JSON.stringify({devices:[{id:'synthetic',name:'Studio interface · synthetic fixture',kind:'input'}],error:null}));
  else if(url.pathname==='/api/audio/state')res.end(JSON.stringify({status:'capturing',error:null,deviceId:'synthetic',deviceName:'Studio interface · synthetic fixture',fresh:true,sampleAgeMs:5,sampleRate:48000,sequence:++sequence,left:signal,right:signal}));
  else{res.statusCode=404;res.end('{}');}return true;
},async({evaluate,call,navigate,until,delay,root})=>{
  const destination=path.join(root,'docs','images'), english=JSON.parse(fs.readFileSync(path.join(root,'web/locales/en.json'),'utf8'));
  const images=[];
  const size=(width,height)=>call('Emulation.setDeviceMetricsOverride',{width,height,deviceScaleFactor:1,mobile:false});
  async function capture(name,width,height){
    if(only&&only!==name)return;
    // Validate each visible translated label, including preview iframes, before saving.
    const mismatches=await evaluate(`(()=>{const dictionary=${JSON.stringify(english)};const problems=[];function inspect(doc){if(doc.documentElement.lang!=='en')problems.push('language:'+doc.documentElement.lang);for(const el of doc.querySelectorAll('[data-i18n]')){if(el.getClientRects().length&&!el.matches('[data-field], #message, #status-label')&&Object.hasOwn(dictionary,el.dataset.i18n)&&el.textContent!==dictionary[el.dataset.i18n])problems.push(el.dataset.i18n+': '+el.textContent);}for(const frame of doc.querySelectorAll('iframe'))if(frame.contentDocument)inspect(frame.contentDocument);}inspect(document);return problems;})()`);
    assert.deepEqual(mismatches,[],'README screenshot must use English labels: '+name);
    const unexpectedTracks=await evaluate(`(()=>{const titles=${JSON.stringify(titles)},artists=${JSON.stringify(artists)},problems=[];function inspect(doc){for(const el of doc.querySelectorAll('[data-field="title"],.history-title'))if(el.getClientRects().length&&!titles.includes(el.textContent))problems.push('title:'+el.textContent);for(const el of doc.querySelectorAll('[data-field="artist"],.history-artist'))if(el.getClientRects().length&&!artists.includes(el.textContent))problems.push('artist:'+el.textContent);for(const frame of doc.querySelectorAll('iframe'))if(frame.contentDocument)inspect(frame.contentDocument);}inspect(document);return problems;})()`);
    assert.deepEqual(unexpectedTracks,[],'README screenshot must use the English fixture tracks: '+name);
    const shot=await call('Page.captureScreenshot',{format:'png',clip:{x:0,y:0,width,height,scale:1}});
    fs.writeFileSync(path.join(destination,name+'-en.png'),Buffer.from(shot.data,'base64'));images.push(name+'-en.png');
  }
  await size(800,900);
  await navigate('/master-overlay?timeline=1&history=2&historyScale=0.82&width=752&align=center&lang=en&duration=0');
  await until(()=>evaluate('document.querySelectorAll(".track").length===3 && document.querySelector("[data-time=position]")?.textContent==="1:05"'),'Master preview missing');
  await evaluate('document.body.style.background="#0b1015";document.body.style.padding="24px"');await delay(250);
  await capture('master-overlay',800,await evaluate('Math.ceil(document.getElementById("tracks").getBoundingClientRect().bottom+24)'));

  await size(1440,1340);await navigate('/?lang=en');
  await until(()=>evaluate('document.querySelectorAll(".timeline[data-available=true]").length===4 && [...document.querySelectorAll(".art img")].every(img=>img.complete&&img.naturalWidth>0)'),'Dashboard preview missing');
  await capture('deckstatus-dashboard',1440,1340);

  await size(1440,1240);await navigate('/overlay/settings?deck=1&timeline=1&lang=en');
  await until(()=>evaluate('document.getElementById("preview").contentDocument?.querySelector("[data-time=position]")?.textContent==="1:05"'),'Settings preview missing');
  await evaluate('document.getElementById("preset").value="light";document.getElementById("preset").dispatchEvent(new Event("change"))');await delay(500);
  await evaluate('document.getElementById("saved-preset").value="synthetic-preset-deck";document.getElementById("saved-preset").dispatchEvent(new Event("change"))');
  await capture('deckstatus-settings',1440,1200);

  await navigate('/waveform/settings?lang=en');
  await until(()=>evaluate('document.getElementById("preview").contentDocument?.querySelector("canvas")?.dataset.signal==="live"'),'Waveform preview missing');await delay(150);
  await evaluate('document.getElementById("saved-preset").value="synthetic-preset-wave";document.getElementById("saved-preset").dispatchEvent(new Event("change"))');
  await capture('waveform-settings',1440,1240);

  await navigate('/history?lang=en');await until(()=>evaluate('document.querySelectorAll("tbody tr").length===8 && [...document.querySelectorAll("tbody img")].every(image=>!image.hidden&&image.naturalWidth>0)'),'Full History preview missing');await delay(200);
  await capture('full-history',1440,1240);

  screenshotMode='prolink';await size(1440,1340);await navigate('/prolink/settings?lang=en');
  await until(()=>evaluate('document.querySelectorAll(".device-card").length===3 && document.getElementById("app-mode").textContent.includes("ProLink")'),'ProLink setup preview missing');
  await capture('prolink-settings',1440,1340);screenshotMode='rekordbox';

  await size(1440,1340);await navigate('/network/settings?lang=en');
  await until(()=>evaluate('document.getElementById("network-urls").children.length===2 && !document.getElementById("network-fields").disabled'),'Network settings preview missing');
  await capture('network-settings',1440,1340);

  await size(1440,1420);await navigate('/scenes?lang=en');
  await until(()=>evaluate('document.querySelectorAll(".scene-item iframe").length===2 && document.querySelector(".scene-item[data-id=master] iframe")?.contentDocument?.querySelectorAll(".track").length===3 && document.querySelector(".scene-item[data-id=wave] iframe")?.contentDocument?.querySelector("canvas")?.dataset.signal==="live"'),'Scene preview missing');await delay(900);
  assert.equal(await evaluate('(()=>{const stage=document.getElementById("scene-stage"),wave=stage.querySelector("[data-id=wave]");return wave.style.left==="0px"&&wave.style.width===stage.style.width;})()'),true,'Scene waveform must span the full canvas width');
  await evaluate('document.getElementById("scene-component").value="synthetic-preset-master";document.getElementById("scene-component").dispatchEvent(new Event("change"))');
  await capture('scene-editor',1440,1420);

  await size(1440,1120);await navigate('/admin?lang=en');await until(()=>evaluate('document.querySelectorAll("#rating-rows tr").length===8'),'Rating admin preview missing');
  await capture('admin-ratings',1440,1120);

  await size(1200,600);await navigate('/?lang=en');await until(()=>evaluate('document.querySelectorAll(".deck").length===4'),'Style gallery setup missing');
  await evaluate(`(async()=>{
    const {defaults,presets,deckOverlayPath}=await import('/master-options.js');
    document.body.innerHTML='<style>body{margin:0;padding:34px;background:#0b1015;color:#eef3f6;font-family:Segoe UI,sans-serif}h1{font-size:30px;letter-spacing:-1px;margin:8px 0 26px}small{color:#a8eccf;letter-spacing:2px}.gallery{display:grid;grid-template-columns:repeat(3,360px);gap:26px}h2{font-size:13px;font-weight:500;letter-spacing:1px;color:#b1c5bc;margin:0 0 12px}.surface{height:400px;border:1px solid #28383d;border-radius:16px;overflow:hidden;background:#152229}.surface.clear{background:repeating-conic-gradient(#152428 0% 25%,#1b2b2f 0% 50%) 50% / 20px 20px}iframe{display:block;border:0;width:358px;height:400px}</style><small>DECKSTATUS</small><h1>One overlay. Your style.</h1><div class="gallery"></div>';
    for(const[name,label]of [['midnight','01 / MIDNIGHT'],['light','02 / LIGHT'],['minimal','03 / MINIMAL + TRANSPARENT']]){
      const section=document.createElement('section'),heading=document.createElement('h2');heading.textContent=label;section.append(heading);
      const surface=document.createElement('div');surface.className='surface'+(name==='minimal'?' clear':'');
      const frame=document.createElement('iframe');frame.src=deckOverlayPath({...defaults,...presets[name],deck:1,width:342,coverSize:name==='light'?84:56,fontSize:22,padding:16,timeline:true,duration:0,layout:name==='light'?'stacked':'horizontal',font:name==='minimal'?'mono':'system',lang:'en'});
      surface.append(frame);section.append(surface);document.querySelector('.gallery').append(section);
    }
  })()`);
  await until(()=>evaluate('[...document.querySelectorAll("iframe")].every(frame=>frame.contentDocument?.querySelector("[data-field=title]")?.textContent==="Night Drive")'),'Style gallery missing');await delay(200);
  await capture('deckstatus-styles',1200,580);
  for(const file of ['README.md']){
    const markdown=fs.readFileSync(path.join(root,file),'utf8');
    for(const match of markdown.matchAll(/!\[[^\]]*\]\(docs\/images\/([^)]*\.png)\)/g)){
      assert.ok(fs.existsSync(path.join(destination,match[1])),'README image is missing: '+match[1]);
      if(!only||match[1]===only+'-en.png')assert.ok(images.includes(match[1]),'README screenshot was not regenerated in English: '+match[1]);
    }
  }
  if(only)assert.ok(images.includes(only+'-en.png'),'Unknown screenshot name: '+only);
  console.log('English README screenshots generated and checked: '+images.join(', '));
}).catch(error=>{console.error(error);process.exitCode=1;});
