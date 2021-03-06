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

#ifndef __M_SQL_TRACE_H__
#define __M_SQL_TRACE_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/sql/m_sql.h>
#include <mstdlib/sql/m_sql_stmt.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_sql_trace SQL Pool Tracing
 *  \ingroup m_sql_conn
 * 
 * SQL Pool Tracing
 *
 * @{
 */

/*! Event types for SQL pool tracing */
typedef enum {
	M_SQL_TRACE_CONNECTING      = 1,  /*!< Starting a new connection */
	M_SQL_TRACE_CONNECTED       = 2,  /*!< New connection was successfully started */
	M_SQL_TRACE_CONNECT_FAILED  = 3,  /*!< New connection failed */
	M_SQL_TRACE_DISCONNECTING   = 4,  /*!< Starting graceful disconnect from server (idle, etc) */
	M_SQL_TRACE_DISCONNECTED    = 5,  /*!< Graceful disconnect has completed */
	M_SQL_TRACE_BEGIN_START     = 6,  /*!< SQL Transaction Begin starting.  This may internally also
	                                   *   call #M_SQL_TRACE_EXECUTE_START and #M_SQL_TRACE_EXECUTE_FINISH
	                                   *   depending on how the driver handles transactions. */
	M_SQL_TRACE_BEGIN_FINISH    = 7,  /*!< SQL Transaction Begin completed (possibly failed) */
	M_SQL_TRACE_ROLLBACK_START  = 8,  /*!< SQL Transaction Rollback starting. This may internally also
	                                   *   call #M_SQL_TRACE_EXECUTE_START and #M_SQL_TRACE_EXECUTE_FINISH
	                                   *   depending on how the driver handles transactions. */
	M_SQL_TRACE_ROLLBACK_FINISH = 9,  /*!< SQL Transaction Rollback completed (possibly failed) */
	M_SQL_TRACE_COMMIT_START    = 10, /*!< SQL Transaction Commit starting. This may internally also
	                                   *   call #M_SQL_TRACE_EXECUTE_START and #M_SQL_TRACE_EXECUTE_FINISH
	                                   *   depending on how the driver handles transactions.   On failure,
	                                   *   this may also flow through #M_SQL_TRACE_ROLLBACK_START and
	                                   *   #M_SQL_TRACE_ROLLBACK_FINISH */
	M_SQL_TRACE_COMMIT_FINISH   = 11, /*!< SQL Transaction Commit completed (possibly failed) */
	M_SQL_TRACE_EXECUTE_START   = 12, /*!< SQL Statement Execution started */
	M_SQL_TRACE_EXECUTE_FINISH  = 13, /*!< SQL Statement Execution finished (possibly failed) */
	M_SQL_TRACE_FETCH_START     = 14, /*!< SQL Statement Fetching result data started */
	M_SQL_TRACE_FETCH_FINISH    = 15, /*!< SQL Statement Fetching result data finished (possibly failed or canceled) */
	M_SQL_TRACE_CONNFAIL        = 16, /*!< Connection to SQL server failed unexpectedly */
	M_SQL_TRACE_TRANFAIL        = 17, /*!< SQL Transaction failed (duplicative of M_SQL_TRACE_EXECUTE_FINISH),
	                                   *    but only on fatal (non-retryable) failure */
	M_SQL_TRACE_DRIVER_DEBUG    = 18, /*!< Debug/Informational message generated by driver */
	M_SQL_TRACE_DRIVER_ERROR    = 19  /*!< Error message generated by driver */
} M_sql_trace_t;


/*! Connection type */
typedef enum {
	M_SQL_CONN_TYPE_UNKNOWN  = 0, /*!< Unknown, probably misuse */
	M_SQL_CONN_TYPE_PRIMARY  = 1, /*!< Primary (read/write) sub-pool */
	M_SQL_CONN_TYPE_READONLY = 2  /*!< Read Only sub-pool */
} M_sql_conn_type_t;


/*! Trace data */
struct M_sql_trace_data;
/*! Typedef for trace data */
typedef struct M_sql_trace_data M_sql_trace_data_t;


/*! Callback prototype used for tracing SQL subsystem events.
 *
 *  \param[in] event_type  The event type
 *  \param[in] data        The metadata about the event, use the M_sql_trace_*() functions to get details.
 *  \param[in] arg         User-supplied argument passed to the trace callback.
 */
typedef void (*M_sql_trace_cb_t)(M_sql_trace_t event_type, const M_sql_trace_data_t *data, void *arg);


/*! Add a trace callback to the SQL subsystem.
 *
 *  Only one trace callback can be registered per pool.  If one is already registered,
 *  it will be replaced.
 *
 *  \note This must be called prior to M_sql_connpool_start()
 *
 * \param[in] pool        Initialized pool object by M_sql_connpool_create().
 * \param[in] cb          Callback to register
 * \param[in] cb_arg      User-supplied argument to pass to callback.
 * \return M_TRUE on success, M_FALSE on misuse
 */
M_API M_bool M_sql_connpool_add_trace(M_sql_connpool_t *pool, M_sql_trace_cb_t cb, void *cb_arg);


/*! Set a flag on the statement to ensure a #M_SQL_TRACE_TRANFAIL is not triggered
 *  in the event of a failure.
 *
 *  This is used to silence warnings in the trace system for failures that may
 *  be expected.  For instance, this is used internally by M_sql_table_exists()
 *  otherwise a warning might be emitted when the table does not exist.
 *
 *  \param[in] stmt Initialized statement handle to apply flag
 */
M_API void M_sql_trace_ignore_tranfail(M_sql_stmt_t *stmt);


/*! Retrieve the error string containing the most recent error condition.
 *
 *  Only Valid on:
 *    - #M_SQL_TRACE_CONNECT_FAILED
 *    - #M_SQL_TRACE_BEGIN_FINISH
 *    - #M_SQL_TRACE_ROLLBACK_FINISH
 *    - #M_SQL_TRACE_COMMIT_FINISH
 *    - #M_SQL_TRACE_EXECUTE_FINISH
 *    - #M_SQL_TRACE_FETCH_FINISH
 *    - #M_SQL_TRACE_CONNFAIL
 *    - #M_SQL_TRACE_TRANFAIL
 *    - #M_SQL_TRACE_DRIVER_DEBUG
 *    - #M_SQL_TRACE_DRIVER_ERROR
 *
 *  \param[in] data Trace Data structure passed to trace callback
 *  \return Pointer to error string, or NULL if no error string available.
 */
M_API const char *M_sql_trace_get_error_string(const M_sql_trace_data_t *data);


/*! Retrieve the most recent error condition identifier.
 *  
 *  Only valid on:
 *    - #M_SQL_TRACE_CONNECT_FAILED
 *    - #M_SQL_TRACE_BEGIN_FINISH
 *    - #M_SQL_TRACE_ROLLBACK_FINISH
 *    - #M_SQL_TRACE_COMMIT_FINISH
 *    - #M_SQL_TRACE_EXECUTE_FINISH
 *    - #M_SQL_TRACE_FETCH_FINISH
 *    - #M_SQL_TRACE_CONNFAIL
 *    - #M_SQL_TRACE_TRANFAIL
 *
 *  \param[in] data Trace Data structure passed to trace callback
 *  \return Most recent error code, possibly #M_SQL_ERROR_SUCCESS if no error.
 */
M_API M_sql_error_t M_sql_trace_get_error(const M_sql_trace_data_t *data);


/*! Retrieve the duration, in milliseconds of operation
 *  
 *  Only valid on:
 *    - #M_SQL_TRACE_CONNECTED - Time to establish connection
 *    - #M_SQL_TRACE_DISCONNECTING - Time connection was up before disconnect was attempted.
 *    - #M_SQL_TRACE_DISCONNECTED - Time connection took to disconnect (from start of disconnect)
 *    - #M_SQL_TRACE_CONNECT_FAILED - Time it took for connection to fail.
 *    - #M_SQL_TRACE_BEGIN_FINISH - Time it took to begin a transaction.
 *    - #M_SQL_TRACE_ROLLBACK_FINISH - Time it took to rollback.
 *    - #M_SQL_TRACE_COMMIT_FINISH - Time it took to commit a transaction.
 *    - #M_SQL_TRACE_EXECUTE_FINISH - Time it took to execute the transaction.
 *    - #M_SQL_TRACE_FETCH_FINISH - Time it took to retrieve the rows after execution.
 *    - #M_SQL_TRACE_CONNFAIL - Time connection was up before a failure was detected.
 *    - #M_SQL_TRACE_TRANFAIL - Time query execution took before failure was returned.
 *
 *  \param[in] data Trace Data structure passed to trace callback
 *  \return Time in milliseconds.
 */
M_API M_uint64 M_sql_trace_get_duration_ms(const M_sql_trace_data_t *data);


/*! Retrieve the total duration of a sequence of events, for a limited set of
 *  events.
 *
 *  Only valid on:
 *    - #M_SQL_TRACE_FETCH_FINISH - Total time of query execution plus row fetch time.
 *    - #M_SQL_TRACE_DISCONNECTED - Total time from connection establishment to disconnect end.
 *
 *  \param[in] data Trace Data structure passed to trace callback
 *  \return Time in milliseconds.
 */
M_API M_uint64 M_sql_trace_get_total_duration_ms(const M_sql_trace_data_t *data);


/*! Retreive type of connection (Primary vs ReadOnly)
 *
 *  Available on all
 *
 *  \param[in] data Trace Data structure passed to trace callback
 *  \return Type of connection
 */
M_API M_sql_conn_type_t M_sql_trace_get_conntype(const M_sql_trace_data_t *data);


/*! Retrieve the internal connection id, enumerated from 0 - max_conns for each
 *  primary and readonly member pool.
 *
 *  Available on all
 *
 *  \param[in] data Trace Data structure passed to trace callback
 *  \return Identifier
 */
M_API size_t M_sql_trace_get_conn_id(const M_sql_trace_data_t *data);


/*! Retreive the user-supplied query being executed.
 *
 *  Only available on:
 *    - #M_SQL_TRACE_EXECUTE_START
 *    - #M_SQL_TRACE_EXECUTE_FINISH
 *    - #M_SQL_TRACE_FETCH_START
 *    - #M_SQL_TRACE_FETCH_FINISH
 *    - #M_SQL_TRACE_TRANFAIL
 *
 *  \param[in] data Trace Data structure passed to trace callback
 *  \return User-supplied query.
 */
M_API const char *M_sql_trace_get_query_user(const M_sql_trace_data_t *data);


/*! Retrieve string for prepared query (rewritten by driver) that has been
 *  executed by the server.
 *
 * Only available on:
 *    - #M_SQL_TRACE_EXECUTE_FINISH
 *    - #M_SQL_TRACE_FETCH_START
 *    - #M_SQL_TRACE_FETCH_FINISH
 *    - #M_SQL_TRACE_TRANFAIL
 *
 *  \param[in] data Trace Data structure passed to trace callback
 *  \return Rewritten user-supplied query.
 */
M_API const char *M_sql_trace_get_query_prepared(const M_sql_trace_data_t *data);


/*! Retrieve the number of request columns bound to the query
 *
 *  Only available on:
 *    - #M_SQL_TRACE_EXECUTE_START
 *    - #M_SQL_TRACE_EXECUTE_FINISH
 *    - #M_SQL_TRACE_FETCH_START
 *    - #M_SQL_TRACE_FETCH_FINISH
 *    - #M_SQL_TRACE_TRANFAIL
 *
 *  \param[in] data Trace Data structure passed to trace callback
 *  \return Number of columns bound to the query by the caller.
 */
M_API size_t M_sql_trace_get_bind_cols(const M_sql_trace_data_t *data);


/*! Retrieve the number of request rows bound to the query
 *
 *  Only available on:
 *    - #M_SQL_TRACE_EXECUTE_START
 *    - #M_SQL_TRACE_EXECUTE_FINISH
 *    - #M_SQL_TRACE_FETCH_START
 *    - #M_SQL_TRACE_FETCH_FINISH
 *    - #M_SQL_TRACE_TRANFAIL
 *
 *  \param[in] data Trace Data structure passed to trace callback
 *  \return Number of rows bound to the query by the caller.
 */
M_API size_t M_sql_trace_get_bind_rows(const M_sql_trace_data_t *data);


/*! Retrieve whether or not the query potentially has result data.
 *
 *  If the query has result data, and this is a #M_SQL_TRACE_EXECUTE_FINISH,
 *  then you know for sure #M_SQL_TRACE_FETCH_START/#M_SQL_TRACE_FETCH_FINISH
 *  will also be called later.
 *
 *  Only available on:
 *    - #M_SQL_TRACE_EXECUTE_FINISH
 *    - #M_SQL_TRACE_FETCH_START
 *    - #M_SQL_TRACE_FETCH_FINISH
 *
 *  \param[in] data Trace Data structure passed to trace callback
 *  \return M_TRUE if the query could have result data, M_FALSE otherwise.
 */
M_API M_bool M_sql_trace_get_has_result_rows(const M_sql_trace_data_t *data);


/*! Retrieve the number of rows affected by a query.
 *
 *  This mostly applies to INSERT/UPDATE/DELETE type queries.
 *
 *  Only available on:
 *    - #M_SQL_TRACE_EXECUTE_FINISH
 *
 *  \param[in] data Trace Data structure passed to trace callback
 *  \return Count of affected rows.
 */
M_API size_t M_sql_trace_get_affected_rows(const M_sql_trace_data_t *data);


/*! Retrieve the total number of rows fetched from the server.
 *
 *  Only available on:
 *    - #M_SQL_TRACE_FETCH_FINISH
 *
 *  \param[in] data Trace Data structure passed to trace callback
 *  \return Count of retrieved rows.
 */
M_API size_t M_sql_trace_get_result_row_count(const M_sql_trace_data_t *data);


/*! @} */

__END_DECLS

#endif /* __M_SQL_TRACE_H__ */
