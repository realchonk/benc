#ifndef BENC_PARSER_H
#define BENC_PARSER_H
#include "lexer.h"

#ifdef __cplusplus
extern "C" {
#endif

enum LValueType {
	LV_PAREN,
	LV_NAME,
	LV_DEREF,
	LV_AT,
	LV_ASSIGN,
};
struct Expression;
typedef struct LValue {
	enum LValueType type;
	size_t row, col;
	union {
		char* name;
		struct Expression* expr;
		struct LValue* lv;
		struct {
			struct LValue* left;
			struct Expression* right;
		} binary;
	};
} LValue;


enum ExpressionType {
	EXPR_PAREN,
	EXPR_UNARY,
	EXPR_BINARY,
	EXPR_NUMBER,
	EXPR_LVALUE,
	EXPR_ADDROF,
	EXPR_FCALL,
	EXPR_STRING,
};
typedef struct Expression {
	enum ExpressionType type;
	size_t row, col;
	union {
		uintmax_t num;
		struct LValue* lv;
		struct Expression* expr;
		char* buf;
		struct {
			Token op;
			struct Expression* expr;
		} unary;
		struct {
			Token op;
			struct Expression* left;
			struct Expression* right;
		} binary;
		struct {
			LValue* func;
			struct Expression** params;
		} fcall;
	};
} Expression;

enum BoolValueType {
	BOOL_EXPR,
	BOOL_NOT,
	BOOL_BINARY,
};
typedef struct BoolValue {
	enum BoolValueType type;
	size_t row, col;
	union {
		Expression* expr;
		struct {
			Token op;
			Expression* left;
			Expression* right;
		} binary;
	};
} BoolValue;

enum StatementType {
	ST_NOP,
	ST_COMP,
	ST_IF,
	ST_WHILE,
	ST_RETURN,
	ST_EXPR,
	ST_VARDECL,
};
struct VarDecl {
	char* name;
	bool has_value;
	intmax_t value;
};
typedef struct Statement {
	enum StatementType type;
	size_t row, col;
	union {
		Expression* expr;
		struct Statement** stmts;
		struct VarDecl* var_decls;
		struct {
			BoolValue* cond;
			struct Statement* true_case;
			struct Statement* false_case; // nullable
		} ifstmt;
		struct {
			BoolValue* cond;
			struct Statement* body;
		} whileloop;
	};
} Statement;

enum FunctionType {
	FT_SIMPLE,
	FT_COMPLEX,
};
typedef struct Function {
	enum FunctionType type;
	char* name;
	size_t row, col;
	char** paramnames;
	union {
		Expression* value;
		Statement** body;
	};
} Function;

typedef struct Program {
	struct Function** funcs;
	char** externs;
} Program;

void parser_init(void);

LValue* parse_lv(void);
Expression* parse_expr(void);
BoolValue* parse_bool(void);
Statement* parse_stmt(void);
Function* parse_func(void);
Program* parse_prog(void);

void print_lv(const LValue* lv, FILE* file);
void print_expr(const Expression* expr, FILE* file);
void print_bool(const BoolValue* bv, FILE* file);
void print_stmt(const Statement* stmt, FILE* file);
void print_func(const Function* func, FILE* file);
void print_prog(const Program* prog, FILE* file);

void free_lv(LValue* lv);
void free_expr(Expression* expr);
void free_bool(BoolValue* bv);
void free_stmt(Statement* stmt);
void free_func(Function* func);
void free_prog(Program* prog);

intmax_t eval_expr(const Expression* expr, bool* success);
bool eval_bool(const BoolValue* bv, bool* success);

#ifdef __cplusplus
}
#endif

#endif //BENC_PARSER_H
