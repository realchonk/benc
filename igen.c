#define __STDC_WANT_LIB_EXT2__ 1
#include <stdlib.h>
#include <string.h>
#include "igen.h"
#include "buf.h"
#include "parser.h"

static IUnit* unit;
static INode* nodes = NULL;
static ireg_t reg = 0;
static unsigned lbl = 0;
static void igen_init(void) {
	nodes = alloc(INode);
	nodes->type = IN_NOP;
	nodes->next = nodes->prev = NULL;
	reg = 0;
	lbl = 0;
}

#define push(x) (nodes = inode_insert(nodes, x))
static void gen_expr(const Expression* expr);
static void gen_lv(const LValue* lv) {
	INode* n;
	switch (lv->type) {
	case LV_PAREN:  gen_lv(lv->lv); break;
	case LV_NAME:
		n = alloc(INode);
		n->type = IN_LDA;
		n->lda.dest = reg++;
		n->lda.name = lv->name;
		push(n);
		break;
	case LV_DEREF:  gen_expr(lv->expr); break;
	case LV_ASSIGN:
		gen_expr(lv->binary.right);
		gen_lv(lv->binary.left);
		n = alloc(INode);
		n->type = IN_WRITE;
		n->move.dest = reg - 1;
		n->move.src = reg - 2;
		push(n);
		n = alloc(INode);
		n->type = IN_MOVE;
		n->move.dest = reg - 2;
		n->move.src = reg - 1;
		push(n);
		--reg;
		break;
	case LV_AT:
		gen_lv(lv->binary.left);
		gen_expr(lv->binary.right);
		n = alloc(INode);
		n->type = IN_ADJOFF;
		n->reg = reg - 1;
		push(n);
		
		n = alloc(INode);
		n->type = IN_ADD;
		n->move.dest = reg - 2;
		n->move.src = reg - 1;
		push(n);
		--reg;
		break;
	}
}
static void gen_expr(const Expression* expr) {
	INode* n;
	switch (expr->type) {
	case EXPR_PAREN: gen_expr(expr->expr); break;
	case EXPR_NUMBER:
		n = alloc(INode);
		n->type = IN_LDC;
		n->ldc.dest = reg++;
		n->ldc.num = expr->num;
		push(n);
		break;
	case EXPR_UNARY:
		gen_expr(expr->unary.expr);
		if (expr->unary.op.type != TK_PLUS) {
			n = alloc(INode);
			switch (expr->binary.op.type) {
			case TK_MINUS:  n->type = IN_NEG;   break;
			default:        n->type = IN_NOP;   break;
			}
			n->reg = reg - 1;
			push(n);
		}
		break;
	case EXPR_BINARY:
		gen_expr(expr->binary.left);
		gen_expr(expr->binary.right);
		n = alloc(INode);
		switch (expr->binary.op.type) {
		case TK_PLUS:   n->type = IN_ADD;   break;
		case TK_MINUS:  n->type = IN_SUB;   break;
		case TK_AND:    n->type = IN_AND;   break;
		case TK_OR:     n->type = IN_OR;    break;
		case TK_XOR:    n->type = IN_XOR;   break;
		default:        n->type = IN_NOP;   break;
		}
		n->binary.dest = reg - 2;
		n->binary.left = reg - 2;
		n->binary.right = reg - 1;
		push(n);
		--reg;
		break;
	case EXPR_ADDROF: gen_lv(expr->lv); break;
	case EXPR_LVALUE:
		gen_lv(expr->lv);
		n = alloc(INode);
		n->type = IN_READ;
		n->move.dest = reg - 1;
		n->move.src = reg - 1;
		push(n);
		break;
	case EXPR_FCALL:
		for (size_t i = buf_len(expr->fcall.params); i; --i) {
			gen_expr(expr->fcall.params[i - 1]);
			n = alloc(INode);
			n->type = IN_PUSH;
			n->reg = --reg;
			push(n);
		}
		gen_lv(expr->lv);
		n = alloc(INode);
		n->type = IN_CALL;
		n->fcall.dest = reg - 1;
		n->fcall.pcount = buf_len(expr->fcall.params);
		push(n);
		reg = 1;
		break;
	case EXPR_STRING:
		n = alloc(INode);
		n->type = IN_LDS;
		n->lda.dest = reg++;
		n->lda.name = expr->buf;
		push(n);
		break;
	}
}
static bool iunit_is_declared(const IUnit* unit, const char* localvar) {
	for (size_t i = 0; i < buf_len(unit->decls); ++i) {
		if (strcmp(unit->decls[i].name, localvar) == 0) return true;
	}
	return false;
}
static void gen_bool(const BoolValue* bv) {
	INode* n;
	switch (bv->type) {
	// LDA R0, a
	// LDC R1, 0
	// CMP R0, R1
	// JE .l1       ; if !a
	// ...
	// .l1:
	case BOOL_EXPR:
		gen_expr(bv->expr);
		n = alloc(INode);
		n->type = IN_LDC;
		n->ldc.dest = reg;
		n->ldc.num = 0;
		push(n);
		n = alloc(INode);
		n->type = IN_CMP;
		n->move.dest = reg - 1;
		n->move.src = reg;
		push(n);
		n = alloc(INode);
		n->type = IN_JE;
		n->label = ++lbl;
		push(n);
		--reg;
		break;
	case BOOL_NOT:
		gen_expr(bv->expr);
		n = alloc(INode);
		n->type = IN_LDC;
		n->ldc.dest = reg;
		n->ldc.num = 0;
		push(n);
		n = alloc(INode);
		n->type = IN_CMP;
		n->move.dest = reg - 1;
		n->move.src = reg;
		push(n);
		n = alloc(INode);
		n->type = IN_JNE;
		n->label = ++lbl;
		push(n);
		--reg;
		break;
	/*
	 *  LDA R0, a
	 *  LDA R1, b
	 *  CMP R1, R0  ; reverse
	 *  Jcc .l1
	 *  ...
	 *  .l1:
	 */
	case BOOL_BINARY:
		gen_expr(bv->binary.left);
		gen_expr(bv->binary.right);
		n = alloc(INode);
		n->type = IN_CMP;
		n->move.dest = reg - 1;
		n->move.src = reg - 2;
		push(n);
		n = alloc(INode);
		switch (bv->binary.op.type) {
		case TK_EQEQ:   n->type = IN_JNE; break;
		case TK_GR:     n->type = IN_JG; break;
		case TK_LE:     n->type = IN_JL; break;
		default:        puts("an error occurred!"); break;
		}
		n->label = ++lbl;
		push(n);
		break;
	}
}
static void gen_stmt(const Statement* st) {
	unsigned tl;
	bool success, b;
	INode* n;
	n = alloc(INode);
	n->type = IN_BEG_STMT;
	push(n);
	reg = 0;
	switch (st->type) {
	case ST_NOP: // optional
		n = alloc(INode);
		n->type = IN_NOP;
		push(n);
		break;
	case ST_EXPR: gen_expr(st->expr); break;
	case ST_RETURN:
		gen_expr(st->expr);
		n = alloc(INode);
		n->type = IN_RETURN;
		push(n);
		break;
	case ST_COMP:
		// TODO: some sort of begin_scope & end_scope
		for (size_t i = 0; i < buf_len(st->stmts); ++i)
			gen_stmt(st->stmts[i]);
		break;
	case ST_VARDECL:
		for (size_t i = 0; i < buf_len(st->var_decls); ++i) {
			if (iunit_is_declared(unit, st->var_decls[i].name)) {
				printf("%zu:%zu: variable %s already declared!\n", st->row, st->col, st->var_decls[i].name);
				exit(1);
			}
			buf_push(unit->decls, st->var_decls[i]);
		}
		break;
	/*
	 * LDC R0, 1
	 * LDC R1, 0
	 * CMP R1, R0
	 * JNE .l1
	 * ...
	 * JMP .l2
	 * .l1:
	 */
	case ST_IF:
		b = eval_bool(st->ifstmt.cond, &success);
		if (success) {
			if (b) gen_stmt(st->ifstmt.true_case);
			else if (st->ifstmt.false_case)
				gen_stmt(st->ifstmt.false_case);
			return;
		}
		gen_bool(st->ifstmt.cond);
		tl = lbl;
		gen_stmt(st->ifstmt.true_case);
		if (st->ifstmt.false_case) {
			n = alloc(INode);
			n->type = IN_JMP;
			n->label = ++lbl;
			push(n);
			n = alloc(INode);
			n->type = IN_LABEL;
			n->label = tl++;
			push(n);
			gen_stmt(st->ifstmt.false_case);
		}
		n = alloc(INode);
		n->type = IN_LABEL;
		n->label = tl;
		push(n);
		break;
	case ST_WHILE:
		b = eval_bool(st->whileloop.cond, &success);
		if (success) {
			if (b) {
				n = alloc(INode);
				n->type = IN_LABEL;
				n->label = tl = ++lbl;
				push(n);
				gen_stmt(st->whileloop.body);
				n = alloc(INode);
				n->type = IN_JMP;
				n->label = tl;
				push(n);
			}
			return;
		}
		n = alloc(INode);
		n->type = IN_LABEL;
		n->label = tl = ++lbl;
		push(n);
		gen_bool(st->whileloop.cond);
		gen_stmt(st->whileloop.body);
		n = alloc(INode);
		n->type = IN_JMP;
		n->label = tl;
		push(n);
		n = alloc(INode);
		n->type = IN_LABEL;
		n->label = tl + 1;
		push(n);
		break;
	}
	n = alloc(INode);
	n->type = IN_END_STMT;
	push(n);
}
static void gen_func(const Function* func) {
	switch (func->type) {
	case FT_SIMPLE: gen_expr(func->value); break;
	case FT_COMPLEX:
		for (size_t i = 0; i < buf_len(func->body); ++i)
			gen_stmt(func->body[i]), reg = 0;
		break;
	}
	INode* const last = inode_last(nodes);
	if (!last->prev || last->prev->type != IN_RETURN) {
		INode* n = alloc(INode);
		n->type = IN_RETURN;
		push(n);
	}
}
#undef push

INode* igen_expr(const Expression* expr) {
	igen_init();
	gen_expr(expr);
	return inode_remove_first(nodes);
}
IUnit* igen_func(const Function* func) {
	unit = alloc(IUnit);
	unit->name = strdup(func->name);
	unit->paramnames = NULL;
	for (size_t i = 0; i < buf_len(func->paramnames); ++i)
		buf_push(unit->paramnames, func->paramnames[i]);
	igen_init();
	gen_func(func);
	unit->nodes = inode_remove_first(nodes);
	return unit;
}
IProgram* igen_prog(const Program* prog) {
	IProgram* ip = alloc(IProgram);
	ip->externs = prog->externs;
	for (size_t i = 0; i < buf_len(prog->funcs); ++i)
		buf_push(ip->units, igen_func(prog->funcs[i]));
	return ip;
}
const char* inode_names[NUM_INODES] = {
	"NOP", "MOVE", "LDC", "ADD", "SUB",
	"AND", "OR", "XOR", "NEG", "LDA",
	"CALL", "READ", "WRITE", "PUSH",
	"ADJOFF", "RETURN", "CMP", "LABEL",
	"JMP", "JE", "JNE", "JG", "JL",
	"LDS",
};
void print_inode(const INode* node, FILE* f) {
	if (node->type >= NUM_INODES) return;
	else if (node->type == IN_LABEL) {
		fprintf(f, ".l%u:\n", node->label);
		return;
	}
	fputs(inode_names[node->type], f);
	switch (node->type) {
	case IN_READ:
	case IN_WRITE:
	case IN_CMP:
	case IN_MOVE:   fprintf(f, " R%u, R%u", node->move.dest, node->move.src); break;
	case IN_LDC:    fprintf(f, " R%u, %jd", node->ldc.dest, node->ldc.num); break;
	case IN_LDA:    fprintf(f, " R%u, %s", node->lda.dest, node->lda.name); break;
	case IN_ADD:
	case IN_SUB:
	case IN_AND:
	case IN_OR:
	case IN_XOR:
		fprintf(f, " R%u, R%u, R%u", node->binary.dest,
				node->binary.left, node->binary.right);
		break;
	case IN_PUSH:
	case IN_ADJOFF:
	case IN_NEG:    fprintf(f, " R%u", node->reg); break;
	case IN_CALL:
		fprintf(f, " R%u, %u", node->fcall.dest, node->fcall.pcount);
		break;
	case IN_JE:
	case IN_JG:
	case IN_JL:
	case IN_JNE:
	case IN_JMP:
		fprintf(f, " .l%u", node->label);
		break;
	default: break;
	}
	fputc('\n', f);
}
static void print_decl(const struct VarDecl* decl, FILE* f) {
	fputs(decl->name, f);
	if (decl->has_value)
		fprintf(f, "=%jd", decl->value);
}
void print_iunit(const IUnit* unit, FILE* f) {
	fprintf(f, "unit %s (", unit->name);
	if (unit->paramnames) {
		fputs(unit->paramnames[0], f);
		for (size_t i = 1; i < buf_len(unit->paramnames); ++i)
			fprintf(f, ", %s", unit->paramnames[i]);
	}
	fputs(") [", f);
	if (unit->decls) {
		print_decl(&unit->decls[0], f);
		for (size_t i = 1; i < buf_len(unit->decls); ++i)
			fputc(',', f), print_decl(unit->decls+i, f);
	}
	fputs("]:\n", f);
	print_inodes(unit->nodes, f);
	fprintf(f, "end unit %s\n", unit->name);
}
void print_iprog(const IProgram* prog, FILE* f) {
	for (size_t i = 0; i < buf_len(prog->units); ++i)
		print_iunit(prog->units[i], f);
}
void free_inode(INode* node) {
	free(node);
}
void free_iunit(IUnit* unit) {
	free(unit->name);
	for (size_t i = 0; i < buf_len(unit->paramnames); ++i)
		free(unit->paramnames[i]);
	buf_free(unit->paramnames);
	free_inodes(unit->nodes);
	free(unit);
}
void free_iprog(IProgram* prog) {
	for (size_t i = 0; i < buf_len(prog->units); ++i)
		free_iunit(prog->units[i]);
	buf_free(prog->units);
}
IProgram* optimize_iprog(IProgram* prog) {
	for (size_t i = 0; i < buf_len(prog->units); ++i)
		prog->units[i] = optimize_iunit(prog->units[i]);
	return prog;
}