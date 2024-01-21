#include "cclib.h"
#include <stdio.h>
#include <stdarg.h>

///////////////////////////////////////////////////////////////////////////////

static int f_expand(File* this, size_t new_cap) {
	if (this->cap < new_cap) {
		void* old_data = this->data;

		if (!(this->data = crealloc(this->data, this->cap, new_cap * 2))) {
			this->data = old_data;
			return EXIT_FAILURE;
		}

		this->cap = new_cap * 2;
	}

	return EXIT_SUCCESS;
}

static void f_throw_read(File* this, const char* fname, size_t read_size) {
	throw(ERR_FREAD, "%s\n"
		"read request of " FV_HEX_SIZE " resulted in " FV_HEX_SIZE " read!",
		fname, this->size, read_size);
}

#undef f_open
int f_open(File* this, const char* fname, struct optarg_f_load arg) {
	FILE* f = NULL;

	try {
		if (!this)
			throw(ERR_NULL, "File* this");
		if (!(f = fopen(fname, "rb")))
			throw(ERR_FOPEN, "could not open file: %s", fname);
		fseek(f, 0, SEEK_END);
		this->size = ftell(f);
		rewind(f);

		size_t read_size;

		if (f_expand(this, this->size + 1))
			throw(ERR_ALLOC, "allocate to: " FV_HEX_SIZE, this->size + 1);

		if ((read_size = fread(this->data, 1, this->size, f)) > this->size)
			f_throw_read(this, fname, read_size);

		// line-ending sanitizer
		if (!arg.bin) {
			if (memchr(this->data, '\0', this->size));
			else if (memchr(this->data, '\r', this->size)) {
				int d = strcrep(this->data, "\r", "");
				strnrep(this->data, this->size, "\r", "");
				this->size += d;
			}
		}

		if (fclose(f))
			throw(ERR_FCLOSE, fname);

	} except {
		f_destroy(this);
		print_exception();

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

#undef f_save
int f_save(File* this, const char* fname, struct optarg_f_save arg) {
	FILE* f = NULL;

	try {
		if (!this)
			throw(ERR_NULL, "File* this");

		if (arg.mkdir) {
			if (sys_mkdir(x_filepath(fname)))
				throw(ERR_MKDIR, x_filepath(fname));
		} else if (strchr(fname, '/') && !sys_isdir(x_filepath(fname)))
			throw(ERR_STAT, fname);

		if (arg.no_overwrite)
			if (sys_stat(fname))
				throw(ERR_ERROR, "no_overwrite save: %s", fname);

		if (!(f = fopen(fname, "wb")))
			throw(ERR_FOPEN, fname);

		if (fwrite(this->data, this->size, 1, f) != 1 && this->size)
			throw(ERR_FWRITE, "%s: " FV_HEX_SIZE, fname, this->size);

		if (fclose(f))
			throw(ERR_FCLOSE, fname);

	} except {
		if (f) fclose(f);
		print_exception();

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static void f_dest_link(File* this);
void f_destroy(File* this) {
	if (this) {
		if (this->link)
			f_dest_link(this);
		delete(this->data);
		memzero(this, sizeof(File));
	}
}

///////////////////////////////////////////////////////////////////////////////

void* f_seek(File* this, size_t seek) {
	if (this->data)
		return &this->data[ (this->seek = u_clamp_max(seek, this->size)) ];
	return NULL;
}

void* f_wind(File* this, int32_t offset) {
	if (this->data)
		return f_seek(this, this->seek + offset);
	return NULL;
}

int f_write(File* this, const void* data, size_t size) {
	if (!size) return EXIT_SUCCESS;
	if (!data) return EXIT_FAILURE;
	if (f_expand(this, this->seek + size)) return EXIT_FAILURE;

	memcpy(&this->data[this->seek], data, size);

	this->seek += size;
	if (this->seek > this->size)
		this->size = this->seek;

	return EXIT_SUCCESS;
}

int f_writeBE(File* this, const void* data, size_t size) {
	size_t seek = this->seek;

	if (!f_write(this, data, size)) {
		membswap(&this->data[seek], size);
		return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}

int f_write_pad(File* this, size_t size) {
	if (!size) return EXIT_SUCCESS;
	if (f_expand(this, this->seek + size)) return EXIT_FAILURE;

	memset(&this->data[this->seek], '\0', size);

	this->seek += size;
	if (this->seek > this->size)
		this->size = this->seek;

	return EXIT_SUCCESS;
}

int f_align(File* this, int align) {
	size_t asz = alignval(this->seek, align);

	if (asz - this->seek)
		return f_write_pad(this, asz - this->seek);
	return EXIT_SUCCESS;
}

int f_print(File* this, const char* fmt, ...) {
	char buffer[1024 * 8];
	va_list va;

	va_start(va, fmt);
	int sz = cc_vsnprintf(buffer, sizeof(buffer), fmt, va);
	va_end(va);

	if (memmem(buffer, sz, "\r", 1)) {
		printerr("OH DAMN!");
		exit(EXIT_FAILURE);
	}

	return f_write(this, buffer, sz);
}

///////////////////////////////////////////////////////////////////////////////

typedef struct Symbol {
	struct Symbol* next;
	const char*    name_sym;
	const char*    name_type;
	File* parent;
	File* this;
	struct SymFlag flags;
	size_t offset;
	bool   processed;
} Symbol;

typedef struct Label {
	struct Label* next;
	const char*   name;
	size_t offset;
} Label;

typedef struct LinkFile {
	const char*   name_sym;
	const char*   name_type;
	Symbol*       sym_head;
	Symbol*       ref_head;
	Label*        label_head;
	struct File** child;
	int num_child;
	int align;
} LinkFile;

#undef f_newsym
File* f_newsym(File* this, const char* name, struct optarg_f_newsym arg) {
	LinkFile* link = this->link;
	void* prevtbl = link ? link->child : NULL;
	File* f = NULL;
	LinkFile* flink;

	try {
		if (!link && !(link = this->link = new(struct LinkFile)))
			throw(ERR_ALLOC, name);

		size_t chtblsz = sizeof(void**[1 + link->num_child]);

		if (!(link->child = realloc(link->child, chtblsz)))
			throw(ERR_REALLOC, "%s\n%p -> " FV_HEX_SIZE, name, prevtbl, chtblsz);

		if (!(f = new(struct File)))
			throw(ERR_ALLOC, "%s\nnew(struct File)", name);
		if (!(flink = f->link = new(struct LinkFile)))
			throw(ERR_ALLOC, "%s\nnew(struct LinkFile)", name);

		if (!(flink->name_sym = strdup(name)))
			throw(ERR_STRDUP, name);

		flink->name_type = arg.type_name ? strdup(arg.type_name) : NULL;
		flink->align = arg.align;

		if (arg.type_name && !flink->name_type)
			throw(ERR_STRDUP, arg.type_name);

	} except {
		if (link && !link->child)
			link->child = prevtbl;
		if (f)
			f_destroy(f);
		print_exception();

		return NULL;
	}

	return (link->child[link->num_child++] = f);
}

#undef f_write_ref
int f_write_ref(File* this, const void* name, struct SymFlag flags) {
	Symbol* sym;

	try {
		if (!name)
			throw(ERR_NULL, "name");

		const char* chrnm = name;

		if (!*chrnm) {
			const File* f = name;

			if (!f->link)
				throw(ERR_NULL, "f->link");

			name = f->link->name_sym;
		}

		if (!(sym = new(Symbol)))
			throw(ERR_ALLOC, name);
		if (!(sym->name_sym = strdup(name)))
			throw(ERR_STRDUP, name);

		sym->flags = flags;
		sym->offset = this->seek;
		sym->parent = this;

		node_add(this->link->ref_head, sym);

		int div = !!flags.split + 1;

		switch (flags.size) {
			case REF_16:
				return_e f_write_pad(this, 2 / div);
			case REF_32:
				return_e f_write_pad(this, 4 / div);
		}

	} except {
		print_exception();
	}

	return EXIT_FAILURE;
}

int f_set_label(File* this, const char* name) {
	try {
		Label* label;
		LinkFile* link = this->link;

		if (!link)
			throw(ERR_NULL, "this->link");

		if (!(label = new(struct Label)))
			throw(ERR_ALLOC, "struct Label");

		if (!(label->name = strdup(name)))
			throw(ERR_STRDUP, name);

		label->offset = this->seek;
		node_add(link->label_head, label);
	} except {
		print_exception();

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int f_link(File* main) {
	try {
		if (!main->link)
			throw(ERR_NULL);

		main->link->name_sym = strdup("__main__");
		Symbol* sym = new(struct Symbol);

		sym->name_sym = main->link->name_sym;
		sym->name_type = "__main__";
		sym->this = main;
		node_add(main->link->sym_head, sym);

		nested(void, AppendRefsAndGatherSyms, (File * file)) {
			File** pchild = file->link->child;
			File** end = pchild + file->link->num_child;

			for (; pchild < end; pchild++) {
				File* child = *pchild;

				if (child->size) {
					Symbol* sym = new(struct Symbol);

					sym->name_sym = child->link->name_sym;
					sym->name_type = child->link->name_type;
					sym->this = child;
					if (child->link->align) f_align(main, child->link->align);
					sym->offset = main->seek;
					node_add(main->link->sym_head, sym);

					fornode(label, child->link->label_head) {
						sym = new(struct Symbol);
						sym->name_sym = label->name;
						sym->name_type = "label";
						sym->this = child;
						sym->offset = main->seek + label->offset;
						node_add(main->link->sym_head, sym);
					}

					f_write(main, child->data, child->size);
				}

				AppendRefsAndGatherSyms(child);
			}
		};

		AppendRefsAndGatherSyms(main);

		nested(Symbol*, FindRef_impl, (LinkFile * link, const char* name)) {
			if (link->ref_head) {
				fornode(sym, link->ref_head) {
					if (!sym->processed && streq(name, sym->name_sym))
						return sym;
				}
			}

			for (int i = 0; i < link->num_child; i++) {
				File* child = link->child[i];
				Symbol* sym = FindRef_impl(child->link, name);

				if (sym)
					return sym;
			}

			return NULL;
		};

		nested(Symbol*, FindRef, (const char* name)) {
			return FindRef_impl(main->link, name);
		};

		nested(Symbol*, FindSym, (const char* name)) {
			fornode(sym, main->link->sym_head) {
				if (streq(sym->name_sym, name))
					return sym;
			}

			return NULL;
		};

		fornode(sym, main->link->sym_head) {
			Symbol* ref = NULL;

			while ((ref = FindRef(sym->name_sym))) {
				Symbol* parent_sym = FindSym(ref->parent->link->name_sym);

				if (!parent_sym)
					throw(ERR_ERROR, "undefined reference: %s", ref->parent->link->name_sym);

				size_t val = sym->offset;
				size_t write_pos = parent_sym->offset + ref->offset;
				struct SymFlag flags = ref->flags;
				char w[16];
				int half = !!flags.split;
				int size = (flags.size == REF_16 ? 2 : 4) / (half + 1);
				int off = (flags.split == REF_HI) ? size / 2 : 0;

				for (int i = 0; i < size; i++)
					w[i] = rmask(val, 0xFF << (i * 8 + off * 8));

				if (flags.endian == REF_BE)
					membswap(w, size);

				memcpy(main->data + write_pos, w, size);
				ref->processed = true;
			}
		}

	} except {
		print_exception();

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static void f_dest_link(File* this) {

	for (int i = 0; i < this->link->num_child; i++) {
		f_destroy(this->link->child[i]);
		delete(this->link->child[i]);
	}

	while (this->link->ref_head) {
		delete(this->link->ref_head->name_sym);
		node_delete(this->link->ref_head, this->link->ref_head);
	}

	while (this->link->sym_head)
		node_delete(this->link->sym_head, this->link->sym_head);

	delete(this->link->child, this->link->name_sym, this->link->name_type, this->link);
}

size_t f_get_sym_offset(File* this, const char* name) {
	fornode(sym, this->link->sym_head) {
		if (streq(sym->name_sym, name))
			return sym->offset;
	}

	return -1;
}

const char* f_get_linker_syms(File* this) {
	char* msg = "";

#if __debug__
	uint32_t offset = 0;
#endif

	fornode(sym, this->link->sym_head) {
		size_t val = sym->offset;

#if __debug__
		const char* color = "";
		if (offset > val)
			color = COL_BLUE;
#endif

		msg = x_fmt("%s%-32s = "
#if __debug__
				"%s"
#endif
				FV_HEX_SIZE COL_RESET ";\n", msg, sym->name_sym,
#if __debug__
				color,
#endif
				val);
#if __debug__
		offset = val;
#endif
	}

	return strdup(msg);
}

const char* f_get_header_syms(File* this, int callback(void*, const char*, size_t), void* udata) {
	char* msg = "";

	fornode(sym, this->link->sym_head) {
		if (sym->name_type) {
			const char* count = "";

			if (callback) {
				int c = callback(udata, sym->name_type, sym->this->size);

				if (c > 0)
					count = x_fmt("%d", callback(udata, sym->name_type, sym->this->size));
			}

			msg = x_fmt("%s" COL_RED "extern " COL_BLUE "%-20s" COL_RESET " %s[%s];\n", msg, sym->name_type, sym->name_sym, count);
		} else
			msg = x_fmt("%s" COL_RED "extern " COL_BLUE "%-20s" COL_RESET " %s[];\n", msg, "unk_t", sym->name_sym);
	}

	return strdup(msg);
}

int memdump(const void* mem, size_t s, const char* file) {
	FILE* f = NULL;

	try {
		if (!(f = fopen(file, "wb")))
			throw(ERR_FOPEN, file);
		if (fwrite(mem, 1, s, f) != s)
			throw(ERR_FWRITE, file);
		if (fclose(f))
			throw(ERR_FCLOSE, file);
	} except {
		if (f) fclose(f);
		print_exception();

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
