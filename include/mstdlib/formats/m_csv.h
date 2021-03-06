/* The MIT License (MIT)
 * 
 * Copyright (c) 2015 Main Street Softworks, Inc.
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

#ifndef __M_CSV_H__
#define __M_CSV_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_csv CSV
 *  \ingroup m_formats
 * CSV Parser.
 *
 * RFC 4180 compliant CSV parser.
 *
 * The first row in the CSV is assumed to be the header. If there is no
 * header the *raw* functions should be used to reterive data. If there
 * is a header the non-raw functions should be used. These functions
 * take into account the header when indexing rows automatically. The
 * first row after the header is index 0.
 *
 * Example:
 *
 * \code{.c}
 *     const char *data = "header1,header1\ncell1,cell2"
 *     M_csv_t    *csv;
 *     const char *const_temp;
 *
 *     csv        = M_csv_parse(data, M_str_len(data), ',', '"', M_CSV_FLAG_NONE);
 *     const_temp = M_csv_get_header(csv, 0);
 *     M_printf("header='%s'\n", const_temp);
 *
 *     const_temp = M_csv_get_cellbynum(csv, 0, 1);
 *     M_printf("cell='%s'\n", const_temp);
 *
 *     M_csv_destroy(csv);
 * \endcode
 *
 * Example output:
 *
 * \code
 *     header='header1'
 *     cell='cell2'
 * \endcode
 *
 * @{
 */

struct M_csv;
typedef struct M_csv M_csv_t;

/*! Flags controlling parse behavior */
enum M_CSV_FLAGS {
	M_CSV_FLAG_NONE            = 0,     /*!< No Flags */
	M_CSV_FLAG_TRIM_WHITESPACE = 1 << 0 /*!< If a cell is not quoted, trim leading and trailing whitespace */
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Parse a string into a CSV object.
 *
 * \param[in] data  The data to parse.
 * \param[in] len   The length of the data to parse.
 * \param[in] delim CSV delimiter character. Typically comma (",").
 * \param[in] quote CSV quote character. Typically double quote (""").
 * \param[in] flags Flags controlling parse behavior.
 *
 * \return CSV object.
 * 
 * \see M_csv_destroy
 */
M_API M_csv_t *M_csv_parse(const char *data, size_t len, char delim, char quote, M_uint32 flags) M_MALLOC;


/*! Parse a string into a CSV object.
 *
 * This will take ownership of the data passed in. The data must be valid for the life of the
 * returned CSV object and will be destroyed by the CSV object when the CSV object is destroyed.
 *
 * \param[in] data  The string to parse.
 * \param[in] len   The length of the data to parse.
 * \param[in] delim CSV delimiter character. Typically comma (",").
 * \param[in] quote CSV quote character. Typically double quote (""").
 * \param[in] flags Flags controlling parse behavior.
 *
 * \return CSV object.
 * 
 * \see M_csv_destroy
 */
M_API M_csv_t *M_csv_parse_inplace(char *data, size_t len, char delim, char quote, M_uint32 flags) M_MALLOC_ALIASED;


/*! Destory a CSV object.
 *
 * \param[in] csv The csv.
 */
M_API void M_csv_destroy(M_csv_t *csv) M_FREE(1);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Raw getters if no headers used */

/*! Get the raw number of csv rows.
 *
 * This should be used when the CSV data does not contain a header.
 * This count will include the header as a row in the count.
 *
 * \param[in] csv The csv.
 *
 * \return The number of rows including the header as a row.
 *
 * \see M_csv_get_numrows
 */
M_API size_t M_csv_raw_num_rows(const M_csv_t *csv);


/*! Get the raw number of csv columns.
 *
 * This should be used when the CSV data does not contain a header.
 *
 * \param[in] csv The csv.
 *
 * \return The number of columns.
 *
 * \see M_csv_get_numcols
 */
M_API size_t M_csv_raw_num_cols(const M_csv_t *csv);


/*! Get the cell at the given position.
 *
 * This should be used when the CSV data does not contain a header.
 * This assumes that the first row is data (not the header).
 *
 * \param[in] csv The csv.
 * \param[in] row The row. Indexed from 0 where 0 is the header (if there is a header).
 * \param[in] col The column. Indexed from 0.
 *
 * \return The csv data at the position or NULL if the position if invalid.
 *
 * \see M_csv_get_cellbynum
 */
M_API const char *M_csv_raw_cell(const M_csv_t *csv, size_t row, size_t col);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Getters if headers used (default) */

/*! Get the number of csv rows.
 *
 * This should be used when the CSV data contains a header.
 * This count will not include the header as a row in the count.
 *
 * \param[in] csv The csv.
 *
 * \return The number of rows excluding the header as a row.
 *
 * \see M_csv_raw_num_rows
 */
M_API size_t M_csv_get_numrows(const M_csv_t *csv);


/*! Get the raw number of csv columns.
 *
 * This should be used when the CSV data contains a header.
 *
 * \param[in] csv The csv.
 *
 * \return The number of columns.
 *
 * \see M_csv_raw_num_cols
 */
M_API size_t M_csv_get_numcols(const M_csv_t *csv);


/*! Get the cell at the given position.
 *
 * This should be used when the CSV data contains a header.
 * This assumes that the first row is a header (not data).
 *
 * \param[in] csv The csv.
 * \param[in] row The row. Indexed from 0 where 0 is the first row after the header.
 * \param[in] col The column. Indexed from 0.
 *
 * \return The csv data at the position or NULL if the position if invalid.
 *
 * \see M_csv_raw_cell
 */
M_API const char *M_csv_get_cellbynum(const M_csv_t *csv, size_t row, size_t col);


/*! Get the header for a given column
 *
 * This should be used when the CSV data contains a header.
 * This assumes that the first row is a header (not data).
 *
 * \param[in] csv The csv.
 * \param[in] col The column. Indexed from 0.
 *
 * \return The header for the given column.
 */
M_API const char *M_csv_get_header(const M_csv_t *csv, size_t col);


/*! Get the cell at the for the given header.
 *
 * This should be used when the CSV data contains a header.
 * This assumes that the first row is a header (not data).
 *
 * \param[in] csv     The csv.
 * \param[in] row     The row. Indexed from 0 where 0 is the first row after the header.
 * \param[in] colname The column name to get the data from.
 *
 * \return The csv data at the position or NULL if the position if invalid.
 */
M_API const char *M_csv_get_cell(const M_csv_t *csv, size_t row, const char *colname);


/*! Get the column number for a given column (header) name.
 *
 * This should be used when the CSV data contains a header.
 * This assumes that the first row is a header (not data).
 *
 * \param[in] csv     The csv.
 * \param[in] colname The column name to get the data from.
 *
 * \return Column number for the given name on success. Otherwise -1.
 */
M_API ssize_t M_csv_get_cell_num(const M_csv_t *csv, const char *colname); 

/*! @} */

__END_DECLS

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#endif /* __M_CSV_H__ */
