/** Census of shipped town bed terrain and overlapping NPC schedule posts. */
import {readFileSync,writeFileSync} from "node:fs";
import {parseSmallMaps} from "../../extractor/src/parsers/smallmap.ts";
import {parseAllNpcs} from "../../extractor/src/parsers/npc.ts";
const root="original/u5/ultima5/";
const load=(name:string)=>new Uint8Array(readFileSync(root+name));
const files={castle:load("CASTLE.DAT"),towne:load("TOWNE.DAT"),dwelling:load("DWELLING.DAT"),keep:load("KEEP.DAT")};
const npcs=parseAllNpcs({castle:load("CASTLE.NPC"),towne:load("TOWNE.NPC"),dwelling:load("DWELLING.NPC"),keep:load("KEEP.NPC")});
const maps=parseSmallMaps(files);
const out:string[]=["location_id,location,floor,x,y,right_hex,north_hex,south_hex,west_hex,npc_schedule_posts"];
const counts=new Map<string,number>();
for(const loc of maps)for(const floor of loc.floors)for(let y=0;y<32;y++)for(let x=0;x<32;x++){
  if(floor.tiles[y]![x]!==0xab)continue;
  const h=(v:number|undefined)=>v===undefined?"edge":v.toString(16).padStart(2,"0");
  const overlaps=npcs[loc.id]!.flatMap(n=>n.type&&n.x.some((nx,i)=>nx===x&&n.y[i]===y&&n.z[i]===floor.z)?[n.slot+":"+h(n.type)]:[]).join("|");
  out.push([loc.id,loc.name,floor.z,x,y,h(floor.tiles[y]![x+1]),h(floor.tiles[y-1]?.[x]),h(floor.tiles[y+1]?.[x]),h(floor.tiles[y]![x-1]),overlaps].join(","));
  counts.set(loc.name,(counts.get(loc.name)||0)+1);
}
writeFileSync("re/verified/batch47-bed-tiles/shipped-left-beds.csv",out.join("\n")+"\n");
console.log("0xab bed heads:",out.length-1,"locations:",counts.size);
console.log([...counts].map(([n,c])=>n+"="+c).join(", "));
console.log("0x11a cannot occur in 8-bit town map cells (max 0xff).");

const type1a=Object.entries(npcs).flatMap(([loc,slots])=>slots.filter(n=>n.type===0x1a).map(n=>loc+":"+n.slot));
console.log("NPC type byte 0x1a:",type1a.length,type1a.join(","));
