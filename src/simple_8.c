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

#include "nodes/pg_list.h"
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
 * List is an list of nodes. In this example
 * list of String nodes.
 */
static
char *concat_list(List *strings)
{
	StringInfoData rstr;
	ListCell   *lc;

	initStringInfo(&rstr);

	foreach(lc, strings)
	{
		appendStringInfoString(&rstr, strVal(lfirst(lc)));
	}

	return rstr.data;
}

/*
 * Very important part of Postgres is processing Node based
 * structs. It allows comfortable work with dynamic structs.
 * Node based structs can be printed, compared, copied, ..
 */
Datum
text_func(PG_FUNCTION_ARGS)
{
	text	   *t;
	List	   *strings = NIL;

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();

	t = PG_GETARG_TEXT_PP(0);

	elog(NOTICE, "input string is: \"%s\"", text_to_cstring(t));

	strings = lappend(strings, makeString(text_to_cstring(t)));
	strings = lappend(strings, makeString(", světe"));

	PG_RETURN_TEXT_P(cstring_to_text(concat_list(strings)));
}
