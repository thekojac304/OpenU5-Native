import glob, os, sys, capstone
# Census every instruction in ULTIMA.EXE and every overlay whose operand names DS:<addr>.
# Offsets are FILE offsets; ULTIMA.EXE code = file - 0x800 (MZ header).
addr = int(sys.argv[1], 16)
pat = bytes([addr & 0xff, addr >> 8])
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_16)
for f in sorted(glob.glob('original/u5/ultima5/*.OVL')) + ['original/u5/ultima5/ULTIMA.EXE']:
    b = open(f, 'rb').read()
    i = 0
    while True:
        i = b.find(pat, i)
        if i < 0: break
        for back in range(1, 5):
            s = i - back
            if s < 0: continue
            for ins in md.disasm(b[s:s + 8], s):
                if hex(addr) in ins.op_str and ins.size >= back + 2:
                    print(os.path.basename(f), hex(s), ins.mnemonic, ins.op_str)
                break
        i += 1
