import os
import sys
import yaml
import re

db_git_dir = os.environ.get("RISCV_UNIFIED_DB_GIT_DIR", None)
if db_git_dir is None:
    print("Please set RISCV_UNIFIED_DB_GIT_DIR environment variable to the path of your local git clone of riscv-unified-db", file=sys.stderr)
    sys.exit(1)

db_git_dir = os.path.expanduser(db_git_dir)

db_inst_dir = os.path.join(db_git_dir, "spec/std/isa/inst/")

ignore_insts = set([
    # Currently unsupported generation, since X[..]=xxx not supported
    # "orc.b", 
    # "rev8",

    "fence",
    "fence.tso",
    "ebreak",
    "ecall",
    "mret",
    "wfi",
    "ld","sd", "lwu",
    # "slliw", "sllw", "sraiw", "sraw", "srlw",
    # "divw", "divuw", "remw", "remuw",
])

ignore_inst_patterns = [
    r".*\.uw",
    r"\b(?!(?:lw|sw)\b)\w*w\b", # ignore instructions with w suffix but not lw/sw
]

upper_name_RV32_list = [
    "RORI","REV8",
    "BCLRI", "BEXTI", "BINVI", "BSETI",
]


def should_ignore_inst(inst_name:str) -> bool:
    return inst_name in ignore_insts or any(
        re.fullmatch(pattern, inst_name) for pattern in ignore_inst_patterns
    )

def add_tab(s:str) -> str:
    return '\n'.join(['\t' + line for line in s.splitlines()])

def expr_lshift(expr:str, amount:int) -> str:
    if amount == 0:
        return expr
    return f"({expr} << {amount})"

def typename_of_bitw(width) -> str:
    return f"Bits<{width}>"


# Given a field string like "12|10-11", return the expression and width
# think a|b|c means c is the least significant bits
def get_field_expr(s:str) -> (str,int):
    def expr_atbit(bit) -> str:
        return f"InstAt({bit})"
    def expr_bits(high:int, low:int) -> str:
        return f"InstRng({high}, {low})"

    if s.isdigit():
        return (expr_atbit(int(s)), 1)
    else:
        groups = s.split("|")
        exprs = []
        width = 0
        for g in groups:
            if "-" in g:
                high, low = map(int, g.split("-"))
                exprs.append(expr_bits(high, low))
                width += (high - low + 1)
            else:
                width += 1
                exprs.append(expr_atbit(g))

        expr = exprs[0] if len(exprs) == 1 else "Concat{" + ", ".join(exprs) + "}"
        return (expr, width)


idl_mappings = {
    # Built-in variable 
    # https://github.com/riscv-software-src/riscv-unified-db/blob/main/doc/idl.adoc#builtin-variables
    "pc" : "*pc",
    "encoding" : "ENCODING_INST",
    # Built-in casting
    # https://github.com/riscv-software-src/riscv-unified-db/blob/main/doc/idl.adoc#explicit-casting
    "signed" : "sext",
    "bits" : "bits",
}
def replace_idl_builtin(match):
    var_name = match.group(1)
    return idl_mappings.get(var_name, "$"+var_name)

has_rv_zbkb = False

def generate_for_inst(ext_name:str,inst_name:str) -> str:
    filename = os.path.join(db_inst_dir, ext_name, f"{inst_name}.yaml")

    with open(filename, "r", encoding="utf-8") as f:
        data = yaml.safe_load(f)

    encoding = data.get("encoding", {})

    if "encoding" not in data:
        print(f"Warning: instruction {inst_name} has no encoding, using default rd/rs1/rs2", file=sys.stderr)

    enc_vars = encoding.get("variables", [])
    if encoding.get("RV32", None):
        enc_vars = encoding["RV32"].get("variables", enc_vars)

    if ext_name == "Zbkb":
        global has_rv_zbkb
        has_rv_zbkb = True

    upper_name = inst_name.upper().replace(".", "_")
    if upper_name == "ZEXT_H":
        if has_rv_zbkb:
            return f"// skip {upper_name} because it has encoding in Zbkb\n"
        upper_name = "PACK /* aka ZEXT.H ZEXT_H_RV32 */"

    if upper_name in upper_name_RV32_list:
        upper_name += "_RV32"

    res = ""

    ignore_var_names = set([
        "xd", "xs1", "xs2",
    ])

    for var in enc_vars:
        if var["name"] in ignore_var_names:
            continue
        sext = var.get("sign_extend", False)
        lshift = var.get("left_shift", 0)

        need_wrap = sext or lshift > 0

        field_expr, width = get_field_expr(var["location"])

        paded_width = width + lshift

        raw_type = typename_of_bitw(width)
        res_type = typename_of_bitw(paded_width) if not sext else "XReg"

        expr = f"{raw_type}({field_expr})" if need_wrap else field_expr

        res += f"{res_type} {var['name']} = {expr}"

        if sext: # must before left shift
            res += "\n\t.sign_extend()"

        if lshift > 0:
            res += f"\n\t.lshift_extend<{lshift}>()"

        res += ";\n"

    res += "// operation:\n"

    var_name_pattern = "[a-zA-Z_][a-zA-Z0-9_]*"

    op = data["operation()"]

    if len(str(op)) < 10:
        print(f"Warning: instruction {inst_name} has very short operation, maybe not implemented yet", file=sys.stderr)
        if upper_name == "PACK": # Patch current PACK instruction no operation field
            print(f"Info: manually patching PACK instruction operation field", file=sys.stderr)
            op = "X[xd] = {X[xs2][15:0], X[xs1][15:0]};"
        if upper_name == "PACKH":
            print(f"Info: manually patching PACKH instruction operation field", file=sys.stderr)
            op = "X[xd] = {X[xs2][7:0], X[xs1][7:0]};"

    # op = re.sub(r"Bits<(.*)>", lambda m: expr_typename_of_width(m.group(1)), op)
    op = re.sub(">>>", "Sra", op)

    # replace comments
    op = re.sub(r"#(.*)", r"// # \1", op)
    # replace $var
    op = re.sub(f"\\$({var_name_pattern})", replace_idl_builtin, op)
    # replace 'number literals
    op = re.sub(r"(\w+)'(\d+)", r"Bits<\1>(\2)", op)
    op = re.sub(r"(\d+)'b([01]+)", r"Bits<\1>(\2)", op)
    op = re.sub(r"(\d+)'d(\d+)", r"Bits<\1>(\2)", op)
    op = re.sub(r"(\d+)'h([0-9a-fA-F]+)", r"Bits<\1>(0x\2)", op)
    # single 'digit means MAXLEN, that is XReg
    op = re.sub(r"\s*'(\d+)", r" XReg(\1)", op)

    # replace Verilog style bit selection
    op = re.sub(r"\[([^:\[\]]*):([^\[\]]*)\]", r" * Rng(\1, \2)", op)
    # replace Verilog single bit selection
    op = re.sub(r"\]\[(.*?)\]", r"] * At(\1)", op)

    # replace Verilog n{} style replication
    op = re.sub(r"{([\w\d\(\)+-]+)\{", r"{Repl<\1>{", op)
    # fix list pass to noraml int variable
    op = re.sub(r"=\s*\{", r"= Concat{", op)
    # fix list pass to normal function
    op = re.sub(r"(\w+)\s*\(([\w,\s]*)\{", r"\1(\2Concat{", op)
    # fix list on the right side of three operand
    # now only handle one case inst div
    # has ambiguous ? signed 32bit : 64bit
    # so convert 64bit Concat to signed 64bit int
    op = re.sub(r"\:\s*\{", r": Concat{", op)

    # replace IDL wide multiply
    op = re.sub(r"`\*", "WIDE_MUL", op)

    # replace IDL xxx? blocks
    op = re.sub(r"implemented\?", "implemented", op)
    op = re.sub(r"compatible_mode\?", "compatible_mode", op)

    # replace CSR[csr_name]
    # op = re.sub(r"CSR\[(\w+)\]", lambda m: f"CSR[CSR_{m.group(1).upper()}]", op)
    op = re.sub(r"CSR\[misa\]\.M", "Bits<1>(1)", op)
    op = re.sub(r"CSR\[misa\]\.B", "Bits<1>(1)", op)
    op = re.sub(r"CSR\[(\w+)\].(\w+)", "false /* CSR[\\1].\\2 not implemented */", op)

    # Fix rv32 ? Bits<A> : Bits<B> to only return Bits<A>
    # To avoid ambiguity of return type
    op = re.sub(r"\(xlen\(\) == 32\) \? (.*) : (.*);", r"\1;", op)

    # Dynamic bit selection
    op = re.sub(r"Rng\(\(?i\+(\d)\)?, i\)", r"DynRng(i+\1, i)", op)
    op = re.sub(r"Rng\(j, \(j-7\)\)", "DynRng(j, j-7)", op)
    op = re.sub(r"Rng\((\d\*\w+\+\d*), (\d\*\w+)\)", r"DynRng(\1, \2)", op)

    # Patch

    if inst_name == "rori":
        op = re.sub(r"XReg shamt", "shamt", op) # fix `XReg shamt = .. shamt`

    if inst_name == "clmul" or inst_name == "clmulh" or inst_name == "clmulr":
        op = re.sub(r": output;", ": (dword_t)output;",op) # fix XReg and dword_t ambigious

    res += op
    res = add_tab(res)
    res = f"TRY_MATCH({upper_name},\n" + res + "\n);\n"
    return res

print("""// Auto-generated code
// Generated by test.py
// Do not edit manually!

#include "IDLHlper.hpp"

// return 0 if instruction matched and executed
extern "C" int execute_instruction(word_t ENCODING_INST, word_t* pc, word_t* regs) {
  INIT();
""")

def gen_ext(ext_name:str):
    for files in os.listdir(os.path.join(db_inst_dir, ext_name)):
        if files.endswith(".yaml"):
            inst_name = files[:-5] 
            if should_ignore_inst(inst_name):
                continue
            print(add_tab(generate_for_inst(ext_name, inst_name)))

gen_ext("I")
gen_ext("M")
# zba, zbb, zbc, zbs, zbkb 和 zbkx
gen_ext("Zbkb") # Zbkb should before ZEXT_H
gen_ext("Zba")
gen_ext("B") # To get full Zbb support
gen_ext("Zbb")
gen_ext("Zbc")
gen_ext("Zbs")
gen_ext("Zbkx")
# gen_ext("Zicsr")

print("\n\tFINISH();")
print("}")
