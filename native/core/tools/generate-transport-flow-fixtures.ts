import {readFileSync,writeFileSync} from 'node:fs';
import {Game} from '../../../game/src/core/game.js';
import {OriginalRng} from '../../../game/src/core/rng-original.js';
import {transportMode} from '../../../game/src/core/world/transport.js';
const proto=Game.prototype as any,rows:string[]=[];
const tiles=[5,0x110,0x11b,0x124,0x128,0x24];
class Hash{h=2166136261;n(v:number){for(let i=0;i<4;i++)this.h=Math.imul(this.h^((v>>>(i*8))&255),16777619)>>>0;}s(s:string){this.n(s.length);for(const c of s)this.h=Math.imul(this.h^c.charCodeAt(0),16777619)>>>0;}}
for(let k=0;k<6;k++)for(let from=16;from<48;from++)for(let v=0;v<8;v++){
 const rng=new OriginalRng(k*901+from*31+v),base=v&1?106:5,near=v&2?1:5;
 let overlay=tiles[k]!,objects:any[]=k===3&&v&4?[{location:0,floor:0,x:20,y:20,tile:0x124,kind:'ship',hull:7,skiffs:2}]:[];
 const state:any={position:{location:0,floor:0,x:20,y:20},time:{year:139,month:1,day:1,hour:5,minute:59},characters:[{status:'G',currentHp:100,maxHp:100,ring:255,partyStatus:0}],partySize:1,activeCharacter:255,food:100,torchTurns:0,turnsSinceStart:0,prevHour:5,lightSpellMins:0,drunkTurns:0,timeSpell:v&4?'Q':'',timeSpellTurns:255,transport:transportMode(from),transportTile:from,shipHull:99,shipSkiffs:v%3,magicCarpets:v%2,worldObjects:objects,wind:2,windDriftCtr:0};
 const tileAt=(x:number,y:number)=>x===20&&y===20?(objects[0]?.tile??(overlay<0?base:overlay)):near;
 const g:any={state,effectiveLocation:0,liveRng:rng,rand:(lo:number,hi:number)=>rng.next(lo,hi),activeMap:{tileAt},worldObjectAt:()=>objects[0],removeWorldObject:(o:any)=>{objects.splice(objects.indexOf(o),1);},clearBoardedVehicleCell:()=>overlay=-1,setMapOverride:(_:number,__:number,tile:number)=>overlay=tile,tickDoorsAndNpcs:()=>{},checkWaterfallUnder:()=>{},checkWaterfall:()=>{},checkRefuge:()=>[],mountPhase:0,quicknessPhase:0};
 for(const name of ['runContextTurn','outdoorWorldTurn','outdoorWorldTurnRuns','orthogonalLandNearby','syncTransportFromTile'])g[name]=(...args:any[])=>proto[name].apply(g,args);
 const hashes:number[]=[];for(let a=0;a<4;a++){
  const ev=proto[a%2?'exitVehicle':'board'].call(g);objects=state.worldObjects;
  const h=new Hash();for(const n of [rng.getSeed(),state.transportTile,['foot','horse','carpet','skiff','ship'].indexOf(state.transport),state.shipHull,state.shipSkiffs,state.magicCarpets,state.turnsSinceStart,state.time.hour,state.time.minute,state.prevHour,state.food,state.wind,state.windDriftCtr,g.mountPhase,g.quicknessPhase,overlay,objects.length])h.n(n);
  for(const o of objects)for(const n of [o.tile,o.hull,o.skiffs])h.n(n);
  for(const e of ev){h.s(e.kind);h.s(e.text??e.sfx?.id??'');}hashes.push(h.h);
 }
 rows.push([k,from,v,...hashes].join(' '));
}
const path=new URL('../fixtures/transport-flow.txt',import.meta.url),text=rows.join('\n')+'\n';if(process.argv.includes('--check')){if(readFileSync(path,'utf8')!==text)throw Error('transport flow drift');}else writeFileSync(path,text);console.log(`${rows.length*4} actual Game.board/exitVehicle/context-turn snapshots`);
