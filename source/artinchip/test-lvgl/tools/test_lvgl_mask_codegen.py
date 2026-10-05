#!/usr/bin/env python3
"""Check real target radius-mask instructions with bounded synthetic circle tables.

The modeled table has one opacity per row. Translation must preserve every
output byte and coverage result. Memory reads outside arguments, stack and
tables fail closed. memset and out-of-line mask_mix are modeled helpers;
this does not emulate the GPU or prove absence of unrelated memory corruption.
"""
from pathlib import Path
import argparse,re,subprocess,sys
from test_lvgl_rect_codegen import MASK64,REG_INDEX,STACK,EXIT_ADDRESS,signed
import test_lvgl_rect_codegen as base

def run_mask(instructions, coords, x, y, length, radius, outer=False, fault=None, line=None):
    regs = [0] * 32
    regs[REG_INDEX["a0"]] = 0x100000
    regs[REG_INDEX["a1"]] = x & MASK64
    regs[REG_INDEX["a2"]] = y & MASK64
    regs[REG_INDEX["a3"]] = length
    regs[REG_INDEX["a4"]] = 0x200000
    regs[REG_INDEX["sp"]] = STACK
    regs[REG_INDEX["ra"]] = EXIT_ADDRESS
    memory = {}
    executing = False
    def store(address, value, size):
        if executing and not (0x100000 <= address and address+size <= 0x100000+length or
                              STACK-4096 <= address and address+size <= STACK+16):
            raise RuntimeError(f"Write outside mask/stack: 0x{address:x}")
        for i in range(size):
            memory[address + i] = (value >> (8 * i)) & 255

    def load(address, size):
        if any(address + i not in memory for i in range(size)):
            raise RuntimeError(f"Guard read outside modeled arguments/stack: 0x{address:x}")
        return sum(memory[address + i] << (8 * i) for i in range(size))

    for i in range(2048): memory[0x100000+i]=255
    for i in range(40): memory[0x200000+i]=0
    for i,v in enumerate(coords):store(0x200010+2*i,v,2)
    store(0x200018,radius,2)
    store(0x20001a,int(outer),1)
    store(0x200020,0x300000,8)
    for i in range(48):memory[0x300000+i]=0
    store(0x300008,0x310000,8)
    store(0x300010,0x320000,8)
    store(0x300018,0x330000,8)
    store(0x300028,radius,2)
    for i in range(radius+1):
        store(0x330000+2*i,i,2)
        store(0x320000+2*i,0,2)
        store(0x310000+i,128,1)
    if fault=="null":store(0x200020,0,8)
    if fault=="radius":store(0x300028,radius+1,2)
    if fault=="length":store(0x330000+radius*2,65535,2)
    if fault=="x":store(0x320000+(radius-1)*2,radius+1,2)
    if fault=="order":
        store(0x330000+(radius-1)*2,1,2)
        store(0x330000+radius*2,0,2)
    if line is not None:
        for i in range(56):memory[0x200000+i]=0
        ox,oy,side,flat,slope=line
        store(0x200018,side,1)
        store(0x20001a,ox,2);store(0x20001c,oy,2)
        store(0x200020,slope,4);store(0x200024,slope,4)
        store(0x200028,slope,4);store(0x20002c,255 if slope else 0,4)
        store(0x200030,int(flat),1)
    executing = True
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
    for _ in range(10000):
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
            if target == EXIT_ADDRESS:
                return reg("a0"), bytes(memory[0x100000+i] for i in range(length))
            raise RuntimeError(f"Unexpected guard indirect call/jump: 0x{target:x}")
        elif opcode in ("mveqz","mvnez"):
            if (reg(operands[2]) == 0) == (opcode == "mveqz"): put(operands[0],reg(operands[1]))
        elif opcode in ("neg", "negw"):
            value=-reg(operands[1])
            put(operands[0], signed(value,32) if opcode=="negw" else value)
        elif opcode == "not":
            put(operands[0],~reg(operands[1]))
        elif opcode == "addsl":
            put(operands[0],reg(operands[1])+(reg(operands[2])<<int(operands[3])))
        elif opcode in ("mulw","mul"):
            v=reg(operands[1])*reg(operands[2]);put(operands[0],signed(v,32) if opcode=="mulw" else v)
        elif opcode in ("lrhu","lrh","lrbu","lrb","srh","srb"):
            size=2 if "h" in opcode else 1
            addr=(reg(operands[1])+(reg(operands[2])<<int(operands[3]))) & MASK64
            if opcode.startswith("s"):store(addr,reg(operands[0]),size)
            else:
                v=load(addr,size);put(operands[0],v if opcode.endswith("u") else signed(v,8*size))
        elif opcode == "jal":
            target=operands[-1]
            if "mask_mix" in target:
                a,b=reg("a0"),reg("a1")
                put("a0", a if b>=252 else 0 if b<=2 else ((a*b)*0x8081)>>23)
            elif "lv_memset_00" in target:
                for i in range(reg("a1")):store(reg("a0")+i,0,1)
            else:raise RuntimeError(target)
        elif opcode == "nop":
            pass
        else:
            raise RuntimeError(f"Unsupported target instruction: {opcode} {operand_text}")
        pc = next_pc
    raise RuntimeError("Guard instruction limit exceeded")


parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--kind',choices=['radius','line'],default='radius')
parser.add_argument('--sdk',required=True,type=Path)
parser.add_argument('--library',required=True,type=Path)
parser.add_argument('--symbols',type=Path,help='Matching unstripped build library; instruction bytes must match')
parser.add_argument('--start',type=lambda v:int(v,0),help='Known baseline function start')
parser.add_argument('--size',type=lambda v:int(v,0),help='Known baseline function size')
args=parser.parse_args()
lib=args.library
function_name="lv_draw_mask_"+args.kind
start,size=args.start,args.size
if args.symbols:
 nm=subprocess.check_output([base.tool_path(args.sdk,'nm'),'-S',str(args.symbols)],text=True)
 match=re.search(r'^([0-9a-f]+)\s+([0-9a-f]+)\s+\w\s+'+function_name+r'$',nm,re.M)
 assert match,'Missing radius function symbol'
 start,size=int(match[1],16),int(match[2],16)
options=['--disassemble='+function_name] if start is None else [f'--start-address={start}',f'--stop-address={start+size}']
def disassemble(path):
 return subprocess.check_output([base.tool_path(args.sdk,'objdump'),'-d',*options,str(path)],text=True)
out=disassemble(lib)
if args.symbols:
 def codes(text):return re.findall(r'^\s*[0-9a-f]+:\s+([0-9a-f]+)\s+',text,re.M)
 assert codes(out) and codes(out)==codes(disassemble(args.symbols)), 'Packaged and build function bytes differ'
 print('PASS packaged/build',function_name,'instruction bytes match')
ins=[]
for line in out.splitlines():
 m=re.match(r'^\s*([0-9a-fA-F]+):\s+[0-9a-fA-F]+\s+([a-zA-Z][\w.]*)\s*(.*)$',line)
 if m:ins.append((int(m[1],16),m[2],m[3].split('#')[0].strip()))
assert ins,'No target function instructions found'
if args.kind == 'line':
 count=0
 for side in range(4):
  for flat in (False,True):
   for slope in (0,1024,-1024):
    for x in (-40,-1,0,1,15,40):
     for y in (-40,-2,-1,0,1,40):
      expected=run_mask(ins,(0,0,0,0),x+100,y+100,36,1,line=(100,100,side,flat,slope))
      for dx,dy in ((-100,-100),(-1,-1),(0,0),(100,-50),(-30000,-30000)):
       actual=run_mask(ins,(0,0,0,0),x+dx,y+dy,36,1,line=(dx,dy,side,flat,slope))
       assert actual==expected,(side,flat,slope,x,y,dx,dy)
       count+=1
 print('PASS line-mask target translation and bounded writes',count)
 sys.exit(0)
failures=[];count=0
for radius in [1,2,8,14,16,18,24,40]:
 for outer in [False,True]:
  for width,height in [(radius*2,radius*2),(radius*2+41,radius*2+7)]:
   for row in sorted(set([0,1,radius-1,radius,height-radius-1,height-radius,height-2,height-1])):
    for dx,dy in [(0,0),(-radius,-radius),(-width+1,-height+1),(-32760,-32760),(30000,30000),(-width//2,0),(0,-height//2)]:
     rect=(dx,dy,dx+width-1,dy+height-1)
     try:
      actual=run_mask(ins,rect,dx,dy+row,width,radius,outer)
      expected=run_mask(ins,(100,100,100+width-1,100+height-1),100,100+row,width,radius,outer)
      if actual!=expected:raise AssertionError('translation changed pixels or coverage')
     except Exception as e:
      if len(failures)<10:failures.append((rect,row,radius,outer,str(e)))
     count+=1
for fault in ['null','radius','length','x','order']:
 for outer in [False,True]:
  try:
   got=run_mask(ins,(0,0,35,35),0,0,36,18,outer,fault)
   assert got==(int(outer),bytes([255])*36), (fault,got)
  except Exception as e:
   if len(failures)<10:failures.append((fault,outer,str(e)))
  count+=1
print('vectors',count,'failures(first10)',failures)

if failures:sys.exit(1)
