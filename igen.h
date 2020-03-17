#ifndef BENC_IGEN_H
#define BENC_IGEN_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "parser.h"

#ifdef __cplusplus
extern "C" {
#endif

#define alloc(t) ((t*)calloc(1, sizeof(t)))
typedef uint16_t ireg_t;

enum INodeType {
	IN_NOP,
	IN_MOVE,
	IN_LDC,
	IN_ADD,
	IN_SUB,
	IN_AND,
	IN_OR,
	IN_XOR,
	IN_NEG,
	IN_LDA,
	IN_CALL,
	IN_READ,
	IN_WRITE,
	IN_PUSH,
	IN_ADJOFF,
	IN_RETURN,
	IN_CMP,
	IN_LABEL,
	IN_JMP,
	IN_JE,
	IN_JNE,
	IN_JG,
	IN_JL,
	IN_LDS,
	
	NUM_INODES,
	IN_BEG_STMT,
	IN_END_STMT,
};
typedef struct INode {
	enum INodeType type;
	struct INode* next;
	struct INode* prev;
	
	union {
		ireg_t reg;
		unsigned label;
		struct {
			ireg_t dest;
			ireg_t src;
		} move;
		struct {
			ireg_t dest;
			uintmax_t num;
		} ldc;
		struct {
			ireg_t dest, left, right;
		} binary;
		struct {
			ireg_t dest;
			char* name;
		} lda;
		struct {
			ireg_t dest;
			uint8_t pcount;
		} fcall;
	};
} INode;
typedef struct IUnit {
	char* name;
	char** paramnames;
	INode* nodes;
	struct VarDecl* decls;
} IUnit;
typedef struct IProgram {
	IUnit** units;
	char** externs;
} IProgram;
extern const char* inode_names[NUM_INODES];

INode* igen_expr(const Expression* expr);
IUnit* igen_func(const Function* func);
IProgram* igen_prog(const Program* prog);
void print_inode(const INode* node, FILE* f);
void print_iunit(const IUnit* unit, FILE* f);
void print_iprog(const IProgram* prog, FILE* f);

IUnit* optimize_iunit(IUnit* unit);
IProgram* optimize_iprog(IProgram* prog);


void free_inode(INode* node);            // Frees a single inode
void free_iunit(IUnit* unit);
void free_iprog(IProgram* prog);
static void free_inodes(INode* node) {   // Frees all inodes
	if (!node) return;
	while (node) {
		INode* const next = node->next;
		free_inode(node);
		node = next;
	}
}
static void print_inodes(const INode* nodes, FILE* f) {
	if (!nodes) return;
	for (; nodes; nodes = nodes->next)
		print_inode(nodes, f);
}

static INode* inode_last(INode* nodes) {
	if (!nodes) return NULL;
	INode* last = NULL;
	for (INode* n = nodes; n; last = n, n = n->next);
	return last;
}
static INode* inode_first(INode* nodes) {
	if (!nodes) return NULL;
	INode* prev = NULL;
	for (INode* n = nodes; n; prev = n, n = n->prev);
	return prev;
}
static INode* inode_get(INode* nodes, size_t index) {
	if (!nodes) return NULL;
	INode* n = nodes;
	for (size_t i = 0; i < index; ++i, n = n->next) {
		if (!n) return NULL;
	}
	return n;
}

static INode* inode_insert(INode* node, INode* x) {
	if (!node || !x) return NULL;
	x->next = node->next;
	if (x->next) x->next->prev = x;
	x->prev = node;
	node->next = x;
	return x;
}
static INode* inode_insert_at(INode* nodes, INode* x, size_t index) {
	if (!nodes || !x) return NULL;
	INode* n = inode_get(nodes, index);
	if (!n) return NULL;
	x->next = n->next;
	if (x->next) x->next->prev = x;
	x->prev = n;
	n->next = x;
	return nodes;
}
static INode* inode_append(INode* nodes, INode* x) {
	if (!x) return NULL;
	INode* last;
	if (!nodes) last = alloc(INode);
	else last = inode_last(nodes);
	last->next = x;
	x->prev = last;
	return inode_last(x);
}
static INode* inode_remove_first(INode* nodes) {
	if (!nodes) return NULL;
	INode* const first = inode_first(nodes);
	INode* const next = first->next;
	next->prev = NULL;
	free_inode(first);
	return next;
}
static INode* inode_remove_last(INode* nodes) {
	if (!nodes) return NULL;
	INode* const last = inode_last(nodes);
	INode* const prev = last->prev;
	prev->next = NULL;
	free_inode(last);
	return prev;
}
static INode* inode_remove(INode* node) {
	if (node->prev) node->prev->next = node->next;
	if (node->next) node->next->prev = node->prev;
	INode* const tmp = node->next;
	//free_inode(node);
	return tmp;
}
static INode* inode_remove_at(INode* nodes, size_t index) {
	inode_remove(inode_get(nodes, index));
	return nodes;
}
static INode* inode_remove_range(INode* begin, INode* end) {
	INode* prev = begin->prev;
	INode* next = end->next;
	if (prev) prev->next = next;
	if (next) next->prev = prev;
	for (INode* n = begin; n != next; n = n->next)
		free_inode(n);
	return prev ? prev : next;
}
static INode* inode_search(INode* nodes, enum INodeType type) {
	for (INode* n = nodes; n; n = n->next) {
		if (n->type == type) return n;
	}
	return NULL;
}

#ifdef __cplusplus
}
#endif

#endif //BENC_IGEN_H
