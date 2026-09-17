// Verify composited pixels, not only computed CSS: Chromium can paint an iframe's
// default background opaque when the parent and child color schemes differ.
const assert=require('node:assert/strict'),zlib=require('node:zlib');
const {withBrowser}=require('./browser_fixture.cjs');
const track={id:1,entryId:1,trackId:1001,loaded:true,metadataAvailable:true,title:'Night Drive',artist:'Studio North',bpm:128,originalBpm:126};
const layer=(id,type,x,options)=>({id,type,x,y:20,width:type==='waveform'?400:416,height:240,opacity:1,visible:true,options});
const scene={id:'fixture',name:'Transparency fixture',width:1400,height:500,background:'transparent',items:[
 layer('master','master',20,{width:400,history:0,fields:['title'],background:'#285078',opacity:100,shadow:false,border:0,radius:0,duration:0}),
 layer('wave','waveform',480,{width:400,height:180,opacity:0,hideSilent:true}),
 layer('deck','deck',940,{deck:1,width:400,fields:['title'],background:'#285078',opacity:100,shadow:false,border:0,radius:0,duration:0})
]};
function firstPixel(data){
 const png=Buffer.from(data,'base64'),chunks=[];let width,height,depth,color;
 for(let offset=8;offset<png.length;){const length=png.readUInt32BE(offset),type=png.toString('ascii',offset+4,offset+8),body=png.subarray(offset+8,offset+8+length);if(type==='IHDR'){width=body.readUInt32BE(0);height=body.readUInt32BE(4);depth=body[8];color=body[9];}if(type==='IDAT')chunks.push(body);offset+=length+12;}
 assert.equal(width,1);assert.equal(height,1);assert.equal(depth,8);assert.ok([2,6].includes(color),'Unexpected PNG format');
 const scanline=zlib.inflateSync(Buffer.concat(chunks));assert.ok(scanline[0]<=4);return [...scanline.subarray(1,4),color===6?scanline[4]:255];
}
withBrowser((req,res,url)=>{
 if(!url.pathname.startsWith('/api/'))return false;res.setHeader('Content-Type','application/json');
 if(url.pathname==='/api/scene')res.end(JSON.stringify(scene));
 else if(url.pathname==='/api/master')res.end(JSON.stringify({status:'connected',current:track,history:[]}));
 else if(url.pathname==='/api/state')res.end(JSON.stringify({status:'connected',decks:[track]}));
 else if(url.pathname==='/api/audio/state')res.end(JSON.stringify({status:'stopped',fresh:false,sequence:0,left:Array(1024).fill(0),right:Array(1024).fill(0)}));
 else{res.statusCode=404;res.end('{}');}return true;
},async({navigate,evaluate,call,until,screenshot})=>{
 const pixel=async(x,y)=>firstPixel((await call('Page.captureScreenshot',{format:'png',clip:{x,y,width:1,height:1,scale:1}})).data);
 await call('Emulation.setDefaultBackgroundColorOverride',{color:{r:0,g:0,b:0,a:0}});
 await navigate('/scene?scene=fixture');
 await until(()=>evaluate('document.querySelectorAll("iframe").length===3 && [...document.querySelectorAll("iframe")].every(f=>f.contentDocument?.querySelector(".track,canvas"))'),'Scene components did not load');
 const points=[[80,220],[540,80],[1000,220]];
 // A root-only check would miss the opaque iframe rectangles.
 assert.equal((await pixel(1390,490))[3],0,'Scene root is opaque');
 for(const scheme of ['light','dark']){
  await call('Emulation.setEmulatedMedia',{features:[{name:'prefers-color-scheme',value:scheme}]});
  for(let i=0;i<points.length;i++)assert.equal((await pixel(...points[i]))[3],0,scene.items[i].type+' iframe has an opaque background in '+scheme+' mode');
 }
 // Intentional card backgrounds must remain visible.
 assert.deepEqual(await pixel(40,45),[40,80,120,255],'Master card background was removed');
 // A solid scene underneath the frames must show through, including empty waveform.
 scene.background='#b24876';
 await until(()=>evaluate('document.getElementById("scene-stage").style.background==="rgb(178, 72, 118)"'),'Scene background did not update');
 for(const point of points)assert.deepEqual(await pixel(...point),[178,72,118,255],'Component obscures scene background');
 await screenshot('scene-transparency-solid');
 // The same renderer is reused in the editor and must tolerate a different surrounding theme.
 await evaluate(`(async()=>{const {renderScene}=await import('/scene-shared.js');window.sceneDocument=await(await fetch('/api/scene?scene=fixture')).json();document.documentElement.style.colorScheme='light';const stage=document.getElementById('scene-stage');stage.replaceChildren();renderScene(stage,window.sceneDocument,undefined,true);})()`);
 await until(()=>evaluate('[...document.querySelectorAll("iframe")].every(f=>f.contentDocument?.querySelector(".track,canvas"))'),'Editor components did not load');
 for(const point of points)assert.deepEqual(await pixel(...point),[178,72,118,255],'Editor theme changed iframe compositing');
 console.log('Scene transparency passed: actual alpha pixels in master/deck/waveform frames, light/dark preferences, intentional card fill, solid scene background and editor theme.');
}).catch(error=>{console.error(error);process.exitCode=1;});
