/*
    exprs_test_ht.c - simple test code for lib_exprs.[ch].
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

#include "lib_hashtbl.h"
#include "lib_exprs.h"
#include "exprs_test_ht.h"

/**
 *  This example shows how to use the expression parser with
 *  external symbol managment. It lets the parser use its
 *  default functions for memory management and message
 *  emitting.
 *
 *  For an example of how to use the parser in a bare bones
 *  configuration, see exprs_test_nos.c. For an example of using
 *  the parser with custom memory management, symbols and
 *  message emit, see exprs_test_bt.c (uses AVL balanced tree
 *  lib to handle symbols).
 *
 **/

/**
 * We make our own private definition of what constitues a
 * symbol table entry. The value member does need to be one
 * specified by the expression parser. Hence the member
 * ExprsSymTerm_t.
 **/
typedef struct
{
	const char *name;
	ExprsSymTerm_t value;
} SymbolTableEntry_t;

/**
 * hashIt - defines our custom hash function
 *
 * At entry:
 * @param symArg - pointer to symbol table root.
 * @param size - size of the hash table as specified at init.
 * @param entry - pointer to symbol table entry
 *
 * At exit:
 * @return integer with hash
 *
 * @note The hash lib requires we provide a Hash function. This
 *  	 since the hash table library has no specification of
 *  	 what is in a symbol table entry, what it is that needs
 *  	 hashing nor how to do it.
 **/
#ifndef HASH_STRING_SEED
#define HASH_STRING_SEED (11)
#endif
static unsigned int hashIt(void *symArg, int size, const HashEntry_t entry)
{
	const SymbolTableEntry_t *pData = (const SymbolTableEntry_t *)entry;
	int hashv=HASH_STRING_SEED;
	unsigned char cc;
	const char *key;

	/* This simple hash algorithm just accumulates the value of each character of the key times the hash table size plus the initial seed */
	key = pData->name;
	while ( (cc= *key++) )
	{
		hashv = hashv * size + cc;
	}
	/* Then returns that sum modulo the size of the hash table. */
	return hashv % size;
}

/**
 * hashCompare - defines our custom compare function
 *
 * At entry:
 * @param symArg - pointer to symbol table root
 * @param aa - pointer to symbol table entry
 * @param bb - pointer to symbol table entry
 *
 * At exit:
 * @return minus if aa < bb, 0 if aa==bb, plus if aa > bb.
 *
 * @note The hash lib requires we provide a compare function.
 *  This since the hash table library has no specification of
 *  what is in a symbol table entry or how to compare the two
 *  entries with one another.
 **/
static int hashCompare(void *symArg, const HashEntry_t aa, const HashEntry_t bb)
{
	const SymbolTableEntry_t *pAa = (const SymbolTableEntry_t *)aa;
	const SymbolTableEntry_t *pBb = (const SymbolTableEntry_t *)bb;
	return strcmp(pAa->name, pBb->name);
}

/** getHashSym - define our function to fetch an entry from
 *  the symbol table.
 *
 *  At entry:
 *  @param symArg - pointer to symbol table root
 *  @param name - null terminated symbol name string
 *  @param value - pointer of place into which to deposit
 *  			 result.
 *
 *  At exit:
 *  @return 0 on success, non-zero on error (as in no such
 *  		symbol).
 *
 *  @note The expression parser requires we provide both a
 *  	  symbol get and a symbol set function to fetch and
 *  	  store items from/into the symbol table respecively.
 *
 *  @note If the returned value is of type string, the string is
 *  	  copied into local expression parser memory. The parser
 *  	  does not keep pointers to externally provided strings
 *  	  in its local memory.
 **/
static ExprsErrs_t getHashSym(void *symArg, const char *name, ExprsSymTerm_t *value)
{
	HashRoot_t *pTable=(HashRoot_t *)symArg;
	SymbolTableEntry_t ent, *found;
	ent.name = name;
	if ( !libHashFind(pTable,(const HashEntry_t)&ent,(HashEntry_t *)&found,0) )
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
		case EXPRS_SYM_TERM_COMPLEX:
			value->term.complex = found->value.term.complex;
			break;
		}
		return EXPR_TERM_GOOD;
	}
	return EXPR_TERM_BAD_UNDEFINED_SYMBOL;
}

/**
* freeEntry - function to free the memory allocated for a symbol
* table entry.
*
* At entry:
* @param freeArg - caller provided argument
* @param entry - pointer to entry to free
*
* At exit:
* @return nothing. Memory for the name, string value if any and
*   	  the entry itself have been freed.
*/
static void freeEntry(void *freeArg, HashEntry_t entry)
{
	SymbolTableEntry_t *ent = (SymbolTableEntry_t *)entry;
	/* free the name string */
	free((void *)ent->name);
	/* if the value is a string, free it too */
	if ( ent->value.termType == EXPRS_SYM_TERM_STRING )
		free(ent->value.term.string);
	/* free the entry itself */
	free(ent);
}

/** setHashSym - define our function to insert/replace an entry
 *  			 in the symbol table.
 *
 *  At entry:
 *  @param symArg - pointer to symbol table root
 *  @param name   - null terminated symbol name string
 *  @param value  - pointer to value to install.
 *
 *  At exit:
 *  @return 0 on success, non-zero if error.
 *
 *  @note If this is to be used multi-threaded, then some care
 *  	  must be taken with regard to locking/unlocking
 *  	  references to the entries stored in the hash table.
 *  	  However, since this example code has no threads and it
 *  	  is completely "in charge" of all memory used by
 *  	  individual symbols, there is no need for locking. But
 *  	  just to demonstrate how one might do updates to
 *  	  individual entries in the symbol table, this function
 *  	  tries to reduce the number of malloc/frees that might
 *  	  otherwise be needed.
 **/
static ExprsErrs_t setHashSym(void *symArg, const char *name, const ExprsSymTerm_t *value)
{
	HashRoot_t *pTable=(HashRoot_t *)symArg;
	SymbolTableEntry_t tEnt, *ent, *found=NULL;
	char *tStr;
	int len;
	
	tEnt.name = name;
	/* first lookup the symbol to see if there is one already */
	if ( !libHashFind(pTable,(const HashEntry_t)&tEnt,(HashEntry_t *)&found,0) )
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
			tStr = (char *)malloc(len);
			if ( !tStr )
				return EXPR_TERM_BAD_OUT_OF_MEMORY;
			strncpy(tStr, value->term.string, len);
			found->value.term.string = tStr;
			break;
		default:
			return EXPR_TERM_BAD_UNSUPPORTED;
		}
		if ( oldStr )
			free(oldStr);
		found->value.termType = value->termType;
		return EXPR_TERM_GOOD;
	}
	/* No existing symbol table entry. */
	/* Malloc space for the name. */
	len = strlen(name)+1;
	if ( !(tStr = malloc(len)) )
		return EXPR_TERM_BAD_OUT_OF_MEMORY;
	/* Malloc a symbol table entry */
	if ( !(ent = malloc(sizeof(SymbolTableEntry_t))) )
	{
		free(tStr);	/* toss the space for the name string */
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
		if ( !(tStr = (char *)malloc(len)) )
		{
			freeEntry(NULL,(HashEntry_t)ent);
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
		freeEntry(NULL,(HashEntry_t)ent);
		return EXPR_TERM_BAD_UNDEFINED_SYMBOL;
	}
	if ( !libHashInsert(pTable,(const HashEntry_t)ent) )
		return EXPR_TERM_GOOD;
	freeEntry(NULL,(HashEntry_t)ent);
	return EXPR_TERM_BAD_UNDEFINED_SYMBOL;
}

/** tblDump - support function to show contents of symbol
 *  table.
 *
 *  At entry:
 *  @param  pArg - user provided pointer (not used in this case)
 *  @param hashIndex - index into hash table
 *  @param pHashEntry - entry at hashTable[hashIndex].
 *
 *  At exit:
 *  @return nothing
 *
 *  @note This function is called by the hashDump() function
 *  	  which is called at the conclusion of the test just to
 *  	  show what's left in the symbol table when all done
 *  	  testing.
 **/
static void tblDump(void* pArg, int hashIndex, const HashPrimitive_t *pHashEntry)
{
	char buf[512];
	size_t len=0;
	char *banner="";
	
	/* We just walk the tree for each element in the hash table. */
	/* This function will never be called with pHashEntry NULL */
	len = snprintf(buf,sizeof(buf),"%3d: ", hashIndex);
	while ( pHashEntry && len < sizeof(buf)-1 )
	{
		const SymbolTableEntry_t *ent = (const SymbolTableEntry_t *)pHashEntry->entry;
		len += snprintf(buf + len, sizeof(buf) - len, "%s{'%s',", banner,ent->name);
		switch (ent->value.termType)
		{
		case EXPRS_TERM_INTEGER:
			len += snprintf(buf + len, sizeof(buf) - len, "(int)%ld}", ent->value.term.s64);
			break;
		case EXPRS_TERM_FLOAT:
			len += snprintf(buf + len, sizeof(buf) - len, "(double)%g}", ent->value.term.f64);
			break;
		case EXPRS_TERM_STRING:
			len += snprintf(buf + len, sizeof(buf) - len, "(char)'%s'}", ent->value.term.string);
			break;
		default:
			len += snprintf(buf + len, sizeof(buf) - len, " UNDEFINED type %d}", ent->value.termType);
			break;
		}
		banner = "->";
		/* Each entry in the hash table is a linked list. So get the next one in the chain. */
		pHashEntry = pHashEntry->next;
	}
	printf("%s\n", buf);
}

/**
 * exprsTestHashTbl - function to test the expression parser
 * 					  with symbols stored in a hash table.
 *
 * At entry:
 * @param hashTblSize - number of top level hash entries
 * @param expression  - pointer to null terminated string
 *  				  containing expression to test.
 * @param verbose 	  - verbose flags
 * 
 * @return 0 on success, non-zero on error.
 **/

int exprsTestHashTbl(int hashTblSize, const char *expression, unsigned long flags, int radix, int verbose)
{
	ExprsCallbacks_t exprsCallbacks;
	HashCallbacks_t hashCallbacks;
	ExprsDef_t *exprs;
	HashRoot_t *pHashTable=NULL;
	ExprsSymTerm_t tmpSym;
	ExprsTerm_t result;
	ExprsErrs_t err;
	int retV=0;
	
	/* Clear the callbacks struct in preparation to use it to pass our function pointers */
	memset(&exprsCallbacks,0,sizeof(ExprsCallbacks_t));
	memset(&hashCallbacks,0,sizeof(HashCallbacks_t));
	hashCallbacks.symCmp = hashCompare;
	hashCallbacks.symHash = hashIt;
	/* Create the hash table using all its default configurations */
	pHashTable = libHashInit(hashTblSize, &hashCallbacks);
	if ( !pHashTable )
		return 1;
	exprsCallbacks.symGet = getHashSym;
	exprsCallbacks.symSet = setHashSym;
	exprsCallbacks.symArg = pHashTable;
	exprs = libExprsInit(&exprsCallbacks, 0, 0, 0);
	if ( !exprs )
	{
		libHashDestroy(pHashTable,freeEntry,NULL);
		return 1;
	}
	/* pre-populate the symbol table with some entries to play with */
	tmpSym.termType = EXPRS_TERM_INTEGER;
	tmpSym.term.s64 = 100;
	setHashSym(pHashTable,"foobar",&tmpSym);
	tmpSym.termType = EXPRS_TERM_INTEGER;
	tmpSym.term.s64 = 1000;
	setHashSym(pHashTable,"oneThousand",&tmpSym);
	tmpSym.termType = EXPRS_TERM_FLOAT;
	tmpSym.term.f64 = 3.14159;
	setHashSym(pHashTable,"pi",&tmpSym);
	libExprsSetVerbose(exprs,verbose,NULL);
	libExprsSetFlags(exprs,flags,NULL);
	libExprsSetRadix(exprs,radix,NULL);
	err = libExprsEval(exprs,expression,&result,0); 
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
	printf("Symbols left in the hash table:\n");
	libHashDump(pHashTable,tblDump,NULL);
	libHashDestroy(pHashTable,freeEntry,NULL);
	return retV;
}

