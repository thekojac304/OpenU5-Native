import { readFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import { alphaEnemyName } from "../../native/tools/u5pack/alpha1-enemy.js";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const data = JSON.parse(readFileSync(resolve(root, "game/assets/data.json"), "utf8"));
const flags = JSON.parse(readFileSync(
  resolve(root, "game/src/core/data/AdditionalEnemyFlags.json"), "utf8",
));

describe("Alpha resource enemy identity", () => {
  it("keeps definition, sprite family, and singular display name aligned", () => {
    const expected = new Map<number, string>([
      [0, "Mage"],
      [8, "Pirates"],
      [20, "Giant Rat"],
      [22, "Giant Spider"],
      [33, "Skeleton"],
      [39, "Dragon"],
      [42, "PoisonField"],
      [43, "Whirpool"],
      [45, "Corpser"],
      [47, "Shadow Lord"],
    ]);
    for (const [index, name] of expected) {
      expect(alphaEnemyName(index, data.monsterNamesMixed, flags), `enemy ${index}`).toBe(name);
    }
  });

  it("fixes the observed Giant Rat/Giant Spider shift", () => {
    expect(data.monsterNamesMixed[20]).toBe("Giant Spider");
    expect(alphaEnemyName(20, data.monsterNamesMixed, flags)).toBe("Giant Rat");
    expect(320 + 20 * 4).toBe(400);
  });
});
