import * as greetings from '../../../game/src/core/shops/shoppe-greetings.js';
import * as tables from '../../../game/src/core/shops/shop-tables.js';
import { writeFileSync, readFileSync } from 'node:fs';
const literal=(v:any):string=>v===null?'-1':Array.isArray(v)?'{'+v.map(literal).join(',')+'}':JSON.stringify(v);
let out='// Generated from shoppe-greetings.ts and shop-tables.ts.\n';
for(const [key,v] of Object.entries({...greetings,...tables})) {
 if(typeof v==='number') out+=`static constexpr int32_t ${key}=${v};\n`;
 else if(Array.isArray(v)&& (typeof v[0]==='number'||typeof v[0]==='string'||v[0]===null)) out+=`static constexpr ${v.some(x=>typeof x==='string')?'const char *':'int32_t'} ${key}[]=${literal(v)};\n`;
 else if(v && typeof v==='object' && !Array.isArray(v) && key!=='SHOP_TOWNES') {
  const rows=Object.keys(tables.SHOP_TOWNES).map(t=>(v as any)[t]??[-1,-1,-1,-1]);
  out+=`static constexpr int32_t ${key}[8][4]=${literal(rows)};\n`;
 }
}
const path='native/core/src/shop_greetings.inc';
if(process.argv.includes('--check')){if(readFileSync(path,'utf8')!==out)throw Error('Shop greeting table drift');}else writeFileSync(path,out);
