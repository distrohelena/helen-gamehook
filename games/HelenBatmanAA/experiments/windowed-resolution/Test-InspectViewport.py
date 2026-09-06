"""@brief Exercise identity, section bounds and complete decoding without process access."""

import importlib.util
from pathlib import Path
from types import SimpleNamespace
import unittest

SPEC = importlib.util.spec_from_file_location("ViewportInspector", Path(__file__).with_name("Inspect-Viewport.py"))
inspector = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(inspector)


class ViewportInspectorTests(unittest.TestCase):
    """@brief Catch unsafe acceptance of foreign executables or misleading disassembly ranges."""

    def setUp(self):
        """@brief Represent a raw section followed by unrelated bytes and virtual zero-fill."""
        self.Image = SimpleNamespace(
            OPTIONAL_HEADER=SimpleNamespace(ImageBase=0x400000),
            sections=[SimpleNamespace(VirtualAddress=0x1000, PointerToRawData=4,
                                      SizeOfRawData=3, Misc_VirtualSize=16)],
            __data__=b'HEAD\x90\x90\xc3TAIL')

    def test_rejects_wrong_length(self):
        """@brief An unrelated file must not reach address interpretation."""
        with self.assertRaisesRegex(ValueError, 'length'):
            inspector.VerifyExecutable(b'not a PE')

    def test_rejects_same_length_wrong_digest(self):
        """@brief File size alone cannot authorize executable contracts."""
        with self.assertRaisesRegex(ValueError, 'SHA256'):
            inspector.VerifyExecutable(bytes(38758728))

    def test_reads_raw_section_not_virtual_address_as_offset(self):
        """@brief Translate the preferred VA to the section's actual file location."""
        self.assertEqual(inspector.ReadVirtualBytes(self.Image, 0x401000, 3), b'\x90\x90\xc3')

    def test_rejects_before_base(self):
        """@brief Negative RVAs must never become Python negative indices."""
        with self.assertRaises(ValueError):
            inspector.ReadVirtualBytes(self.Image, 0x3fffff, 1)

    def test_rejects_empty_range(self):
        """@brief An empty decode cannot serve as successful evidence."""
        with self.assertRaises(ValueError):
            inspector.ReadVirtualBytes(self.Image, 0x401000, 0)

    def test_rejects_crossing_raw_section(self):
        """@brief Adjacent file bytes are not automatically mapped into this section."""
        with self.assertRaises(ValueError):
            inspector.ReadVirtualBytes(self.Image, 0x401000, 4)

    def test_rejects_virtual_zero_fill(self):
        """@brief Virtual-only bytes cannot be presented as raw instruction evidence."""
        with self.assertRaises(ValueError):
            inspector.ReadVirtualBytes(self.Image, 0x401003, 1)

    def test_rejects_truncated_file(self):
        """@brief Section declarations cannot authorize reading beyond actual input."""
        self.Image.__data__ = b'HEAD\x90'
        with self.assertRaises(ValueError):
            inspector.ReadVirtualBytes(self.Image, 0x401000, 3)

    def test_decodes_complete_return(self):
        """@brief Preserve instruction address and operand when the full return is present."""
        Instructions = inspector.DecodeInstructions(b'\xc2\x10\x00', 0xEAA0F0)
        self.assertEqual(len(Instructions), 1)
        self.assertEqual(Instructions[0].address, 0xEAA0F0)
        self.assertEqual(Instructions[0].mnemonic, 'ret')
        self.assertEqual(Instructions[0].op_str, '0x10')

    def test_rejects_partial_instruction_after_valid_prefix(self):
        """@brief A valid prefix must not hide an undecoded truncated final instruction."""
        with self.assertRaises(ValueError):
            inspector.DecodeInstructions(b'\x90\xc2', 0xEAA0EF)


if __name__ == '__main__':
    unittest.main()
