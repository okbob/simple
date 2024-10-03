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

#include "utils/builtins.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(int_func);
PG_FUNCTION_INFO_V1(text_func);

/*
 *
 *
 * #define PG_FUNCTION_ARGS<-->FunctionCallInfo fcinfo
 * #define PG_ARGISNULL(n)  (fcinfo->args[n].isnull)
 * viz fmgr.h
 */
Datum
int_func(PG_FUNCTION_ARGS)
{
	Datum	arg = PG_GETARG_DATUM(0);
	Datum	result;

	result = DirectFunctionCall2(int4pl,
								 arg,
								 Int32GetDatum((int32) 10));
	PG_RETURN_DATUM(result);
}

Datum
text_func(PG_FUNCTION_ARGS)
{
	text	   *t = NULL;
	text	   *result;
	StringInfoData str;

	if (PG_ARGISNULL(0))
	{
		elog(NOTICE, "input argument is null");
		PG_RETURN_NULL();
	}

	t = PG_GETARG_TEXT_PP(0);

	elog(NOTICE, "input string is: \"%s\"", text_to_cstring(t));

	initStringInfo(&str);

	appendBinaryStringInfo(&str, VARDATA_ANY(t), VARSIZE_ANY_EXHDR(t));
	appendStringInfoString(&str, ", světe");

	result = cstring_to_text_with_len(str.data, str.len);

	pfree(str.data);

	PG_RETURN_TEXT_P(result);
}
