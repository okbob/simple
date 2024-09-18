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

Datum int_func(PG_FUNCTION_ARGS);
Datum text_func(PG_FUNCTION_ARGS);

Datum
int_func(PG_FUNCTION_ARGS)
{
	int32	arg = PG_GETARG_INT32(0);

	PG_RETURN_INT32(arg + 10);
}

Datum
text_func(PG_FUNCTION_ARGS)
{
	text	   *t = PG_GETARG_TEXT_PP(0);
	text	   *result;
	size_t		input_str_sz;
	size_t		append_str_sz;
	const char *str = ", světe";

	input_str_sz = VARSIZE_ANY_EXHDR(t);
	append_str_sz = strlen(str);

	result = palloc(input_str_sz + append_str_sz + VARHDRSZ);

	/* use macros _ANY for input, without for output */

	memcpy(VARDATA(result), VARDATA_ANY(t), input_str_sz);
	memcpy(VARDATA(result) + input_str_sz, str, append_str_sz);

	SET_VARSIZE(result, input_str_sz + append_str_sz + VARHDRSZ);

	PG_RETURN_TEXT_P(result);
}
