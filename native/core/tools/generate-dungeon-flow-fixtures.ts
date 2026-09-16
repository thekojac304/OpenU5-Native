import {readFileSync,writeFileSync} from 'node:fs';
import * as dc from '../../../game/src/core/dungeon/dungeon-cmds.js';
import {DungeonState,type DungeonData} from '../../../game/src/core/dungeon/dungeon.js';
import {OriginalRng} from '../../../game/src/core/rng-original.js';
import {Game} from '../../../game/src/core/game.js';
const root=new URL('../',import.meta.url),check=process.argv.includes('--check');
const maps:DungeonData[]=JSON.parse(readFileSync(new URL('../../../game/assets/maps/dungeons.json',import.meta.url),'utf8'));
const data=JSON.parse(readFileSync(new URL('../../../game/assets/data.json',import.meta.url),'utf8'));
function output(path:string,text:string){const u=new URL(path,root);if(check){if(readFileSync(u,'utf8')!==text)throw Error(`drift ${path}`);}else writeFileSync(u,text);}
class Hash{h=2166136261;n(v:number){for(let i=0;i<4;i++)this.h=Math.imul(this.h^((v>>>(i*8))&255),16777619)>>>0;}s(s:string){this.n(s.length);for(const c of s)this.h=Math.imul(this.h^c.charCodeAt(0),16777619)>>>0;}}
const facing=['north','east','south','west'] as const,dirs=['east','west','south','north'] as const;
const rows:string[]=[];
for(let mi=0;mi<8;++mi)for(let v=0;v<32;++v)for(let a=0;a<11;++a){const rng=new OriginalRng(mi*3011+v*97+a),fx:number[]=[];
 const state:any={position:{location:0,floor:v&1?255:0,x:10,y:20},characters:[{name:'A',status:'G',currentHp:100,maxHp:100,dexterity:20,partyStatus:0,ring:255}],partySize:1,activeCharacter:255,food:100,torchTurns:100,time:{year:139,month:1,day:1,hour:5,minute:59},turnsSinceStart:0,prevHour:5,lightSpellMins:0,drunkTurns:0,timeSpell:v&2?'Q':'',timeSpellTurns:255,dungeonRoomsCleared:Array(14).fill(0)};
 let ds:DungeonState|null=null;const effect=(n:number)=>fx.push(n,rng.getSeed());
 const ctx:any={state,rand:(lo:number,hi:number)=>rng.next(lo,hi),liveRng:rng,dungeons:maps,locationsX:data.locationsX,locationsY:data.locationsY,getDungeonState:()=>ds,setDungeonState:(d:DungeonState|null)=>ds=d,locationNameBanner:(_:number,ev:any[])=>ev.push({kind:'message',text:'BANNER'}),clearOverworldEnemies:()=>effect(0),hydrateUnderworldPlot:()=>effect(1),startDungeonRoomCombat:(n:number)=>[{kind:'message',text:'room:'+n}],startDungeonCorridorCombat:(cause:string)=>[{kind:'message',text:'corridor:'+cause}],checkDoomRescue:()=>{effect(2);return [];},checkRefuge:()=>{effect(3);return [];}};
 function snap(events:any[]){const h=new Hash(),d=ds as DungeonState|null;for(const n of [state.position.location,state.position.floor,state.position.x,state.position.y,rng.getSeed(),state.time.year,state.time.month,state.time.day,state.time.hour,state.time.minute,state.turnsSinceStart,state.food,state.torchTurns,state.prevHour,state.timeSpell?.charCodeAt(0)||0,state.timeSpellTurns,state.characters[0].currentHp,state.characters[0].status.charCodeAt(0),+!!d])h.n(n);
  if(d){const p=d.pos,w=d.wanderer;for(const n of [p.dungeon,p.floor,p.x,p.y,facing.indexOf(p.facing),w.bank,w.type,w.x,w.y,w.floor,w.attr,+w.hidden,w.prevX,w.prevY,(d as any).quicknessToggle])h.n(n);for(let f=0;f<8;f++)for(let y=0;y<8;y++)for(let x=0;x<8;x++){const c=d.cellAt(f,x,y);h.n(c.type*16+c.sub);}}
  for(const e of events){h.s(e.kind);h.s(e.text??e.sfx?.id??'');h.n(e.dungeonId??-1);}for(const n of fx)h.n(n);fx.length=0;return h.h;
 }
 const hashes=[snap(dc.enterDungeon(ctx,maps[mi]!.location,state.position.floor))];
 const d=ds as unknown as DungeonState;d.pos.floor=v%8;d.pos.x=(v>>3)&7;d.pos.y=(v*3)&7;d.pos.facing=facing[v%4]!;
 const cmds=['forward','back','left','right','turnAround','klimb','pass','attack'] as const;
 const ev=a<8?dc.dungeonCommand(ctx,cmds[a]!,v&1?'up':'down'):a===8?dc.dungeonMagicChangeLevel(ctx,-1):a===9?dc.dungeonMagicChangeLevel(ctx,1):dc.dungeonSpellTurn(ctx);
 hashes.push(snap(ev));
 if(ds){const saved=ds as DungeonState;saved.wanderer.type=20;const m=saved.buildCorridorArena([1,3,8,16][v%4]!);const h=new Hash();h.n(rng.getSeed());for(const n of m.tiles.flat())h.n(n);for(const dir of dirs)for(const p of m.playerStarts[dir]){h.n(p.x);h.n(p.y);}h.n(m.units.length);for(const u of m.units){h.n(u.x);h.n(u.y);h.n(u.sprite);}hashes.push(h.h);}else hashes.push(0);
 const cause=v%3===0?null:v%3===1?'ambush':'attack';const mock:any={state,dungeonState:ds,liveRng:rng,combat:{victory:!!(v&4),combatants:[],finalSeed:rng.getSeed(),escapeFloorDelta:v%3===0?null:v%3===1?-1:1,lastEscapeBorder:dirs[v%4]},roomCombatEntryCell:null,corridorCombatCause:cause,exitDungeonTo:(ev:any[],under:boolean)=>{dc.exitDungeonTo(ctx,ev,under);mock.dungeonState=ds;},checkRefuge:ctx.checkRefuge};
 hashes.push(snap(Game.prototype.endCombat.call(mock)));ds=mock.dungeonState;
 rows.push([mi,v,a,...hashes].join(' '));
}
output('fixtures/dungeon-flow.txt',rows.join('\n')+'\n');output('fixtures/dungeon-locations.txt',[...data.locationsX,...data.locationsY].join(' ')+'\n');console.log(`${rows.length*4} dungeon orchestration/corridor/aftermath snapshots`);



