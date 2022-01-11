/*
    lib_hashtbl.c - a generic hash table symbol table example.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "lib_hashtbl.h"

/* */
typedef unsigned char Bool;
typedef enum
{
	False,
	True
} Bool_t;

typedef struct
{
	HashErrors_t err;
	const char *msg;
} ErrorMsgs_t;
static const ErrorMsgs_t ErrorMsgs[] =
{
	{ HashSuccess, "Success" },
	{ HashInvalidParam, "Invalid parameter" },
	{ HashNoSuchSymbol, "No such symbol" },
	{ HashDuplicateSymbol, "Duplicate symbol" },
	{ HashOutOfMemory, "Out of memory" },
	{ HashEndOfTable, "End of hash table" },
	{ HashNoLock, "Failed to lock pthread mutex" },
	{ HashNoUnLock, "Failed to unlock pthread mutex" },
};

const char *libHashErrorString(HashErrors_t error)
{
	int ii;
	for (ii=0; ii < n_elts(ErrorMsgs);++ii)
	{
		if ( ErrorMsgs[ii].err == error )
			return ErrorMsgs[ii].msg;
	}
	return "Undefined error code";
}

static void *lclMalloc(void *arg, size_t size)
{
	return malloc(size);
}

static void lclFree(void *arg, void *ptr)
{
	free(ptr);
}

static void lclMsg(void *msgArg, HashMsgSeverity_t severity, const char *msg)
{
	static const char *Severities[] = { "INFO", "WARN", "ERROR", "FATAL" };
	fprintf(severity > HASH_SEVERITY_INFO ? stderr:stdout,"%s-libHash: %s",Severities[severity],msg);
}

HashRoot_t* libHashInit(int tableSize, const HashCallbacks_t *callbacks)
{
	HashRoot_t *tbl;
	HashCallbacks_t tCallbacks;
	char emsg[128];
	
	memset(&tCallbacks,0,sizeof(HashCallbacks_t));
	if ( callbacks && callbacks->msgOut )
	{
		tCallbacks.msgArg = callbacks->msgArg;
		tCallbacks.msgOut = callbacks->msgOut;
	}
	else
		tCallbacks.msgOut = lclMsg;
	if ( !callbacks || !callbacks->symCmp || !callbacks->symHash )
	{
		tCallbacks.msgOut(tCallbacks.msgArg,HASH_SEVERITY_FATAL,"Must provide a symHash and symCmp function\n");
		return NULL;
	}
	tCallbacks.symArg = callbacks->symArg;
	tCallbacks.symCmp = callbacks->symCmp;
	tCallbacks.symHash = callbacks->symHash;
	if ( !callbacks->memAlloc && !callbacks->memFree )
	{
		tCallbacks.memAlloc = lclMalloc;
		tCallbacks.memFree = lclFree;
	}
	else if ( !callbacks->memAlloc || !callbacks->memFree )
	{
		tCallbacks.msgOut(tCallbacks.msgArg,HASH_SEVERITY_FATAL,"Cannot provide just malloc or just free. Must provide both.\n");
		return NULL;
	}
	else
	{
		tCallbacks.memAlloc = callbacks->memAlloc;
		tCallbacks.memFree = callbacks->memFree;
	}
	tCallbacks.memArg = callbacks->memArg;
	tbl = (HashRoot_t *)tCallbacks.memAlloc(tCallbacks.memArg, sizeof(HashRoot_t));
	if ( !tbl )
	{
		snprintf(emsg,sizeof(emsg),"Not enough memory to allocate %ld bytes for HashRoot_t.\n", sizeof(HashRoot_t));
		tCallbacks.msgOut(tCallbacks.msgArg,HASH_SEVERITY_FATAL,emsg);
		return NULL;
	}
	memset(tbl,0,sizeof(HashRoot_t));
	if ( tableSize <= 0 )
		tableSize = 997;
	tbl->callbacks = tCallbacks;
	tbl->hashTable = (HashPrimitive_t **)tCallbacks.memAlloc(tCallbacks.memArg, sizeof(HashPrimitive_t *)*tableSize);
	if ( !tbl->hashTable )
	{
		snprintf(emsg,sizeof(emsg),"Not enough memory to allocate %ld bytes for %d hash table slots\n", sizeof(HashPrimitive_t *)*tableSize, tableSize);
		tCallbacks.msgOut(tCallbacks.msgArg,HASH_SEVERITY_FATAL,emsg);
		tCallbacks.memFree(tCallbacks.memArg, tbl);
		return NULL;
	}
	memset(tbl->hashTable, 0, sizeof(HashPrimitive_t *)*tableSize);
	tbl->hashTableSize = tableSize;
	pthread_mutex_init(&tbl->lock,NULL);
	tbl->numEntries = 0;
	return tbl;
}

void libHashDestroy(HashRoot_t *pTable, void (*entry_free_fn)(void *freeArg, HashEntry_t entry), void *freeArg)
{
	int ii;
	void (*memFree)(void *memArg, void *ptr) = pTable->callbacks.memFree;
	void *memArg = pTable->callbacks.memArg;
	
	pthread_mutex_destroy(&pTable->lock);
	for (ii = 0; ii < pTable->hashTableSize; ++ii)
	{
		HashPrimitive_t *pHash, *pNext;
		pNext = pTable->hashTable[ii];
		while ( (pHash=pNext) )
		{
			if ( entry_free_fn )
				entry_free_fn(freeArg, pHash->entry);
			pNext = pHash->next;
			pHash->entry = NULL;
			pHash->next = NULL;
			pHash->prev = NULL;
			memFree(memArg,pHash);
		}
	}
	memFree(memArg,pTable->hashTable);
	memFree(memArg,pTable);
}

typedef struct
{
	HashPrimitive_t **ppHash;	/* Pointer to previous pointer (or to hash table entry if appropriate) */
	HashPrimitive_t *pCurr;		/* Pointer to place where entry is found or place where new is to be inserted */
	int diff;					/* result of compare */
	unsigned int hashIdx;		/* index into hash table */
} FindTbl_t;

/** findPlace - find the place in the hash tree where an
 *  entry either is or would be placed if it isn't there.
 *  At Entry:
 *  @param pTable - pointer to hash table
 *  @param entry  - user's entry
 *  @param param  - pointer to FindTbl_t struct
 *
 *  At exit:
 *  @return pointer to found entry or NULL if none found.
 **/
static HashPrimitive_t* findPlace(const HashRoot_t *pTable, const HashEntry_t entry, FindTbl_t *param)
{
	HashPrimitive_t **ppPrev, *pHashEntry;

	param->diff = -1;
	/* Find the index into the hash table */
	param->hashIdx = pTable->callbacks.symHash(pTable->callbacks.symArg, pTable->hashTableSize, entry);
	/* make sure the result is valid */
	if ( param->hashIdx >= pTable->hashTableSize )
		param->hashIdx = 0;
	/* get a pointer to the position in the hash table */
	ppPrev = &pTable->hashTable[param->hashIdx];
	/* return it to caller */
	param->ppHash = ppPrev;
	/* Start at the head of the list */
	pHashEntry = *ppPrev;
	/* return pointer to the potential place to deposit */
	param->pCurr = pHashEntry;
	/* loop through the list */
	while ( pHashEntry )
	{
		param->diff = pTable->callbacks.symCmp(pTable->callbacks.symArg, pHashEntry->entry, entry);
		if ( param->diff < 0 )
		{
			/* the list is stored sorted, low to high. The insert is to be later in the list */
			pHashEntry = pHashEntry->next;
			if ( pHashEntry )
				param->pCurr = pHashEntry;
			continue;
		}
		/* diff is >= 0 */
		if ( param->diff > 0 )
		{
			/* Didn't find it. We return NULL but pCurr points where a new one should be inserted */
			pHashEntry = NULL;
		}
		/* diff >= 0 means we're done looking */
		break;
	}
	return pHashEntry;
}

/*
 * Get the next available HashPrimitive_t either from the
 * pool or from the list of those previously deleted.
 */
static HashPrimitive_t *getNewEntry(HashRoot_t *pTable )
{
	HashPrimitive_t *pNewEntry;
	
	pNewEntry = (HashPrimitive_t *)pTable->callbacks.memAlloc(pTable->callbacks.memArg, sizeof(HashPrimitive_t));
	pNewEntry->entry = NULL;
	pNewEntry->next = NULL;
	pNewEntry->prev = NULL;
	++pTable->numEntries;
	return pNewEntry;
}

static HashErrors_t internalInsert(HashRoot_t *pTable, FindTbl_t *pFtbl, HashPrimitive_t *pNewEntry)
{
	if ( pFtbl->pCurr )
	{
		if ( pFtbl->diff > 0 )
		{
			/* the new one is to be inserted ahead of pCurr */
			pNewEntry->prev = pFtbl->pCurr->prev;
			pNewEntry->next = pFtbl->pCurr;
			pFtbl->pCurr->prev = pNewEntry;
			if ( pNewEntry->prev )
				pNewEntry->prev->next = pNewEntry;
		}
		else
		{
			/* The new one is to be inserted after pCurr */
			pNewEntry->next = pFtbl->pCurr->next;
			pNewEntry->prev = pFtbl->pCurr;
			pFtbl->pCurr->next = pNewEntry;
			if ( pNewEntry->next )
				pNewEntry->next->prev = pNewEntry;
		}
	}
	if ( !pNewEntry->prev )
		*pFtbl->ppHash = pNewEntry;
	return HashSuccess;
}

HashErrors_t libHashReplace(HashRoot_t *pTable, const HashEntry_t entry, HashEntry_t *pExisting)
{
	HashPrimitive_t *pHashEntry;
	FindTbl_t fTbl;
	
	if ( pExisting )
		*pExisting = NULL;
	/* validate input parameters */
	if ( !pTable || !entry )
		return HashInvalidParam;
	/* Lock the hash table for the search */ 
	pthread_mutex_lock(&pTable->lock);
	/* Look for the current entry */
	pHashEntry = findPlace(pTable,entry,&fTbl);
	if ( !pHashEntry )
	{
		if ( !(pHashEntry = getNewEntry(pTable)) )
		{
			pthread_mutex_unlock(&pTable->lock);
			return HashOutOfMemory;
		}
		pHashEntry->entry = entry;
		internalInsert(pTable, &fTbl, pHashEntry);
	}
	else
	{
		if ( pExisting )
			*pExisting = pHashEntry->entry;
		pHashEntry->entry = entry;
	}
	pthread_mutex_unlock(&pTable->lock);
	return HashSuccess;
}

HashErrors_t libHashInsert(HashRoot_t *pTable, const HashEntry_t entry)
{
	HashPrimitive_t *pHashEntry;
	FindTbl_t fTbl;
	
	/* */
	if ( !pTable || !entry )
		return HashInvalidParam; 
	pthread_mutex_lock(&pTable->lock);
	pHashEntry = findPlace(pTable,entry,&fTbl);
	if ( pHashEntry )
	{
		pthread_mutex_unlock(&pTable->lock);
		return HashDuplicateSymbol;
	}
	if ( !(pHashEntry = getNewEntry(pTable)) )
	{
		pthread_mutex_unlock(&pTable->lock);
		return HashOutOfMemory;
	}
	pHashEntry->entry = entry;
	internalInsert(pTable,&fTbl,pHashEntry);
	pthread_mutex_unlock(&pTable->lock);
	return HashSuccess;
}

HashErrors_t libHashDelete(HashRoot_t *pTable, HashEntry_t entry, HashEntry_t *pExisting)
{
	HashPrimitive_t *pHashEntry;
	FindTbl_t fTbl;
	
	if ( pExisting )
		*pExisting = NULL;
	if ( !pTable || !entry )
		return HashInvalidParam;
	pthread_mutex_lock(&pTable->lock);
	pHashEntry = findPlace(pTable,entry,&fTbl);
	if ( !pHashEntry )
	{
		pthread_mutex_unlock(&pTable->lock);
		return HashNoSuchSymbol;
	}
	if ( pHashEntry->next )
		pHashEntry->next->prev = pHashEntry->prev;
	if ( pHashEntry->prev )
		pHashEntry->prev->next = pHashEntry->next;
	else
		*fTbl.ppHash = pHashEntry->next;
	if ( pExisting )
		*pExisting = pHashEntry->entry;
	pHashEntry->entry = NULL;
	pHashEntry->next = NULL;
	pHashEntry->prev = NULL;
	pTable->callbacks.memFree(pTable->callbacks.memArg,pHashEntry);
	--pTable->numEntries;
	pthread_mutex_unlock(&pTable->lock);
	return HashSuccess;
}

HashErrors_t libHashFind(HashRoot_t *pTable, HashEntry_t entry, HashEntry_t *pExisting,int alreadyLocked)
{
	HashPrimitive_t *pHashEntry;
	FindTbl_t fTbl;
	
	if ( pExisting )
		*pExisting = NULL;
	if ( !pTable || !entry )
		return HashInvalidParam;
	if ( !alreadyLocked )
		pthread_mutex_lock(&pTable->lock);
	pHashEntry = findPlace(pTable,entry,&fTbl);
	if ( !pHashEntry )
	{
		if ( !alreadyLocked )
			pthread_mutex_unlock(&pTable->lock);
		return HashNoSuchSymbol;
	}
	if ( pExisting )
		*pExisting = pHashEntry->entry;
	if ( !alreadyLocked )
		pthread_mutex_unlock(&pTable->lock);
	return HashSuccess;
}

int libHashWalk(HashRoot_t *pTable, HashWalkCallback_t callback_fn, void *pUserData, int alreadyLocked)
{
	int ii,err=HashSuccess;
	HashPrimitive_t *pHashEntry;

	if ( !pTable || !callback_fn )
		return HashInvalidParam;
	if ( !alreadyLocked )
		pthread_mutex_lock(&pTable->lock);
	for (ii=0; ii < pTable->hashTableSize; ++ii)
	{
		pHashEntry = pTable->hashTable[ii];
		while ( pHashEntry )
		{
			err = callback_fn(pHashEntry->entry, pUserData);
			if ( err )
			{
				err += HashMaxError;
				break;
			}
			pHashEntry = pHashEntry->next;
		}
	}
	if ( !alreadyLocked )
		pthread_mutex_unlock(&pTable->lock);
	return err;
}

void libHashDump(HashRoot_t *pTable, HashDumpCallback_t callback_fn, void *pUserData)
{
	if ( pTable && callback_fn )
	{
		int ii;

		pthread_mutex_lock(&pTable->lock);
		for (ii=0; ii < pTable->hashTableSize; ++ii)
		{
			HashPrimitive_t *pHashEntry;
			if ( (pHashEntry = pTable->hashTable[ii]) )
				callback_fn(pUserData, ii, pHashEntry);
		}
		pthread_mutex_unlock(&pTable->lock);
	}
}

HashErrors_t hashTableLock(HashRoot_t *pTable)
{
	if ( pthread_mutex_lock(&pTable->lock) )
	{
		char emsg[128];
		snprintf(emsg,sizeof(emsg),"Failed to lock hash table: %s\n", strerror(errno));
		pTable->callbacks.msgOut(pTable->callbacks.msgArg,HASH_SEVERITY_ERROR,emsg);
		return HashNoLock;
	}
	return HashSuccess;
}

HashErrors_t hashTableUnlock(HashRoot_t *pTable)
{
	if ( pthread_mutex_unlock(&pTable->lock) )
	{
		char emsg[128];
		snprintf(emsg,sizeof(emsg),"Failed to unlock hash table: %s\n", strerror(errno));
		pTable->callbacks.msgOut(pTable->callbacks.msgArg,HASH_SEVERITY_ERROR,emsg);
		return HashNoUnLock;
	}
	return HashSuccess;
}

