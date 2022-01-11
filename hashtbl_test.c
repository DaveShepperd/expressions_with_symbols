/*
    hashtbl_test.c - simple test code for lib_hashtbl.[ch].
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
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>

#include "hashtbl_test.h"

/* This test code uses example local memory allocation/free routines.
 * This is just to keep track of the number of malloc's and free's and
 * the total memory used. It helps discover any memory leaks that may
 * crop up in real code. Also it can be used to make the hash functions
 * safe to use in multi-threaded applications, etc.
 */
typedef struct
{
	pthread_mutex_t mutex;
	int numMallocs;
	int numFrees;
	int numCompares;
	size_t totalMemUsed;
	size_t totalUserMem;
	size_t totalFreed;
} MemStats_t;

static MemStats_t memStats = { PTHREAD_MUTEX_INITIALIZER };

static void showMemoryStats(void)
{
	printf("Memory: numMallocs=%d, numFrees=%d, totMemUsed=%ld, totUserMem=%ld, totFrees=%ld\n",
		   memStats.numMallocs, memStats.numFrees,
		   memStats.totalMemUsed, memStats.totalUserMem, memStats.totalFreed
		   );
}

static void *xMalloc(void *arg,size_t size)
{
	MemStats_t *mem = (MemStats_t *)arg;
	size_t *newPtr;
	size_t newSize;

	/* probably not strictly necessary, but make the request
	 * size a multiple of size_t since we're going to add two
	 * amounts to the allocated space.
	 */
	newSize = (size+sizeof(size_t)-1)&-sizeof(size_t);
	/* add room for two size_t variables */
	newSize += 2*sizeof(size_t);
	/* lock our stats struct */
	pthread_mutex_lock(&mem->mutex);
	/* grab some memory */
	newPtr = (size_t *)malloc(newSize);
	if ( !newPtr )
	{
		/* Ooops, out of memory */
		pthread_mutex_unlock(&mem->mutex);
		return NULL;
	}
	/* Remember how much we grabbed */
	newPtr[0] = newSize;
	/* Remember how much the caller asked for */
	newPtr[1] = size;
	/* total the allocation and sizes */
	++mem->numMallocs;
	mem->totalMemUsed += newSize;
	mem->totalUserMem += size;
	/* unlock and return */
	pthread_mutex_unlock(&mem->mutex);
	return newPtr+2;
}

static void xFree(void *arg, void *ptr)
{
	MemStats_t *mem = (MemStats_t *)arg;
	size_t *oldPtr = (size_t *)ptr;
	/* lock our memory stats struct */
	pthread_mutex_lock(&mem->mutex);
	/* point back to the real starting point */
	oldPtr -= 2;
	/* record the free and free it */
	mem->totalFreed += oldPtr[0];
	++mem->numFrees;
	free(oldPtr);
	/* unlock and return */
	pthread_mutex_unlock(&mem->mutex);
}

/* Some arbitrary variables. */
#define KEY_SIZE	(32)			/* arbitrary length of our key's string */
#define VALUE_SIZE	(32)			/* arbitrary length of our key's value */
#define FLAG_DATA_MALLOCD	(0x01)	/* mark whether the data area has been malloc'd or provided via a pool */

/* Arbitrary definition of some data. In this case, some flags, a string key and a string value */
typedef struct
{
	int flags;
	char key[KEY_SIZE];
	char value[VALUE_SIZE];
} Data_t;

static const Data_t SampleEntries[] = 
{
	{ 0, "h", "8" },
	{ 0, "g", "7" },
	{ 0, "f", "6" },
	{ 0, "e", "5" },
	{ 0, "d", "4" },
	{ 0, "c", "3" },
	{ 0, "b", "2" },
	{ 0, "a", "1" },
};

/* Provide some random number to use as a seed to the hash algorithm */
#ifndef HASH_STRING_SEED
#define HASH_STRING_SEED (11)
#endif

static unsigned int hashIt(void *userArg, int size, const HashEntry_t entry)
{
	const Data_t *pData = (const Data_t *)entry;
	int hashv=HASH_STRING_SEED;
	unsigned char cc;
	const char *key;

	/* This simple hash algorithm just accumulates the value of each character of the key times the hash table size plus the initial seed */
	key = pData->key;
	while ( (cc= *key++) )
	{
		hashv = hashv * size + cc;
	}
	/* Then returns that sum modulo the size of the hash table. */
	return hashv % size;
}

static int compareThem(void *userArg, const HashEntry_t aa,const HashEntry_t bb)
{
	MemStats_t *mem = (MemStats_t *)userArg;
	const Data_t *pAa = (const Data_t *)aa;
	const Data_t *pBb = (const Data_t *)bb;
	/* As in this example the memory stats pointer may be used to
	 * keep track of how many compares have been performed. Just
	 * for gathering statistics if so desired.
	 *
	 * NOTE: There is no need to lock the MemStats_t here because the
	 * whole hash table has been locked for the entire duration of the
	 * lookup and any add, update or delete that may follow.
	 */
	++mem->numCompares;
	return strcmp(pAa->key,pBb->key);
}

static void dumpTable(void* pUserData, int hashIndex, const HashPrimitive_t *pHashEntry)
{
	const char *banner="";
	
	/* This is how one could show the contents of the hash table.
	 * pUserData is a pointer provided by the hashDump() call. In this example it is NULL.
	 * hashIndex is the index into the hash table for the entry tree.
	 * pHashEntry is the pointer to the first entry in the tree.
	 * NOTE: This function will never be called with a NULL pHashEntry.
	 */
	printf("%2d: ", hashIndex);
	while ( pHashEntry )
	{
		const Data_t *data = (Data_t *)pHashEntry->entry;
		printf("%s{'%s':'%s'}", banner, data->key, data->value );
		banner = " -> ";
		pHashEntry = pHashEntry->next;
	}
	printf("\n");
}

int hashtblTest(int tblSize, int numItems)
{
	HashRoot_t *pTable;
	HashCallbacks_t tCallbacks;
	HashErrors_t err;
	int lim, opt;
	
	/* tblSize = number of items to allocate for the hash table itself.
	 * numItems = Max number of items to add to hash table.
	 */
	memset(&tCallbacks,0,sizeof(tCallbacks));
	tCallbacks.memAlloc = xMalloc;
	tCallbacks.memFree = xFree;
	tCallbacks.memArg = &memStats;
	tCallbacks.symArg = &memStats;
	tCallbacks.symCmp = compareThem;
	tCallbacks.symHash = hashIt;
	pTable = libHashInit(tblSize, &tCallbacks);
	if ( !pTable )
	{
		fprintf(stderr,"Out of memory doing hashInit()\n");
		return 1;
	}
	lim = n_elts(SampleEntries);
	if ( numItems && numItems < lim )
		lim = numItems;
	for (opt=0; opt < lim; ++opt)
	{
		if ( (err=libHashInsert(pTable,(const HashEntry_t)(SampleEntries+opt))) )
		{
			fprintf(stderr,"hashInsert() of {'%s':'%s'} returned %d:%s\n",
					SampleEntries[opt].key, SampleEntries[opt].value,
					err, libHashErrorString(err)
					);
			libHashDestroy(pTable,NULL,NULL);
			showMemoryStats();
			return 1;
		}
	}
	libHashDump(pTable,dumpTable,NULL);
	libHashDestroy(pTable,NULL,NULL);
	showMemoryStats();
	return 0;
}

