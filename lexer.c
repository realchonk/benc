#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"
#include "buf.h"

static FILE* file = NULL;
static int peekd = '\0';
static Token token_peekd = { 0 };
static size_t row, col;

static int read(void) {
	const int ch = fgetc(file);
	if (ch == '\n') ++row, col = 0;
	else ++col;
	return ch;
}
static int peek(void) {
	return peekd ? peekd : (peekd = read());
}
static int next(void) {
	if (peekd) {
		const int tmp = peekd;
		peekd = '\0';
		return tmp;
	}
	else return read();
}

void lexer_init(FILE* f) {
	lexer_free();
	file = f;
	peekd = '\0';
	row = col = 0;
	token_peekd.type = TK_DUMMY;
}
void lexer_free(void) {
	if (file) fclose(file);
}
static Token lexer_impl(void);
Token lexer_peek(void) {
	return token_peekd.type ? token_peekd : (token_peekd = lexer_impl());
}
Token lexer_next(void) {
	if (token_peekd.type) {
		const Token tk = token_peekd;
		token_peekd.type = TK_DUMMY;
		return tk;
	}
	else return lexer_impl();
}
bool lexer_eof(void) {
	while (isspace(peek())) next();
	return peek() == EOF;
}

static int escape_char(int ch) {
	switch (ch) {
	case 'a':   return '\a';
	case 'b':   return '\b';
	case 'f':   return '\f';
	case 'n':   return '\n';
	case 'r':   return '\r';
	case 't':   return '\t';
	case 'v':   return '\v';
	case '\\':  return '\\';
	case '\'':  return '\'';
	case '"':   return '\"';
	default:    return -1;
	}
}

const char* tk_names[NUM_TOKEN_TYPES] = {
	"dummy", "integer", "name", "string",
	"+", "-", "*", "&", "|", "^",
	"=", "(", ")", "{", "}",
	"[", "]", ",", ";", "==",
	">", "<", "!", "end of file",
	"if", "var", "else", "func", "while",
	"return", "extern", "dummy",
};
static bool strieq(const char* s1, const char* s2) {
	const size_t len = strlen(s1);
	if (len != strlen(s2)) return false;
	for (size_t i = 0; i < len; ++i) {
		if (toupper(s1[i]) != toupper(s2[i])) return false;
	}
	return true;
}
void print_token(const Token tk, FILE* f) {
	switch (tk.type) {
	case TK_INTEGER:    fprintf(f, "%ju", tk.num); break;
	case TK_NAME:       fputs(tk.text, f); break;
	case TK_STRING:     fprintf(f, "\"%s\"", tk.text); break;
	default:            fputs(tk_names[tk.type], f); break;
	}
}
void print_token_info(const Token tk, FILE* f) {
	fprintf(f, "Token{ .type=%s, .row=%zu, .col=%zu", tk_names[tk.type], tk.row, tk.col);
	switch (tk.type) {
	case TK_NAME:       fprintf(f, ", .name='%s'", tk.text); break;
	case TK_INTEGER:    fprintf(f, ", .value=%ju", tk.num); break;
	default:            break;
	}
	fputs(" }", f);
}

inline static bool isname(int ch) {
	return isalnum(ch) || (ch == '_');
}
static Token lexer_impl(void) {
	while (isspace(peek())) next();
	int ch = peek();
	if (isdigit(ch)) {
		const size_t r = row, c = col;
		uintmax_t num = 0;
		while (isdigit(peek())) num = num * 10 + (next() - '0');
		return (Token){ TK_INTEGER, r, c, num };
	}
	else if (isalpha(ch) || ch == '_') {
		const size_t r = row, c = col;
		char* buf = NULL;
		while (isname(peek())) buf_push(buf, next());
		buf_push(buf, 0);
		for (size_t i = 0; i < NUM_KEYWORDS; ++i) {
			if (strieq(tk_names[i + KW_IF], buf))
				return (Token){ i + KW_IF, r, c, i };
		}
		char* str = malloc(buf_len(buf));
		strncpy(str, buf, buf_len(buf));
		buf_free(buf);
		return (Token){ TK_NAME, r, c, .text = str };
	}
	else if (ch == '"') {
		const size_t r = row, c = col;
		char* buf = NULL;
		next();
		while ((ch = next()) != '"') {
			if (ch == EOF) {
				fprintf(stderr, "%zu:%zu: unterminated string\n", r, c);
				exit(1);
			}
			else if (ch == '\\') buf_push(buf, escape_char(next()));
			else buf_push(buf, ch);
		}
		buf_push(buf, 0);
		return (Token) { TK_STRING, r, c, .text = buf };
	}
	else if (ch == '\'') {
		next();
		ch = next();
		if (ch == '\\')
			ch = escape_char(next());
		if (next() != '\'') {
			fprintf(stderr, "%zu:%zu: expected '\n", row, col);
			exit(1);
		}
		return (Token){ TK_INTEGER, row, col, ch };
	}
	
	else {
		const size_t r = row, c = col;
		ch = next();
		switch (ch) {
		case '+':   return (Token){ TK_PLUS, r, c };
		case '-':   return (Token){ TK_MINUS, r, c };
		case '*':   return (Token){ TK_STAR, r, c };
		case '&':   return (Token){ TK_AND, r, c };
		case '|':   return (Token){ TK_OR, r, c };
		case '^':   return (Token){ TK_XOR, r, c };
		case '=':
			if (peek() == '=')
					return next(), (Token){ TK_EQEQ, r, c};
			else    return (Token){TK_EQUALS,r, c};
		case '(':   return (Token){ TK_LPAREN, r, c };
		case ')':   return (Token){ TK_RPAREN, r, c };
		case '{':   return (Token){ TK_CLPAREN, r, c };
		case '}':   return (Token){ TK_CRPAREN, r, c };
		case '[':   return (Token){ TK_LBRACK, r, c };
		case ']':   return (Token){ TK_RBRACK, r, c };
		case ',':   return (Token){ TK_COMMA, r, c };
		case ';':   return (Token){ TK_SEMICOLON, r, c };
		case '>':   return (Token){ TK_GR, r, c };
		case '<':   return (Token){ TK_LE, r, c };
		case '!':   return (Token){ TK_NOT, r, c };
		case 0:
		case EOF:   return (Token){ TK_EOF, r, c };
		default:
			fprintf(stderr, "%zu:%zu: unknown input '%c'\n", r, c, ch);
			exit(1);
		}
	}
}