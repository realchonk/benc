#ifndef BENC_LEXER_H
#define BENC_LEXER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

enum TokenType {
	TK_DUMMY,
	TK_INTEGER, TK_NAME, TK_STRING,
	TK_PLUS, TK_MINUS,
	TK_STAR, TK_AND, TK_OR,
	TK_XOR, TK_EQUALS,
	TK_LPAREN, TK_RPAREN,
	TK_CLPAREN, TK_CRPAREN,
	TK_LBRACK, TK_RBRACK,
	TK_COMMA, TK_SEMICOLON,
	TK_EQEQ, TK_GR, TK_LE,
	TK_NOT,
	TK_EOF,
	
	KW_IF,
	KW_VAR,
	KW_ELSE,
	KW_FUNC, // func a() {}
	KW_WHILE,
	KW_RETURN,
	KW_EXTERN,
	END_KEYWORDS,
	
	NUM_TOKEN_TYPES,
	NUM_KEYWORDS = END_KEYWORDS - KW_IF,
};
typedef struct Token {
	enum TokenType type;
	size_t row, col;
	union {
		uintmax_t num;
		char* text;
	};
} Token;

extern const char* tk_names[NUM_TOKEN_TYPES];

void lexer_init(FILE* file);
void lexer_free(void);
Token lexer_peek(void);
Token lexer_next(void);
bool lexer_eof(void);
void print_token(Token tk, FILE* file);
void print_token_info(Token tk, FILE* file);

#ifdef __cplusplus
}
#endif

#endif // BENC_LEXER_H
