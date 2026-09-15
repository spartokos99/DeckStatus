// Small dependency-free Chromium harness for synthetic UI tests.
const fs=require('node:fs'), path=require('node:path'), http=require('node:http');
const {spawn}=require('node:child_process');
const root=path.resolve(__dirname,'..');
const delay=ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function until(fn,message,timeout=8000){const start=Date.now();while(Date.now()-start<timeout){if(await fn())return;await delay(40);}throw Error(message);}
async function withBrowser(handler,test){
  const output=path.join(root,'build','test-artifacts');fs.mkdirSync(output,{recursive:true});
  const profile=fs.mkdtempSync(path.join(output,'browser-waveform-'));
  const fixture=http.createServer((req,res)=>{
    res.setHeader('Cache-Control','no-store');
    res.setHeader('Content-Security-Policy',"default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; img-src 'self'; connect-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'self'; form-action 'none'");
    const url=new URL(req.url,'http://localhost');if(handler(req,res,url))return;
    const routes={'/waveform':'waveform.html','/waveform/settings':'waveform-settings.html','/':'index.html'};
    const name=routes[url.pathname]||url.pathname.slice(1);
    if(!/^(?:[a-z0-9-]+\.(?:js|html|css|svg)|locales\/(?:en|de)\.json)$/.test(name)){res.statusCode=404;return res.end();}
    const asset=path.join(root,'web',name);if(!fs.existsSync(asset)){res.statusCode=404;return res.end();}
    const types={'.js':'text/javascript','.css':'text/css','.svg':'image/svg+xml','.json':'application/json','.html':'text/html; charset=utf-8'};
    res.setHeader('Content-Type',types[path.extname(name)]);res.end(fs.readFileSync(asset));
  });
  let browser,socket;
  try{
    await new Promise(resolve=>fixture.listen(0,'127.0.0.1',resolve));const base='http://127.0.0.1:'+fixture.address().port;
    browser=spawn(process.argv[2]||'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',['--headless=new','--no-first-run','--disable-background-networking','--remote-debugging-port=0','--user-data-dir='+profile,'about:blank'],{windowsHide:true,stdio:'ignore'});
    let error;browser.on('error',e=>{error=e;});const portFile=path.join(profile,'DevToolsActivePort');
    await until(()=>{if(error)throw error;return fs.existsSync(portFile);},'Browser did not start',15000);
    const port=fs.readFileSync(portFile,'utf8').split('\n')[0],targets=await(await fetch('http://127.0.0.1:'+port+'/json/list')).json();
    socket=new WebSocket(targets.find(t=>t.type==='page').webSocketDebuggerUrl);await new Promise((resolve,reject)=>{socket.onopen=resolve;socket.onerror=reject;});
    let sequence=0;const pending=new Map(),exceptions=[];
    socket.onmessage=({data})=>{const m=JSON.parse(data),job=pending.get(m.id);if(job){pending.delete(m.id);clearTimeout(job.timer);m.error?job.reject(Error(JSON.stringify(m.error))):job.resolve(m.result);}if(m.method==='Runtime.exceptionThrown')exceptions.push(m.params.exceptionDetails);};
    const call=(method,params={})=>new Promise((resolve,reject)=>{const id=++sequence;const timer=setTimeout(()=>{pending.delete(id);reject(Error('CDP timeout: '+method));},10000);pending.set(id,{resolve,reject,timer});socket.send(JSON.stringify({id,method,params}));});
    const evaluate=async expression=>{const result=await call('Runtime.evaluate',{expression,returnByValue:true,awaitPromise:true});if(result.exceptionDetails)throw Error(JSON.stringify(result.exceptionDetails));return result.result.value;};
    await call('Page.enable');await call('Runtime.enable');await call('Emulation.setDeviceMetricsOverride',{width:1440,height:1120,deviceScaleFactor:1,mobile:false});
    const navigate=async route=>{await call('Page.navigate',{url:base+route});await until(()=>evaluate('document.readyState==="complete" && location.pathname==='+JSON.stringify(route.split('?')[0])),'Page did not load');};
    const screenshot=async name=>{const shot=await call('Page.captureScreenshot',{format:'png',captureBeyondViewport:false});fs.writeFileSync(path.join(output,name+'.png'),Buffer.from(shot.data,'base64'));};
    await test({evaluate,call,navigate,screenshot,until,delay,base,output,root});
    if(exceptions.length)throw Error(JSON.stringify(exceptions));await call('Browser.close');
  }finally{socket?.close();browser?.kill();fixture.closeAllConnections();await new Promise(resolve=>fixture.close(resolve));}
}
module.exports={withBrowser};
