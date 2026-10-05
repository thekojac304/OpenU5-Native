# T-Deck manual certification

Target time: 20 minutes (acceptable range 15–30 minutes). Run the automated smoke suite first;
this checklist covers qualities automation cannot judge reliably.

- [ ] **2 min — controls:** short Mic press consistently backs out; long Mic hold toggles Movement
  Mode once; WASD and trackball each feel responsive without repeats or missed releases.
- [ ] **2 min — directional prompt:** start Talk and Open direction prompts. In Movement Mode verify
  W/A/S/D and trackball select the same four directions; in dialogue text entry verify `wasd`
  remains literal text.
- [ ] **2 min — display stability:** walk outdoors, enter a local map, open/close Developer tools,
  and teleport once. Check for full-screen flashes, stale frames, tearing, or layout jumps.
- [ ] **2 min — typography:** inspect normal transcript, explicit dialogue line breaks, and a long
  wrapped line. Check font readability, clipping, and sensible word wrapping.
- [ ] **3 min — shop/inn:** visit one inn and one item shop. Confirm keeper/service text, readable
  choices, real item/service names, prices, prompts, purchase result, Mic Back, and clean return to
  gameplay.
- [ ] **2 min — inventory:** press R and inspect equipment plus Use/Cast lists. Confirm real names
  (including Spiked Helm/Mystic Armour if present), quantities, selection marker, and no raw IDs.
- [ ] **3 min — combat:** confirm active-actor marker and target reticle remain obvious against the
  map; cancel a target; finish a short fight; immediately try L/O/G/U/T/A routing.
- [ ] **2 min — animation:** observe terrain and actor animation at rest and while moving. Check
  cadence, readability, and absence of distracting flashing.
- [ ] **2 min — diagnostics/layout:** run one individual smoke group, then Run All. Confirm progress
  updates remain responsive, the first-failure/status area is legible, and normal play continues.

Record firmware SHA-256, resource-pack identity, smoke totals, and any failed checkbox with a short
video or photo. A full playthrough is not part of certification.

