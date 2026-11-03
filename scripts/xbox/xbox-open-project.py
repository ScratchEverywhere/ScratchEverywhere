"""GDB command: xbox-open-project D:/p.sb2 (before the first format detection).

Select a short DVD-root project path without rebuilding the Xbox executable.
Only an empty 32-bit little-endian NXDK/libc++ string may be changed. This is a
debugger-only input; the executable and game scripts are not patched.
"""
import re
import gdb


class XboxOpenProject(gdb.Command):
    def __init__(self):
        super().__init__("xbox-open-project", gdb.COMMAND_USER)

    def invoke(self, argument, from_tty):
        path = argument.strip().replace("/", "\\")
        if not re.fullmatch(r"[A-Za-z]:\\[A-Za-z0-9_.-]+\.(sb2|sb3|sb)", path):
            raise gdb.GdbError("Use a DVD-root project path such as D:/p.sb2.")
        if len(path) > 10:
            raise gdb.GdbError("The 32-bit libc++ short string holds at most 10 characters; use a short filename.")
        if gdb.lookup_type("void").pointer().sizeof != 4 or "little endian" not in gdb.execute("show endian", to_string=True):
            raise gdb.GdbError("This command requires the 32-bit little-endian Xbox target.")
        frame = gdb.newest_frame()
        if frame is None or "detectFormat" not in (frame.name() or ""):
            raise gdb.GdbError("Stop at Unzip::detectFormat before the initial project load.")
        symbols = gdb.execute("info variables filePath@Unzip", to_string=True)
        matches = re.findall(r"^(0x[0-9a-fA-F]+)\s+\?filePath@Unzip@@", symbols, re.MULTILINE)
        if len(matches) != 1:
            raise gdb.GdbError("Could not uniquely locate Unzip::filePath in the matching Xbox symbols.")
        address = int(matches[0], 16)
        inferior = gdb.selected_inferior()
        old = bytes(inferior.read_memory(address, 12))
        # The low bit marks a heap string; other bits hold a short string's size.
        if old[0] != 0:
            raise gdb.GdbError("Unzip::filePath must be empty; refusing to overwrite an existing project path.")
        encoded = path.encode("ascii")
        inferior.write_memory(address, bytes([len(encoded) * 2]) + encoded + bytes(11 - len(encoded)))
        gdb.write(f"Initial project path set to {path}; continue for normal format detection and loading.\n")


XboxOpenProject()
