/** Synthetic, deterministic calls to the authoritative runtime; never rewrites it. */
import {readFileSync,writeFileSync} from 'node:fs';
import * as E from '../../../game/src/core/equip.js';
import * as C from '../../../game/src/core/world/camp.js';
import {campHoleUp} from '../../../game/src/core/world/commands.js';
import {igniteTorch} from '../../../game/src/core/world/survival.js';
import {advanceTurn} from '../../../game/src/core/world/movement.js';
import {OriginalRng} from '../../../game/src/core/rng-original.js';
import {CombatRng} from '../../../game/src/core/combat/formulas.js';
import {applyPotionEffect,rerollPotionColor} from '../../../game/src/core/usePotion.js';
import {buildReadyRows} from '../../../game/src/core/readyPicker.js';
import {addByteCapped} from '../../../game/src/core/counters.js';
import {instalaDsStrings} from '../../../game/src/core/data/ds-strings.js';
import {Game} from '../../../game/src/core/game.js';
import {usePocketWatch,useSextant,useWoodenBox} from '../../../game/src/core/endgame/use-tools.js';
import type {GameState} from '../../../game/src/core/state.js';
instalaDsStrings({'KARMA.DAT':Array.from({length:6},(_,i)=>`TEST KARMA ${i}`),'MISCMSG.DAT':['TEST'],'ENDMSG.DAT':['TEST']});
const slots=['helmet','armor','weapon','shield','ring','amulet'] as const;
const chars=['G','P','S','D'];
const transports=['foot','horse','carpet','skiff','ship'];
const rows:string[]=[];const coverage:Record<string,number>={};
const str=(s:string)=>[s.length,...Array.from(s).map(x=>x.charCodeAt(0))];
const hit=(s:string)=>coverage[s]=(coverage[s]??0)+1;
// Text zero-run encoding keeps the full state comparison without repeating
// hundreds of absent pack entries in every row. zN expands to N integer zeros.
function packed(v:number[]):string {const out:string[]=[];for(let i=0;i<v.length;){if(v[i]===0){let j=i+1;while(j<v.length&&v[j]===0)j++;out.push(j-i>2?`z${j-i}`:v.slice(i,j).join(' '));i=j;}else out.push(String(v[i++]));}return out.join(' ');}
function state(k:number):GameState {return {
 characters:Array.from({length:6},(_,i)=>({name:`MEM${i}`,class:['A','M','B','F'][i%4],status:chars[(k+i)%4],
 strength:k%2?30:0,dexterity:20,intelligence:21,currentMp:0,currentHp:k%3?10:200,maxHp:200,exp:[0,100,800,6400,9999,65535][i],level:1,
 helmet:255,armor:255,weapon:255,shield:255,ring:i===2?44:255,amulet:255,partyStatus:0,monthsAtInn:0})),
 partySize:6,activeCharacter:0,food:k%2?0:9999,gold:0,torches:k%4,keys:0,gems:0,skullKeys:0,magicCarpets:0,
 equipmentQuantities:Array(256).fill(0),potionQuantities:Array(8).fill(1),scrollQuantities:Array(8).fill(0),reagentQuantities:Array(8).fill(0),
 time:{year:139,month:13,day:28,hour:23,minute:55},position:{location:0,floor:0,x:10,y:10},turnsSinceStart:4294967290,
 torchTurns:10,transport:'foot',transportTile:28,prevHour:23,lightSpellMins:8,drunkTurns:0,timeSpellTurns:2,
 wind:2,windDriftCtr:3,shadowlordLocs:[128,128,128],skullTreeFoundDay:20,reagentPatchFoundDay:[1,2,3],feluccaPhase:49,trammelPhase:50,karma:k%100,
 } as unknown as GameState;}
function encode(s:GameState,rng:OriginalRng){return [s.partySize,s.characters.length,s.food,s.torches,s.torchTurns,s.turnsSinceStart,
 s.time.year,s.time.month,s.time.day,s.time.hour,s.time.minute,s.position.location,s.position.floor,s.position.x,s.position.y,
 transports.indexOf(s.transport),s.transportTile??28,s.prevHour??0,s.lightSpellMins??0,s.drunkTurns??0,s.timeSpell?.charCodeAt(0)??0,s.timeSpellTurns??-1,
 s.wind??0,s.windDriftCtr??0,...s.shadowlordLocs!,s.skullTreeFoundDay??0,...s.reagentPatchFoundDay!,s.feluccaPhase??48,s.trammelPhase??48,s.karma,
 ...s.characters.flatMap(c=>[c.status.charCodeAt(0),c.class.charCodeAt(0),c.strength,c.dexterity,c.intelligence,c.currentMp,c.currentHp,c.maxHp,c.exp,c.level,...slots.map(k=>c[k])]),
 ...s.equipmentQuantities,...s.potionQuantities,s.shipHull??99,rng.getSeed()];}
function run(s:GameState,rng:OriginalRng,op:number,a=0,b=0,c=0){
 const before=encode(s,rng),draws:number[]=[],calls:number[]=[];let events:any[]=[],extra:number[]=[];
 const next=rng.next.bind(rng);rng.next=(lo,hi)=>{const v=next(lo,hi);draws.push(lo,hi,v);return v;};
 const rand=(lo:number,hi:number)=>rng.next(lo,hi);let snaps=0;
 const ctx:C.CampCtx={state:s,rand,dungeonState:null,enemyDefs:undefined,mapTileWithOverrides:()=>a,
  snapNpcsToSchedule:()=>{snaps++;calls.push(1,s.time.hour,s.time.minute);},objectOrNpcAt:(x,y,z)=>{calls.push(2,x,y,z);return c>0&&snaps>=c;},
  campCellFree:(x,y)=>{calls.push(3,x,y);return (x+y)%3!==0;},campGuardStartCell:()=>({col:5,row:6}),
  startCombat:(enemy)=>{extra.push(enemy.defIndex);return [];},runContextTurn:()=>{throw Error('unexpected context turn');}};
 let r:{ok:boolean;message:string;vanished?:boolean;removed?:boolean}={ok:true,message:''};
 if(op===0)r=Game.prototype.readyItem.call({state:s,rand} as any,a,b,false);
 if(op===1)r=E.equipItem(s,a,b,undefined,{randRange:rand,inDungeonCombat:!!c});
 if(op===2)r=E.unequipSlot(s,a,slots[b]!);
 if(op===3)extra=[slots.indexOf(E.unequipItemById(s,a,b) as any)];
 if(op===4)extra=[slots.indexOf(E.unequipWeaponById(s,a,b) as any)];
 if(op===5)extra=E.rollRingExpiry(s,rand).flatMap(e=>[e.charIdx,e.ringId]);
 if(op===6){const message=igniteTorch(s,rand,a);r={ok:message===null,message:message??''};}
 if(op===7)extra=[+campHoleUp(s.characters.slice(0,s.partySize),rand,a).apparition];
 if(op===8)events=C.campWake(ctx,a);
 if(op===9)events=C.camp(ctx,a,b);
 if(op===10)events=C.bedSleep(ctx,a);
 if(op===11){const e=C.campContext({...ctx,dungeonState:c?{} as any:null});extra=[+e.ok,+(e.ok&&e.ship),+(e.ok&&!!e.bed),+(!e.ok&&!!e.inTown),...str(e.ok?'':e.message)];}
 if(op===12){const cell=C.campGuardWalk({col:a,row:b},rand,ctx.campCellFree!);extra=[cell.col,cell.row];}
 if(op===13){const crng=new CombatRng(rng);const col=c?rerollPotionColor(b,crng):b;const e=applyPotionEffect(s.characters[a]!,col,crng,s.position.location);r=e;extra=[col,+(col===7&&e.ok)];}
 if(op===14){if((s.potionQuantities[a]??0)>0){s.potionQuantities[a]!--;extra=[1];}else extra=[0];}
 if(op===15)extra=[addByteCapped(a,b,c)];
 if(op===16){events=advanceTurn(s,a,rand).map(text=>({kind:'message',text}));}
 if(op===17){const ch=s.characters[a]!;const selected=buildReadyRows({qtyOf:id=>s.equipmentQuantities[id]??0,isEquipped:id=>E.isItemEquipped(ch,id),nameOf:id=>String(id)});
  extra=[E.handState(ch),E.totalEquippedWeight(ch),E.equipTypeOf(b),slots.indexOf(E.slotForEquip(b) as any),E.ammoItemFor(b)??-1,+E.isThrownWeapon(b),selected.length,...selected.map(r=>r.equipId),C.campWatchCount(ctx)];}
 if(op===18){
  const proto=Game.prototype as any;let worlds=0;const old=s.turnsSinceStart;const had=s.torches;
  const g:any={state:s,rand,effectiveLocation:s.position.location,activeMap:{tileAt:()=>b},world:{smallMaps:new Map([[2,{}]])},mountPhase:0,quicknessPhase:0,
   townPhasesLoc:-1,townNpcPhases:{mount:0,quickness:0},drunkPreRolled:false,
   checkWaterfallUnder:()=>{},tickDoorsAndNpcs:()=>{},checkRefuge:()=>[],checkWaterfall:()=>{},tickDoors:()=>{},
   tickNpcs:()=>{},tickGuards:()=>{worlds++;},checkBlackthornCapture:()=>null,checkGuardTribute:()=>null,refreshHourTiles:()=>{},
  };
  for(const name of ['runContextTurn','outdoorWorldTurn','npcEngineSecondTurn'])g[name]=(...args:any[])=>proto[name].apply(g,args);
  g.outdoorWorldTurnRuns=()=>{const yes=proto.outdoorWorldTurnRuns.call(g);if(yes)worlds++;return yes;};
  events=a===0?proto.ignite.call(g):a===1?C.campReject({...ctx,runContextTurn:g.runContextTurn},'Hole up- Only in bed!\n'):a===2?[]:proto.pass.call(g);
  extra=[a===2?2:a===1||a===0&&had===0?1:0,s.turnsSinceStart-old,worlds,events.length];
 }
 if(op===19){const ctx={state:s} as any;events=a===35?usePocketWatch(ctx):a===34?useSextant(ctx):useWoodenBox();}
 if(op===20)events=C.campRepairShip(ctx);
 if(op===22)events=a>0?C.bedSleep(ctx,a):[];
 rng.next=next;
 const ev=events.flatMap(e=>[e.kind==='message'?0:e.kind==='party-changed'?3:e.kind==='sfx'?6:-1,...str(e.text??e.sfx?.id??'')]);
 if(events.some(e=>!['message','party-changed','sfx'].includes(e.kind)))throw Error('event mapping');
 hit(`op:${op}`);hit(`result:${r.ok}`);if(r.message)hit(`message:${r.message}`);if(extra.length&&op===9)hit('ambush');
 if(op===5&&extra.length)hit('ring-expiry');
 if(events.some(e=>e.text==='Thrown out of bed!\n'))hit('bed-interrupted');
 if(r.removed)hit('toggle-off');
 if(events.some(e=>e.text==='An apparition!\n'))hit('apparition');
 const after=encode(s,rng);
 const out=[+r.ok,+!!r.vanished,+!!r.removed,...str(r.message),extra.length,...extra,after.length,...after,ev.length,...ev,calls.length,...calls,draws.length,...draws];
 const row=[op,a,b,c,before.length,...before,out.length,...out];
 if(row.some(v=>!Number.isSafeInteger(v)))throw Error(`Noninteger fixture op ${op}`);
 rows.push(`${op} ${a} ${b} ${c} ${before.length} ${packed(before)} ${out.length} ${packed(out)}`);
}
for(let seed=0;seed<32;seed++)for(let id=-1;id<=49;id++)for(let variant=0;variant<8;variant++){
 const s=state(seed),rng=new OriginalRng(seed*2053+id);const member=variant===7?17:0;
 if(id>=0)s.equipmentQuantities[id]=[0,1,99,100,255,32767,1,1][variant]!;
 s.equipmentQuantities[27]=variant%2;s.equipmentQuantities[29]=variant%2;
 if(variant===2)s.characters[0]!.weapon=16;
 if(variant===3)s.characters[0]!.shield=4;
 if(variant===4){s.characters[0]!.helmet=0;s.characters[0]!.armor=9;s.characters[0]!.ring=43;s.characters[0]!.amulet=45;}
 if(variant===5&&id>=0) s.characters[0]!.weapon=id;
 run(s,rng,1,member,id,variant===6?1:0);
}
for(let seed=0;seed<128;seed++){
 const s=state(seed),rng=new OriginalRng(seed*509);
 run(s,rng,0,0,255); // sentinel toggle is a real TS quirk
 for(let id=0;id<48;id++){s.equipmentQuantities[id]=1;run(s,rng,0,0,id);run(s,rng,17,0,id);}
 for(let slot=0;slot<6;slot++){run(s,rng,2,0,slot);run(s,rng,2,0,slot);}
 s.characters[0]!.helmet=42;s.characters[0]!.ring=42;run(s,rng,3,0,42);run(s,rng,4,0,42);
 run(s,rng,5);run(s,rng,2,17,0);run(s,rng,3,-1,16);
 for(const loc of [0,32,33,40,41])run(s,rng,6,loc);
 for(let color=-1;color<9;color++) {run(s,rng,13,seed%6,color,seed%2);if(color>=0&&color<8){run(s,rng,14,color);run(s,rng,14,color);}}
 for(let repeat=0;repeat<3;repeat++){run(s,rng,9,1+seed%9,seed%7-1);run(s,rng,16,2);}
 run(s,rng,7,seed%7-1);run(s,rng,8,seed%7-1);
 s.position.location=2;run(s,rng,10,1+seed%9,0,seed%4);run(s,rng,10,0);
 for(let t=0;t<5;t++){s.transport=transports[t] as any;for(const tile of [1,3,4,171,172]){s.position.location=seed%3?0:2;run(s,rng,11,tile,0,seed%2);}}
 run(s,rng,12,seed%11,seed%11);
 for(const cap of [30,99,9999])run(s,rng,15,seed*2,1,cap);
 const q=state(seed);for(const ch of q.characters)ch.status='G';
 run(q,rng,18,0,5);run(q,rng,18,3,5);q.position.location=2;
 run(q,rng,18,0,68);run(q,rng,18,1,68);run(q,rng,18,2,68);
 q.time.hour=seed%24;q.time.minute=seed%60;q.position.location=seed%2?0:2;q.position.floor=seed%3===0?-1:seed%3===1?255:0;
 for(const item of [34,35,37])run(q,rng,19,item);
 q.shipHull=seed;q.position.location=0;q.transport='ship';q.transportTile=36;run(q,rng,20);
 q.position.location=2;q.position.floor=0;q.transport='foot';q.transportTile=28;run(q,rng,22,1+seed%9,0,seed%4);run(q,rng,22,0);
 const share=state(seed);share.characters[0]!.strength=30;share.characters[1]!.strength=30;share.equipmentQuantities[16]=1;
 run(share,rng,0,0,16);run(share,rng,0,1,16);run(share,rng,0,0,16);run(share,rng,0,1,16);
}
for(const n of [0,1,6])for(let seed=0;seed<16;seed++){
 const s=state(seed);s.characters=s.characters.slice(0,n);s.partySize=n;const rng=new OriginalRng(seed*991);
 run(s,rng,7,-1);run(s,rng,8,-1);run(s,rng,9,1,-1);run(s,rng,10,1);
}
for(let tile=32;tile<40;tile++){
 const s=state(tile);s.transport='ship';s.transportTile=tile;run(s,new OriginalRng(tile),11,5);
}
for(const key of ['ambush','apparition','ring-expiry','bed-interrupted','toggle-off','message:\n\nRing vanishes!\n'])if(!coverage[key])throw Error(`Uncovered branch: ${key}`);
const body=rows.join('\n')+'\n';const url=new URL('../fixtures/items.txt',import.meta.url);
const coverageUrl=new URL('../fixtures/items-coverage.json',import.meta.url),coverageBody=JSON.stringify(coverage,null,2)+'\n';
if(process.argv.includes('--check')){if(readFileSync(url,'utf8')!==body||readFileSync(coverageUrl,'utf8')!==coverageBody)throw Error('Item fixture drift');}
else {writeFileSync(url,body);writeFileSync(coverageUrl,coverageBody);}
console.log(`${rows.length} inventory/equipment/item/rest parity cases`);
