/** Differential oracle. Does not modify TypeScript runtime or publish user saves. */
import { readFileSync, writeFileSync, mkdirSync, existsSync } from 'node:fs';
import { resolve } from 'node:path';
import { spawnSync } from 'node:child_process';
import { deepStrictEqual } from 'node:assert';
import { exportNativeSave, importNativeSave, buildNativeOol, SAVED_GAM_SIZE, type SaveSidecar } from '../../../game/src/core/saveNative.js';
import { emptySidecar, readU5gamEnvelope } from '../../../game/src/core/u5gam.js';
import { deserialize, serialize, type GameState } from '../../../game/src/core/state.js';
import { setCargaFiel } from '../../../game/src/core/npc/carga-fiel.js';
import { NpcManager } from '../../../game/src/core/npc/manager.js';
const dir=resolve('native/core/build-persistence'); mkdirSync(dir,{recursive:true});
const queries: unknown[]=[], expected: unknown[]=[], labels:string[]=[];
const clean=(v:unknown)=>JSON.parse(JSON.stringify(v));
const hex=(b:Uint8Array)=>Buffer.from(b).toString('hex');
const add=(label:string,q:unknown,e:unknown)=>{ labels.push(label); queries.push(q); expected.push(clean(e)); };
let seed=0x926a; const rand=()=>{ seed=(Math.imul(seed,1664525)+1013904223)>>>0; return seed>>>24; };
const template=Uint8Array.from({length:SAVED_GAM_SIZE},rand);
const base=importNativeSave(template,emptySidecar());
base.partySize=6; base.characters.forEach((c,i)=>{c.name=`Member${i}`;});
const ool=Uint8Array.from({length:512},rand);
function exportCase(label:string,s:GameState,t:Uint8Array,o:Uint8Array,gate=false) {
  setCargaFiel(gate); const ex=exportNativeSave(s,t); const imported=importNativeSave(ex.gam,ex.sidecar);
  const meta={name:'Résumé 雨',locationName:'Test',turns:s.turnsSinceStart,timestamp:1234,provenance:'momento'};
  // Envelope serialization is compared logically after native -> TS decoding below.
  add(label,{op:'export',state:s,template:hex(t),ool:hex(o),meta,gate},{error:0,gam:hex(ex.gam),sidecar:ex.sidecar,ool:hex(buildNativeOol(s,o)),imported,meta});
}
for(let i=0;i<256;++i) {
  const s=structuredClone(base); s.position={location:i%3===0?0:17,floor:i%2?255:-1,x:i,y:255-i}; s.transportTile=i; s.transport=['foot','horse','carpet','skiff','ship'][i%5] as GameState['transport'];
  s.turnsSinceStart=i*999; s.keys=i-128; s.shipHull=i; s.shipSkiffs=255-i; s.grapple=!!(i%2);
  s.characters[0]!.name=i%2?'123456789':'é\u00ff'; s.characters[0]!.currentHp=i*257;
  s.moonstones.forEach((m,k)=>{m.buried=!!(i%2);m.location=(i+k)%256;});
  s.questFlags={'word-spoken:33':!!(i&1),'search:13':true}; s.shadowlordDoomBits=i; s.shadowlordLocs=[i,255-i,0];
  s.worldObjects=[{location:0,floor:0,x:1,y:2,tile:0x120,kind:'ship',hull:99,skiffs:2,slot:1},{location:0,floor:255,x:3,y:4,tile:0x110,kind:'horse'}];
  s.overworldEnemies=[{tile:300,defIndex:8,water:true,x:i,y:1,hull:i,windCtr:i,slot:2},{tile:320,defIndex:0,water:false,x:5,y:6}];
  s.journal=[{custom:'preserved 雨',nested:[1,true,null]}] as unknown as GameState['journal'];
  s.mapOverrides={'17:-1:6:10':257,'0:0:1:2':68}; s.openDoors=[];
  exportCase(`synthetic-${i}`,s,template,i%3?ool:new Uint8Array(3),!!(i%2));
}
let real=0;
for(let tile=0;tile<256;++tile) {
  const bytes=template.slice(); bytes[0x2ed]=0; bytes[0x2ef]=tile%2?255:0;
  bytes[0x6bc]=tile; bytes[0x6bd]=255-tile;
  setCargaFiel(false);
  add(`native-actor-tile-${tile}`,{op:'import',gam:hex(bytes)},{error:0,source:0,state:importNativeSave(bytes,emptySidecar())});
}
for(const fill of [0,255]) {
  const bytes=new Uint8Array(SAVED_GAM_SIZE).fill(fill);
  const s=importNativeSave(bytes,emptySidecar()); exportCase(`all-bytes-${fill}`,s,bytes,new Uint8Array(512).fill(fill));
}
for(const count of [0,1,6,15]) {
  const s=structuredClone(base); s.characters=s.characters.slice(0,count); exportCase(`short-roster-${count}`,s,template,ool);
}
for(const path of ['original/u5/ultima5/INIT.GAM','original/u5/ultima5/SAVED.GAM']) {
  if(!existsSync(path)) continue; const bytes=readFileSync(path); let s=importNativeSave(bytes,emptySidecar());
  for(let cycle=0;cycle<3;++cycle) { exportCase(`${path}-cycle${cycle}`,s,bytes,ool); const ex=exportNativeSave(s,bytes); s=importNativeSave(ex.gam,ex.sidecar); } ++real;
}
// Explicit and enveloped sidecar precedence, permissive version semantics, damaged tails.
for(const tail of ['', 'junk','U5PARTIDA1\n{','U5PARTIDA1\nnull', ...[
  {formato:'openu5-partida',version:1,sidecar:emptySidecar()},
  {formato:'openu5-partida',version:2,sidecar:emptySidecar()},
  {formato:'openu5-partida',version:1,sidecar:{...emptySidecar(),version:999}},
  {formato:'openu5-partida',version:1,sidecar:{qol:{journal:[]},gameState:[]}},
  {formato:'openu5-partida',version:1,sidecar:{qol:{},gameState:{}}},
].map(x=>'U5PARTIDA1\n'+JSON.stringify(x))]) {
  const bytes=Buffer.concat([template,Buffer.from(tail)]); const env=readU5gamEnvelope(bytes);
  add(`envelope-${tail}`,{op:'envelope',gam:hex(bytes)},{kind:env.kind==='ok'?2:env.kind==='bad'?1:0,...(env.kind==='ok'?{envelope:env.envelope}:{})});
  for(const explicit of [undefined,JSON.stringify(emptySidecar()),'{']) {
    let result:unknown; try { const side=explicit===undefined?(env.kind==='ok'?env.envelope.sidecar:emptySidecar()):JSON.parse(explicit); result={error:0,state:importNativeSave(bytes,side),source:explicit!==undefined?1:env.kind==='ok'?2:0}; } catch(err) { result={error:err instanceof SyntaxError?1:5}; }
    add('import-precedence',{op:'import',gam:hex(bytes),...(explicit===undefined?{}:{sidecarText:explicit})},result);
  }
}
for(const length of [0,1,512,4191]) add(`truncated-${length}`,{op:'import',gam:hex(template.slice(0,length))},{error:6});
for(const length of [0,4191]) add(`short-template-${length}`,{op:'export',state:base,template:hex(template.slice(0,length)),ool:''},{error:7});
const states:unknown[]=[base,{...base,version:2},{...base,characters:[]},{...base,partySize:0},{...base,time:{}},{...base,equipmentQuantities:[null]}, {...base,treasuryLoot:{foo:1},mapOverrides:{'17:-1:6:10':257,'17:-1:7:10':79},questFlags:{'search:13':true,'search:14':true,'search:15':true,other:true}}, {...base,worldObjects:null,overworldEnemies:{}}];
const partial=structuredClone(base) as unknown as Record<string,unknown>;
for(const k of ['questFlags','journal','food','equipmentQuantities','npcMet','specialItems']) delete partial[k];
states.push(partial);
for(const state of states) { const text=JSON.stringify(state); let e:unknown; try {e={error:0,state:deserialize(text)};} catch(err) {const msg=String(err);e={error:msg.includes('Versión')?3:msg.includes('sin personajes')?4:5};} add('deserialize',{op:'deserialize',text},e); }
const badUtf=Buffer.concat([template,Buffer.from('U5PARTIDA1\n\ufeff{"formato":"openu5-partida","version":1,"meta":{"name":"'),Buffer.from([0xed,0xa0,0x80]),Buffer.from('"},"sidecar":'+JSON.stringify(emptySidecar())+'}')]);
const decodedUtf=readU5gamEnvelope(badUtf); if(decodedUtf.kind!=='ok') throw new Error('UTF8 reference fixture failed');
add('replacement-utf8-envelope',{op:'envelope',gam:hex(badUtf)},{kind:2,envelope:decodedUtf.envelope});
for(const text of ['{','null','[1]','{"version":1,"characters":[]}']) { let e:unknown; try {e={error:0,state:deserialize(text)};} catch(err) {const msg=String(err);e={error:msg.includes('Versión')?3:msg.includes('sin personajes')?4:text==='null'?3:1};} add('malformed-json',{op:'deserialize',text},e); }
// Native core binding keeps unsupported document fields and the live RNG stream.
const bound=deserialize(serialize(base)); bound.characters.forEach(c=>{c.name='Avatar';});
const changed=structuredClone(bound); changed.gold++; changed.position.x=(changed.position.x+1)&255;
add('core-binding',{op:'bind',state:bound},{error:0,state:changed,seed:12345,atomic:true});
const valid={sequence:1,committed:true,identity:true,gam:hex(template),ool:hex(ool),sidecar:emptySidecar(),requireOol:true,requireSidecar:true};
for(const bad of [{committed:false},{identity:false},{gam:'00'},{ool:''},{sidecar:{qol:{journal:[]},gameState:[]}},{badCrc:true}]) add('generation-recovery',{op:'recovery',generations:[valid,{...valid,sequence:2,...bad}]},{selected:0});
add('newest-generation',{op:'recovery',generations:[valid,{...valid,sequence:2}]},{selected:1});
add('no-generation',{op:'recovery',generations:[]},{selected:-1});
for(let i=0;i<12;++i) {
  const index=[{id:'older',timestamp:100},{id:'newer',timestamp:101},{id:'same',timestamp:101}];
  add('metadata',{op:'metadata',index,pointer:i},{selected:1,slot:i%3+1});
}
for(const text of ['null','true','false','[1,-0,1.5,2e3]','{"duplicate":1,"duplicate":2}','"\\ud800"','"🌍雨"','{"unknown":[{},null]}']) add('json-domain',{op:'json',text},{error:0,value:JSON.parse(text)});
for(const text of ['','[1,]','{"a":}','01','+1','1.','NaN','"\\q"']) add('json-invalid',{op:'json',text},{error:1});
add('json-depth-capacity',{op:'json',text:'['.repeat(66)+'0'+']'.repeat(66)},{error:2});
add('json-node-capacity',{op:'json',text:'['+new Array(65536).fill('0').join(',')+']'},{error:2});
const slots=Array.from({length:3},(_,i)=>({slot:i+1,type:0x30,dialogNumber:1,aiTypes:[0,0,0],x:[1,1,1],y:[2,2,2],z:[0,0,0],times:[0,8,16,24]}));
for(const gate of [false,true]) for(const walk of [undefined,null,{location:18,slots:[]},{location:17,slots:[]},{location:17,slots:[{slot:2,x:12,y:15,z:-1,state:2,servedSlot:1,pathBuf:[3,4],pathIdx:2,stuck:100}]}]) {
  setCargaFiel(gate); const state=structuredClone(base); state.time.hour=0; state.npcDead=Array.from({length:32},()=>new Array(32).fill(false));
  state.npcWalk=walk; const manager=new NpcManager({17:slots} as never); manager.enterMap(17,state,true);
  add('npc-restore',{op:'npc',state:{npcWalk:walk},gate},{error:0,walk:state.npcWalk});
}
add('native-api-contracts',{op:'contracts'},{passed:8});
add('sizes',{op:'sizes'},null);
writeFileSync(`${dir}/requests.ndjson`,queries.map(q=>JSON.stringify(q)).join('\n')+'\n');
const exe=process.argv[2]??'native/core/build-zig/persistence_driver.exe';
const run=spawnSync(resolve(exe),[`${dir}/requests.ndjson`,`${dir}/results.ndjson`],{encoding:'utf8'}); if(run.status!==0) throw new Error(`Native driver failed: ${run.status} ${run.stderr} ${run.error}`);
const results=readFileSync(`${dir}/results.ndjson`,'utf8').trim().split('\n').map(x=>JSON.parse(x)); deepStrictEqual(results.length,expected.length);
for(let i=0;i<results.length;++i) {
  const r=results[i], e=expected[i] as Record<string,unknown>|null;
  if(!e) { console.log('Sizes',r); continue; }
  if(typeof r.envelope === 'string') {const env=readU5gamEnvelope(Buffer.from(r.envelope,'hex')); deepStrictEqual(env.kind,'ok'); if(env.kind==='ok') {deepStrictEqual(clean(env.envelope.meta),e.meta); deepStrictEqual(clean(env.envelope.sidecar),e.sidecar);} delete r.envelope; delete e.meta; }
  try {deepStrictEqual(r,e);} catch { writeFileSync(`${dir}/mismatch.json`,JSON.stringify({label:labels[i],actual:r,expected:e},null,2)); throw new Error(`Parity mismatch ${i}: ${labels[i]} (${dir}/mismatch.json)`); }
  if(typeof r.gam==='string') {setCargaFiel((queries[i] as {gate:boolean}).gate); const nativeState=importNativeSave(Buffer.from(r.gam,'hex'),r.sidecar as SaveSidecar); deepStrictEqual(clean(nativeState),r.imported);}
}
console.log(`Persistence: ${results.length-1} scenarios passed (${results.length-12} compatibility, 8 generation contracts, 2 capacity contracts, 1 API scenario with 8 assertions); ${real} real save sources, ${real*3} real cycles; native -> TS envelopes and saves verified.`);
