-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION simple" to load this file. \quit

---------------------------------------------------
CREATE FUNCTION int_func(int)
	RETURNS int
	AS 'MODULE_PATHNAME'
	LANGUAGE C
	IMMUTABLE STRICT;

CREATE FUNCTION text_func(text)
	RETURNS text
	AS 'MODULE_PATHNAME'
	LANGUAGE C
	IMMUTABLE STRICT;
