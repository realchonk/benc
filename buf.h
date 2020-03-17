#ifndef BENC_BUF_H
#define BENC_BUF_H
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifdef __cplusplus
#warning This file may not work with C++
extern "C" {
#endif

struct BufHdr {
	size_t capacity;
	size_t size;
	char buf[0];
};
#define buf__hdr(b) ((struct BufHdr*)((char*)(b) - offsetof(struct BufHdr, buf)))
#define buf_size(buf) ((buf) ? buf__hdr(buf)->size : 0)
#define buf_len(buf) buf_size(buf)
#define buf_capacity(buf) ((buf) ? buf__hdr(buf)->capacity : 0)
#define buf__fit(buf, n) ((n) <= buf_capacity(buf) ? 0 : ((buf) = buf__grow((buf), (n), (sizeof(*(buf))))))
#define buf_push(buf, ...) (buf__fit((buf), 1 + buf_size(buf)), (buf)[buf__hdr(buf)->size++] = (__VA_ARGS__))
#define buf_pop(buf) ((buf) ? (buf)[--buf__hdr(buf)->size], 1 : 0)
#define buf_free(buf) ((buf) ? (free(buf__hdr(buf)), (buf) = NULL) : 0)
#define buf_last(buf) ((buf) + buf_size(buf) - 1)

inline static void* buf__grow(void* buf, size_t new_length, size_t elem_size) {
	assert(buf_capacity(buf) <= (SIZE_MAX - 1) / 2);
	const size_t new_cap = max(64, max(1 + 2 * buf_capacity(buf), new_length));
	assert(new_length <= new_cap);
	assert(new_cap <= (SIZE_MAX - offsetof(struct BufHdr, buf)) / elem_size);
	const size_t new_size = offsetof(struct BufHdr, buf) + new_cap * elem_size;
	
	struct BufHdr* new_hdr;
	if (buf)
		new_hdr = (struct BufHdr*)realloc(buf__hdr(buf), new_size);
	else {
		new_hdr = (struct BufHdr*)malloc(new_size);
		if (!new_hdr) return NULL;
		new_hdr->size = 0;
	}
	if (!new_hdr) return NULL;
	new_hdr->capacity = new_cap;
	return (void*)new_hdr->buf;
}

#ifdef __cplusplus
}
#endif

#endif //BENC_BUF_H
