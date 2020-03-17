#ifndef BENC_CMDOPTS_H
#define BENC_CMDOPTS_H
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cmdline_opts {
	const char** inputs;
	const char* target;
	bool optimize;
	bool intermediate;
} cmdline_opts;

cmdline_opts parse_cmdline(int argc, const char** argv);

#ifdef __cplusplus
}
#endif

#endif //BENC_CMDOPTS_H
