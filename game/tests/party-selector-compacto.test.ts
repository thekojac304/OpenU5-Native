// @vitest-environment jsdom
/**
 * EL SELECTOR COMPACTO DE MIEMBRO — que sea OTRA SUPERFICIE y no otro camino.
 *
 * QUÉ CIERRA, y por qué cada cosa:
 *
 *  1. ★ LA EQUIVALENCIA DE TECLAS, que es el aserto central. Tocar la fila del miembro N
 *     tiene que producir EXACTAMENTE el mismo evento que teclear el dígito N, y cancelar
 *     el mismo que pulsar Esc. Si esto se rompe, el selector deja de ser una superficie y
 *     pasa a ser una segunda vía de resolución — justo lo que el encargo prohíbe.
 *  2. LAS FILAS SON LAS DEL PARTY Y NADA MÁS. El defecto que esto sustituye era una
 *     rejilla de diez teclas (0-9) para elegir entre tres compañeros.
 *  3. NADIE SE OCULTA, ni muertos ni dormidos. El picker del binario tampoco los oculta y
 *     quien valida es el llamador; esconderlos haría inalcanzable `In Mani Corp`
 *     (resucitar), cuyo objetivo es por definición un compañero muerto. Se ATENÚA.
 *  4. NO ES MODAL Y NO SE TRAGA TECLAS. El prompt `party-select` sigue vivo debajo: sus
 *     flechas, su Enter y su Esc tienen que seguir llegándole. Un selector que capturara
 *     teclado rompería el camino del teclado físico que viene a complementar.
 *  5. LA CONCILIACIÓN (`syncPartyChooser`) abre, cierra y es idempotente — y cuando no se
 *     hace cargo, lo DICE, para que el llamador vuelva a alzar el numpad genérico.
 *  6. SUELO TÁCTIL 44 px y rótulos que envuelven, medidos sobre la HOJA con postcss (la
 *     misma técnica que `hechizos-panel-dom.test.ts`: jsdom no tiene motor de layout).
 */
import { afterEach, beforeEach, describe, expect, it } from "vitest";
import postcss, { type Declaration, type Rule } from "postcss";
import { BASE_LANG, setLang } from "../src/i18n/index.js";
import {
  MAX_SLOTS,
  partyChoices,
  type PartyStateLike,
} from "../src/enhanced/party/catalog.js";
import {
  closePartyChooser,
  openPartyChooser,
  partyChooserOpen,
  syncPartyChooser,
} from "../src/enhanced/party/panel.js";
import {
  partyPickerCss,
  PARTY_PICKER_STYLE_ID,
} from "../src/enhanced/party/css.js";
import { selectPartyMemberKey } from "../src/core/selectPartyMember.js";

/** Party de tres con un caído, que es el caso interesante (resucitar / despertar). */
function estado(over: Partial<PartyStateLike> = {}): PartyStateLike {
  return {
    partySize: 3,
    characters: [
      { name: "", status: "G", currentHp: 40, maxHp: 60 }, // sin nombre ⇒ «Avatar»
      { name: "Shamino", status: "P", currentHp: 52, maxHp: 60 },
      { name: "Iolo", status: "D", currentHp: 0, maxHp: 90 },
      { name: "Geoffrey", status: "G", currentHp: 70, maxHp: 70 }, // FUERA del party
    ],
    ...over,
  };
}

let teclas: string[] = [];
const espia = (k: string): void => {
  teclas.push(k);
};

function abre(st: PartyStateLike = estado()): void {
  teclas = [];
  openPartyChooser({ choices: partyChoices(st), press: espia });
}

function filas(): HTMLButtonElement[] {
  return [...document.querySelectorAll<HTMLButtonElement>(".u5pp-row")];
}

beforeEach(() => {
  document.body.innerHTML = "";
  document.getElementById(PARTY_PICKER_STYLE_ID)?.remove();
  setLang(BASE_LANG, { persist: false });
  teclas = [];
});
afterEach(() => {
  closePartyChooser();
  document.body.innerHTML = "";
  setLang(BASE_LANG, { persist: false });
});

describe("catálogo: sólo el party, con el vocabulario del roster", () => {
  it("una fila por miembro DENTRO de `partySize`, y ninguna más", () => {
    const c = partyChoices(estado());
    expect(c.map((x) => x.name)).toEqual(["Avatar", "Shamino", "Iolo"]);
    expect(c.map((x) => x.slot)).toEqual([1, 2, 3]);
    expect(c.map((x) => x.index)).toEqual([0, 1, 2]);
  });

  it("★ la tecla de cada fila es su dígito 1-based — el contrato con el reductor", () => {
    for (const c of partyChoices(estado())) {
      expect(c.key).toBe(String(c.slot));
      // Y ese dígito, metido en el reductor REAL del kernel, devuelve ese mismo índice.
      expect(selectPartyMemberKey({ cursor: 0 }, c.key, 3)).toEqual({
        kind: "select",
        index: c.index,
      });
    }
  });

  it("el nombre vacío degrada a «Avatar», como en el roster", () => {
    expect(partyChoices(estado())[0]!.name).toBe("Avatar");
  });

  it("caídos y dormidos ENTRAN igual, marcados con el mismo predicado que la flecha", () => {
    const c = partyChoices(estado());
    expect(c.map((x) => x.enPie)).toEqual([true, true, false]); // 'G','P' sí; 'D' no
    const dormido = partyChoices(
      estado({ characters: [{ name: "Min", status: "S", currentHp: 5, maxHp: 30 }], partySize: 1 }),
    );
    expect(dormido[0]!.enPie).toBe(false);
  });

  it("party a cero o roster incompleto no inventa filas", () => {
    expect(partyChoices(estado({ partySize: 0 }))).toEqual([]);
    expect(partyChoices({ partySize: 3, characters: [] })).toEqual([]);
  });

  it("no ofrece ranuras que el reductor no sabría leer (tope de 9)", () => {
    const muchos: PartyStateLike = {
      partySize: 12,
      characters: Array.from({ length: 12 }, (_, i) => ({
        name: `M${i}`,
        status: "G",
        currentHp: 10,
        maxHp: 10,
      })),
    };
    const c = partyChoices(muchos);
    expect(c.length).toBe(MAX_SLOTS);
    // Control positivo: el reductor ignora de verdad lo que queda fuera.
    expect(selectPartyMemberKey({ cursor: 0 }, "0", 12)).toEqual({ kind: "ignore" });
  });
});

describe("★ equivalencia de teclas: la superficie no es un camino nuevo", () => {
  it("tocar al miembro 2 emite EXACTAMENTE la tecla «2»", () => {
    abre();
    filas()[1]!.click();
    expect(teclas).toEqual(["2"]);
  });

  it("cada fila emite su propio dígito, y ninguna emite dos", () => {
    abre();
    for (const f of filas()) f.click();
    expect(teclas).toEqual(["1", "2", "3"]);
  });

  it("cancelar emite EXACTAMENTE «Escape» — la misma salida del reductor", () => {
    abre();
    document.querySelector<HTMLButtonElement>(".u5pp-cancel")!.click();
    expect(teclas).toEqual(["Escape"]);
    expect(selectPartyMemberKey({ cursor: 1 }, "Escape", 3)).toEqual({ kind: "close" });
  });

  it("★ tocar NO cierra el panel: el dueño del cierre es la conciliación", () => {
    // El bucle de re-pregunta «Disabled!» (kernel 0x4a4e) re-arma el MISMO prompt tras un
    // elegido no apto. Si la vista se cerrara sola, la segunda vuelta se quedaría sin
    // selector y el jugador, mirando un prompt que ya no puede contestar con el dedo.
    abre();
    filas()[2]!.click(); // Iolo, muerto: el llamador puede rechazarlo y re-preguntar
    expect(partyChooserOpen()).toBe(true);
  });
});

describe("★ no es modal: el teclado del juego sigue llegando al prompt", () => {
  it("las teclas de `window` NO se consumen mientras el selector vive", () => {
    const vistas: string[] = [];
    window.addEventListener("keydown", (ev) => vistas.push(ev.key));
    abre();
    for (const k of ["ArrowDown", "Enter", "Escape", "2"]) {
      window.dispatchEvent(new KeyboardEvent("keydown", { key: k, bubbles: true }));
    }
    expect(vistas).toEqual(["ArrowDown", "Enter", "Escape", "2"]);
  });

  it("no se anuncia como diálogo modal (el roster de debajo sigue vivo)", () => {
    abre();
    const root = document.querySelector<HTMLElement>(".u5pp")!;
    expect(root.getAttribute("role")).toBe("group");
    expect(root.hasAttribute("aria-modal")).toBe(false);
    expect(root.getAttribute("aria-label")).toBe("Choose party member");
  });
});

describe("render: lo que cada fila dice", () => {
  it("número, nombre, HP y la letra de estado del roster", () => {
    abre();
    const f = filas()[1]!;
    expect(f.querySelector(".u5pp-slot")!.textContent).toBe("2");
    expect(f.querySelector(".u5pp-name")!.textContent).toBe("Shamino");
    expect(f.querySelector(".u5pp-hp")!.textContent).toBe("52/60");
    expect(f.querySelector(".u5pp-st")!.textContent).toBe("P");
  });

  it("el caído se ATENÚA pero sigue siendo un botón elegible (no `disabled`)", () => {
    abre();
    const iolo = filas()[2]!;
    expect(iolo.dataset.down).toBe("1");
    expect(iolo.disabled).toBe(false);
    expect(iolo.hasAttribute("aria-disabled")).toBe(false);
    iolo.click();
    expect(teclas).toEqual(["3"]); // In Mani Corp tiene que poder apuntarle
  });

  it("cada fila lleva un nombre accesible COMPUESTO, no cuatro fragmentos sueltos", () => {
    abre();
    expect(filas()[1]!.getAttribute("aria-label")).toBe("2. Shamino — 52/60 HP (P)");
    for (const cls of [".u5pp-slot", ".u5pp-name", ".u5pp-hp", ".u5pp-st"]) {
      expect(filas()[1]!.querySelector(cls)!.getAttribute("aria-hidden")).toBe("true");
    }
  });

  it("las filas son `<button>` de verdad: foco, Enter y Espacio gratis", () => {
    abre();
    expect(filas().every((f) => f.tagName === "BUTTON" && f.type === "button")).toBe(true);
  });

  it("el cromo se traduce; los NOMBRES PROPIOS no", () => {
    setLang("es", { persist: false });
    abre();
    expect(document.querySelector(".u5pp-title")!.textContent).toBe("Elige un miembro");
    expect(document.querySelector(".u5pp-cancel")!.textContent).toBe("Cancelar");
    expect(filas()[1]!.querySelector(".u5pp-name")!.textContent).toBe("Shamino");
    expect(filas()[1]!.getAttribute("aria-label")).toContain("PV");
  });

  it("abrir dos veces no deja dos paneles", () => {
    abre();
    abre();
    expect(document.querySelectorAll(".u5pp").length).toBe(1);
  });

  it("sin miembros no se monta nada (un «Cancelar» solitario es peor que nada)", () => {
    expect(openPartyChooser({ choices: [], press: espia })).toBeNull();
    expect(document.querySelector(".u5pp")).toBeNull();
  });
});

describe("conciliación con el prompt vivo", () => {
  it("abre con el prompt armado y se hace cargo (⇒ el numpad no se alza)", () => {
    const tomado = syncPartyChooser({
      activo: true,
      disponible: true,
      state: estado(),
      press: espia,
    });
    expect(tomado).toBe(true);
    expect(partyChooserOpen()).toBe(true);
    expect(filas().length).toBe(3);
  });

  it("★ NO se hace cargo si la chapa no está disponible — el numpad vuelve", () => {
    const tomado = syncPartyChooser({
      activo: true,
      disponible: false,
      state: estado(),
      press: espia,
    });
    expect(tomado).toBe(false);
    expect(partyChooserOpen()).toBe(false);
  });

  it("tampoco con el party a cero: no hay a quién ofrecer", () => {
    const tomado = syncPartyChooser({
      activo: true,
      disponible: true,
      state: estado({ partySize: 0 }),
      press: espia,
    });
    expect(tomado).toBe(false);
    expect(document.querySelector(".u5pp")).toBeNull();
  });

  it("cierra en cuanto el prompt deja de estar armado", () => {
    syncPartyChooser({ activo: true, disponible: true, state: estado(), press: espia });
    expect(partyChooserOpen()).toBe(true);
    expect(
      syncPartyChooser({ activo: false, disponible: true, state: estado(), press: espia }),
    ).toBe(false);
    expect(partyChooserOpen()).toBe(false);
    expect(document.querySelector(".u5pp")).toBeNull();
  });

  it("★ es IDEMPOTENTE: no re-pinta bajo el dedo entre el pointerdown y el pointerup", () => {
    syncPartyChooser({ activo: true, disponible: true, state: estado(), press: espia });
    const antes = filas()[0]!;
    for (let i = 0; i < 5; i++) {
      syncPartyChooser({ activo: true, disponible: true, state: estado(), press: espia });
    }
    expect(filas()[0]).toBe(antes); // el MISMO nodo, no uno equivalente
    expect(document.querySelectorAll(".u5pp").length).toBe(1);
  });
});

describe("CSS: suelo táctil, envoltura y la invariante de reserva (medido sobre la hoja)", () => {
  const raiz = postcss.parse(partyPickerCss());
  const reglas = (pred: (sel: string) => boolean): Rule[] => {
    const out: Rule[] = [];
    raiz.walkRules((r) => {
      if (pred(r.selector)) out.push(r);
    });
    return out;
  };
  const decls = (pred: (sel: string) => boolean, prop: string): string[] => {
    const out: string[] = [];
    for (const r of reglas(pred)) {
      r.walkDecls(prop, (d: Declaration) => {
        out.push(d.value);
      });
    }
    return out;
  };

  it("★ todo lo que se pulsa declara el suelo táctil de 44 px", () => {
    for (const sel of [".u5pp-row", ".u5pp-cancel"]) {
      expect(
        decls((s) => s.split(",").some((p) => p.trim() === sel), "min-height"),
        `${sel}: sin suelo táctil declarado`,
      ).toContain("44px");
    }
    expect(decls((s) => s.includes(".u5pp-cancel"), "min-width")).toContain("44px");
  });

  it("los rótulos ENVUELVEN, nunca se recortan (política del deck)", () => {
    for (const sel of [".u5pp-name", ".u5pp-title"]) {
      expect(decls((s) => s.includes(sel), "white-space"), sel).toContain("normal");
    }
  });

  it("★ vive FUERA del deck y descuenta la reserva — la invariante que no se toca", () => {
    // `syncReserve()` mide el border box de `.touch-controls`; un panel que apareciera
    // dentro movería la reserva (y con ella el canvas) dos veces por prompt. `fixed` lo
    // deja fuera de ese border box, y el tope de alto descuenta la reserva para no
    // invadir el deck que hay debajo.
    expect(decls((s) => s.trim() === ".u5pp", "position")).toContain("fixed");
    // La reserva se descuenta en el SUELO (`--u5pp-piso`), que es de donde sale también el
    // tope de alto: el panel nunca puede invadir el deck valga lo que valga el HUD.
    expect(decls((s) => s.trim() === ".u5pp", "--u5pp-piso").join(" ")).toContain(
      "--u5-touch-reserve",
    );
    expect(decls((s) => s.trim() === ".u5pp", "max-height").join(" ")).toContain("--u5pp-piso");
  });

  it("★ se ancla ABAJO pero con SUELO: ni banda tapada ni pulgar estirado", () => {
    // Medido en su día: anclado al borde INFERIOR DEL VIEWPORT, en un 375×667 partido el
    // panel caía en y=334..538 y la banda del re-flow ocupa y=387..544 — o sea que tapaba el
    // cursor de vídeo inverso del picker Y la fila de consola con la pregunta. La respuesta
    // de entonces fue irse ARRIBA del todo, y el usuario cobró el precio declarado («too
    // high, too much thumb reach», 12-09 §5).
    // ★ La premisa falsa era «cualquier panel anclado abajo se come la banda»: sólo la come
    // si ancla al VIEWPORT. Anclando al borde SUPERIOR DEL HUD —que la piel publica en
    // `--u5-hud-top`— el panel baja a la zona del pulgar y lo que tapa es el mapa.
    const piso = decls((s) => s.trim() === ".u5pp", "--u5pp-piso").join(" ");
    expect(piso).toContain("--u5-hud-top");
    // EL FALLBACK LO VUELVE INERTE: sin la propiedad publicada, ese término se anula y manda
    // el del deck. Sin fallback, `100dvh - 0` empujaría el panel fuera de la pantalla.
    expect(piso).toMatch(/--u5-hud-top,\s*100dvh/);
    expect(decls((s) => s.trim() === ".u5pp", "bottom").join(" ")).toContain("--u5pp-piso");
    expect(decls((s) => s.trim() === ".u5pp", "top")).toEqual(["auto"]);
  });

  it("cada control tiene anillo de foco propio", () => {
    const foco = reglas((s) => s.includes(":focus-visible"));
    expect(foco.length).toBeGreaterThan(0);
    const trazos: string[] = [];
    for (const r of foco) {
      r.walkDecls("outline", (d) => {
        trazos.push(d.value);
      });
    }
    expect(trazos.join(" ")).toMatch(/\d+px/);
  });

  it("en apaisado se aparta del RAÍL, que ahí la reserva es de ancho", () => {
    const enApaisado = decls(
      (s) => s.includes('data-orient="landscape"') && s.includes(".u5pp"),
      "left",
    )
      .concat(
        decls((s) => s.includes('data-orient="landscape"') && s.includes(".u5pp"), "right"),
      )
      .join(" ");
    expect(enApaisado).toContain("--u5-touch-reserve-x");
  });
});
