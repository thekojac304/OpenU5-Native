/** Reproducible nearest-neighbor contact sheets from the shipped EGA TILES.16. */
import { readFileSync, mkdirSync, writeFileSync } from "node:fs";
import { createHash } from "node:crypto";
import { PNG } from "pngjs";
import { decompressLzw } from "../../extractor/src/parsers/lzw.ts";
import { parseTiles } from "../../extractor/src/parsers/tiles.ts";

const source = "original/u5/ultima5/TILES.16";
const output = "re/verified/batch47-bed-tiles";
const raw = readFileSync(source);
const tiles = parseTiles(decompressLzw(raw));
const font: Record<string, string[]> = {
  "0": ["111","101","101","101","111"], "1": ["010","110","010","010","111"],
  "2": ["111","001","111","100","111"], "3": ["111","001","111","001","111"],
  "4": ["101","101","111","001","001"], "5": ["111","100","111","001","111"],
  "6": ["111","100","111","101","111"], "7": ["111","001","001","001","001"],
  "8": ["111","101","111","101","111"], "9": ["111","101","111","001","111"],
  A: ["010","101","111","101","101"], B: ["110","101","110","101","110"],
  C: ["011","100","100","100","011"], D: ["110","101","101","101","110"],
  E: ["111","100","110","100","111"], F: ["111","100","110","100","100"],
  X: ["101","101","010","101","101"], " ": ["000","000","000","000","000"],
};
const scale = 4, cellW = 78, cellH = 86, cols = 16, rows = 8;
function put(png: PNG, x: number, y: number, rgb: readonly number[]) {
  const p = (y * png.width + x) * 4;
  png.data[p] = rgb[0]!; png.data[p+1] = rgb[1]!; png.data[p+2] = rgb[2]!; png.data[p+3] = 255;
}
function label(png: PNG, x: number, y: number, value: string) {
  for (const char of value) {
    for (let j = 0; j < 5; j++) for (let i = 0; i < 3; i++)
      if (font[char]![j]![i] === "1") put(png, x+i, y+j, [255,255,255]);
    x += 4;
  }
}
function sheet(first: number, last: number, name: string) {
  const png = new PNG({width: cols*cellW, height: rows*cellH});
  for (let p = 0; p < png.data.length; p += 4) { png.data[p]=24; png.data[p+1]=24; png.data[p+2]=24; png.data[p+3]=255; }
  for (let id = first; id <= last; id++) {
    const n = id-first, ox=(n%cols)*cellW+7, oy=Math.floor(n/cols)*cellH+2;
    const t=tiles[id]!;
    for(let y=0;y<16;y++) for(let x=0;x<16;x++) {
      const s=(y*16+x)*4;
      for(let yy=0;yy<scale;yy++) for(let xx=0;xx<scale;xx++)
        put(png,ox+x*scale+xx,oy+y*scale+yy,[t[s]!,t[s+1]!,t[s+2]!]);
    }
    label(png,ox,oy+67,`0X${id.toString(16).padStart(3,"0").toUpperCase()} ${id.toString().padStart(3,"0")}`);
  }
  writeFileSync(`${output}/${name}.png`,PNG.sync.write(png));
}
mkdirSync(output,{recursive:true});
for(let block=0;block<4;block++) sheet(block*128,block*128+127,`tiles-${block*128}-${block*128+127}`);
writeFileSync(`${output}/source.txt`,`${source}\nSHA-256 ${createHash("sha256").update(raw).digest("hex")}\n512 tiles, 16x16, canonical EGA palette, 4x nearest-neighbor; labels hex and decimal.\n`);
console.log(`Wrote four sheets to ${output}`);

function focus(first: number, last: number, name: string) {
  const ncols=8, w=148, h=154, factor=8;
  const png=new PNG({width:ncols*w,height:Math.ceil((last-first+1)/ncols)*h});
  for(let p=0;p<png.data.length;p+=4){png.data[p]=24;png.data[p+1]=24;png.data[p+2]=24;png.data[p+3]=255;}
  for(let id=first;id<=last;id++){
    const n=id-first,ox=n%ncols*w+10,oy=Math.floor(n/ncols)*h+3,t=tiles[id]!;
    for(let y=0;y<16;y++)for(let x=0;x<16;x++){
      const s=(y*16+x)*4;
      for(let yy=0;yy<factor;yy++)for(let xx=0;xx<factor;xx++)
        put(png,ox+x*factor+xx,oy+y*factor+yy,[t[s]!,t[s+1]!,t[s+2]!]);
    }
    label(png,ox,oy+131,`0X${id.toString(16).padStart(3,"0").toUpperCase()} ${id.toString().padStart(3,"0")}`);
  }
  writeFileSync(`${output}/${name}.png`,PNG.sync.write(png));
}
focus(0x80,0xbf,"furniture-080-0bf");
focus(0x100,0x13f,"sleep-100-13f");

focus(0x110,0x11f,"candidate-110-11f");
function castleContext(occupied:boolean){
  const span=7, factor=4, step=70, png=new PNG({width:span*step,height:span*step});
  const data=readFileSync("original/u5/ultima5/CASTLE.DAT"),base=1024;
  for(let p=0;p<png.data.length;p+=4){png.data[p]=24;png.data[p+1]=24;png.data[p+2]=24;png.data[p+3]=255;}
  for(let gy=0;gy<span;gy++)for(let gx=0;gx<span;gx++){
    const x=6+gx,y=4+gy,terrain=data[base+y*32+x]!;
    const id=occupied&&x===9&&y===7?0x11a:terrain;
    const t=tiles[id]!,ox=gx*step+3,oy=gy*step+2;
    for(let py=0;py<16;py++)for(let px=0;px<16;px++){
      const s=(py*16+px)*4;
      for(let yy=0;yy<factor;yy++)for(let xx=0;xx<factor;xx++)
        put(png,ox+px*factor+xx,oy+py*factor+yy,[t[s]!,t[s+1]!,t[s+2]!]);
    }
    label(png,ox,oy+64,`0X${id.toString(16).padStart(3,"0").toUpperCase()}`);
  }
  writeFileSync(`${output}/castle-bed-${occupied?"pose-illustration":"static"}-x9y7.png`,PNG.sync.write(png));
}
castleContext(false);
castleContext(true);
