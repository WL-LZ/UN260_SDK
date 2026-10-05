#!/usr/bin/env python3
"""Execute the target LVGL rectangle guard instructions without a target board.

This deliberately reads real RISC-V machine-code disassembly, rather than
matching source text or requiring a particular opcode. The model supplies
lv_draw_rect's three arguments and stops when it reaches the draw callback or
returns without drawing. Unsupported instructions fail closed.

Example:
  python3 test_lvgl_rect_codegen.py --sdk output/d211_d213_devkitf/host \
    --library output/d211_d213_devkitf/target/usr/local/lib/liblvgl.so \
    --object output/d211_d213_devkitf/build/lvgl-8.3.2/CMakeFiles/lvgl.dir/src/draw/lv_draw_rect.c.o

Add --source SOURCE/src/draw/lv_draw_rect.c --lvgl-root SOURCE to compile the
production source into an isolated temporary directory, with the LVGL O1 flags.
The emulator handles the SDK's O1/O2/Os leaf guard; it is not a general CPU model.
"""

import argparse
import hashlib
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile


MASK64 = (1 << 64) - 1
DRAW_CALLBACK = 0x70000000
EXIT_ADDRESS = 0x70000100
CTX, DSC, COORDS, STACK = 0x100000, 0x200000, 0x300000, 0x400000
REGISTER_NAMES = [
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2", "s0", "s1",
    "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "s2", "s3",
    "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6",
]
REG_INDEX = {name: i for i, name in enumerate(REGISTER_NAMES)}
REG_INDEX.update({f"x{i}": i for i in range(32)})
REG_INDEX["fp"] = 8

# Each vector is a full logical rectangle, before screen clipping. An entirely
# off-screen rectangle still dispatches the renderer, which owns its clipping.
CASES = [
    ("positive", (25, 65, 323, 160)),
    ("negative_x", (-324, 65, -26, 160)),
    ("negative_y", (25, -97, 323, -2)),
    ("cross_x_299", (-1, 65, 297, 160)),
    ("cross_x_middle", (-149, 65, 149, 160)),
    ("cross_y_96", (25, -1, 323, 94)),
    ("cross_both", (-149, -47, 149, 48)),
    ("one_pixel", (0, 0, 0, 0)),
    ("negative_one_pixel", (-1, -1, -1, -1)),
    ("empty_x", (30, 65, 29, 160)),
    ("empty_y", (25, 80, 323, 79)),
    ("inverted_x_across_zero", (0, 65, -1, 160)),
    ("inverted_y_across_zero", (25, 0, 323, -1)),
    ("inverted_both", (8, 12, -8, -12)),
    ("negative_inverted_x", (-2, -97, -3, -2)),
    ("near_negative_limit", (-32768, -32768, -32470, -32673)),
    ("near_positive_limit", (32469, 32672, 32767, 32767)),
]

# Model the actual component footprints across every pixel of their horizontal
# and vertical movement ranges. Include an empty rectangle with adjacent,
# inverted bounds at each position, so acceptance and rejection share edges.
for component, width, height in (
    ("quick_card", 299, 96), ("switch", 46, 26),
    ("knob", 18, 18), ("sheet", 1280, 328),
):
    for x in range(-1230, 1256):
        CASES.append((f"{component}/move_x/{x}", (x, 65, x + width - 1, 65 + height - 1)))
        CASES.append((f"{component}/empty_x/{x}", (x + 1, 65, x, 65 + height - 1)))
    for y in range(-328, 66):
        CASES.append((f"{component}/move_y/{y}", (25, y, 25 + width - 1, y + height - 1)))
        CASES.append((f"{component}/empty_y/{y}", (25, y + 1, 25 + width - 1, y)))


def signed(value, bits=64):
    value &= (1 << bits) - 1
    return value - (1 << bits) if value & (1 << (bits - 1)) else value


def run(command):
    result = subprocess.run(command, text=True, capture_output=True)
    if result.returncode:
        raise RuntimeError(f"Command failed: {' '.join(map(str, command))}\n{result.stderr}")
    return result.stdout


def tool_path(sdk, suffix):
    sdk = Path(sdk).resolve()
    for directory in (sdk / "bin", sdk):
        preferred = directory / f"riscv64-unknown-linux-gnu-{suffix}"
        if preferred.is_file():
            return str(preferred)
        candidates = sorted(directory.glob(f"riscv*-{suffix}"))
        if len(candidates) == 1:
            return str(candidates[0])
    raise RuntimeError(f"Cannot find RISC-V {suffix} under {sdk}")


def disassemble(objdump, artifact):
    output = run([objdump, "-d", "--disassemble=lv_draw_rect", str(artifact)])
    instructions = []
    for line in output.splitlines():
        match = re.match(r"^\s*([0-9a-fA-F]+):\s+[0-9a-fA-F]+\s+([a-zA-Z][\w.]*)\s*(.*)$", line)
        if match:
            address, opcode, operands = match.groups()
            operands = operands.split("#", 1)[0].strip()
            instructions.append((int(address, 16), opcode, operands))
    if not instructions or "<lv_draw_rect" not in output:
        raise RuntimeError(f"No lv_draw_rect instructions in {artifact}")
    return instructions


def instruction_digest(instructions):
    # Linking changes instruction addresses and branch labels. Normalize branch
    # destinations to instruction indices before comparing object and library.
    indices = {address: i for i, (address, _, _) in enumerate(instructions)}
    canonical = []
    for _, opcode, operands in instructions:
        pieces = [piece.strip() for piece in operands.split(",")]
        if opcode.startswith("b") or opcode in ("j", "jal"):
            address = int(pieces[-1].split()[0], 16)
            if address not in indices:
                raise RuntimeError(f"Guard branch leaves lv_draw_rect: {opcode} {operands}")
            pieces[-1] = f"instruction[{indices[address]}]"
        canonical.append(opcode + " " + ",".join(pieces))
    return hashlib.sha256("\n".join(canonical).encode()).hexdigest()


def reaches_draw(instructions, coords):
    regs = [0] * 32
    regs[REG_INDEX["a0"]] = CTX
    regs[REG_INDEX["a1"]] = DSC
    regs[REG_INDEX["a2"]] = COORDS
    regs[REG_INDEX["sp"]] = STACK
    regs[REG_INDEX["ra"]] = EXIT_ADDRESS
    memory = {}

    def store(address, value, size):
        for i in range(size):
            memory[address + i] = (value >> (8 * i)) & 255

    def load(address, size):
        if any(address + i not in memory for i in range(size)):
            raise RuntimeError(f"Guard read outside modeled arguments/stack: 0x{address:x}")
        return sum(memory[address + i] << (8 * i) for i in range(size))

    for i, byte in enumerate(struct.pack("<hhhh", *coords)):
        memory[COORDS + i] = byte
    # In this RV64 LVGL ABI draw_rect is the fourth draw-context pointer. Keep
    # other fields zero so an unexpected indirect target fails rather than
    # being silently treated as the callback.
    for i in range(256):
        memory[CTX + i] = 0
    store(CTX + 24, DRAW_CALLBACK, 8)
    index_for_address = {address: i for i, (address, _, _) in enumerate(instructions)}

    def reg(name):
        return regs[REG_INDEX[name]]

    def put(name, value):
        if REG_INDEX[name]:
            regs[REG_INDEX[name]] = value & MASK64

    def address(operand):
        match = re.fullmatch(r"(-?(?:0x[0-9a-fA-F]+|\d+))\((\w+)\)", operand)
        if not match:
            raise RuntimeError(f"Unsupported memory operand: {operand}")
        return (reg(match[2]) + int(match[1], 0)) & MASK64

    pc = 0
    for _ in range(256):
        if pc >= len(instructions):
            raise RuntimeError("Guard fell outside its function without return/callback")
        _, opcode, operand_text = instructions[pc]
        operands = [piece.strip() for piece in operand_text.split(",") if piece.strip()]
        next_pc = pc + 1
        if opcode in ("lh", "lhu", "lw", "lwu", "ld", "lb", "lbu"):
            size = {"lh": 2, "lhu": 2, "lw": 4, "lwu": 4, "ld": 8, "lb": 1, "lbu": 1}[opcode]
            value = load(address(operands[1]), size)
            put(operands[0], value if opcode.endswith("u") else signed(value, size * 8))
        elif opcode in ("sd", "sw", "sh", "sb"):
            store(address(operands[1]), reg(operands[0]), {"sd": 8, "sw": 4, "sh": 2, "sb": 1}[opcode])
        elif opcode in ("li", "mv", "lui"):
            value = reg(operands[1]) if opcode == "mv" else int(operands[1], 0)
            put(operands[0], value << 12 if opcode == "lui" else value)
        elif opcode in ("addi", "addiw", "add", "addw", "sub", "subw"):
            lhs = reg(operands[1])
            rhs = int(operands[2], 0) if opcode in ("addi", "addiw") else reg(operands[2])
            value = lhs - rhs if opcode.startswith("sub") else lhs + rhs
            put(operands[0], signed(value, 32) if opcode.endswith("w") else value)
        elif opcode in ("slli", "slliw", "srli", "srliw", "srai", "sraiw"):
            bits = 32 if opcode.endswith("w") else 64
            value = reg(operands[1]) & ((1 << bits) - 1)
            amount = int(operands[2], 0)
            value = (value << amount if opcode.startswith("sll") else
                     signed(value, bits) >> amount if opcode.startswith("sra") else value >> amount)
            put(operands[0], signed(value, 32) if bits == 32 else value)
        elif opcode in ("sext.w", "sext.h", "zext.h"):
            value = reg(operands[1])
            put(operands[0], value & 65535 if opcode == "zext.h" else signed(value, 32 if opcode == "sext.w" else 16))
        elif opcode in ("ext", "extu"):
            high, low = int(operands[2], 0), int(operands[3], 0)
            width = high - low + 1
            value = (reg(operands[1]) >> low) & ((1 << width) - 1)
            put(operands[0], signed(value, width) if opcode == "ext" else value)
        elif opcode in ("slti", "sltiu", "slt", "sltu"):
            lhs = reg(operands[1])
            rhs = int(operands[2], 0) if opcode.startswith("slti") else reg(operands[2])
            unsigned = opcode.endswith("u")
            put(operands[0], int((lhs if unsigned else signed(lhs)) < (rhs & MASK64 if unsigned else signed(rhs))))
        elif opcode in ("seqz", "snez", "sltz", "sgtz"):
            value = signed(reg(operands[1]))
            put(operands[0], int({"seqz": value == 0, "snez": value != 0, "sltz": value < 0, "sgtz": value > 0}[opcode]))
        elif opcode in ("and", "or", "xor", "andi", "ori", "xori"):
            lhs = reg(operands[1])
            rhs = int(operands[2], 0) if opcode.endswith("i") else reg(operands[2])
            put(operands[0], lhs & rhs if opcode.startswith("and") else lhs | rhs if opcode.startswith("or") else lhs ^ rhs)
        elif opcode.startswith("b"):
            zero_branch = opcode.endswith("z")
            lhs = reg(operands[0])
            rhs = 0 if zero_branch else reg(operands[1])
            relation = opcode[1:-1] if zero_branch else opcode[1:]
            unsigned = relation.endswith("u")
            relation = relation[:-1] if unsigned else relation
            lhs, rhs = (lhs, rhs) if unsigned else (signed(lhs), signed(rhs))
            conditions = {"eq": lhs == rhs, "ne": lhs != rhs, "lt": lhs < rhs,
                          "le": lhs <= rhs, "gt": lhs > rhs, "ge": lhs >= rhs}
            if relation not in conditions:
                raise RuntimeError(f"Unsupported branch: {opcode}")
            if conditions[relation]:
                target = int(operands[-1].split()[0], 16)
                if target not in index_for_address:
                    raise RuntimeError(f"Guard branch outside function: 0x{target:x}")
                next_pc = index_for_address[target]
        elif opcode == "j":
            next_pc = index_for_address[int(operands[0].split()[0], 16)]
        elif opcode in ("jalr", "jr", "ret"):
            if opcode == "ret":
                target = reg("ra")
            else:
                source = operands[-1]
                target = address(source) if "(" in source else reg(source)
            if target == DRAW_CALLBACK:
                return True
            if target == EXIT_ADDRESS:
                return False
            raise RuntimeError(f"Unexpected guard indirect call/jump: 0x{target:x}")
        elif opcode == "nop":
            pass
        else:
            raise RuntimeError(f"Unsupported target instruction: {opcode} {operand_text}")
        pc = next_pc
    raise RuntimeError("Guard instruction limit exceeded")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--sdk", required=True, help="Buildroot SDK/host directory containing bin/riscv*-objdump and gcc")
    parser.add_argument("--library", "--lib", type=Path, help="Actual built or packaged liblvgl.so")
    parser.add_argument("--object", type=Path, help="Actual production lv_draw_rect.c.o; compared with --library")
    parser.add_argument("--source", type=Path, help="Production lv_draw_rect.c to compile in a temporary directory")
    parser.add_argument("--lvgl-root", type=Path, help="Source LVGL root for --source; inferred from src/draw path")
    parser.add_argument("--sysroot", type=Path, help="SDK sysroot; defaults to riscv64-linux-gnu/sysroot under SDK")
    parser.add_argument("--optimization", choices=("O1", "O2", "Os"), default="O1")
    args = parser.parse_args()
    if not any((args.library, args.object, args.source)):
        parser.error("Provide --library, --object, or --source")
    objdump = tool_path(args.sdk, "objdump")
    failures = []
    failure_count = 0
    instruction_sets = {}
    with tempfile.TemporaryDirectory(prefix="un260-lvgl-rect-codegen-") as temporary:
        artifacts = []
        if args.library:
            artifacts.append(("library", args.library.resolve()))
        if args.object:
            artifacts.append(("object", args.object.resolve()))
        if args.source:
            source = args.source.resolve()
            lvgl_root = args.lvgl_root.resolve() if args.lvgl_root else source.parents[2]
            sysroot = args.sysroot.resolve() if args.sysroot else Path(args.sdk).resolve() / "riscv64-linux-gnu/sysroot"
            compiled = Path(temporary) / "lv_draw_rect.c.o"
            command = [tool_path(args.sdk, "gcc"), f"--sysroot={sysroot}",
                       "-DLV_CONF_INCLUDE_SIMPLE", "-DLV_LVGL_H_INCLUDE_SIMPLE", "-Dlvgl_EXPORTS",
                       "-isystem", str(lvgl_root), f"-{args.optimization}", "-g0", "-DNDEBUG", "-fPIC",
                       "-c", str(source), "-o", str(compiled)]
            run(command)
            artifacts.append((f"source_{args.optimization}", compiled))
        for label, artifact in artifacts:
            instructions = disassemble(objdump, artifact)
            instruction_sets[label] = instruction_digest(instructions)
            passed = 0
            for name, coords in CASES:
                expected = coords[0] <= coords[2] and coords[1] <= coords[3]
                actual = reaches_draw(instructions, coords)
                if actual == expected:
                    passed += 1
                else:
                    failure_count += 1
                    # A broken target produces many contiguous failures. Keep
                    # the log bounded while still checking every single case.
                    if len(failures) < 24:
                        failures.append(f"{label}/{name}: coords={coords}; expected draw={expected}, actual draw={actual}")
            print(f"{label}: {passed}/{len(CASES)} vectors; guard={instruction_sets[label]} ({artifact})")
        for other in ("object", f"source_{args.optimization}"):
            if "library" in instruction_sets and other in instruction_sets:
                if instruction_sets["library"] != instruction_sets[other]:
                    failure_count += 1
                    failures.append(f"Actual library guard does not match {other} guard")
                else:
                    print(f"library/{other}: target guard instructions match")
    if failure_count:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        if failure_count > len(failures):
            print(f"FAIL: {failure_count - len(failures)} additional mismatches; all {len(CASES)} vectors per artifact were checked", file=sys.stderr)
        return 1
    print("PASS: target rectangle guard accepts signed coordinates and rejects empty/inverted areas")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RuntimeError, ValueError, KeyError, OSError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(2)
