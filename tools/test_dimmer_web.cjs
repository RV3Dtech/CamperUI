const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const root=process.argv[2] || path.resolve(__dirname,'..');
const source=fs.readFileSync(path.join(root,'web_settings_html.h'),'utf8');
const scripts=[...source.matchAll(/<script>([\s\S]*?)<\/script>/g)].map(m=>m[1]);
const nodes=new Map();
function element(){return {value:'0',checked:false,disabled:false,style:{},options:[],innerHTML:'',children:[],
 classList:{add(){},remove(){},contains(){return false;}},appendChild(child){this.children.push(child);},
 closest(){return element();},addEventListener(){},setAttribute(){}};}
const document={documentElement:{dataset:{},style:{setProperty(){}}},
 getElementById(id){if(!nodes.has(id))nodes.set(id,element());return nodes.get(id);},
 querySelectorAll(){return [];},createElement(){return element();}};
let captured;
const context=vm.createContext({document,window:{addEventListener(){}},console,setTimeout(){},setInterval(){},
 fetch(url,options){assert.equal(url,'/api/settings');captured=JSON.parse(options.body);return Promise.resolve({ok:true,json:async()=>({status:'ok'})});}});
for(const script of scripts)vm.runInContext(script,context);
vm.runInContext('tabOrder = tabMeta.map(t=>t.id); tabEnabled = {}; renderTabReorderList();',context);
const master=document.getElementById('show_dimmers'),list=document.getElementById('dimmersList');
assert.equal(master.checked,true);assert.equal(list.style.display,'block');
// Preserve mixed per-channel choices when hiding/reopening the whole group.
for(let i=0;i<10;i++)document.getElementById('dimmer_vis_'+i).checked=i%2===0;
const expected=[true,false,true,false,true,false,true,false,true,false];
vm.runInContext('setTabEnabled(1,false);saveSettings()',context);
assert.equal(master.checked,false);assert.equal(list.style.display,'none');
assert.equal(captured.tab_enabled[1],false);assert.deepEqual(captured.dimmer_visible,expected);
vm.runInContext('setTabEnabled(1,true);saveSettings()',context);
assert.equal(master.checked,true);assert.equal(list.style.display,'block');
assert.equal(captured.tab_enabled[1],true);assert.deepEqual(captured.dimmer_visible,expected);
// Changes from the menu settings use the same callback/value, in both directions.
assert(source.includes('onchange="setTabEnabled(${tabId}, this.checked)"'));
vm.runInContext('setTabEnabled(0,false)',context);
assert.equal(master.checked,true,'Other tabs must not change the dimmer master');
vm.runInContext('tabEnabled[1]=false;renderTabReorderList()',context);
assert.equal(master.checked,false);
console.log('PASS: actual Websettings JS, dimmer/menu synchronization and saved individual preferences.');
