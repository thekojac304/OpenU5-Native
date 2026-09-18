import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
import * as S from '../../../game/src/core/shops/shops.js';
import {OriginalRng} from '../../../game/src/core/rng-original.js';
import {initial,digest} from './shop-test-state.js';
const rows:string[]=[];let count=0;
function run(op:number,v:number,a:number,b:number,d:number){const g=initial(v);if(op===18)g.gold=d;const rng=new OriginalRng(v*97+13);let calls=0;const hashes:number[]=[];for(let repeat=0;repeat<3;repeat++){let r:any;switch(op){case 18:case 1:r=S.buyEquipment(g,a,b);break;case 2:r=S.sellEquipment(g,a,b);break;case 3:r=S.buyReagent(g,a,b,d);break;case 4:r=S.buyGuildItem(g,a,b as S.GuildItem,g.characters[0].intelligence);break;case 5:r=S.buyHorse(g,a,g.characters[0].intelligence);break;case 6:r=S.buyShip(g,a,b?'frigate':'skiff',g.characters[0].intelligence);break;case 7:r=S.buyWine(g,a);break;case 8:r=S.buyRations(g,a,g.characters[0].intelligence,b);break;case 9:r=S.healerHeal(g,a,['heal','cure','resurrect'][b] as S.HealerService,d);break;case 10:r=S.innRest(g,a,b);break;case 11:r=S.innLeave(g,a,b);break;case 12:r=S.innPickup(g,a,b,d);break;case 13:r=S.buyTavernRound(g,a,{north:b,south:149},!!d);break;case 14:r=S.payRumor(g,a);break;case 15:S.innNightPass(g,(lo,hi)=>rng.next(lo,hi),()=>calls++);r={};break;case 16:r={bought:S.postPurchaseDrain(g,(lo,hi)=>rng.next(lo,hi))};break;default:throw Error('op');}hashes.push(digest(g,r,rng,calls));count++;}rows.push([op,v,a,b,d,...hashes].join(' '));}
for(let intel=0;intel<=255;intel++)for(const base of [0,1,2,3,7,10,25,99,100,101,190,255,600,753,9999,32767,65535]){rows.push([0,base,intel,S.shopBuyPrice(base,intel),S.shopSellPrice(base,intel)].join(' '));count++;}
for(let v=0;v<60;v++){
 for(let id=0;id<48;id++)for(const price of [0,1,100,9999]){run(1,v,id,price,0);run(2,v,id,price,0);}
 for(let id=0;id<8;id++)for(const qty of [-1,0,1,12,99,1000])for(const price of [0,1,100])run(3,v,id,qty,price);
 for(let town=-1;town<10;town++){for(let item=0;item<3;item++)run(4,v,town,item,0);run(5,v,town,0,0);for(let k=0;k<2;k++)run(6,v,town,k,0);for(const qty of [-1,0,1,2,99,1000])run(8,v,town,qty,0);for(const north of [0,149])for(let house=0;house<2;house++)run(13,v,town,north,house);}
 for(let id=-1;id<7;id++)run(7,v,id,0,0);
 for(const i of [-1,0,1,5,15,16])for(let k=0;k<3;k++)for(const price of [0,10,100,9999])run(9,v,i,k,price);
 for(const loc of [0,2,3,7,20,22,24,255])for(const i of [-1,0,1,5,15,16]){run(10,v,i,loc,0);run(11,v,i,loc,0);run(12,v,0,i,loc);}
 for(let i=-1;i<27;i++)run(14,v,i,0,0);run(15,v,0,0,0);run(16,v,0,0,0);
}
const realData=JSON.parse(readFileSync('game/assets/data.json','utf8'));
for(let v=0;v<60;v++)for(let id=0;id<48;id++){
 const price=S.shopBuyPrice(realData.equipmentBasePrices[id]??0,initial(v).characters[0].intelligence);
 for(const gold of [price,Math.max(0,price-1),0])run(18,v,id,price,gold);
}
mkdirSync('native/core/build-shops',{recursive:true});
const path='native/core/build-shops/helpers.txt',text=rows.join('\n')+'\n';if(process.argv.includes('--check')){if(readFileSync(path,'utf8')!==text)throw Error('shop fixture drift');}else writeFileSync(path,text);
console.log(`${count} shop helper snapshots`);
