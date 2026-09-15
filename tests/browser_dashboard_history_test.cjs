const assert = require('node:assert/strict');
const { withBrowser } = require('./browser_fixture.cjs');
const tracks = Array.from({length:137}, (_,i) => ({entryId:i+1,trackId:i+1001,id:i%4+1,loaded:true,metadataAvailable:true,
  title:i%2?'Night Drive':'Ocean Lights',artist:'Studio North',album:'After Hours',key:'8A',bpm:128.5,originalBpm:126,
  startedAt:Date.UTC(2026,8,15,20,0,i*30),endedAt:i===136?null:Date.UTC(2026,8,15,20,0,i*30+30),isMaster:i===136,
  coverUrl:'/api/history/covers/'+(i+1001)}));
let status='connected';
const decks = [1,2,3,4].map(id=>({...tracks[id],id,positionMs:id*30000,durationMs:240000,coverUrl:null}));
let fail=false;
withBrowser((req,res,url)=>{
  if(url.pathname.startsWith('/api/history/covers/')){res.setHeader('Content-Type','image/svg+xml');res.end('<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16"><rect width="16" height="16" fill="#a8eccf"/></svg>');return true;}
  if(url.pathname==='/api/state'){res.setHeader('Content-Type','application/json');res.end(JSON.stringify({status,demo:false,decks}));return true;}
  if(url.pathname==='/api/history'){
    res.setHeader('Content-Type','application/json');if(fail){res.statusCode=503;res.end('{}');return true;}
    const before=Number(url.searchParams.get('before'))||Infinity,limit=Number(url.searchParams.get('limit'))||100;
    const all=tracks.filter(e=>e.entryId<before).slice().reverse(),entries=all.slice(0,limit);
    res.end(JSON.stringify({status,total:tracks.length,entries,nextBefore:all.length>limit?entries.at(-1).entryId:null}));return true;
  }return false;
},async({evaluate,navigate,until,delay,screenshot})=>{
  await navigate('/?lang=en');await until(()=>evaluate('document.querySelectorAll(".timeline[data-available=true]").length===4'),'Four deck timelines missing');
  assert.equal(await evaluate(`document.querySelector('nav a[href="/waveform/settings"]').textContent`),'Waveform');
  assert.equal(await evaluate(`document.querySelector('nav a[href="/history"]').textContent`),'Full History');
  assert.equal(await evaluate('document.querySelector(".brand img").getAttribute("src")'),'/icon.svg');
  await until(()=>evaluate('document.querySelector(".brand img").naturalWidth>0'),'Application logo did not render');
  assert.deepEqual(await evaluate('[...document.querySelectorAll(".time-bar")].map(e=>Number(e.getAttribute("aria-valuenow")))'),[12.5,25,37.5,50]);
  decks[0].positionMs=-5000;decks[1].positionMs=300000;decks[2].durationMs=null;decks[3].loaded=false;
  await until(()=>evaluate('document.querySelector("[data-time=position]").textContent==="−0:05"'),'Negative preroll not shown');
  assert.deepEqual(await evaluate('[...document.querySelectorAll(".time-bar")].map(e=>e.getAttribute("aria-valuenow"))'),['0.00','100.00',null,null]);
  status='stale';await until(()=>evaluate('document.querySelectorAll(".timeline[data-available=true]").length===0'),'Stale dashboard retained timing');
  status='connected';
  await navigate('/history?lang=en');await until(()=>evaluate('document.querySelectorAll("tbody tr").length===100'),'History first page missing');
  assert.equal(await evaluate('document.querySelector("tbody tr").dataset.entryId'),'137');
  await until(()=>evaluate('[...document.querySelectorAll("tbody img")].every(image=>!image.hidden&&image.naturalWidth>0)'),'History cover images did not load');
  assert.equal(await evaluate('document.querySelectorAll("tbody tr[data-current=true]").length'),1);
  await evaluate('document.getElementById("older").click()');await until(()=>evaluate('document.querySelectorAll("tbody tr").length===37'),'History beyond first page missing');
  assert.equal(await evaluate('document.querySelector("tbody tr:last-child").dataset.entryId'),'1');
  assert.equal(await evaluate('document.getElementById("older").disabled'),true);
  // New arrivals do not shift the page of earlier entries.
  tracks.push({...tracks.at(-1),entryId:138,trackId:2000,title:'New arrival'});await delay(1200);
  assert.equal(await evaluate('document.querySelector("tbody tr").dataset.entryId'),'37');
  await evaluate('document.getElementById("refresh").click()');await until(()=>evaluate('document.querySelector("tbody tr").dataset.entryId==="138"'),'Return to latest failed');
  tracks.at(-1).title='<img src=x onerror=alert(1)>';tracks.at(-1).coverUrl='https://example.com/cover.png';
  await until(()=>evaluate('document.querySelector(".history-title").textContent.includes("<img")'),'Metadata update missing');
  assert.equal(await evaluate('document.querySelectorAll(".history-title img").length'),0);
  assert.equal(await evaluate('document.querySelector("tbody tr img").hasAttribute("src")'),false);
  await evaluate('document.querySelector("[data-language]").value="de";document.querySelector("[data-language]").dispatchEvent(new Event("change"))');
  assert.equal(await evaluate('document.querySelector("h1").textContent'),'Deine vollständige Track-History.');
  fail=true;await until(()=>evaluate('document.getElementById("status").textContent.includes("erreichbar")'),'History network failure missing');
  assert.equal(await evaluate('document.querySelectorAll("tbody tr[data-current=true]").length'),0,'Failure retained a live badge');
  assert.equal(await evaluate('document.querySelectorAll("tbody tr").length'),100,'Failure erased observed history');
  fail=false;tracks.splice(0);await until(()=>evaluate('document.getElementById("empty").hidden===false'),'Empty/restarted session not shown');
  assert.equal(await evaluate('document.getElementById("older").disabled'),true);
  await screenshot('history-empty-de');
  console.log('Dashboard/history passed: navigation, shared logo, 4 timelines, preroll/clamping/missing/stale, >100 entries, stable cursors, refresh, safe text/cover, EN/DE, recovery and empty session.');
}).catch(error=>{console.error(error);process.exitCode=1;});
