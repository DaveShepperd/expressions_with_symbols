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
#include "exprs_test_walk.h"

/**
 *  This example shows how to use the expression parser in a
 *  bare bones configuration. It lets the parser use its default
 *  functions for memory management and message display.
 *
 *  For an example of how to use it with symbols, see
 *  exprs_test_ht.c (uses hash table lib to handle symbols). For
 *  a complete example with custom memory management, symbols
 *  and message emit, see exprs_test_bt.c (uses AVL balanced
 *  tree lib to handle symbols).
 *
 **/

static ExprsErrs_t getSym(void *symArg, const char *name, ExprsSymTerm_t *value)
{
	/* just a dummy function for this test */
	return EXPR_TERM_GOOD;
}

static void printString(const char *str)
{
	char quote;
	const char *cp = str;
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
	printf("%c\n", quote);
}

static ExprsErrs_t showTerm(ExprsDef_t *exprs, const ExprsTerm_t *term)
{
	printf("Returned: type=%d, value=", term->termType );
	switch (term->termType)
	{
	case EXPRS_TERM_INTEGER:
		printf("Int: %ld (0x%lX)\n", term->term.s64, term->term.u64);
		break;
	case EXPRS_TERM_FLOAT:
		printf("Float: %g\n", term->term.f64);
		break;
	case EXPRS_TERM_STRING:
		printf("String:");
		printString(libExprsStringPoolTop(exprs) + term->term.string);
		break;
	case EXPRS_TERM_SYMBOL:
		printf("Symbol:");
		if ( (term->flags&EXPRS_TERM_FLAG_LOCAL_SYMBOL) )
			printf("(local)");
		printString(libExprsStringPoolTop(exprs) + term->term.string);
		break;
	case EXPRS_TERM_FUNCTION:/* Function call (not supported yet) */
		printf("Function:");
		printString(libExprsStringPoolTop(exprs) + term->term.string);
		break;
	case EXPRS_TERM_PLUS:	/* + (unary term in this case) */
		printf("Operator: Unary +\n");
		break;
	case EXPRS_TERM_MINUS:	/* - (unary term in this case) */
		printf("Operator: Unary -\n");
		break;
	case EXPRS_TERM_HIGH_BYTE: /* high byte */
		printf("Operator: high byte\n");
		break;
	case EXPRS_TERM_LOW_BYTE:/* low byte */
		printf("Operator: low byte\n");
		break;
	case EXPRS_TERM_XCHG:	/* exchange bytes */
		printf("Operator: exchange bytes\n");
		break;
	case EXPRS_TERM_POW:		/* ** */
	case EXPRS_TERM_MUL:		/* * */
	case EXPRS_TERM_DIV:		/* / */
	case EXPRS_TERM_MOD:		/* % */
	case EXPRS_TERM_ADD:		/* + (binary terms in this case) */
	case EXPRS_TERM_SUB:		/* - (binary terms in this case) */
	case EXPRS_TERM_ASL:		/* << */
	case EXPRS_TERM_ASR:		/* >> */
	case EXPRS_TERM_GT:		/* > */
	case EXPRS_TERM_GE:		/* >= */
	case EXPRS_TERM_LT:		/* < */
	case EXPRS_TERM_LE:		/* <= */
	case EXPRS_TERM_EQ:		/* == */
	case EXPRS_TERM_NE:		/* != */
	case EXPRS_TERM_AND:		/* & */
	case EXPRS_TERM_XOR:		/* ^ */
	case EXPRS_TERM_OR:		/* | */
	case EXPRS_TERM_LAND:	/* && */
	case EXPRS_TERM_LOR:		/* || */
	case EXPRS_TERM_COM:		/* ~ */
	case EXPRS_TERM_NOT:		/* ! */
	case EXPRS_TERM_ASSIGN:	/* = */
		printf("Operator: %s\n", term->term.oper);
		break;
	default:
		printf("%d is not integer, float, string or symbol\n", term->termType);
	}
	return EXPR_TERM_GOOD;
}

int exprsTestWalk(int incs, const char *expression, unsigned long flags, int radix, int verbose)
{
	ExprsDef_t *exprs;
	ExprsCallbacks_t exprsCallbacks;
	ExprsErrs_t err;
	int retV=0;
	
	memset(&exprsCallbacks,0,sizeof(ExprsCallbacks_t));
	exprsCallbacks.symGet = getSym;	/* dummy function (won't be called) */
	
	exprs = libExprsInit(&exprsCallbacks, incs, incs);
	if ( !exprs )
	{
		fprintf(stderr,"Out of memory doing libExprsInit()\n");
		return 1;
	}
	libExprsSetVerbose(exprs,verbose,NULL);
	libExprsSetFlags(exprs,flags,NULL);
	libExprsSetRadix(exprs,radix,NULL);
	err = libExprsParseToRPN(exprs,expression,0); 
	if ( err > EXPR_TERM_END )
	{
		printf("libExprsParseToRPN() returned error %d: %s\nLeft over text: %s\n", err, libExprsGetErrorStr(err), exprs->mCurrPtr);
		retV = 1;
	}
	else
	{
		ExprsStack_t *stack;
		
		if ( *exprs->mCurrPtr )
			printf("Left over text: '%s'\n", exprs->mCurrPtr);
		stack = &exprs->mStack;
		printf("  Stack nTerms=%d\n", stack->mTermsPool.mNumUsed);
		err = libExprsWalkParsedStack(exprs, showTerm, 0);
		if ( err )
		{
			printf("libExprsWalkParsedStack() returned error %d: %s\n", err, libExprsGetErrorStr(err));
			retV = 1;
		}
	}
	libExprsDestroy(exprs);
	return retV;
}

