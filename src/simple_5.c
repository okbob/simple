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

#include "catalog/pg_type.h"
#include "executor/spi.h"
#include "utils/builtins.h"
#include "utils/datum.h"

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
	Datum		args[1];
	char		nulls[1];
	Oid			types[1];
	int			res;
	Datum		result;
	MemoryContext call_mcxt;

	if (PG_ARGISNULL(0))
	{
		elog(NOTICE, "input argument is null");
		PG_RETURN_NULL();
	}

	args[0] = PG_GETARG_DATUM(0);
	nulls[0] = ' ';
	types[0] = TEXTOID;

	elog(NOTICE, "input string is: \"%s\"", text_to_cstring(DatumGetTextP(args[0])));

	call_mcxt = CurrentMemoryContext;

	SPI_connect();

	res = SPI_execute_with_args("SELECT ($1 || ', světe')::text",
							  1, types, args, nulls,
							  true, 1);

	if (res == SPI_OK_SELECT)
	{
		TupleDesc   tupdesc;
		MemoryContext oldcxt;
		bool		isnull;

		Assert(SPI_tuptable);

		tupdesc = SPI_tuptable->tupdesc;

		if (SPI_processed != 1 || tupdesc->natts != 1)
			elog(ERROR, "unexpected format (rows: %ld, cols: %d)",
						  SPI_processed, tupdesc->natts);

		if (SPI_gettypeid(tupdesc, 1) != TEXTOID)
			elog(ERROR, "unexpected type of result column `%s`",
				format_type_be(SPI_gettypeid(tupdesc, 1)));

		result = SPI_getbinval(SPI_tuptable->vals[0], tupdesc, 1, &isnull);

		if (isnull)
			elog(ERROR, "unexpected null");

		elog(NOTICE, "result: %s", TextDatumGetCString(result));

		oldcxt = MemoryContextSwitchTo(call_mcxt);

		result = datumCopy(result, false, -1);

		MemoryContextSwitchTo(oldcxt);
	}
	else
		elog(ERROR, "unexpected result status");

	SPI_finish();

	PG_RETURN_DATUM(result);
}
