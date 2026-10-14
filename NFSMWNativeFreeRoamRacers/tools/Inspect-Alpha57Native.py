"""Read-only native instruction inspection; does not launch or patch the game."""
import sys, pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
pe=pefile.PE(sys.argv[1]); image=pe.get_memory_mapped_image(); base=pe.OPTIONAL_HEADER.ImageBase
dis=Cs(CS_ARCH_X86,CS_MODE_32); dis.skipdata=True
if sys.argv[2]=='calls':
    targets={int(x,16) for x in sys.argv[3:]}
    for section in pe.sections:
        if not section.Characteristics&0x20000000: continue
        for ins in dis.disasm(section.get_data(),base+section.VirtualAddress):
            if ins.mnemonic=='call' and ins.op_str.startswith('0x') and int(ins.op_str,16)-base in targets:
                print(f'RVA {ins.address-base:08X} return {ins.address-base+ins.size:08X} -> {int(ins.op_str,16)-base:08X}')
else:
    rva=int(sys.argv[2],16); size=int(sys.argv[3],16)
    for ins in dis.disasm(image[rva:rva+size],base+rva):
        print(f'{ins.address:08X} {ins.bytes.hex():24s} {ins.mnemonic} {ins.op_str}')
