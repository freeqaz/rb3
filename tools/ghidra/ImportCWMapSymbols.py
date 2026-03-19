# Import symbols from a CodeWarrior/Metrowerks linker map file into Ghidra.
#
# This is a Ghidra Python (Jython) script that runs inside Ghidra's Script Manager.
# It parses the .text, .data, .rodata, .bss, .sdata, .sbss sections from a CW map
# and creates labeled symbols at each address.
#
# For the .text section, it also creates Function objects.
#
# @author MiloHax
# @category Import
# @menupath Tools.Import CW Map Symbols

import re
from ghidra.program.model.symbol import SourceType, SymbolUtilities

# Ask for the map file
map_file = askFile("CodeWarrior Map File", "Import")
map_path = str(map_file.getAbsolutePath())

# Sections to import
CODE_SECTIONS = {".text", ".init"}
DATA_SECTIONS = {".rodata", ".data", ".sdata", ".sbss", ".bss", ".sdata2", ".sbss2"}
ALL_SECTIONS = CODE_SECTIONS | DATA_SECTIONS

# Parse the map
section_header_re = re.compile(r'^(\.\w+) section layout')
symbol_re = re.compile(
    r'^\s*([0-9a-fA-F]{8})\s+'   # starting address
    r'([0-9a-fA-F]{6,8})\s+'     # size
    r'([0-9a-fA-F]{8})\s+'       # virtual address
    r'([0-9a-fA-F]{6,8})\s+'     # file offset
    r'(\d+)\s+'                   # alignment
    r'(\S+)'                      # symbol name
)

current_section = None
in_section = False
past_header = False

symbols = []  # (name, vaddr, size, section, obj_file)

with open(map_path, 'r') as f:
    for line in f:
        m = section_header_re.match(line)
        if m:
            section_name = m.group(1)
            if section_name in ALL_SECTIONS:
                current_section = section_name
                in_section = True
                past_header = False
            else:
                in_section = False
                current_section = None
            continue

        if not in_section:
            continue

        if '---' in line:
            past_header = True
            continue

        if not past_header:
            continue

        stripped = line.strip()
        if not stripped:
            continue

        m = symbol_re.match(stripped)
        if not m:
            continue

        size = int(m.group(2), 16)
        vaddr = int(m.group(3), 16)
        alignment = int(m.group(5))
        name = m.group(6)

        # Skip alignment-1 entries (section/object headers)
        if alignment == 1:
            continue

        # Skip *fill* entries
        if '*fill*' in name:
            continue

        # Skip zero-size entries
        if size == 0:
            continue

        # Extract object file
        rest = stripped[m.end():].strip()
        obj_file = rest.split('\t')[-1].strip() if rest else ""

        symbols.append((name, vaddr, size, current_section, obj_file))

print("Parsed {} symbols from map file".format(len(symbols)))

# Apply symbols to the program
symbolTable = currentProgram.getSymbolTable()
listing = currentProgram.getListing()
functionManager = currentProgram.getFunctionManager()
addressFactory = currentProgram.getAddressFactory()
defaultSpace = addressFactory.getDefaultAddressSpace()

created_funcs = 0
created_labels = 0
skipped = 0
errors = 0

monitor.initialize(len(symbols))
monitor.setMessage("Importing CW map symbols...")

for i, (name, vaddr, size, section, obj_file) in enumerate(symbols):
    monitor.checkCancelled()
    monitor.incrementProgress(1)

    if i % 1000 == 0:
        print("Progress: {}/{}".format(i, len(symbols)))

    try:
        addr = defaultSpace.getAddress(vaddr)

        # Sanitize the symbol name for Ghidra
        safe_name = SymbolUtilities.replaceInvalidChars(name, True)

        # Create label
        createLabel(addr, safe_name, True, SourceType.IMPORTED)
        created_labels += 1

        # For code sections, create function if one doesn't exist
        if section in CODE_SECTIONS:
            func = functionManager.getFunctionAt(addr)
            if func is None:
                try:
                    func = createFunction(addr, safe_name)
                    if func is not None:
                        created_funcs += 1
                except Exception as e:
                    # Function creation can fail if the bytes don't look like code
                    pass

    except Exception as e:
        errors += 1
        if errors <= 10:
            print("Error at 0x{:08x} {}: {}".format(vaddr, name, e))

print("")
print("Import complete:")
print("  Labels created: {}".format(created_labels))
print("  Functions created: {}".format(created_funcs))
print("  Skipped: {}".format(skipped))
print("  Errors: {}".format(errors))
