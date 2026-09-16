import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
import {Combat,combatCastEffect,combatCastAbsorbed,type CombatMapData} from '../../../game/src/core/combat/combat.js';
import {decodeAbilities} from '../../../game/src/core/combat/enemies.js';
import {OriginalRng} from '../../../game/src/core/rng-original.js';
import {Game} from '../../../game/src/core/game.js';
import {buildSpellDefs} from '../../../game/src/core/magic/spells.js';
import {castSpell,applyMani,applyVasMani,applyCure,applyAwaken,applyResurrect} from '../../../game/src/core/magic/cast.js';
const root=new URL('../',import.meta.url),check=process.argv.includes('--check'),real=process.argv.includes('--real-arenas');
const defs=buildSpellDefs(JSON.parse(readFileSync(new URL('../../../game/src/core/data/MagicDefinitions.json',import.meta.url),'utf8')));
const maps:CombatMapData[]=real?JSON.parse(readFileSync(new URL('../../../game/assets/maps/combatmaps.json',import.meta.url),'utf8')):[];
const rows:string[]=[];let previous:number[]=[];let cases=0;const coverage:Record<string,number>={};
const hit=(s:string)=>coverage[s]=(coverage[s]??0)+1;
function packed(v:number[]){let out:string[]=[];for(let i=0;i<v.length;){let j=i;while(j<v.length&&v[j]===previous[j])j++;if(j-i>2){out.push(`p${j-i}`);i=j;continue;}j=i+1;while(j<v.length&&v[j]===v[i])j++;out.push(j-i>2?`r${j-i}:${v[i]}`:String(v[i]));i=j-i>2?j:i+1;}previous=v;return out.join(' ');}
const str=(s:string|undefined)=>s===undefined?[-1]:[s.length,...Array.from(s).map(c=>c.charCodeAt(0))];
function snap(c:any,s:any,events:any[],draws:number[]){
 const statuses=['active','dead','fled','sleeping','charmed','absorbed'];const kinds=['message','echo','moved','attacked','died','turn','ended','projectile','quake','lineSpray'];
 const actors=c.combatants.flatMap((a:any)=>[a.id,a.charIdx??255,a.enemyDef?.index??-1,a.x,a.y,a.hp,a.maxHp,a.str,a.dex,a.int,a.defense,a.attack,a.attackRange,statuses.indexOf(a.status),+a.isFleeing,a.speed,a.counter,+a.sleeping,+a.draggedUnder,+a.charmed,+a.invisible,a.renderTile??-1,a.lastAttacker??-1,a.lastTargetId??-1,a.weapons?.length??0,...(a.weapons??[]).flatMap((w:any)=>[w.id,w.attack,w.range])]);
 const loot=Array(121).fill(-1),chests=Array(121).fill(-1);for(const [key,v] of c.lootLayer){const [x,y]=key.split(':').map(Number);loot[y*11+x]=v;}for(const [key,v] of c.chestContents){const [x,y]=key.split(':').map(Number);chests[y*11+x]=v;}
 return [c.finalSeed,c.actionCount,c.scanIdx,c.currentActor?.id??-1,+c.ended,+c.victory,+c.over,c.spoilChests,['east','west','south','north'].indexOf(c.escapeBorder),c.combatants.length,...actors,c.activeWeaponQueue===null?-1:c.activeWeaponQueue.length,...(c.activeWeaponQueue??[]).flatMap((w:any)=>[w.id,w.attack,w.range]),s.activeCharacter,s.food,...s.characters.flatMap((a:any)=>[a.currentHp,a.status.charCodeAt(0),a.exp,a.helmet,a.armor,a.weapon,a.shield,a.ring,a.amulet]),...s.equipmentQuantities,...Array.from({length:16},(_,i)=>c.xpByChar.get(i)??0),...c.mapTiles.flat(),...loot,...chests,c.triggers.length,...c.triggers.flatMap((t:any)=>[t.sprite,t.at.x,t.at.y,t.pos1.x,t.pos1.y,t.pos2.x,t.pos2.y]),events.length,...events.flatMap(e=>[kinds.indexOf(e.kind),e.actorId??-1,e.targetId??-1,e.x??-1,e.y??-1,e.damage??-1,e.hit===undefined?-1:+e.hit,e.grazed===undefined?-1:+e.grazed,e.dragged===undefined?-1:+e.dragged,...str(e.text)]),draws.length,...draws,
 ...s.characters.flatMap((a:any)=>[a.currentMp,a.maxHp,a.level,a.intelligence]),...s.spellQuantities,...s.reagentQuantities,s.timeSpell.charCodeAt(0)||0,s.timeSpellTurns,...events.map(e=>e.mode??-1),c.fieldSlots.length,...c.fieldSlots.flatMap((f:any)=>[f.x,f.y,f.tile])];
}
const flags=[0,0x1000,0x4000,8,4,0x20,0x80,0x2000,0x800,0x402,0x402c,0x1080,0,0,0,0];
const spells=real?[1,10,24,28,30,34,35,38,40,43,44,45,47]:Array.from({length:49},(_,i)=>i);
for(let arena=0;arena<(real?512:1);++arena)for(const spell of spells)for(let variant=0;variant<(real?2:40);++variant)for(let si=0;si<(real?1:8);++si){
 const v=real?(variant===0?1:10):variant,seed=(si*8191+v*101+spell*31+arena*17)&65535;
 const s:any={characters:Array.from({length:2},(_,i)=>({name:`MEM${i}`,class:'A',status:i===1?['G','P','S','D'][v%4]:'G',currentHp:v%9===0?100:50,maxHp:100,currentMp:v%7===0?0:30,level:v%11===0?1:8,strength:20,dexterity:30,intelligence:v%3===0?1:30,exp:1000,helmet:255,armor:255,weapon:255,shield:255,ring:255,amulet:v%2===0?45:255,partyStatus:0})),partySize:2,activeCharacter:255,equipmentQuantities:Array(256).fill(0),reagentQuantities:Array(8).fill(10),spellQuantities:Array(48).fill(v%13===0?0:5),food:100,position:{location:v%17===0?18:0,floor:0,x:5,y:5},transportTile:28,karma:75,wornCrown:v%2===0,timeSpell:v===18?'N':v===19?'C':v===20?'T':v===21?'Q':'',timeSpellTurns:30};
 const enemies:any[]=Array.from({length:48},(_,i)=>({index:i,name:'Enemy',groupName:'ENEMIES',str:15,dex:10,int:v%3===0?30:1,armour:v%5===0?20:0,damage:10,hp:v%5===0?10:100,maxPerMap:3,treasure:10,attackRange:5,moveClass:0,abilities:decodeAbilities(i===20?flags[v%16]!:0),doesNotMove:false}));
 let map:CombatMapData={index:0,territory:'test',name:null,tiles:Array.from({length:11},()=>Array(11).fill(v===22?12:5)),playerStarts:{east:[{x:2,y:2},{x:3,y:3}],west:[{x:2,y:2},{x:3,y:3}],south:[{x:2,y:2},{x:3,y:3}],north:[{x:2,y:2},{x:3,y:3}]},units:Array.from({length:16},(_,i)=>({sprite:0,x:4+i%5,y:2+Math.floor(i/5)})),triggers:[]};
 if(v===23)map.tiles[2]![3]=12;if(v===24)map.tiles[2]![2]=143;
 if(real)map=structuredClone(maps[Math.floor(arena/4)]!);
 const draws:number[]=[];const original=OriginalRng.prototype.next;
 OriginalRng.prototype.next=function(lo,hi){const n=original.call(this,lo,hi);draws.push(lo,hi,n);return n;};
 const c:any=new Combat({map,entryDirection:['east','west','south','north'][arena%4] as any,party:s.characters.map((record:any,charIdx:number)=>({record,charIdx,weapons:[{id:255,attack:1,range:1}]})),enemies:[{def:enemies[20],count:v===25?16:3}],enemyDefs:enemies,seed,state:s,roomCombat:real?map.territory==='dungeon':false});
 // Set deterministic turn/status fixtures in both engines; constructor RNG remains compared.
 c.currentActor=c.combatants[0];c.combatants[0].counter=6;
 if(!real && v>=24 && v<=27)c.fieldSlots=[{x:2,y:2,tile:232+v-24},{x:2,y:2,tile:233},{x:4,y:2,tile:235}];
 if(v===26)c.combatants[2].sleeping=true;
 if(v===27)c.combatants[2].isFleeing=true;
 if(v===28)c.combatants[2].invisible=true;
 if(v===29)c.combatants[2].charmed=true;
 if(v===30){c.combatants[2].hp=0;c.combatants[2].status='dead';}
 if(v>=32&&v<=34)c.combatants[2].enemyDef=enemies[[47,14,15][v-32]!];
 if(v===35)for(const a of c.combatants)if(a.kind==='enemy')a.hp=1;
 if(v===36)for(let i=0;i<300;++i)c.combatants.push({...c.combatants[2],id:c.nextId++});
 if(v===37)for(let y=0;y<11;++y)for(let x=0;x<11;++x)c.combatants.push({...c.combatants[2],id:c.nextId++,x,y});
 function record(op:number,x:number,y:number,ev:any[]){for(const e of ev){hit(`event:${e.kind}`);if(e.text)hit(`text:${e.text}`);} hit(`spell:${spell}`);if(c.combatants.length>22)hit('growth:over22');rows.push(packed([arena,spell,v,seed,op,x,y,...snap(c,s,ev,draws)]));cases++;draws.length=0;}
 record(-1,0,0,[]);
 for(let step=0;step<6;++step){
  let ev:any[]=[],op=step%2===0?0:step===5?3:4;
  if(op===0)c.currentActor=c.combatants[0];
  if(op===4)c.currentActor=c.combatants[2];
  const cur=c.currentUnit;const foe=c.combatants.find((a:any)=>a.kind==='enemy'&&a.status==='active');const aim=v%4===0?null:v%4===1?{x:10,y:10}:foe?{x:foe.x,y:foe.y}:{x:4,y:2};
  let x=aim?.x??-1,y=aim?.y??-1;
  if(op===0 && cur?.kind==='player'){
   ev.push({kind:'echo',text:'Cast...\n'});
   if(combatCastAbsorbed(s.timeSpell,s.position.location,s.wornCrown)){ev.push({kind:'message',text:'Absorbed!\n'});ev.push(...c.playerCast(null,null));}
   else {
    const r=castSpell(s,s.characters[cur.charIdx],defs[spell]!,{location:s.position.location,inCombat:true},c.rng);
    if(r.message)ev.push({kind:'message',text:r.message});if(!r.ok&&r.consumed)ev.push({kind:'message',text:'Failed!'});
    const fx=combatCastEffect(r.effect);if(fx)hit(`effect:${fx.kind}`);
    if(v===31 && fx && ['combatAttack','lineAoe','illusion','dispelField'].includes(fx.kind)) { /* paid target cancellation, no turn */ }
    else if(fx && ['healTarget','cure','awaken','resurrect'].includes(fx.kind)){
      const target=s.characters[1];let ok=false;
      if(fx.kind==='healTarget')ok=fx.mode==='full'?applyVasMani(target):applyMani(target,c.rng)>0;
      else if(fx.kind==='cure')ok=applyCure(target);else if(fx.kind==='awaken')ok=applyAwaken(target);else ok=applyResurrect(target,s.karma);
      ev.push({kind:'message',text:ok?'Success!':'Failed!'});
      const actor=c.combatants.find((a:any)=>a.kind==='player'&&a.charIdx===1);
      if(actor){actor.hp=target.currentHp;if(target.status!=='D'&&actor.status==='dead')actor.status='active';if(target.status==='S')actor.sleeping=true;else if(actor.sleeping&&target.status==='G')actor.sleeping=false;}
      ev.push(...c.playerCast(null,null));
    }else ev.push(...c.playerCast(fx,aim));
   }
  }else if(op===3)ev=c.playerEscapeQuick();
  else if(cur?.kind==='enemy'||cur?.charmed){if(op!==4)op=1;ev=c.tickEnemyTurnStep();}
  else {if(op!==4)op=2;ev=c.playerPass();}
  record(op,x,y,ev);
 }
 const live=new OriginalRng(seed);
 const mock:any={state:s,combat:c,liveRng:live,dungeonState:null,pirateShipVictoryLatch:()=>undefined,checkRefuge:()=>[]};
 const end=Game.prototype.endCombat.call(mock);
 rows.push(packed([arena,spell,v,seed,6,0,0,...snap(c,s,[],draws),live.getSeed(),end.length,...end.flatMap(e=>[['message','sfx','party-changed','combat-started','combat-ended'].indexOf(e.kind),...str(e.kind==='message'?e.text:undefined)])]));cases++;
 OriginalRng.prototype.next=original;
}
if(!real)for(const k of ['text:Enemy divides!','text:Enemy gates in a daemon!','text:MEM0 possessed!','text:Enemy teleports!','event:lineSpray','event:quake','growth:over22'])if(!coverage[k])throw Error(`missing ${k}`);
if(real)for(const k of ['effect:combatAttack','effect:summonAlly','effect:summonSwarms','effect:summonDaemon','effect:polymorphRat','effect:illusion','event:lineSpray','event:quake','text:Enemy divides!'])if(!coverage[k])throw Error(`missing real-arena ${k}`);
const dest=real?'build-real-arenas/':'fixtures/';if(real)mkdirSync(new URL(dest,root),{recursive:true});
for(const [path,s] of [[dest+'advanced-combat.txt',rows.join('\n')+'\n'],[dest+'advanced-combat-coverage.json',JSON.stringify({cases,coverage},null,2)+'\n']]){const url=new URL(path!,root);if(check){if(readFileSync(url,'utf8')!==s)throw Error(`drift ${path}`);}else writeFileSync(url,s!);}
console.log(`${cases} ${real?'real-arena ':''}advanced combat snapshots`);
