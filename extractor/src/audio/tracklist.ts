/**
 * Las DIECISÉIS pistas del parche comunitario "The Exodus Project" (Ultima V Upgrade
 * Patch, de Voyager Dragon), EN EL ORDEN DE SU ID DE CANCIÓN.
 *
 * 🔴 EL ORDEN NO ES DECORATIVO: ES LA TABLA DEL DRIVER. `mid.drv` (823 B) lleva en el
 * offset 0x20 una tabla de DIECISÉIS punteros a nombre de fichero, y su rutina de
 * reproducción (0x0ff) hace literalmente `bx = [0x120 + al*2]` con AL = el id de canción.
 * O sea: el id ES el índice de esta lista. Volcada byte a byte el 2026-09-12, coincide
 * con el orden del `Files.txt` del parche. Derivación completa (con el disasm) en
 * `re/notes/music-location-mapping.md` §2.
 *
 * 🔴 LOS NOMBRES DE SALIDA SON LOS DE LA CANCIÓN, NO LOS DE UN CONTEXTO, y eso es el
 * arreglo de un error real. Antes se llamaban por el sitio donde CREÍAMOS que sonaban
 * («castle.mid», «tavern.mid», «combat.mid»…) y tres de esos nombres resultaron FALSOS al
 * leer el driver: `RULEBRIT` no es la música del castillo (es el cierre del endgame), y
 * `HORNPIPE` no es la de las tabernas (es la de la FRAGATA). Un fichero que se llama como
 * el sitio equivocado propaga el error a todo el que lo lea. El sitio lo decide
 * `game/src/ui/music.ts`, que es donde vive la regla del driver; aquí sólo hay canciones.
 *
 * `out` es un `.mid`: el XMI se emite como datos y lo sintetiza EN VIVO el emulador de OPL
 * del juego con el banco de timbres del propio parche (`FAT.OPL`).
 */
export interface Track {
  /** Id de canción del driver (índice en la tabla de punteros de `mid.drv` 0x20). */
  id: number;
  xmi: string;
  out: string;
  /** Título humano, EXACTO del `Files.txt` del parche. */
  title: string;
}

export const TRACKS: Track[] = [
  { id: 0x0, xmi: "U5THEME.XMI", out: "music/theme.mid", title: "Ultima V Theme" },
  { id: 0x1, xmi: "BRITLAND.XMI", out: "music/britannia.mid", title: "Britannic Lands" },
  { id: 0x2, xmi: "HORNPIPE.XMI", out: "music/hornpipe.mid", title: "Cap'n Johne's Hornpipe" },
  { id: 0x3, xmi: "ENGGMNT.XMI", out: "music/engagement.mid", title: "Engagement and Melee" },
  { id: 0x4, xmi: "STONES.XMI", out: "music/stones.mid", title: "Stones" },
  { id: 0x5, xmi: "GREYSON.XMI", out: "music/greyson.mid", title: "Greyson's Tale" },
  { id: 0x6, xmi: "FANFARE.XMI", out: "music/fanfare.mid", title: "Fanfare for the Virtuous" },
  { id: 0x7, xmi: "MONARCH.XMI", out: "music/monarch.mid", title: "The Missing Monarch" },
  { id: 0x8, xmi: "TRNTLLA.XMI", out: "music/tarantella.mid", title: "Villager Tarantella" },
  { id: 0x9, xmi: "HALLS.XMI", out: "music/halls.mid", title: "Halls of Doom" },
  { id: 0xa, xmi: "WRLDBLW.XMI", out: "music/worlds-below.mid", title: "Worlds Below" },
  { id: 0xb, xmi: "BLCKTHRN.XMI", out: "music/blackthorn.mid", title: "Lord Blackthorn" },
  { id: 0xc, xmi: "LADYNAN.XMI", out: "music/ladynan.mid", title: "Dream of Lady Nan" },
  { id: 0xd, xmi: "REUNION.XMI", out: "music/reunion.mid", title: "Joyous Reunion" },
  { id: 0xe, xmi: "RULEBRIT.XMI", out: "music/rule-britannia.mid", title: "Rule Britannia" },
  { id: 0xf, xmi: "AMIGA.XMI", out: "music/amiga.mid", title: "Amiga Theme" },
];
