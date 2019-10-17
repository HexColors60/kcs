#include <kcc.h>
#if !AMALGAMATION
# define INTERNAL
# define EXTERNAL extern
#endif
#include "abi.h"
#include "assemble.h"

#include <kcc/assert.h>
#include <ctype.h>
#include <stdarg.h>

#define SUFFIX(w) ((w) == 1 ? 'b' : (w) == 2 ? 'w' : (w) == 4 ? 'l' : 'q')
#define X87SFX(w) ((w) == 4 ? 's' : (w) == 8 ? 'l' : 't')
#define X87IFX(w) ((w) == 2 ? 's' : (w) == 4 ? 'l' : 'q')

#define I0(instr)           out("\t%s\n", instr)
#define I1(instr, a)        out("\t%s\t%s\n", instr, a)
#define I2(instr, a, b)     out("\t%s\t%s, %s\n", instr, a, b)
#define U1(instr, w, a)     out("\t%s%c\t%s\n", instr, SUFFIX(w), a)
#define U2(instr, w, a, b)  out("\t%s%c\t%s, %s\n", instr, SUFFIX(w), a, b)
#define X1(instr, w, a)     out("\t%s%c\t%s\n", instr, X87SFX(w), a);
#define Y1(instr, w, a)     out("\t%s%c\t%s\n", instr, X87IFX(w), a);

#define MAX_OPERAND_TEXT_LENGTH 256

static FILE *asm_output;

static const struct symbol *current_symbol;

static const char *reg_name[] = {
    "%al",   "%ax",   "%eax",  "%rax",
    "%cl",   "%cx",   "%ecx",  "%rcx",
    "%dl",   "%dx",   "%edx",  "%rdx",
    "%bl",   "%bx",   "%ebx",  "%rbx",
    "%spl",  "%sp",   "%esp",  "%rsp",
    "%bpl",  "%bp",   "%ebp",  "%rbp",
    "%sil",  "%si",   "%esi",  "%rsi",
    "%dil",  "%di",   "%edi",  "%rdi",
    "%r8b",  "%r8w",  "%r8d",  "%r8",
    "%r9b",  "%r9w",  "%r9d",  "%r9",
    "%r10b", "%r10w", "%r10d", "%r10",
    "%r11b", "%r11w", "%r11d", "%r11",
    "%r12b", "%r12w", "%r12d", "%r12",
    "%r13b", "%r13w", "%r13d", "%r13",
    "%r14b", "%r14w", "%r14d", "%r14",
    "%r15b", "%r15w", "%r15d", "%r15"
};

static const char *xmm_name[] = {
    "%xmm0",  "%xmm1",  "%xmm2",  "%xmm3",
    "%xmm4",  "%xmm5",  "%xmm6",  "%xmm7",
    "%xmm8",  "%xmm9",  "%xmm10", "%xmm11",
    "%xmm12", "%xmm13", "%xmm14", "%xmm15"
};

static const char *x87_name[] = {
    "%st(0)", "%st(1)", "%st(2)", "%st(3)",
    "%st(4)", "%st(5)", "%st(6)", "%st(7)"
};

static void out(const char *s, ...)
{
    va_list args;

    va_start(args, s);
    vfprintf(asm_output, s, args);
    va_end(args);
}

static const char *mnemonic(struct registr reg)
{
    int i, j;

    if (reg.r == IP) {
        assert(reg.w == 8);
        return "%rip";
    } else if (reg.r < XMM0) {
        i = 4 * (reg.r - 1);
        j = reg.w - 1;

        if (j == 3) j = 2;
        if (j == 7) j = 3;

        return reg_name[i + j];
    } else if (reg.r < ST0) {
        return xmm_name[reg.r - XMM0];
    } else {
        i = x87_stack_pos(reg.r);
        return x87_name[i];
    }
}

static const char *asm_address(struct address addr)
{
    static char buf[MAX_OPERAND_TEXT_LENGTH];

    struct registr reg = {0, 8};
    int w = 0;

    if (addr.sym) {
        w += sprintf(buf + w, "%s", sym_name(addr.sym));
        switch (addr.type) {
        case ADDR_GLOBAL_OFFSET:
            assert(addr.disp == 0);
            w += sprintf(buf + w, "@GOTPCREL");
            break;
        case ADDR_PLT:
            assert(addr.disp == 0);
            w += sprintf(buf + w, "@PLT");
            break;
        default:
            if (addr.disp != 0) {
                w += sprintf(buf + w, "%s%d",
                    (addr.disp > 0) ? "+" : "", addr.disp);
            }
            break;
        }
    } else if (addr.disp != 0) {
        w += sprintf(buf, "%d", addr.disp);
    }

    if (addr.base) {
        reg.r = addr.base;
        w += sprintf(buf + w, "(%s", mnemonic(reg));
        if (addr.offset) {
            reg.r = addr.offset;
            w += sprintf(buf + w, ",%s,%d", mnemonic(reg), addr.mult);
        }
        sprintf(buf + w, ")");
    }

    return buf;
}

static const char *immediate(struct immediate imm, int *size)
{
    static char buf[MAX_OPERAND_TEXT_LENGTH];

    if (imm.type == IMM_INT) {
        *size = imm.w;
        if (imm.w < 8) {
            sprintf(buf, "$%d",
                (imm.w == 1) ? imm.d.byte :
                (imm.w == 2) ? imm.d.word : imm.d.dword);
        } else {
            sprintf(buf, "$%lld", imm.d.qword);
        }

        return buf;
    }

    assert(imm.type == IMM_ADDR);
    assert(imm.d.addr.sym);
    assert(imm.d.addr.sym->symtype != SYM_STRING_VALUE);

    *size = 8;
    return asm_address(imm.d.addr);
}

INTERNAL void asm_init(FILE *output, const char *file)
{
    asm_output = output;
    if (file) {
        out("\t.file\t\"%s\"\n", file);
    }
}

INTERNAL int asm_symbol(const struct symbol *sym)
{
    /*
     * Labels stay in the same function context, otherwise flush to
     * write any end of function metadata.
     */
    if (sym->symtype != SYM_LABEL) {
        asm_flush();
        current_symbol = sym;
    }

    switch (sym->symtype) {
    case SYM_TENTATIVE:
        assert(is_object(sym->type));
        if (sym->linkage == LINK_INTERN)
            out("\t.local %s\n", sym_name(sym));
        out("\t.comm %s,%d,%d\n",
            sym_name(sym), size_of(sym->type), type_alignment(sym->type));
        break;
    case SYM_DEFINITION:
        if (is_function(sym->type)) {
            I0(".text");
            if (sym->linkage == LINK_EXTERN)
                I1(".globl", sym_name(sym));
            I2(".type", sym_name(sym), "@function");
            out("%s:\n", sym_name(sym));
        } else {
            I0(".data");
            if (sym->linkage == LINK_EXTERN)
                I1(".globl", sym_name(sym));
            out("\t.align\t%d\n", sym_alignment(sym));
            out("\t.type\t%s, @object\n", sym_name(sym));
            out("\t.size\t%s, %d\n", sym_name(sym), size_of(sym->type));
            out("%s:\n", sym_name(sym));
        }
        break;
    case SYM_STRING_VALUE:
        I0(".data");
        out("\t.align\t%d\n", sym_alignment(sym));
        out("\t.type\t%s, @object\n", sym_name(sym));
        out("\t.size\t%s, %d\n", sym_name(sym), size_of(sym->type));
        out("%s:\n", sym_name(sym));
        out("\t.string\t");
        fprintstr(asm_output, sym->value.string);
        out("\n");
        break;
    case SYM_CONSTANT:
        I0(".section\t.rodata");
        out("\t.align\t%d\n", sym_alignment(sym));
        out("%s:\n", sym_name(sym));
        if (is_float(sym->type)) {
            out("\t.long\t%llu\n", sym->value.constant.u & 0xFFFFFFFFu);
        } else if (is_double(sym->type)) {
            out("\t.quad\t%lld\n", sym->value.constant.i);
        } else {
            union {
                long double ld;
                int64_t i[2];
            } conv = {0};
            assert(is_long_double(sym->type));
            conv.ld = sym->value.constant.ld;
            out("\t.quad\t%lld\n", conv.i[0]);
            out("\t.quad\t%lld\n", conv.i[1] & 0xFFFF);
        }
        break;
    case SYM_LABEL:
        out("%s:\n", sym_name(sym));
        break;
    default:
        break;
    }

    return 0;
}

INTERNAL int asm_text(struct instruction instr)
{
    int ws = 0,
        wd = 0;
    const char
        *source = NULL,
        *destin = NULL;

    switch (instr.optype) {
    case OPT_REG:
    case OPT_REG_REG:
    case OPT_REG_MEM:
        ws = instr.source.reg.w;
        source = mnemonic(instr.source.reg);
        break;
    case OPT_IMM:
    case OPT_IMM_REG:
    case OPT_IMM_MEM:
        if (is_x64_jmp(instr.opcode)) {
            source = str_raw(instr.source.imm.d.addr.label_name);
        } else {
            source = immediate(instr.source.imm, &ws);
        }
        break;
    case OPT_MEM:
    case OPT_MEM_REG:
        ws = instr.source.mem.w;
        source = asm_address(instr.source.mem.addr);
        break;
    default:
        break;
    }

    switch (instr.optype) {
    case OPT_REG_REG:
    case OPT_MEM_REG:
    case OPT_IMM_REG:
        wd = instr.dest.reg.w;
        destin = mnemonic(instr.dest.reg);
        break;
    case OPT_REG_MEM:
    case OPT_IMM_MEM:
        wd = instr.dest.mem.w;
        destin = asm_address(instr.dest.mem.addr);
        break;
    default:
        break;
    }

    switch (instr.opcode) {
    case INSTR_ADD:      U2("add", wd, source, destin); break;
    case INSTR_ADDSD:    I2("addsd", source, destin); break;
    case INSTR_ADDSS:    I2("addss", source, destin); break;
    case INSTR_CVTSS2SD: I2("cvtss2sd", source, destin); break;
    case INSTR_CVTSD2SS: I2("cvtsd2ss", source, destin); break;
    case INSTR_CVTSI2SS: U2("cvtsi2ss", ws, source, destin); break;
    case INSTR_CVTSI2SD: U2("cvtsi2sd", ws, source, destin); break;
    case INSTR_CVTTSD2SI:U2("cvttsd2si", wd, source, destin); break;
    case INSTR_CVTTSS2SI:U2("cvttss2si", wd, source, destin); break;
    case INSTR_CDQ:      I0("cdq"); break;
    case INSTR_CQO:      I0("cqo"); break;
    case INSTR_DIV:      U1("div", ws, source); break;
    case INSTR_DIVSD:    I2("divsd", source, destin); break;
    case INSTR_DIVSS:    I2("divss", source, destin); break;
    case INSTR_SUB:      U2("sub", wd, source, destin); break;
    case INSTR_SUBSD:    I2("subsd", source, destin); break;
    case INSTR_SUBSS:    I2("subss", source, destin); break;
    case INSTR_NOT:      U1("not", ws, source); break;
    case INSTR_MUL:      U1("mul", ws, source); break;
    case INSTR_XOR:      U2("xor", wd, source, destin); break;
    case INSTR_AND:      U2("and", wd, source, destin); break;
    case INSTR_OR:       U2("or", wd, source, destin); break;
    case INSTR_SHL:      U2("shl", wd, source, destin); break;
    case INSTR_SHR:      U2("shr", wd, source, destin); break;
    case INSTR_SAR:      U2("sar", wd, source, destin); break;
    case INSTR_IDIV:     U1("idiv", ws, source); break;
    case INSTR_MOV:      U2("mov", wd, source, destin); break;
    case INSTR_MOVZX:
        assert(ws == 1 || ws == 2);
        assert(ws < wd);
        U2((ws == 1) ? "movzb" : "movzw", wd, source, destin);
        break;
    case INSTR_MOVSX:
        assert(ws == 1 || ws == 2 || ws == 4);
        assert(ws < wd);
        U2((ws == 1) ? "movsb" : (ws == 2) ? "movsw" : "movsl",
            wd, source, destin);
        break;
    case INSTR_MOVAPS:
        I2("movaps", source, destin);
        break;
    case INSTR_MOVSS:    I2("movss", source, destin); break;
    case INSTR_MOVSD:    I2("movsd", source, destin); break;
    case INSTR_MULSD:    I2("mulsd", source, destin); break;
    case INSTR_MULSS:    I2("mulss", source, destin); break;
    case INSTR_SETE:     I1("sete", source); break;
    case INSTR_SETA:     I1("seta", source); break;
    case INSTR_SETNA:    I1("setna", source); break;
    case INSTR_SETG:     I1("setg", source); break;
    case INSTR_SETNG:    I1("setng", source); break;
    case INSTR_SETP:     I1("setp", source); break;
    case INSTR_SETAE:    I1("setae", source); break;
    case INSTR_SETNAE:   I1("setnae", source); break;
    case INSTR_SETGE:    I1("setge", source); break;
    case INSTR_SETNGE:   I1("setnge", source); break;
    case INSTR_SETNP:    I1("setnp", source); break;
    case INSTR_SETNE:    I1("setne", source); break;
    case INSTR_TEST:     U2("test", wd, source, destin); break;
    case INSTR_UCOMISS:  I2("ucomiss", source, destin); break;
    case INSTR_UCOMISD:  I2("ucomisd", source, destin); break;
    case INSTR_CMP:      U2("cmp", wd, source, destin); break;
    case INSTR_LEA:      U2("lea", wd, source, destin); break;
    case INSTR_PUSH:     U1("push", ws, source); break;
    case INSTR_POP:      U1("pop", ws, source); break;
    case INSTR_PXOR:     I2("pxor", source, destin); break;
    case INSTR_JMP:      I1("jmp", source); break;
    case INSTR_JE:       I1("je", source); break;
    case INSTR_JA:       I1("ja", source); break;
    case INSTR_JNA:      I1("jna", source); break;
    case INSTR_JG:       I1("jg", source); break;
    case INSTR_JNG:      I1("jng", source); break;
    case INSTR_JS:       I1("js", source); break;
    case INSTR_JP:       I1("jp", source); break;
    case INSTR_JAE:      I1("jae", source); break;
    case INSTR_JNAE:     I1("jnae", source); break;
    case INSTR_JGE:      I1("jge", source); break;
    case INSTR_JNGE:     I1("jnge", source); break;
    case INSTR_JNE:      I1("jne", source); break;
    case INSTR_JNS:      I1("jns", source); break;
    case INSTR_CALL:
        if (instr.optype == OPT_REG)
            out("\tcall\t*%s\n", source);
        else
            I1("call", source);
        break;
    case INSTR_LEAVE:    I0("leave"); break;
    case INSTR_RET:      I0("ret"); break;
    case INSTR_REP_MOVSQ:I0("rep movsq"); break;
    case INSTR_FLD:      X1("fld", ws, source); break;
    case INSTR_FILD:     Y1("fild", ws, source); break;
    case INSTR_FSTP:
        if (instr.optype == OPT_REG) {
            I1("fstp", source);
        } else {
            X1("fstp", ws, source);
        }
        break;
    case INSTR_FXCH:     I1("fxch", source); break;
    case INSTR_FNSTCW:   I1("fnstcw", source); break;
    case INSTR_FLDCW:    I1("fldcw", source); break;
    case INSTR_FISTP:    Y1("fistp", ws, source); break;
    case INSTR_FUCOMIP:  I1("fucomip", source); break;
    case INSTR_FADDP:    I1("faddp", source); break;
    case INSTR_FSUBRP:   I1("fsubrp", source); break;
    case INSTR_FMULP:    I1("fmulp", source); break;
    case INSTR_FDIVRP:   I1("fdivrp", source); break;

    case INSTR_INC:      U1("inc", ws, source); break;
    case INSTR_DEC:      U1("dec", ws, source); break;

    case INSTR_BUILTIN:  I0("(builtin)");
    }

    return 0;
}

INTERNAL int asm_data(struct immediate data)
{
    switch (data.type) {
    case IMM_INT:
        if (data.w == 1)
            out("\t.byte\t%d\n", data.d.byte);
        else if (data.w == 2)
            out("\t.short\t%d\n", data.d.word);
        else if (data.w == 4)
            out("\t.int\t%d\n", data.d.dword);
        else {
            assert(data.w == 8);
            out("\t.quad\t%lld\n", data.d.qword);
        }
        break;
    case IMM_ADDR:
        assert(data.d.addr.sym);
        if (data.d.addr.disp) {
            out("\t.quad\t%s%s%d\n", sym_name(data.d.addr.sym),
                data.d.addr.disp < 0 ? "" : "+",
                data.d.addr.disp);
        } else
            out("\t.quad\t%s\n", sym_name(data.d.addr.sym));
        break;
    case IMM_STRING:
        if (data.w == data.d.string.len) {
            out("\t.ascii\t");
        } else {
            assert(data.w == data.d.string.len + 1);
            out("\t.string\t");
        }
        fprintstr(asm_output, data.d.string);
        out("\n");
        break;
    }
    return 0;
}

INTERNAL int asm_flush(void)
{
    if (current_symbol) {
        if (is_function(current_symbol->type) &&
                current_symbol->symtype == SYM_DEFINITION)
            out("\t.size\t%s, .-%s\n",
                sym_name(current_symbol), sym_name(current_symbol));
        current_symbol = NULL;
    }
    return 0;
}
