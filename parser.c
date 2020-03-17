#include <stdnoreturn.h>
#include <stdarg.h>
#include <stdlib.h>
#include "parser.h"
#include "buf.h"

#define peek lexer_peek
#define next lexer_next
#define matches(x) (peek().type == x)
#define getrow() (peek().row)
#define getcol() (peek().col)
#define alloc(t) ((t*)calloc(1, sizeof(t)))
noreturn static void syntax_error(const char* msg, size_t row, size_t col, ...) {
	va_list ap;
	va_start(ap, col);
	fprintf(stderr, "%zu:%zu: ", row, col);
	vfprintf(stderr, msg, ap);
	fputc('\n', stderr);
	va_end(ap);
	exit(1);
}
static bool match(enum TokenType type) {
	if (matches(type)) return next(), true;
	else return false;
}
static Token expect(enum TokenType type) {
	if (matches(type)) return next();
	else syntax_error("expected %s got %s", getrow(), getcol(), tk_names[type], tk_names[peek().type]);
}

void parser_init(void) {}

static LValue* lv_prim(void) {
	if (matches(TK_NAME)) {
		LValue* lv = alloc(LValue);
		lv->type = LV_NAME;
		lv->row = getrow();
		lv->col = getcol();
		lv->name = next().text;
		return lv;
	}
	else if (match(TK_LPAREN)) {
		LValue* lv = alloc(LValue);
		lv->type = LV_PAREN;
		lv->row = getrow();
		lv->col = getcol();
		lv->lv = parse_lv();
		expect(TK_RPAREN);
		return lv;
	}
	else syntax_error("expected lvalue got %s", getrow(), getcol(), tk_names[peek().type]);
}
static LValue* lv_unary(void) {
	if (matches(TK_STAR)) {
		LValue* lv = alloc(LValue);
		lv->type = LV_DEREF;
		lv->row = getrow();
		lv->col = getcol(); next();
		lv->expr = parse_expr();
		return lv;
	}
	else return lv_prim();
}
static LValue* lv_at(void) {
	LValue* left = lv_unary();
	while (match(TK_LBRACK)) {
		LValue* lv = alloc(LValue);
		lv->type = LV_AT;
		lv->row = getrow();
		lv->col = getcol();
		lv->binary.left = left;
		lv->binary.right = parse_expr();
		left = lv;
		expect(TK_RBRACK);
	}
	return left;
}
LValue* parse_lv(void) {
	LValue* left = lv_at();
	while (match(TK_EQUALS)) {
		LValue* lv = alloc(LValue);
		lv->type = LV_ASSIGN;
		lv->row = getrow();
		lv->col = getcol();
		lv->binary.left = left;
		lv->binary.right = parse_expr();
		left = lv;
	}
	return left;
}

static Expression* expr_prim(void) {
	Expression* expr = alloc(Expression);
	if (matches(TK_INTEGER)) {
		expr->type = EXPR_NUMBER;
		expr->num = next().num;
		expr->row = getrow();
		expr->col = getcol();
	}
	else if (matches(TK_STRING)) {
		expr->type = EXPR_STRING;
		expr->buf = next().text;
		expr->row = getrow();
		expr->col = getcol();
	}
	else if (match(TK_LPAREN)) {
		expr->type = EXPR_PAREN;
		expr->row = getrow();
		expr->col = getcol();
		expr->expr = parse_expr();
		expect(TK_RPAREN);
	}
	else {
		expr->row = getrow();
		expr->col = getcol();
		LValue* lv = parse_lv();
		if (match(TK_LPAREN)) {
			expr->type = EXPR_FCALL;
			expr->fcall.func = lv;
			expr->fcall.params = NULL;
			if (!matches(TK_RPAREN)) {
				do {
					buf_push(expr->fcall.params, parse_expr());
				}
				while (match(TK_COMMA));
			}
			expect(TK_RPAREN);
		}
		else {
			expr->type = EXPR_LVALUE;
			expr->lv = lv;
		}
	}
	return expr;
}
static Expression* expr_unary(void) {
	if (matches(TK_MINUS) || matches(TK_PLUS)) {
		Expression* expr = alloc(Expression);
		expr->type = EXPR_UNARY;
		expr->unary.op = next();
		expr->row = getrow();
		expr->col = getcol();
		expr->unary.expr = expr_unary();
		return expr;
	}
	else if (match(TK_AND)) {
		Expression* expr = alloc(Expression);
		expr->type = EXPR_ADDROF;
		expr->row = getrow();
		expr->col = getcol();
		expr->lv = parse_lv();
		return expr;
	}
	else return expr_prim();
}
static Expression* expr_bitwise(void) {
	Expression* left = expr_unary();
	while (matches(TK_AND) || matches(TK_OR) || matches(TK_XOR)) {
		Expression* expr = alloc(Expression);
		expr->type = EXPR_BINARY;
		expr->binary.op = next();
		expr->row = getrow();
		expr->col = getcol();
		expr->binary.left = left;
		expr->binary.right = expr_unary();
		left = expr;
	}
	return left;
}
Expression* parse_expr(void) {
	Expression* left = expr_bitwise();
	while (matches(TK_PLUS) || matches(TK_MINUS)) {
		Expression* expr = alloc(Expression);
		expr->type = EXPR_BINARY;
		expr->binary.op = next();
		expr->row = getrow();
		expr->col = getcol();
		expr->binary.left = left;
		expr->binary.right = expr_bitwise();
		left = expr;
	}
	return left;
}
BoolValue* parse_bool(void) {
	BoolValue* bv = alloc(BoolValue);
	bv->row = getrow();
	bv->col = getcol();
	if (match(TK_NOT)) {
		bv->type = BOOL_NOT;
		bv->expr = parse_expr();
	}
	else {
		Expression* left = parse_expr();
		if (matches(TK_EQEQ) || matches(TK_GR) || matches(TK_LE)) {
			bv->type = BOOL_BINARY;
			bv->binary.op = next();
			bv->binary.left = left;
			bv->binary.right = parse_expr();
		}
		else {
			bv->type = BOOL_EXPR;
			bv->expr = left;
		}
	}
	return bv;
}
Statement* parse_stmt(void) {
	if (match(TK_SEMICOLON)) {
		Statement* st = alloc(Statement);
		st->type = ST_NOP;
		st->row = getrow();
		st->col = getcol();
		return st;
	}
	else if (match(KW_RETURN)) {
		Statement* st = alloc(Statement);
		st->type = ST_RETURN;
		st->row = getrow();
		st->col = getcol();
		st->expr = parse_expr();
		expect(TK_SEMICOLON);
		return st;
	}
	else if (match(KW_WHILE)) {
		Statement* st = alloc(Statement);
		st->type = ST_WHILE;
		st->row = getrow();
		st->col = getcol();
		expect(TK_LPAREN);
		st->whileloop.cond = parse_bool();
		expect(TK_RPAREN);
		st->whileloop.body = parse_stmt();
		return st;
	}
	else if (match(KW_IF)) {
		Statement* st = alloc(Statement);
		st->type = ST_IF;
		st->row = getrow();
		st->col = getcol();
		expect(TK_LPAREN);
		st->ifstmt.cond = parse_bool();
		expect(TK_RPAREN);
		st->ifstmt.true_case = parse_stmt();
		st->ifstmt.false_case = match(KW_ELSE) ? parse_stmt() : NULL;
		return st;
	}
	else if (match(KW_VAR)) {
		Statement* st = alloc(Statement);
		st->type = ST_VARDECL;
		st->row = getrow();
		st->col = getcol();
		st->var_decls = NULL;
		bool success = false;
		do {
			struct VarDecl decl;
			decl.name = expect(TK_NAME).text;
			decl.has_value = match(TK_EQUALS);
			if (decl.has_value) {
				decl.value = eval_expr(parse_expr(), &success);
				if (!success) syntax_error("expected constant expression", st->row, st->col);
			}
			buf_push(st->var_decls, decl);
		} while (match(TK_COMMA));
		expect(TK_SEMICOLON);
		return st;
	}
	else if (match(TK_CLPAREN)) {
		Statement* st = alloc(Statement);
		st->row = getrow();
		st->col = getcol();
		st->type = ST_COMP;
		st->stmts = NULL;
		while (!match(TK_CRPAREN))
			buf_push(st->stmts, parse_stmt());
		return st;
	}
	else {
		Statement* st = alloc(Statement);
		st->row = getrow();
		st->col = getcol();
		st->type = ST_EXPR;
		st->expr = parse_expr();
		expect(TK_SEMICOLON);
		return st;
	}
}
Function* parse_func(void) {
	expect(KW_FUNC);
	Function* func = alloc(Function);
	func->row = getrow();
	func->col = getcol();
	func->name = expect(TK_NAME).text;
	func->paramnames = NULL;
	expect(TK_LPAREN);
	if (!matches(TK_RPAREN)) {
		do {
			buf_push(func->paramnames, expect(TK_NAME).text);
		}
		while (match(TK_COMMA));
	}
	expect(TK_RPAREN);
	if (match(TK_EQUALS)) {
		func->type = FT_SIMPLE;
		func->value = parse_expr();
		expect(TK_SEMICOLON);
	}
	else {
		expect(TK_CLPAREN);
		func->type = FT_COMPLEX;
		func->body = NULL;
		while (!match(TK_CRPAREN))
			buf_push(func->body, parse_stmt());
	}
	return func;
}
Program* parse_prog(void) {
	Program* prog = alloc(Program);
	while (!lexer_eof()) {
		if (match(KW_EXTERN)) {
			buf_push(prog->externs, expect(TK_NAME).text);
			expect(TK_SEMICOLON);
		}
		else buf_push(prog->funcs, parse_func());
	}
	return prog;
}

void print_lv(const LValue* lv, FILE* f) {
	switch (lv->type) {
	case LV_NAME:   fputs(lv->name, f); break;
	case LV_DEREF:  fputc('*', f), print_expr(lv->expr, f); break;
	case LV_ASSIGN:
		print_lv(lv->binary.left, f);
		fputs(" = ", f);
		print_expr(lv->binary.right, f);
		break;
	case LV_AT:
		print_lv(lv->binary.left, f);
		fputc('[', f);
		print_expr(lv->binary.right, f);
		fputc(']', f);
		break;
	case LV_PAREN: fputc('(', f), print_lv(lv->lv, f), fputc(')', f); break;
	}
}
void print_expr(const Expression* expr, FILE* f) {
	switch (expr->type) {
	case EXPR_NUMBER:   fprintf(f, "%ju", expr->num); break;
	case EXPR_LVALUE:   print_lv(expr->lv, f); break;
	case EXPR_ADDROF:   fputc('&', f), print_lv(expr->lv, f); break;
	case EXPR_PAREN:
		fputc('(', f);
		print_expr(expr->expr, f);
		fputc(')', f);
		break;
	case EXPR_UNARY:
		print_token(expr->unary.op, f);
		print_expr(expr->unary.expr, f);
		break;
	case EXPR_BINARY:
		print_expr(expr->binary.left, f);
		print_token(expr->binary.op, f);
		print_expr(expr->binary.right, f);
		break;
	case EXPR_FCALL:
		print_lv(expr->fcall.func, f);
		fputc('(', f);
		if (expr->fcall.params) {
			print_expr(expr->fcall.params[0], f);
			for (size_t i = 1; i < buf_len(expr->fcall.params); ++i)
				fputs(", ", f), print_expr(expr->fcall.params[i], f);
		}
		fputc(')', f);
		break;
	case EXPR_STRING:
		fputs(expr->buf, f);
		break;
	}
}
void print_bool(const BoolValue* bv, FILE* f) {
	switch (bv->type) {
	case BOOL_NOT: fputc('!', f);
	case BOOL_EXPR: print_expr(bv->expr, f); break;
	case BOOL_BINARY:
		print_expr(bv->binary.left, f);
		fprintf(f, " %s ", tk_names[bv->binary.op.type]);
		print_expr(bv->binary.right, f);
		break;
	}
}
void print_stmt(const Statement* stmt, FILE* f) {
	switch (stmt->type) {
	case ST_RETURN: fputs("return ", f);
	case ST_EXPR: print_expr(stmt->expr, f);
	case ST_NOP: fputs(";\n", f); break;
	case ST_WHILE:
		fputs("while (", f);
		print_bool(stmt->whileloop.cond, f);
		fputs(") ", f);
		print_stmt(stmt->whileloop.body, f);
		break;
	case ST_IF:
		fputs("if (", f);
		print_bool(stmt->ifstmt.cond, f);
		fputs(") ", f);
		print_stmt(stmt->ifstmt.true_case, f);
		if (stmt->ifstmt.false_case)
			fputs("else ", f), print_stmt(stmt->ifstmt.false_case, f);
		break;
	case ST_COMP:
		fputs("{\n", f);
		for (size_t i = 0; i < buf_len(stmt->stmts); ++i)
			print_stmt(stmt->stmts[i], f);
		fputs("}\n", f);
		break;
	case ST_VARDECL:
		// TODO
		break;
	}
}
void print_func(const Function* func, FILE* f) {
	fprintf(f, "func %s(", func->name);
	if (func->paramnames) {
		fputs(func->paramnames[0], f);
		for (size_t i = 1; i < buf_len(func->paramnames); ++i)
			fprintf(f, ", %s", func->paramnames[i]);
	}
	fputs(") ", f);
	switch (func->type) {
	case FT_SIMPLE: fputs("= ", f), print_expr(func->value, f); break;
	case FT_COMPLEX:
		fputs("{\n", f);
		for (size_t i = 0; i < buf_len(func->body); ++i)
			print_stmt(func->body[i], f);
		fputc('}', f);
		break;
	default: break;
	}
	fputc('\n', f);
}
void print_prog(const Program* prog, FILE* f) {
	for (size_t i = 0; i < buf_len(prog->externs); ++i)
		fprintf(f, "extern %s;\n", prog->externs[i]);
	for (size_t i = 0; i < buf_len(prog->funcs); ++i)
		print_func(prog->funcs[i], f);
}

void free_lv(LValue* lv) {
	switch (lv->type) {
	case LV_NAME:   free(lv->name); break;
	case LV_ASSIGN:
	case LV_AT:     free_lv(lv->binary.left), free_expr(lv->binary.right); break;
	case LV_DEREF:  free_expr(lv->expr); break;
	case LV_PAREN:  free_lv(lv->lv); break;
	}
	free(lv);
}
void free_expr(Expression* expr) {
	switch (expr->type) {
	case EXPR_NUMBER:   break;
	case EXPR_PAREN:    free_expr(expr->expr); break;
	case EXPR_ADDROF:
	case EXPR_LVALUE:   free_lv(expr->lv); break;
	case EXPR_UNARY:    free_expr(expr->unary.expr); break;
	case EXPR_BINARY:   free_expr(expr->binary.left), free_expr(expr->binary.right); break;
	case EXPR_FCALL:
		free_lv(expr->fcall.func);
		for (size_t i = 0; i < buf_len(expr->fcall.params); ++i)
			free_expr(expr->fcall.params[i]);
		buf_free(expr->fcall.params);
		break;
	case EXPR_STRING:   buf_free(expr->buf);
	}
	free(expr);
}
void free_bool(BoolValue* bv) {
	switch (bv->type) {
	case BOOL_EXPR:
	case BOOL_NOT:
		free_expr(bv->expr);
		break;
	case BOOL_BINARY:
		free_expr(bv->binary.left);
		free_expr(bv->binary.right);
		break;
	}
	free(bv);
}
void free_stmt(Statement* stmt) {
	switch (stmt->type) {
	case ST_NOP: break;
	case ST_RETURN:
	case ST_EXPR:       free_expr(stmt->expr); break;
	case ST_WHILE:
		free_bool(stmt->whileloop.cond);
		free_stmt(stmt->whileloop.body);
		break;
	case ST_IF:
		free_bool(stmt->ifstmt.cond);
		free_stmt(stmt->ifstmt.true_case);
		if (stmt->ifstmt.false_case)
			free_stmt(stmt->ifstmt.false_case);
		break;
	case ST_COMP:
		for (size_t i = 0; i < buf_len(stmt->stmts); ++i)
			free_stmt(stmt->stmts[i]);
		buf_free(stmt->stmts);
		break;
	case ST_VARDECL:
		for (size_t i = 0; i < buf_len(stmt->var_decls); ++i)
			free(stmt->var_decls[i].name);
		buf_free(stmt->var_decls);
		break;
	}
	free(stmt);
}
void free_func(Function* func) {
	free(func->name);
	for (size_t i = 0; i < buf_len(func->paramnames); ++i)
		free(func->paramnames[i]);
	buf_free(func->paramnames);
	switch (func->type) {
	case FT_SIMPLE: free_expr(func->value); break;
	case FT_COMPLEX:
		for (size_t i = 0; i < buf_len(func->body); ++i)
			free_stmt(func->body[i]);
		buf_free(func->body);
		break;
	default: break;
	}
}
void free_prog(Program* prog) {
	for (size_t i = 0; i < buf_len(prog->externs); ++i)
		free(prog->externs[i]);
	for (size_t i = 0; i < buf_len(prog->funcs); ++i)
		free_func(prog->funcs[i]);
	buf_free(prog->externs);
	buf_free(prog->funcs);
	free(prog);
}

static bool eval_failed;
static intmax_t eval_impl(const Expression* expr) {
	if (eval_failed) return 0;
	switch (expr->type) {
	case EXPR_NUMBER:   return expr->num;
	case EXPR_PAREN:    return eval_impl(expr->expr);
	case EXPR_UNARY:
		switch (expr->unary.op.type) {
		case TK_PLUS:   return eval_impl(expr->unary.expr);
		case TK_MINUS:  return -eval_impl(expr->unary.expr);
		default:        return eval_failed = true, 0;
		}
	case EXPR_BINARY:
		switch (expr->binary.op.type) {
		case TK_PLUS:   return eval_impl(expr->binary.left) + eval_impl(expr->binary.right);
		case TK_MINUS:  return eval_impl(expr->binary.left) - eval_impl(expr->binary.right);
		case TK_AND:    return eval_impl(expr->binary.left) & eval_impl(expr->binary.right);
		case TK_OR:     return eval_impl(expr->binary.left) | eval_impl(expr->binary.right);
		case TK_XOR:    return eval_impl(expr->binary.left) ^ eval_impl(expr->binary.right);
		default:        return eval_failed = true, 0;
		}
	default:            return eval_failed = true, 0;
	}
}
intmax_t eval_expr(const Expression* expr, bool* success) {
	eval_failed = false;
	const intmax_t v = eval_impl(expr);
	if (success) *success = !eval_failed;
	return eval_failed ? 0 : v;
}
bool eval_bool(const BoolValue* bv, bool* success) {
	intmax_t a, b;
	switch (bv->type) {
	case BOOL_EXPR: return eval_expr(bv->expr, success);
	case BOOL_NOT: return !eval_expr(bv->expr, success);
	case BOOL_BINARY:
		a = eval_expr(bv->binary.left, success);
		if (!*success)  return 0;
		b = eval_expr(bv->binary.right, success);
		if (!*success)  return 0;
		switch (bv->binary.op.type) {
		case TK_EQEQ:   return a == b;
		case TK_GR:     return a >  b;
		case TK_LE:     return a <  b;
		default:        return *success = false;
		}
	}
}