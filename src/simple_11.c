/*-------------------------------------------------------------------------
 *
 * simple
 *	  example of usage plpgsql plugin API
 *
 * Author:	Pavel Stehule
 * Postcardware licence @2026
 *
 * IDENTIFICATION
 *	  simple.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "plpgsql.h"

/*
 * Module signature - the extension should be compiled
 * with correct postgres libraries. Important build
 * description is encoded into module's magic, and
 * checked when module is loaded.
 */
PG_MODULE_MAGIC;

static void simple_func_setup(PLpgSQL_execstate *estate, PLpgSQL_function *func);
static void simple_func_beg(PLpgSQL_execstate *estate, PLpgSQL_function *func);
static void simple_func_end(PLpgSQL_execstate *estate, PLpgSQL_function *func);
static void simple_stmt_beg(PLpgSQL_execstate *estate, PLpgSQL_stmt *stmt);
static void simple_stmt_end(PLpgSQL_execstate *estate, PLpgSQL_stmt *stmt);

static PLpgSQL_plugin simple_plpgsql_plugin = {
	simple_func_setup,
	simple_func_beg,
	simple_func_end,
	simple_stmt_beg,
	simple_stmt_end,
	NULL, NULL, NULL, NULL, NULL
};

typedef const char *(*plpgsql_stmt_typename_t) (PLpgSQL_stmt *stmt);
static plpgsql_stmt_typename_t plpgsql_stmt_typename_p;

#define LOAD_EXTERNAL_FUNCTION(file, funcname)	((void *) (load_external_function(file, funcname, true, NULL)))

typedef struct {
	Oid		fn_oid;
	char   *fn_signature;
	int		use_count;
	PLpgSQL_execstate *estate;
	int		current_stmtid;

	int	   *parentids;
	int	   *naturalids;
	int	   *levels;

	PLpgSQL_stmt **stmts_stack;
	int		stmts_stack_top;

	MemoryContextCallback er_mcb;
} SimplePluginData;

typedef SimplePluginData *SimplePluginInfo;

static void
build_maps_stmt(PLpgSQL_stmt *stmt,
				int *parentids, int parentid,
				int *naturalids, int *naturalid,
				int *levels, int level,
				int *max_deep, int cur_deep);

static void
build_maps_stmts(List *stmts,
				 int *parentids, int parentid,
				 int *naturalids, int *naturalid,
				 int *levels, int level,
				 int *max_deep, int cur_deep)
{
	ListCell *lc;

	foreach(lc, stmts)
	{
		build_maps_stmt((PLpgSQL_stmt *) lfirst(lc),
						parentids, parentid,
						naturalids, naturalid,
						levels, level,
						max_deep, cur_deep);
	}
}

static void
build_maps_stmt(PLpgSQL_stmt *stmt,
				int *parentids, int parentid,
				int *naturalids, int *naturalid,
				int *levels, int level,
				int *max_deep, int cur_deep)
{
	int		stmtid_zb = stmt->stmtid - 1;

	if (parentids)
		parentids[stmtid_zb] = parentid;

	if (naturalids)
		naturalids[stmtid_zb] = ++(*naturalid);

	if (levels)
		levels[stmtid_zb] = level;

	/*
	 * When this statement is visible, then nested
	 * statements will be in higher levels.
	 */
	if (stmt->lineno < 1)
		level += 1;

	if (cur_deep > *max_deep)
		*max_deep = cur_deep;

	cur_deep += 1;

	switch (stmt->cmd_type)
	{
		case PLPGSQL_STMT_BLOCK:
			{
				PLpgSQL_stmt_block *s = (PLpgSQL_stmt_block *) stmt;

				build_maps_stmts(s->body,
								 parentids, stmt->stmtid,
								 naturalids, naturalid,
								 levels, level,
								 max_deep, cur_deep);

				if (s->exceptions)
				{
					ListCell *lc;

					foreach(lc, s->exceptions->exc_list)
					{
						build_maps_stmts(((PLpgSQL_exception *) lfirst(lc))->action,
										 parentids, stmt->stmtid,
										 naturalids, naturalid,
										 levels, level,
										 max_deep, cur_deep);
					}
				}
			}
			break;

		case PLPGSQL_STMT_IF:
			{
				PLpgSQL_stmt_if *s = (PLpgSQL_stmt_if *) stmt;
				ListCell *lc;

				build_maps_stmts(s->then_body,
								 parentids, stmt->stmtid,
								 naturalids, naturalid,
								 levels, level,
								 max_deep, cur_deep);

				foreach(lc, s->elsif_list)
				{
					build_maps_stmts(((PLpgSQL_if_elsif *) lfirst(lc))->stmts,
									 parentids, stmt->stmtid,
									 naturalids, naturalid,
									 levels, level,
									 max_deep, cur_deep);
				}

				build_maps_stmts(s->else_body,
								 parentids, stmt->stmtid,
								 naturalids, naturalid,
								 levels, level,
								 max_deep, cur_deep);
			}
			break;

		case PLPGSQL_STMT_CASE:
			{
				PLpgSQL_stmt_case *s = (PLpgSQL_stmt_case *) stmt;
				ListCell *lc;

				foreach(lc, s->case_when_list)
				{
					build_maps_stmts(((PLpgSQL_case_when *) lfirst(lc))->stmts,
									 parentids, stmt->stmtid,
									 naturalids, naturalid,
									 levels, level,
									 max_deep, cur_deep);
				}

				build_maps_stmts(s->else_stmts,
								 parentids, stmt->stmtid,
								 naturalids, naturalid,
								 levels, level,
								 max_deep, cur_deep);
			}
			break;

		case PLPGSQL_STMT_LOOP:
			build_maps_stmts(((PLpgSQL_stmt_loop *) stmt)->body,
							 parentids, stmt->stmtid,
							 naturalids, naturalid,
							 levels, level,
							 max_deep, cur_deep);
			break;

		case PLPGSQL_STMT_FORI:
			build_maps_stmts(((PLpgSQL_stmt_fori *) stmt)->body,
							 parentids, stmt->stmtid,
							 naturalids, naturalid,
							 levels, level,
							 max_deep, cur_deep);
			break;

		case PLPGSQL_STMT_FORS:
			build_maps_stmts(((PLpgSQL_stmt_fors *) stmt)->body,
							 parentids, stmt->stmtid,
							 naturalids, naturalid,
							 levels, level,
							 max_deep, cur_deep);
			break;

		case PLPGSQL_STMT_FORC:
			build_maps_stmts(((PLpgSQL_stmt_forc *) stmt)->body,
							 parentids, stmt->stmtid,
							 naturalids, naturalid,
							 levels, level,
							 max_deep, cur_deep);
			break;

		case PLPGSQL_STMT_DYNFORS:
			build_maps_stmts(((PLpgSQL_stmt_dynfors *) stmt)->body,
							 parentids, stmt->stmtid,
							 naturalids, naturalid,
							 levels, level,
							 max_deep, cur_deep);
			break;

		case PLPGSQL_STMT_FOREACH_A:
			build_maps_stmts(((PLpgSQL_stmt_foreach_a *) stmt)->body,
							 parentids, stmt->stmtid,
							 naturalids, naturalid,
							 levels, level,
							 max_deep, cur_deep);
			break;

		case PLPGSQL_STMT_WHILE:
			build_maps_stmts(((PLpgSQL_stmt_while *) stmt)->body,
							 parentids, stmt->stmtid,
							 naturalids, naturalid,
							 levels, level,
							 max_deep, cur_deep);
			break;

		default:
			break;
	}
}


static void
plugin_info_reset(void *arg)
{
	SimplePluginInfo plugin_info = (SimplePluginInfo) arg;

	while (plugin_info->stmts_stack_top > 0)
	{
		PLpgSQL_stmt *failed = plugin_info->stmts_stack[plugin_info->stmts_stack_top - 1];

		elog(NOTICE, "execution failed %s", plpgsql_stmt_typename_p(failed));
		plugin_info->stmts_stack_top -= 1;
	}

	elog(NOTICE, "execution failed - leave %s", plugin_info->fn_signature);
}

static void
simple_func_setup(PLpgSQL_execstate *estate, PLpgSQL_function *func)
{
	SimplePluginInfo plugin_info = palloc0(sizeof(SimplePluginData));
	int		max_deep = 0;
	int		naturalid = 0;

	elog(NOTICE, ">>>func_setup %s", estate->func->fn_signature);

	estate->plugin_info = plugin_info;

	plugin_info->fn_oid = func->fn_oid;
	elog(NOTICE, "*** >>>> fn_oid: %d", plugin_info->fn_oid);

	plugin_info->fn_signature = pstrdup(func->fn_signature);
	plugin_info->use_count = func->cfunc.use_count;
	plugin_info->estate = estate;

	plugin_info->parentids = palloc0(sizeof(int) * func->nstatements);
	plugin_info->naturalids = palloc0(sizeof(int) * func->nstatements);
	plugin_info->levels = palloc0(sizeof(int) * func->nstatements);

	build_maps_stmt((PLpgSQL_stmt *) func->action,
					plugin_info->parentids, 0,
					plugin_info->naturalids, &naturalid,
					plugin_info->levels, 0,
					&max_deep, 0);

	plugin_info->stmts_stack = palloc(sizeof(PLpgSQL_stmt*) * (max_deep + 1));
	plugin_info->stmts_stack_top = 0;

	plugin_info->er_mcb.func = plugin_info_reset;
	plugin_info->er_mcb.arg = plugin_info;

	MemoryContextRegisterResetCallback(CurrentMemoryContext,
									   &plugin_info->er_mcb);
}

static void
simple_func_beg(PLpgSQL_execstate *estate, PLpgSQL_function *func)
{
	SimplePluginInfo plugin_info = (SimplePluginInfo) estate->plugin_info;

	elog(NOTICE, ">>>func_beg %s", estate->func->fn_signature);

	if (plugin_info && plugin_info->estate == estate &&
		plugin_info->fn_oid == func->fn_oid &&
		plugin_info->use_count == func->cfunc.use_count)
		elog(NOTICE, "plugin info is correct");
}

static void
simple_func_end(PLpgSQL_execstate *estate, PLpgSQL_function *func)
{
	SimplePluginInfo plugin_info = (SimplePluginInfo) estate->plugin_info;

	elog(NOTICE, ">>>func_end %s", estate->func->fn_signature);

	if (plugin_info && plugin_info->estate == estate &&
		plugin_info->fn_oid == func->fn_oid &&
		plugin_info->use_count == func->cfunc.use_count)
		elog(NOTICE, "plugin info is correct");

	MemoryContextUnregisterResetCallback(CurrentMemoryContext,
									   &plugin_info->er_mcb);
}

static void
simple_stmt_beg(PLpgSQL_execstate *estate, PLpgSQL_stmt *stmt)
{
	SimplePluginInfo plugin_info = (SimplePluginInfo) estate->plugin_info;

	elog(NOTICE, ">>> %s.%s[%d]", estate->func->fn_signature,
		 plpgsql_stmt_typename_p(stmt),
		 stmt->stmtid);

	if (plugin_info && plugin_info->estate == estate &&
		plugin_info->fn_oid == estate->func->fn_oid &&
		plugin_info->use_count == estate->func->cfunc.use_count)
		elog(NOTICE, "plugin info is correct");

	if (estate->cur_error)
	{
		/*
		 * Only inside error handler we need reduce statements
		 * from stacks, because stmt_end was skipped due some
		 * exception. All statements until parent of current
		 * statements should be closed.
		 */
		int		parentid = plugin_info->parentids[stmt->stmtid - 1];

		while (plugin_info->stmts_stack[plugin_info->stmts_stack_top - 1]->stmtid != parentid)
		{
			PLpgSQL_stmt *failed = plugin_info->stmts_stack[plugin_info->stmts_stack_top - 1];

			elog(NOTICE, "execution failed %s", plpgsql_stmt_typename_p(failed));
			plugin_info->stmts_stack_top -= 1;

			if (plugin_info->stmts_stack_top < 1)
				elog(ERROR, "broken statement stack, missing parent statement on stack");
		}
	}

	plugin_info->current_stmtid = stmt->stmtid;
	plugin_info->stmts_stack[plugin_info->stmts_stack_top++] = stmt;
}

static void
simple_stmt_end(PLpgSQL_execstate *estate, PLpgSQL_stmt *stmt)
{
	SimplePluginInfo plugin_info = (SimplePluginInfo) estate->plugin_info;

	elog(NOTICE, "<<< %s.%s[%d]", estate->func->fn_signature,
		 plpgsql_stmt_typename_p(stmt),
		 stmt->stmtid);

	if (plugin_info && plugin_info->estate == estate &&
		plugin_info->fn_oid == estate->func->fn_oid &&
		plugin_info->use_count == estate->func->cfunc.use_count)
		elog(NOTICE, "plugin info is correct");

	if (plugin_info->stmts_stack[--plugin_info->stmts_stack_top] == stmt)
	{
		elog(NOTICE, "%s was correctly removed from stack",
			 plpgsql_stmt_typename_p(stmt));

		plugin_info->current_stmtid = plugin_info->stmts_stack[plugin_info->stmts_stack_top]->stmtid;
		elog(NOTICE, "NEW current_stmtid: %d", plugin_info->current_stmtid);
	}
	else
		elog(NOTICE, "broken stack");
}

void
_PG_init()
{
	PLpgSQL_plugin **plugin_ptr;
	static bool inited = false;

	if (inited)
		return;

	elog(NOTICE, "PLpgSQL plugin simple initialized");

	AssertVariableIsOfType(plpgsql_stmt_typename_p, plpgsql_stmt_typename_t);
	plpgsql_stmt_typename_p = (plpgsql_stmt_typename_t)
		LOAD_EXTERNAL_FUNCTION("$libdir/plpgsql", "plpgsql_stmt_typename");

	plugin_ptr = (PLpgSQL_plugin **) find_rendezvous_variable("PLpgSQL_plugin");
	*plugin_ptr = &simple_plpgsql_plugin;
	inited = true;
}
