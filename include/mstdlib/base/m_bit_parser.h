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

#ifndef __M_BIT_PARSER_H__
#define __M_BIT_PARSER_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>
#include <mstdlib/base/m_buf.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_bit_parser Bitwise Data Parser
 *  \ingroup mstdlib_base
 *
 * Buffer based data parser that reads data per-bit, instead of per-byte.
 *
 * Example (creating a parser, reading some bits):
 *
 * \code{.c}
 *     M_bit_parser_t *bparser;
 *	   const M_uint8   bytes[] = {0x70, 0x3F};
 *     size_t          nbits   = 12;
 *
 *     M_uint8         bit;
 *     char           *str;
 *     size_t          nbits_in_range;
 *
 *     bparser = M_bit_parser_create_const(bytes, nbits);
 *
 *     M_bit_parser_read_bit(bparser, &bit); // bit == 0
 *
 *     str == M_bit_parser_read_strdup(bparser, 5); // str == "11100"
 *     M_free(str);
 *
 *     M_bit_parser_read_range(bparser, &bit, &nbits_in_range, M_bit_parser_len(bparser));
 *     // bit == 0
 *     // nbits_in_range == 6
 *
 *     M_bit_parser_destroy(bparser);
 * \endcode
 *
 * @{
 */

struct M_bit_parser;
typedef struct M_bit_parser M_bit_parser_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create a bit parser over the given data (copies input data).
 *
 * The parser will copy the data internally, so after this function is called, the caller's copy of the
 * data may be copied or freed without affecting the parser.
 *
 * If your data isn't going to change, you can use M_bit_parser_create_const() instead to avoid duplicating
 * the data.
 *
 * \param[in] bytes data to parse bitwise
 * \param[in] nbits number of bits in data
 * \return          a new parser object over the given data
 *
 * \see M_bit_parser_reset
 * \see M_bit_parser_destroy
 */
M_API M_bit_parser_t *M_bit_parser_create(const void *bytes, size_t nbits) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Create a bit parser over the given data (assumes given data won't change).
 *
 * Assumes the given data pointer won't be modified until after you're done with the parser.
 *
 * \warning
 * Violating this assumption can lead to undefined behavior (including program crashes).
 *
 * \param[in] bytes data to parse bitwise
 * \param[in] nbits number of bits in data
 * \return          a new parser object over the given data
 *
 * \see M_bit_parser_reset
 * \see M_bit_parser_destroy
 */
M_API M_bit_parser_t *M_bit_parser_create_const(const void *bytes, size_t nbits) M_WARN_UNUSED_RESULT M_MALLOC;


/*! Reset parser to use new data (copies input data).
 *
 * Parser state (including any mark) is reset to initial values. Any data that was in the parser before this
 * call is dropped.
 *
 * The new data is copied into the parser, so the caller's copy of the data may be modified or freed
 * after this call without affecting the parser.
 *
 * \param[in] bparser bit parser object
 * \param[in] bytes   bytes to read from
 * \param[in] nbits   number of bits to read out of input bytes
 */
M_API void M_bit_parser_reset(M_bit_parser_t *bparser, const void *bytes, size_t nbits);


/*! Destroy the bit parser object.
 *
 * \param[in] bparser bit parser object
 */
M_API void M_bit_parser_destroy(M_bit_parser_t *bparser) M_FREE(1);


/*! Returns the number of bits left to read in the parser.
 *
 * \param[in] bparser bit parser object
 * \return            number of bits left that haven't been read yet
 */
M_API size_t M_bit_parser_len(M_bit_parser_t *bparser);


/*! Retrieve the current position of the parser (number of bits read).
 *
 * \param[in] bparser bit parser object
 * \return            current parser position, relative to start of data (in bits)
 */
M_API size_t M_bit_parser_current_offset(M_bit_parser_t *bparser);


/*! Rewind parser (and any mark) back to start of data.
 *
 * \param[in] bparser bit parser object
 *
 * \see M_bit_parser_mark
 * \see M_bit_parser_mark_rewind
 */
M_API void M_bit_parser_rewind_to_start(M_bit_parser_t *bparser);


/*! Mark the current position in the stream, so we can return to it later.
 *
 * \param[in] bparser bit parser object
 *
 * \see M_bit_parser_mark_len
 * \see M_bit_parser_mark_rewind
 */
M_API void M_bit_parser_mark(M_bit_parser_t *bparser);


/*! Return the number of bits from a mark to the current parser position.
 *
 * If no mark has been set, returns the number of bits from the start of the data.
 *
 * For example, if I set a mark, read 3 bits, and then call this function, it'll return 3.
 *
 * \param[in] bparser bit parser object
 * \return            number of bits read/consumed from the point where we last marked the bitstream
 *
 * \see M_bit_parser_mark
 */
M_API size_t M_bit_parser_mark_len(M_bit_parser_t *bparser);


/*! Rewind parser back to the marked position.
 *
 * This will not clear the mark - you can read and then return to a marked position multiple times.
 *
 * If no mark has been set, this will rewind all the way back to the beginning of the stream.
 *
 * \param[in] bparser bit parser object
 * \return            number of bits we rewound the stream
 *
 * \see M_bit_parser_rewind_to_start
 * \see M_bit_parser_mark
 */
M_API size_t M_bit_parser_mark_rewind(M_bit_parser_t *bparser);


/*! Skip past the given number of bits.
 *
 * \param[in] bparser bit parser object
 * \param[in] nbits   number of bits to consume
 * \return            M_TRUE on success, M_FALSE if not enough bits left
 */
M_API M_bool M_bit_parser_consume(M_bit_parser_t *bparser, size_t nbits);


/*! Read a single bit at the parser's current position without advancing.
 *
 * \param[in]  bparser bit parser object
 * \param[out] bit     0 or 1
 * \return             M_TRUE on success, M_FALSE if there are no bits left to read
 */
M_API M_bool M_bit_parser_peek_bit(M_bit_parser_t *bparser, M_uint8 *bit);


/*! Read a single bit at the parser's current position and advance.
 *
 * \param[in]  bparser bit parser object
 * \param[out] bit     0 or 1
 * \return             M_TRUE on success, M_FALSE if there are no bits left to read
 */
M_API M_bool M_bit_parser_read_bit(M_bit_parser_t *bparser, M_uint8 *bit);


/*! Read multiple bits and add them to the end of the given bit buffer.
 *
 * \param[in]     bparser bit parser to read bits from
 * \param[in,out] bbuf    bit buffer to store bits in
 * \param[in]     nbits   number of bits to read
 * \return                M_TRUE on success, M_FALSE if there aren't enough bits left
 */
M_API M_bool M_bit_parser_read_bit_buf(M_bit_parser_t *bparser, M_bit_buf_t *bbuf, size_t nbits);


/*! Read multiple bits, then return them as a bit string.
 *
 * A bit string is just a list of '0' and '1' characters (e.g., "100101").
 *
 * \warning
 * The caller assumes ownership of returned string, and must free it with M_free().
 *
 * \param[in] bparser bit parser to read bits from
 * \param[in] nbits   number of bits to read
 * \return            bitstring on success, NULL if there aren't enough bits left
 * \see M_free
 */
M_API char *M_bit_parser_read_strdup(M_bit_parser_t *bparser, size_t nbits);


/*! Skip bits until we hit a bit different than the current one.
 *
 * For example, if the parser contains "11100001", calling this function will move the parser's position
 * to the first \a 0.
 *
 * Note that this function will always consume at least one bit, if any bits are left to skip.
 *
 * \param[in]  bparser  bit parser to read bits from
 * \param[in]  max_bits maximum number of bits to skip (if set to zero, no bits will be skipped)
 * \return              M_TRUE if at least one bit was skipped, M_FALSE if no bits are left or \a max_bits was zero
 */
M_API M_bool M_bit_parser_consume_range(M_bit_parser_t *bparser, size_t max_bits);


/*! Read bits until we hit a bit different than the current one.
 *
 * For example, if the parser contain "11100001", calling this function will move the parser's position
 * to the first \a 0, and return \a 1 in \a bit and \a 3 in \a nbits_in_range
 *
 * Note that this function will always read at least one bit, if any bits are left to read.
 *
 * \param[in]  bparser        bit parser to read bits from
 * \param[out] bit            bit value in range we just read (0 or 1)
 * \param[out] nbits_in_range number of bits in range we just read
 * \param[in]  max_bits       maximum number of bits to read (if set to zero, no bits will be read)
 * \return                    M_TRUE if at least one bit was read, M_FALSE if no bits are left or \a max_bits was zero
 */
M_API M_bool M_bit_parser_read_range(M_bit_parser_t *bparser, M_uint8 *bit, size_t *nbits_in_range, size_t max_bits);

/*! @} */

__END_DECLS

#endif /* __M_BIT_PARSER_H__ */
