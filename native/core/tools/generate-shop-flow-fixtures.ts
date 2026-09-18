import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
import {deepStrictEqual} from 'node:assert';
import {ShopConsole} from '../../../game/src/ui/shop-console.js';
import {Game} from '../../../game/src/core/game.js';
import * as S from '../../../game/src/core/shops/shops.js';
import {SHOP_TOWNES,shopTownIndex,RUMOR_KEYWORDS} from '../../../game/src/core/shops/shop-tables.js';
import {TAVERN_KEYS,TAVERN_SUBTYPE} from '../../../game/src/core/shops/shoppe-greetings.js';
import {OriginalRng} from '../../../game/src/core/rng-original.js';
import {parseDataOvl} from '../../../extractor/src/parsers/dataovl.js';
import {parseShoppeDat} from '../../../extractor/src/parsers/shoppe.js';
import {extractCompressedWords} from '../../../extractor/src/parsers/tlk.js';
import {initial,digest,Hash} from './shop-test-state.js';
const data=JSON.parse(readFileSync('game/assets/data.json','utf8')),pool=JSON.parse(readFileSync('game/assets/shoppe.json','utf8')),keepers=JSON.parse(readFileSync('game/src/core/data/ShoppeKeeperMap.json','utf8').replace(/^\uFEFF/,''));
const raw=readFileSync('original/u5/ultima5/DATA.OVL');
const parsed:any=parseDataOvl(raw);for(const key of ['equipmentBasePrices','weaponsSoldByMerchants','reagentBasePrices','reagentQuantities','healPrices','curePrices','resurrectPrices','storeNames','shoppeKeeperNames'])deepStrictEqual(data[key],parsed[key]);
deepStrictEqual(pool,parseShoppeDat(readFileSync('original/u5/ultima5/SHOPPE.DAT'),extractCompressedWords(raw)));
const types=Object.keys(SHOP_TOWNES) as S.ShopType[];
const shops=Object.entries(keepers).map(([id,k]:[string,any])=>{const location=Number(Object.entries(S.LOCATION_NAMES).find(([,name])=>name===k.Location)![0]);return {location,type:k.ShoppeKeeperType as S.ShopType,...S.shoppeKeeperAt(location,k.ShoppeKeeperType,keepers,data.storeNames,data.shoppeKeeperNames)!};});
const texts=['','0','-1','1','99','1000','2garbage','honeymoon','the crown','unknown','the unde','  comp  ','999999999999999'];
texts.push(...RUMOR_KEYWORDS.flatMap(k=>[k,k.toUpperCase()+'more','the '+k,'x'+k+' '+k]),'\u00a0comp\u00a0','\ufeffhone','123456789012 crow','hone comp');
const phases=['closed','greet-yn','blacksmith-pause','menu','buy-list','sell-list','reagent-list','guild-list','ship-menu','healer-need','healer-member','healer-again','inn-again','tavern-menu','tavern-again','wine-list','tavern-drunk-gate','rations-quantity','rumor-text','buy-deal','sell-deal','reagent-deal','guild-deal','horse-deal','ship-take','healer-pay','inn-rest-take','inn-leave-member','inn-leave-take','inn-pickup-member','rumor-deal','buy-full-pause','reagent-full-pause','ship-else','heal-heal','heal-cure','heal-resurrect'];
const actions=['continue','buy','sell','item','member','heal','cure','resurrect','rest','leave','pickup','round','drink','rations','rumor','confirm','decline','cancel','end','text'];
type Action=[number,number];
const a=(name:string,value=0):Action=>[actions.indexOf(name),value];
function output(path:string,text:string){if(process.argv.includes('--check')){if(readFileSync(path,'utf8')!==text)throw Error('Shop flow fixture drift: '+path);}else writeFileSync(path,text);}
const rows:string[]=[];let count=0;const covered=new Set<string>();
function run(shop:typeof shops[number],v:number,sequence:Action[]){
 const g=initial(v);g.position.location=shop.location;const rng=new OriginalRng(v*97+13);let closed=false,waiting='',choose:((i:number)=>void)|undefined,cancel:(()=>void)|undefined,textCb:((text:string)=>void)|undefined,calls=0;const effects:number[]=[];
 const game:any={state:g,shopGreetingRand:(lo:number,hi:number)=>rng.next(lo,hi),shopPostPurchaseDrain:()=>S.postPurchaseDrain(g,(lo,hi)=>rng.next(lo,hi)),stableSpotFree:()=>true,activeMap:{tileAt:()=>149},setVolatileTerrain:(x:number,y:number,t:number)=>effects.push(1,x,y,t),spawnDockShip:(x:number,y:number,flags:number)=>effects.push(2,x,y,flags===130?293:297,99,flags&63),innSleepUntilMorning:()=>S.innNightPass(g,(lo,hi)=>rng.next(lo,hi),()=>calls++),wakeSnapNpcs:()=>effects.push(3)};
  game.rand=(lo:number,hi:number)=>rng.next(lo,hi);
 game.worldObjectAt=()=>undefined;
 game.mapTileWithOverrides=()=>5;
 game.setMapOverride=(x:number,y:number)=>effects.push(4,x,y);
 game.findStableSpot=()=> (Game.prototype as any).findStableSpot.call(game);
 game.stableHorse=(town:number,intel:number)=>(Game.prototype as any).stableHorse.call(game,town,intel);
 game.shopPostPurchaseDrain=()=>Game.prototype.shopPostPurchaseDrain.call(game);
 game.innSleepUntilMorning=()=>Game.prototype.innSleepUntilMorning.call(game);
 game.refreshHourTiles=()=>calls++;
 game.spawnDockShip=(x:number,y:number,flags:number,location:number)=>{Game.prototype.spawnDockShip.call(game,x,y,flags,location);const o=g.worldObjects.at(-1);effects.push(2,o.x,o.y,o.tile,o.hull,o.skiffs);};
 const console:any=new ShopConsole(shop.type,{game,shopData:data,info:v>=60?null:shop,shoppeTexts:v>=30&&v<60?null:pool,message:()=>{},refreshGold:()=>{},armKey:()=>{},armText:(_prefix,_max,cb)=>{waiting=_max===2?'rations-quantity':'rumor-text';textCb=cb;},close:()=>{closed=true;waiting='';},pickMember:(pick,no)=>{waiting=console.healerService?'healer-member':'inn-leave-member';choose=pick;cancel=no;},openInnRegister:(guests,pick,no)=>{waiting='inn-pickup-member';choose=(idx)=>{const list=g.characters.map((c:any,i:number)=>({c,i})).filter((x:any)=>x.c.partyStatus===shop.location);const n=list.findIndex((x:any)=>x.i===idx);if(n>=0)pick(n);};cancel=no;},openArmsPicker:(_rows,pick,no)=>{choose=(id)=>{const ids=console.sellableIds();const n=ids.indexOf(id);if(n>=0)pick(n);};cancel=no;}});
 const hash=()=>{const h=new Hash();h.n(digest(g,{},rng,calls));for(const n of [g.position.x,g.position.y,g.karma,g.drunkTurns])h.n(n);const phase=closed?'closed':waiting||console.phase;const pi=phases.indexOf(phase);if(pi<0)throw Error(phase);h.n(pi);const pend=({'buy-deal':console.pendingBuy,'sell-deal':console.pendingSell,'reagent-deal':console.pendingReagent,'guild-deal':console.pendingGuild,'horse-deal':console.pendingHorse,'ship-take':console.pendingShip,'healer-pay':console.pendingHeal} as Record<string,any>)[phase]??null;const item=closed?-1:pend?.equipId??pend?.slot??pend?.item??pend?.idx??(pend?.kind===undefined?undefined:pend.kind==='frigate'?0:1)??(phase==='inn-leave-take'?console.pendingInnLeave:phase==='rumor-deal'?console.pendingRumor:-1);h.n(item);h.n(pend?.price??(phase==='rumor-deal'?S.rumorPrice(['hone','comp','valo','just','sacr','hono','spir','humi','dece','desp','dest','wron','cove','sham','hyth','crow','scep','amul','fals','hatr','cowa','astr','oppr','brit','resi','unde'][console.pendingRumor]!):0));h.n(pend?.qty??0);for(const n of [+console.purchased,+console.boughtInBuy,+(console.blacksmithFlow==='sell'),+(console.blacksmithFlow==='buy'),+(console.thrownOut||console.buyThrownOut),+console.tavernServed,console.served])h.n(n);const offers:number[][]=[];const intel=g.characters[0]?.intelligence??15,town=shopTownIndex(shop.type,shop.location);
 if(phase==='buy-list')for(const id of console.buyStock())offers.push([id,S.equipmentBuyPrice(data.equipmentBasePrices[id]??0,intel),id===27||id===29?99:1]);
 else if(phase==='sell-list')for(const id of console.sellableIds())offers.push([id,S.equipmentSellPrice(data.equipmentBasePrices[id]??0,intel),g.equipmentQuantities[id]]);
 else if(phase==='reagent-list'){for(let id=0;id<8;id++){const p=S.reagentPriceAt(town,id,intel,data.reagentBasePrices);if(p!==null)offers.push([id,p,S.reagentGrantQty(town,id,data.reagentQuantities)]);}}
 else if(phase==='guild-list')for(let id=0;id<3;id++)offers.push([id,S.guildPrice(town,id as S.GuildItem,intel),id+3]);
 else if(phase==='ship-menu')for(let id=0;id<2;id++)offers.push([id,S.shipwrightPrice(town,id===0?'frigate':'skiff',intel),1]);
 else if(phase==='wine-list')for(let id=0;id<6;id++)offers.push([id,S.winePrice(id),1]);
 else if(phase==='inn-pickup-member')g.characters.forEach((ch:any,id:number)=>{if(ch.partyStatus===shop.location)offers.push([id,0,1]);});
 else if(['healer-member','inn-leave-member','heal-heal','heal-cure','heal-resurrect'].includes(phase))g.characters.map((ch:any,id:number)=>({ch,id})).filter((x:any)=>x.ch.partyStatus===0).slice(0,6).forEach((x:any)=>offers.push([x.id,0,1]));
 h.n(offers.length);for(const offer of offers)for(const n of offer)h.n(n);
 for(const n of effects)h.n(n);return h.h;};
 console.start();rows.push([shops.indexOf(shop),v,sequence.length,hash()].join(' '));count++;
 for(const [action,value]of sequence){
  if(!closed){const name=actions[action];
   if(waiting&&!['text','member','cancel','end'].includes(name!)){} else if(name==='continue')console.key('Enter');else if(name==='confirm')console.key('y');else if(name==='decline')console.key('n');else if(name==='end'){console.leave();}else if(name==='cancel'){if(waiting==='rumor-text'||waiting==='rations-quantity'){const cb=textCb;waiting='';textCb=undefined;cb?.('');}else if(waiting&&cancel){const f=cancel;waiting='';f();}else if(console.phase==='sell-list')cancel?.();else console.key(console.phase==='wine-list'?' ':'Escape');}
   else if(name==='text'){if(waiting==='rumor-text'||waiting==='rations-quantity'){const cb=textCb;waiting='';textCb=undefined;cb?.(texts[value]!);}}
   else if(name==='member'){if(console.phase.startsWith('heal-')){const index=console.options.findIndex((o:any)=>o.idx===value);if(index>=0)console.key(String.fromCharCode(97+index));}else if(waiting&&choose&&g.characters[value]?.partyStatus===(waiting==='inn-pickup-member'?shop.location:0)){const f=choose;waiting='';choose=undefined;f(value);}}
   else if(name==='item'){
    const p=console.phase;let index=-1;if(p==='sell-list')choose?.(value);else {if(p==='buy-list')index=console.buyStock().indexOf(value);else if(p==='reagent-list'){const slots=Array.from({length:8},(_,i)=>i).filter(i=>S.reagentPriceAt(shopTownIndex(shop.type,shop.location),i,g.characters[0].intelligence,data.reagentBasePrices)!==null);index=slots.indexOf(value);}else if(p==='guild-list'||p==='wine-list')index=value;else if(p==='ship-menu'){console.key(value===0?'f':'s');}if(index>=0&&index<26)console.key(String.fromCharCode(97+index));}
   }else if(['menu','healer-need','tavern-menu'].includes(console.phase)){const keys:any={buy:'b',sell:'s',heal:'h',cure:'c',resurrect:'r',rest:'r',leave:'l',pickup:'p'};if(shop.type==='Barkeeper'){const k=TAVERN_KEYS[TAVERN_SUBTYPE[shopTownIndex(shop.type,shop.location)]!]!;Object.assign(keys,{round:k.round,drink:k.wine,rations:k.rations,rumor:k.chat});}if(keys[name!])console.key(keys[name!]);}
  }
  rows.push([action,value,hash()].join(' '));count++;
 }
}
for(const shop of shops)for(let v=0;v<90;v++){
 const start=[a(shop.type==='Blacksmith'?'continue':'confirm')];
 const town=shopTownIndex(shop.type,shop.location);
 for(const ending of ['decline','cancel','end'])run(shop,v,[a(ending)]);
 if(shop.type==='Blacksmith'){
   run(shop,v,[...start,a('buy'),a('cancel')]);run(shop,v,[...start,a('sell'),a('cancel')]);
 }else if(shop.type==='Healer'){
   run(shop,v,[...start,a('heal'),a('cancel'),a('decline')]);
 }else if(shop.type==='InnKeeper'){
   run(shop,v,[...start,a('leave'),a('cancel'),a('decline')]);run(shop,v,[...start,a('pickup'),a('cancel'),a('decline')]);run(shop,v,[...start,a('rest'),a('decline')]);
 }
 if(shop.type==='Blacksmith'){
  for(const id of S.blacksmithStock(town,data.weaponsSoldByMerchants)){covered.add(shop.index+':buy:'+id);run(shop,v,[...start,a('buy'),a('item',id),a('confirm'),a('continue'),a('item',id),a('decline'),a('cancel')]);}
  for(const id of Array.from({length:48},(_,i)=>i)){covered.add(shop.index+':sell:'+id);run(shop,v,[...start,a('sell'),a('item',id),a('confirm'),a('item',id),a('decline'),a('cancel')]);}
 }else if(shop.type==='MagicSeller'||shop.type==='GuildMaster'||shop.type==='Shipwright'){
  const ids=shop.type==='MagicSeller'?Array.from({length:8},(_,i)=>i).filter(i=>data.reagentBasePrices[town*8+i]>0):shop.type==='GuildMaster'?[0,1,2]:[0,1];
  for(const id of ids){covered.add(shop.index+':buy:'+id);run(shop,v,[...start,a('item',id),a('confirm'),a('continue'),a('confirm'),a('item',id),a('decline'),a('cancel')]);}
 }else if(shop.type==='HorseSeller')run(shop,v,[...start,a('confirm'),a('end')]);
 else if(shop.type==='Healer'){for(const remedy of ['heal','cure','resurrect'])for(const member of [0,1,5]){covered.add(shop.index+':'+remedy);run(shop,v,[...start,a(remedy),a('member',member),a('confirm'),a('confirm'),a(remedy),a('member',member),a('decline'),a('cancel')]);}}
 else if(shop.type==='InnKeeper'){for(const service of ['rest','leave','pickup']){covered.add(shop.index+':'+service);run(shop,v,[...start,a(service),a('member',1),a('confirm'),a('confirm'),a(service),a('member',5),a('decline'),a('cancel')]);}}
 else {for(const service of ['round','drink','rations']){covered.add(shop.index+':'+service);run(shop,v,[...start,a(service),a('text',v%7),a('item',v%6),a('confirm'),a('rumor'),a('text',7+v%6),a('confirm'),a('confirm'),a('drink'),a('item',v%6),a('confirm'),a('drink'),a('item',v%6),a('confirm'),a('drink'),a('decline'),a('item',v%6),a('cancel')]);}}
}
for(const shop of shops.filter(s=>s.type==='Barkeeper'))for(const v of [4,34,64])for(let text=0;text<texts.length;text++)run(shop,v,[a('confirm'),a('round'),a('confirm'),a('rumor'),a('text',text),a('confirm'),a('end')]);
mkdirSync('native/core/build-shops',{recursive:true});output('native/core/build-shops/flow.txt',rows.join('\n')+'\n');
let inc='// Generated real assets, intentionally ignored.\n';
for(const key of ['equipmentBasePrices','weaponsSoldByMerchants','reagentBasePrices','reagentQuantities','healPrices','curePrices','resurrectPrices'])inc+=`static const int32_t ${key}[]={${data[key].flat().join(',')}};\n`;
inc+='static const ShopRecord records[]={\n'+shops.map(s=>`{${s.location},ShopType::${s.type},${s.index},${JSON.stringify(s.shopName.trim())},${JSON.stringify(s.keeperName.trim())}}`).join(',\n')+'\n};\n';
inc+='static const bool present[]={'+pool.map((s:string)=>s?'true':'false').join(',')+'};\n';
inc+='static const char16_t *texts[]={'+texts.map(s=>'u'+JSON.stringify(s)).join(',')+'};\n';
output('native/core/build-shops/assets.inc',inc);
output('native/core/build-shops/coverage.json',JSON.stringify({shops:shops.length,paths:covered.size,snapshots:count,rumorKeywords:RUMOR_KEYWORDS.length,categories:Object.fromEntries(types.map(type=>[type,[...covered].filter(key=>shops.find(s=>s.index===Number(key.split(':')[0]))?.type===type).length])),assetSkips:[]},null,2));
console.log(`${count} shop flow snapshots, ${shops.length} real shops, ${covered.size} offering/service paths`);
