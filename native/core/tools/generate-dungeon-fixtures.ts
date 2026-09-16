import {deepStrictEqual} from 'node:assert';
import {parseDungeons} from '../../../extractor/src/parsers/dungeon.js';
import {readFileSync,writeFileSync} from 'node:fs';
import {DungeonState,type DungeonData,type DungeonEvent} from '../../../game/src/core/dungeon/dungeon.js';
import {OriginalRng} from '../../../game/src/core/rng-original.js';
const root=new URL('../',import.meta.url),check=process.argv.includes('--check');
const maps:DungeonData[]=JSON.parse(readFileSync(new URL('../../../game/assets/maps/dungeons.json',import.meta.url),'utf8'));
deepStrictEqual(maps,parseDungeons(readFileSync(new URL('../../../original/u5/ultima5/DUNGEON.DAT',import.meta.url))));
function output(path:string,text:string){const u=new URL(path,root);if(check){if(readFileSync(u,'utf8')!==text)throw Error(`drift ${path}`);}else writeFileSync(u,text);}
const facings=['north','east','south','west'] as const;
const kinds=['message','damage','combat-room','exit-overworld','exit-underworld','moved','turned','floor-changed','combat-corridor','sfx','damage-script'];
class Hash {h=2166136261;n(v:number){for(let i=0;i<4;i++){this.h=Math.imul(this.h^((v>>>(i*8))&255),16777619)>>>0;}}s(s:string){this.n(s.length);for(const c of s)this.h=Math.imul(this.h^c.charCodeAt(0),16777619)>>>0;}}
function snapshot(d:DungeonState,g:any,rng:OriginalRng,events:DungeonEvent[]){const h=new Hash();const p=d.pos,w=d.wanderer;
 for(const n of [p.dungeon,p.floor,p.x,p.y,facings.indexOf(p.facing),w.bank,w.type,w.x,w.y,w.floor,w.attr,+w.hidden,w.prevX,w.prevY,rng.getSeed(),g.activeCharacter])h.n(n);
 for(const c of g.characters){h.n(c.currentHp);h.n(c.status.charCodeAt(0));}
 for(const n of g.dungeonRoomsCleared)h.n(n);h.n((d as any).quicknessToggle);for(let f=0;f<8;f++)for(let y=0;y<8;y++)for(let x=0;x<8;x++)h.n(+d.revealed.has(`${f}:${x}:${y}`));for(const n of [g.gold,g.food,g.keys,g.gems,g.torches,...g.equipmentQuantities,...g.potionQuantities,...g.scrollQuantities])h.n(n);
 for(let f=0;f<8;f++)for(let y=0;y<8;y++)for(let x=0;x<8;x++){const c=d.cellAt(f,x,y);h.n(c.type*16+c.sub);}
 for(const e of events){h.n(kinds.indexOf(e.kind));h.s(e.text??e.sfx??'');h.n(e.slots?.reduce((m,n)=>m|(1<<n),0)??e.amount??e.roomCombatMapIndex??(e.corridorCause==='attack'?1:e.corridorCause==='ambush'?0:-1));h.n(e.charIdx??-1);}
 return h.h;
}
const rows:string[]=[];
output('fixtures/dungeon-maps.txt',maps.map(d=>[d.location,...d.floors.flat(2).map(c=>c.type*16+c.sub)].join(' ')).join('\n')+'\n');
for(let mi=0;mi<maps.length;mi++)for(let cell=0;cell<512;cell++)for(let variant=0;variant<4;variant++){
 const seed=(mi*811+cell*31+variant*7043)&65535,rng=new OriginalRng(seed);
 const g:any={torchTurns:variant<2?30:0,gold:20,food:20,keys:20,gems:20,torches:20,equipmentQuantities:Array(256).fill(0),potionQuantities:Array(8).fill(0),scrollQuantities:Array(8).fill(0),specialItems:{},characters:Array.from({length:6},(_,i)=>({status:['G','S','P','D','G','S'][i],currentHp:20+i,maxHp:50,dexterity:5+i*5,partyStatus:0})),partySize:6,activeCharacter:4,grapple:variant%2===1,timeSpell:['','Q','T',''][variant],dungeonRoomsCleared:Array(14).fill(variant===3?0x55:0)};
 const d=new DungeonState(maps,{dungeon:maps[mi]!.location,floor:cell>>6,x:cell&7,y:(cell>>3)&7,facing:facings[variant]!},rng);
 d.applyClearedRooms(g);d.respawnWanderer();
 const hashes=[snapshot(d,g,rng,[])];
 for(let a=0;a<16;a++){
  let e:DungeonEvent[]=[];if(a===11){d.pos.floor=cell>>6;d.pos.x=cell&7;d.pos.y=(cell>>3)&7;}
  switch(a){case 0:e=d.forward(g);break;case 1:e=d.back(g);break;case 2:e=d.turnLeft();break;case 3:e=d.turnRight();break;case 4:e=d.turnAround();break;case 5:e=d.klimb(g,['up','down','pass',undefined][variant] as any);break;case 6:e=d.pass();break;case 7:e=d.attack();break;case 8:e=d.magicChangeLevel(g,-1);break;case 9:e=d.magicChangeLevel(g,1);break;case 10:e=d.turnTick(g);break;case 11:e=d.openChest(g);break;case 12:e=d.getHere(g);break;case 13:e=d.jimmyHere(g);break;case 14:e=d.drinkFountain(g);break;case 15:e=d.search(g);break;}
  hashes.push(snapshot(d,g,rng,e));
 }
 d.markCurrentRoomCleared(g);hashes.push(snapshot(d,g,rng,[]));
 rows.push([mi,cell,variant,seed,...hashes].join(' '));
}
output('fixtures/dungeon.txt',rows.join('\n')+'\n');console.log(`${rows.length*18} dungeon rule snapshots on ${maps.length} real dungeons`);



