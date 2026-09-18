import {readFileSync,writeFileSync} from 'node:fs';
import {Game} from '../../../game/src/core/game.js';
import {OriginalRng} from '../../../game/src/core/rng-original.js';
import {transportMode} from '../../../game/src/core/world/transport.js';
const proto=Game.prototype as any, rows:string[]=[];
const dirs=['north','south','east','west'];
class Hash { h=2166136261; n(v:number){for(let i=0;i<4;i++)this.h=Math.imul(this.h^((v>>>(i*8))&255),16777619)>>>0;} s(v:string){this.n(v.length);for(const c of v)this.h=Math.imul(this.h^c.charCodeAt(0),16777619)>>>0;} }
// Actual Game movement and turns. Resource-owned effects are deliberately absent
// in this corpus; their integrated parity is tracked separately.
for(const from of [28,29,18,19,20,21,32,33,34,35,36,37,38,39,40,41,42,43])
for(let terrain=0;terrain<256;terrain++) for(let v=0;v<8;v++) {
 if((v&1)&&terrain===140)continue; // existing quest trapdoor corpus
 const town=!!(v&1), side=town?32:256,seed=from*8191+terrain*31+v;
 const rng=new OriginalRng(seed);
 const state:any={position:{location:town?2:0,floor:0,x:v&2?side-1:10,y:10},time:{year:139,month:1,day:1,hour:5,minute:59},characters:[{status:'G',currentHp:100,maxHp:100,ring:255,partyStatus:0,strength:20,dexterity:20}],partySize:1,activeCharacter:255,food:100,torchTurns:0,turnsSinceStart:0,prevHour:5,lightSpellMins:0,drunkTurns:0,timeSpell:v&4?'Q':'',timeSpellTurns:255,transport:transportMode(from),transportTile:from,shipHull:v&2?1:99,shipSkiffs:v%3,magicCarpets:v%2,worldObjects:[],wind:v%5,windDriftCtr:0,sailDir:0,hmsCapeToggle:0,specialItems:{hmsCape:!!(v&4)}};
 const map:any={kind:town?'small':'overworld',wraps:!town,width:side,height:side,tileAt:(x:number,y:number)=>town&&(x<0||y<0||x>=32||y>=32)?-1:terrain};
 const grid=Array.from({length:side},()=>Array(side).fill(terrain));
 state.gold=v&2?500:0;
 const g:any={state,world:{overworld:grid,underworld:grid,smallMaps:new Map([[2,{id:2,floors:[{z:0,tiles:grid}]}]])},effectiveLocation:town?2:0,liveRng:rng,rand:(lo:number,hi:number)=>rng.next(lo,hi),activeMap:map,
 npcAtTarget:()=>null,overworldActorTileAt:()=>0,tickDoorsAndNpcs:()=>{},tickGuards:()=>{},tickNpcs:()=>{},tickDoors:()=>{},refreshHourTiles:()=>{},checkWaterfallUnder:()=>{},checkWaterfall:()=>{},checkRefuge:()=>[],checkBlackthornCapture:()=>null,checkGuardTribute:()=>null,applyShrineGuardian:()=>{},checkMoongate:()=>{},checkShrineEntry:()=>{},applyStairStep:()=>{},townTrapdoorFall:()=>false,
 mountPhase:0,quicknessPhase:0,townPhasesLoc:-1,townNpcPhases:{mount:0,quickness:0},npcEngineSecondTurn:()=>false};
 for(const name of ['move','moveEcho','targetCoord','runContextTurn','outdoorWorldTurn','outdoorWorldTurnRuns','navalMove','resolveNavalStep','runNavalTurn','headMessage','pushHullWeak','syncTransportFromTile','dirFromDelta','buildTrollSneakScript','resolveTrollToll','spawnTrollCombat','yellSails','checkWaterfall','checkWaterfallUnder','waterfallFalls'])g[name]=(...args:any[])=>proto[name].apply(g,args);
 const hashes:number[]=[];
 for(let a=0;a<4;a++) {
  // No prompt continuation is modeled: stop at a town boundary until answered.
  const events=g.move(dirs[a]);
  if(g.pendingTroll)events.push(...g.resolveTrollToll(!!(v&2)));
  const h=new Hash();for(const n of [rng.getSeed(),state.position.x,state.position.y,state.transportTile,['foot','horse','carpet','skiff','ship'].indexOf(state.transport),state.shipHull,state.shipSkiffs,state.magicCarpets,state.turnsSinceStart,state.time.hour,state.time.minute,state.prevHour,state.food,state.wind,state.windDriftCtr,state.sailDir,state.hmsCapeToggle,g.mountPhase,g.quicknessPhase,g.townNpcPhases.mount,g.townNpcPhases.quickness,state.characters[0].currentHp,state.characters[0].status.charCodeAt(0)])h.n(n);
  h.n(state.gold);
  for(const e of events){h.s(e.kind);h.s(e.text??e.sfx?.id??'');for(const slot of e.poisonTick?.slots??[])h.n(slot);if(e.toll!==undefined)h.n(e.toll);for(const b of e.trollSneak?.beats??[]){h.s(b.message??b.append??'');h.n(b.pauseUnits??-1);h.n(b.append!==undefined?1:0);}}hashes.push(h.h);
 }
 rows.push([from,terrain,v,...hashes].join(' '));
}
const path=new URL('../fixtures/movement-flow.txt',import.meta.url),body=rows.join('\n')+'\n';
if(process.argv.includes('--check')){if(readFileSync(path,'utf8')!==body)throw Error('movement flow drift');}else writeFileSync(path,body);
console.log(`${rows.length} movement sequences (${rows.length*4} snapshots)`);
