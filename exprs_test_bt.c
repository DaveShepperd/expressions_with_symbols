/*
    exprs_test_bt.c - simple test code for lib_exprs.[ch].
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
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>

#include "lib_btree.h"
#include "lib_exprs.h"
#include "exprs_test_bt.h"

/**
 *  This example shows how to use the expression parser with all
 *  available external functions: memory management, symbol
 *  managment using the AVL btree lib and a custom message
 *  emitter.
 *
 *  For an example of how to use the parser in a bare bones
 *  configuration, see exprs_test_nos.c. For an example of using
 *  the parser with just symbol management, see exprs_test_ht.c
 *  (uses hash table lib to handle symbols).
 *
 **/

/**
 * The expression parser knows little about the contents of an
 * individual symbol table entry. Any reference to a symbol
 * within the expression parser is via a pointer and the only
 * thing the parser needs to have are external functions to do
 * symbol table lookups and assignments. To that end,
 * SymbolTableEntry_t is our definition of what a symbol table
 * entry is. In this case, the contents are just a name and a
 * value. In this example, to keep things easy and simple, the
 * value is defined as being one of the primitives the
 * expression parser does know how to deal with which is the
 * ExprsSymTerm_t type.
 **/
 
typedef struct
{
	const char *name;
	ExprsSymTerm_t value;
} SymbolTableEntry_t;

/** MemStats_t - define an example of an option to use with
 *  expression handler's internal memory manager.
 **/
typedef struct
{
	pthread_mutex_t mutex;
	int numMallocs;
	int numFrees;
	int numCompares;
	size_t totalMemUsed;
	size_t totalFreed;
} MemStats_t;

static MemStats_t memStats = { PTHREAD_MUTEX_INITIALIZER };

/**
 * The expression parser expects pointers to functions to handle
 * memory allocation and free. Those functions are called with a
 * provided pointer. This is to allow one to externally keep
 * statistics of memory use or any other function (or not).
 *
 * In this example we show a simple example of keeping memory
 * use statistics and to help discover memory leaks, etc. To
 * that end, we use a pointer to the memStats struct as the
 * parameter passed to the memory allocation and free functions.
 *
 * And to keep things simple, this local memory allocation
 * function uses malloc to get the memory.
 */

/**
 * lclAlloc - function to allocate memory.
 *
 * At entry:
 * @param memArg - pointer to memStats
 * @param size - amount of memory to allocate
 *
 * At exit:
 * @return pointer to memory or NULL if error.
 */
static void *lclAlloc(void *memArg, size_t size)
{
	MemStats_t *sp = (MemStats_t *)memArg;
	static const size_t SizeT = sizeof(size_t);
	size_t *ans;
	int newSize;
	
	/* round up size to sizeof(size_t) and add an additional sizeof(size_t) */	
	newSize = (size+2*SizeT-1)&-SizeT;
	/* lock our stats area */
	pthread_mutex_lock(&sp->mutex);
	/* Grab the memory */
	ans = (size_t *)malloc(newSize);
	if ( ans )
	{
		/* put the size in the first spot and skip over it */
		*ans++ = newSize;
		/* record our stats */
		sp->totalMemUsed += newSize;
		++sp->numMallocs;
	}
	/* unlock */
	pthread_mutex_unlock(&sp->mutex);
	return ans;
}

/**
* lclFree - free memory allocated with lclAlloc()
*
* At entry:
* @param memArg - pointer to memStats
* @param hisPtr - pointer to memory to free
*
* At exit:
* @return nothing.
*/
static void lclFree(void *memArg, void *hisPtr)
{
	MemStats_t *sp;
	
	sp = (MemStats_t *)memArg;
	if ( hisPtr )
	{
		size_t *ourPtr = (size_t *)hisPtr;
		size_t size;

		--ourPtr;
		size = *ourPtr;
		pthread_mutex_lock(&sp->mutex);
		free(ourPtr);
		sp->totalFreed += size;
		++sp->numFrees;
		pthread_mutex_unlock(&sp->mutex);
	}
	else
	{
		/* we should never get here. But just in case... */
		pthread_mutex_lock(&sp->mutex);
		++sp->numFrees;
		pthread_mutex_unlock(&sp->mutex);
	}
}

/**
* freeEntry - function to free the memory allocated for a symbol
* table entry.
*
* At entry:
* @param freeArg - caller provided argument in this example it
*   			 is a pointer to memStats.
* @param entry - pointer to symbol table entry to free.
*
* At exit:
* @return nothing. Memory for the name, string value if any and
*   	  the entry itself have been freed.
*/
static void freeEntry(void *freeArg, void *pEntry)
{
	SymbolTableEntry_t *ent = (SymbolTableEntry_t *)pEntry;
	/* free the name string */
	lclFree(freeArg,(void *)ent->name);
	/* if the value is a string, free it too */
	if ( ent->value.termType == EXPRS_SYM_TERM_STRING )
		lclFree(freeArg,ent->value.term.string);
	/* free the entry itself */
	lclFree(freeArg,ent);
}

/**
* lclShow - function to display messages from the expression
* parser. It is also used as a message display for the symbol
* table function.
*
* At entry:
* @param msgArg - pointer to message title. 
* @param severity - message severity
* @param msg - message
*
* At exit:
* @return nothing. Message emitted to stdout or stderr depending
*   	  on value of severity.
*/
static void lclShow(void *msgArg, ExprsMsgSeverity_t severity, const char *msg)
{
	static const char *Banners[] = {"INFO","WARN","ERROR","FATAL"};
	fprintf(severity > EXPRS_SEVERITY_INFO ? stderr : stdout,"%s%s:%s", msgArg ? (const char *)msgArg : "", Banners[severity], msg);
}

/**
* btreeCmp - compare two symbol table entries.
*
* The symbol table handler requires a function to compare two
* entries to decide how to place them.
*
* At entry:
* @param symArg - in this example, pointer to memStats
* @param aa - pointer to entry 0
* @param bb - pointer to entry 1
* 
* @return integer result of aa-bb
*/
static int btreeCmp(void *symArg, const BtreeEntry_t aa, const BtreeEntry_t bb)
{
	MemStats_t *mem = (MemStats_t *)symArg;
	const SymbolTableEntry_t *paa = (const SymbolTableEntry_t *)aa;
	const SymbolTableEntry_t *pbb = (const SymbolTableEntry_t *)bb;
	/**
	 * As in this example the memory stats pointer may be used to
	 * keep track of how many compares have been performed. Just
	 * for gathering statistics if so desired.
	 *
	 * NOTE: There is no need to lock the MemStats_t here because the
	 * whole btree table has been locked for the entire duration of the
	 * lookup and any add, update or delete that may follow. So no
	 * other threads, even though there are none in this example,
	 * will be updating the member numCompares at the same time.
	 **/
	++mem->numCompares;
	return strcmp(paa->name,pbb->name);
}

/**
 * getBtreeSym - fetch an entry from the btree symbol table.
 *
 * The expression parser requires a function to fetch an entry
 * from the symbol table. This function is to return just the
 * value member of the symbol table.
 *
 * At entry:
 * @param symArg - pointer to symbol table control struct.
 * @param name - name of symbol to lookup
 * @param value - pointer to place to deposit result
 * 
 * @return 0 on success, non-zero on error. If success, symbol
 *  	   value will be deposited where argument 'value'
 *  	   points.
 **/
static ExprsErrs_t getBtreeSym(void *userArg, const char *name, ExprsSymTerm_t *value)
{
	BtreeControl_t *pTable=(BtreeControl_t *)userArg;
	SymbolTableEntry_t ent, *found;
	ent.name = name;
	if ( !libBtreeFind(pTable,(const BtreeEntry_t)&ent,(BtreeEntry_t *)&found,0) )
	{
		value->termType = found->value.termType;
		switch (found->value.termType)
		{
		case EXPRS_SYM_TERM_INTEGER:
			value->term.s64 = found->value.term.s64;
			break;
		case EXPRS_SYM_TERM_FLOAT:
			value->term.f64 = found->value.term.f64;
			break;
		case EXPRS_SYM_TERM_STRING:
			value->term.string = found->value.term.string;
			break;
		}
		return EXPR_TERM_GOOD;
	}
	return EXPR_TERM_BAD_UNDEFINED_SYMBOL;
}

/**
 * setBtreeSym - function to assign value to symbol.
 * 
 * The expression parser requires a function to set the value to
 * an entry in the symbol table. Either insert or replace as
 * necessary.
 *
 * At entry:
 * @param symArg - pointer to symbol table control
 * @param name - name of symbol
 * @param value - pointer to value to assign to symbol
 * 
 * @return 0 on success or non-zero on error.
 *
 * @note If this is to be used multi-threaded, then some care
 *  	 must be taken with regard to locking/unlocking
 *  	 references to the entries stored in the hash table.
 *  	 However, since this example code has no threads and it
 *  	 is completely "in charge" of all memory used by
 *  	 individual symbols, there is no need for locking. But
 *  	 just to demonstrate how one might do updates to
 *  	 individual entries in the symbol table, this function
 *  	 tries to reduce the number of malloc/frees that might
 *  	 otherwise be needed.
 *
 **/
static ExprsErrs_t setBtreeSym(void *symArg, const char *name, const ExprsSymTerm_t *value)
{
	BtreeControl_t *pTable=(BtreeControl_t *)symArg;
	MemStats_t *pMemStats = (MemStats_t *)pTable->pUser1;
	SymbolTableEntry_t tEnt, *ent, *found=NULL;
	char *tStr;
	int len;

	tEnt.name = name;
	/* first lookup the symbol to see if there is one already */
	if ( !libBtreeFind(pTable,(const BtreeEntry_t)&tEnt,(BtreeEntry_t *)&found,0) )
	{
		char *oldStr=NULL;
		/* Since there is one already, there is no need to duplicate the name string. */
		/* But make a note if the current value is a string */
		if ( found->value.termType == EXPRS_SYM_TERM_STRING )
			oldStr = found->value.term.string;
		/* just smack the contents of the current entry pointed to by 'found' */
		switch (value->termType)
		{
		case EXPRS_SYM_TERM_FLOAT:
			found->value.term.f64 = value->term.f64;
			break;
		case EXPRS_SYM_TERM_INTEGER:
			found->value.term.s64 = value->term.s64;
			break;
		case EXPRS_SYM_TERM_STRING:
			if ( oldStr )
			{
				/* we're changing the string. Just do a simple check to see if they are the same */
				if ( !strcmp( value->term.string, oldStr ) )
					return EXPR_TERM_GOOD; /* nothing is different. Just leave it be */
			}
			len = strlen(value->term.string)+1;
			tStr = (char *)lclAlloc(pMemStats,len);
			if ( !tStr )
				return EXPR_TERM_BAD_OUT_OF_MEMORY;
			strncpy(tStr, value->term.string, len);
			found->value.term.string = tStr;
			break;
		default:
			return EXPR_TERM_BAD_UNSUPPORTED;
		}
		if ( oldStr )
			lclFree(pMemStats,oldStr);
		found->value.termType = value->termType;
		return EXPR_TERM_GOOD;
	}
	/* No existing symbol table entry. */
	/* Malloc space for the name. */
	len = strlen(name)+1;
	if ( !(tStr = lclAlloc(pMemStats,len)) )
		return EXPR_TERM_BAD_OUT_OF_MEMORY;
	/* Malloc a symbol table entry */
	if ( !(ent = lclAlloc(pMemStats,sizeof(SymbolTableEntry_t))) )
	{
		lclFree(pMemStats,tStr);	/* toss the space for the name string */
		return EXPR_TERM_BAD_OUT_OF_MEMORY;
	}
	/* start with an empty slate */
	memset(ent,0,sizeof(SymbolTableEntry_t));
	strncpy(tStr,name,len);	/* copy the name string */
	ent->name = tStr;		/* and record its pointer */
	ent->value.termType = value->termType;	/* record the term type */
	switch ( value->termType )
	{
	case EXPRS_SYM_TERM_STRING:
		/* malloc space for the value string */
		len = strlen(value->term.string)+1;
		if ( !(tStr = (char *)lclAlloc(pMemStats,len)) )
		{
			freeEntry(pMemStats,(BtreeEntry_t)ent);
			return EXPR_TERM_BAD_OUT_OF_MEMORY;
		}
		/* copy the string and record a pointer to it */
		strncpy(tStr, value->term.string,len);
		ent->value.term.string = tStr;
		break;
	case EXPRS_SYM_TERM_FLOAT:
		ent->value.term.f64 = value->term.f64;
		break;
	case EXPRS_SYM_TERM_INTEGER:
		ent->value.term.s64 = value->term.s64;
		break;
	default:
		freeEntry(pMemStats,(BtreeEntry_t)ent);
		return EXPR_TERM_BAD_UNDEFINED_SYMBOL;
	}
	if ( !libBtreeInsert(pTable,(const BtreeEntry_t)ent) )
		return EXPR_TERM_GOOD;
	freeEntry(pMemStats,(BtreeEntry_t)ent);
	return EXPR_TERM_BAD_UNDEFINED_SYMBOL;
}

int exprsTestBtree(int btreeSize, const char *expression, int verbose)
{
	ExprsCallbacks_t ourCallbacks;
	BtreeCallbacks_t btCallbacks;
	ExprsDef_t *exprs;
	BtreeControl_t *pBtreeTable=NULL;
	ExprsSymTerm_t tmpSym;
	ExprsTerm_t result;
	ExprsErrs_t err;
	int retV=0;
	
	memset(&ourCallbacks,0,sizeof(ExprsCallbacks_t));
	ourCallbacks.memAlloc = lclAlloc;
	ourCallbacks.memFree = lclFree;
	ourCallbacks.memArg = &memStats;
	ourCallbacks.msgOut = lclShow;
	memset(&btCallbacks,0,sizeof(BtreeCallbacks_t));
	btCallbacks.memAlloc = lclAlloc;
	btCallbacks.memFree = lclFree;
	btCallbacks.memArg = &memStats;
	btCallbacks.symCmp = btreeCmp;
	btCallbacks.symArg = &memStats;
	pBtreeTable = libBtreeInit(&btCallbacks);
	if ( !pBtreeTable )
	{
		fprintf(stderr, "libBtreeInit(): Out of memory\n");
		return 1;
	}
	ourCallbacks.symArg = pBtreeTable;
	ourCallbacks.symGet = getBtreeSym;
	ourCallbacks.symSet = setBtreeSym;
	pBtreeTable->pUser1 = &memStats;
	exprs = libExprsInit(&ourCallbacks, 0, 0, 0);
	if ( !exprs )
	{
		fprintf(stderr,"Out of memory doing libExprsInit()\n");
		retV = 1;
	}
	else
	{
		/* pre-populate the symbol table with some entries to play with */
		tmpSym.termType = EXPRS_TERM_INTEGER;
		tmpSym.term.s64 = 100;
		setBtreeSym(pBtreeTable,"foobar",&tmpSym);
		tmpSym.termType = EXPRS_TERM_INTEGER;
		tmpSym.term.s64 = 1000;
		setBtreeSym(pBtreeTable,"oneThousand",&tmpSym);
		tmpSym.termType = EXPRS_TERM_FLOAT;
		tmpSym.term.f64 = 3.14159;
		setBtreeSym(pBtreeTable,"pi",&tmpSym);
		libExprsSetVerbose(exprs,verbose);
		err = libExprsEval(exprs,expression,&result); 
		if ( err )
		{
			printf("Expression returned error: %s\n", libExprsGetErrorStr(err));
			retV = 1;
		}
		else
		{
			printf("Returned: type=%d, value=", result.termType );
			if ( result.termType == EXPRS_TERM_INTEGER )
				printf("%ld", result.term.s64);
			else if ( result.termType == EXPRS_TERM_FLOAT )
				printf("%g", result.term.f64);
			else if (    result.termType == EXPRS_TERM_STRING
					  || result.termType == EXPRS_TERM_SYMBOL
					)
			{
				char quote, *cp = result.term.string;
				unsigned char cc;
				quote = strchr(cp,'"') ? '\'':'"';
				printf("%c",quote);
				while ( (cc = *cp) )
				{
					if ( isprint(cc) )
						printf("%c",cc);
					else
						printf("\\x%02X",cc);
					++cp;
				}
				printf("%c", quote);
			}
			else 
				printf("(not integer, float, string or symbol)");
			printf("\n");
		}
	}
	if ( pBtreeTable )
		libBtreeDestroy(pBtreeTable,freeEntry,&memStats);
	return retV;
}

