/** Executes real Game methods and real turn/NPC code; only excluded services are instrumented. */
import { readFileSync, writeFileSync } from 'node:fs';
import { Game } from '../../../game/src/core/game.js';
import type { GameState } from '../../../game/src/core/state.js';
import { OriginalRng } from '../../../game/src/core/rng-original.js';
import { getActiveMap, type WorldData } from '../../../game/src/core/world/map.js';
import { NpcManager, type NpcSlot, type NpcRuntime } from '../../../game/src/core/npc/manager.js';
import { DoorManager } from '../../../game/src/core/world/doors.js';
const rows: string[] = [];
const coverage: Record<string,number> = {};
const hit=(name:string)=>{coverage[name]=(coverage[name]??0)+1;};
const proto = Game.prototype as any;
const dirs = ['north','south','east','west'] as const;
const kinds = ['message','moved','map-changed','party-changed','town-exit-prompt','walk-echo','sfx','poison-tick','quake','needs-direction'];
const text = (s: string) => [s.length,...Array.from(s).map(c=>c.charCodeAt(0))];
function actor(n: NpcRuntime) { return [n.slot,n.type,n.dialogNumber,...n.aiTypes,...n.schedX,...n.schedY,...n.schedZ,...n.times,n.x,n.y,n.z,n.location,n.state,n.servedSlot,n.pathIdx,n.stuck,...n.pathBuf]; }
const slots: NpcSlot[] = [
 {slot:1,type:64,dialogNumber:0,aiTypes:[0,1,0],x:[11,18,8],y:[10,18,8],z:[0,1,0],times:[0,6,12,18]},
 {slot:2,type:16,dialogNumber:0,aiTypes:[0,0,0],x:[8,8,8],y:[8,8,8],z:[0,0,0],times:[0,6,12,18]},
];
for (let scenario=0;scenario<36;scenario++) for (let seed=0;seed<32;seed++) {
 const local = (scenario>=8 && scenario<=18) || scenario===21 || (scenario>=25 && scenario<=27) || scenario>=29;
 const terrain = [5,48,4,9,47,4,143,5,68,48,68,196,200,201,134,202,68,68,68,20,26,68,5,20,20,68,68,68,5,68,188,68,68,68,68,68][scenario]!;
 const large = Array.from({length:256},()=>Array(256).fill(terrain) as number[]);
 const small = Array.from({length:32},()=>Array(32).fill(local?terrain:68) as number[]);
 // Keep the party's current tile free for attempted steps, except explicit hazards.
 if (scenario===1 || scenario===4) large[10]![10]=5;
 if (scenario===9 || scenario===15) small[10]![10]=68;
 const floors = [-1,0,1].map(z=>({z,tiles:small}));
 const world: WorldData = {overworld:large,underworld:large,smallMaps:new Map([[2,{id:2,name:'TEST',floors}]])};
 const s = {
  position:{location:local?2:0,floor:!local && seed%4===0?255:0,x:scenario===7?255:scenario===10?31:10,y:10},
  time:{year:139,month:13,day:28,hour:0,minute:59},
  characters:Array.from({length:3},(_,i)=>({status:scenario===25||scenario===33?'S':scenario===27?'D':i===1?'P':'G',currentHp:100,maxHp:150,ring:i===2?44:0,strength:20,dexterity:10+i*10,monthsAtInn:23,partyStatus:0})),
  partySize:3,activeCharacter:1,food:seed%2?0:20,torchTurns:10,turnsSinceStart:4294967290,
  prevHour:0,lightSpellMins:8,drunkTurns:scenario===29||scenario>=32?5:0,timeSpell:([undefined,'Q','T'] as const)[seed%3],timeSpellTurns:seed%4?2:255,
  transport:'foot',transportTile:28,wind:2,windDriftCtr:3,shadowlordLocs:[128,128,128],skullTreeFoundDay:20,reagentPatchFoundDay:[1,2,3],feluccaPhase:49,trammelPhase:50,npcDead:[],
 } as unknown as GameState;
 const rng = new OriginalRng(seed*2053+scenario), manager = new NpcManager({2:slots}), doors = new DoorManager();
 manager.setRng(rng); manager.enterMap(2,s);
 s.time.hour=[23,5,19,11][seed%4]!;
 let effects:number[]=[], draws:number[]=[], worlds=0, pending=false;
 const pos=()=>[s.position.location,s.position.floor,s.position.x,s.position.y];
 const fx=(n:number)=>{effects.push(n,...pos(),rng.getSeed());};
 const g:any={state:s,world,data:{locationsX:[40,scenario===23?41:10],locationsY:[80,10]},
  rand:(lo:number,hi:number)=>{const v=rng.next(lo,hi);draws.push(lo,hi,v);return v;},
  npcManager:manager,doors,combatResources:undefined,drunkPreRolled:false,drunkStaggerInFlight:false,
  townPhasesLoc:-1,townNpcPhases:{mount:0,quickness:0},mountPhase:0,quicknessPhase:0,
  volatileTerrainWipe:undefined,
  refreshHourTiles:()=>fx(1),tickDoors:()=>fx(0),
  checkRefuge:()=>{fx(2);return [];},checkBlackthornCapture:()=>{fx(3);return null;},checkGuardTribute:()=>{fx(4);return null;},
  checkWaterfallUnder:()=>fx(5),checkWaterfall:()=>fx(6),applyShrineGuardian:()=>fx(7),checkMoongate:()=>fx(8),checkShrineEntry:()=>fx(9),
  overworldActorTileAt:()=>0,
  tickGuards:()=>{worlds++;manager.tickGuards(s,world,doors);},tickNpcs:()=>manager.tick(s,world,doors),
  clearVolatileTerrain:()=>fx(23),hydrateInteriorObjects:()=>fx(24),applyUrbanShadowlord:()=>fx(27),discardInteriorObjects:()=>fx(28),
  overworldEnemies:{clear:()=>fx(21)},locationNameBanner:(_id:number,e:any[])=>e.push({kind:'message',text:'\n\nTEST\n'}),
 };
 // Observe reload operations while retaining actual actor entry mutation.
 const enter=manager.enterMap.bind(manager);manager.enterMap=(id:number,state:GameState)=>{fx(20);enter(id,state);};
 doors.reset=()=>fx(22);
 for (const name of ['move','moveEcho','targetCoord','npcAtTarget','runContextTurn','applyStairStep','floorExists','klimbLadder','klimbTown','klimb','klimbCancel','pass','enter','confirmTownExit','exitToOverworld','loadSmallMap','npcEngineSecondTurn','townAutoSleepTurn']) g[name]=(...args:any[])=>proto[name].apply(g,args);
 g.commandDrunkIntercept=()=>proto.commandDrunkIntercept.call(g);
 g.tickDoorsAndNpcs=()=>{g.tickDoors();g.tickNpcs();};
 g.outdoorWorldTurnRuns=()=>{const yes=proto.outdoorWorldTurnRuns.call(g);if(yes)worlds++;return yes;};
 g.outdoorWorldTurn=(t:number)=>proto.outdoorWorldTurn.call(g,t);
 Object.defineProperties(g,{activeMap:{get:()=>getActiveMap(world,s.position.location,s.position.floor)},effectiveLocation:{get:()=>s.position.location},skyRefreshCtx:{get:()=>undefined}});
 if(scenario===24){g.data.locationsX=[10,41];g.data.locationsY=[10,10];} // matching location 1 has no loaded small map
 // Capture every RNG draw, including manager calls through its OriginalRng owner.
 const next=rng.next.bind(rng);rng.next=(lo:number,hi:number)=>{const v=next(lo,hi);draws.push(lo,hi,v);return v;};
 g.rand=(lo:number,hi:number)=>rng.next(lo,hi);
 const encode=()=>{
  const t=s.time,p=s.position;
  const actors=((manager as any).npcs.get(2)??[]) as NpcRuntime[];
  return [t.year,t.month,t.day,t.hour,t.minute,...pos(),s.partySize,s.activeCharacter,s.food,s.torchTurns,s.turnsSinceStart,
   s.prevHour??0,s.lightSpellMins??0,s.drunkTurns??0,s.timeSpell?.charCodeAt(0)??0,s.timeSpellTurns??-1,s.transportTile??28,s.wind??0,s.windDriftCtr??0,
   ...(s.shadowlordLocs??[]),s.skullTreeFoundDay??0,...(s.reagentPatchFoundDay??[]),s.feluccaPhase??48,s.trammelPhase??48,
   ...s.characters.flatMap(c=>[c.status.charCodeAt(0),c.currentHp,c.maxHp,c.ring,c.strength,c.dexterity,c.monthsAtInn]),rng.getSeed(),
   g.townPhasesLoc,g.townNpcPhases.mount,g.townNpcPhases.quickness,g.mountPhase,g.quicknessPhase,+g.drunkPreRolled,+pending,s.shadowlordHere??-1,
   actors.length,...actors.flatMap(actor)];
 };
 for(let step=0;step<12;step++) {
  let cmd=1,dir=seed%4,has=0;
  if(scenario<=11 || scenario===29) {cmd=step%4===3?1:0;if(scenario===10)dir=2;}
  else if(scenario<=17){cmd=5;has=+(scenario===15||scenario===16||scenario===17&&step%2===1);}
  else if(scenario===18)cmd=6;
  else if(scenario<=24)cmd=2;
  else if(scenario===25||scenario===26)cmd=7;
  else if(scenario===31)cmd=step%2?6:1;
  else if(scenario===33)cmd=0;
  else if(scenario===34)cmd=2;
  else if(scenario===35){cmd=5;has=1;}
  if(pending)cmd=(step+seed)%3?3:4;
  const before=encode(),oldPos=pos().join(','),oldTurns=s.turnsSinceStart;
  effects=[];draws=[];worlds=0;
  const primary=cmd===0||cmd===1||cmd===2||(cmd===5&&!has);
  const intercepted:any[]|null=scenario>=32&&primary&&cmd!==0?g.commandDrunkIntercept():null;
  const sleeping:any[]|null=scenario>=32&&primary&&!intercepted?g.townAutoSleepTurn():null;
  const events:any[]=intercepted??sleeping??(cmd===0?g.move(dirs[dir]):cmd===1?g.pass():cmd===2?g.enter():cmd===3?g.confirmTownExit(true):cmd===4?g.confirmTownExit(false):cmd===5?g.klimb(has?dirs[dir]:undefined):cmd===6?g.klimbCancel():g.townAutoSleepTurn()??[]);
  let status=0;
  if(events.some(e=>e.kind==='town-exit-prompt')){pending=true;status=3;}
  else if(cmd===3||cmd===4)pending=false;
  if(events.some(e=>e.kind==='needs-direction'))status=3;
  if(events.some(e=>['Blocked!','Klimb-What?','Klimb--On foot!','Enter What?','Enter what?'].includes(e.text)))status=1;
  if(cmd===2 && !intercepted && !sleeping && status===0 && oldPos===pos().join(',') && oldTurns===s.turnsSinceStart)status=2;
  if((cmd===6 && s.position.location===0)||(cmd===7 && events.length===0))status=2;
  const ev=events.flatMap(e=>[kinds.indexOf(e.kind),...text(e.text??e.sfx?.id??e.command??''),(e.poisonTick?.slots??[]).length,...(e.poisonTick?.slots??[])]);
  if(events.some(e=>!kinds.includes(e.kind)))throw new Error('Unmapped event');
  const after=encode();
  hit(`status:${status}`);hit(s.turnsSinceStart===oldTurns?'no-turn':'turn');
  hit(`command:${cmd}`);
  if(worlds)hit('world-tail');if(intercepted)hit('intercepted');if(sleeping)hit('sleep-dispatch');
  for(const e of events){hit(`event:${e.kind}`);if(e.kind==='message')hit(`message:${e.text}`);}
  const out=[status,s.turnsSinceStart-oldTurns,worlds,events.length,after.length,...after,ev.length,...ev,effects.length,...effects,draws.length,...draws];
  rows.push([scenario,seed,step,cmd,dir,has,before.length,...before,out.length,...out].join(' '));
 }
}
const path=new URL('../fixtures/commands.txt',import.meta.url),body=rows.join('\n')+'\n';
if(process.argv.includes('--check')){if(readFileSync(path,'utf8')!==body)throw new Error('Command fixture drift');}
else writeFileSync(path,body);
console.log(`${rows.length} command orchestration parity cases`);
for(const name of ['command:4','message:Very slow!','message:Slow progress!','message:EARTHQUAKE!\n','message:OUCH!','intercepted','sleep-dispatch','status:0','status:1','status:2','status:3'])if(!coverage[name])throw new Error(`Missing coverage ${name}`);
const reportPath=new URL('../fixtures/commands-coverage.json',import.meta.url),report=JSON.stringify({cases:rows.length,coverage},null,2)+'\n';
if(process.argv.includes('--check')){if(readFileSync(reportPath,'utf8')!==report)throw new Error('Command coverage drift');}
else writeFileSync(reportPath,report);
