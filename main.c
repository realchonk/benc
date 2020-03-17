#include <stdnoreturn.h>
#include <stdarg.h>
#include <string.h>
#include "cmdopts.h"
#include "target.h"
#include "buf.h"

static void lexer_dump(void) {
	while (!lexer_eof()) {
		print_token(lexer_next(), stdout);
	}
	exit(0);
}

static char* change_filename_suffix(const char* n, const char* suf) {
	char* str = (char*)malloc(strlen(n) + 5);
	size_t i;
	for (i = 0; n[i] && n[i] != '.'; ++i)
		str[i] = n[i];
	return strcat(str, suf);
}
static bool is_suffix(const char* n, const char* suf) {
	size_t i;
	for (i = 0; n[i] && n[i] != '.'; ++i);
	return n[i] ? strcmp(n + i, suf) == 0 : false;
}
noreturn static void error(const char* name, const char* msg, ...) {
	va_list ap;
	fprintf(stderr, "%s: fatal error: ", name);
	va_start(ap, msg);
	vfprintf(stderr, msg, ap);
	va_end(ap);
	fputs(".\ncompilation terminated\n", stderr);
	exit(1);
}

static void compile(const char* name, const char* srcfile, const Target* target, bool optimize, bool intermediate) {
	FILE* src = fopen(srcfile, "r");
	if (!src) error(name, "couldn't open file %s", srcfile);
	char* outname = change_filename_suffix(srcfile, intermediate ? ".ic" : ".asm");
	if (!outname) error(name, "out of memory");
	FILE* out = fopen(outname, "w");
	if (!out) error(name, "couldn't open file %s", outname);
	lexer_init(src);
	
	Program* p = parse_prog();
	if (!p) error(name, "couldn't parse program");
	
	IProgram* i = igen_prog(p);
	if (!i) error(name, "couldn't generate intermediate code");
	
	if (optimize) i = optimize_iprog(i);
	if (!i) error(name, "couldn't optimize intermediate code");
	
	if (intermediate) print_iprog(i, out);
	else if (target->gen_asm(i, out) != 0)
		error(name, "couldn't generate assembly output");
	printf("compiled %s -> %s.\n", srcfile, outname);
	
	free_iprog(i);
	free_prog(p);
	free(outname);
	fclose(out);
	fclose(src);
	lexer_free();
}

int main(int argc, const char** argv) {
#if !DEBUG
	cmdline_opts opts = parse_cmdline(argc, argv);
	const Target* target = get_target_by_name(opts.target);
	if (!target) error(argv[0], "target %s not found", opts.target);
	for (size_t i = 0; i < buf_len(opts.inputs); ++i) {
		compile(argv[0], opts.inputs[i], target, opts.optimize, opts.intermediate);
	}
	puts("compiled all files successfully.");
	return 0;
#else
	FILE* src = fopen("../test.b", "r");
	FILE* out = fopen("../test.asm", "w");
	FILE* ic = fopen("../test.ic", "w");
	lexer_init(src);
	lexer_dump();
	Program* prog = parse_prog();
	IProgram* ip = igen_prog(prog);
	print_iprog(ip, ic);
	fputc('\n', ic);
	optimize_iprog(ip);
	print_iprog(ip, ic);
	get_target(TARGET_i386)->gen_asm(ip, out);
#endif
}