/** Synthetic differential corpus: all expectations execute authoritative TS. */
import { readFileSync, writeFileSync } from 'node:fs';
import { OriginalRng } from '../../../game/src/core/rng-original.js';
import type { GameState } from '../../../game/src/core/state.js';
import { advanceClock, turnHousekeeping } from '../../../game/src/core/world/survival.js';
import { outdoorTurn, townTurn, townNpcTailRuns } from '../../../game/src/core/world/loops/turn.js';
import { NpcManager, npcCheckSchedule, type NpcRuntime, type NpcSlot } from '../../../game/src/core/npc/manager.js';
import { DoorManager } from '../../../game/src/core/world/doors.js';
import type { WorldData } from '../../../game/src/core/world/map.js';
import { composeWorldPool, acquireActorSlot, scanRecyclableSlot, findFreeActorSlot, firstFreeRecycleSlot, type PoolObjectLike, type PoolEnemyLike } from '../../../game/src/core/world/actorPool.js';
import { Game } from '../../../game/src/core/game.js';
import { rollSpawnGate } from '../../../game/src/core/world/loops/spawn.js';

const transport = ['foot','horse','carpet','skiff','ship'] as const;
const sites = ['?','wind','troll','troll.wind','swamp','burnTick','burn','hazard','housekeeping','spawn','confusion','wake','swampTown','damageTick','wind2','hook'];
const messages = ['Starving!','Hic!','A TRAPDOOR!','Burning!'];
const code = (s?: string) => s ? s.charCodeAt(0) : 0;
const rows: {kind: number; input: number[]; expected: number[]}[] = [];
const coverage:Record<string,number>={};
const hit=(name:string,condition:boolean)=>{if(condition)coverage[name]=(coverage[name]??0)+1;};
function base(scenario: number): GameState {
  return {
    time:{year:139,month:13,day:28,hour:[23,5,11,17,0,19][scenario%6]!,minute:59},
    position:{location:scenario%2?2:0,floor:[0,-1,255][scenario%3]!,x:10,y:10},
    characters:Array.from({length:8},(_,i)=>({status:['G','P','S','D'][i%4],currentHp:i+2,maxHp:40,ring:i%2?44:0,strength:40-i,dexterity:i*4,monthsAtInn:23+i})),
    partySize:6,activeCharacter:1,food:scenario%2?0:2,torchTurns:3,turnsSinceStart:4294967290,
    prevHour:3,lightSpellMins:5,drunkTurns:3,timeSpell:([undefined,'Q','T'] as const)[scenario%3],timeSpellTurns:scenario%4?2:255,
    transport:transport[scenario%5],transportTile:[28,18,19,20,21,22][scenario%6],wind:2,windDriftCtr:3,
    shadowlordLocs:[1,2,128],skullTreeFoundDay:20,reagentPatchFoundDay:[1,2,3],feluccaPhase:49,trammelPhase:50,npcDead:[],
  } as unknown as GameState;
}
// State field order is the shared test wire format, not a save codec.
function state(s: GameState, rng: OriginalRng): number[] {
  const t=s.time,p=s.position;
  return [t.year,t.month,t.day,t.hour,t.minute,p.location,p.floor,p.x,p.y,s.partySize,s.activeCharacter,s.food,s.torchTurns,s.turnsSinceStart,
    s.prevHour??-1,s.lightSpellMins??0,s.drunkTurns??0,code(s.timeSpell),s.timeSpellTurns??-1,transport.indexOf(s.transport),s.transportTile??28,s.wind??0,s.windDriftCtr??0,
    +(s.shadowlordLocs!==undefined),...(s.shadowlordLocs??[-1,-1,-1]),s.skullTreeFoundDay??0,...(s.reagentPatchFoundDay??[0,0,0]),s.feluccaPhase??48,s.trammelPhase??48,
    s.characters.length,...s.characters.flatMap(c=>[code(c.status),c.currentHp,c.maxHp,c.ring,c.strength,c.dexterity,c.monthsAtInn]),rng.getSeed()];
}
const sky={moonPhasesRaw:Array.from({length:56},(_,i)=>48+i%8),location:2};
for(let scenario=0;scenario<18;scenario++) for(let seed=0;seed<24;seed++) {
  const s=base(scenario), rng=new OriginalRng(seed*2731), phases={mount:0,quickness:0};
  s.timeSpell=([undefined,'Q','T'] as const)[seed%3];
  s.position.floor=[0,-1,255][Math.floor(seed/3)%3]!;
  if(seed%4===0)s.shadowlordLocs=undefined;
  if(seed%5===0)s.characters=s.characters.slice(0,2);
  if(seed%7===0)s.timeSpellTurns=undefined;
  for(let step=0;step<16;step++) {
    const mode=scenario%3;
    let flags=(step*37+scenario*11+seed*17)&255;
    const tile=[5,4,143,188,140][step%5]!;
    if(mode===1&&tile===140)flags|=1;
    const minutes=[-1,0,1,2,6,120][step%6]!;
    const before=state(s,rng);
    const phaseBefore=[phases.mount,phases.quickness];
    const trace: number[]=[]; let hooks=0,falls=0;
    const rand=(lo:number,hi:number)=>rng.next(lo,hi);
    // External hook draws are omitted by TS's internal trace; collect separately
    // in a total draw stream below, while retaining internal site labels.
    const allDraws:number[]=[];
    const raw=(lo:number,hi:number)=>{const value=rand(lo,hi);allDraws.push(lo,hi,value);return value;};
    const hookRaw=()=>{hooks++;allDraws.push(0,31,rand(0,31));};
    const ctxSky={...sky,location:flags&128?33:2};
    let result:number[];
    if(mode===0) {
      const r=outdoorTurn(s,raw,{tileUnderParty:tile,minutes,blocked:!!(flags&1),onBridge:!!(flags&2),onSwamp:!!(flags&4),skipWorldTurn:!!(flags&8),sky:ctxSky,afterWind:hookRaw});
      result=[r.messages.length,...r.messages.map(m=>messages.indexOf(m)),r.poisoned.length,...r.poisoned,r.poisonTicks.length,...r.poisonTicks,+!!r.spawn,...(r.spawn?[r.spawn.roll,r.spawn.threshold,+r.spawn.spawn]:[]),+!!r.troll,
        ...(r.troll?[+r.troll.fired,+r.troll.onFoot,+r.troll.runsInnerWorldTurn,r.troll.payerIndex??-1,r.troll.toll,r.troll.dexRolls.length,...r.troll.dexRolls.flatMap((v,i)=>[r.troll!.rolledIndices[i]!,v])]:[]),+r.hazard,+r.burning];
      trace.push(...r.trace.flatMap(t=>[sites.indexOf(t.site),t.lo,t.hi,t.value]));
      hit('underworldHazard',r.hazard);hit('spawnFired',r.spawn?.spawn===true);hit('trollFired',r.troll?.fired===true);hit('trollPayer',r.troll?.payerIndex!=null);hit('outdoorBurning',r.burning);
    } else if(mode===1) {
      const r=townTurn(s,raw,{consumesTurn:!!(flags&1),confused:!!(flags&2),preRolled:!!(flags&4),damageTile:!!(flags&8),onSwampTile:!!(flags&16),secondWorldTurn:!!(flags&32),passCommand:!!(flags&64),npcPhases:phases,sky:ctxSky,afterHousekeeping:hookRaw,
        ...(step%2?{tileUnderParty:()=>falls<(seed%4===0?20:2)?tile:143,onTrapdoor:()=>{falls++;if(seed%4===1)return 'none' as const;if(seed%4===2)return 'tpk' as const;s.position.floor--;return 'fell' as const;}}:{})});
      result=[r.messages.length,...r.messages.map(m=>messages.indexOf(m)),r.poisoned.length,...r.poisoned,r.poisonTicks.length,...r.poisonTicks,0,0,0,0];
      trace.push(...r.trace.flatMap(t=>[sites.indexOf(t.site),t.lo,t.hi,t.value]));
      hit('trapdoorGuard',falls===9);hit('trapdoorNone',falls>0&&seed%4===1);hit('trapdoorTpkHook',falls>0&&seed%4===2);hit('townBurning',r.messages.includes('Burning!'));
    } else {
      advanceClock(s,minutes,raw,ctxSky);
      const poison:number[]=[];const m=turnHousekeeping(s,raw,i=>poison.push(i));
      result=[m.length,...m.map(m=>messages.indexOf(m)),0,poison.length,...poison,0,0,0,0];
    }
    rows.push({kind:0,input:[scenario*24+seed,step,mode,flags,tile,minutes,...phaseBefore,...before],expected:[...state(s,rng),phases.mount,phases.quickness,hooks,falls,...result,trace.length,...trace,allDraws.length,...allDraws]});
  }
}
// Cadence compares both actual TS gate methods, including their different order.
for(let tile=16;tile<=29;tile++) for(const spell of [undefined,'T','Q'] as const) for(let pass=0;pass<2;pass++) {
  const s=base(0);s.transportTile=tile;s.timeSpell=spell;
  s.transport=tile<20?'horse':tile<22?'carpet':'foot';
  const phases={mount:0,quickness:0};
  const game={state:s,mountPhase:0,quicknessPhase:0};
  const gate=(Game.prototype as unknown as {outdoorWorldTurnRuns(this: typeof game): boolean}).outdoorWorldTurnRuns;
  for(let i=0;i<12;i++) {
    const input=[tile,code(spell),transport.indexOf(s.transport),pass,phases.mount,phases.quickness,game.mountPhase,game.quicknessPhase];
    rows.push({kind:1,input,expected:[+townNpcTailRuns(s,phases,!!pass),phases.mount,phases.quickness,+gate.call(game),game.mountPhase,game.quicknessPhase]});
  }
}
function slot(i:number,ai=1):NpcSlot {return {slot:i,type:64,dialogNumber:0,aiTypes:[ai,ai,ai],x:[10,15,20],y:[10,15,20],z:[0,255,1],times:[0,6,12,18]};}
function slotWire(s:NpcSlot):number[]{return [s.slot,s.type,s.dialogNumber,...s.aiTypes,...s.x,...s.y,...s.z,...s.times];}
function actorWire(n:NpcRuntime):number[]{return [...slotWire({slot:n.slot,type:n.type,dialogNumber:n.dialogNumber,aiTypes:n.aiTypes,x:n.schedX,y:n.schedY,z:n.schedZ,times:n.times}),n.location,n.x,n.y,n.z,n.state,n.servedSlot,n.pathIdx,n.stuck,...n.pathBuf];}
function actor(s:NpcSlot):NpcRuntime {return {slot:s.slot,type:s.type,dialogNumber:s.dialogNumber,aiTypes:s.aiTypes,schedX:s.x,schedY:s.y,schedZ:s.z,times:s.times,location:2,x:10,y:10,z:0,state:1,servedSlot:0,pathIdx:-1,stuck:0,pathBuf:Array(32).fill(0)};}
for(const hour of [0,1,6,12,18,23]) for(const z of [-1,0,1,2]) for(const dest of [0,1,255]) for(const visible of [-1,0,1]) for(const served of [0,1,2]) for(const atPost of [false,true]) {
  const n=actor(slot(1));n.z=z;n.schedZ=[dest,dest,dest];n.servedSlot=served;n.x=atPost?15:7;
  const input=[hour,visible,...actorWire(n)];const value=npcCheckSchedule(n,hour,visible);
  rows.push({kind:2,input,expected:[value,...actorWire(n)]});
}
// Public manager fresh-entry + multi-actor ticks; includes reversed input order,
// blocked cells, party occupancy, map edges, all eight AI modes and RNG order.
for(let scenario=0;scenario<16;scenario++) for(let seed=0;seed<12;seed++) {
  const s=base(0);s.position={location:2,floor:0,x:scenario%2?1:11,y:scenario%2?1:11};s.time.hour=1;
  const tiles=Array.from({length:32},(_,y)=>Array.from({length:32},(_,x)=>scenario%4===3 || (x===12&&y!==10)?0:scenario>=8&&x===11&&y===9?162:5));
  const world={overworld:[],underworld:[],smallMaps:new Map([[2,{id:2,name:'synthetic',floors:[{z:0,tiles}]}]])} as unknown as WorldData;
  const slots=[slot(3,scenario%8),slot(1,(scenario+1)%8),slot(2,(scenario+2)%8),{...slot(4),type:1},slot(0),{...slot(5),type:0,x:[0,0,0],y:[0,0,0]} as NpcSlot,slot(6)];
  slots.forEach((n,i)=>{if(n.type===64){n.x=[scenario%2?i%2:10+i,15,20];n.y=[scenario%2?0:10,15,20];}});
  if(scenario>=8)slots[0]!.type=16;
  s.npcDead[1]=[];s.npcDead[1]![6]=true;
  const rng=new OriginalRng(seed*4093),manager=new NpcManager({2:slots},rng),doors=new DoorManager();
  manager.enterMap(2,s);
  let live=manager.npcsAt(2,0);
  rows.push({kind:3,input:[scenario,seed*4093,s.position.x,s.position.y,slots.length,...slots.flatMap(slotWire)],expected:[live.length,...live.flatMap(actorWire),rng.getSeed()]});
  for(let step=0;step<24;step++) {
    const before=live.flatMap(actorWire);const oldSeed=rng.getSeed();
    manager.tickGuards(s,world,doors);manager.tick(s,world,doors);live=manager.npcsAt(2,0);
    rows.push({kind:4,input:[scenario,step,s.position.x,s.position.y,oldSeed,live.length,...before],expected:[live.length,...live.flatMap(actorWire),rng.getSeed()]});
  }
}
for(let scenario=0;scenario<256;scenario++) {
  const loc=scenario%3===0?2:0,floor=scenario%2?-1:255;
  const enemies:PoolEnemyLike[]=Array.from({length:scenario%33},(_,i)=>({slot:i%3?i+1:undefined,tile:256+(i%4?128:181),x:(scenario+i*7)&255,y:i*3}));
  const objects:PoolObjectLike[]=Array.from({length:scenario%35},(_,i)=>({slot:i%3?31-i:undefined,tile:[1,181,16,64][i%4]!,x:(scenario-i)&255,y:i*9,floor:i%2?-1:0,location:i%4?loc:3}));
  const wire=(e:PoolEnemyLike|PoolObjectLike)=>[e.slot??-1,e.tile,e.x,e.y,'floor' in e?e.floor:0,'location' in e?e.location:0];
  const input=[loc,floor,scenario,scenario,enemies.length,...enemies.flatMap(wire),objects.length,...objects.flatMap(wire)];
  const view=composeWorldPool({location:loc,floor,enemies,objects});
  const occupied=new Set(view.flatMap((v,i)=>v.owner?[i]:[]));
  rows.push({kind:5,input,expected:[...enemies.map(e=>e.slot??-1),...objects.map(o=>o.slot??-1),...view.flatMap(v=>[v.tile0,v.x,v.y,v.floor,v.owner?.kind==='enemy'?1:v.owner?.kind==='object'?2:0,v.owner?(v.owner.kind==='enemy'?enemies.indexOf(v.owner.ref):objects.indexOf(v.owner.ref)):0]),acquireActorSlot(view,scenario,scenario),scanRecyclableSlot(view,128,255,true,scenario,scenario),findFreeActorSlot(occupied),firstFreeRecycleSlot(occupied)]});
}
for(const minutes of [-1,0,1,2,59,60,120,1440,2147483000]) for(const spell of [undefined,'Q','T'] as const) for(let variant=0;variant<12;variant++) {
  const s=base(0);s.timeSpell=spell;s.position.floor=[0,-1,255][variant%3]!;
  s.shadowlordLocs=variant%2?[1,2,3]:undefined;
  s.time={year:139,month:13,day:28,hour:23,minute:59};
  const rng=new OriginalRng(variant*4999),before=state(s,rng);
  const rand=(lo:number,hi:number)=>rng.next(lo,hi);
  const context={location:variant>=6?33:2,moonPhasesRaw:variant%2?[]:sky.moonPhasesRaw};
  advanceClock(s,minutes,variant&1?rand:undefined,variant&2?context:undefined);
  rows.push({kind:6,input:[minutes,code(spell),variant,...before],expected:state(s,rng)});
}
for(const tile of [-1,0,4,5,8,9,15,16,31,32,38,39,143,255]) for(const floor of [-1,0,1,127,128,255]) for(const hour of [0,4,5,19,20,23,31,32,255]) {
  const rng=new OriginalRng((tile+1)*211+floor+hour),seed=rng.getSeed();
  const r=rollSpawnGate((lo,hi)=>rng.next(lo,hi),tile,floor,hour);
  rows.push({kind:7,input:[tile,floor,hour,seed],expected:[r.roll,r.threshold,+r.spawn,rng.getSeed()]});
}
// End-to-end composition: town housekeeping -> guard/NPC hook -> wind2,
// all consumers share one live RNG. No command/UI or pathfinding involved.
for(let scenario=0;scenario<4;scenario++) for(let seed=0;seed<4;seed++) {
  const s=base(1);s.position={location:2,floor:0,x:11,y:11};s.time.hour=5;s.time.minute=58;
  s.timeSpell=([undefined,'Q','T',undefined] as const)[scenario];s.transportTile=scenario===3?18:28;
  const slots=[slot(3,0),slot(1,1),slot(2,4)];slots[0]!.type=16;
  slots.forEach((n,i)=>{n.times=[255,255,255,255];n.x=[10+i,10+i,10+i];n.y=[10,10,10];n.z=[0,0,0];});
  const tiles=Array.from({length:32},()=>Array(32).fill(5));
  const world={overworld:[],underworld:[],smallMaps:new Map([[2,{id:2,name:'synthetic',floors:[{z:0,tiles}]}]])} as unknown as WorldData;
  const rng=new OriginalRng(seed*1777),mgr=new NpcManager({2:slots},rng),doors=new DoorManager(),phases={mount:0,quickness:0};mgr.enterMap(2,s);
  for(let step=0;step<16;step++) {
    const input=[step,phases.mount,phases.quickness,...state(s,rng),mgr.npcsAt(2,0).length,...mgr.npcsAt(2,0).flatMap(actorWire)];
    const r=townTurn(s,(lo,hi)=>rng.next(lo,hi),{consumesTurn:step%4!==0,secondWorldTurn:true,passCommand:step%3===0,npcPhases:phases,afterHousekeeping:()=>{mgr.tickGuards(s,world,doors);mgr.tick(s,world,doors);}});
    rows.push({kind:8,input,expected:[...state(s,rng),phases.mount,phases.quickness,mgr.npcsAt(2,0).length,...mgr.npcsAt(2,0).flatMap(actorWire),r.messages.length,...r.messages.map(m=>messages.indexOf(m)),r.poisoned.length,...r.poisoned,r.poisonTicks.length,...r.poisonTicks,0,0,0,0,r.trace.length*4,...r.trace.flatMap(t=>[sites.indexOf(t.site),t.lo,t.hi,t.value])]});
  }
}
const data:number[]=[];
const records=rows.map(r=>{const start=data.length;data.push(...r.input,...r.expected);return [r.kind,start,r.input.length,r.expected.length];});
for(const name of ['underworldHazard','spawnFired','trollFired','trollPayer','outdoorBurning','trapdoorGuard','trapdoorNone','trapdoorTpkHook','townBurning'])if(!coverage[name])throw new Error(`Uncovered branch: ${name}`);
const json=JSON.stringify({schema:1,sites,messages,coverage,rows})+'\n';
const inc=`// Generated by executing TypeScript. Do not edit.\nstatic const int64_t turn_fixture_data[] = {\n${data.map(n=>String(n)).reduce<string[]>((a,n,i)=>{if(i%100===0)a.push(n);else a[a.length-1]+=', '+n;return a;},[]).join(',\n')}\n};\nstatic const size_t turn_fixture_rows[][4] = {\n${records.map(r=>`{${r.join(',')}}`).join(',\n')}\n};\n`;
for(const [name,text] of [['turns.json',json],['turns.inc',inc]] as const) {
  const path=new URL('../fixtures/'+name,import.meta.url);
  if(process.argv.includes('--check')) {if(readFileSync(path,'utf8')!==text)throw new Error(`Reference drift: ${name}`);}
  else writeFileSync(path,text);
}
console.log(JSON.stringify({cases:rows.length,byKind:records.reduce<Record<number,number>>((a,r)=>{a[r[0]!] = (a[r[0]!]??0)+1;return a;},{})}));
