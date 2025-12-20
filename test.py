import os
import sys
import yaml
import re

db_inst_dir = os.path.expanduser("~/gitclones/riscv-unified-db/spec/std/isa/inst/")

ignore_insts = set([
    "fence",
    "fence.tso",
    "ebreak",
    "ecall",
    "mret",
    "wfi",
    "ld","sd",
    "addiw",

])

def add_tab(s:str) -> str:
    return '\n'.join(['\t' + line for line in s.splitlines()])

def expr_atbit(bit) -> str:
    return f"GET_BITAT({bit})"
def expr_bits(high:int, low:int) -> str:
    return f"GET_BITS({high}, {low})"

def expr_lshift(expr:str, amount:int) -> str:
    if amount == 0:
        return expr
    return f"({expr} << {amount})"
def expr_sext(expr:str) -> str:
    return f"SEXT({expr})"

def expr_typename_of_width(width) -> str:
    return f"Bits<{width}>"


# Given a field string like "12|10-11", return the expression and width
# think a|b|c means c is the least significant bits
def get_field_expr(s:str) -> (str,int):
    if s.isdigit():
        return (expr_atbit(int(s)), 1)
    else:
        groups = s.split("|")
        exprs = []
        width = 0
        for g in reversed(groups):
            if "-" in g:
                high, low = map(int, g.split("-"))
                exprs.append(expr_lshift(expr_bits(high, low), width))
                width += (high - low + 1)
            else:
                width += 1
                exprs.append(expr_lshift(expr_atbit(int(g)), width - 1))
        return (" | ".join(exprs), width)


idl_mappings = {
    "pc" : "GET_PC()",
    "signed" : "AS_SIGNED",
    "encoding" : "_UNUSED_INTVAL_",
}
def replace_idl_vars(match):
    var_name = match.group(1)
    return idl_mappings.get(var_name, "$"+var_name)


def generate_for_inst(ext_name:str,inst_name:str) -> str:
    filename = os.path.join(db_inst_dir, ext_name, f"{inst_name}.yaml")

    with open(filename, "r", encoding="utf-8") as f:
        data = yaml.safe_load(f)

    encoding = data["encoding"]
    enc_vars = encoding.get("variables", [])
    if encoding.get("RV32", None):
        enc_vars = encoding["RV32"].get("variables", enc_vars)


    upper_name = inst_name.upper()

    res = ""

    for var in enc_vars:
        sext = var.get("sign_extend", False)
        expr, width = get_field_expr(var["location"])

        raw_bits_type = expr_typename_of_width(width)

        if sext: # must before left shift
            inner_expr = f"{raw_bits_type}({expr})"
            res += f"XReg {var['name']} = {expr_sext(inner_expr)};\n"
        else:
            res += f"{raw_bits_type} {var['name']} = {expr};\n"

        lshift = var.get("left_shift", 0)
        if lshift > 0:
            res += f"{var['name']} = {expr_lshift(var['name'], lshift)};\n"

    res += "// operation:\n"

    var_name_pattern = "[a-zA-Z_][a-zA-Z0-9_]*"

    op = data["operation()"]
    # op = re.sub(r"Bits<(.*)>", lambda m: expr_typename_of_width(m.group(1)), op)
    op = re.sub(">>>", "SRA", op)

    # replace comments
    op = re.sub(r"#(.*)", r"// # \1", op)
    # replace $var
    op = re.sub(f"\\$({var_name_pattern})", replace_idl_vars, op)
    # replace 'number literals
    op = re.sub(r"(\w+)'(\d+)", r"Bits<\1>(\2)", op)
    op = re.sub(r"(\d+)'b([01])", r"Bits<\1>(\2)", op)
    op = re.sub(r"(\d+)'d(\d+)", r"Bits<\1>(\2)", op)
    op = re.sub(r"(\d+)'h([0-9a-fA-F]+)", r"Bits<\1>(0x\2)", op)

    op = re.sub(r"\s*'(\d+)", r" \1", op)
    # replace Verilog style bit selection
    op = re.sub(r"\[([^:\[\]]*):([^\[\]]*)\]", r" * Rng(\1, \2)", op)
    # replace Verilog single bit selection
    op = re.sub(r"\]\[(.*)\]", r"] * At(\1)", op)

    # replace Verilog n{} style replication
    op = re.sub(r"{([\w\d\(\)+-]+)\{", r"{Repl<\1>{", op)
    # fix list pass to noraml int variable
    op = re.sub(r"=\s*\{", r"= Concat{", op)
    # fix list pass to normal function
    op = re.sub(r"(\w+)\s*\(\{", r"\1(DXReg{", op)
    # fix list on the right side of three operand
    op = re.sub(r"\:\s*\{", r": WIDER_UNIT * DXReg{", op)

    # replace IDL wide multiply
    op = re.sub(r"`\*", "WIDE_MUL", op)

    # remove impl check
    op = re.sub(r"if\s+\(implemented\?.*?\}", "", op, flags=re.DOTALL)

    res += op
    res = add_tab(res)
    res = f"if (INST_IS({upper_name})) {{ \n\t// variables:\n" + res + "\n}\n"
    return res

print("// Auto-generated code for RISC-V instructions\n")
print("#include <stdint.h>")
print("""
#include "IDLHlper.hpp"
#include "encoding.out.h" 

#define INST() (instruction)
#define INST_IS(name) (INST() & MASK_##name) == MATCH_##name

void jump_halfword(uint32_t);
void jump(uint32_t);

void execute_instruction(uint32_t instruction) {
  uint32_t X[32];
""")

def gen_ext(ext_name:str) -> str:
    for files in os.listdir(os.path.join(db_inst_dir, ext_name)):
        if files.endswith(".yaml"):
            inst_name = files[:-5] 
            if inst_name in ignore_insts:
                continue
            print(add_tab(generate_for_inst(ext_name, inst_name)))

gen_ext("I")
gen_ext("M")

print("}")
