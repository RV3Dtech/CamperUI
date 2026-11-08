// Real Node-RED HTTP In -> unchanged VanPi function -> simulated command sink / HTTP Response.
// Binds loopback only; contains no hardware, MQTT, HTTP-out or shell nodes.
const fs=require('node:fs'),path=require('node:path'),os=require('node:os'),http=require('node:http');
const assert=require('node:assert/strict');
const nodeRedDir=path.resolve(process.argv[2]);
const repo=path.resolve(process.argv[3]);
const requests=fs.readFileSync(process.argv[4],'utf8').trim().split(/\r?\n/).map(JSON.parse);
const RED=require(nodeRedDir);
const raw=fs.readFileSync(path.join(repo,'docs/flows.json'));
const nodes=JSON.parse(raw.toString(raw[0]===255?'utf16le':'utf8').replace(/^\uFEFF/,''));
const original=nodes.find(n=>n.id==='766532c5bcc4ca2d');
const answer=nodes.find(n=>n.id==='56e1abb5b2e7247e');
assert(original&&answer);
const captures=[],commands=[],errors=[];
const z='local-test-tab';
const fn=(id,func,wires)=>({id,z,type:'function',name:id,func,outputs:1,wires:[wires]});
const flow=[{id:z,type:'tab',label:'Loopback-only WLAN relay test'},
 {id:'input',z,type:'http in',url:'/wrelay/:input/:value',method:'put',upload:false,wires:[['observe']]},
 fn('observe',`global.get('captures').push({id:msg.req.headers['x-test-case'],type:typeof msg.payload,isBuffer:Buffer.isBuffer(msg.payload),payload:msg.payload});return msg;`,[original.id]),
 {...original,z,wires:[['command-sink'],[answer.id]]},
 fn('command-sink',`global.get('commands').push({id:msg.req.headers['x-test-case'],relay:msg.relay,state:msg.relaystate});return null;`,[]),
 {...answer,z,wires:[['response']]},
 {id:'response',z,type:'http response',statusCode:'',headers:{},wires:[]},
 {id:'catch',z,type:'catch',scope:[original.id],uncaught:false,wires:[['error-sink']]},
 fn('error-sink',`global.get('errors').push({id:msg.req.headers['x-test-case'],message:msg.error.message});return null;`,[])];
// Node-RED identifies executable nodes by their editor coordinates, even headless.
for(const n of flow)if(n.type!=='tab'){n.x=n.x??100;n.y=n.y??100;}
const server=http.createServer((req,res)=>RED.httpNode(req,res));
const folder=fs.mkdtempSync(path.join(os.tmpdir(),'camperui-wrelay-http-'));
fs.writeFileSync(path.join(folder,'flows.json'),JSON.stringify(flow));
const seenSockets=new Set();server.on('connection',s=>{seenSockets.add(s);s.on('close',()=>seenSockets.delete(s));});
function request(port,id,route,contentType,body){return new Promise((resolve,reject)=>{
 const req=http.request({host:'127.0.0.1',port,path:route,method:'PUT',headers:{'Content-Type':contentType,'Content-Length':Buffer.byteLength(body),'X-Test-Case':id}},res=>{
   let response='';res.setEncoding('utf8');res.on('data',c=>response+=c);res.on('end',()=>resolve({status:res.statusCode,body:response}));
 });
 req.on('error',e=>e.code==='EXPECTED_TIMEOUT'?resolve({timeout:true}):reject(e));
 req.setTimeout(1500,()=>{const e=new Error('Expected missing HTTP response');e.code='EXPECTED_TIMEOUT';req.destroy(e);});
 req.end(body);
});}
(async()=>{
 let started=false;
 try{
  RED.init(server,{userDir:folder,flowFile:'flows.json',httpAdminRoot:false,httpNodeRoot:'/',disableEditor:true,
    functionGlobalContext:{captures,commands,errors},logging:{console:{level:'info'}},
    editorTheme:{projects:{enabled:false}},contextStorage:{default:{module:'memory'}},externalModules:{autoInstall:false}});
  const ready=new Promise((resolve,reject)=>{const timeout=setTimeout(()=>reject(Error('Node-RED startup timeout')),60000);RED.events.once('flows:started',()=>{clearTimeout(timeout);resolve();});});
  started=true;await Promise.all([RED.start(),ready]);
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));const port=server.address().port;
  // Reproduce the asymmetry over actual HTTP, including Node-RED body parsing.
  let response=await request(port,'old-on','/wrelay/1/true','application/json','{}');
  assert.equal(response.status,200);assert.equal(JSON.parse(response.body).state,true);
  assert.equal(captures.find(x=>x.id==='old-on').type,'object');
  response=await request(port,'old-off','/wrelay/1/false','application/json','{}');
  assert.equal(response.timeout,true);
  assert(errors.some(x=>x.id==='old-off'&&/includes is not a function/.test(x.message)));
  assert(!commands.some(x=>x.id==='old-off'));
  console.log('PASS: original JSON {} -> object; ON forwarded, OFF throws .includes and has no HTTP response.');
  // An empty JSON body is NOT assumed safe: record its actual parsed type and result.
  response=await request(port,'empty-json','/wrelay/1/false','application/json','');
  const empty=captures.find(x=>x.id==='empty-json');assert(empty);
  console.log(`OBSERVED: empty application/json -> ${empty.type}; ${response.timeout?'no HTTP response':'HTTP '+response.status}.`);
  const wrelay=requests.filter(r=>r.path.startsWith('/wrelay/'));
  assert.equal(wrelay.length,16);
  for(let i=0;i<wrelay.length;i++){
    const r=wrelay[i],id='workaround-'+i;
    response=await request(port,id,r.path,r.contentType,r.body);
    assert.equal(response.status,200,r.path);
    const expected=r.path.endsWith('/true');const data=JSON.parse(response.body);
    assert.equal(data.state,expected);
    const input=captures.find(x=>x.id===id);assert.equal(input.type,'string');assert.equal(input.payload,'{}');assert.equal(input.isBuffer,false);
    const output=commands.filter(x=>x.id===id);assert.equal(output.length,1);assert.equal(output[0].state,expected);assert.equal(output[0].relay,data.relay);
    assert(!errors.some(x=>x.id===id));
  }
  console.log('PASS: actual CamperUI packet selection -> text/plain string {} -> unchanged VanPi flow; all 8 relays ON/OFF forwarded once with HTTP 200.');
  console.log('Node-RED version: '+require(path.join(nodeRedDir,'package.json')).version);
 }finally{
  if(started)await RED.stop();for(const s of seenSockets)s.destroy();
  if(server.listening)await new Promise(resolve=>server.close(resolve));
  assert(path.resolve(folder).startsWith(path.resolve(os.tmpdir())+path.sep));fs.rmSync(folder,{recursive:true,force:true});
 }
})().catch(error=>{console.error(error);process.exitCode=1;});
