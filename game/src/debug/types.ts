/**
 * Tipos del REGISTRO de secciones del menú debug. El panel (panel.ts) se
 * auto-construye de una lista de `DebugSection`; añadir un campo futuro = 1
 * entrada nueva en registry.ts, sin tocar el panel.
 */

/** Contexto que el panel pasa a los renderers/handlers de una sección. */
export interface DebugRenderCtx {
  /** Re-lee TODOS los valores visibles del panel y repinta. Llamar tras mutar. */
  refresh(): void;
}

interface FieldBase {
  label: string;
  /** Ayuda opcional bajo el control. */
  hint?: string;
  /** Si devuelve true, el control se pinta deshabilitado. */
  disabled?: () => boolean;
  /** data-testid opcional en el control (e2e estable e independiente del idioma). */
  testId?: string;
}

export interface NumberField extends FieldBase {
  widget: "number";
  get(): number;
  set(v: number): void;
  min?: number;
  max?: number;
  step?: number;
}

export interface TextField extends FieldBase {
  widget: "text";
  get(): string;
  set(v: string): void;
}

export interface SelectOption {
  label: string;
  value: number | string;
  disabled?: boolean;
}

export interface SelectField extends FieldBase {
  widget: "select";
  get(): number | string;
  set(v: number | string): void;
  options: SelectOption[];
}

export interface CheckboxField extends FieldBase {
  widget: "checkbox";
  get(): boolean;
  set(v: boolean): void;
}

export interface ButtonField extends FieldBase {
  widget: "button";
  run(): void;
  /** Pinta un badge rojo de advertencia (p.ej. "consume RNG del stream"). */
  danger?: boolean;
}

export type DebugField =
  | NumberField
  | TextField
  | SelectField
  | CheckboxField
  | ButtonField;

export interface DebugSection {
  id: string;
  title: string;
  /**
   * CATEGORÍA a la que pertenece la sección en el panel de ajustes con navegador
   * (`ui/shell/settingsNav.ts`). Es un dato DECLARATIVO más, como `title`: la sección
   * dice dónde vive y el navegador la coloca — no hay una segunda lista «categoría →
   * secciones» que pueda quedar desincronizada con ésta.
   *
   * El valor reservado `FOOTER_GROUP` ("footer") manda la sección al PIE fijo, visible
   * desde cualquier categoría (la salida rotulada del drawer usa esto).
   *
   * Ausente ⇒ el drawer QA de debug, que se pinta como acordeón plano y no la mira.
   */
  group?: string;
  /**
   * Renderer a medida (p.ej. el canvas del mapa de teletransporte). Si está
   * presente, se pinta ANTES de los `fields`. Devuelve el elemento a insertar.
   */
  custom?: (ctx: DebugRenderCtx) => HTMLElement;
  /** Campos declarativos; el panel los pinta según su `widget`. */
  fields?: DebugField[];
}
