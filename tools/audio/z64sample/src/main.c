#include <cclib.h>
#include "z64sample.h"

typedef struct {
	z64sample sample;
	char*     in_name;
	char*     out_name;
	char*     target_bit_depth;
	char*     target_type;

	char* td_order;
	char* td_bits;
	char* td_refine;
	char* td_thresh;

	char* chan_num;

	char* external_book;
} MainContext;

static int arg_one_char(void* udata, char* cmd, int n, char** arg) {
	MainContext* ctx = udata;

	switch (*cmd) {
		case 'i':
		case 'o':
		case 'b':
		case 't':
		case 'O':
		case 'B':
		case 'R':
		case 'T':
		case 'c':
			if (n < 1) goto throw_arg_num;
			break;
	}

	switch (*cmd) {
		case 'i': ctx->in_name = arg[0];          return 1;
		case 'o': ctx->out_name = arg[0];         return 1;
		case 'b': ctx->target_bit_depth = arg[0]; return 1;
		case 't': ctx->target_type = arg[0];      return 1;
		case 'O': ctx->td_order = arg[0];         return 1;
		case 'B': ctx->td_bits = arg[0];          return 1;
		case 'R': ctx->td_refine = arg[0];        return 1;
		case 'T': ctx->td_thresh = arg[0];        return 1;
		case 'e': ctx->external_book = arg[0];    return 1;
		case 'c': ctx->chan_num = arg[0];         return 1;
	}

	throw(ERR_NONIMPLEMENTED, "non-implemented arg: --" COL_GRAY "" COL_RESET "%s", cmd);
	throw_arg_num:
	throw(ERR_ERROR, "missing sub-argument: --%c " COL_RED "<sub-arg>" COL_RESET, *cmd);
}

static int arg_any(void* udata, char* cmd, int n, char** arg) {
	MainContext* ctx = udata;

	if (!ctx->in_name && sys_stat(cmd)) {
		arg_one_char(udata, "i", 1, &cmd);
	} else if (!ctx->out_name && ctx->in_name) {
		arg_one_char(udata, "o", 1, &cmd);
	} else
		print_exit("unknown arg token: %s", cmd);

	return 0;
}

static int arg_type_list(void* udata, char* cmd, int n, char** arg) {
	z64_print_sample_type_tbl();
	exit(0);
}

static int arg_matching(void* udata, char* cmd, int n, char** arg) {
	MainContext* ctx = udata;

	ctx->sample.matching = true;

	return 0;
}

static ArgTable s_argtab[] = {
	//crustify
	argtab_entry_any(arg_any),
	argtab_entry_title("basic:"),
	argtab_entry_cmd("i",           arg_one_char,     "<str>",  "input file"),
	argtab_entry_cmd("o",           arg_one_char,     "<str>",  "output file"),
	argtab_entry_title("properties:"),
	argtab_entry_cmd("matching",    arg_matching,     "",       "bruteforce matching decode result"),
	argtab_entry_cmd("b",           arg_one_char,     "<int>",  "output bit depth"),
	argtab_entry_cmd("t",           arg_one_char,     "<str>",  "output type"),
	argtab_entry_cmd("c",           arg_one_char,     "<int>",  "output channel num"),
	argtab_entry_title("table-design:"),
	argtab_entry_cmd("O",           arg_one_char,     "<int>",  "order          (2)"),
	argtab_entry_cmd("B",           arg_one_char,     "<int>",  "bits           (2)"),
	argtab_entry_cmd("R",           arg_one_char,     "<int>",  "refine iter    (2)"),
	argtab_entry_cmd("T",           arg_one_char,     "<dbl>",  "threshold   (10.0)"),
	argtab_entry_title("other:"),
	argtab_entry_cmd("e",           arg_one_char,     "<str>",  "external codebook"),
	argtab_entry_cmd("type-list",   arg_type_list,    "",       "output type list"),
	//uncrustify
};

int main(int narg, char** arg) {
	if (narg == 1) argtab_help(s_argtab);

	MainContext* ctx = new(MainContext);

	if (argtab_parse(ctx, s_argtab, narg, arg))
		print_exit("error parsing arguments!");

	if (!ctx->in_name || !ctx->out_name)
		print_exit("can't convert without input and output!");

	if (z64sample_read_file(&ctx->sample, ctx->in_name))
		print_exit("error reading: %s", ctx->in_name);

	if (ctx->chan_num)
		if (z64sample_set_channel_num(&ctx->sample, strtoull(ctx->chan_num, 0, 10)))
			print_exit("error changing channel num!");

	if (ctx->target_bit_depth || ctx->target_type) {
		SampleBitDepth bit = ctx->sample.bit_depth;
		SampleType type = ctx->sample.type;

		if (ctx->target_bit_depth)
			if ((bit = z64_get_sample_bit_depth(ctx->target_bit_depth)) == SAMPLE_BIT_MAX)
				print_exit("unknown bit depth: %s", ctx->target_bit_depth);

		if (ctx->target_type)
			if ((type = z64_get_sample_type(ctx->target_type)) == SAMPLE_TYPE_MAX)
				print_exit("unknown type: %s", ctx->target_type);

		if (z64sample_set_type_and_bit(&ctx->sample, type, bit))
			print_exit("error changing type and/or bit depth!");
	}

	if (ctx->external_book) {
		File __f = {}, * f = &__f;

		if (f_open(f, ctx->external_book))
			print_exit("can't load external codebook: %s", ctx->external_book);

		z64sample_destroy_codebook(&ctx->sample);
		if (z64sample_read_codebook(&ctx->sample, f->data))
			print_exit("failed to read external codebook: %s", ctx->external_book);

		f_destroy(f);
	} else if (ctx->td_bits || ctx->td_order || ctx->td_refine || ctx->td_thresh) {
		z64TableDesign* design = ctx->sample.tabledesign = new(z64TableDesign);

		design->bits =         ctx->td_bits   ? strtoull(ctx->td_bits, 0, 10)   : 2;
		design->order =        ctx->td_order  ? strtoull(ctx->td_order, 0, 10)  : 2;
		design->refine_iters = ctx->td_refine ? strtoull(ctx->td_refine, 0, 10) : 2;
		design->thresh =       ctx->td_thresh ? strtod(ctx->td_thresh, 0)       : 10.0;
	}

	if (z64sample_write_file(&ctx->sample, ctx->out_name))
		print_exit("error writing: %s", ctx->out_name);

	z64sample_destroy(&ctx->sample);
	delete(ctx);
}
