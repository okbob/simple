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

Datum
int_func(PG_FUNCTION_ARGS)
{
	int32	arg = PG_GETARG_INT32(0);
	int32	result;

	result = DatumGetInt32(DirectFunctionCall2(int4pl,
											   Int32GetDatum(arg),
											   Int32GetDatum((int32) 10)));
	PG_RETURN_INT32(result);
}

Datum
text_func(PG_FUNCTION_ARGS)
{
	text	   *t = PG_GETARG_TEXT_PP(0);
	text	   *result;
	StringInfoData str;

	elog(NOTICE, "input string is: \"%s\"", text_to_cstring(t));

	initStringInfo(&str);

	appendBinaryStringInfo(&str, VARDATA_ANY(t), VARSIZE_ANY_EXHDR(t));
	appendStringInfoString(&str, ", světe");

	result = cstring_to_text_with_len(str.data, str.len);

	pfree(str.data);

	PG_RETURN_TEXT_P(result);
}
