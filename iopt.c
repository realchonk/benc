#include "iutil.h"
#include "igen.h"


/// OPTIMIZATION FUNCTIONS
/*
 * Removes NOPs
 * NOP
 * NOP
 * ---
 *
 */
static bool remove_nops(INode* nodes) {
	bool opt = false;
	for (INode* n = nodes; n; n = n->next) {
		if (n->type == IN_NOP) opt = true, inode_remove(n);
	}
	return opt;
}
/*
 * Merges compile-time evaluateable expressions.
 * LDC R0, 3
 * LDC R1, 2
 * ADD R0, R0, R1
 * --------------
 * LDC R0, 5
 *
 */
static bool const_eval(INode* nodes) {
	bool opt = false;
	for (INode* n = nodes; n; n = n->next) {
		if (is_type(IN_LDC, -1, n) && is_type(IN_LDC, -2, n)) {
			if (!is_binary(n->type)
			|| n->prev->prev->ldc.dest + 1 != n->prev->ldc.dest
			|| n->binary.left != n->prev->prev->ldc.dest
			|| n->binary.right != n->prev->ldc.dest)
				continue;
			const ireg_t dest = n->binary.dest;
			const uintmax_t r = perform_binary(n->type, n->prev->prev->ldc.num, n->prev->ldc.num);
			inode_remove(n->prev->prev);
			inode_remove(n->prev);
			n->type = IN_LDC;
			n->ldc.dest = dest;
			n->ldc.num = r;
			opt = true;
		}
	}
	return opt;
}
/*
 * Merges compile-time evaluateable expressions.
 * LDC R1, 3
 * ADD R0, R0, R1
 * LDC R1, 2
 * SUB R0, R0, R1
 * --------------
 * LDC R1, 1
 * ADD R0, R0, R1
 */
static bool const_eval2(INode* nodes) {
	bool opt = false;
	for (INode* n = nodes; n; n = n->next) {
		if (!(is_type(IN_LDC, 0, n)
		&& is_binary_type(1, n)
		&& is_type(IN_LDC, 2, n)
		&& is_binary_type(3, n)))
			continue;
		const uintmax_t r = perform_binary(inode_get(n, 3)->type, n->ldc.num, inode_get(n, 2)->ldc.num);
		n->ldc.num = r;
		inode_remove_at(n, 2);
		inode_remove_at(n, 2);
		opt = true;
	}
	return opt;
}
/*
 * Merges compile-time evaluateable unary expressions.
 * LDC R1, 3
 * NEG R1
 * ---------
 * LDC R1, -3
 */
static bool const_unary(INode* nodes) {
	bool opt = false;
	for (INode* n = nodes; n; n = n->next) {
		if (!(is_type(IN_LDC, 0, n)
		&& is_unary_type(1, n))) continue;
		n->ldc.num = perform_unary(n->next->type, n->ldc.num);
		inode_remove(n->next);
		opt = true;
	}
	return opt;
}
/*
 * Removes code that does nothing.
 * BEG
 * LDC R0, 2
 * LDA R1, a
 * ADD R0, R0, R1
 * END
 * --------------
 */
static bool remove_unused(INode* nodes) {
	INode* first = inode_search(nodes, IN_BEG_STMT);
	if (!first) return false;
	INode* last = inode_search(first, IN_END_STMT);
	if (!last) return false;
	for (INode* n = first; n && n != last; n = n->next) {
		if (has_effect(n->type)) return remove_unused(last);
	}
	inode_remove_range(first, last);
	return true;
}
/*
 * ...
 * WRITE R1, R0
 * MOVE R0, R1
 * READ R0, R0
 * ------------
 * ...
 * WRITE R1, R0
 */
static bool remove_readback(INode* nodes) {
	bool opt = false;
	for (INode* n = nodes; n; n = n->next) {
		if (is_type(IN_MOVE, 0, n)
		&& is_type(IN_READ, 1, n)
		&& is_type(IN_END_STMT, 2, n)) {
			inode_remove(n->next);
			inode_remove(n);
			opt = true;
		}
	}
	return opt;
}
/*
 * LDA R0, a
 * READ R0, R0
 * ...
 * LDA R0, a
 * READ R0, R0
 * ...
 * LDC R0, 1
 * LDA R1, a
 * WRITE R1, R0
 * LDC R0, 4
 * LDA R1, a
 * WRITE R1, R0
 * -----------
 * LDA R0, a
 * READ R0, R0
 * ...
 * LDC R0, 1
 * LDA R1, a
 * WRITE R1, R0
 * LDC R0, 4
 * WRITE R1, R0
 */
static bool register_caching(INode* nodes) {
	bool opt = false;
	rcache_invlall();
	for (INode* n = nodes; n; n = n->next) {
		if (is_type(IN_LDA, 0, n)) {
			if (is_type(IN_WRITE, 1, n)) {
				bool b = rcache_write(n->next->move.src, n->lda.name);
				bool c = rcache_addrof(n->next->move.dest, n->lda.name);
				
				if (is_type(IN_MOVE, 2, n) && is_type(IN_READ, 3, n)) {
					const INode* tmp = inode_get(n, 2);
					if (tmp->move.dest != (tmp->move.src - 1)) continue;
					tmp = inode_get(n, 3);
					if (tmp->move.dest != tmp->move.src) continue;
					inode_remove_at(n, 2);
					inode_remove_at(n, 2);
				}
				/*if (b) {
					n = inode_remove(n);
					n = inode_remove(n);
				}*/  // CODE IS BUGGY
				if (c) n = inode_remove(n);
			}
			else if (is_type(IN_READ, 1, n)) {
				const ireg_t dest = n->next->move.dest;
				ireg_t r = rcache_read(dest, n->lda.name);
				if (r != 0xffff) {
					n = inode_remove(n);
					n = inode_remove(n);
					if (r != dest) {
						INode *tmp = alloc(INode);
						tmp->type = IN_MOVE;
						tmp->move.dest = dest;
						tmp->move.src = r;
						inode_insert(n->prev, tmp);
					}
					opt = true;
				}
			}
		}
		else if (is_type(IN_LDC, 0, n)) {
			if (rcache_ldc(n->ldc.dest, n->ldc.num))
				inode_remove(n), opt = true;
		}
		else if (is_type(IN_MOVE, 0, n)) {
			rcache_move(n->move.dest, n->move.src);
		}
		else if (is_type(IN_CALL, 0, n)
				|| is_type(IN_LABEL, 0, n))
			rcache_invlall();
	}
	return opt;
}


IUnit* optimize_iunit(IUnit* unit) {
	while (remove_nops(unit->nodes)
		|| register_caching(unit->nodes)
        || const_eval(unit->nodes)
		|| const_eval2(unit->nodes)
		|| const_unary(unit->nodes)
		|| remove_unused(unit->nodes)
		|| remove_readback(unit->nodes)
	);
	return unit;
}