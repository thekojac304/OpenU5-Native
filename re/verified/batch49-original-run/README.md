# Batch 49 original-run attempt

The bundled original DOSBox-X 2024.03.01 and original game files were launched
from an isolated temporary copy. The copy's SAVED.GAM was diagnostically
patched to castle location 17, floor 0, (9,9), 23:45. The preserved original
files were not modified.

DOSBox-X exited normally at its 55-second time limit. Its process had a
visible window handle, and the script sent the documented screenshot shortcut.
No PNG or video capture appeared in the configured capture directory. The
emulator log reports "Screen report: Method 'None'" with invalid display size
throughout this session. This attempt is **not** an original rendered witness.
The synthetic save also does not prove that the NPC table was reloaded for the
patched clock or that the live terrain retained the authored bed byte.

Reproduce with re/tools/batch49_original_run_attempt.py on a desktop session
that supports capture, then inspect live object byte, DS:6608 cell, composed
byte at EXE 0x537d, and framebuffer tile for castle slot 13 or 1. Compare map
entry at bedtime with remaining on the map across the schedule transition.
