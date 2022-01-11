/*
    lib_btree.h - a generic AVL balanced tree symbol table example.
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
#ifndef _LIB_BTREE_H_
#define _LIB_BTREE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#ifndef n_elts
#define n_elts(x) (int)(sizeof(x)/sizeof((x)[0]))
#endif

typedef void *BtreeEntry_t;

typedef struct BtreeNode_t
{
	BtreeEntry_t entry;
	union
	{
		struct BtreeNode_t *parent;
		unsigned long balance;
	} upb;
	struct BtreeNode_t *leftPtr;
	struct BtreeNode_t *rightPtr;
} BtreeNode_t;

typedef enum
{
	BtreeSuccess,			/*! No error */
	BtreeInvalidParam,		/*! Invalid parameter passed to function */
	BtreeNoSuchSymbol,		/*! No symbol found */
	BtreeDuplicateSymbol,	/*! Duplicate symbol found */
	BtreeOutOfMemory,		/*! Ran out of memory */
	BtreeEndOfTable,		/*! Reached end of symbol table */
	BtreeNotSupported,		/*! Function not yet supported */
	BtreeLockFail,			/*! pthread lock failed. Look at errno for actual error */
	BtreeMaxError			/*! pthread unlock failed. Look at errno for actual error */
} BtreeErrors_t;

/** BtreeMsgSeverity_t - define the severity of messages
 *  that may be emitted by the btree library functions.
 **/
typedef enum
{
	BTREE_SEVERITY_INFO,
	BTREE_SEVERITY_WARN,
	BTREE_SEVERITY_ERROR,
	BTREE_SEVERITY_FATAL
} BtreeMsgSeverity_t;

/** BtreeCallbacks_t - define the pointers to functions to
 *  callback for various btree lib primitive functions.
 *
 *  @note The memory callbacks are used only to get memory for
 *  	  the btree lib's internal use. Both memAlloc and
 *  	  memFree must contain a pointer or both must be NULL.
 *  	  If set to NULL, default functions using malloc/free
 *  	  will be used. The memArg pointer will be delivered to
 *  	  the memAlloc and memFree functions when called.
 *
 *  @note The msgOut callback is used by the btree lib to emit
 *  	  informational and error messages. If msgOut is set to
 *  	  NULL, a default function using stdout and stderr will
 *  	  be used. The msgArg pointer will be delivered to the
 *  	  msgOut function when it is called.
 *
 *  @note The symCmp callback must be set and the function is
 *  	  to return a -/0/+ value depending on the comparison of
 *  	  its two parameters: aa-bb. The pointer symArg will be
 *  	  delivered to the symCmp function when called.
 **/
typedef struct
{
	void *(*memAlloc)(void *memArg, size_t size);	/*! pointer to memory allocation function */
	void (*memFree)(void *memArg, void *ptr);		/*! pointer to memory free function */
	void *memArg;									/*! argument that will be passed to memAlloc() and memFree() */
	void (*msgOut)(void *msgArg, BtreeMsgSeverity_t severity, const char *msg);	/*! Message (error and info) output callback */
	void *msgArg;									/*! Argument to pass to msgOut callback */
	int (*symCmp)(void *symArg, const BtreeEntry_t aa,const BtreeEntry_t bb);		/*! pointer to function to perform compare */
	void *symArg;									/*! Argument that will be passed to symCmp() */
} BtreeCallbacks_t;

#define BTREE_VERBOSE_ERROR (0x01)	/*! set this flag in verbose to emit error messages directly */

/**
 * BtreeControl_t - the principal structure containing all the
 * details of the btree table. With the exception of pUser1, 
 * pUser2 and verbose user code ought not alter any of the
 * entries in this structure directly.
 **/
typedef struct BtreeControl_t
{
	pthread_mutex_t lock;		/*! thread locker */
	int verbose;				/*! verbose flags */
	BtreeCallbacks_t callbacks;	/*! Pointers to various callback functions */
	int numEntries;				/*! number of active entries in the hash table (table itself + any chains) */
	BtreeNode_t *root;			/*! root of the btree */
	void *pUser1;				/*! pointer free to use for anything */
	void *pUser2;				/*! pointer free to use for anything */
} BtreeControl_t;

/** libBtreeErrorString - Get error string.
 *
 *  At data:
 *  @param error - error code
 *
 *  At exit:
 *  @return const pointer to error string
 **/
extern const char *libBtreeErrorString(BtreeErrors_t error);

/** libBtreeInit - Inialize the btree functions.
 *
 *  At entry:
 *  @param callbacks - pointer to list of various callback
 *  				 functions.
 *
 *  At exit:
 *  @return pointer to BtreeControl_t struct which holds details
 *  		of the btree table for further access or NULL if the
 *  		init could not be performed. Possibly errno might
 *  		have further details for reason of any failure.
 *
 *  @note This BST uses the AVL method of keeping the tree
 *  	  balanced. The balance factor bits are stored in the
 *  	  low order 2 bits of the pointer to parent. In order
 *  	  for that to work and if a memAlloc function is
 *  	  provided it must ensure the size is made a multiple of
 *  	  4 before memory is allocated.
 **/
extern BtreeControl_t* libBtreeInit(const BtreeCallbacks_t *callbacks);

/** libBtreeDestroy - Free all the memory in the btree table.
 *
 *  At Entry:
 *  @param pTable - Pointer to btree table root.
 *  @param entry_free_fn - optional pointer to function that
 *  					 will be called for each node in the
 *  					 tree passing the entry member to this
 *  					 function.
 *  @param freeArg - argument passed to entry_free_fn along with
 *  			   the btree entry.
 *
 *  At Exit:
 *  @return 0 on success else error. All allocated memory should
 *  		have been freed.
 *
 *  @note If the entry_free_fn parameter is provided it will be
 *  	  called for each member in the btree table with the
 *  	  anticipation it be used to free any memory allocated
 *  	  for that member. Essentially a btreeWalk() but each
 *  	  member is expected to be free'd and its data nulled
 *  	  out.
 **/
extern BtreeErrors_t libBtreeDestroy(BtreeControl_t *pTable, void (*entry_free_fn)(void *freeArg, BtreeEntry_t entry), void *freeArg);

/** libBtreeInsert - insert item into btree table.
 *
 *  At entry:
 *  @param pTable - pointer to btree table control.
 *  @param entry - entry to insert
 *
 *  At exit:
 *  @return 0 if success else error code. Will not insert a
 *  		duplicate entry.
 **/
extern BtreeErrors_t libBtreeInsert(BtreeControl_t *pTable, const BtreeEntry_t entry);

/** libBtreeReplace - replace or insert an entry into btree
 *  		table.
 *
 *  At entry:
 *  @param pTable - pointer to btree table control.
 *  @param entry - entry to look for and replace.
 *  @param pExisting - optional pointer to return existing
 *  				entry if any.
 *
 *  At exit:
 *  @return 0 if success else error code.
 *  @note if no existing entry found, entry is inserted. If
 *  	  existing entry found, it is returned in place pointed
 *  	  to by pExisting else *pExisting=NULL.
 **/
extern BtreeErrors_t libBtreeReplace(BtreeControl_t *pTable, const BtreeEntry_t entry, BtreeEntry_t *pExisting);

/** libBtreeDelete - delete a matching data from btree table.
 *
 *  At entry:
 *  @param pTable - pointer to btree table control.
 *  @param data - entry to look for.
 *  @param pExisting - optional pointer to return existing
 *  				entry if any.
 *
 *  At exit:
 *  @return 0 if success else error if not found.
 **/
extern BtreeErrors_t libBtreeDelete(BtreeControl_t *pTable, const BtreeEntry_t entry, BtreeEntry_t *pExisting);

/** libBtreeFind - find a matching entry in btree table.
 *
 *  At entry:
 *  @param pTable - pointer to btree table control.
 *  @param entry - pointer to entry to look for.
 *  @param pResult - pointer to place to deposit result.
 *  @param alreadyLocked - set non-zero if hash table has
 *  					previously been locked by
 *  					libBtreeLock().
 *
 *  At exit:
 *  @return 0 on success else error code.
 *     The *pResult contains the result or NULL if nothing
 *     found.
 **/
extern BtreeErrors_t libBtreeFind(BtreeControl_t *pTable, const BtreeEntry_t entry, BtreeEntry_t *pResult, int alreadyLocked);

typedef enum
{
	BtreeInorder,	/* first all the left nodes, then root, then right nodes (sorted: ascending) */
	BtreePreorder,	/* first the root, then the left nodes, then the right nodes */
	BtreePostorder,	/* first left nodes, then the right nodes, then the root */
	BtreeEndorder	/* first the right nodes, then the root, then the left nodes (sorted: descending) */
} BtreeOrders_t;

/** libBtreeWalk - Walk entire btree.
 *
 *  At entry:
 *  @param pTable - pointer to btree table control.
 *  @param order - which order to use to walk.
 *  @param callback_fn - pointer to function to callback for
 *  				 each entry in btree table.
 *  @param pUserData - optional pointer to pass to callback
 *  				 function.
 *
 *  At exit:
 *  @return 0 on success, non-zero on error.
 *
 *  @note callback will be called for each entry found in the
 *  	  entire btree table in succession. Function must return
 *  	  0 to continue with the walk. A non-zero will stop the
 *  	  walk and the btreeWalk function will return with that
 *  	  value added to BtreeMaxError.
 *
 *  @note The btreetable is locked for the entire transaction. Do
 *  	  not call any other btree table primitives in the
 *  	  callback function.
 **/
typedef int (*BtreeWalkCallback_t)(void *userData, const BtreeEntry_t data);
extern int libBtreeWalk(BtreeControl_t *pTable, BtreeOrders_t order, BtreeWalkCallback_t callback_fn, void *userData);

/** libBtreeTableLock - lock the btree table.
 *
 * At entry:
 * @param pTable - pointer to btree table
 *
 * At exit:
 * @return 0 on success else error code. Look in errno for
 *  	   further error indication.
 **/
extern BtreeErrors_t libBtreeLock(BtreeControl_t *pTable);

/** libBtreeTableUnlock - unlock the btree table.
 *
 * At entry:
 * @param pTable - pointer to btree table
 *
 * At exit:
 * @return 0 on success else error code. Look in errno for
 *  	   further error indication.
 **/
extern BtreeErrors_t  libBtreeUnlock(BtreeControl_t *pTable);

/** libBtreeHeight - compute the maximum length of any one
 *  tree.
 *
 *  At entry:
 *  @param pTable - pointer to btree table
 *
 *  At exit:
 *  @return length of longest tree
 **/
extern int libBtreeHeight(BtreeControl_t *pTable);

#endif	/* _LIB_BTREE_H_*/
