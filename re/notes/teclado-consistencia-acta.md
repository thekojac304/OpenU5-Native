# Acta — el teclado en pantalla, un solo componente (2026-09-12)

## El reporte

> «El teclado emergente cambia de forma según el layout activo; en split vertical se queda
> apretujado a un lado; partes suyas las tapa la ventana de juego; no parece el mismo
> componente en un layout y en otro.»

## La causa raíz, medida

Las hojas `.touch-sheet-{az,num,yesno}` son **un solo DOM** — `ui/touch.ts` las construye una
vez, con sus teclas y su orden — pero **nadie era dueño de su geometría**. Lo era, por
accidente, el contenedor en el que cada layout las dejaba caer. Censo de las cuatro
composiciones que se servían:

| layout             | dónde vivía la hoja                        | resultado                                            |
|--------------------|--------------------------------------------|------------------------------------------------------|
| clásico vertical   | en FLUJO dentro del deck, `min-height:196px`| banda ancha; tecla ≈ 35 px                           |
| partido vertical   | `display:none` (az) · `grid-area:1/2/-1/3` (num, yesno) | **A–Z inexistente**; numpad estrujado en la columna de acciones (~90 px), con excepción de suelo táctil a 34 px |
| Enhanced vertical  | ninguna regla propia                        | hija flex de un deck en `flex-direction:row` ⇒ **columna estrecha a un lado**, recortada por el `overflow` del deck y por `max-height: var(--u5e-hueco)` |
| apaisado           | `position:fixed` a lo ancho, **sólo az**    | correcto… para una hoja y una orientación; num y yesno seguían dentro del raíl de ~244 px |

Cada una de las cuatro reglas era **razonable en su fichero** y estaba medida: la del
apaisado nació de que 10 teclas en el raíl daban 19,1 px; la del partido, de que el deck
vivía acotado a ~104 px; la de Enhanced, de que esa chapa no hereda la hoja del deck clásico
a propósito. El defecto no fue ninguna de ellas: fue que **la suma no era de nadie**.

## Lo que se fija

`game/src/ui/teclado-capa.ts` — **una sola autoridad**:

1. la hoja alzada es `position: fixed` (fuera de flujo ⇒ no la estruja ningún contenedor y
   no mueve el rect que `syncReserve()` mide: cero bucle medir→resize→medir);
2. se ancla en `--u5-kb-suelo`, que publica `ui/touch.ts` con el alto de la banda del deck
   (vertical) o 0 (apaisado, donde el deck es un raíl lateral) ⇒ **nada del deck queda
   tapado**, por construcción y no por un z-index afortunado;
3. la reserva vertical pasa a ser la **unión** de banda del deck + teclado
   (`bandaDelTeclado`), así que el juego conserva su hueco;
4. las medidas (alto de tecla, huecos, cuerpo de letra, tope de ancho) viven en custom
   properties de ese fichero y **ningún layout las redefine**.

El lift de la pila de juego pasa a tener también un solo dueño
(`game/src/ui/elevacion-juego.ts`): el puente al teclado del SISTEMA y la capa propia
**registran demandas** y se aplica la mayor. Antes el puente escribía `transform = ""` en cada
`resize` del visual viewport, o sea que habría borrado el lift del otro.

## La decisión de producto que esto lleva dentro

El layout partido **gana teclado propio** (antes `display:none`) y, a cambio, el teclado del
SISTEMA deja de abrirse **solo** cuando ya hay teclas en pantalla. El gate no es nuevo: es
`necesitaTecladoSistema()`, el mismo predicado que ya gobernaba el realce del botón «ABC»
(«la hoja alzada NO se ve ⇒ el sistema es la única vía»).

El puente **no se pierde**: sigue a un toque del botón «ABC», con su campo, su centinela de
⌫ y su Enter intactos — que es literalmente lo que la spec del usuario del 27-07 pedía («el
teclado estándar… que este se active con un botón»).

## Lo que NO se ha tocado

Las teclas que emite cada botón, el orden QWERTY, la fila de acción (Espacio · ⌫ · ⏎), la
semántica de los prompts, el auto-alzado por `expectInput` y la fusión de hojas (num y yesno
siguen siendo hojas distintas). Este carril es geometría y presentación.

## Excepción retirada del ledger

«numpad (alto 34)» sale de `game/e2e/mobile/suelo-tactil.ts`: su condición de cierre escrita
era «que viva el scroll de la zona de hojas, con lo que la botonera podría ceder alto sin
robárselo al mapa», y un overlay fuera de flujo con scroll propio es exactamente eso. El
numpad mide hoy 44 px de alto en los cuatro layouts.

## Residuo declarado

En **apaisado** la barra a lo ancho pasa por delante de la parte baja del raíl (decisión del
25-07, conservada): ahí no existe un «encima del deck» sin renunciar a la barra. Lo que el
producto promete en esa celda —y lo que el gate aserta— es que quede **una salida visible de
la hoja** por encima del teclado.
