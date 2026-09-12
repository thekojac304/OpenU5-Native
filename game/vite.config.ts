import { defineConfig, type Plugin } from "vite";
import { cacheDirDeEsteArbol } from "./tools/vite-cache-dir.js";
import { defaultExclude } from "vitest/config";
import { createReadStream, cpSync, existsSync, rmSync, statSync } from "node:fs";
import { fileURLToPath } from "node:url";
import path from "node:path";
import { writeDeployArtifacts } from "./tools/deploy-artifacts.js";

// Atlas/walkthrough autocontenido que vive FUERA de game/ (docs/manual/companion).
// El menú SISTEMA lo enlaza en /companion/ — este plugin lo sirve en dev y lo copia
// a dist/ en build, para que el enlace funcione en cualquier puerto y en producción.
const HERE = path.dirname(fileURLToPath(import.meta.url));
const COMPANION_SRC = path.resolve(HERE, "../docs/manual/companion");
const COMPANION_ROUTE = "/companion";

const MIME: Record<string, string> = {
  ".html": "text/html; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".mjs": "text/javascript; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".css": "text/css; charset=utf-8",
  ".png": "image/png",
  ".jpg": "image/jpeg",
  ".jpeg": "image/jpeg",
  ".gif": "image/gif",
  ".webp": "image/webp",
  ".svg": "image/svg+xml",
  ".woff2": "font/woff2",
  ".woff": "font/woff",
  ".ttf": "font/ttf",
  ".txt": "text/plain; charset=utf-8",
};

/**
 * Sirve el companion atlas (docs/manual/companion) bajo /companion.
 * - DEV: middleware estático propio (game/ ya usa la publicDir única de vite, así que
 *   no podemos reutilizarla). Funciona en :5199 y en cualquier puerto e2e.
 * - BUILD: copia recursiva a dist/companion (reproducible; sin copia commiteada).
 */
/**
 * Copia los ASSETS DE DATOS del juego (game/assets/** → dist/assets/**) en el BUILD.
 *
 * En DEV, `/assets/...` lo sirve el dev-server de vite desde `game/assets/` (bajo la
 * raíz del proyecto), así que `fetchJson("/assets/maps/overworld.json")` funciona. Pero
 * `game/assets/` NO es la publicDir ni se importa como módulo, así que `vite build` NO
 * lo copiaba a dist/ → bajo `vite preview` (que sirve dist/) esos fetch 404 y el
 * fallback SPA devuelve index.html → «Unexpected token '<' … is not valid JSON» al boot.
 * Este plugin cierra el hueco copiando el árbol de datos a dist/assets en `closeBundle`
 * (merge con los bundles JS ya emitidos ahí; nombres disjuntos, sin colisión). Mismo
 * patrón que `companionAtlas`. `dereference` porque en worktrees `game/assets` es symlink.
 */
function gameDataAssets(): Plugin {
  const ASSETS_SRC = path.resolve(HERE, "assets");
  return {
    name: "u5-game-data-assets",
    closeBundle() {
      if (!existsSync(ASSETS_SRC)) return;
      const dest = path.resolve(HERE, "dist/assets");
      cpSync(ASSETS_SRC, dest, { recursive: true, dereference: true });
    },
  };
}

/**
 * Repone en dist/ los artefactos de DESPLIEGUE (robots.txt siempre; _worker.js sólo
 * con credenciales en el entorno). Antes era una regla de prosa —«reponer _worker.js
 * + robots.txt tras cada build, que el build los borra»— que la auditoría de cierre
 * 07-27 encontró incumplida en game/dist. Detalles y régimen de cada destino en
 * tools/deploy-artifacts.ts.
 */
function deployArtifacts(): Plugin {
  return {
    name: "u5-deploy-artifacts",
    closeBundle() {
      const dist = path.resolve(HERE, "dist");
      if (!existsSync(dist)) return;
      const out = writeDeployArtifacts(dist);
      // Visible a propósito: desplegar el staging sin gate es el fallo caro.
      this.info?.(
        `robots.txt (${out.robots})` +
          (out.worker
            ? " + _worker.js (gate de staging)"
            : " · SIN _worker.js: define U5_STAGING_USER/U5_STAGING_PASS para emitir el gate"),
      );
    },
  };
}

function companionAtlas(): Plugin {
  return {
    name: "u5-companion-atlas",
    configureServer(server) {
      server.middlewares.use(COMPANION_ROUTE, (req, res, next) => {
        const rel = decodeURIComponent((req.url ?? "/").split("?")[0]);
        const target = path.normalize(path.join(COMPANION_SRC, rel));
        // Anti path-traversal: nunca servir fuera del directorio del atlas.
        if (target !== COMPANION_SRC && !target.startsWith(COMPANION_SRC + path.sep)) {
          res.statusCode = 403;
          res.end("Forbidden");
          return;
        }
        let file = target;
        if (existsSync(file) && statSync(file).isDirectory()) file = path.join(file, "index.html");
        if (!existsSync(file) || !statSync(file).isFile()) {
          next();
          return;
        }
        res.setHeader("Content-Type", MIME[path.extname(file).toLowerCase()] ?? "application/octet-stream");
        createReadStream(file).pipe(res);
      });
    },
    closeBundle() {
      const dest = path.resolve(HERE, "dist/companion");
      rmSync(dest, { recursive: true, force: true });
      if (existsSync(COMPANION_SRC)) cpSync(COMPANION_SRC, dest, { recursive: true });
    },
  };
}

export default defineConfig({
  // host: true = escuchar en todas las interfaces (acceso desde el móvil en la LAN)
  server: { port: 5199, host: true },
  // Aislamiento de worktrees (régimen CLAUDE.md/R3): `node_modules` suele ser un symlink
  // al checkout principal, y el `.vite` por defecto viviría DENTRO de él — un `vite`
  // lanzado desde un worktree reoptimizaría la caché del server :5199 del usuario y
  // puede tumbarlo.
  // 🔴 ESTO ERA UNA REGLA-QUE-RECORDAR Y AHORA ES UNA PROPIEDAD DEL SISTEMA. Antes decía
  // «los carriles exportan U5_VITE_CACHE_DIR y quedan aislados por construcción», que
  // era falso: quedaban aislados por MEMORIA. El 08-08 la olvidé y reescribí la caché
  // compartida; y `.gitignore` lleva NUEVE cachés de carril distintas, o sea nueve
  // carriles que tropezaron antes y lo parchearon cada uno por su cuenta. `cacheDirDeEsteArbol`
  // lo detecta solo (ver `game/tools/vite-cache-dir.ts`); la variable sigue mandando
  // cuando está puesta, así que quien ya la exporta no se entera del cambio.
  cacheDir: cacheDirDeEsteArbol(),
  // FORMATO ESM PARA LOS WORKERS: el sintetizador de música corre en un Web Worker
  // (`src/ui/opl/render-worker.ts`, importado con `?worker`) que importa los módulos
  // puros del OPL; como módulo ES se empaqueta con su grafo y sin envoltorio `iife`.
  // No hay ningún otro worker en el proyecto (medido), así que esto no afecta a nada más.
  worker: { format: "es" },
  plugins: [gameDataAssets(), companionAtlas(), deployArtifacts()],
  test: {
    // e2e/ son specs de Playwright, no de vitest — el patrón *.spec.ts los solaparía.
    exclude: [...defaultExclude, "e2e/**"],
    // Cobertura OCASIONAL (auditoría Q8): `npm run coverage` (no es gate — el gate
    // sigue siendo `vitest run`). Da el mapa de bolsas sin red (main.ts 4k líneas,
    // faithful-intro…) para que la próxima no crezca sin aviso. text+html en
    // coverage/ (gitignored).
    coverage: {
      provider: "v8",
      include: ["src/**"],
      reporter: ["text-summary", "html"],
      reportsDirectory: "coverage",
    },
  },
});
