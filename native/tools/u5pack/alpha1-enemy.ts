export interface AlphaEnemyNameFlag {
  Name: string;
}

/**
 * Resolve the 48-definition combat index through DATA.OVL's compressed
 * mixed-case name pool.  That pool omits definitions 8, 9, 42, and 43; direct
 * indexing therefore labels Giant Rat (20) as Giant Spider (mixed[20]).
 */
export function alphaEnemyName(
  index: number,
  mixed: readonly string[],
  flags: readonly AlphaEnemyNameFlag[],
): string {
  if (index !== 8 && index !== 9 && index !== 42 && index !== 43) {
    let packedIndex = index;
    if (index > 8) packedIndex -= 2;
    if (index > 41) packedIndex -= 2;
    const value = mixed[packedIndex];
    if (value) return value;
  }

  const [key = "Enemy", display = ""] = (flags[index]?.Name ?? "Enemy").split("/");
  if (display && display !== "x") {
    return display.charAt(0) + display.slice(1).toLowerCase();
  }
  return key.replace(/\d+$/, "") || "Enemy";
}
