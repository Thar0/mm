#include "cclib.h"

typedef struct LibNode {
	struct DTA_BE {
		uint32_t magic;
		enum Attr {
			ATTR_ALLOCATED = 1 << 0
		} attr;
		size_t size;
	};
	struct LibNode* next;
	char data[];
} LibNode;

static size_t s_heap_size;
static LibNode* s_heap;

#define MAGIC   'HEAP'
#define UNMAGIC 'DEAD'
#define ALIGN   (0x8)

onexit_fun_t lib_heapdest() {
	delete(s_heap);
}

int lib_heapinit(size_t heap_size) {
	heap_size = alignval(heap_size, ALIGN);

	try {
		if (!(s_heap = malloc(heap_size)))
			throw(ERR_ALLOC, "Could not allocate lib heap, size: " FV_HEX_SIZE, heap_size);

		s_heap_size = heap_size;
		memzero(s_heap, heap_size);
		s_heap->magic = MAGIC;
		s_heap->size = heap_size - sizeof(struct LibNode);
	} except {
		print_exception();

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int lib_heapdump(void) {
	File f = {
		.data = (char*)s_heap,
		.size = s_heap_size
	};

	return f_save(&f, "heap_dump.bin");
}

void* lib_alloc(int64_t size) {
	size = alignval(size, ALIGN);

	fornode(node, s_heap) {
		if (node->attr & ATTR_ALLOCATED)
			continue;
		if (node->size < size + sizeof(struct LibNode))
			continue;

		size_t left = node->size - size - sizeof(struct LibNode);

		node->attr |= ATTR_ALLOCATED;
		node->size = size;

		if (left >= sizeof(struct LibNode) + ALIGN) {
			LibNode* split = (void*)&node->data[size];

			node_insert(node, split);

			split->attr = 0;
			split->magic = MAGIC;
			split->size = left;
		} else
			node->size = size + left;

		return memzero(&node->data, size);
	}

	return NULL;
}

void* lib_free(const void* ptr) {
	char* p = (char*)ptr;

	if (p < (char*)s_heap || p >= (char*)s_heap + s_heap_size)
		return NULL;

	LibNode* prev = NULL;
	LibNode* next = NULL;

	fornode(node, s_heap) {
		char* s = node->data;

		if (!(node->attr & ATTR_ALLOCATED) || p < s || p > s + node->size) {
			prev = node;
			continue;
		}

		if (prev && prev->attr & ATTR_ALLOCATED)
			prev = NULL;
		if (node->next && !(node->next->attr & ATTR_ALLOCATED))
			next = node->next;

		if (prev) {
			prev->size += node->size + sizeof(struct LibNode);
			node->magic = UNMAGIC;
			node_remove(prev, node);
			node = prev;
		}

		if (next) {
			node->size += next->size + sizeof(struct LibNode);
			next->magic = UNMAGIC;
			node_remove(node, next);
		}

		node->attr &= ~ATTR_ALLOCATED;

		return NULL;
	}

	return NULL;
}
