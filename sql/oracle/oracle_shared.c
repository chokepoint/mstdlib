/* The MIT License (MIT)
 * 
 * Copyright (c) 2017 Main Street Softworks, Inc.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <mstdlib/mstdlib_sql.h>
#include <mstdlib/sql/m_sql_driver.h>
#include "oracle_shared.h"


M_sql_error_t oracle_resolve_error(const char *sqlstate, M_int32 errorcode)
{
	/* Code reference: http://ora-${CODE}.ora-code.com/ 
	 * This code list obtained from:
	 * http://www.oracle.com/technology/tech/oci/pdf/taf_10.2.pdf
	 */
	(void)sqlstate;

	switch (errorcode) {
		case 1012: /* not logged on */
		case 12203: /* TNS: unable to connect to destination */
		case 12500: /* TNS: listener failed to start a dedicated server process */
		case 12571: /* TNS: packet writer failure */
		case 12153: /* TNS: not connected */
			return M_SQL_ERROR_CONN_FAILED;

		case 1033: /* initialization or shutdown in progress */
		case 1034: /* not available */
		case 1089: /* immediate shutdown in progress */
		case 3113: /* end of file on communication channel */
		case 3114: /* not connected */
		case 3135: /* connection lost contact */
			return M_SQL_ERROR_CONN_LOST;


		case 1017: /* invalid username/password; logon denied */
			return M_SQL_ERROR_CONN_BADAUTH;

		case 54:    /* resource busy and acquire with NOWAIT specified */
		case 8176:  /* consistent read failure; rollback data not available */
		case 8177:  /* can't serialize access for this transaction */
		case 30006: /* resource busy; acquire with WAIT timeout expired */
			return M_SQL_ERROR_QUERY_DEADLOCK;

		case 1: /* unique constraint violated */
		case 2239: /* there are objects which reference this sequence */
		case 2266: /* unique/primary keys in table referenced by enabled foreign keys */
		case 2290: /* check constraint (string.string) violated */
		case 2291: /* integrity constraint (string.string) violated - parent key not found */
		case 2292: /* integrity constraint (string.string) violated - child record found */
		case 2449: /* unique/primary keys in table referenced by foreign keys */
			return M_SQL_ERROR_QUERY_CONSTRAINT;

		default:
			break;
	}

	return M_SQL_ERROR_QUERY_FAILURE;
}


M_sql_error_t oracle_cb_connect_runonce(M_sql_conn_t *conn, M_sql_driver_connpool_t *dpool, M_bool is_first_in_pool, M_bool is_readonly, char *error, size_t error_size)
{
	M_sql_error_t           err   = M_SQL_ERROR_SUCCESS;
	M_sql_stmt_t           *stmt;

	(void)dpool;

	/* Set the default session isolation level to READ COMMITTED (should be the default, but
	 * doesn't hurt to make sure). */
	stmt = M_sql_conn_execute_simple(conn, "ALTER SESSION SET ISOLATION_LEVEL = READ COMMITTED", M_FALSE);
	err  = M_sql_stmt_get_error(stmt);
	if (stmt == NULL || err != M_SQL_ERROR_SUCCESS) {
		M_snprintf(error, error_size, "SET SESSION ISOLATION READ COMMITTED failed: %s: %s", M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
		M_sql_stmt_destroy(stmt);
		return err;
	}
	M_sql_stmt_destroy(stmt);

	/* If not the first connection in the write pool, end here */
	if (!is_first_in_pool || is_readonly) {
		return M_SQL_ERROR_SUCCESS;
	}

	/* Oracle lacks the BITOR() function, create our own */
	stmt = M_sql_conn_execute_simple(conn,
	                                 "CREATE OR REPLACE FUNCTION BITOR(x IN NUMBER, y IN NUMBER) RETURN NUMBER AS\n"
	                                 "BEGIN\n"
	                                 "  RETURN x + y - BITAND(x, y);\n"
	                                 "END;",
	                                 M_TRUE
	                                );
	err  = M_sql_stmt_get_error(stmt);
	if (stmt == NULL || err != M_SQL_ERROR_SUCCESS) {
		M_snprintf(error, error_size, "Failed to create a BITOR function: %s: %s", M_sql_error_string(err), M_sql_stmt_get_error_string(stmt));
		M_sql_stmt_destroy(stmt);
		return err;
	}
	M_sql_stmt_destroy(stmt);

	return M_SQL_ERROR_SUCCESS;
}




M_bool oracle_cb_datatype(M_sql_connpool_t *pool, M_buf_t *buf, M_sql_data_type_t type, size_t max_len)
{
	(void)pool;

	if (max_len == 0) {
		max_len = SIZE_MAX;
	}

	switch (type) {
		case M_SQL_DATA_TYPE_BOOL:
			M_buf_add_str(buf, "NUMBER(1)");   /* 1 bit max -> 0/1 (1 digit) */
			return M_TRUE;
		case M_SQL_DATA_TYPE_INT16:
			M_buf_add_str(buf, "NUMBER(5)");   /* 16 bit max -> 32767 (5 digits) */
			return M_TRUE;
		case M_SQL_DATA_TYPE_INT32:
			M_buf_add_str(buf, "NUMBER(10)");  /* 32 bit max -> 2,147,483,647 (10 digits) */
			return M_TRUE;
		case M_SQL_DATA_TYPE_INT64:
			M_buf_add_str(buf, "NUMBER(19)");  /* 64 bit max -> 9,223,372,036,854,775,807 (19 digits) */
			return M_TRUE;
		case M_SQL_DATA_TYPE_TEXT:
			if (max_len <= 4000) {
				M_buf_add_str(buf, "VARCHAR2(");
				M_buf_add_uint(buf, max_len);
				M_buf_add_str(buf, ")");
			} else {
				M_buf_add_str(buf, "CLOB");
			}
			return M_TRUE;
		case M_SQL_DATA_TYPE_BINARY:
			if (max_len <= 2000) {
				M_buf_add_str(buf, "RAW(");
				M_buf_add_uint(buf, max_len);
				M_buf_add_str(buf, ")");
			} else {
				M_buf_add_str(buf, "BLOB"); /* Not LONG RAW */
			}
			return M_TRUE;
		/* These data types don't really exist */
		case M_SQL_DATA_TYPE_NULL:
		case M_SQL_DATA_TYPE_UNKNOWN:
			break;
	}
	return M_FALSE;
}


void oracle_cb_append_updlock(M_sql_connpool_t *pool, M_buf_t *query, M_sql_query_updlock_type_t type)
{
	(void)pool;
	M_sql_driver_append_updlock(M_SQL_DRIVER_UPDLOCK_CAP_FORUPDATE, query, type);
}


M_bool oracle_cb_append_bitop(M_sql_connpool_t *pool, M_buf_t *query, M_sql_query_bitop_t op, const char *exp1, const char *exp2)
{
	(void)pool;
	return M_sql_driver_append_bitop(M_SQL_DRIVER_BITOP_CAP_FUNC, query, op, exp1, exp2);
}
