#include <stdio.h>
#include <math.h>
#include "tex-parser/head.h"
#include "config.h"
#include "prepare-math-qry.h"

/*
 * functions to sort subpaths by bound variable size
 */
struct cnt_same_symbol_args {
	uint32_t          cnt;
	symbol_id_t       symbol_id;
	enum subpath_type path_type;
	bool              pseudo;
};

static LIST_CMP_CALLBK(compare_qry_path)
{
	struct subpath *sp0 = MEMBER_2_STRUCT(pa_node0, struct subpath, ln);
	struct subpath *sp1 = MEMBER_2_STRUCT(pa_node1, struct subpath, ln);

	/* larger size bound variables are ranked higher, if sizes are equal,
	 * rank by path type (wildcard or concrete) then symbol (alphabet).*/
	if (sp0->pseudo != sp1->pseudo ||
	         sp0->type != sp1->type) {

		/* Order this case: normal (00), normal pseudo (01), wildcards (11) */
		if (sp0->type != sp1->type)
			return (sp0->type == SUBPATH_TYPE_NORMAL) ? 1 : 0;
		else
			return (sp0->pseudo) ? 0 : 1;

	}

	if (sp0->path_id != sp1->path_id)
		return sp0->path_id > sp1->path_id;

	return sp0->lf_symbol_id < sp1->lf_symbol_id;
}

static LIST_IT_CALLBK(cnt_same_symbol)
{
	LIST_OBJ(struct subpath, sp, ln);
	P_CAST(cnt_arg, struct cnt_same_symbol_args, pa_extra);

	if (cnt_arg->symbol_id == sp->lf_symbol_id &&
	    cnt_arg->path_type == sp->type &&
	    cnt_arg->pseudo == sp->pseudo)
		cnt_arg->cnt ++;

	LIST_GO_OVER;
}


static LIST_IT_CALLBK(overwrite_pathID_to_bondvar_sz)
{
	struct list_it this_list;
	struct cnt_same_symbol_args cnt_arg;
	LIST_OBJ(struct subpath, sp, ln);

	/* get iterator of this list */
	this_list = list_get_it(pa_head->now);

	/* go through this list to count subpaths with same symbol */
	cnt_arg.cnt = 0;
	cnt_arg.symbol_id = sp->lf_symbol_id;
	cnt_arg.path_type = sp->type;
	cnt_arg.pseudo    = sp->pseudo;
	list_foreach(&this_list, &cnt_same_symbol, &cnt_arg);

	/* overwrite path_id to cnt number */
	sp->path_id = cnt_arg.cnt;

	LIST_GO_OVER;
}

static LIST_IT_CALLBK(assign_pathID_by_order)
{
	LIST_OBJ(struct subpath, sp, ln);
	P_CAST(new_path_id, uint32_t, pa_extra);

	/* assign path_id in order, from 1 to maximum 64. */
	sp->path_id = ++(*new_path_id);

	LIST_GO_OVER;
}

/*
 * path inverted list wrappers
 */
static uint64_t path_invlist_cur(void *ent_)
{
	P_CAST(ent, struct math_invlist_entry_reader, ent_);
	return invlist_iter_curkey(ent->reader);
}

static int path_invlist_next(void *ent_)
{
	P_CAST(ent, struct math_invlist_entry_reader, ent_);
	return invlist_iter_next(ent->reader);
}

static int path_invlist_skip(void *ent_, uint64_t target)
{
	P_CAST(ent, struct math_invlist_entry_reader, ent_);
	return invlist_iter_jump(ent->reader, target);
}

static size_t path_invlist_read(void *ent_, void *dest, size_t sz)
{
	P_CAST(ent, struct math_invlist_entry_reader, ent_);
	P_CAST(ditem, struct math_invlist_deep_item, dest);
	size_t rd_sz = 0;

	switch (sz) {
	case sizeof(struct math_invlist_item):
		rd_sz = invlist_iter_read(ent->reader, dest);
		break;

	case sizeof(struct math_invlist_deep_item):
		rd_sz += invlist_iter_read(ent->reader, ditem);
		fseek(ent->fh_symbinfo, ditem->item.symbinfo_offset, SEEK_SET);
		rd_sz += fread(&ditem->info, 1, sizeof ditem->info, ent->fh_symbinfo);
		break;

	default:
		break;
	}

	return rd_sz;
}

/*
 * main exported functions
 */
int math_qry_prepare(math_index_t mi, const char *tex, struct math_qry *mq)
{
	memset(mq, 0, sizeof *mq);

	/*
	 * save TeX
	 */
	mq->tex = strdup(tex);

	/*
	 * parse TeX
	 */
	struct tex_parse_ret parse_res;
	parse_res = tex_parse(tex, 0, true /* keep OPT */, true /* concrete */);
	if (parse_res.code == PARSER_RETCODE_ERR ||
	    parse_res.operator_tree == NULL) {
		return 1;
	}

	/*
	 * save OPT
	 */
	mq->optr = parse_res.operator_tree;

#ifdef DEBUG_PREPARE_MATH_QRY
	optr_print(mq->optr, stdout);
#endif

	/*
	 * save subpaths and sort them by bond variable size
	 */
	struct subpaths subpaths = parse_res.subpaths;
	list_foreach(&subpaths.li, &overwrite_pathID_to_bondvar_sz, NULL);

	struct list_sort_arg sort_arg = {&compare_qry_path, NULL};
	list_sort(&subpaths.li, &sort_arg);

	uint32_t new_path_id = 0;
	list_foreach(&subpaths.li, &assign_pathID_by_order, &new_path_id);

	mq->subpaths = subpaths;

#ifdef DEBUG_PREPARE_MATH_QRY
	subpaths_print(&subpaths, stdout);
#endif

	/*
	 * save subpath set
	 */
	mq->subpath_set = subpath_set(subpaths, SUBPATH_SET_QUERY);

#ifdef DEBUG_PREPARE_MATH_QRY
	print_subpath_set(mq->subpath_set);
#endif

	/*
	 * setup merger for elements in subpath set
	 */
	float N  = mi->stats.N;

	foreach (iter, li, mq->subpath_set) {
		struct subpath_ele *ele = li_entry(ele, iter->cur, ln);
		struct subpath *sp = ele->dup[0];
		char path_key[MAX_DIR_PATH_NAME_LEN] = "";

		if (0 != mk_path_str(sp, ele->prefix_len, path_key)) {
			prerr("subpath too long or unexpected type.\n");
			continue;
		}

		struct math_invlist_entry_reader *ent = malloc(sizeof *ent);
		*ent = math_index_lookup(mi, path_key);

		if (ent->pf) {
			mq->merge_set.iter[mq->merge_set.n] = ent;
			mq->merge_set.upp[mq->merge_set.n]  = logf(N / ent->pf);
			mq->merge_set.cur[mq->merge_set.n]  = path_invlist_cur;
			mq->merge_set.next[mq->merge_set.n] = path_invlist_next;
			mq->merge_set.skip[mq->merge_set.n] = path_invlist_skip;
			mq->merge_set.read[mq->merge_set.n] = path_invlist_read;
		} else {
			mq->merge_set.iter[mq->merge_set.n] = NULL;
			mq->merge_set.upp[mq->merge_set.n]  = 0;
			mq->merge_set.cur[mq->merge_set.n]  = empty_invlist_cur;
			mq->merge_set.next[mq->merge_set.n] = empty_invlist_next;
			mq->merge_set.skip[mq->merge_set.n] = empty_invlist_skip;
			mq->merge_set.read[mq->merge_set.n] = empty_invlist_read;
			free(ent);
		}

		mq->ele[mq->merge_set.n] = ele; /* save for later */
		mq->merge_set.n += 1;
	}


	return 0;
}

void math_qry_release(struct math_qry *mq)
{
	if (mq->tex)
		free((char*)mq->tex);

	if (mq->optr)
		optr_release((struct optr_node*)mq->optr);

	if (mq->subpaths.n_lr_paths)
		subpaths_release(&mq->subpaths);

	if (mq->subpath_set) {
		li_free(mq->subpath_set, struct subpath_ele, ln, free(e));
	}

	for (int i = 0; i < mq->merge_set.n; i++) {
		struct math_invlist_entry_reader *ent;
		ent = mq->merge_set.iter[i];
		if (ent) {
			invlist_iter_free(ent->reader);
			fclose(ent->fh_symbinfo);
			free(ent);
		}
	}
}
