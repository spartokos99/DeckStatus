// Synthetic analysis only: no Rekordbox, ProLink networking or audio device.
const assert=require('node:assert/strict'),{withBrowser}=require('./browser_fixture.cjs');
let current={id:1,trackId:41,loaded:true,metadataAvailable:true,title:'Midnight Signal',artist:'Studio North',album:'Nightfall',bpm:128,originalBpm:128,positionMs:5000,durationMs:20000,playing:false,isMaster:true,entryId:1};
let available=true,requests=[];
const samples=Array.from({length:3000},(_,i)=>(7<<13)|((i%8)<<10)|(4<<7)|((i%31)<<2));
withBrowser((req,res,url)=>{
 if(!url.pathname.startsWith('/api/'))return false;
 if(url.pathname==='/api/app')return false;
 res.setHeader('Content-Type','application/json');
 if(url.pathname==='/api/master')res.end(JSON.stringify({status:'connected',current,history:[{...current,entryId:99,trackId:40,title:'Previous Track'}],demo:false}));
 else if(url.pathname==='/api/state')res.end(JSON.stringify({status:'connected',masterDeckId:1,decks:[current]}));
 else if(url.pathname.endsWith('/waveform')){requests.push(url.href);res.end(JSON.stringify({trackId:Number(url.searchParams.get('trackId')),available,format:'rgb5',durationMs:20000,samples}));}
 else if(url.pathname==='/api/presets')res.end('{"presets":[]}');
 else if(url.pathname==='/api/broadcast')res.end(JSON.stringify({master:'a'.repeat(64),deck:'b'.repeat(64)}));
 else{res.statusCode=404;res.end('{}');}return true;
},async({navigate,evaluate,until,delay,screenshot})=>{
 const open=style=>navigate('/master-overlay?timeline=1&history=1&fields=title,artist&timelineStyle='+style+'&timelineHeight=90&timelineWindow=8');
 const ready=style=>until(()=>evaluate(`document.querySelector('.track:not(.leaving) .timeline')?.dataset.timelineStyle===${JSON.stringify(style)}`),'Timeline design missing: '+style);
 await open('bar');await ready('bar');assert.equal(requests.length,0,'Classic timeline downloaded waveform');
 await open('segments');await ready('segments');assert.equal(requests.length,0);
 await open('ring');await ready('ring');assert.equal(requests.length,0,'Ring downloaded analysis');
 await until(()=>evaluate(`document.querySelector('canvas.track-waveform')?.height===90`),'Ring did not size to the chosen height');
 for(const style of ['waveform','overview']){
  await open(style);await ready(style);
  assert.equal(await evaluate('document.querySelectorAll("canvas.track-waveform").length'),1,'History acquired waveform timeline');
  const before=await evaluate('document.querySelector("canvas.track-waveform").toDataURL()');
  const count=requests.length;await delay(650);assert.equal(requests.length,count,'Polling re-fetched cached analysis');
  assert.equal(await evaluate('document.querySelector("canvas.track-waveform").toDataURL()'),before,'Paused waveform moved');
  current={...current,positionMs:current.positionMs+3000};
  await until(async()=>await evaluate('document.querySelector("canvas.track-waveform").toDataURL()')!==before,'Seek did not move waveform');
  await screenshot('track-timeline-'+style);
 }
 available=false;current={...current,trackId:42,entryId:2};await open('waveform');await ready('bar');
 await until(()=>evaluate(`document.querySelector('.timeline-status')?.textContent.includes('Track waveform unavailable')`),'Missing waveform fallback not explained');
 assert.equal(await evaluate(`document.querySelector('.time-fill').style.width`),'55%');
 const count=requests.length;await delay(600);assert.equal(requests.length,count,'Missing analysis retried every poll');
 available=true;current={...current,trackId:43,entryId:3,positionMs:1000};
 await ready('waveform');assert.equal(await evaluate(`document.querySelector('.timeline').dataset.track`),'43');
 await navigate('/overlay?deck=1&timeline=1&timelineStyle=waveform&key='+'b'.repeat(64));await ready('waveform');
 assert(requests.some(url=>url.includes('/api/decks/1/waveform?trackId=43')&&url.includes('key=')),'Deck waveform omitted its scoped key');
 current={...current,positionMs:null};
 await until(()=>evaluate('!document.querySelector("canvas.track-waveform")'),'Unknown position retained waveform');
 assert.equal(await evaluate(`document.querySelector('.timeline').dataset.available`),'false');
 await navigate('/master-overlay/settings?timeline=1&timelineStyle=waveform&timelineWindow=20&timelineHeight=100');
 await until(()=>evaluate(`document.querySelector('[data-track-option="timelineStyle"]')?.value==='waveform'`),'Timeline settings did not load');
 assert.deepEqual(await evaluate(`['timelineWindow','timelineHeight'].map(n=>document.querySelector('[data-track-option="'+n+'"]').value)`),['20','100']);
 const roundtrip=await evaluate(`(async()=>{const m=await import('/master-options.js'),o=m.normalize({timeline:true,timelineStyle:'overview',timelineHeight:120,timelineWindow:16,timelineColor:false});return JSON.stringify(o)===JSON.stringify(m.parseOptions('?'+m.optionQuery(o)));})()`);
 assert(roundtrip,'Timeline design lost in OBS URL');
 console.log('Track timelines passed: five designs, real-data rendering, pause/seek, history isolation, caching, fallback, deck scoped URL, unknown position and settings roundtrip.');
}).catch(error=>{console.error(error);process.exitCode=1;});
