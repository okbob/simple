/*-------------------------------------------------------------------------
 *
 * simple
 *	  simple demo extension
 *
 * Author:	Pavel Stehule
 * Postcardware licence @2024
 *
 * IDENTIFICATION
 *	  simple.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "varatt.h"

#include "catalog/namespace.h"

#include "utils/builtins.h"
#include "utils/regproc.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(int_func);
PG_FUNCTION_INFO_V1(text_func);

static needs_fmgr_hook_type prev_needs_fmgr_hook = NULL;
static fmgr_hook_type prev_fmgr_hook = NULL;

static Oid text_func_oid = InvalidOid;
static Oid int_func_oid = InvalidOid;

#define SIMPLE_MAGIC		2024100316

typedef struct
{
	int			magic;
	bool		is_text_func;
	bool		is_int_func;
	Datum		prev_arg;
} simple_fmgr_cache;

/*
 * This is an example of fmgr hook - this hook is used for any
 * call of SQL function. It is one possibility for handling an
 * exeption inside SQL function. Note: the exception cannot be
 * ignored.
 */
Datum
int_func(PG_FUNCTION_ARGS)
{
	int32	arg = PG_GETARG_INT32(0);

	PG_RETURN_INT32(arg + 10);
}

Datum
text_func(PG_FUNCTION_ARGS)
{
	text	   *t;
	text	   *result;
	StringInfoData str;

	if (PG_ARGISNULL(0))
		ereport(ERROR,
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
				 errmsg("argument of text func cannot be null"),
				 errhint("use non null value")));

	t = PG_GETARG_TEXT_PP(0);

	elog(NOTICE, "input string is: \"%s\"", text_to_cstring(t));

	initStringInfo(&str);

	appendBinaryStringInfo(&str, VARDATA_ANY(t), VARSIZE_ANY_EXHDR(t));
	appendStringInfoString(&str, ", světe");

	result = cstring_to_text_with_len(str.data, str.len);

	pfree(str.data);

	PG_RETURN_TEXT_P(result);
}

static Oid
get_proname_oid(const char *proname)
{
	List	   *names;
	FuncCandidateList clist;

	names = stringToQualifiedNameList(proname, NULL);
	clist = FuncnameGetCandidates(names, -1, NIL, false, false, false, true);

	if (clist == NULL)
		elog(ERROR, "function \"%s\" doesn't exists", proname);
	else if (clist->next != NULL)
		elog(ERROR, "more than one function named \"%s\"", proname);

	return clist->oid;
}

static bool
simple_needs_fmgr_hook(Oid fn_oid)
{
	if (prev_needs_fmgr_hook &&
		(*prev_needs_fmgr_hook) (fn_oid))
		return true;

	if (!OidIsValid(text_func_oid))
	{
		text_func_oid = get_proname_oid("text_func");
		int_func_oid = get_proname_oid("int_func");
	}

	return fn_oid == text_func_oid || fn_oid == int_func_oid;
}
/*
 * Inside hooks we should to think about other extensions
 * that can to use same hook.
 */
static void
simple_fmgr_hook(FmgrHookEventType event,
					FmgrInfo *flinfo, Datum *private)
{
	simple_fmgr_cache *fcache = (simple_fmgr_cache *) DatumGetPointer(*private);
	const char *funcname;

	/*
	 * fmgr hook events should be executed in an order
	 * START, END | ABORT. But the extension can be initialized
	 * inside an function, and then START can missing. Theoretically
	 * the hook can be called with private data of another extension.
	 */
	if ((!fcache && event != FHET_START) ||
		(fcache && fcache->magic != SIMPLE_MAGIC))
	{
		if (prev_fmgr_hook)
			(*prev_fmgr_hook) (event, flinfo, private);

		return;
	}

	if (!fcache)
	{
		MemoryContext oldcxt = MemoryContextSwitchTo(flinfo->fn_mcxt);

		Assert(event == FHET_START);
		Assert(OidIsValid(text_func_oid));
		Assert(OidIsValid(int_func_oid));

		fcache = palloc0(sizeof(simple_fmgr_cache));

		fcache->magic = SIMPLE_MAGIC;
		fcache->is_text_func = flinfo->fn_oid == text_func_oid;
		fcache->is_int_func = flinfo->fn_oid == int_func_oid;

		MemoryContextSwitchTo(oldcxt);

		*private = PointerGetDatum(fcache);
	}

	if (fcache->is_text_func)
		funcname = "text_func";
	else if (fcache->is_int_func)
		funcname = "int_func";
	else
		funcname = NULL;

	if (funcname)
	{
		if (event == FHET_START)
			elog(NOTICE, "Function \"%s\" started", funcname);
		else if (event == FHET_END)
			elog(NOTICE, "Function \"%s\" ended", funcname);
		else if (event == FHET_ABORT)
			elog(NOTICE, "Function \"%s\" aborted", funcname);
	}

	if (prev_fmgr_hook)
		(*prev_fmgr_hook) (event, flinfo, &fcache->prev_arg);
}

/*
 * module init function - attention, we cannot to touch
 * system catalog there. It can be loaded (from shared_preload_libraries)
 * before system catalog is prepared.
 */
void
_PG_init(void)
{
	prev_needs_fmgr_hook = needs_fmgr_hook;
	prev_fmgr_hook = fmgr_hook;

	needs_fmgr_hook = simple_needs_fmgr_hook;
	fmgr_hook = simple_fmgr_hook;
}
