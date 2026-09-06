"""@brief Inspect pinned Batman x86 code on disk; never access or modify a process."""

import argparse
import hashlib
from pathlib import Path
import capstone
import pefile

#: Exact supported retail build length; reject other files before interpreting addresses.
EXPECTED_LENGTH = 38758728
#: Whole-file identity for the only executable whose static leads are documented.
EXPECTED_SHA256 = '4dac1f5e2ac6710b7378fdce74601f616f4753e3756cb5fda63c7519cc2eb028'


def VerifyExecutable(Data: bytes) -> None:
    """@brief Reject foreign or modified inputs before parsing any engine contract."""
    if len(Data) != EXPECTED_LENGTH:
        raise ValueError('Unsupported executable length')
    if hashlib.sha256(Data).hexdigest() != EXPECTED_SHA256:
        raise ValueError('Unsupported executable SHA256')


def ResolveFileOffset(Image, Address: int, Length: int) -> int:
    """@brief Require a positive range wholly backed by one raw PE section and the file."""
    Rva = Address - Image.OPTIONAL_HEADER.ImageBase
    if Rva < 0 or Length <= 0:
        raise ValueError('Invalid virtual address or length')
    for Section in Image.sections:
        Relative = Rva - Section.VirtualAddress
        if 0 <= Relative < Section.SizeOfRawData:
            Offset = Section.PointerToRawData + Relative
            if Relative + Length > Section.SizeOfRawData or Offset + Length > len(Image.__data__):
                raise ValueError('Range exceeds raw section or file bounds')
            return Offset
    raise ValueError('Virtual address has no raw section backing')


def ReadVirtualBytes(Image, Address: int, Length: int) -> bytes:
    """@brief Read static instruction evidence using PE mapping, never process addresses."""
    Offset = ResolveFileOffset(Image, Address, Length)
    return bytes(Image.__data__[Offset:Offset + Length])


def DecodeInstructions(Data: bytes, Address: int) -> list:
    """@brief Decode every requested byte or fail instead of hiding a truncated tail."""
    Decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    Instructions = list(Decoder.disasm(Data, Address))
    if not Data or sum(Instruction.size for Instruction in Instructions) != len(Data):
        raise ValueError('Incomplete instruction decoding; choose verified boundaries')
    return Instructions


def ParseInteger(Value: str) -> int:
    """@brief Accept decimal or explicitly prefixed hexadecimal CLI addresses and lengths."""
    return int(Value, 0)


def Main() -> None:
    """@brief Print verified static disassembly; input files are opened only for reading."""
    Parser = argparse.ArgumentParser(description=__doc__)
    Parser.add_argument('--executable', required=True, type=Path)
    Parser.add_argument('--address', required=True, type=ParseInteger)
    Parser.add_argument('--length', required=True, type=ParseInteger)
    Arguments = Parser.parse_args()
    Data = Arguments.executable.read_bytes()
    VerifyExecutable(Data)
    Image = pefile.PE(data=Data, fast_load=True)
    try:
        Code = ReadVirtualBytes(Image, Arguments.address, Arguments.length)
        Instructions = DecodeInstructions(Code, Arguments.address)
        print(f'SHA256 {EXPECTED_SHA256.upper()}')
        print('STATIC EVIDENCE ONLY: decoding does not verify ABI, ownership, or thread safety')
        for Instruction in Instructions:
            Rva = Instruction.address - Image.OPTIONAL_HEADER.ImageBase
            Offset = ResolveFileOffset(Image, Instruction.address, Instruction.size)
            print(f'VA {Instruction.address:08X} RVA {Rva:08X} FILE {Offset:08X} '
                  f'{Instruction.bytes.hex().upper():24} {Instruction.mnemonic} {Instruction.op_str}')
    finally:
        Image.close()


if __name__ == '__main__':
    Main()
