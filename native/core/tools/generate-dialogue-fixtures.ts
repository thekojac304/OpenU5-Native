// Reference calls only: no replacement dialogue interpreter or runtime edits.
import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
import {deepStrictEqual} from 'node:assert';
import {Conversation,type TalkScript,type ScriptLine,type QA,type DialogueOutput} from '../../../game/src/core/dialogue/conversation.js';
import {applyDialogueEffect} from '../../../game/src/core/dialogue/effects.js';
import {OriginalRng} from '../../../game/src/core/rng-original.js';
import {Game} from '../../../game/src/core/game.js';
import {NpcManager} from '../../../game/src/core/npc/manager.js';
import {TalkScriptRegistry} from '../../../game/src/core/dialogue/registry.js';
import {TalkConsole} from '../../../game/src/ui/talk-console.js';
import {parseTlkFile,extractCompressedWords} from '../../../extractor/src/parsers/tlk.js';
const root=new URL('../build-dialogue/',import.meta.url);
mkdirSync(root,{recursive:true});
class Writer {
  parts:Buffer[]=[];
  n(v:number){const b=Buffer.alloc(4);b.writeUInt32LE(v>>>0);this.parts.push(b);}
  s(s:string){this.n(s.length);const b=Buffer.alloc(s.length*2);for(let i=0;i<s.length;i++)b.writeUInt16LE(s.charCodeAt(i),i*2);this.parts.push(b);}
  bytes(){return Buffer.concat(this.parts);}
}
const ops=['Text','AvatarsName','NewLine','Rune','Pause','KeyWait','Gold','Change','JoinParty','KarmaPlusOne','KarmaMinusOne','CallGuards','EndConversation','IfElseKnowsName','AskName','Label','StartLabelDefinition','DefineLabel','DoNothingSection','EndScript','Or','StartNewSection','Unknown'];
const effects=['joinParty','gold','giveItem','karma','callGuards','end'];
function line(w:Writer,l:ScriptLine){w.n(l.length);for(const i of l){w.n(i.kind==='text'?0:Math.max(0,ops.indexOf(i.op)));w.s(i.kind==='text'?i.text:'');w.n(i.kind==='op'?i.data??0:0);}}
function qa(w:Writer,qs:QA[]){w.n(qs.length);for(const q of qs){w.n(q.keywords.length);q.keywords.forEach(k=>w.s(k));w.n(q.answer.length);q.answer.forEach(l=>line(w,l));}}
function script(w:Writer,s:TalkScript){w.n(s.npcIndex);[s.name,s.description,s.greeting,s.job,s.bye].forEach(l=>line(w,l));qa(w,s.qa);w.n(s.labels.length);for(const l of s.labels){w.n(l.label);line(w,l.initialLine);w.n(l.defaultAnswers.length);l.defaultAnswers.forEach(a=>line(w,a));qa(w,l.qa);}}
function output(w:Writer,os:DialogueOutput[]){w.n(os.length);for(const o of os){w.n(['line','prompt','effect'].indexOf(o.kind));if(o.kind==='line'){w.s(o.text);w.n(+(o.rune??false));w.n(['','key','timed'].indexOf(o.pause??''));w.n(o.segments?.length??0);for(const s of o.segments??[]){w.s(s.text);w.n(+s.rune);}}else if(o.kind==='prompt')w.n(+o.question);else{w.n(effects.indexOf(o.effect.kind));w.n('amount'in o.effect?o.effect.amount:'item'in o.effect?o.effect.item:'delta'in o.effect?o.effect.delta:0);}}}
function state(v:number):any{return {gold:[0,1,30,9999][v%4],food:9999,keys:98,gems:99,torches:0,skullKeys:99,magicCarpets:98,karma:[0,50,98,99][v%4],turnsSinceStart:[0,99,100,255][v%4],grapple:false,specialItems:{},equipmentQuantities:Array([0,48,64,256][v%4]).fill(v%4===3?100:98),characters:Array.from({length:16},(_,i)=>({name:i===0?'Avatar':i===1?'Mariah':i===8?'Gorn':`MEM${i}`,partyStatus:i<(v%4===3?6:2)?0:255,weapon:i,exp:i*123})),partySize:v%4===3?6:2,activeCharacter:1};}
function stateWrite(w:Writer,g:any){for(const n of [g.gold,g.food,g.keys,g.gems,g.torches,g.skullKeys,g.magicCarpets,g.karma,g.turnsSinceStart,+g.grapple,+(g.specialItems.sextant??false),+(g.specialItems.spyglass??false),+(g.specialItems.blackBadge??false),g.partySize,g.activeCharacter,...Array.from({length:256},(_,i)=>g.equipmentQuantities[i]??0),g.equipmentQuantities.length])w.n(n);for(const c of g.characters){w.s(c.name);w.n(c.partyStatus);w.n(c.weapon);w.n(c.exp);}}
function hash(b:Buffer){let h=2166136261;for(const v of b)h=Math.imul(h^v,16777619)>>>0;return h;}
const t=(text:string):ScriptLine=>[{kind:'text',text}];
const op=(op:string,data?:number):ScriptLine=>[{kind:'op',op,...(data===undefined?{}:{data})}];
const synthetic:TalkScript={npcIndex:1,name:t('Gorn'),description:t('a tester.'),greeting:[...t('Welcome '),...op('AskName'),...op('IfElseKnowsName'),...t('known'),...op('StartNewSection'),...t('unknown')],job:t('Testing'),bye:t('Farewell\n\n'),qa:[],labels:[]};
for(const [k,l] of [
 ['ask',op('AskName')],['branch',[...op('Label',0)]],['cycle',op('Label',1)],['missing',op('Label',99)],
 ['mixed',[...t('Chant '),...op('Rune'),...t('AHM'),...op('Rune'),...t(' now\n')]],
 ['empty',[]],['join',op('JoinParty')],['end',op('EndConversation')],['guards',op('CallGuards')],
 ['pay',op('Gold',30)],['free',op('Gold',-9)],['karma',[...op('KarmaPlusOne'),...op('KarmaMinusOne')]],
 ['pause',[...op('Pause'),...op('KeyWait'),...t('last'),...op('Pause')]],
 ['raw',[...t('  '),...op('NewLine'),...op('StartNewSection'),...t('answer')]],
 ['name',t('duplicate')],['aaaa',t('overlap')],[' JOB ',t('case-sensitive map key')],['',t('ignored')]
] as [string,ScriptLine][])synthetic.qa.push({keywords:[k],answer:[l,t('ignored second answer')]});
for(let i=-1;i<=80;i++)synthetic.qa.push({keywords:[`item${i}!`],answer:[op('Change',i)]});
synthetic.labels=[{label:0,initialLine:[...op('StartLabelDefinition'),...op('Label',0),...t('Yes or no?')],defaultAnswers:[t('default one'),t('default two')],qa:[{keywords:['y'],answer:[[...op('KarmaPlusOne'),...t('yes')]]},{keywords:['n'],answer:[[...op('Gold',1),...t('no')]]},{keywords:['name'],answer:[op('AskName')]}]},
 {label:1,initialLine:op('Label',2),defaultAnswers:[],qa:[]},{label:2,initialLine:op('Label',1),defaultAnswers:[],qa:[]}];
const scripts:TalkScript[]=[synthetic,{...synthetic,npcIndex:2,name:[],description:[],greeting:[],job:[],bye:[],qa:[],labels:[]}];
const real:{index:number;master:string}[]=[];
const words=extractCompressedWords(readFileSync(new URL('../../../original/u5/ultima5/DATA.OVL',import.meta.url)));
for(const master of ['towne','dwelling','castle','keep']){
 const data=JSON.parse(readFileSync(new URL(`../../../game/assets/talk/${master}.json`,import.meta.url),'utf8')) as TalkScript[];
 deepStrictEqual(data,parseTlkFile(readFileSync(new URL(`../../../original/u5/ultima5/${master.toUpperCase()}.TLK`,import.meta.url)),words));
 for(const s of data){real.push({index:scripts.length,master});scripts.push(s);}
}
type Case={si:number;v:number;known:boolean;inputs:string[];label:number};
const cases:Case[]=[];
const edge=['occupation','workish','name','NAME',' name ','name!','give me a job','work','health','job job','xjob job','jobless','x job','\tjob','!job','goodbye','BYE FUCK','thanks','damn','ELECTRONIC ARTS','',' \t\n','unknown','aaaaa','xaaaa aaaa','nÁme','\u00a0job','\uFEFF','😀 job','yes','YES!','no','n','y','YEAH','nobody'];
for(let v=0;v<4;v++)for(const word of [...edge,...synthetic.qa.flatMap(q=>q.keywords)])cases.push({si:0,v,known:!!(v&1),inputs:['Avatar',word,word,'branch','','  ','yes','bye','after'],label:-1});
for(let v=0;v<4;v++)cases.push({si:1,v,known:!!(v&1),inputs:edge,label:-1});
// Exhaustive JS UTF-16 code-unit prefix domain, including whitespace, surrogates
// and the DOS mask/upcase behavior. No JS code-point iteration shortcuts.
for(let code=0;code<65536;code++)cases.push({si:0,v:0,known:false,inputs:[String.fromCharCode(code)+'job'],label:-1});
let realBranches=0,labels=0;
for(const r of real){const s=scripts[r.index]!;
 for(const known of [false,true])for(const word of [...edge,...s.qa.flatMap(q=>q.keywords)])cases.push({si:r.index,v:cases.length%4,known,inputs:[word,'Avatar','y','n','bye',''],label:-1});
 // Isolated real label entry: keep original record and route its job to the label.
 // This measures label/default/QA coverage independently of narrative reachability.
 for(const l of s.labels){labels++;for(const answer of ['', '???',...l.qa.flatMap(q=>q.keywords)]){
   realBranches++;for(const known of [false,true])cases.push({si:r.index,v:cases.length%4,known,inputs:['job',answer,'Avatar','yes','no','','bye'],label:l.label});
 }}
}
const w=new Writer();w.n(0x54414c4b);w.n(scripts.length);scripts.forEach(s=>script(w,s));w.n(cases.length);
let snapshots=0,outputsCount=0;
const transcript:Record<string,number>={};
// Observe actual reference section entry, using original item identity. This is
// coverage instrumentation only; it neither selects branches nor alters output.
const branchItems=new WeakMap<object,string>(),entered=new Set<string>(),naturalEntered=new Set<string>();
for(const r of real){const s=scripts[r.index]!;const mark=(line:ScriptLine,id:string)=>{if(line[0])branchItems.set(line[0],`${r.master}/${s.npcIndex}/${id}`);};
 for(const k of ['name','description','greeting','job','bye'] as const)mark(s[k],`topic:${k}`);
 s.qa.forEach((q,i)=>mark(q.answer[0]??[],`qa:${i}`));
 for(const l of s.labels){mark(l.initialLine,`label:${l.label}:initial`);l.defaultAnswers.forEach((a,i)=>mark(a,`label:${l.label}:default:${i}`));l.qa.forEach((q,i)=>mark(q.answer[0]??[],`label:${l.label}:qa:${i}`));}
}
for(const c of cases){w.n(c.si);w.n(c.v);w.n(+c.known);w.n(c.label);w.n(c.inputs.length);c.inputs.forEach(s=>w.s(s));
 const original=scripts[c.si]!;const s=c.label<0?original:{...original,description:[],greeting:[],job:op('Label',c.label)};
 const g=state(c.v),rng=new OriginalRng(c.v*1709+13);
 const language=c.si===0&&c.v===3?{tr:(s:string)=>s?'['+s+']':'',seeCompose:(s:string)=>'SEE{'+s+'}',aliasFor:(k:string)=>k==='job'?['occupation','workish']:[]}:{};
 const conv=new Conversation(s,{avatarName:'Avatar',partyNames:['Avatar','Mariah'],npcKnowsAvatar:c.known,selfIntroRoll:()=>rng.next(0,1),...language});
 const instrumented=conv as any,processSection=instrumented.processSection;
 instrumented.processSection=function*(section:ScriptLine){for(const item of section){const id=branchItems.get(item);if(id){entered.add(id);if(c.label<0)naturalEntered.add(id);}}return yield* processSection.call(this,section);};
 const npcName=original.name.filter(i=>i.kind==='text').map(i=>(i as any).text).join('').trim();
 for(let step=-1;step<c.inputs.length;step++){
  const os=step<0?conv.start():conv.input(c.inputs[step]!);const snap=new Writer();
  output(snap,os);snap.n(+conv.ended);snap.n(+conv.metAvatar);
  for(const o of os)if(o.kind==='effect'&&step>=0){const res=applyDialogueEffect(g,o.effect,npcName,0x6c+c.v);snap.n(res.messages.length);res.messages.forEach(m=>snap.s(m));snap.n(+res.ended);snap.n(+(res.despawnNpc??false));snap.n(+(res.alarm??false));transcript[o.effect.kind]=(transcript[o.effect.kind]??0)+1;}
  stateWrite(snap,g);snap.n(rng.getSeed());w.n(hash(snap.bytes()));snapshots++;outputsCount+=os.length;
 }
}
// Reference orchestration: real Game methods, NpcManager mutations and the
// platform-independent portion of TalkConsole with a synchronous recording sink.
// The quest-end implementation is an explicit boundary marker, never simulated.
const flowCases= scripts.length*4;
w.n(flowCases);let flowSnapshots=0;
function actor(slot:number,type:number,dialogNumber:number,scheduled:boolean){return {slot,type,dialogNumber,location:1,x:10+slot,y:10,z:0,state:1,servedSlot:0,pathBuf:Array(32).fill(0),pathIdx:-1,stuck:0,aiTypes:[1,2,3],times:scheduled?[1,2,3,4]:[0,0,0,0],xs:[1,2,3],ys:[4,5,6],zs:[0,0,0]};}
function actorsWrite(w:Writer,g:any,list:any[]){
 for(const key of ['npcMet','npcDead'])for(const row of g[key]){let mask=0;row.forEach((b:boolean,i:number)=>{if(b)mask|=1<<i;});w.n(mask);}
 w.n(g.time.hour);w.n(g.time.minute);w.n(list.length);
 for(const a of list){for(const n of [a.slot,a.type,a.dialogNumber,a.location,a.x,a.y,a.z,a.state,a.servedSlot,a.pathIdx,a.stuck,...a.aiTypes,...a.times,...a.pathBuf])w.n(n);}
}
for(let si=0;si<scripts.length;si++)for(let v=0;v<4;v++){
 w.n(si);w.n(v);const s=scripts[si]!,g=state(v),rng=new OriginalRng(v*1709+13);
 g.position={location:1,floor:0,x:10,y:10};g.time={hour:12,minute:34};g.npcMet=Array.from({length:32},()=>Array(32).fill(false));g.npcDead=structuredClone(g.npcMet);g.npcMet[0][1]=!!(v&1);
 const target=actor(1,0x6c+v,s.npcIndex,true),list=[target,actor(6,0xb4,1,true),actor(5,0x90,0xfe,true),actor(4,0x40,0xfe,false),actor(3,0x71,1,true),actor(2,0xfc,255,true)];
 const manager=Object.create(NpcManager.prototype);manager.npcs=new Map([[1,list]]);
 const game=Object.create(Game.prototype);Object.assign(game,{state:g,npcManager:manager,rand:(lo:number,hi:number)=>rng.next(lo,hi),targetCoord:()=>({nx:11,ny:10}),talkScripts:new TalkScriptRegistry({towne:[s],dwelling:[],castle:[],keep:[]})});
 let snap=new Writer();
 game.faulineiTheftOnTalkEnd=()=>{snap.n(3);return [];};
 const console=new TalkConsole({game,hud:{message:(text,rune)=>{snap.n(0);snap.s(text);snap.n(+(rune??false));},messageSegments:(segs)=>{snap.n(1);snap.n(segs.length);for(const seg of segs){snap.s(seg.text);snap.n(+seg.rune);}},echoCursor:()=>{}},prompts:{current:null},refreshAwaiting:()=>{},instant:()=>true});
 const actions=[['start',''],...['Avatar','ask','Mariah','job','y','guards','join','yes','bye'].map(t=>['input',t]),['start',''],['input','bye'],['cancel','']];
 w.n(actions.length);
 for(const [action,text] of actions){snap=new Writer();w.n(action==='start'?0:action==='input'?1:2);w.s(text!);
  if(action==='start'){
   if(!console.active){const possessed=game.tryTalkPossessed('east');if(possessed){for(const e of possessed){snap.n(0);snap.s(e.text);snap.n(0);}}else{const target=game.talkTarget('east');if(target)console.start(target);else{snap.n(0);snap.s('Funny, no response!');snap.n(0);}}}
  }else if(action==='input')(console as any).input(text);else if(console.active)(console as any).end();
  snap.n(99);snap.n(+console.active);stateWrite(snap,g);actorsWrite(snap,g,manager.npcs.get(1));snap.n(rng.getSeed());w.n(hash(snap.bytes()));flowSnapshots++;
 }
}
const rules:{v:number;kind:number;value:number;tile:number;name:string}[]=[];
for(let v=0;v<4;v++){
 for(let code=-2;code<=258;code++)rules.push({v,kind:2,value:code,tile:-1,name:''});
 for(const tile of [-1,0x40,0x6b,0x6c,0x6f,0x70])for(const amount of [-2147483648,-1,0,1,30,10000,2147483647])rules.push({v,kind:1,value:amount,tile,name:''});
 for(const name of ['Gorn',' GORN ','Avatar','Mariah','missing','MEM5',' MEM5 ','MEM15'])rules.push({v,kind:0,value:0,tile:-1,name});
 for(const delta of [-1,1])rules.push({v,kind:3,value:delta,tile:-1,name:''});
 for(const kind of [4,5])rules.push({v,kind,value:0,tile:-1,name:''});
}
w.n(rules.length);
for(const r of rules){for(const n of [r.v,r.kind,r.value,r.tile])w.n(n);w.s(r.name);const g=state(r.v);const e:any={kind:effects[r.kind]};if(r.kind===1)e.amount=r.value;if(r.kind===2)e.item=r.value;if(r.kind===3)e.delta=r.value;const res=applyDialogueEffect(g,e,r.name,r.tile<0?undefined:r.tile);const snap=new Writer();snap.n(res.messages.length);res.messages.forEach(m=>snap.s(m));snap.n(+res.ended);snap.n(+(res.despawnNpc??false));snap.n(+(res.alarm??false));stateWrite(snap,g);w.n(hash(snap.bytes()));}
const alarmCases=4096;w.n(alarmCases);
for(let seed=0;seed<alarmCases;seed++){
 const g=state(seed%4);g.npcMet=Array.from({length:32},()=>Array(32).fill(false));g.npcDead=structuredClone(g.npcMet);g.time={hour:12,minute:34};
 const list=Array.from({length:32},(_,i)=>actor(31-i,[0xfc,0xd8,0x70,0x71,0x40,0x73,0x74,0xb4][(i+seed)%8]!,i%3===0?0xfe:1,(i+seed)%3!==0));
 const manager=Object.create(NpcManager.prototype);manager.npcs=new Map([[1,list]]);const rng=new OriginalRng(seed);manager.arrestAlarm(1,(lo:number,hi:number)=>rng.next(lo,hi));const snap=new Writer();actorsWrite(snap,g,list);snap.n(rng.getSeed());w.n(hash(snap.bytes()));
}
const bytes=w.bytes(),path=new URL('dialogue.bin',root);
if(process.argv.includes('--check'))deepStrictEqual(readFileSync(path),bytes);else writeFileSync(path,bytes);
const report={realRecords:real.length,realLabels:labels,isolatedLabelInputPairs:realBranches,realBranchLinesEntered:entered.size,naturalBranchLinesEntered:naturalEntered.size,sequences:cases.length,snapshots,flowCases,flowSnapshots,effectCases:rules.length,alarmCases,totalSnapshots:snapshots+flowSnapshots+rules.length+alarmCases,outputs:outputsCount,effects:transcript,assetSkips:0,bytes:bytes.length};
writeFileSync(new URL('coverage.json',root),JSON.stringify(report,null,2)+'\n');
console.log(report);

