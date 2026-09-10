"""Read-only extraction of the pinned loader's literal key-to-data-offset table; never runs game code."""
import hashlib
import json
import sys
import capstone
import pefile
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_OP_REG, X86_REG_ESI, X86_REG_ESP


def Main() -> None:
    """Track the loader's stack stores, including its intervening push, and report exact config pairs."""
    executable = sys.argv[1]
    with open(executable, 'rb') as source:
        digest = hashlib.file_digest(source, 'sha256').hexdigest()
    if digest != '4dac1f5e2ac6710b7378fdce74601f616f4753e3756cb5fda63c7519cc2eb028':
        raise RuntimeError('Unsupported executable fingerprint')
    pe = pefile.PE(executable, fast_load=True)
    decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    decoder.detail = True
    registers = {X86_REG_ESI: {'offset': 0}}
    stack = {}
    stack_delta = 0
    start = 0xC2013B
    end = 0xC20773
    decoded_end = start
    for instruction in decoder.disasm(pe.get_data(start - pe.OPTIONAL_HEADER.ImageBase, end - start), start):
        decoded_end = instruction.address + instruction.size
        operands = instruction.operands
        if instruction.mnemonic == 'push':
            stack_delta -= 4
        elif instruction.mnemonic == 'lea' and operands[0].type == X86_OP_REG:
            memory = operands[1].mem
            if memory.base == X86_REG_ESI and memory.index == 0:
                registers[operands[0].reg] = {'offset': memory.disp}
            else:
                registers.pop(operands[0].reg, None)
        elif instruction.mnemonic == 'mov':
            source = operands[1]
            value = source.imm if source.type == X86_OP_IMM else registers.get(source.reg) if source.type == X86_OP_REG else None
            destination = operands[0]
            if destination.type == X86_OP_MEM and destination.mem.base == X86_REG_ESP and destination.mem.index == 0:
                stack[stack_delta + destination.mem.disp] = value
            elif destination.type == X86_OP_REG:
                registers[destination.reg] = value
    if decoded_end != end or stack_delta != -4:
        raise RuntimeError('Loader decode or stack shape changed')
    fields = []
    for key_slot in list(range(0x10C, 0x25C, 8)) + list(range(0x14, 0x84, 8)):
        address = stack.get(stack_delta + key_slot)
        target = stack.get(stack_delta + key_slot + 4)
        if not isinstance(address, int) or not isinstance(target, (dict, int)):
            raise RuntimeError(f'Incomplete settings table entry at {key_slot:#x}: key={address}, target={target}')
        key = pe.get_data(address - pe.OPTIONAL_HEADER.ImageBase, 256).decode('utf-16-le', errors='replace').split('\0')[0]
        if isinstance(target, dict):
            fields.append({'key': key, 'dataOffset': hex(target['offset']), 'ownerAddress': hex(0x26C0B3C + target['offset'])})
        else:
            fields.append({'key': key, 'absoluteGlobal': hex(target)})
    print(json.dumps(fields, indent=2))


if __name__ == '__main__':
    Main()
