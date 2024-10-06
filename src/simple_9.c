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

#include "nodes/parsenodes.h"		/* for DefElem */
#include "nodes/pg_list.h"
#include "nodes/print.h"			/* for elog_node_display */
#include "nodes/value.h"
#include "utils/builtins.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(int_func);
PG_FUNCTION_INFO_V1(text_func);

Datum int_func(PG_FUNCTION_ARGS);
Datum text_func(PG_FUNCTION_ARGS);

Datum
int_func(PG_FUNCTION_ARGS)
{
	int32	arg = PG_GETARG_INT32(0);

	PG_RETURN_INT32(arg + 10);
}

/*
 * We are able to process nodes dynamicaly. Nodes holds type info.
 * Use castNode for safe cast (with assertions) or nodeTag to get
 * node tag (type enum) or IsA function.
 */
static
char *concat_list(List *vals, char *delim)
{
	StringInfoData rstr;
	ListCell   *lc;
	bool		is_first = true;

	initStringInfo(&rstr);

	foreach(lc, vals)
	{
		Node	   *n = (Node *) lfirst(lc);

		if (!is_first)
			appendStringInfoString(&rstr, delim);
		else
			is_first = false;

		if (IsA(n, String))
			appendStringInfoString(&rstr, strVal(n));
		else if (IsA(n, Integer))
		{
			Integer		*intnode = castNode(Integer, n);

			Assert(nodeTag(n) == T_Integer);

			/* appendStringInfo(&rstr, "%d", intVal(n)); */
			appendStringInfo(&rstr, "%d", intnode->ival);
		}
		/*
		else
			elog(ERROR, "unexpected node type: %d", nodeTag(n));
		*/
	}

	return rstr.data;
}

/*
 * Defelem Node (named value) is often used type.
 */
Datum
text_func(PG_FUNCTION_ARGS)
{
	text	   *t;
	List	   *vals = NIL;
	DefElem	   *defelem = NULL;

	if (PG_ARGISNULL(0))
		ereport(ERROR,
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
				 errmsg("argument of text func cannot be null"),
				 errhint("use non null value")));

	t = PG_GETARG_TEXT_PP(0);

	elog(NOTICE, "input string is: \"%s\"", text_to_cstring(t));

	vals = lappend(vals, makeString(text_to_cstring(t)));
	vals = lappend(vals, makeString("světe"));
	vals = lappend(vals, makeInteger(10));

	/* copy of makeDefElem */
	defelem = makeNode(DefElem);
	defelem->defnamespace = NULL;
	defelem->defname = "test bool argument";
	defelem->arg = (Node *) makeBoolean(true);
	defelem->location = -1;

	vals = lappend(vals, (Node *) defelem);

	elog_node_display(NOTICE, "*** debug output ****", vals, true);

	PG_RETURN_TEXT_P(cstring_to_text(concat_list(vals, ", ")));
}
