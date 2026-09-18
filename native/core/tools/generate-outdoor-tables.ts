import {readFileSync,writeFileSync} from 'node:fs';
import {TILE_INFO} from '../../../game/src/core/tiles.js';
const text='// Generated from authoritative TILE_INFO; bit 0 land, bit 1 water.\n'+
 'static constexpr uint8_t outdoor_tile_flags[]={\n'+TILE_INFO.map(t=>t?(+t.landEnemyPassable|(+t.waterEnemyPassable<<1)):255).map((v,i)=>`${i%24===0?'    ':''}${v},${i%24===23?'\n':''}`).join('')+'\n};\n';
const path=new URL('../src/outdoor_tiles.inc',import.meta.url);
if(process.argv.includes('--check')){if(readFileSync(path,'utf8')!==text)throw Error('Outdoor tile table drift');}else writeFileSync(path,text);
