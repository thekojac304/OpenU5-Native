import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {isDeepStrictEqual} from 'node:util';
import {grantPlotItem} from '../../../game/src/core/quest/items.js';
import {destroyShadowlord} from '../../../game/src/core/quest/shadowlords.js';
import {castShardIntoFlame,summonShadowlord} from '../../../game/src/core/quest/ritual.js';
import {yellWordOfPower} from '../../../game/src/core/quest/words.js';
import {endgameReady,rescueLordBritish} from '../../../game/src/core/quest/lordbritish.js';
import {endgamePlaytime} from '../../../game/src/core/quest/endgame.js';
import * as shrine from '../../../game/src/core/world/shrines.js';
import {advanceMelody} from '../../../game/src/core/world/harpsichord.js';
import {applyFaulineiTheft} from '../../../game/src/core/world/faulinei-theft.js';
import {OriginalRng} from '../../../game/src/core/rng-original.js';
import {instalaDsStrings} from '../../../game/src/core/data/ds-strings.js';
import {exportNativeSave,importNativeSave,SAVED_GAM_SIZE} from '../../../game/src/core/saveNative.js';
import {emptySidecar} from '../../../game/src/core/u5gam.js';
import * as ceremony from '../../../game/src/core/world/shrine-ceremonies.js';
import {Game} from '../../../game/src/core/game.js';
import {isPassable} from '../../../game/src/core/world/movement.js';
import {DungeonState} from '../../../game/src/core/dungeon/dungeon.js';
import {Combat} from '../../../game/src/core/combat/combat.js';
import {buildEnemyDefs} from '../../../game/src/core/combat/enemies.js';
import {NpcManager} from '../../../game/src/core/npc/manager.js';
import * as capture from '../../../game/src/core/world/blackthorn-capture.js';
const dir='native/core/build-quests'; mkdirSync(dir,{recursive:true});
const data=JSON.parse(readFileSync('game/assets/data.json','utf8'));
const worldQuestData={locationsX:data.locationsX,locationsY:data.locationsY,wordsOfPower:data.wordsOfPower,shardSpawns:data.shardSpawns,searchObjects:data.searchObjects,shrineX:data.shrineX,shrineY:data.shrineY,virtues:data.virtues,mantras:data.mantras,shrines:{virtues:data.virtues,mantras:data.mantras,shrineX:data.shrineX,shrineY:data.shrineY}};
const realOverworld=JSON.parse(readFileSync('game/assets/maps/overworld.json','utf8'));writeFileSync(dir+'/overworld.bin',Buffer.from(realOverworld.flat()));
const ds=JSON.parse(readFileSync('game/assets/ds-strings.json','utf8')); instalaDsStrings(ds);
const bits=['falsehood','hatred','cowardice'];
const items=['amulet','crown','sceptre','wooden-box','carpet','shard-falsehood','shard-hatred','shard-cowardice'];
function initial():any {
 return {version:1,characters:[{name:'Avatar',gender:11,class:'A',status:'G',strength:20,dexterity:20,intelligence:20,currentMp:0,currentHp:30,maxHp:30,exp:0,level:1,monthsAtInn:0,helmet:255,armor:255,weapon:255,shield:255,ring:255,amulet:255,partyStatus:0}],partySize:1,activeCharacter:0,food:100,gold:500,keys:0,gems:0,torches:0,skullKeys:0,magicCarpets:0,karma:50,turnsSinceStart:0,torchTurns:0,transport:'foot',time:{year:139,month:4,day:5,hour:12,minute:0},position:{location:0,floor:0,x:10,y:10},equipmentQuantities:Array(48).fill(0),scrollQuantities:Array(8).fill(0),potionQuantities:Array(8).fill(0),spellQuantities:Array(48).fill(0),reagentQuantities:Array(8).fill(0),npcDead:Array.from({length:32},()=>Array(32).fill(false)),npcMet:Array.from({length:32},()=>Array(32).fill(false)),questFlags:{},shards:{falsehood:false,hatred:false,cowardice:false},lbArtifacts:{amulet:false,crown:false,sceptre:false},specialItems:{woodenBox:false}};
}
const fields=['food','partySize','reagentQuantities','questFlags','shards','lbArtifacts','shrineQuestBitmap','shrineVisitedBitmap','shrineDestroyed','shadowlordDoomBits','shadowlordSummoned','magicCarpets','karma','gold','keys','gems','torches','equipmentQuantities','potionQuantities','scrollQuantities','characters','time','position','turnsSinceStart','npcDead'];
function projection(s:any){return Object.fromEntries([...fields.map(k=>[k,s[k]]),['box',s.specialItems.woodenBox],['hmsCape',s.specialItems.hmsCape??false],['skullKeys',s.skullKeys],['moonstones',s.moonstones??[]],['worldObjects',s.worldObjects??[]],['wornCrown',s.wornCrown??false],['timeSpell',s.timeSpell],['timeSpellTurns',s.timeSpellTurns]]);}
function combatSnapshot(c:any){return c?{active:true,seed:c.finalSeed,map:c.map.index,tiles:c.mapTiles.flat(),absorbed:c.absorptionSentinel,current:c.currentActor?.id??-1,actors:c.combatants.map((a:any)=>[a.id,a.charIdx??255,a.enemyDef?.index??-1,a.x,a.y,a.hp,a.maxHp,a.str,a.dex,a.int,a.defense,a.attack,a.attackRange,['active','dead','fled','sleeping','charmed','absorbed'].indexOf(a.status),a.speed,a.counter])}:{active:false};}
const inputs:any[]=[],expected:any[]=[],counts:Record<string,number>={};
function add(op:string,p:any={},edit:(s:any)=>void=()=>{}) {
 if(process.env.QUEST_FOCUS && !p[process.env.QUEST_FOCUS])return;
 // Combat-only replays never search. Omit that unrelated catalog to retain the
 // production JSON parser's existing node budget while supplying real arenas.
 if(p.combat && !p.actions?.some((a:any)=>a.kind==='search'))p={...p,data:{...p.data,searchObjects:[]}};
 const s=initial();edit(s); const q={op,...p,state:structuredClone(s)}; let r:any={};
 switch(op) {
 case 'world-flow':{
   const rows=p.realOverworld?realOverworld:Array.from({length:256},()=>Array(256).fill(5));
   const small=Array.from({length:32},()=>Array(32).fill(5));
   if(p.tiles)for(let i=0;i<p.tiles.length;i++)small[i>>5]![i&31]=p.tiles[i];
   const npcs=p.npcs?new NpcManager({[s.position.location]:p.npcs}):undefined;
   const extraMaps:any[]=p.townTiles?[[17,{id:17,name:'',floors:[{z:0,tiles:Array.from({length:32},(_,y)=>p.townTiles.slice(y*32,y*32+32))}]}]]:[];
   const g=new Game({} as any,{overworld:rows,underworld:rows,smallMaps:new Map([[s.position.location,{id:s.position.location,name:'',floors:[{z:s.position.floor,tiles:small}]}],...extraMaps])},p.data,s,{npcManager:npcs,endgameText:p.endgameText,combatResources:p.combat?{...p.combat,combatMaps:Object.fromEntries(p.combat.combatMaps.map((m:any)=>[m.index,m]))}:undefined});
   if(p.dungeon){const d=p.dungeon;g.dungeonState=new DungeonState([{location:40,name:'Doom',floors:Array.from({length:8},(_,f)=>Array.from({length:8},(_,y)=>Array.from({length:8},(_,x)=>({type:d.cells[f*64+y*8+x]>>4,sub:d.cells[f*64+y*8+x]&15}))))}],{dungeon:40,floor:d.floor,x:d.x,y:d.y,facing:['north','east','south','west'][d.facing] as any});}
   g.reseed(p.seed);if(p.here!==undefined)s.shadowlordHere=p.here;r.steps=[];
   for(const a of p.actions){let events:any[]=[],combatEvents:any[]=[];
     if(a.kind==='hydrate')(g as any).hydrateUnderworldPlot();
     else if(a.kind==='fixed'){
       const map=p.combat.combatMaps.find((m:any)=>m.sourceIndex===a.map);
       (g as any).combat=new Combat({map,entryDirection:'south',party:s.characters.slice(0,s.partySize).map((record:any,charIdx:number)=>({record,charIdx,weapons:[{id:255,attack:1,range:1}]})),enemies:{fixedFromMap:true,defs:p.combat.enemyDefs},seed:g.liveSeed(),state:s,roomCombat:true,dungeonFloor:7});
       events=[{kind:'message',text:'Entering room...'},{kind:'combat-started'}];
     }
     else if(a.kind==='battle-fight'){for(let tick=0;tick<(a.repeat??1);tick++){const previous=combatEvents;combatEvents=[];const c=(g as any).combat,u=c.currentUnit;if(u){if(u.kind!=='player')combatEvents=c.tickEnemyTurnStep();else{const targets=c.combatants.filter((e:any)=>e.kind==='enemy'&&(e.status==='active'||e.status==='sleeping')).sort((a:any,b:any)=>(Math.max(Math.abs(a.x-u.x),Math.abs(a.y-u.y))-Math.max(Math.abs(b.x-u.x),Math.abs(b.y-u.y)))||a.id-b.id),e=targets[0];if(!e)combatEvents=c.playerPass();else if(Math.max(Math.abs(e.x-u.x),Math.abs(e.y-u.y))<=u.attackRange)combatEvents=c.playerAttack(e.x,e.y);else combatEvents=c.playerMove(e.x!==u.x?(e.x>u.x?'east':'west'):(e.y>u.y?'south':'north'));}}previous.push(...combatEvents);combatEvents=previous;}}
     else if(a.kind==='battle-hold'){const c=(g as any).combat,actor=c.currentUnit;if(actor)combatEvents=actor.kind==='player'?c.playerPass():c.tickEnemyTurnStep();}
     else if(a.kind==='battle-step'){const c=(g as any).combat,actor=c.currentUnit;if(actor)combatEvents=actor.kind==='player'?c.playerMove('south'):c.tickEnemyTurnStep();}
     else if(a.kind==='battle-sceptre'){const count=(g as any).combat.sceptreDissolveFields();events=[{kind:'message',text:'Sceptre'},{kind:'message',text:'Wielding the Sceptre of Lord British...'},...(count?[]:[{kind:'message',text:'No effect!'}])];}
     else if(a.kind==='battle-move')combatEvents=(g as any).combat.playerMove(['east','west','south','north'][a.dir]);
     else if(a.kind==='battle-pass')combatEvents=(g as any).combat.playerPass();
     else if(a.kind==='battle-enemy')combatEvents=(g as any).combat.tickEnemyTurnStep();
     else if(a.kind==='battle-end')events=g.endCombat();
     else if(a.kind==='doom')events=(g as any).doomEntranceAmbush(40)??[];
     else if(a.kind==='npc-attack')events=(g as any).hostileNpcAttack(npcs!.npcsAt(s.position.location,s.position.floor).find(n=>n.slot===a.slot));
     else if(a.kind==='interior')g.hydrateInteriorObjects(s.position.location);
     else if(a.kind==='urban')(g as any).applyUrbanShadowlord(s.position.location,events);
     else if(a.kind==='move')events=g.move(['north','south','east','west'][a.dir] as any);
     else if(a.kind==='enter')events=g.enter();
     else if(a.kind==='shrine-answer')events=g.submitShrineVisit(a.word,[a.mantra,a.mantra,a.mantra]);
     else if(a.kind==='refuge-check')events=(g as any).checkRefuge();
     else if(a.kind==='refuge-resolve')events=g.resolveRefuge();
     else if(a.kind==='check-guard')events=(s.position.location===18?(g as any).checkBlackthornCapture():(g as any).checkGuardTribute())??[];
     else if(a.kind==='password')events=g.submitGuardPassword(a.word);
     else if(a.kind==='tribute')events=g.resolveGuardTribute(a.agree);
     else if(a.kind==='arrest')events=g.resolveGuardArrest(a.agree);
     else if(a.kind==='capture')events=capture.runCaptureScene((g as any).captureCtx());
     else if(a.kind==='answer')events=g.submitInterrogationResponse(a.word);
     else if(a.kind==='absorb')events=(g as any).fireAbsorptionEndgame();
     else if(a.kind==='search')events=g.search(a.dir===undefined?undefined:['north','south','east','west'][a.dir] as any);
     else if(a.kind==='get')events=g.get(['north','south','east','west'][a.dir??0] as any);
     else if(a.kind==='moon')events=g.useMoonstone(a.item);
     else if(a.kind==='yell')events=g.yellWord(a.word);
     else if(a.kind==='note')events=g.playHarpsichordNote(a.item);
     else if(a.item>=29&&a.item<=31)events=g.useShard(bits[a.item-29] as any);
     else if(a.item===18)events=g.useAmulet();
     else if(a.item===19)events=g.useCrown();
     else if(a.item===20)events=g.useSceptre();
     else if(a.item===36)events=g.useBlackBadge();
     else if(a.item===33)events=g.useHmsCape();
     else if(a.item===37)events=g.useWoodenBox();
     r.steps.push({events:events.map(e=>({kind:e.kind,text:e.kind==='sfx'?e.sfx.id:e.kind==='game-won'||e.kind==='endgame'?e.ending:e.text??'',...(e.kind==='guard-tribute-prompt'?{charity:e.charity,...(e.toll!==undefined?{toll:e.toll}:{})}:{}),...(e.endgame?{endgame:e.endgame}:{}),...(e.refuge?{refuge:e.refuge}:{}),...(e.kind==='cell-explosion'?{cellFx:e.cellFx}:{}),...(e.kind==='sfx'&&e.sfx.id==='instrument-note'?{note:e.sfx.n}: {})})),state:projection(structuredClone(s)),seed:g.liveSeed(),progress:(g as any).harpsichordProgress,passage:g.harpsichordPassageRevealed,...(p.dungeon?{dungeonCells:Array.from({length:512},(_,i)=>{const c=g.dungeonState!.cellAt(i>>6,i&7,(i>>3)&7);return c.type*16+c.sub;})}:{}),...(p.combat?{combat:combatSnapshot((g as any).combat),combatEvents:combatEvents.map(e=>[e.kind,e.text??"",e.actorId??-1,e.targetId??-1,e.x??-1,e.y??-1,e.damage??-1])}:{}),...(p.inspectTiles?{tiles:Array.from({length:1024},(_,i)=>(g as any).mapTileWithOverrides(i&31,i>>5))}:{}),...(npcs?{npcs:[...new Set(p.npcs.flatMap((n:any)=>n.z.map((z:number)=>z===255?-1:z)))].flatMap((z:any)=>npcs.npcsAt(s.position.location,z)).sort((a,b)=>a.slot-b.slot).map(n=>({slot:n.slot,dialog:n.dialogNumber,ai:n.aiTypes}))}:{})});
   }break;
 }
 case 'destroy':r=destroyShadowlord(s,bits[p.i] as any);break;
 case 'grant':r.message=grantPlotItem(s,items[p.item] as any);break;
 case 'shrine-flow':{
   const pending:ceremony.ShrinePendingHolder={visit:null,restore:null,scene:null};
   const ctx={state:s,shrines:p.data,pending,tileAt:()=>26};r.steps=[];
   for(const a of p.actions){let events:any[]=[],combatEvents:any[]=[];
     switch(a.action){
       case 0:events=ceremony.runShrineCeremony(ctx,{kind:'visit',virtue:a.value});break;
       case 1:events=ceremony.runShrineCeremony(ctx,{kind:'codex'});break;
       case 2:events=ceremony.submitShrineVisit(ctx,a.virtue??'',a.mantras??[]);break;
       case 3:events=ceremony.submitShrineRestore(ctx,a.virtue??'',a.mantras??[]);break;
       case 4:events=ceremony.submitDonation(ctx,a.value);break;
       case 5:ceremony.applyShrineGuardian(ctx,events);break;
       case 6:ctx.tileAt=()=>a.tile;ceremony.checkShrineEntry(ctx,events);break;
       case 7:events=Game.prototype.enter.call({state:s,data:{shrines:p.data},activeMap:{tileAt:()=>a.tile},runShrineCeremony:(pending:any)=>ceremony.runShrineCeremony(ctx,pending)} as unknown as Game);break;
     }
     r.steps.push({events:events.map(e=>({kind:e.kind,text:e.kind==='sfx'?e.sfx.id:e.kind==='ritual-invert'?(e.ritual??''):e.text??''})),visit:pending.visit?.virtue??-1,restore:pending.restore?.virtue??-1,state:projection(structuredClone(s))});
   }break;
 }
 case 'native-roundtrip':{const ex=exportNativeSave(s,new Uint8Array(SAVED_GAM_SIZE));Object.assign(s,importNativeSave(ex.gam,ex.sidecar));break;}
 case 'ritual':r=castShardIntoFlame({shardIdx:p.i,partyX:p.x,partyY:p.y,location:p.location,floor:p.floor,tileAbove:p.above,summonedIdx:p.summoned});break;
 case 'summon':r={idx:summonShadowlord({word:p.word,partyY:p.y,alive:p.alive,shadowlordPresent:p.present}).idx??-1};break;
 case 'word':{const a=yellWordOfPower(p.words,p.word,p.adjacent);r={uttered:a.uttered,opened:a.opened,location:a.openedLocation??-1,lines:a.messages};break;}
 case 'rescue':{const ready=endgameReady(s),a=rescueLordBritish(s,{viaAbsorption:p.absorption});r={ready,ending:['incomplete','victory','stranded'].indexOf(a.ending)};break;}
 case 'melody':r=advanceMelody(p.progress,p.digit);break;
 case 'donate':r=shrine.shrineDonate(s,p.cycles);break;
 case 'codex':{const a=shrine.shrineCodexLesson(s);r={virtue:a.virtue??-1,ceremony:a.ceremony};break;}
 case 'shrine':r.mode=['show-mantra','quest-complete','donation'].indexOf(shrine.shrineMode(s,p.v));if(p.action===0)shrine.shrineShowMantra(s,p.v,data);else r.attrs=shrine.shrineCompleteQuest(s,p.v,s.characters[0]).attrs.reduce((n,a)=>n|({strength:1,dexterity:2,intelligence:4}[a]),0);break;
 case 'check':r.ok=shrine.shrineVisitCheck(p.v,p.virtue,p.mantras,p.data);break;
 case 'restore':r.ok=shrine.shrineRestore(s,p.v,p.virtue,p.mantras,p.x,p.y,p.data).restored;break;
 case 'theft':{let draws=0;s.shadowlordHere=p.here;const rng=new OriginalRng(p.seed);try {const a=applyFaulineiTheft(s,(lo,hi)=>{if(++draws>65536)throw 'cycle';return rng.next(lo,hi);});r={kind:a.loot?['keys','gems','torches','equipment','potion','scroll','gold'].indexOf(a.loot.kind)+1:0,index:a.loot?.index??-1,amount:a.loot?.amount??0};}catch(e){if(e!=='cycle')throw e;r={nonterminating:true};}r.seed=rng.getSeed();break;} case 'playtime':r=endgamePlaytime(s);break;
 }
 if(p.victory && !r.steps.some((s:any)=>s.combatEvents?.some((e:any)=>e[1]==='VICTORY!')))throw Error('Victory fixture did not win');
 if(p.defeat && !r.steps.some((s:any)=>s.events.some((e:any)=>e.kind==='refuge')))throw Error('Defeat fixture did not reach Refuge');
 inputs.push(q);expected.push({...r,state:projection(s)});counts[op]=(counts[op]??0)+1;
}
for(let mask=0;mask<128;mask++)add('world-flow',{seed:1,data:worldQuestData,actions:[{kind:'hydrate'},{kind:'hydrate'}]},s=>{s.position.floor=255;s.lbArtifacts.amulet=!!(mask&64);bits.forEach((b,i)=>{s.shards[b]=!!(mask&(1<<i));s.questFlags['shadowlord-dead:'+b]=!!(mask&(8<<i));});s.worldObjects=[{location:1,floor:255,x:1,y:1,tile:180,kind:'plot',plotItem:'shard-falsehood'},{location:0,floor:0,x:2,y:2,tile:5,kind:'other'}];});
for(let i=0;i<3;i++)for(let mask=0;mask<32;mask++)add('world-flow',{seed:20,data:worldQuestData,actions:[{kind:'use',item:29+i},{kind:'use',item:29+i}]},s=>{s.position={location:30+i,floor:[2,1,-1][i],x:mask&1?15:14,y:[9,3,16][i]};s.shadowlordSummoned=mask&2?i:-1;s.shards[bits[i]!]=!!(mask&4);s.worldObjects=mask&8?[{location:30+i,floor:s.position.floor,x:15,y:s.position.y-1,tile:252,kind:'shadowlord'},{location:30+i,floor:s.position.floor,x:15,y:s.position.y-1,tile:252,kind:'other'}]:[];if(mask&16)s.questFlags['shadowlord-dead:'+bits[i]]=true;});
for(const location of [0,6,30,31,32])for(const word of ['FAULINEI','astaroth','nosfentor','wrong'])for(const y of [1,10])add('world-flow',{seed:19,data:worldQuestData,actions:[{kind:'yell',word},{kind:'yell',word}]},s=>{s.position={location,floor:0,x:15,y};});
for(const location of [0,17,30])for(const floor of [0,2])add('world-flow',{seed:3,data:worldQuestData,actions:[18,19,36,18,18,36,36,19].map(item=>({kind:'use',item})).concat([6,7,8,9,8,7,8,7,6,7,6,5,3].map(item=>({kind:'note',item})))},s=>{s.position={location,floor,x:15,y:15};});
for(let i=0;i<3;i++)for(let mask=0;mask<16;mask++)add('destroy',{i},s=>{s.shards[bits[i]!]=!!(mask&1);if(mask&2)s.questFlags['shadowlord-dead:'+bits[i]]=!!(mask&4);if(mask&8)destroyShadowlord(s,bits[i] as any);});
for(let i=0;i<8;i++)for(const n of [0,1,98,99,100,255])add('grant',{item:i},s=>{s.magicCarpets=n;s.shards.falsehood=true;});
for(let i=0;i<3;i++)for(let mask=0;mask<64;mask++)add('ritual',{i,x:mask&1?15:14,y:mask&2?[9,3,16][i]:0,location:mask&4?30+i:0,floor:mask&8?[2,1,-1][i]:5,above:mask&16?252:0,summoned:mask&32?i:-1});
for(const word of ['','faulinei','ASTAROTH','XnosfentorX','faulıneı','faulİnei','ß','faulinei astaroth'])for(let mask=0;mask<16;mask++)for(const y of [1,2])add('summon',{word,y,alive:bits.map((_,i)=>!!(mask&(1<<i))),present:!!(mask&8)});
for(const word of ['',...data.wordsOfPower,...data.wordsOfPower.map((w:string)=>` ${w.toLowerCase()}! `),'FALLAX AVIDUS'])for(const adjacent of [[],[33],[40],[33,34,35,36,37,38,39,40]])add('word',{words:data.wordsOfPower,word,adjacent});
for(let mask=0;mask<256;mask++)for(const absorption of [false,true])add('rescue',{absorption},s=>{bits.forEach((k,i)=>{s.questFlags['shadowlord-dead:'+k]=!!(mask&(1<<i));s.lbArtifacts[['amulet','crown','sceptre'][i]!]=!!(mask&(8<<i));});s.questFlags['in-doom']=!!(mask&64);s.specialItems.woodenBox=!!(mask&128);});
for(let progress=0;progress<13;progress++)for(let digit=0;digit<11;digit++)add('melody',{progress,digit});
for(const cycles of [-1,0,1,2,9,100,2147483647])for(const gold of [0,99,100,199,200,9999])for(const karma of [0,96,98,99,255])add('donate',{cycles},s=>{s.gold=gold;s.karma=karma;});
for(let quest=0;quest<256;quest++)for(const visited of [0,127,254,255])add('codex',{},s=>{s.shrineQuestBitmap=quest;s.shrineVisitedBitmap=visited;});
for(let v=0;v<8;v++)for(let mask=0;mask<4;mask++)for(const action of [0,1])for(const attr of [0,29,30,255])add('shrine',{v,action},s=>{s.shrineQuestBitmap=mask&1?1<<v:0;s.shrineVisitedBitmap=mask&2?1<<v:0;s.karma=98;s.characters[0].strength=attr;s.characters[0].dexterity=attr;s.characters[0].intelligence=attr;});
for(let v=0;v<8;v++)for(let variant=0;variant<7;variant++)for(const op of ['check','restore']) {
 const d=data, virtue=variant===1?d.virtues[v].toLowerCase():variant===2?'X'+d.virtues[v]+'X':variant===3?'':d.virtues[v];
 const mantras=Array(variant===4?2:3).fill(variant===5?'':d.mantras[v]);
 add(op,{v,virtue,mantras,data:{virtues:d.virtues,mantras:d.mantras,shrineX:d.shrineX,shrineY:d.shrineY},x:d.shrineX[v]+(variant===6?1:0),y:d.shrineY[v]},s=>s.shrineDestroyed=Array(8).fill(255));
}
for(const here of [-1,0,1,2])for(let mode=0;mode<9;mode++)for(let seed=0;seed<32;seed++)add('theft',{here,seed},s=>{if(mode<3)s[['keys','gems','torches'][mode]!]=2;else if(mode<6)s[['equipmentQuantities','potionQuantities','scrollQuantities'][mode-3]!][mode===3?47:7]=1;else s.gold=[0,5,100][mode-6];});
for(const year of [139,140,141])for(let month=1;month<=13;month++)for(const day of [1,4,5,28])add('playtime',{},s=>Object.assign(s.time,{year,month,day}));
for(let mask=0;mask<32;mask++)add('roundtrip',{},s=>{s.questFlags={'unknown:future':true,'game-won':!!(mask&1)};if(mask&2)s.shrineQuestBitmap=255;if(mask&4)s.shadowlordSummoned=2;if(mask&8)s.shadowlordDoomBits=14;if(mask&16)s.shrineDestroyed=[128,1];});
const nativeInitial=importNativeSave(readFileSync('original/u5/ultima5/INIT.GAM'),emptySidecar());
const shrineData={virtues:data.virtues,mantras:data.mantras,shrineX:data.shrineX,shrineY:data.shrineY};
for(let v=0;v<8;v++)for(let mask=0;mask<4;mask++)for(const valid of [false,true]) {
 const answer={action:2,virtue:valid?data.virtues[v]:'incorrect',mantras:Array(3).fill(data.mantras[v])};
 add('shrine-flow',{data:shrineData,records:ds['MISCMSG.DAT'],actions:[{action:7,tile:25},answer,{action:7,tile:17},{action:0,value:v},answer,{action:4,value:9},{action:4,value:1},{action:4,value:0},{action:6,tile:26},{action:3,virtue:'incorrect',mantras:[]},{action:6,tile:26},{action:3,virtue:data.virtues[v],mantras:Array(3).fill(data.mantras[v])}]},s=>{s.position.x=data.shrineX[v];s.position.y=data.shrineY[v];s.shrineQuestBitmap=mask&1?1<<v:0;s.shrineVisitedBitmap=mask&2?255:0;s.shrineDestroyed=Array(8).fill(128);s.gold=199;s.karma=98;});
}
for(const quest of [0,1])for(const floor of [0,255])add('shrine-flow',{data:shrineData,records:ds['MISCMSG.DAT'],actions:[{action:5},{action:5}]},s=>{s.position={location:0,floor,x:233,y:235};s.shrineQuestBitmap=quest;});
for(let mask=0;mask<16;mask++)add('native-roundtrip',{},s=>{Object.assign(s,structuredClone(nativeInitial));s.questFlags={'game-won':!!(mask&1),'in-doom':true,'unknown:future':true,'word-spoken:40':!!(mask&2)};s.shards.falsehood=!!(mask&4);s.lbArtifacts.crown=!!(mask&8);s.shrineQuestBitmap=mask;s.shrineVisitedBitmap=255-mask;s.shrineDestroyed=[128,0,128,0,128,0,128,0];s.shadowlordSummoned=mask%3;s.shadowlordDoomBits=14;});
const realSmall=JSON.parse(readFileSync('game/assets/maps/smallmaps.json','utf8'));
for(const entry of data.searchObjects.slice(0,113)){
 const floor=realSmall.find((m:any)=>m.id===entry.location)?.floors.find((f:any)=>f.z===entry.floor);
 for(const seed of [1,19])add('world-flow',{seed,data:worldQuestData,...(floor?{tiles:floor.tiles.flat()}:{}),actions:[{kind:'search',dir:0},{kind:'search',dir:0},{kind:'get',dir:0},{kind:'get',dir:0},{kind:'search',dir:0}]},s=>{s.position={location:entry.location,floor:entry.floor,x:entry.x,y:(entry.y+1)&255};});
}
for(let phase=0;phase<8;phase++)for(const location of [0,17])add('world-flow',{seed:7,data:worldQuestData,actions:[{kind:'moon',item:phase},{kind:'search'},{kind:'search'},{kind:'moon',item:phase}]},s=>{s.position={location,floor:0,x:15,y:15};s.moonstones=Array.from({length:8},()=>({x:0,y:0,z:0,location:255,buried:false}));});
for(const box of [false,true])for(const won of [false,true])for(const year of [139,140,200])add('world-flow',{seed:4,data:worldQuestData,records:ds['ENDMSG.DAT'],actions:[{kind:'absorb'},{kind:'absorb'}]},s=>{s.specialItems.woodenBox=box;s.questFlags['game-won']=won;s.time.year=year;});
for(let v=0;v<8;v++)for(const party of [1,2,6])for(let answer=0;answer<5;answer++)add('world-flow',{seed:11,data:worldQuestData,misc:ds['MISCMSG.DAT'],actions:[{kind:'capture'},...Array.from({length:4},(_,i)=>({kind:'answer',word:i===answer?data.mantras[v]:'NO'}))]},s=>{s.characters=Array.from({length:16},(_,i)=>({...s.characters[0],name:i?'Iolo'+i:'Avatar',class:i?'B':'A',partyStatus:i<party?0:255}));s.partySize=party;s.position={location:18,floor:0,x:10,y:10};s.shrineDestroyed=Array.from({length:8},(_,i)=>i<v?255:0);});
const realNpcs=JSON.parse(readFileSync('game/assets/npcs.json','utf8'));
for(const location of [17,18,29])for(const hour of [0,12,20])for(let mask=0;mask<8;mask++)add('world-flow',{seed:5,data:worldQuestData,npcs:realNpcs[location],actions:[{kind:'interior'},{kind:'interior'}]},s=>{s.position={location,floor:0,x:15,y:15};s.time.hour=hour;s.lbArtifacts.crown=!!(mask&1);s.lbArtifacts.sceptre=!!(mask&2);s.specialItems.woodenBox=!!(mask&4);});
for(const location of [1,4,5,17,18,29])for(const here of [-1,0,1,2])for(const day of [1,9,28]){
 const floor=realSmall.find((m:any)=>m.id===location)?.floors.find((f:any)=>f.z===0);if(!floor)throw new Error('Missing urban asset '+location);
 add('world-flow',{seed:31,data:worldQuestData,npcs:realNpcs[location],tiles:floor.tiles.flat(),inspectTiles:true,here,actions:[{kind:'urban'},{kind:'urban'}]},s=>{s.position={location,floor:0,x:15,y:15};s.time.day=day;s.shadowlordLocs=[128,128,128];if(here>=0)s.shadowlordLocs[here]=location;});
}
const combatMaps=JSON.parse(readFileSync('game/assets/maps/combatmaps.json','utf8'));
const enemyDefs=buildEnemyDefs(data,JSON.parse(readFileSync('game/src/core/data/AdditionalEnemyFlags.json','utf8'))).map(d=>({...d,nativeMask:(data.enemyFlags[d.index][0]<<8)|data.enemyFlags[d.index][1]}));
const combat={enemyDefs,combatMaps:combatMaps.slice(0,16).filter((m:any)=>[0,2,8,10].includes(m.index)),attackValues:data.attackValues,attackRangeValues:data.attackRangeValues,defenseValues:data.defenseValues,spellAttackRange:data.spellAttackRange};
// Game indexes arenas directly; the native loader also preserves their original index.
const denseCombat={...combat,combatMaps:Array.from({length:11},(_,i)=>combatMaps[i])};
for(const seed of [1,2,65535])for(let mask=0;mask<8;mask++)add('world-flow',{seed,data:worldQuestData,combat,actions:[{kind:'doom'}]},s=>{bits.forEach((b,i)=>s.questFlags['shadowlord-dead:'+b]=!!(mask&(1<<i)));});
for(const slot of [17,18])for(const seed of [1,17])add('world-flow',{seed,data:worldQuestData,combat,npcs:realNpcs[18],actions:[{kind:'npc-attack',slot}]},s=>{s.position={location:18,floor:3,x:15,y:15};});
for(const box of [false,true])for(const seed of [1,7,65535])add('world-flow',{seed,data:worldQuestData,records:ds['ENDMSG.DAT'],combat:{...combat,combatMaps:[{...combatMaps[127],sourceIndex:127}]},actions:[{kind:'fixed',map:127},...Array.from({length:4},()=>({kind:'battle-move',dir:3})),{kind:'battle-end'},{kind:'battle-end'}]},s=>{s.specialItems.woodenBox=box;});
const endgameAsset=JSON.parse(readFileSync('game/assets/endgame.json','utf8'));
for(const box of [false,true])for(const name of ['Avatar',' Iolo ',''])add('world-flow',{seed:2,data:worldQuestData,records:ds['ENDMSG.DAT'],endgameText:{dialogue:endgameAsset.dialogue.records,narration:endgameAsset.narration.pages},actions:[{kind:'absorb'},{kind:'absorb'}]},s=>{s.specialItems.woodenBox=box;s.characters[0].name=name;});
for(const tile of [5,111,112,115,127,128])add('world-flow',{special:true,seed:7,data:worldQuestData,tiles:Array(1024).fill(tile),inspectTiles:true,actions:[{kind:'use',item:20},{kind:'use',item:20}]},s=>{s.position={location:17,floor:0,x:15,y:15};});
for(const transport of ['foot','ship','horse','carpet'])add('world-flow',{special:true,seed:3,data:worldQuestData,actions:[{kind:'use',item:33},{kind:'use',item:33}]},s=>{s.transport=transport;});
for(const phase of [0,7])for(const floor of [-1,0,1])add('world-flow',{special:true,seed:7,data:worldQuestData,actions:[{kind:'moon',item:phase},{kind:'search'},{kind:'search'}]},s=>{s.position={location:17,floor,x:15,y:15};s.moonstones=Array.from({length:8},()=>({x:0,y:0,z:0,location:255,buried:false}));});
for(const location of [17,18,29])for(const type of [14,27,181,182]){
 const npc=realNpcs[location].find((n:any)=>n.type===type);if(!npc)continue;
 for(const seed of [1,17]){const floor=npc.z[0]===255?-1:npc.z[0];const asset=realSmall.find((m:any)=>m.id===location)?.floors.find((f:any)=>f.z===floor);if(!asset)throw Error('Missing plot floor');
 add('world-flow',{pickup:true,seed,karma:ds["KARMA.DAT"],data:worldQuestData,npcs:realNpcs[location],tiles:asset.tiles.flat(),actions:[{kind:'interior'},{kind:'get',dir:0},{kind:'interior'},{kind:'get',dir:0}]},s=>{s.position={location,floor,x:npc.x[0],y:npc.y[0]+1};s.time.hour=0;});
 }
}
for(const karma of [0,19,20,74,75,99])for(const food of [0,10])add('world-flow',{refuge:true,seed:9,data:worldQuestData,karma:ds['KARMA.DAT'],actions:[{kind:'refuge-check'},{kind:'refuge-check'},{kind:'refuge-check'},{kind:'refuge-resolve'}]},s=>{s.characters[0].status='D';s.characters[0].currentHp=0;s.karma=karma;s.food=food;s.timeSpell='T';s.torchTurns=55;s.lightSpellMins=77;});
for(const location of [1,5])for(const gold of [0,9,10,101])for(const pay of [false,true])for(const quietly of [false,true]){
 const npc=realNpcs[location].find((n:any)=>n.type===112&&n.dialogNumber===255&&n.aiTypes[1]===4);
 const floor=realSmall.find((m:any)=>m.id===location).floors.find((f:any)=>f.z===0);
 add('world-flow',{guard:true,seed:13,data:worldQuestData,combat,npcs:realNpcs[location],tiles:floor.tiles.flat(),misc:ds['MISCMSG.DAT'],karma:ds['KARMA.DAT'],actions:[{kind:'check-guard'},{kind:'tribute',agree:pay},{kind:'arrest',agree:quietly}]},s=>{s.position={location,floor:0,x:npc.x[1],y:npc.y[1]+1};s.time.hour=10;s.gold=gold;});
}
for(const badge of [false,true])for(const word of ['IMPE','impeRIUM',' NO ','ıMPE'])add('world-flow',{guard:true,seed:3,data:worldQuestData,npcs:realNpcs[18],misc:ds['MISCMSG.DAT'],karma:ds['KARMA.DAT'],actions:[{kind:'check-guard'},{kind:'password',word},{kind:'answer',word:'AHM'}]},s=>{s.position={location:18,floor:0,x:20,y:25};s.time.hour=10;s.timeSpell=badge?String.fromCharCode(29):undefined;s.shrineDestroyed=Array(8).fill(0);});
const travelGaps:string[]=[];
for(let v=0;v<8;v++){
 const gx=data.shrineX[v],gy=data.shrineY[v],goal=gy*256+gx;
 if(realOverworld[gy][gx]!==25){travelGaps.push(`Shrine ${v}: reference coordinates do not identify an overworld shrine tile`);continue;}
 const queue=[goal],parent=new Map<number,number>(),depth=new Map([[goal,0]]);let start=goal;
 for(let head=0;head<queue.length;head++){const key=queue[head]!,d=depth.get(key)!;start=key;if(d>=40)break;
  const x=key&255,y=key>>8;for(const [dx,dy] of ([[0,-1],[0,1],[1,0],[-1,0]] as const)){const nx=(x+dx+256)&255,ny=(y+dy+256)&255,k=ny*256+nx,t=realOverworld[ny][nx];if(depth.has(k)||t<5||t>15||!isPassable(t,'foot'))continue;parent.set(k,key);depth.set(k,d+1);queue.push(k);}
 }
 if(start===goal){travelGaps.push(`Shrine ${v}: no dry walkable approach in this foot-only route search`);continue;}
 const actions:any[]=[];let at=start;while(at!==goal){const to=parent.get(at)!;const x=at&255,y=at>>8,tx=to&255,ty=to>>8;const dir=tx===x?(((y-1)&255)===ty?0:1):(((x+1)&255)===tx?2:3);actions.push({kind:'move',dir});at=to;}
 actions.push({kind:'enter'},{kind:'shrine-answer',word:data.virtues[v],mantra:data.mantras[v]});
 add('world-flow',{travel:true,seed:11,realOverworld:true,data:worldQuestData,misc:ds['MISCMSG.DAT'],karma:ds['KARMA.DAT'],actions},s=>{s.position={location:0,floor:0,x:start&255,y:start>>8};s.shrineDestroyed=Array(8).fill(0);});
}
for(const hour of [0,1,4,12,20,23])for(const destination of [0,17,255])add('world-flow',{gate:true,seed:3,data:{...worldQuestData,moonPhases:data.moonPhases},townTiles:realSmall.find((m:any)=>m.id===17).floors.find((f:any)=>f.z===0).tiles.flat(),actions:[{kind:'move',dir:0}]},s=>{s.position={location:0,floor:0,x:15,y:16};s.time.hour=hour;s.time.minute=0;s.feluccaPhase=49;s.trammelPhase=49;s.prevHour=hour;s.moonstones=Array.from({length:8},(_,i)=>({x:i===0?15:10,y:i===0?15:10,z:0,location:i===0?0:i===1?destination:255,buried:i<2}));});
for(const slot of [1,2,3])for(const sceptre of [false,true])add('world-flow',{combat,seed:9,data:worldQuestData,npcs:realNpcs[29],actions:[{kind:'npc-attack',slot},{kind:'battle-sceptre'}]},s=>{s.position={location:29,floor:0,x:15,y:15};s.lbArtifacts.sceptre=sceptre;});
for(const seed of [1,7,13])for(const hp of [1,100])add('world-flow',{combat,seed,data:worldQuestData,karma:ds['KARMA.DAT'],npcs:realNpcs[18],actions:[{kind:'npc-attack',slot:17},...Array.from({length:24},()=>({kind:'battle-step'})),{kind:'battle-end'}]},s=>{s.position={location:18,floor:3,x:15,y:15};s.characters[0].currentHp=s.characters[0].maxHp=hp;});
// Continue through retained-document and native GAM/sidecar boundaries with new flags.
for(const op of ['roundtrip','native-roundtrip'])for(let mask=0;mask<8;mask++)add(op,{persistence:true},s=>{
 Object.assign(s,structuredClone(nativeInitial));s.questFlags={'unknown:future':true};
 for(let i=0;i<113;i++)if(i%3!==(mask%3))s.questFlags['search:'+i]=!!(mask&1);
 s.specialItems.hmsCape=!!(mask&2);s.specialItems.woodenBox=!!(mask&4);
});
for(const seed of [1,7,65535])for(const contents of [0,8,128,137,149,255])for(const intelligence of [0,20,99])add('world-flow',{chest:true,seed,data:worldQuestData,actions:[{kind:'search',dir:0},{kind:'search',dir:0}]},s=>{
 s.position={location:17,floor:0,x:15,y:15};s.characters[0].intelligence=intelligence;
 s.worldObjects=[{location:17,floor:0,x:15,y:14,tile:257,kind:'chest',contents,trapped:!!(contents&128)}];
});
add('world-flow',{special:true,seed:3,data:worldQuestData,actions:[{kind:'use',item:37},{kind:'use',item:37}]});
for(const seed of [1,17])add('world-flow',{harp:true,seed,data:worldQuestData,npcs:realNpcs[17],tiles:realSmall.find((m:any)=>m.id===17).floors.find((f:any)=>f.z===2).tiles.flat(),actions:[{kind:'interior'},{kind:'move',dir:0},...[6,7,8,9,8,7,8,7,6,7,6,5,3].map(item=>({kind:'note',item})),{kind:'move',dir:0},{kind:'move',dir:0},{kind:'get',dir:2},{kind:'interior'}]},s=>{s.position={location:17,floor:2,x:17,y:14};s.time.hour=0;});
for(let i=0;i<8;i++)for(const word of [data.wordsOfPower[i],data.wordsOfPower[i].toLowerCase(),'wrong'])add('world-flow',{wordsWorld:true,seed:11,realOverworld:true,data:worldQuestData,actions:[{kind:'yell',word},{kind:'yell',word}]},s=>{s.position={location:0,floor:0,x:data.locationsX[32+i],y:(data.locationsY[32+i]+1)&255};});
const realDungeons=JSON.parse(readFileSync('game/assets/maps/dungeons.json','utf8'));
for(const map of realDungeons)for(const field of [false,true]){
 const cells=map.floors.flat(2).map((c:any)=>c.type*16+c.sub),i=cells.findIndex((c:number)=>(c>>4===8)===field);
 if(i<0)continue;
 for(let facing=0;facing<4;facing++){const dx=[0,1,0,-1][facing]!,dy=[-1,0,1,0][facing]!;
 add('world-flow',{dungeon:{cells,floor:i>>6,x:((i&7)-dx+8)%8,y:(((i>>3)&7)-dy+8)%8,facing},seed:1,data:worldQuestData,actions:[{kind:'use',item:20},{kind:'use',item:20}]});}
}
for(const seed of [1,7,13])add('world-flow',{defeat:true,combat,seed,data:worldQuestData,karma:ds['KARMA.DAT'],npcs:realNpcs[18],actions:[{kind:'npc-attack',slot:17},...Array.from({length:200},()=>({kind:'battle-hold'})),{kind:'battle-end'},{kind:'refuge-resolve'}]},s=>{s.position={location:18,floor:3,x:15,y:15};s.characters[0].currentHp=1;});
add('world-flow',{victory:true,combat,seed:7,data:worldQuestData,karma:ds['KARMA.DAT'],npcs:realNpcs[1],actions:[{kind:'npc-attack',slot:realNpcs[1].find((n:any)=>n.type===112).slot},{kind:'battle-fight',repeat:2000},{kind:'battle-end'}]},s=>{s.position={location:1,floor:0,x:15,y:15};s.characters[0].currentHp=s.characters[0].maxHp=30000;s.characters[0].strength=99;s.characters[0].dexterity=30;s.characters[0].weapon=38;});
writeFileSync(dir+'/input.jsonl',inputs.map(x=>JSON.stringify(x)).join('\n')+'\n');
const run=spawnSync(process.argv[2]??'native/core/build-zig/quest_driver.exe',[dir+'/input.jsonl',dir+'/actual.jsonl'],{encoding:'utf8'});
if(run.status!==0)throw new Error(run.stderr||run.error?.message||('Native driver failed: '+run.status));
const actual=readFileSync(dir+'/actual.jsonl','utf8').trim().split('\n').map(x=>JSON.parse(x));
if(actual.length!==expected.length)throw new Error('case count mismatch');
for(let i=0;i<actual.length;i++) {
 actual[i].state=projection(actual[i].state);
 if(actual[i].steps) for(const step of actual[i].steps)step.state=projection(step.state);
 if(!isDeepStrictEqual(actual[i],expected[i])) {writeFileSync(dir+'/mismatch.json',JSON.stringify({input:inputs[i],expected:expected[i],actual:actual[i]},null,2));throw new Error(`Quest mismatch ${i}: ${inputs[i].op}`);}
}
const flows=inputs.filter(q=>q.op==='world-flow');
const report={cases:expected.length,nonterminatingObservations:expected.filter(r=>r.nonterminating).length,counts,
 realShrines:8,realWords:data.wordsOfPower.length,
 scriptedEncounterSequences:flows.filter(q=>q.combat).length,
 realNpcSequences:flows.filter(q=>q.npcs).length,
 combatVictorySequences:expected.filter(r=>r.steps?.some((s:any)=>s.combatEvents?.some((e:any)=>e[1]==='VICTORY!'))).length,
 combatRefugeSequences:expected.filter(r=>r.steps?.some((s:any)=>s.combat&&s.events.some((e:any)=>e.kind==='refuge'))).length,
 partyEscapeSequences:expected.filter(r=>r.steps?.some((s:any)=>s.combat?.actors?.some((a:any)=>a[1]!==255&&a[13]===2))).length,
 combatAbsorptionSequences:expected.filter(r=>r.steps?.some((s:any)=>s.combat&&s.events.some((e:any)=>e.kind==='game-won'))).length,
 longShrineTravelSequences:flows.filter(q=>q.travel).length,
 travelGaps,requiredAssetSkips:0,sizes:run.stderr.trim()};
writeFileSync(dir+(process.env.QUEST_FOCUS?'/coverage-focus.json':'/coverage.json'),JSON.stringify(report,null,2));console.log(report);












