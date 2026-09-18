import * as tables from '../../../game/src/core/shops/shop-tables.js';
import { SHOP_UI } from '../../../game/src/core/world/cmd-strings.js';
import { writeFileSync, readFileSync } from 'node:fs';
const names = ['GUILD_PRICES','GUILD_GRANT','HORSE_PRICES','FRIGATE_PRICES','SKIFF_PRICES','SHIP_DOCK_X','SHIP_DOCK_Y','TAVERN_ROUND_PRICE','WINE_PRICES','RATION_BASE','INN_RATE','INN_CAPACITY','INN_ROOM_X','INN_ROOM_Y','RUMOR_PRICES','RUMOR_KEYWORDS','RUMOR_SUBJECTS','RUMOR_GOSSIP_MAP','RUMOR_PLACES'];
let out = '// Generated from authoritative TypeScript by generate-shop-tables.ts.\n';
for (const name of names) {
 const value = (tables as any)[name];
 const nested = Array.isArray(value[0]);
 const type = typeof value.flat()[0] === 'string' ? 'const char *' : 'int32_t';
 const literal = (v: any): string => Array.isArray(v) ? '{'+v.map(literal).join(',')+'}' : JSON.stringify(v);
 out += `static constexpr ${type} ${name}[]${nested ? '['+value[0].length+']' : ''} = ${literal(value)};\n`;
}
for (const [name, values] of Object.entries(tables.SHOP_TOWNES)) out += `static constexpr int32_t towns_${name}[] = {${values.join(',')}};\n`;
for (const [key,value] of Object.entries(SHOP_UI)) out += `static constexpr const char *msg_${key} = ${JSON.stringify(value)};\n`;
const path = 'native/core/src/shop_tables.inc';
if (process.argv.includes('--check')) { if (readFileSync(path,'utf8') !== out) throw Error('Shop table drift'); }
else writeFileSync(path,out);
