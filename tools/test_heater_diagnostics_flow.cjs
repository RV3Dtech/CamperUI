const fs=require('fs'),vm=require('vm'),assert=require('assert');
const root=require('path').resolve(__dirname,'..')+'/';
const bytes=fs.readFileSync(root+'docs/flows.json');
const nodes=JSON.parse(bytes.toString(bytes[0]===255 && bytes[1]===254 ? 'utf16le':'utf8').replace(/^\uFEFF/,''));
const code=nodes.find(n=>n.id==='ef5df576.d9abc8').func;
// Exercise the real VanPi HTTP response builder, including its historical heatglow key.
const globals=new Map(Object.entries({autoterm1:true,usbport_autoterm1:1,
 heatvolt_autoterm1:13,heatfan_autoterm1:4200,heatpump_autoterm1:1.5,heattemp_autoterm1:144}));
const ctx={global:{get:k=>globals.get(k),set:(k,v)=>globals.set(k,v)},msg:{}};
const lines=code.split('\n').filter(l=>/autoterm1\s*=/.test(l));
for(const l of lines){const key=l.match(/global.get\(["'`]([^"'`]+)["'`]\)/);if(key)globals.set(key[1],true);}
const result=vm.runInNewContext('(function(){'+code+'})()',ctx);
assert(result.payload.autoterm1,'VanPi fixture must enable the first Autoterm');
const h=result.payload.autoterm1;
assert.equal(Number(h.heatvolt),13);assert.equal(Number(h.heatfan),4200);
assert.equal(Number(h.heatglow),1.5);assert.equal(Number(h.heattemp),144);
assert(!('heatpump' in h),'Bundled API uses heatglow for fuel pump Hz');
console.log('PASS: real VanPi /heater response maps V/rpm/Hz/C without extra scaling.');

// Execute the real Webasto telemetry writers, then the real /heater builder.
// This proves the bundled mapping only, not live support on a user's installation.
const runNode=(id,payload)=>vm.runInNewContext('(function(){'+nodes.find(n=>n.id===id).func+'})()',
 {global:ctx.global,msg:{payload}});
globals.delete('usbport_autoterm1');
runNode('634cf50f64721a70',{temp:86});
runNode('94ea8fc746d56e91',{volts:13200});
const webasto=vm.runInNewContext('(function(){'+code+'})()', {...ctx,msg:{}}).payload;
assert.equal(Number(webasto.heattemp),36);
assert.equal(Number(webasto.heatvolt),13.2);
assert(!webasto.autoterm1,'Webasto does not require Autoterm enabled');
globals.delete('heattemp');globals.delete('heatvolt');
const missing=vm.runInNewContext('(function(){'+code+'})()', {...ctx,msg:{}}).payload;
assert.equal(missing.heattemp,'wait');assert.equal(missing.heatvolt,0);
if(process.argv[2]==='--fixtures')fs.writeFileSync(process.argv[3],JSON.stringify(webasto)+'\n'+JSON.stringify(missing)+'\n');
console.log('PASS: real Webasto writers -> /heater root fields; missing values become wait/0 upstream.');
