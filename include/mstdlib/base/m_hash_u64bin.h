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

#ifndef __M_HASH_U64BIN_H__
#define __M_HASH_U64BIN_H__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <mstdlib/base/m_defs.h>
#include <mstdlib/base/m_types.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

__BEGIN_DECLS

/*! \addtogroup m_hash_u64bin Hashtable - uint64/Binary
 *  \ingroup m_hashtable
 * 
 * Hashtable, meant for storing uint64 keys and binary data values.
 *
 * References to the data will always be read-only.
 * All keys and values will be duplicated by the hashtable.
 *
 * @{
 */

struct M_hash_u64bin;
/* Currently a direct map to M_hashtable private opaque type,
 * simply using casting to prevent the 'wrap' overhead of mallocing when it
 * is not necessary.
 */
typedef struct M_hash_u64bin M_hash_u64bin_t;

struct M_hash_u64bin_enum;
/* Used for enumerating a M_hash_u64bin. */
typedef struct M_hash_u64bin_enum M_hash_u64bin_enum_t;


/*! Flags for controlling the behavior of the hashtable. */
typedef enum {
	M_HASH_U64BIN_NONE          = 0,      /*!< Case sensitive single value (new values replace). */
	M_HASH_U64BIN_KEYS_ORDERED  = 1 << 0, /*!< Keys should be ordered. Default is insertion order unless the
	                                           sorted option is specified. */
	M_HASH_U64BIN_KEYS_SORTASC  = 1 << 1, /*!< When the keys are ordered sort them using the key_equality function. */
	M_HASH_U64BIN_KEYS_SORTDESC = 1 << 2, /*!< When the keys are ordered sort them using the key_equality function. */
	M_HASH_U64BIN_MULTI_VALUE   = 1 << 3, /*!< Allow keys to contain multiple values.
	                                           Sorted in insertion order another sorting is specified. */
	M_HASH_U64BIN_MULTI_GETLAST = 1 << 4  /*!< When using get and get_direct function get the last value from the list
	                                           when allowing multiple values. The default is to get the first value. */
} M_hash_u64bin_flags_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Create a new hashtable.
 *
 * The hashtable will pre-allocate an array of buckets based on the rounded up size specified. Any hash collisions
 * will result in those collisions being chained together via a linked list. The hashtable will auto-expand by a
 * power of 2 when the fill percentage specified is reached. All key entries are compared in a case-insensitive
 * fashion, and are duplicated internally. Values are duplicated. Case is preserved for both keys and values.
 *
 * \param[in] size     Size of the hash table. If not specified as a power of 2, will
 *                     be rounded up to the nearest power of 2.
 * \param[in] fillpct  The maximum fill percentage before the hash table is expanded. If
 *                     0 is specified, the hashtable will never expand, otherwise the
 *                     value must be between 1 and 99 (recommended: 75).
 * \param[in] flags    M_hash_u64bin_flags_t flags for modifying behavior.
 *
 * \return Allocated hashtable.
 *
 * \see M_hash_u64bin_destroy
 */
M_API M_hash_u64bin_t *M_hash_u64bin_create(size_t size, M_uint8 fillpct, M_uint32 flags) M_MALLOC_ALIASED;


/*! Destroy the hashtable.
 *
 * \param[in] h Hashtable to destroy
 */
M_API void M_hash_u64bin_destroy(M_hash_u64bin_t *h) M_FREE(1);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Insert an entry into the hashtable.
 *
 * \param[in,out] h         Hashtable being referenced.
 * \param[in]     key       Key to insert.
 * \param[in]     value     Value to insert. Value will be duplicated. May be NULL.
 * \param[in]     value_len Size of the value being placed into the hash table.
 *
 * \return M_TRUE on success, or M_FALSE on failure.
 */
M_API M_bool M_hash_u64bin_insert(M_hash_u64bin_t *h, M_uint64 key, const M_uint8 *value, size_t value_len);


/*! Remove an entry from the hashtable.
 *
 * \param[in,out] h   Hashtable being referenced.
 * \param[in]     key Key to remove from the hashtable. 
 *
 * \return M_TRUE on success, or M_FALSE if key does not exist.
 */
M_API M_bool M_hash_u64bin_remove(M_hash_u64bin_t *h, M_uint64 key);


/*! Retrieve the value for a key from the hashtable.
 *
 * \param[in]  h         Hashtable being referenced.
 * \param[in]  key       Key for value to retrieve from hashtable.
 * \param[out] value     Pointer to value stored in the hashtable. Optional, pass NULL if not needed.
 * \param[out] value_len Size of the value. Optional, pass NULL if not needed.
 *
 * \return M_TRUE if value retrieved, M_FALSE if key does not exist.
 */
M_API M_bool M_hash_u64bin_get(const M_hash_u64bin_t *h, M_uint64 key, const M_uint8 **value, size_t *value_len);

/*! Retrieve the value for a key from the hashtable, and return it directly as the return value.
 *
 * This cannot be used if you need to differentiate between a key that doesn't exist vs a key with a NULL value.
 *
 * \param[in]  h         Hashtable being referenced.
 * \param[in]  key       Key for value to retrieve from the hashtable.
 * \param[out] value_len Size of the value. Optional, pass NULL if not needed.
 *
 * \return NULL if key doesn't exist, otherwise the value.
 */
M_API const M_uint8 *M_hash_u64bin_get_direct(const M_hash_u64bin_t *h, M_uint64 key, size_t *value_len);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Get the number of values for a given key.
 *
 * \param[in]  h   Hashtable being referenced.
 * \param[in]  key Key for value to retrieve. 
 * \param[out] len The number of values.
 *
 * \return M_TRUE if length is retrieved, M_FALSE if key does not exist.
 */
M_API M_bool M_hash_u64bin_multi_len(const M_hash_u64bin_t *h, M_uint64 key, size_t *len);


/*! Retrieve the value for a key from the given index when supporting muli-values.
 *
 * \param[in]  h         Hashtable being referenced.
 * \param[in]  key       Key for value to retrieve.
 * \param[in]  idx       The index the value resides at.
 * \param[out] value     Pointer to value stored. Optional, pass NULL if not needed.
 * \param[out] value_len Size of the value. Optional, pass NULL if not needed.
 *
 * \return M_TRUE if value retrieved, M_FALSE if key does not exist.
 */
M_API M_bool M_hash_u64bin_multi_get(const M_hash_u64bin_t *h, M_uint64 key, size_t idx, const M_uint8 **value, size_t *value_len);


/*! Retrieve the value for a key from the given index when supporting muli-values.
 *
 * \param[in]  h         Hashtable being referenced.
 * \param[in]  key       Key for value to retrieve.
 * \param[in]  idx       The index the value resides at.
 * \param[out] value_len Size of the value. Optional, pass NULL if not needed.
 *
 * \return M_TRUE if value retrieved, M_FALSE if key does not exist.
 */
M_API const M_uint8 *M_hash_u64bin_multi_get_direct(const M_hash_u64bin_t *h, M_uint64 key, size_t idx, size_t *value_len);


/*! Remove a value from the hashtable when supporting muli-values.
 *
 * If all values have been removed then the key will be removed.
 *
 * \param[in,out] h   Hashtable being referenced.
 * \param[in]     key Key for value to retrieve.
 * \param[in]     idx The index the value resides at.
 *
 * \return M_TRUE if the value was removed, M_FALSE if key does not exist.
 */
M_API M_bool M_hash_u64bin_multi_remove(M_hash_u64bin_t *h, M_uint64 key, size_t idx);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Retrieve the current size (number of buckets/slots, not necessarily used).
 *
 * \param[in] h Hashtable being referenced.
 *
 * \return Size of the hashtable.
 */
M_API M_uint32 M_hash_u64bin_size(const M_hash_u64bin_t *h);


/*! Retrieve the number of collisions for hashtable entries that has occurred since creation.
 *
 * \param[in] h Hashtable being referenced.
 *
 * \return Number of collisions.
 */
M_API size_t M_hash_u64bin_num_collisions(const M_hash_u64bin_t *h);


/*! Retrieve the number of expansions/rehashes since creation.
 *
 * \param[in] h Hashtable being referenced.
 *
 * \return number of expansions/rehashes.
 */
M_API size_t M_hash_u64bin_num_expansions(const M_hash_u64bin_t *h);


/*! Retrieve the number of entries in the hashtable.
 *
 * This is the number of keys stored.
 *
 * \param[in] h Hashtable being referenced.
 *
 * \return number of entries in the hashtable.
 */
M_API size_t M_hash_u64bin_num_keys(const M_hash_u64bin_t *h);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Start an enumeration of the keys within the hashtable.
 *
 * \param[in] h         Hashtable being referenced.
 * \param[out] hashenum Outputs an initialized state variable for starting an enumeration.
 *
 * \return Number of values in the hashtable. 
 *
 * \see M_hash_u64bin_enumerate_free
 */
M_API size_t M_hash_u64bin_enumerate(const M_hash_u64bin_t *h, M_hash_u64bin_enum_t **hashenum);


/*! Retrieve the next item from a hashtable enumeration.
 *
 * \param[in]     h         Hashtable being referenced.
 * \param[in,out] hashenum  State variable for tracking the enumeration process.
 * \param[out]    key       Value of next enumerated key. Optional, may be NULL
 * \param[out]    value     Value of next enumerated value. Optional, may be NULL
 * \param[out]    value_len Size of next enumerated value.
 *
 * \return M_TRUE if enumeration succeeded, M_FALSE if no more keys
 */
M_API M_bool M_hash_u64bin_enumerate_next(const M_hash_u64bin_t *h, M_hash_u64bin_enum_t *hashenum, M_uint64 *key, const M_uint8 **value, size_t *value_len);


/*! Destroy an enumeration state.
 *
 * \param[in] hashenum Enumeration to destroy.
 */
M_API void M_hash_u64bin_enumerate_free(M_hash_u64bin_enum_t *hashenum);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*! Merge two hashtables together.
 *
 * The second (src) hashtable will be destroyed automatically upon completion of this function. Any key/value
 * pointers for the hashtable will be directly copied over to the destination hashtable, they will not be
 * duplicated. Any keys which exist in 'dest' that also exist in 'src' will be overwritten by the 'src' value.
 *
 * \param[in,out] dest Pointer by reference to the hashtable receiving the key/value pairs.
 *                     if dest is NULL, the src address will simply be copied to dest.
 * \param[in,out] src  Pointer to the hashtable giving up its key/value pairs.
 */
M_API void M_hash_u64bin_merge(M_hash_u64bin_t **dest, M_hash_u64bin_t *src) M_FREE(2);

/*! Duplicate an existing hashtable.
 *
 * Copying all keys and values.
 *
 * \param[in] h Hashtable to be copied.
 *
 * \return Duplicated hashtable.
 */
M_API M_hash_u64bin_t *M_hash_u64bin_duplicate(const M_hash_u64bin_t *h) M_MALLOC;

/*! @} */

__END_DECLS

#endif /* __M_HASH_U64BIN_H__ */
