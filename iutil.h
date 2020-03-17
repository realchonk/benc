#ifndef BENC_IUTIL_H
#define BENC_IUTIL_H
#include <string.h>
#include "igen.h"

#ifdef __cplusplus
extern "C" {
#endif

static bool is_binary(enum INodeType t) {
	switch (t) {
	case IN_ADD:
	case IN_SUB:
	case IN_AND:
	case IN_OR:
	case IN_XOR: return true;
	default:     return false;
	}
}
static bool is_unary(enum INodeType t) {
	switch (t) {
	case IN_NEG: return true;
	default:     return false;
	}
}
static bool has_effect(enum INodeType t) {
	switch (t) {
	case IN_PUSH:
	case IN_CALL:
	case IN_WRITE:
	case IN_RETURN:
	case IN_CMP:
	case IN_LABEL:
	case IN_JMP:
	case IN_JE:
	case IN_JNE:
	case IN_JG:
	case IN_JL:
		return true;
	default: return false;
	}
}
static intmax_t perform_binary(enum INodeType t, intmax_t a, intmax_t b) {
	switch (t) {
	case IN_ADD: return a + b;
	case IN_SUB: return a - b;
	case IN_AND: return a & b;
	case IN_OR:  return a | b;
	case IN_XOR: return a ^ b;
	default:     return 0;
	}
}
static intmax_t perform_unary(enum INodeType t, intmax_t a) {
	switch (t) {
	case IN_NEG: return -a;
	default:     return 0;
	}
}
static bool is_type(enum INodeType type, int off, INode* n) {
	if (!n) return false;
	if (!off) return n->type == type;
	INode* tmp = n;
	if (off > 0)
		for (int i = 0; i < off; ++i) {
			tmp = tmp->next;
			if (!tmp) return false;
		}
	else
		for (int i = 0; i < -off; ++i) {
			tmp = tmp->prev;
			if (!tmp) return false;
		}
	return tmp->type == type;
}
static bool is_binary_type(int off, INode* n) {
	if (!n) return false;
	if (!off) return is_binary(n->type);
	INode* tmp = n;
	if (off > 0)
		for (int i = 0; i < off; ++i) {
			tmp = tmp->next;
			if (!tmp) return false;
		}
	else
		for (int i = 0; i < -off; ++i) {
			tmp = tmp->prev;
			if (!tmp) return false;
		}
	return is_binary(tmp->type);
}
static bool is_unary_type(int off, INode* n) {
	if (!n) return false;
	if (!off) return is_unary(n->type);
	INode* tmp = n;
	if (off > 0)
		for (int i = 0; i < off; ++i) {
			tmp = tmp->next;
			if (!tmp) return false;
		}
	else
		for (int i = 0; i < -off; ++i) {
			tmp = tmp->prev;
			if (!tmp) return false;
		}
	return is_unary(tmp->type);
}


#define RCE_NAME        0
#define RCE_IMM         1
#define RCE_ADDROF_NAME 2
typedef struct reg_cache_entry {
	bool valid;
	int type;
	union {
		const char *name;
		intmax_t value;
	};
} reg_cache_entry;

#define RCACHE_NUM 64
static struct reg_cache_entry rcache[RCACHE_NUM] = { 0 };
static void rcache_invl(ireg_t e) {
	if (e < RCACHE_NUM) rcache[e].valid = false;
}
static void rcache_invlall(void) {
	for (uint8_t i = 0; i < RCACHE_NUM; ++i)
		rcache[i].valid = false;
}
static bool rcache_addrof(ireg_t e, const char* name) {
	if (e < RCACHE_NUM) {
		if (rcache[e].valid && rcache[e].type == RCE_ADDROF_NAME && strcmp(name, rcache[e].name) == 0) return true;
		rcache[e].valid = true;
		rcache[e].type = RCE_ADDROF_NAME;
		rcache[e].name = name;
	}
	return false;
}
static bool rcache_write(ireg_t e, const char* name) {
	if (e < RCACHE_NUM) {
		if (rcache[e].valid && rcache[e].type == RCE_NAME && strcmp(name, rcache[e].name) == 0) return true;
		rcache[e].valid = true;
		rcache[e].type = RCE_NAME;
		rcache[e].name = name;
	}
	return false;
}
static bool rcache_move(ireg_t d, ireg_t s) {
	if (d < RCACHE_NUM && s < RCACHE_NUM && rcache[s].valid) {
		rcache[d].valid = true;
		rcache[d].type = rcache[s].type;
		switch (rcache[s].type) {
		case RCE_NAME:  rcache[d].name = rcache[s].name; break;
		case RCE_IMM:   rcache[d].value = rcache[s].value; break;
		}
		return true;
	}
	return false;
}
static ireg_t rcache_read(ireg_t x, const char* name) {
	if (x >= RCACHE_NUM) return 0xffff;
	if (rcache[x].valid && (rcache[x].type = RCE_NAME) && strcmp(rcache[x].name, name) == 0) return x;
	rcache[x].valid = true;
	rcache[x].type = RCE_NAME;
	rcache[x].name = name;
	for (size_t i = 0; i < RCACHE_NUM; ++i) {
		if (rcache[i].valid && i != x && rcache[i].type == RCE_NAME && strcmp(name, rcache[i].name) == 0)
			return i;
	}
	return 0xffff;
}
static bool rcache_ldc(ireg_t x, intmax_t val) {
	if (x >= RCACHE_NUM) return false;
	if (rcache[x].valid && rcache[x].type == RCE_IMM && rcache[x].value == val) return true;
	rcache[x].valid = true;
	rcache[x].type = RCE_IMM;
	rcache[x].value = val;
	return false;
}

#ifdef __cplusplus
}
#endif

#endif //BENC_IUTIL_H
