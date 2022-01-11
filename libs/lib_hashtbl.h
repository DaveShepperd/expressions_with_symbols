/*
    lib_hashtbl.h - a generic hash table symbol table example.
    Copyright (C) 2022 David Shepperd

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef _LIB_HASHTBL_H_
#define _LIB_HASHTBL_H_ (1)

#include <pthread.h>

#ifndef POOL_BLOCK_SIZE
#define POOL_BLOCK_SIZE (128)
#endif
#ifndef n_elts
#define n_elts(x) (int)(sizeof(x)/sizeof((x)[0]))
#endif

/** HashEntry_t - defines the primitive user defined data stored in
 *  the hash table.
 **/
typedef void *HashEntry_t;

/** HashTablePrimitive_t - defines the struct actually stored
 *  in the hash table. User functions should not attempt to
 *  modify anything in this structure.
 **/
typedef struct HashPrimitive_t
{
	struct HashPrimitive_t *next;
	struct HashPrimitive_t *prev;
	HashEntry_t entry;
} HashPrimitive_t;

typedef enum
{
	HashSuccess,		/*! No error */
	HashInvalidParam,	/*! Invalid parameter passed to function */
	HashNoSuchSymbol,	/*! No symbol found */
	HashDuplicateSymbol,/*! Cannot add duplicate symbol */
	HashOutOfMemory,	/*! Ran out of memory */
	HashEndOfTable,		/*! Reached end of symbol table */
	HashNoLock,			/*! pthread lock error. Look at errno for additional error code */
	HashNoUnLock,		/*! pthread unlock error. Look at errno for additional error code */
	HashMaxError
} HashErrors_t;

/** HashMsgSeverity_t - define the severity of messages
 *  that may be emitted by the hash library functions.
 **/
typedef enum
{
	HASH_SEVERITY_INFO,
	HASH_SEVERITY_WARN,
	HASH_SEVERITY_ERROR,
	HASH_SEVERITY_FATAL
} HashMsgSeverity_t;

/** HashCallbacks_t - define the pointers to functions to
 *  callback for various hash lib primitive functions.
 *
 *  @note The memory callbacks are used only to get memory for
 *  	  the hash lib's internal use. Both memAlloc and memFree
 *  	  must contain a pointer or both must be NULL. If set to
 *  	  NULL, default functions using malloc/free will be
 *  	  used. The memArg pointer will be delivered to the
 *  	  memAlloc and memFree functions when called.
 *
 *  @note The msgOut callback is used by the hash lib to emit
 *  	  informational and error messages. If msgOut is set to
 *  	  NULL, a default function using stdout and stderr will
 *  	  be used. The msgArg pointer will be delivered to the
 *  	  msgOut function when it is called.
 *
 *  @note The symCmp callback must be set and the function is
 *  	  to return a -/0/+ value depending on the comparison of
 *  	  its two parameters: aa-bb. The pointer symArg will be
 *  	  delivered to the symCmp function when called.
 *
 *  @note The symHash callback must be set and the function is
 *  	  to return an unsigned int with the result of the hash
 *  	  of the supplied 'entry'. The result must be >= 0 and <
 *  	  hashTableSize. The pointer symArg will be delivered to
 *  	  the symHash function when called.
 **/
typedef struct
{
	void *(*memAlloc)(void *memArg, size_t size);	/*! pointer to memory allocation function */
	void (*memFree)(void *memArg, void *ptr);		/*! pointer to memory free function */
	void *memArg;									/*! argument that will be passed to memAlloc() and memFree() */
	void (*msgOut)(void *msgArg, HashMsgSeverity_t severity, const char *msg);	/*! Message (error and info) output callback */
	void *msgArg;									/*! Argument to pass to msgOut callback */
	unsigned int (*symHash)(void *symArg, int hashTableSize, const HashEntry_t entry);	/*! pointer to function to perform hash */
	int (*symCmp)(void *symArg, const HashEntry_t aa,const HashEntry_t bb);		/*! pointer to function to perform compare */
	void *symArg;									/*! Argument that will be passed to symHash() and symCmp() */
} HashCallbacks_t;

#define HASHTBL_VERBOSE_ERROR (0x01)	/*! set this flag in verbose to emit error messages directly */

/** HashRoot_t - the principal structure containing all the
 *  details of the hash table. With the exception of pUser1 and
 *  pUser2, user code ought not alter any of the entries in this
 *  structure directly.
 **/
typedef struct HashRoot_t
{
	void *pUser1;				/*! spare pointer to use for anything */
	void *pUser2;				/*! spare pointer to use for anything */
	int verbose;				/*! verbose flags */
	pthread_mutex_t lock;		/*! thread locker */
	HashCallbacks_t callbacks;	/*! pointers to various callback functions */
	int hashTableSize;			/*! The size of the hash table */
	int numEntries;				/*! number of active entries in the hash table (table itself + any chains) */
	HashPrimitive_t **hashTable;	/*! pointer to hash table which is array of pointers */
} HashRoot_t;

/** libHashErrorString - Get error string.
 *
 *  At entry:
 *  @param error - error code
 *
 *  At exit:
 *  @return const pointer to error string
 **/
extern const char *libHashErrorString(HashErrors_t error);

/** libHashInit - Inialize the hash functions.
 *
 *  At entry:
 *  @param tableSize - size of hash table. Probably for best
 *  			results, this should be a prime number. If this
 *  			is 0, defaults to 997.
 *  @param callbacks - pointer to list of various callback
 *  				 functions.
 *
 *  At exit:
 *  @return pointer to HashRoot_t struct which holds details of
 *  		the hash table for further access or NULL if the
 *  		init could not be performed for some reason (out of
 *  		memory likely the only error).
 **/
extern HashRoot_t* libHashInit(int tableSize, const HashCallbacks_t *callbacks );

/** libHashDestroy - Free all the memory in the hash table.
 *
 *  At Entry:
 *  @param pTable - Pointer to hash table root.
 *  @param entry_free_fn - optional pointer to function that
 *  					 will be called for each member in the
 *  					 hash table.
 *  @param freeArg - argument passed to entry_free_fn along with
 *  			   the hash entry.
 *
 *  At Exit:
 *  @return 0 on success and all allocated memory will have been
 *  		freed. Else error then look in errno for further
 *  		information about error.
 *
 *  @note If the entry_free_fn parameter is provided it will be
 *  	  called for each member in the hash table with the
 *  	  anticipation it be used to free any memory allocated
 *  	  for that member. Essentially a hashWalk() but each
 *  	  member is expected to be free'd and its entry nulled
 *  	  out.
 **/
extern HashErrors_t libHashDestroy(HashRoot_t *pTable, void (*entry_free_fn)(void *freeArg, HashEntry_t entry), void *freeArg);

/** libHashInsert - insert item into hash table.
 *
 *  At entry:
 *  @param pTable - pointer to hash table root.
 *  @param entry - entry to insert
 *
 *  At exit:
 *  @return 0 if success else error code. Will not insert a
 *  		duplicate entry.
 **/
extern HashErrors_t libHashInsert(HashRoot_t *pTable, const HashEntry_t entry);

/** libHashReplace - replace or insert an item into hash
 *  		table.
 *
 *  At entry:
 *  @param pTable - pointer to hash table root.
 *  @param entry - entry to look for and replace.
 *  @param pExisting - optional pointer to return existing
 *  				entry if any.
 *
 *  At exit:
 *  @return 0 if success else error code.
 *  @note if no existing entry found, entry is inserted and
 *  	  *pPrevious is set to NULL. If existing entry found, it
 *  	  is returned in the place pointed to by pPrevious and
 *  	  the new entry takes its place in the hash table.
 **/
extern HashErrors_t libHashReplace(HashRoot_t *pTable, const HashEntry_t entry, HashEntry_t *pExisting);

/** libHashDelete - delete a matching entry from hash table.
 *
 *  At entry:
 *  @param pTable - pointer to hash table root.
 *  @param entry - entry to look for.
 *  @param pExisting - optional pointer to return existing
 *  				entry if any.
 *
 *  At exit:
 *  @return 0 if success else error if not found.
 **/
extern HashErrors_t libHashDelete(HashRoot_t *pTable, const HashEntry_t entry, HashEntry_t *pExisting);

/** libHashFind - find a matching entry in hash table.
 *
 *  At entry:
 *  @param pTable - pointer to hash table root.
 *  @param entry - pointer to entry to look for.
 *  @param pResult - pointer where to deposit the found entry.
 *  @param alreadyLocked - set non-zero if hash table has
 *  					previously been locked by libHashLock().
 *
 *  At exit:
 *  @return 0 on success else error code.
 *     The variable *pExisting is set to pEntry->value if found
 *     else it is set NULL.
 *
 *  @note If alreadyLocked is 0 the hash table will be locked
 *  	  for the duration of the lookup and delete. If
 *  	  alreadyLocked is set to non-zero, the hash table will
 *  	  not be locked. One would use the alreadyLocked if one
 *  	  planned on making dynamic changes to the found entry
 *  	  without fear of other threads attempting to change
 *  	  anything in the symbol table, particularly the found
 *  	  entry, at the same time. Call hashLock(), hashFind(),
 *  	  make the changes then call hashUnlock().
 **/
extern HashErrors_t libHashFind(HashRoot_t *pTable, const HashEntry_t entry, HashEntry_t *pResult, int alreadyLocked);

/** libHashWalk - Walk entire hash tree.
 *
 *  At entry:
 *  @param pTable - pointer to hash table root.
 *  @param callback_fn - pointer to function to callback for
 *  				 each item in hash table.
 *  @param pUserData - optional pointer to pass to callback
 *  				 function.
 *  @param alreadyLocked - set non-zero if hash table has
 *  					previously been locked by hashLock().
 *
 *  At exit:
 *  @return 0 on success, non-zero on error.
 *
 *  @note callback_fn will be called for each entry found in the
 *  	  entire hash table in succession. Callback_fn must
 *  	  return 0 to continue with the walk. A non-zero return
 *  	  will stop the walk and libHashWalkFind() will return
 *  	  with that return value added to HashMaxError.
 *
 *  @note If alreadyLocked==0, the hashtable will be locked for
 *  	  the entire transaction. To preserve the continuity of
 *  	  the hash table links and prevent deadlocks, do not
 *  	  call any other hash table functions in the callback.
 **/
typedef int (*HashWalkCallback_t)(const HashEntry_t pEntry, void* pUserData);
extern int libHashWalk(HashRoot_t *pTable, HashWalkCallback_t callback_fn, void *pUserData, int alreadyLocked);

/** libHashDump - Dump the hash tree. Used for debugging.
 *
 *  At entry:
 *  @param pTable - pointer to hash table root.
 *  @param callback_fn - pointer to function to callback for
 *  				 each item in hash table.
 *  @param pDmpPtr - optional pointer to pass to callback
 *  				 function.
 *  @param alreadyLocked - set non-zero if hash table has
 *  					previously been locked by hashLock().
 *
 *  At exit:
 *  @return nothing.
 *
 *  @note callback will be called for each entry found in the
 *  	  top level hash table but unlike libHashWalk(), the
 *  	  entry returned is of type HashPrimitive_t so the
 *  	  callback function will have to follow the next links
 *  	  itself to get each of the members in the hash table.
 *
 *  @note If alreadyLocked==0, the hashtable will be locked for
 *  	  the entire transaction. To preserve the continuity of
 *  	  the hash table links and prevent deadlocks, do not
 *  	  call any other hash table functions in the callback.
 **/
typedef void (*HashDumpCallback_t)(void* pDmpPtr, int hashIndex, const HashPrimitive_t *pHashEntry);
extern void libHashDump(HashRoot_t *pTable, HashDumpCallback_t callback_fn, void *pDmpPtr);

/** libHashLock - lock the hash table.
 *
 * At entry:
 * @param pTable - pointer to hash table
 *
 * At exit:
 * @return 0 on success else error code. Look in errno for
 *  	   further indication of error.
 **/
extern HashErrors_t libHashLock(HashRoot_t *pTable);

/** libHashUnlock - unlock the hash table.
 *
 * At entry:
 * @param pTable - pointer to hash table
 *
 * At exit:
 * @return 0 on success else error code. Look in errno for
 *  	   further indication of error.
 **/
extern HashErrors_t  libHashUnlock(HashRoot_t *pTable);

#endif		/* _LIB_HASHTBL_H_ */

