#include <string.h>
#include "target.h"

extern int i386_gen_asm();
static Target targets[NUM_TARGETS] = {
	{ "i386", i386_gen_asm },
};

const Target* get_target(enum Targets t) {
	return t < NUM_TARGETS ? &targets[t] : NULL;
}
const Target* get_target_by_name(const char* name) {
	for (size_t i = 0; i < NUM_TARGETS; ++i) {
		if (strcmp(name, targets[i].name) == 0) return &targets[i];
	}
	return NULL;
}
void print_targets() {
	printf("%s", targets[0].name);
	for (size_t i = 1; i < NUM_TARGETS; ++i)
		printf(" %s", targets[i].name);
}