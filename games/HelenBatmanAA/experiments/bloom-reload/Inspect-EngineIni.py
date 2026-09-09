"""Read-only inspection of the pinned engine INI buffer in an explicitly selected Batman process."""
import ctypes
import sys
import struct
from ctypes import wintypes


def Main() -> None:
    """Open only query/read access and print the required INI buffer; never invoke or modify game code."""
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.ReadProcessMemory.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
    kernel.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)]
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    handle = kernel.OpenProcess(0x1010, False, int(sys.argv[1]))
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        path = ctypes.create_unicode_buffer(32768)
        length = wintypes.DWORD(len(path))
        if not kernel.QueryFullProcessImageNameW(handle, 0, path, ctypes.byref(length)):
            raise ctypes.WinError(ctypes.get_last_error())
        if not path.value.lower().endswith('\\shippingpc-bmgame.exe'):
            raise RuntimeError('Selected process is not Batman')
        print('PROCESS:', path.value)
        buffer = ctypes.create_string_buffer(2048)
        copied = ctypes.c_size_t()
        if not kernel.ReadProcessMemory(handle, 0x266F080, buffer, len(buffer), ctypes.byref(copied)) or copied.value != len(buffer):
            raise ctypes.WinError(ctypes.get_last_error())
        print('ENGINE_INI:', buffer.raw.decode('utf-16-le').split('\0')[0])
        pointer = ctypes.c_uint32()
        for label, address in [('FILE_MANAGER', 0x26667C0)]:
            if not kernel.ReadProcessMemory(handle, address, ctypes.byref(pointer), 4, ctypes.byref(copied)):
                raise ctypes.WinError(ctypes.get_last_error())
            print(label + ':', hex(pointer.value))
            manager = ctypes.create_string_buffer(32)
            if not kernel.ReadProcessMemory(handle, pointer.value, manager, 32, ctypes.byref(copied)):
                raise ctypes.WinError(ctypes.get_last_error())
            print('FILE_MANAGER_FIELDS:', [hex(value) for value in struct.unpack('<8I', manager.raw)])
            fields = struct.unpack('<8I', manager.raw)
            for name, data, count in [('USER_ROOT', fields[2], fields[3]), ('INSTALL_ROOT', fields[5], fields[6])]:
                if count < 1 or count > 32768:
                    raise RuntimeError('Invalid file manager root length')
                root = ctypes.create_string_buffer(count * 2)
                if not kernel.ReadProcessMemory(handle, data, root, len(root), ctypes.byref(copied)) or copied.value != len(root):
                    raise ctypes.WinError(ctypes.get_last_error())
                print(name + ':', root.raw.decode('utf-16-le').split('\0')[0])
            if not kernel.ReadProcessMemory(handle, pointer.value, ctypes.byref(pointer), 4, ctypes.byref(copied)):
                raise ctypes.WinError(ctypes.get_last_error())
            print('FILE_MANAGER_VTABLE:', hex(pointer.value))
            table = ctypes.create_string_buffer(96)
            if not kernel.ReadProcessMemory(handle, pointer.value, table, 96, ctypes.byref(copied)):
                raise ctypes.WinError(ctypes.get_last_error())
            print('FILE_MANAGER_METHODS:', [hex(value) for value in struct.unpack('<24I', table.raw)])
        for address in [0x1E6F8BC, 0x1E6F910, 0x1E6F928, 0x1E6F958, 0x1E6F984, 0x1E6F998, 0x1E6F9C4]:
            if not kernel.ReadProcessMemory(handle, address, buffer, len(buffer), ctypes.byref(copied)):
                raise ctypes.WinError(ctypes.get_last_error())
            print('PATH_LITERAL', hex(address), buffer.raw.decode('utf-16-le', errors='replace').split('\0')[0])
    finally:
        kernel.CloseHandle(handle)


if __name__ == '__main__':
    Main()
