/*
    exprs_test_nos.c - simple test code for lib_exprs.[ch].
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

#include "lib_exprs.h"
#include "exprs_test_nos.h"

/**
 *  This example shows how to use the expression parser in a
 *  bare bones configuration. It lets the parser use its default
 *  functions for memory management and message display and uses
 *  no symbol management.
 *
 *  For an example of how to use it with symbols, see
 *  exprs_test_ht.c (uses hash table lib to handle symbols). For
 *  a complete example with custom memory management, symbols
 *  and message emit, see exprs_test_bt.c (uses AVL balanced
 *  tree lib to handle symbols).
 *
 **/

int exprsTestNoSym(int incs, const char *expression, unsigned long flags, int radix, int verbose)
{
	ExprsDef_t *exprs;
	ExprsTerm_t result;
	ExprsErrs_t err;
	int retV=0;
	
	exprs = libExprsInit(NULL, incs, incs);
	if ( !exprs )
	{
		fprintf(stderr,"Out of memory doing libExprsInit()\n");
		return 1;
	}
	libExprsSetVerbose(exprs,verbose,NULL);
	libExprsSetFlags(exprs,flags,NULL);
	libExprsSetRadix(exprs,radix,NULL);
	err = libExprsEval(exprs,expression,&result,0); 
	if ( err )
	{
		printf("Expression returned error %d: %s\n", err, libExprsGetErrorStr(err));
		retV = 1;
	}
	else
	{
		printf("Returned: type=%d, value=", result.termType );
		if ( result.termType == EXPRS_TERM_INTEGER )
			printf("%ld (0x%lX)", result.term.s64, result.term.u64);
		else if ( result.termType == EXPRS_TERM_FLOAT )
			printf("%g", result.term.f64);
		else if (    result.termType == EXPRS_TERM_STRING
				  || result.termType == EXPRS_TERM_SYMBOL
				)
		{
			char quote, *cp = libExprsStringPoolTop(exprs) + result.term.string;
			unsigned char cc;
			if ( (result.flags&EXPRS_TERM_FLAG_LOCAL_SYMBOL) )
				printf("(local)");
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
		if ( *exprs->mCurrPtr )
			printf("Left over text: '%s'\n", exprs->mCurrPtr);
	}
	libExprsDestroy(exprs);
	return retV;
}

