/*
    exprs.h - part of a generic expression parser example
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

#ifndef _LIB_EXPRS_H_
#define _LIB_EXPRS_H_ (1)

#include <pthread.h>

#ifndef n_elts
#define n_elts(x) (int)(sizeof(x)/sizeof(x[0]))
#endif

/**
 * @brief Example of an generic expression evaluator.
 *
 * This subsystem includes the optional use of symbols
 * maintained by an external symbol table manager.
 *
 * Expression operators use the syntax of the 'C' language. With
 * that in mind, an expression term can be either a unary or a
 * binary. A unary means the operator applies only the immediate
 * term on the right (i.e. -4 or !foo, etc.) where a binary
 * operator applies to one term on the left and one on the right
 * (i.e. a+b, 5*4, etc.). The unary operators are: +,-,! and ~.
 * The binary operators in decreasing precedence are **, *, /,
 * %, +, -, ^, |, &, <<, >>, <, <=, >, >=, ==, !=, &&, || and =.
 *
 * NOTE: the operator '**' available in this expression parser
 * is not available in the 'C' language but it means the operand
 * on the left is raised to the power of the operand on the
 * right. I.e. x**y = the term x is raised to the power of y.
 * And x**0.5 = the square root of x.
 *
 * A subexpression is built and stored in a stack. After all the
 * subexpressions (terms delimited with parenthesis), they are
 * evaluated left to right with operator precedence as defined
 * by the 'C' language.
 *
 * @note The 'C' operator '++' would mean add a unary positive
 * (i.e. 5++6 means add a +6 to 5) and '--' would mean subtract
 * a unary negative (i.e. 5--6 means subtract a negative 6 from
 * 5.
 */

/** ExprsTermTypes_t - Ident of the various types of terms
 *  that may be parsed.
 **/
typedef enum
{
	/**
	 * If any of this is changed, be sure to also change the
	 * Precedence[] table found in lib_exprs.c to reflect the actual
	 * precedence.
	 **/
	EXPRS_TERM_NULL,
	EXPRS_TERM_LINK,	/* link to another stack */
	EXPRS_TERM_SYMBOL,	/* Symbol */
	EXPRS_TERM_FUNCTION,/* Function call (not supported yet) */
	EXPRS_TERM_STRING,	/* Text string (text delimited with quotes) */
	EXPRS_TERM_FLOAT,	/* floating point number */
	EXPRS_TERM_INTEGER,	/* integer number */
	EXPRS_TERM_PLUS,	/* + (unary term in this case) */
	EXPRS_TERM_MINUS,	/* - (unary term in this case) */
	EXPRS_TERM_COM,		/* ~ */
	EXPRS_TERM_NOT,		/* ! */
	EXPRS_TERM_POW,		/* ** */
	EXPRS_TERM_MUL,		/* * */
	EXPRS_TERM_DIV,		/* / */
	EXPRS_TERM_MOD,		/* % */
	EXPRS_TERM_ADD,		/* + (binary terms in this case) */
	EXPRS_TERM_SUB,		/* - (binary terms in this case) */
	EXPRS_TERM_ASL,		/* << */
	EXPRS_TERM_ASR,		/* >> */
	EXPRS_TERM_GT,		/* > */
	EXPRS_TERM_GE,		/* >= */
	EXPRS_TERM_LT,		/* < */
	EXPRS_TERM_LE,		/* <= */
	EXPRS_TERM_EQ,		/* == */
	EXPRS_TERM_NE,		/* != */
	EXPRS_TERM_AND,		/* & */
	EXPRS_TERM_XOR,		/* ^ */
	EXPRS_TERM_OR,		/* | */
	EXPRS_TERM_LAND,	/* && */
	EXPRS_TERM_LOR,		/* || */
	EXPRS_TERM_ASSIGN	/* = */
} ExprsTermTypes_t;

/** ExprsTerm_t - definition of the primitive contents of any
 *  individual term.
 **/
typedef struct
{
	ExprsTermTypes_t termType;	/* the type of the term */
	const char *chrPtr;			/* pointer to place in expression string where this term was found */
	/* The actual term is one of the following: */
	union
	{
		void *link;		/* Link to a different stack */
		char *string;	/* Pointer to malloc'd string if type is string */
		double f64;
		long s64;
		unsigned long u64;
		char oper[3];
	} term;
} ExprsTerm_t;

/** ExprsOpers_t - definition of the primitive operator
 *  terms.
 **/
typedef struct
{
	ExprsTermTypes_t mOperType;		/* Really just the operator types will be here not any generic term types */
	const char *chrPtr;				/* pointer in expression string where operator found */
	char mOper[2];					/* operator string */
} ExprsOpers_t;

/** ExprsStack_t - definition of a expression stack
 **/
typedef struct
{
	int mNumTerms;					/* Total number of terms in the stack */
	ExprsOpers_t *mOpers; 			/* stacked operators */
	ExprsTerm_t *mTerms;			/* stacked terms */
} ExprsStack_t;

/** ExprsErrs_t - definition of errors that may be returned
 *  by the various expression functions.
 **/
typedef enum
{
	EXPR_TERM_GOOD,
	EXPR_TERM_END,
	EXPR_TERM_BAD_OUT_OF_MEMORY,
	EXPR_TERM_BAD_NO_STRING_TERM,
	EXPR_TERM_BAD_SYMBOL_SYNTAX,
	EXPR_TERM_BAD_SYMBOL_TOO_LONG,
	EXPR_TERM_BAD_NUMBER,
	EXPR_TERM_BAD_UNARY,
	EXPR_TERM_BAD_OPER,
	EXPR_TERM_BAD_SYNTAX,
	EXPR_TERM_BAD_TOO_MANY_TERMS,
	EXPR_TERM_BAD_TOO_FEW_TERMS,
	EXPR_TERM_BAD_NO_TERMS,
	EXPR_TERM_BAD_NO_CLOSE,
	EXPR_TERM_BAD_UNSUPPORTED,
	EXPR_TERM_BAD_DIV_BY_0,
	EXPR_TERM_BAD_UNDEFINED_SYMBOL,
	EXPR_TERM_BAD_NO_SYMBOLS,
	EXPR_TERM_BAD_SYMBOL_TABLE_FULL,
	EXPR_TERM_BAD_LVALUE,
	EXPR_TERM_BAD_RVALUE,
	EXPR_TERM_BAD_PARAMETER,
	EXPR_TERM_BAD_NOLOCK,
	EXPR_TERM_BAD_NOUNLOCK
} ExprsErrs_t;

/** ExprsSymTermTypes_t - definition of the subset of types
 *  of terms stored in an external symbol table.
 **/
typedef enum
{
	EXPRS_SYM_TERM_STRING=EXPRS_TERM_STRING,	/* Text string */
	EXPRS_SYM_TERM_FLOAT=EXPRS_TERM_FLOAT,		/* 64 bit floating point number */
	EXPRS_SYM_TERM_INTEGER=EXPRS_TERM_INTEGER	/* 64 bit integer number */
} ExprsSymTermTypes_t;

/** ExprsSymTerm_t - definition of the primitive contents of any
 *  individual term as stored in an external symbol table. The
 *  valid types are a small subset of what the expression parser
 *  would use internally.
 **/
typedef struct
{
	ExprsSymTermTypes_t termType;	/* type of item this is */
	union
	{
		char *string;
		double f64;
		long s64;
	} term;
} ExprsSymTerm_t;

/** ExprsMsgSeverity_t - define the severity of messages that
 *  may be emitted by the parser. */
typedef enum
{
	EXPRS_SEVERITY_INFO,
	EXPRS_SEVERITY_WARN,
	EXPRS_SEVERITY_ERROR,
	EXPRS_SEVERITY_FATAL
} ExprsMsgSeverity_t;

/** ExprsCallbacks_t - define the pointers to functions to
 *  callback for various parser primitives.
 *
 *  @note The memory callbacks are used only to get memory for
 *  	  the parser's internal use. Both memAlloc and memFree
 *  	  must contain a pointer or both must be NULL. If set to
 *  	  NULL, default functions using malloc/free will be
 *  	  used.
 *
 *  @note The msgOut callback is used by the parser to emit
 *  	  informational and error messages. If msgOut is set to
 *  	  NULL, a default function using stdout and stderr will
 *  	  be used.
 *
 *  @note The symGet/symSet members must contain a pointer or
 *  	  both must be set to NULL. If NULL, symbols appearing
 *  	  in an expression results in a parse error.
 *
 **/
typedef struct
{
	void *(*memAlloc)(void *memArg, size_t size);	/*! Memory allocation callback */
	void (*memFree)(void *memArg, void *memPtr);	/*! Memory free callback */
	void *memArg;									/*! Argument to pass to above memory callbacks */
	void (*msgOut)(void *msgArg, ExprsMsgSeverity_t severity, const char *msg);	/*! Message (error and info) output callback */
	void *msgArg;									/*! Argument to pass to msgOut callback */
	ExprsErrs_t (*symGet)(void *symArg, const char *symName, ExprsSymTerm_t *symValue);	/*! Symbol value fetch callback */
	ExprsErrs_t (*symSet)(void *symArg, const char *symName, const ExprsSymTerm_t *symValue);	/*! Symbol value set callback */
	void *symArg;									/*! Argument to pass to symGet/symSet callbacks */
} ExprsCallbacks_t;

/** ExprsDef_t - definition of expression stack internal
 *  variables. With the exception of userArg1 and userArg2
 *  these struct members should be thought of as read-only.
 **/
typedef struct
{
	pthread_mutex_t mMutex;			/*! Used to lock updates to members of this structure */
	void *userArg1;					/*! can be used for any purpose */
	void *userArg2;					/*! can be used for any purpose */
	ExprsCallbacks_t mCallbacks;	/*! callbacks to be used by the parser */
	int mVerbose;					/*! verbose flags */
	ExprsStack_t *mStacks;			/*! Pointer to expression stacks */
	ExprsTerm_t *mTermPool;			/*! Pointer to pool of expression terms */
	int mNumAvailTerms;				/*! Total number of available terms in term pool */
	ExprsOpers_t *mOpersPool;		/*! Pointer to pool of operator terms */
	int mStringPoolUsed;			/*! Number of bytes assigned from string pool */
	int mStringPoolAvailable;		/*! Total bytes available int string pool */
	char *mStringPool;				/*! Pointer to string pool */
	int mNumStacks;					/*! Number of stacks in use */
	int mNumAvailStacks;			/*! Total number of stacks available */
	const char *mCurrPtr;			/*! Pointer to current place in expression string being processed */
	const char *mLineHead;			/*! Pointer to first character in expression string */
} ExprsDef_t;

/** libExprsInit - Initialize an expression parser.
 *
 *  At entry:
 * @param callbacks - pointer to list of various callbacks the
 *  				parser is to use.
 * @param maxStacks - set the maximum number of expression
 *  				stacks to configure (if 0, defaults to 32).
 * @param maxTerms - sets the term pool size which is the
 *  			   maximum number of expression terms that will
 *  			   appear in any individual stack (if 0,
 *  			   defaults to 32).
 * @param stringPoolSize - sets the size of the string pool (if
 *  					 0, defaults to 2048).
 *
 * At exit:
 * @return pointer to expression parser control struct or NULL
 *  	   if error (out of memory).
 *
 * @note The contents of the callbacks struct is copied into the
 *  	 ExprsDef_t struct. The struct pointed to by 'callbacks'
 *  	 does not need to remain in static memory.
 **/
extern ExprsDef_t *libExprsInit(const ExprsCallbacks_t *callbacks, int maxStacks, int maxTerms, int stringPoolSize);

/** libExprsDestroy - free all the previously allocated
 *  memory used by the expression parser.
 *
 *  At entry:
 *  @param exprs - pointer as returned from previous call to
 *  			 libExprsInit.
 *
 *  At exit:
 *  @return nothing. All the memory allocated for use by the
 *  		parser has been freed.
 **/
extern void libExprsDestroy(ExprsDef_t *exprs);

/** libExprsSetCallbacks - Alter the list of callbacks in use
 *  by the expression parser.
 *
 *  At entry:
 *  @param exprs - pointer to parser control as returned from
 *  			 libExprsInit().
 *  @param newPtr - pointer to new list of callbacks.
 *  @param oldPtrs - if not NULL, pointer to place into which
 *  			   the current pointers will be copied.
 *
 *  At exit:
 *  @return 0 on success, non-zero on error.
 *
 * @note The contents of newPtrs is copied into the struct
 *  	 pointed to by exprs. The struct pointed to by newPtrs
 *  	 does not need to be in static memory.
 **/
extern ExprsErrs_t libExprsSetCallbacks(ExprsDef_t *exprs, const ExprsCallbacks_t *newPtrs, ExprsCallbacks_t *oldPtrs);

/** libExprsSetVerbose - set the verbose flags.
 *
 *  At entry:
 *  @param exprs - pointer to parser control as returned from
 *  			 libExprsInit().
 *  @param newVal - verbose flags.
 *
 *  At exit:
 *  @return previously set verbose flags.
 **/
extern int libExprsSetVerbose(ExprsDef_t *exprs, int newVal);

/** libExprsGetErrorStr - Translate a parser error into a
 *  text string.
 *
 *  At entry:
 *  @param errCode - code returned from an expression parser
 *  			   function.
 *
 *  At exit:
 *  @return null terminated text string.
 **/
extern const char *libExprsGetErrorStr(ExprsErrs_t errCode);

/** libExprsEval - Evaluate an expression
 *
 *  At entry:
 *  @param exprs - pointer to expression parser control as
 *  			 returned from libExprsInit().
 *  @param text - null terminated text of expressino to
 *  			evaluate.
 *  @param returnTerm - pointer to place into which to deposit
 *  				  the result.
 *
 *  At exit:
 *  @return 0 on success, else error. On success, the place
 *  		pointed to by 'returnTerm' will have been set to the
 *  		result. The result may only be of type integer,
 *  		float or string. It will not return anything more
 *  		complex.
 **/
extern ExprsErrs_t libExprsEval(ExprsDef_t *exprs, const char *text, ExprsTerm_t *returnTerm);

#endif	/* _LIB_EXPRS_H_ */
