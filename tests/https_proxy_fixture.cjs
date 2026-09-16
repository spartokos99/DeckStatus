// Local TLS termination with the same Host/Origin forwarding used by Caddy's HTTP upstream.
// No DNS, hosts-file or trust-store changes. The test browser maps the domain to this ephemeral port.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const http=require('node:http'),https=require('node:https'),{spawnSync}=require('node:child_process');
async function httpsProxy(backendPort,directory){
 const domain='deckstatus.example',baseUrl='https://'+domain,certificate=path.join(directory,'proxy.pfx');
 const generated=spawnSync('powershell.exe',['-NoProfile','-ExecutionPolicy','Bypass','-File',path.join(__dirname,'proxy_certificate.ps1'),'-OutputFile',certificate],{windowsHide:true,encoding:'utf8',timeout:15000});
 assert.equal(generated.status,0,'Test certificate generation failed: '+(generated.stderr||generated.error||''));
 const requests=[];
 const server=https.createServer({pfx:fs.readFileSync(certificate),passphrase:'proxy-fixture'},(req,res)=>{
  const record={method:req.method,path:req.url,host:req.headers.host,origin:req.headers.origin};requests.push(record);
  const upstream=http.request({host:'127.0.0.1',port:backendPort,path:req.url,method:req.method,
   headers:{...req.headers,'x-forwarded-proto':'https','x-forwarded-host':req.headers.host,'x-forwarded-for':req.socket.remoteAddress}},response=>{
    record.status=response.statusCode;res.writeHead(response.statusCode,response.headers);response.on('error',()=>res.destroy());response.pipe(res);
   });
  upstream.on('error',()=>{if(res.headersSent){res.destroy();return;}record.status=502;res.writeHead(502);res.end('Test backend unavailable');});
  req.pipe(upstream);
 });
 await new Promise((resolve,reject)=>{server.once('error',reject);server.listen(0,'127.0.0.1',resolve);});
 return {domain,baseUrl,requests,browserArgs:['--no-proxy-server','--host-resolver-rules=MAP '+domain+':443 127.0.0.1:'+server.address().port],
  async close(){server.closeAllConnections();await new Promise(resolve=>server.close(resolve));}};
}
module.exports={httpsProxy};
