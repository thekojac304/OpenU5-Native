import {readFileSync,writeFileSync} from 'node:fs';
import {GEM_CATEGORY} from '../../../game/src/skin/fiel/gemmap-overworld.js';
const text='// Generated from GEM_CATEGORY (LOOKOBJ 0xf88, DATA.OVL 0x1d2a); see\n'+
 '// game/src/skin/fiel/gemmap-overworld.ts. Regenerate with\n'+
 '// generate-gem-category-table.ts, do not hand-edit.\n'+
 'static constexpr uint8_t kGemCategoryTable[256]={\n'+
 GEM_CATEGORY.map((v,i)=>`${i%16===0?'    ':''}${v},${i%16===15?'\n':' '}`).join('')+
 '\n};\n';
const path=new URL('../src/gem_category.inc',import.meta.url);
if(process.argv.includes('--check')){if(readFileSync(path,'utf8')!==text)throw Error('Gem category table drift');}
else writeFileSync(path,text);
