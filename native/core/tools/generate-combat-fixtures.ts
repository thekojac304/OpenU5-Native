/** Execute the unchanged TS engine; snapshots include private deterministic state. */
import {readFileSync,writeFileSync} from 'node:fs';
import {Combat,type CombatMapData} from '../../../game/src/core/combat/combat.js';
import {decodeAbilities} from '../../../game/src/core/combat/enemies.js';
import {characterWeapons} from '../../../game/src/core/equip.js';
import {TILE_INFO} from '../../../game/src/core/tiles.js';
import {blocksSpellLine} from '../../../game/src/core/magic/areaSpellTables.js';
import {combatMapForTile} from '../../../game/src/core/combat/encounters.js';
import {OriginalRng} from '../../../game/src/core/rng-original.js';
import {Game} from '../../../game/src/core/game.js';
const root=new URL('../',import.meta.url),check=process.argv.includes('--check');
function output(path:string,text:string){let url=new URL(path,root);if(check){if(readFileSync(url,'utf8')!==text)throw Error(`drift ${path}`);}else writeFileSync(url,text);}
const flags=Array.from({length:TILE_INFO.length},(_,i)=>{const t=TILE_INFO[i];return t?(+t.walkable|(+t.landEnemyPassable<<1)|(+t.rangeWeaponPassable<<2)):255;});
const los=Array.from({length:32},(_,i)=>Array.from({length:8},(_,b)=>blocksSpellLine(i*8+b)?0:128>>b).reduce((a,b)=>a|b,0));
output('src/combat_tiles.inc',`// Generated from final TypeScript TILE_INFO / blocksSpellLine / combatMapForTile.\nstatic constexpr uint8_t kCombatTileFlags[] = {${flags.join(',')}};\nstatic constexpr uint8_t kCombatLos[] = {${los.join(',')}};\nstatic constexpr int8_t kCombatMapIndex[] = {${Array.from({length:TILE_INFO.length},(_,i)=>combatMapForTile(i)).join(',')}};\n`);
const rows:string[]=[];const coverage:Record<string,number>={};let cases=0;
const hit=(key:string)=>coverage[key]=(coverage[key]??0)+1;
const str=(s:string|undefined)=>s===undefined?[-1]:[s.length,...Array.from(s).map(c=>c.charCodeAt(0))];
let previous:number[]=[];
function packed(v:number[]){let out:string[]=[];for(let i=0;i<v.length;){let j=i;while(j<v.length&&v[j]===previous[j])j++;if(j-i>2){out.push(`p${j-i}`);i=j;continue;}j=i+1;while(j<v.length&&v[j]===v[i])j++;out.push(j-i>2?`r${j-i}:${v[i]}`:String(v[i]));i=j-i>2?j:i+1;}previous=v;return out.join(' ');}
function snap(c:any,s:any,events:any[],draws:number[]){const statuses=['active','dead','fled','sleeping','charmed','absorbed'];const kinds=['message','echo','moved','attacked','died','turn','ended','projectile'];
 const actors=c.combatants.flatMap((a:any)=>[a.id,a.charIdx??255,a.enemyDef?.index??-1,a.x,a.y,a.hp,a.maxHp,a.str,a.dex,a.int,a.defense,a.attack,a.attackRange,statuses.indexOf(a.status),+a.isFleeing,a.speed,a.counter,+a.sleeping,+a.draggedUnder,+a.charmed,+a.invisible,a.renderTile??-1,a.lastAttacker??-1,a.lastTargetId??-1,a.weapons?.length??0,...(a.weapons??[]).flatMap((w:any)=>[w.id,w.attack,w.range])]);
 const loot=Array(121).fill(-1),chests=Array(121).fill(-1);for(const [key,v] of c.lootLayer){const [x,y]=key.split(':').map(Number);loot[y*11+x]=v;}for(const [key,v] of c.chestContents){const [x,y]=key.split(':').map(Number);chests[y*11+x]=v;}
 return [c.finalSeed,c.actionCount,c.scanIdx,c.currentActor?.id??-1,+c.ended,+c.victory,+c.over,c.spoilChests,['east','west','south','north'].indexOf(c.escapeBorder),c.combatants.length,...actors,c.activeWeaponQueue===null?-1:c.activeWeaponQueue.length,...(c.activeWeaponQueue??[]).flatMap((w:any)=>[w.id,w.attack,w.range]),s.activeCharacter,s.food,...s.characters.flatMap((a:any)=>[a.currentHp,a.status.charCodeAt(0),a.exp,a.helmet,a.armor,a.weapon,a.shield,a.ring,a.amulet]),...s.equipmentQuantities,...Array.from({length:16},(_,i)=>c.xpByChar.get(i)??0),...c.mapTiles.flat(),...loot,...chests,c.triggers.length,...c.triggers.flatMap((t:any)=>[t.sprite,t.at.x,t.at.y,t.pos1.x,t.pos1.y,t.pos2.x,t.pos2.y]),events.length,...events.flatMap(e=>[kinds.indexOf(e.kind),e.actorId??-1,e.targetId??-1,e.x??-1,e.y??-1,e.damage??-1,e.hit===undefined?-1:+e.hit,e.grazed===undefined?-1:+e.grazed,e.dragged===undefined?-1:+e.dragged,...str(e.text)]),draws.length,...draws];
}
const weaponIds=[255,18,25,26,28,36,17,19,34,35,38,39,40];
for(let k=0;k<78;k++)for(let si=0;si<32;si++){
 const seed=(si*2017+k*17)&65535,weapon=weaponIds[k%13]!;
 const attack=Array(256).fill(0),range=Array(256).fill(0),defense=Array(256).fill(0),strength=Array(256).fill(0);
 attack[weapon]=[1,20,99][k%3];range[weapon]=k%13<2?1:[2,4,15][k%3];defense[10]=k%4*10;strength[17]=8;
 const s:any={characters:Array.from({length:2},(_,i)=>({name:`MEM${i}`,class:'A',status:i===1&&k%7===0?'D':i===0&&k%11===0?'S':'G',currentHp:[1,10,100][k%3],maxHp:100,strength:20,dexterity:i===0?30:15,intelligence:20,currentMp:0,exp:9990,level:1,helmet:255,armor:10,weapon,shield:k%6===0?weapon:255,ring:k%17===0?44:k%19===0?42:255,amulet:255,partyStatus:0})),partySize:2,activeCharacter:k%5===0?0:255,equipmentQuantities:Array(256).fill(0),food:100,position:{location:0,floor:0,x:5,y:5},transportTile:k%23===0?32:28};
 s.equipmentQuantities[16]=s.equipmentQuantities[27]=k%3;s.equipmentQuantities[weapon]=k%2;
 s.characters[1].dexterity=[0,15,36,255][k%4];if(k%29===0)s.characters[0].currentHp=0;
 const map:CombatMapData={index:0,territory:'test',name:null,tiles:Array.from({length:11},()=>Array(11).fill(5)),playerStarts:{east:[],west:[],south:[{x:5,y:5},{x:4,y:5}],north:[]},units:[{sprite:0,x:6,y:5},{sprite:0,x:8,y:5},{sprite:0,x:5,y:7}],triggers:[]};
 if(k%4===0)map.tiles[5]![7]=12;if(k%9===0)map.tiles[4]![5]=4;
 map.tiles[5]![8]=[5,1,7,4][k%4]!;if(k%31===0)map.tiles[5]![5]=143;
 if(k%5===0)map.triggers=[{sprite:12,at:{x:5,y:4},pos1:{x:9,y:9},pos2:{x:10,y:9}}];
 const masks=[0,0x2000,0x800,0x100,0x8000,0x200,2,0x400,0x10];
 const def:any={index:k%10===0?30:k%14===0?45:20,name:'Rat',groupName:'RATS',str:15,dex:10,int:15,armour:k%4*8,damage:[1,20,99][k%3],hp:[1,25,100][k%3],maxPerMap:3,treasure:k%3*15,attackRange:k%2?4:1,moveClass:0,abilities:decodeAbilities(masks[k%masks.length]!),doesNotMove:false};
 def.moveClass=[0,1,2,4,7,8,9,10,255][k%9];
 if(k%13===12){for(let i=2;i<6;i++)s.characters.push({...s.characters[1],name:`MEM${i}`});s.partySize=6;map.playerStarts.south=Array.from({length:6},(_,i)=>({x:5-i,y:5}));map.units=Array.from({length:16},(_,i)=>({sprite:0,x:i%11,y:Math.floor(i/11)}));}
 def.maxPerMap=[1,3,8,16,30][k%5];if(k%4===0)def.index=12;s.position.location=k%3===0?1:0;
 const initial=structuredClone(s),mountDraws:number[]=[];const originalNext=OriginalRng.prototype.next;
 OriginalRng.prototype.next=function(lo,hi){const v=originalNext.call(this,lo,hi);mountDraws.push(lo,hi,v);return v;};
 const c:any=new Combat({map,entryDirection:'south',party:s.characters.map((record:any,charIdx:number)=>({record,charIdx,weapons:characterWeapons(record,attack,range)})),enemies:[{def,count:k%8===0?0:k%13===12?16:3}],seed,state:s,defenseValues:defense,spellAttackRange:strength,roomCombat:k%5===0});
 OriginalRng.prototype.next=originalNext;
 rows.push(packed([k,seed,-1,0,0,...snap(c,s,[],mountDraws)]));cases++;
 const draws:number[]=[];const next=c.rng.rng.next.bind(c.rng.rng);c.rng.rng.next=(lo:number,hi:number)=>{const v=next(lo,hi);draws.push(lo,hi,v);return v;};
 for(let step=0;step<18;step++){
  draws.length=0;let op=2,x=0,y=0;const cur=c.currentUnit;
  // Peek itself is a tested operation: scanning can mutate status/RNG.
  rows.push(packed([k,seed,8,0,0,...snap(c,s,[],draws)]));cases++;draws.length=0;
  if(cur&&(cur.kind==='enemy'||cur.charmed))op=7;
  else if(step===17)op=4;
  else if(step%7===6){op=3;x=step%4;}
  else if(step%6===5)op=5;
  else if(step%5===4){op=0;x=(k+step)%8;}
  else if(step%4!==3){op=1;const foes=c.combatants.filter((a:any)=>a.kind==='enemy'&&a.status==='active');x=foes[0]?.x??6;y=foes[0]?.y??5; if(k%7===0){x=10;y=10;}}
  if(step===0&&k%5===0&&cur?.kind==='player'&&!cur.charmed){op=0;x=3;}
  const priorAmmo=[s.equipmentQuantities[16],s.equipmentQuantities[27]];
  let ev:any[]=[];if(op===0)ev=c.playerMove(['east','west','south','north','ne','nw','se','sw'][x]);if(op===1)ev=c.playerAttack(x,y);if(op===2)ev=c.playerPass();if(op===3)ev=c.playerEscape(['east','west','south','north'][x]);if(op===4)ev=c.playerEscapeQuick();if(op===5)ev=c.playerAttackCancel();if(op===7)ev=c.tickEnemyTurnStep();
  for(const [i,id] of [16,27].entries()){if(priorAmmo[i]===1&&s.equipmentQuantities[id]===0)hit('ammo:depleted');if(priorAmmo[i]===0&&s.equipmentQuantities[id]===255)hit('ammo:wrapped');}
  if(c.triggers.some((t:any)=>t.at.x===255))hit('trigger:spent');
  for(const e of ev){hit(`event:${e.kind}`);if(e.kind==='attacked')hit(`attack:${e.hit}:${e.damage??'none'}`);if(e.text)hit(`text:${e.text}`);}
  rows.push(packed([k,seed,op,x,y,...snap(c,s,ev,draws)]));cases++;
 }
 // Actual Game handoff methods, with only excluded world services stubbed.
 const hs=structuredClone(initial),live=new OriginalRng(seed),defs:any[]=[];defs[def.index]=def;
 const handoffDraws:number[]=[];OriginalRng.prototype.next=function(lo,hi){const v=originalNext.call(this,lo,hi);handoffDraws.push(lo,hi,v);return v;};
 const mock:any={state:hs,liveRng:live,rand:(lo:number,hi:number)=>live.next(lo,hi),combat:null,
  combatResources:{enemyDefs:defs,combatMaps:[map],attackValues:attack,attackRangeValues:range,defenseValues:defense,spellAttackRange:strength},
  doors:{reset:()=>{}},activeMap:{tileAt:()=>5},overworldEnemies:{removeEnemy:()=>{}},pirateShipVictoryLatch:()=>undefined,
  ringExpiryEvents:()=>Game.prototype['ringExpiryEvents'].call(mock),checkRefuge:()=>[],dungeonState:null};
 const ev=Game.prototype.startCombat.call(mock,{defIndex:def.index} as any,'south',{intro:k%2?'attacked':'none'});
 const hc=mock.combat;OriginalRng.prototype.next=originalNext;
 const ge=(events:any[])=>[events.length,...events.flatMap(e=>[['message','sfx','party-changed','combat-started','combat-ended'].indexOf(e.kind),...str(e.text??e.sfx?.id)])];
 rows.push(packed([k,seed,9,0,0,...snap(hc,hs,[],handoffDraws),live.getSeed(),...ge(ev)]));cases++;
 const end=Game.prototype.endCombat.call(mock);
 rows.push(packed([k,seed,10,0,0,...snap(hc,hs,[],[]),live.getSeed(),...ge(end)]));cases++;
}
for(const key of ['ammo:depleted','ammo:wrapped','trigger:spent','event:moved','event:died','event:ended','event:projectile','attack:false:none','attack:true:0','attack:true:1','attack:true:20','attack:true:99','text:Blocked!','text:Out of range.','text:VICTORY!','text:All must use the same exit!','text:Thy sword hath shattered!','text:Rat interferes!'])if(!coverage[key])throw Error(`missing combat coverage ${key}`);
output('fixtures/combat.txt',rows.join('\n')+'\n');output('fixtures/combat-coverage.json',JSON.stringify({cases,coverage},null,2)+'\n');console.log(`${cases} combat parity snapshots`);
