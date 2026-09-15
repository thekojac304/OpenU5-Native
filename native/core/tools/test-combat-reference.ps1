$ErrorActionPreference = 'Stop'
# Run from the repository root. Missing commercial assets are explicit suite skips.
$available = @('combat-exact','charmed-attacker','combat-hotfix','equip','attack-snap','combat-use-potion','serpent-ranged')
$arenas = @('combat-active-player','combat-party-placement','combat-exit-gating','combat-escape-ship-guard','combat-seed','combat-ready','combat-rats-cycle','combat-victory-linger','muertos-combate-356','antym-ranged-interference','polearm-over-obstacle','combat','camp-ambush','camp-guard-walk')
if (Test-Path -LiteralPath 'game/assets/maps/combatmaps.json') {
    $available += $arenas
} else {
    foreach ($name in $arenas) { Write-Output "SKIP game/tests/$name.test.ts: missing game/assets/maps/combatmaps.json" }
}
$files = $available | ForEach-Object { "tests/$_.test.ts" }
& node node_modules/vitest/vitest.mjs run --root game @files
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$extractor = @('tests/native-input.test.ts','tests/native-movement.test.ts','tests/native-pack.test.ts')
if ((Test-Path -LiteralPath 'original/u5/ultima5/BRIT.CBT') -and (Test-Path -LiteralPath 'original/u5/ultima5/DUNGEON.CBT')) {
    $extractor += 'tests/combatmap.test.ts'
} else {
    Write-Output 'SKIP extractor/tests/combatmap.test.ts: missing original/u5/ultima5/BRIT.CBT and/or DUNGEON.CBT'
}
& node node_modules/vitest/vitest.mjs run --root extractor @extractor
exit $LASTEXITCODE
