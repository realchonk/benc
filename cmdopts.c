#include <stdnoreturn.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "cmdopts.h"
#include "target.h"
#include "benc.h"
#include "buf.h"

noreturn static void print_help(const char* name) {
	printf("Usage: %s [options] file...\n", name);
	puts("Options:");
	puts("  --help | -h\t\t\tDisplay this information.");
	puts("  --version | -v\t\tDisplay compiler version information.");
	//puts("  -o <file>\t\t\t\tPlace the output into <file>.");
	puts("  -i\t\t\t\tOutput intermediate code.");
	puts("  -O\t\t\t\tEnable optimizations.");
	puts("  -m <target>\t\t\tSelect the output <target> (default i386).");
	puts("");
	printf("Existing targets: ");
	print_targets();
	putchar('\n');
	exit(0);
}
noreturn static void print_version(const char* name) {
	printf("%s %s\n", name, BENC_VERSION);
	puts("Build at " __TIME__ " on the " __DATE__);
	puts("Copyright (C) 2020 Benjamin Stuerz");
	puts("You are allowed to modify and/or distribute this software,");
	puts("if and ONLY if you give credits to the author.");
	puts("This is an experimental project; so there is NO warranty;");
	puts("not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.");
	exit(0);
}
noreturn static void print_usage(const char* name) {
	fprintf(stderr, "%s: fatal error: no input files\ncompilation terminated.\n", name);
	exit(1);
}

#define streq(s) (strcmp(argv[i], s) == 0)
cmdline_opts parse_cmdline(int argc, const char** argv) {
	cmdline_opts opts = { 0 };
	opts.target = "i386";
	for (int i = 1; i < argc; ++i) {
		if (streq("-h") || streq("--help"))
			print_help(argv[0]);
		else if (streq("-v") || streq("--version"))
			print_version(argv[0]);
		else if (streq("-m"))
			opts.target = argv[++i];
		else if (streq("-i"))
			opts.intermediate = true;
		else if (streq("-O"))
			opts.optimize = true;
		else if (argv[i][0] == '-')
			print_usage(argv[0]);
		else buf_push(opts.inputs, argv[i]);
	}
	if (!opts.inputs) print_usage(argv[0]);
	else return opts;
}