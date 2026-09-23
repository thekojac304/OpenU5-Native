import {readFileSync,writeFileSync} from 'node:fs';
import {NpcManager,npcScan,npcBacktrace,type NpcRuntime,type NpcSlot} from '../../../game/src/core/npc/manager.js';
import {OriginalRng} from '../../../game/src/core/rng-original.js';
import {DoorManager} from '../../../game/src/core/world/doors.js';
import {findPath,stepDirection} from '../../../game/src/core/world/pathfind.js';
import {getActiveMap,type WorldData} from '../../../game/src/core/world/map.js';
import type {GameState} from '../../../game/src/core/state.js';
import {Game} from '../../../game/src/core/game.js';
import {resolveStep,locationAt} from '../../../game/src/core/world/movement.js';
const rows:string[]=[];
const counts:number[]=[];
function add(kind:number,input:number[],expected:number[]){rows.push([kind,input.length,expected.length,...input,...expected].join(' '));counts[kind]=(counts[kind]??0)+1;}
function tiles(mode:number){return Array.from({length:32},(_,y)=>Array.from({length:32},(_,x)=>{
 if(x===10&&y===10)return 200;if(x===20&&y===20)return 201;
 if(mode%6===1&&x===16)return 7===y?68:48;
 if(mode%6===2&&((x*17+y*23)%11)<3)return 48;
 if(mode%6===3&&y===12)return 184;
 if(mode%6===4&&x===12)return 144;
 if(mode%6===5&&((x+y)%3===0))return 48;
 return 68;
}));}
function actor(n:NpcRuntime){return [n.slot,n.type,n.dialogNumber,...n.aiTypes,...n.schedX,...n.schedY,...n.schedZ,...n.times,n.x,n.y,n.z,n.location,n.state,n.servedSlot,n.pathIdx,n.stuck,...n.pathBuf];}
for(let scenario=0;scenario<24;scenario++)for(let seed=0;seed<16;seed++){
 const map=tiles(scenario),world:WorldData={overworld:[],underworld:[],smallMaps:new Map([[2,{id:2,name:'test',floors:[-1,0,1,2].filter(z=>scenario!==23||z===0).map(z=>({z,tiles:map}))}]])};
 const s={position:{location:2,floor:0,x:15,y:30},time:{hour:0},npcDead:[]} as unknown as GameState;
 const rng=new OriginalRng(seed*4093),doors=new DoorManager();
 const slots:NpcSlot[]=Array.from({length:3},(_,j)=>({slot:3-j,type:64,dialogNumber:0,aiTypes:[j%3,j%3,0],x:[8+j,18+j,22],y:[8,18,22],z:[0,scenario%4===0?1:0,255],times:[0,6,12,18]}));
 const manager=new NpcManager({2:slots});manager.setRng(rng);manager.enterMap(2,s);
 const list=manager.npcsAt(2,0);
 list.forEach((n,j)=>{n.x=8+j;n.y=8;n.servedSlot=0;n.state=scenario%8+1;n.z=n.state===4?1:n.state===5?-1:0;
   if(scenario>=8&&scenario<16){n.pathIdx=0;n.pathBuf[0]=scenario%3?3:0;n.pathBuf[1]=1;n.stuck=seed%6;}
   if(scenario>=16)n.stuck=[0,1,199,200,204][seed%5]!;
 });
 for(let step=0;step<40;step++){
   s.time.hour=step<28?6:7;s.position.x=step%9===0?list[0]!.x+1:15;s.position.y=step%9===0?list[0]!.y:30;
   const delta=step===10?1:step===20?-1:0;
   const input=[scenario,s.time.hour,s.position.x,s.position.y,rng.getSeed(),list.length,...list.flatMap(actor),s.position.floor,delta];
   const e:any[]=[];
   if(delta)(Game.prototype as any).applyStairStep.call({state:s,activeMap:{tileAt:()=>196},floorExists:(id:number,z:number)=>world.smallMaps.get(id)?.floors.some(f=>f.z===z),refreshHourTiles:()=>{},clearVolatileTerrain:()=>{},hydrateInteriorObjects:()=>{}},delta>0?'north':'south',e);
   manager.tick(s,world,doors);
   const walk=s.npcWalk!;
   // Validate the authoritative manager's persisted projection too.
   if(!walk)throw new Error('missing synchronized walk state');
   add(0,input,[s.position.floor,e.length,rng.getSeed(),...list.flatMap(actor)]);
 }
}
for(let scenario=0;scenario<122;scenario++){
 const grid=new Uint8Array(1056);grid.fill(144,1024);
 for(let i=0;i<1024;i++)grid[i]=(i*17+scenario*29)%19<(scenario%8)?144:0;
 const sx=scenario%32,sy=Math.floor(scenario/4)%32,tx=(scenario*7+9)%32,ty=(scenario*13+11)%32;
 let startX=sx,startY=sy;
 grid[ty*32+tx]=5;grid[sy*32+sx]=70;
 if(scenario===120){ // 29 runs: exercise 16-pair truncation at the start end.
   grid.fill(144);for(let y=2;y<=30;y+=2){for(let x=2;x<=28;x++)grid[y*32+x]=0;if(y<30)grid[(y+1)*32+((y/2)%2?28:2)]=0;}
   startX=2;startY=2;grid[30*32+28]=5;grid[2*32+2]=70;
 }
 if(scenario===121){grid.fill(144);grid[10*32+10]=70;startX=10;startY=10;}
 const input=[startX,startY,...grid];
 const f=npcScan(grid,startY,startX),n={pathIdx:-1,pathBuf:Array(32).fill(0) as number[]};
 let bytes=0;if(f)bytes=npcBacktrace(n,f.y,f.x,grid);
 if(scenario===120&&bytes!==32)throw new Error('Truncation witness did not reach 32 bytes');
 add(1,input,[+(f!==null),f?.x??0,f?.y??0,bytes,n.pathIdx,...n.pathBuf,...grid]);
}
for(let scenario=0;scenario<6;scenario++)for(let seed=0;seed<128;seed++){
 const wrap=seed%4===0,w=wrap?256:32;const small=tiles(scenario);
 const data=Array.from({length:w},(_,y)=>Array.from({length:w},(_,x)=>small[y%32]![x%32]!));
 const map={kind:wrap?'overworld':'small',location:wrap?0:2,floor:0,width:w,height:w,wraps:wrap,edgeFillTile:-1,tileAt:(x:number,y:number)=>data[y]![x]!} as ReturnType<typeof getActiveMap>;
 const from={x:seed%w,y:seed*3%w},to=seed%13===0?from:{x:(seed*7+5)%w,y:(seed*11+7)%w};
 const limit=[0,1,4,32,4000][seed%5]!;const ti=Math.floor(seed/5)%5;const transport=(['foot','horse','carpet','skiff','ship'] as const)[ti]!;
 const p=findPath(map,from,to,transport,{maxNodes:limit,isBlocked:(x,y)=>(x*13+y*19+seed)%47===0});
 add(2,[scenario,seed,+wrap,from.x,from.y,to.x,to.y,limit,ti],[+(p!==null),p?.length??0,...(p??[]).flatMap(q=>[q.x,q.y])]);
 for(const to of [{x:from.x+1,y:from.y},{x:from.x,y:from.y-1},{x:w-1,y:from.y},{x:from.x+2,y:from.y}])add(3,[+wrap,from.x,from.y,to.x,to.y],[['north','south','east','west'].indexOf(stepDirection(from,to,map)??'')]);
}
// Run actual Game methods; hooks represent systems outside this translation.
for(let scenario=0;scenario<240;scenario++) {
 const mode=scenario%6,loc=[2,14,29,0][Math.floor(scenario/6)%4]!,floor=[-1,0,1,255][Math.floor(scenario/24)%4]!;
 const s={position:{location:loc,floor,x:10,y:10},time:{hour:6},drunkTurns:5,npcDead:[],shadowlordLocs:[2,14,128],shadowlordHere:2,
 moonstones:[{x:9,y:11,z:scenario%2?255:1,location:[0,2,255][Math.floor(scenario/6)%3]!,buried:false}]} as unknown as GameState;
 const map=tiles(0),world:WorldData={overworld:[],underworld:[],smallMaps:new Map([2,14,29].map(id=>[id,{id,name:'TEST',floors:[-1,0,1].map(z=>({z,tiles:map}))}]))};
 const events:any[]=[];const position=()=>[s.position.location,s.position.floor,s.position.x,s.position.y];
 const fx=(n:number)=>events.push({kind:'effect',n,p:position()});
 const game:any={state:s,world,data:{locationsX:Array.from({length:32},(_,i)=>i+40),locationsY:Array.from({length:32},(_,i)=>i+80)},volatileTerrainWipe:{},drunkPreRolled:true,
   floorExists:(id:number,z:number)=>world.smallMaps.get(id)?.floors.some(f=>f.z===z)??false,
   refreshHourTiles:()=>fx(5),runContextTurn:()=>{fx(10);return [];},
   locationNameBanner:(id:number,e:any[])=>{if(!(id>=14&&id<=18)&&id!==40)e.push({kind:'message',text:'\n\nTEST\n'});},
   npcManager:{enterMap:()=>fx(0)},overworldEnemies:{clear:()=>fx(1)},doors:{reset:()=>fx(2)},clearVolatileTerrain:()=>fx(3),
   hydrateInteriorObjects:()=>fx(4),applyUrbanShadowlord:()=>fx(7),discardInteriorObjects:()=>fx(8),hydrateUnderworldPlot:()=>fx(9),
   activeMap:{tileAt:()=>196+Math.floor(scenario/6)%4},
 };
 const proto=Game.prototype as any;
 game.loadSmallMap=(id:number,e:any[])=>proto.loadSmallMap.call(game,id,e);
 game.exitToOverworld=(e:any[])=>proto.exitToOverworld.call(game,e);
 let result=0;
 for(let repeat=0;repeat<3;repeat++) {
   if(mode===0)proto.applyStairStep.call(game,['north','south','east','west'][Math.floor(scenario/24)%4],events);
   if(mode===1)proto.klimbLadder.call(game,scenario%12<6?1:-1,events);
   if(mode===2)proto.loadSmallMap.call(game,[2,14,29][Math.floor(scenario/6)%3],events);
   if(mode===3) {try{proto.exitToOverworld.call(game,events);}catch{result=1;}}
   if(mode===4)result=+proto.moonstoneTeleport.call(game,0,events);
   if(mode===5)events.push(...proto.confirmTownExit.call(game,false));
 }
 const trace=events.flatMap(e=>e.kind==='effect'?[10,e.n,...e.p]:e.kind==='map-changed'?[2]:[0,e.text.length,...Array.from(e.text as string).map(c=>c.charCodeAt(0))]);
 add(4,[scenario],[result,...position(),s.drunkTurns??0,+!!game.volatileTerrainWipe,+game.drunkPreRolled,s.shadowlordHere??-1,...trace]);
}
for(const wraps of [false,true])for(const x of [0,1,15,30,31])for(const y of [0,1,15,30,31])for(let dir=0;dir<4;dir++){
 const map={kind:wraps?'overworld':'small',location:wraps?0:2,floor:0,width:wraps?256:32,height:wraps?256:32,wraps,edgeFillTile:68,tileAt:()=>68} as ReturnType<typeof getActiveMap>;
 const s={position:{location:map.location,floor:0,x,y},transport:'foot'} as GameState;
 const result=resolveStep(s,map,(['north','south','east','west'] as const)[dir]!,0);
 add(5,[+wraps,x,y,dir],[+result.exitedMap]);
}
for(let i=0;i<70;i++)add(6,[i],[locationAt([1,2,2,4],[7,8,8],i%6,Math.floor(i/6))??0]);
const output=rows.join('\n')+'\n',path=new URL('../fixtures/travel.txt',import.meta.url);
if(process.argv.includes('--check')){if(readFileSync(path,'utf8')!==output)throw new Error('Travel reference drift');}else writeFileSync(path,output);
console.log(`Travel parity ${rows.length}: ${counts.join(', ')}`);
