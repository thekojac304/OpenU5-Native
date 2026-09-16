import {readFileSync,writeFileSync} from 'node:fs';
import {buildSpellDefs} from '../../../game/src/core/magic/spells.js';
import {TIME_PERMITTED_BITS} from '../../../game/src/core/magic/tables.js';
import {castSpell,applyMani,applyVasMani,applyCure,applyAwaken,applyResurrect} from '../../../game/src/core/magic/cast.js';
import {mixSpell} from '../../../game/src/core/magic/mix.js';
import {CombatRng} from '../../../game/src/core/combat/formulas.js';
import {OriginalRng} from '../../../game/src/core/rng-original.js';
const root=new URL('../',import.meta.url),check=process.argv.includes('--check');
function output(path:string,s:string){const url=new URL(path,root);if(check){if(readFileSync(url,'utf8')!==s)throw Error(`drift ${path}`);}else writeFileSync(url,s);}
export const defs=buildSpellDefs(JSON.parse(readFileSync(new URL('../../../game/src/core/data/MagicDefinitions.json',import.meta.url),'utf8')));
const effects=['Light,100','Attack,48','Awaken','Cure','Mani','Poof','Disarm','Repel','Wind','Peer','Summon','Food','Light,255','Attack,49','Field,53,0','Field,51,1','Field,52,2','Blink','Dispel','TimeStatus,80,20','Field,54,3','Ascend','Descend','Reveal','Swarms,31,4','Seal','Animation','FullHeal','Line,1,2','TimeStatus,81,30','Quake','TimeStatus,67,20','TimeStatus,78,10','DeathVision','Charm','Polymorph','Invisible','Attack,50','Illusion','Animation','Line,2,1','Fear','Resurrect','Daemon','Line,4,1','Line,3,2','Gate','TimeStatus,84,10','Animation'];
output('src/magic_tables.inc','// Generated from authoritative MagicDefinitions.json, buildSpellDefs and magic/tables.ts.\nstatic constexpr SpellDef kSpells[] = {\n'+defs.map(d=>`{${[d.key,d.name,d.targetType,d.type,d.timePermitted].map(s=>JSON.stringify(s)).join(',')},${d.circle},${d.reagents.reduce((m,r)=>m|1<<r,0)},${TIME_PERMITTED_BITS[d.index]??0}}`).join(',\n')+'\n};\nstatic constexpr SpellEffect kEffects[] = {\n'+effects.map(e=>'{MagicEffect::'+e+'}').join(',\n')+'\n};\n');
const rows:string[]=[];let cases=0;
for(let spell=0;spell<49;++spell)for(let variant=0;variant<32;++variant)for(let si=0;si<16;++si){
 const seed=(si*4051+variant*31+spell)&65535;
 const caster:any={name:'MEM0',status:['G','P','S','D'][variant%4],class:['A','M','B','F'][variant%4],currentHp:variant%3===0?100:10,maxHp:100,currentMp:variant%5===0?0:30,level:variant%7===0?1:8,intelligence:20,exp:1000};
 const state:any={characters:[caster],spellQuantities:Array(48).fill(variant%11===0?0:3),reagentQuantities:Array(8).fill(variant%3===0?0:10),position:{location:[0,1,18,29,33,128][variant%6]},food:9998,wornCrown:variant%2===0,timeSpell:'',timeSpellTurns:0,lightSpellMins:0,wind:0,windDriftCtr:9};
 const raw=new OriginalRng(seed),draws:number[]=[];const next=raw.next.bind(raw);raw.next=(lo,hi)=>{const v=next(lo,hi);draws.push(lo,hi,v);return v;};
 const rng=new CombatRng(raw);
 const mix=mixSpell(state,defs[spell]!,variant%5-1);
 const r=castSpell(state,caster,defs[spell]!,{location:state.position.location,inCombat:variant%2===1,windArrow:variant%6},rng);
 let heal=-1;
 if(r.effect?.kind==='healTarget')heal=+(r.effect.mode==='full'?applyVasMani(caster):applyMani(caster,rng)>0);
 if(r.effect?.kind==='cure')heal=+applyCure(caster);
 if(r.effect?.kind==='awaken')heal=+applyAwaken(caster);
 if(r.effect?.kind==='resurrect')heal=+applyResurrect(caster,variant*3);
 const messages=['','Absorbed!','Not here!','None mixed!','M.P. too low!'];
 rows.push([spell,variant,seed,+mix.ok,+r.ok,+r.consumed,messages.indexOf(r.message),heal,raw.getSeed(),caster.currentHp,caster.maxHp,caster.currentMp,caster.level,caster.exp,caster.status.charCodeAt(0),state.food,state.timeSpell.charCodeAt(0)||0,state.timeSpellTurns,state.lightSpellMins,state.wind,state.windDriftCtr,...state.spellQuantities,...state.reagentQuantities,draws.length,...draws].join(' '));cases++;
}
output('fixtures/magic.txt',rows.join('\n')+'\n');console.log(`${cases} magic resource/target parity cases`);
