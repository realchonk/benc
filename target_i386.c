#include "iutil.h"
#include "igen.h"
#include "buf.h"

static char* string_pool = NULL;
static uint32_t alloc_str(const char* str) {
	const uint32_t pos = buf_len(string_pool);
	for (size_t i = 0; str[i]; ++i)
		buf_push(string_pool, str[i]);
	buf_push(string_pool, 0);
	return pos;
}

static const char* regs[] = { "eax", "ebx", "ecx", "edx", "esi", "edi" };
static int32_t get_off(const IUnit* unit, const char* name) {
	for (size_t i = 0; i < buf_len(unit->paramnames); ++i) {
		if (strcmp(unit->paramnames[i], name) == 0)
			return i * 4 + 8;
	}
	for (size_t i = 0; i < buf_len(unit->decls); ++i) {
		if (strcmp(unit->decls[i].name, name) == 0)
			return -i * 4 - 12;
	}
	return INT32_MAX;
}
static INode* translate(IUnit* unit, INode* n, FILE* f) {
	int32_t tmp;
	switch (n->type) {
	case IN_LABEL:  fprintf(f, ".l%u:\n", n->label); break;
	case IN_LDC:    fprintf(f, "mov %s, %jd\n", regs[n->ldc.dest], n->ldc.num); break;
	case IN_LDA:
		tmp = get_off(unit, n->lda.name);
		if (tmp == INT32_MAX) fprintf(f, "mov %s, %s\n", regs[n->lda.dest], n->lda.name);
		else fprintf(f, "lea %s, [ebp + %d]\n", regs[n->lda.dest], tmp);
		break;
	case IN_MOVE:   fprintf(f, "mov %s, %s\n", regs[n->move.dest], regs[n->move.src]); break;
	case IN_JMP:    fprintf(f, "jmp .l%u\n", n->label); break;
	case IN_JE:     fprintf(f, "je .l%u\n", n->label); break;
	case IN_JNE:    fprintf(f, "jne .l%u\n", n->label); break;
	case IN_JG:     fprintf(f, "jg .l%u\n", n->label); break;
	case IN_JL:     fprintf(f, "jl .l%u\n", n->label); break;
	case IN_RETURN: fprintf(f, "jmp .ret\n"); break;
	case IN_READ:   fprintf(f, "mov %s, dword [%s]\n", regs[n->move.dest], regs[n->move.src]); break;
	case IN_WRITE:  fprintf(f, "mov dword [%s], %s\n", regs[n->move.dest], regs[n->move.src]); break;
	case IN_PUSH:   fprintf(f, "push %s\n", regs[n->reg]); break;
	case IN_NOP:    fprintf(f, "nop\n"); break;
	case IN_CMP:    fprintf(f, "cmp %s, %s\n", regs[n->move.dest], regs[n->move.src]); break;
	case IN_CALL:   fprintf(f, "call %s\nadd esp, %d\n", regs[n->fcall.dest], n->fcall.pcount * 4); break;
	case IN_NEG:    fprintf(f, "neg %s\n", regs[n->reg]); break;
	case IN_ADD:    fprintf(f, "lea %s, [%s + %s]\n",
			regs[n->binary.dest], regs[n->binary.left], regs[n->binary.right]); break;
	case IN_SUB:    fprintf(f, "neg %s\nlea %s, [%s + %s]\n",regs[n->binary.right],
			regs[n->binary.dest], regs[n->binary.left], regs[n->binary.right]); break;
	case IN_AND:
		fprintf(f, "push %s\n", regs[n->binary.left]);
		fprintf(f, "and %s, %s\n", regs[n->binary.left], regs[n->binary.right]);
		fprintf(f, "mov %s, %s\n", regs[n->binary.dest], regs[n->binary.left]);
		fprintf(f, "pop %s\n", regs[n->binary.left]);
		break;
	case IN_OR:
		fprintf(f, "push %s\n", regs[n->binary.left]);
		fprintf(f, "or %s, %s\n", regs[n->binary.left], regs[n->binary.right]);
		fprintf(f, "mov %s, %s\n", regs[n->binary.dest], regs[n->binary.left]);
		fprintf(f, "pop %s\n", regs[n->binary.left]);
		break;
	case IN_XOR:
		fprintf(f, "push %s\n", regs[n->binary.left]);
		fprintf(f, "xor %s, %s\n", regs[n->binary.left], regs[n->binary.right]);
		fprintf(f, "mov %s, %s\n", regs[n->binary.dest], regs[n->binary.left]);
		fprintf(f, "pop %s\n", regs[n->binary.left]);
		break;
	case IN_LDS:
		fprintf(f, "mov %s, __string_pool + %u\n", regs[n->lda.dest], alloc_str(n->lda.name));
		break;
		
	default: break;
	}
	return n->next;
}

static int i386_gen_asm_f(IUnit * unit, FILE* f) {
	buf_free(string_pool);
	fprintf(f, "section .text\n");
	fprintf(f, "global %s:function (%s.end - %s)\n%s:\n", unit->name, unit->name, unit->name, unit->name);
	fprintf(f, "push ebp\nmov ebp, esp\npush ebx\npush esi\npush edi\n");
	if (unit->decls) {
		fprintf(f, "sub esp, %zu\n", buf_len(unit->decls) * 4);
		for (size_t i = 0; i < buf_len(unit->decls); ++i) {
			if (unit->decls[i].has_value)
				fprintf(f, "mov dword [ebp - %zu], %jd\n", i * 4 + 12, unit->decls[i].value);
		}
	}
	fputc('\n', f);
	
	for (INode* n = unit->nodes; n; n = translate(unit, n, f));
	fprintf(f, "\n.ret:\n");
	if (unit->decls) fprintf(f, "add esp, %zu\n", buf_len(unit->decls) * 4);
	fprintf(f, "pop edi\npop esi\npop ebx\npop ebp\nret\n.end:\n");
	fprintf(f, "\n\n; string section\n");
	if (string_pool) {
		fprintf(f, "__string_pool: db 0x%02X", string_pool[0]);
		for (size_t i = 1; i < buf_len(string_pool); ++i)
			fprintf(f, ", 0x%02X", string_pool[i]);
		fputc('\n', f);
	}
	return 0;
}
int i386_gen_asm(IProgram* prog, FILE* f) {
	for (size_t i = 0; i < buf_len(prog->externs); ++i)
		fprintf(f, "extern %s\n", prog->externs[i]);
	for (size_t i = 0; i < buf_len(prog->units); ++i)
		i386_gen_asm_f(prog->units[i], f);
	return 0;
}