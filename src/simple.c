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

/*
 * Module signature - the extension should be compiled
 * with correct postgres libraries. Important build
 * description is encoded into module's magic, and
 * checked when module is loaded.
 */
PG_MODULE_MAGIC;

/*
 * Function signature - ensures outer visibility of
 * function in library and v1 call convention can
 * be used safely.
 */
PG_FUNCTION_INFO_V1(int_func);
PG_FUNCTION_INFO_V1(text_func);

/*
 * Usage of V1 call convention macros
 */
Datum
int_func(PG_FUNCTION_ARGS)
{
	int32	arg = PG_GETARG_INT32(0);

	PG_RETURN_INT32(arg + 10);
}

/*
 * This function is not marked as STRICT, so call with NULL
 * should to fail (NULL is not handled yet). You should to
 * use gdb to attach process and catch SIGSEGV.
 *
 * For extension development use the PostgreSQL buuild with
 * enabled assertions (--enable-cassert), without optimalizations
 * (-O0) and with debug support (--enable-debug). Debug support and
 * O0 are important for gdb. Assertions increases possibility to
 * detect memory errors and random fails. You can check build by
 * `SELECT * FROM pg_config` or `SHOW debug_assertions`.
 *
 * PostgreSQL varchar or text type are not C zero ended string!
 * The PostgreSQL's types are different system than C languages
 * types. C strings are used for input/output operations. For any
 * other operations are used types encoded to Datum type (fix bytes
 * of same size like pointer size or pointer to VARLENA structure).
 *
 * There are lot of postgresql's internal functions for conversions
 * from/to C string. You can see a StringInfo related functions.
 * StringInfo allows comfortable work with dynamicaly sized C strings.
 * StringInfo functions are very fast, but there is some memory
 * overhead (usually can be accepted).
 */
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
