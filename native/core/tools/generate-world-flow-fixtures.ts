import {deepStrictEqual} from 'node:assert';
import {parseCombatMaps} from '../../../extractor/src/parsers/combatmap.js';
import {readFileSync,writeFileSync} from 'node:fs';
import {Combat,type CombatMapData} from '../../../game/src/core/combat/combat.js';
import {buildEnemyDefs} from '../../../game/src/core/combat/enemies.js';
import {roomEntryDirectionFor} from '../../../game/src/core/combat/roomEntry.js';
import {board,exitTransport} from '../../../game/src/core/world/transport.js';
const root=new URL('../',import.meta.url),check=process.argv.includes('--check');
function json(path:string){return JSON.parse(readFileSync(new URL(path,import.meta.url),'utf8'));}
function output(path:string,text:string){const u=new URL(path,root);if(check){if(readFileSync(u,'utf8')!==text)throw Error(`drift ${path}`);}else writeFileSync(u,text);}
const data=json('../../../game/assets/data.json'),defs=buildEnemyDefs(data,json('../../../game/src/core/data/AdditionalEnemyFlags.json'));
const maps:CombatMapData[]=json('../../../game/assets/maps/combatmaps.json');
output('src/loot_names.inc','// Generated from game/src/core/data/longEquipNames.json.\nstatic const char *kLootNames[] = {'+json('../../../game/src/core/data/longEquipNames.json').names.map((n:string)=>JSON.stringify(n)).join(',')+'};\n');
deepStrictEqual(maps,parseCombatMaps(readFileSync(new URL('../../../original/u5/ultima5/BRIT.CBT',import.meta.url)),readFileSync(new URL('../../../original/u5/ultima5/DUNGEON.CBT',import.meta.url))));
const dirs=['east','west','south','north'] as const,facings=['north','east','south','west'] as const;
class Hash {h=2166136261;n(v:number){for(let i=0;i<4;i++)this.h=Math.imul(this.h^((v>>>(i*8))&255),16777619)>>>0;}s(s:string){this.n(s.length);for(const c of s)this.h=Math.imul(this.h^c.charCodeAt(0),16777619)>>>0;}}
output('fixtures/fixed-maps.txt',maps.map(m=>[m.index,...m.tiles.flat(),...dirs.flatMap(d=>[m.playerStarts[d].length,...m.playerStarts[d].flatMap(p=>[p.x,p.y])]),m.units.length,...m.units.flatMap(u=>[u.x,u.y,u.sprite]),m.triggers.length,...m.triggers.flatMap(t=>[t.sprite,t.at.x,t.at.y,t.pos1.x,t.pos1.y,t.pos2.x,t.pos2.y])].join(' ')).join('\n')+'\n');
output('fixtures/fixed-enemies.txt',defs.map(d=>[d.index,d.str,d.dex,d.int,d.armour,d.damage,d.hp,d.attackRange,d.treasure,d.maxPerMap,(data.enemyFlags[d.index][0]<<8)|data.enemyFlags[d.index][1],d.moveClass,+d.doesNotMove].join(' ')).join('\n')+'\n');
function snap(c:any,ev:any[]){const h=new Hash();for(const n of [c.finalSeed,c.actionCount,c.scanIdx,c.currentActor?.id??-1,+c.ended,+c.victory,+c.over,c.spoilChests,dirs.indexOf(c.escapeBorder),c.escapeFloorDelta??0,c.combatants.length])h.n(n);
 const statuses=['active','dead','fled','sleeping','charmed','absorbed'];
 for(const a of c.combatants)for(const n of [a.id,a.charIdx??255,a.enemyDef?.index??-1,a.x,a.y,a.hp,a.maxHp,a.str,a.dex,a.int,a.defense,a.attack,a.attackRange,statuses.indexOf(a.status),+a.isFleeing,a.speed,a.counter,+a.sleeping,+a.draggedUnder,+a.charmed,+a.invisible])h.n(n);
 for(const row of c.mapTiles)for(const n of row)h.n(n);
 const loot=Array(121).fill(-1),chests=Array(121).fill(-1);for(const [k,n] of c.lootLayer){const [x,y]=k.split(':').map(Number);loot[y*11+x]=n;}for(const [k,n] of c.chestContents){const [x,y]=k.split(':').map(Number);chests[y*11+x]=n;}for(const n of [...loot,...chests])h.n(n);
 h.n(c.fieldSlots.length);for(const f of c.fieldSlots){h.n(f.x);h.n(f.y);h.n(f.tile);}
 const piles=Array.from(c.lootPiles.entries()).flatMap(([key,p]:any)=>p.map((v:any)=>({key,...v})));h.n(piles.length);for(const p of piles){const [x,y]=p.key.split(':').map(Number);for(const n of [x,y,p.id,p.qty])h.n(n);}
 for(const n of [c.opts.state.gold,c.opts.state.food,c.opts.state.keys,c.opts.state.gems,c.opts.state.torches,c.opts.state.torchTurns,c.spoilGold,...c.opts.state.equipmentQuantities,...c.opts.state.potionQuantities,...c.opts.state.scrollQuantities])h.n(n);for(const e of ev){h.s(e.kind);h.s(e.text??'');for(const n of [e.actorId??-1,e.targetId??-1,e.x??-1,e.y??-1,e.damage??-1])h.n(n);}return h.h;
}
const rows:string[]=[];
for(let mi=0;mi<128;mi++)for(let f=0;f<4;f++)for(let variant=0;variant<8;variant++){
 const seed=(mi*811+f*7103+variant*313)&65535,map=maps[mi]!,entry=roomEntryDirectionFor(map.playerStarts,facings[f]!);
 const state:any={characters:Array.from({length:2},(_,i)=>({name:`MEM${i}`,class:'A',status:'G',currentHp:100,maxHp:100,strength:20,dexterity:30-i*5,intelligence:20,exp:0,level:8,helmet:255,armor:255,weapon:255,shield:255,ring:255,amulet:255,partyStatus:0})),partySize:2,activeCharacter:255,gold:0,food:100,keys:0,gems:0,torches:0,torchTurns:0,karma:50,specialItems:{},potionQuantities:Array(8).fill(0),scrollQuantities:Array(8).fill(0),equipmentQuantities:Array(256).fill(0)};
 const c:any=new Combat({map,entryDirection:entry,party:state.characters.map((record:any,charIdx:number)=>({record,charIdx,weapons:[{id:255,attack:1,range:1}]})),enemies:{fixedFromMap:true,defs},seed,state,roomCombat:true,dungeonFloor:variant});
 const hashes=[snap(c,[])];
 hashes.push(snap(c,c.playerKlimbEscape()));
 hashes.push(snap(c,c.playerEscape(dirs[f]!)));
 hashes.push(snap(c,c.playerPass()));
 const unit=map.units.find(u=>u.sprite>0&&u.sprite<16);for(let a=0;a<9;a++){const actor=c.combatants[0];actor.x=unit?.x??5;actor.y=unit?.y??5;actor.status='active';c.currentActor=actor;c.ended=false;hashes.push(snap(c,a===0?c.playerOpen():c.playerGet()));}
 rows.push([mi,f,variant,seed,dirs.indexOf(entry),...hashes].join(' '));
}
output('fixtures/fixed-combat.txt',rows.join('\n')+'\n');
const transport:string[]=[];
for(let tile=0;tile<64;tile++)for(let from=0x10;from<0x30;from++)for(let v=0;v<8;v++){
 const state:any={position:{location:v===7?33:0},shipHull:v===0?1:99,shipSkiffs:v%3,magicCarpets:v%2};
 const b=board(state,tile,from,v===6);const e=exitTransport(state,b.transportTile??from,!!(v&1),!!(v&2),!!(v&4));const h=new Hash();
 for(const r of [b,e]){h.n(+r.ok);h.s(r.message);h.n(r.transportTile??-1);h.n((r as any).dropTile??-1);h.n((r as any).parkedShipTile??-1);h.n(+(r===b&&!!b.warnings?.includes('DANGER: SHIP BADLY DAMAGED!')));h.n(+(r===b&&!!b.warnings?.includes('WARNING: NO SKIFFS ON BOARD!')));}
 for(const n of [state.shipHull,state.shipSkiffs,state.magicCarpets])h.n(n);transport.push([tile,from,v,h.h].join(' '));
}
output('fixtures/transport.txt',transport.join('\n')+'\n');console.log(`${rows.length*13} fixed combat snapshots (128 real arenas, four facings, eight floors); ${transport.length} transport sequences`);




