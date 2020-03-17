#ifndef BENC_TARGET_H
#define BENC_TARGET_H
#include <stdint.h>
#include "igen.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Targets {
	TARGET_i386,
	
	NUM_TARGETS,
};
typedef struct Target {
	const char* name;
	int(*gen_asm)(IProgram*, FILE*);
} Target;

const Target* get_target(enum Targets t);
const Target* get_target_by_name(const char* name);
void print_targets();

#ifdef __cplusplus
}
#endif

#endif //BENC_TARGET_H
