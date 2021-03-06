#ifndef MSTDLIB_LOG_H
#define MSTDLIB_LOG_H

#include <mstdlib/mstdlib.h>
#include <mstdlib/mstdlib_thread.h>
#include <mstdlib/mstdlib_io.h>

/*! \defgroup m_log Logging Subsystem
 * 
 * \code{.c}
 * #include <mstdlib/mstdlib_log.h>
 * \endcode
 *
 * Example:
 *
 * \code{.c}
 *     #include <mstdlib/mstdlib_log.h>
 *     
 *     #define STREAM_QUEUE_SIZE (1500*1000) // 1.5 MB - small enough to cause a few drops, since all threads combined will
 *                                           // output about 1.7 MB of data
 *     #define FILE_QUEUE_SIZE   (1500*1000) // 1.5 MB
 *     #define SYSLOG_QUEUE_SIZE (10*1000)   // 10 KB
 *     #define CRIT_FREQ  (500)      // critical messages are sent every time message count hits a multiple of this number
 *     #define NUM_MSGS   (10*1000)  // total number of messages to send
 *     
 *     //#define FILE_DO_ARCHIVE //Uncomment to test file archiving.
 *     //#define ADD_LINE_END    //Uncomment to add embedded line endings to log messages (tests multiline functionality).
 *     
 *     //#define DO_MEMBUF //Uncomment to test membuf.
 *     
 *     
 *     #ifdef FILE_DO_ARCHIVE
 *         static const char *archive_cmd = "bzip2 -f";
 *         static const char *archive_ext = ".bz2";
 *     #else
 *         static const char *archive_cmd = NULL;
 *         static const char *archive_ext = NULL;
 *     #endif
 *     
 *     static const M_bool flush_on_destroy = M_TRUE;
 *     
 *     typedef enum {
 *         TAG_1  = 1 << 0,
 *         TAG_2  = 1 << 1,
 *         TAG_3  = 1 << 2,
 *         CRIT_1 = 1 << 3,
 *         CRIT_2 = 1 << 4,
 *         CRIT_3 = 1 << 5
 *     } tags_t;
 *     
 *     static const char *tag_to_str(tags_t tag)
 *     {
 *         switch (tag) {
 *             case TAG_1:  return "tag 1";
 *             case TAG_2:  return "tag 2";
 *             case TAG_3:  return "tag 3";
 *             case CRIT_1: return "CRITICAL 1";
 *             case CRIT_2: return "CRITICAL 2";
 *             case CRIT_3: return "CRITICAL 3";
 *         }
 *         return "unknown tag";
 *     }
 *     
 *     
 *     typedef struct {
 *         M_log_t    *log;
 *         const char *msg;
 *         tags_t      tag;
 *         tags_t      crit_tag;
 *     } tdata_t;
 *     
 *     static void set(tdata_t *td, M_log_t *log, const char *msg, tags_t tag, tags_t crit_tag)
 *     {
 *         td->log      = log;
 *         td->msg      = msg;
 *         td->tag      = tag;
 *         td->crit_tag = crit_tag;
 *     }
 *     
 *     
 *     static void prefix_cb(M_buf_t *buf, M_uint64 tag, void *prefix_thunk, void *msg_thunk)
 *     {
 *         (void)prefix_thunk;
 *         (void)msg_thunk;
 *         M_bprintf(buf, ": [%s]\t", tag_to_str((tags_t)tag));
 *     }
 *     
 *     
 *     static void *test_thread(void *arg)
 *     {
 *         tdata_t  *td = arg;
 *         M_uint64  i;
 *         M_uint64  crit;
 *     
 *         crit = CRIT_FREQ;
 *         for (i=0; i<NUM_MSGS; i++) {
 *             M_log_error_t err;
 *             tags_t        tag;
 *     
 *             //M_thread_sleep(10*1000); //Add 10 ms delay
 *     
 *             if (crit >= CRIT_FREQ) {
 *                 tag = td->crit_tag;
 *                 crit = 1;
 *             } else {
 *                 tag = td->tag;
 *                 ++crit;
 *             }
 *     
 *             err = M_log_printf(td->log, tag, NULL,
 *     #ifdef ADD_LINE_END
 *                 "%s --\n   %llu",
 *     #else
 *                 "%s -- %llu",
 *     #endif
 *                 td->msg, i);
 *     
 *             if (err != M_LOG_SUCCESS) {
 *                 M_fprintf(stderr, "Error writing log message: %s\n", M_log_err_to_str(err));
 *             }
 *         }
 *     
 *         return NULL;
 *     }
 *     
 *     
 *     int main(int argc, char *argv[])
 *     {
 *         M_log_t         *log;
 *         M_log_error_t    res;
 *         M_log_module_t  *mod_stream;
 *         M_log_module_t  *mod_syslog;
 *         M_log_module_t  *mod_file;
 *     #ifdef DO_MEMBUF
 *         M_log_module_t  *mod_membuf;
 *         M_fs_file_t     *membuf_out;
 *         M_buf_t         *membuf;
 *     #endif
 *         M_thread_attr_t *attr;
 *         M_threadid_t     t1, t2, t3;
 *         tdata_t          data1, data2, data3;
 *     
 *         (void)argc;
 *         (void)argv;
 *     
 *         // Set up the log.
 *         log = M_log_create(M_LOG_LINE_END_NATIVE, flush_on_destroy, NULL);
 *         M_log_set_time_format(log, "[%a %D %Y %H:%m:%s:%u %z]");
 *         M_log_set_tag_name(log, TAG_1,  "tag_1_name");
 *         M_log_set_tag_name(log, TAG_2,  "tag_2_name");
 *         M_log_set_tag_name(log, TAG_3,  "tag_3_name");
 *         M_log_set_tag_name(log, CRIT_1, "crit_1_name");
 *         M_log_set_tag_name(log, CRIT_2, "crit_2_name");
 *         M_log_set_tag_name(log, CRIT_3, "crit_3_name");
 *     
 *         // Set up the stream module.
 *         res = M_log_module_add_stream(log, M_STREAM_STDOUT, STREAM_QUEUE_SIZE, &mod_stream);
 *         //res = M_log_module_add_nslog(log, STREAM_QUEUE_SIZE, &mod_stream);
 *         if (res != M_LOG_SUCCESS) {
 *             M_fprintf(stderr, "Could not add stream module: %s\n", M_log_err_to_str(res));
 *         } else {
 *             M_log_module_set_accepted_tags(log, mod_stream, TAG_1 | TAG_2 | TAG_3 | CRIT_1 | CRIT_2 | CRIT_3);
 *     
 *             M_log_module_set_prefix(log, mod_stream, prefix_cb, NULL, NULL);
 *         }
 *     
 *         // Set up the file module.
 *         res = M_log_module_add_file(log, "~/Tmp/logs/testing.log", 15, 150000, 0, FILE_QUEUE_SIZE, archive_cmd, archive_ext, &mod_file);
 *         if (res != M_LOG_SUCCESS) {
 *             M_fprintf(stderr, "Could not add file module: %s\n", M_log_err_to_str(res));
 *         } else {
 *             M_log_module_set_accepted_tags(log, mod_file, TAG_1 | TAG_2 | TAG_3 | CRIT_1 | CRIT_2 | CRIT_3);
 *     
 *             M_log_module_set_prefix(log, mod_file, prefix_cb, NULL, NULL);
 *         }
 *     
 *     
 *         // Set up the syslog module.
 *         res = M_log_module_add_syslog(log, "log_example", M_SYSLOG_FACILITY_LOCAL5, SYSLOG_QUEUE_SIZE, &mod_syslog);
 *     
 *         if (res != M_LOG_SUCCESS) {
 *             M_fprintf(stderr, "Could not add syslog module: %s\n", M_log_err_to_str(res));
 *         } else {
 *             M_log_module_set_accepted_tags(log, mod_syslog, CRIT_1 | CRIT_2 | CRIT_3);
 *     
 *             M_log_module_set_prefix(log, mod_syslog, prefix_cb, NULL, NULL);
 *     
 *             M_log_module_syslog_set_tag_priority(log, mod_syslog, CRIT_1 | CRIT_2, M_SYSLOG_WARNING);
 *     
 *             M_log_module_syslog_set_tag_priority(log, mod_syslog, CRIT_3, M_SYSLOG_CRIT);
 *         }
 *     
 *         // Do an emergency call (just to see if it really works).
 *         M_log_emergency(log, "RED ALERT! WOOT WOOT WOOT\r\n");
 *     
 *         // Launch three test threads that spam the logger with a bunch of messages.
 *         set(&data1, log, "data1", TAG_1, CRIT_1);
 *         set(&data2, log, "data2", TAG_2, CRIT_2);
 *         set(&data3, log, "data3", TAG_3, CRIT_3);
 *     
 *         attr = M_thread_attr_create();
 *         M_thread_attr_set_create_joinable(attr, M_TRUE);
 *     
 *         t1 = M_thread_create(attr, test_thread, &data1);
 *         t2 = M_thread_create(attr, test_thread, &data2);
 *         t3 = M_thread_create(attr, test_thread, &data3);
 *     
 *         M_thread_attr_destroy(attr);
 *     
 *     #ifdef DO_MEMBUF
 *         // Wait a little before we add the membuf module.
 *         M_thread_sleep(50*1000); // 50 ms
 *     
 *         res = M_log_module_add_membuf(log, 400*1000, 60*1000, NULL, NULL, &mod_membuf);
 *         if (res != M_LOG_SUCCESS) {
 *             M_fprintf(stderr, "Could not add membuf module: %s\n", M_log_err_to_str(res));
 *         } else {
 *             M_log_module_set_accepted_tags(log, mod_membuf, TAG_1 | CRIT_2);
 *     
 *             M_log_module_set_prefix(log, mod_membuf, prefix_cb, NULL, NULL);
 *         }
 *     #endif
 *     
 *         // Do a suspend/resume operation.
 *         M_log_suspend(log);
 *         M_thread_sleep(500); // sleep for 0.5 ms
 *         M_log_resume(log, NULL);
 *     
 *         // Wait until all three threads are done spamming.
 *         M_thread_join(t1, NULL);
 *         M_thread_join(t2, NULL);
 *         M_thread_join(t3, NULL);
 *     
 *     #ifdef DO_MEMBUF
 *         // Pull contents of membuf, dump to file.
 *         M_log_module_take_membuf(log, mod_membuf, &membuf);
 *     
 *         M_fs_file_open(&membuf_out, "~/Tmp/logs/log_membuf.txt", 0, M_FS_FILE_MODE_WRITE | M_FS_FILE_MODE_OVERWRITE, NULL);
 *         M_fs_file_write(membuf_out, (const unsigned char *)M_buf_peek(membuf), M_buf_len(membuf),
 *             NULL, M_FS_FILE_RW_NORMAL);
 *         M_fs_file_close(membuf_out);
 *     
 *         M_buf_cancel(membuf);
 *     #endif
 *     
 *         // Destroy the log. If internal workers are still processing messages, this will wait until they finish outputting
 *         // their internal message queues and exit. This ensures that we don't see any memory leaks at process exit.
 *         //
 *         // Wait up to five seconds for threads to finish writing.
 *         M_log_destroy_blocking(log, 5000);
 *     
 *         return EXIT_SUCCESS;
 *     }
 * \endcode
 */

#include <mstdlib/log/m_async_writer.h>
#include <mstdlib/log/m_log.h>

#endif /* MSTDLIB_LOG_H */

