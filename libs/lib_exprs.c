/*
    lib_exprs.c - generic expression parser example
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
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <search.h>
#include <errno.h>
#include "lib_exprs.h"

/* Define the character classes */
/* WARNING: Don't alter the order of the following items */

#define CC_ALP  0		/* alphabetic (A-Z,a-z) */
#define CC_NUM	1		/* numeric (0-9) */
#define CC_DOT	2		/* period (.) */
#define CC_OPER	3		/* expression operator character */
#define CC_PCX  4		/* any other printing character */
#define CC_WS   5		/* white space */
#define CC_EOL  6		/* end of line */

/* define the character types */

#define CT_EOL 	 0x0001		/* EOL */
#define CT_WS 	 0x0002		/* White space (space or tab) */
#define CT_COM 	 0x0004		/* comma */
#define CT_DOT 	 0x0008		/* period */
#define CT_PCX 	 0x0010		/* printing character */
#define CT_NUM 	 0x0020		/* numeric (0-9) */
#define CT_ALP 	 0x0040		/* just A-Z and a-z */
#define CT_XALP  0x0080		/* dollar ($) and underscore (_) */
#define CT_EALP (CT_ALP|CT_XALP) /* just A-Z, a-z, dollar ($) and underscore (_) */
#define CT_SMC 	 0x0100		/* semi-colon */
#define CT_BOP   0x0200		/* binary operator (legal between terms) */
#define CT_UOP   0x0400		/* unary operator (legal starting a term) */
#define CT_OPER (CT_BOP|CT_UOP)	/* An expression operator */
#define CT_HEX  (0x0800|CT_ALP)	/* hex digit */
#define CT_QUO	(0x1000)	/* quote */
#define CT_BSL	(0x2000)	/* backslash */

#define EOL 	CT_EOL		/* EOL */
#define COM 	CT_COM		/* COMMA */
#define DOT 	CT_DOT		/* period */
#define WS		CT_WS		/* White space */
#define PCX 	CT_PCX		/* PRINTING CHARACTER */
#define NUM 	CT_NUM		/* NUMERIC */
#define ALP 	CT_ALP		/* ALPHA */
#define XALP 	CT_XALP		/* DOLLAR, UNDERSCORE */
#define LC		CT_ALP		/* LOWER CASE ALPHA */
#define SMC 	CT_SMC		/* SEMI-COLON */
#define EXP		CT_OPER		/* expression operator */
#define HEX		CT_HEX		/* hex digit */
#define LHEX	CT_HEX		/* lowercase hex digit */
#define BOP		CT_BOP		/* binary operator */
#define UOP		CT_UOP		/* unary operator */
#define QUO		CT_QUO		/* quote character */
#define BSL		CT_BSL		/* backslash */

static unsigned short cttblNormal[] =
{
    EOL, EOL, EOL, EOL, EOL, EOL, EOL, EOL,  /* NUL,SOH,STX,ETX,EOT,ENQ,ACK,BEL */
    EOL,  WS, EOL, EOL, EOL, EOL, EOL, EOL,  /* BS,HT,LF,VT,FF,CR,SO,SI */
    EOL, EOL, EOL, EOL, EOL, EOL, EOL, EOL,  /* DLE,DC1,DC2,DC3,DC4,NAK,SYN,ETB */
    EOL, EOL, EOL, EOL, EOL, EOL, EOL, EOL,  /* CAN,EM,SUB,ESC,FS,GS,RS,US */

    WS , EXP, QUO, PCX,XALP, EXP, BOP, QUO,  /*   ! " # $ % & ' */
    UOP, UOP, BOP, EXP, COM, EXP, DOT, BOP,  /* ( ) * + , - . / */
    NUM, NUM, NUM, NUM, NUM, NUM, NUM, NUM,  /* 0 1 2 3 4 5 6 7 */
    NUM, NUM, PCX, SMC, BOP, BOP, BOP, PCX,  /* 8 9 : ; < = > ? */

    PCX, HEX, HEX, HEX, HEX, HEX, HEX, ALP,  /* @ A B C D E F G */
    ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP,  /* H I J K L M N O */
    ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP,  /* P Q R S T U V W */
    ALP, ALP, ALP, PCX, BSL, PCX, EXP,XALP,  /* X Y Z [ \ ] ^ _ */

    QUO,LHEX,LHEX,LHEX,LHEX,LHEX,LHEX, LC ,  /* ` a b c d e f g */
    LC , LC , LC , LC , LC , LC , LC , LC ,  /* h i j k l m n o */
    LC , LC , LC , LC , LC , LC , LC , LC ,  /* p q r s t u v w */
    LC , LC , LC , PCX, BOP, PCX, UOP, EOL   /* x y z { | } ~ DEL */
};

static unsigned short cttblSpecial[] =
{
    EOL, EOL, EOL, EOL, EOL, EOL, EOL, EOL,  /* NUL,SOH,STX,ETX,EOT,ENQ,ACK,BEL */
    EOL,  WS, EOL, EOL, EOL, EOL, EOL, EOL,  /* BS,HT,LF,VT,FF,CR,SO,SI */
    EOL, EOL, EOL, EOL, EOL, EOL, EOL, EOL,  /* DLE,DC1,DC2,DC3,DC4,NAK,SYN,ETB */
    EOL, EOL, EOL, EOL, EOL, EOL, EOL, EOL,  /* CAN,EM,SUB,ESC,FS,GS,RS,US */

    WS , EXP, QUO, PCX,XALP, EXP, BOP, QUO,  /*   ! " # $ % & ' */
    UOP, UOP, BOP, EXP, COM, EXP, DOT, BOP,  /* ( ) * + , - . / */
    NUM, NUM, NUM, NUM, NUM, NUM, NUM, NUM,  /* 0 1 2 3 4 5 6 7 */
    NUM, NUM, PCX, SMC, UOP, BOP, UOP, BOP,  /* 8 9 : ; < = > ? */

    PCX, HEX, HEX, HEX, HEX, HEX, HEX, ALP,  /* @ A B C D E F G */
    ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP,  /* H I J K L M N O */
    ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP,  /* P Q R S T U V W */
    ALP, ALP, ALP, PCX, BSL, PCX, EXP,XALP,  /* X Y Z [ \ ] ^ _ */

    QUO,LHEX,LHEX,LHEX,LHEX,LHEX,LHEX, LC ,  /* ` a b c d e f g */
    LC , LC , LC , LC , LC , LC , LC , LC ,  /* h i j k l m n o */
    LC , LC , LC , LC , LC , LC , LC , LC ,  /* p q r s t u v w */
    LC , LC , LC , BOP, BOP, BOP, UOP, EOL   /* x y z { | } ~ DEL */
};

typedef unsigned char bool;
enum
{
	false,
	true
};
/* This establishes the precedence of the various operators. */
static const ExprsPrecedence_t PrecedenceNormal[] =
{
	10,	/* EXPRS_TERM_NULL, */
	10,	/* EXPRS_TERM_LINK,  *//* link to another stack */
	10,	/* EXPRS_TERM_SYMBOL, *//* Symbol */
	10,	/* EXPRS_TERM_FUNCTION, *//* Function call */
	10,	/* EXPRS_TERM_STRING, *//* Text string */
	10,	/* EXPRS_TERM_FLOAT, *//* 64 bit floating point number */
	10,	/* EXPRS_TERM_INTEGER, *//* 64 bit integer number */
	9,	/* EXPRS_TERM_PLUS,  *//* + (unary term) */
	9,	/* EXPRS_TERM_MINUS, *//* - (unary term) */
	8,	/* EXPRS_TERM_COM,	 *//* ~ */
	8,	/* EXPRS_TERM_NOT,	 *//* ! */
	8,	/* EXPRS_TERM_HIGH_BYTE *//* */
	8,	/* EXPRS_TERM_LOW_BYTE *//* */
	8,	/* EXPRS_TERM_XCHG	 *//*    */
	7,	/* EXPRS_TERM_POW,	 *//* ** */
	6,	/* EXPRS_TERM_MUL,	 *//* * */
	6,	/* EXPRS_TERM_DIV,	 *//* / */
	6,	/* EXPRS_TERM_MOD,	 *//* % */
	5,	/* EXPRS_TERM_ADD,	 *//* + (binary terms) */
	5,	/* EXPRS_TERM_SUB,	 *//* - (binary terms) */
	4,	/* EXPRS_TERM_ASL,	 *//* << */
	4,	/* EXPRS_TERM_ASR,	 *//* >> */
	3,	/* EXPRS_TERM_GT,	 *//* > */
	3,	/* EXPRS_TERM_GE,	 *//* >= */
	3,	/* EXPRS_TERM_LT,	 *//* < */
	3,	/* EXPRS_TERM_LE,	 *//* <= */
	3,	/* EXPRS_TERM_EQ,	 *//* == */
	3,	/* EXPRS_TERM_NE,	 *//* != */
	2,	/* EXPRS_TERM_AND,	 *//* & */
	2,	/* EXPRS_TERM_XOR,	 *//* ^ */
	2,	/* EXPRS_TERM_OR,	 *//* | */
	1,	/* EXPRS_TERM_LAND,	 *//* && */
	1,	/* EXPRS_TERM_LOR,	 *//* || */
	0,	/* EXPRS_TERM_ASSIGN,*//* = */
};

static const ExprsPrecedence_t PrecedenceNone[] =
{
	10,	/* EXPRS_TERM_NULL, */
	10,	/* EXPRS_TERM_LINK,  *//* link to another stack */
	10,	/* EXPRS_TERM_SYMBOL, *//* Symbol */
	10,	/* EXPRS_TERM_FUNCTION, *//* Function call */
	10,	/* EXPRS_TERM_STRING, *//* Text string */
	10,	/* EXPRS_TERM_FLOAT, *//* 64 bit floating point number */
	10,	/* EXPRS_TERM_INTEGER, *//* 64 bit integer number */
	9,	/* EXPRS_TERM_PLUS,  *//* + (unary term) */
	9,	/* EXPRS_TERM_MINUS, *//* - (unary term) */
	8,	/* EXPRS_TERM_COM,	 *//* ~ */
	8,	/* EXPRS_TERM_NOT,	 *//* ! */
	8,	/* EXPRS_TERM_HIGH_BYTE *//* */
	8,	/* EXPRS_TERM_LOW_BYTE *//* */
	8,	/* EXPRS_TERM_XCHG	 *//*    */
	7,	/* EXPRS_TERM_POW,	 *//* ** */
	7,	/* EXPRS_TERM_MUL,	 *//* * */
	7,	/* EXPRS_TERM_DIV,	 *//* / */
	7,	/* EXPRS_TERM_MOD,	 *//* % */
	7,	/* EXPRS_TERM_ADD,	 *//* + (binary terms) */
	7,	/* EXPRS_TERM_SUB,	 *//* - (binary terms) */
	7,	/* EXPRS_TERM_ASL,	 *//* << */
	7,	/* EXPRS_TERM_ASR,	 *//* >> */
	7,	/* EXPRS_TERM_GT,	 *//* > */
	7,	/* EXPRS_TERM_GE,	 *//* >= */
	7,	/* EXPRS_TERM_LT,	 *//* < */
	7,	/* EXPRS_TERM_LE,	 *//* <= */
	7,	/* EXPRS_TERM_EQ,	 *//* == */
	7,	/* EXPRS_TERM_NE,	 *//* != */
	7,	/* EXPRS_TERM_AND,	 *//* & */
	7,	/* EXPRS_TERM_XOR,	 *//* ^ */
	7,	/* EXPRS_TERM_OR,	 *//* | */
	7,	/* EXPRS_TERM_LAND,	 *//* && */
	7,	/* EXPRS_TERM_LOR,	 *//* || */
	0,	/* EXPRS_TERM_ASSIGN,*//* = */
};

static char *showTermType(ExprsDef_t *exprs, ExprsStack_t *sPtr, ExprsTerm_t *term, char *dst, int dstLen)
{
	dst[0] = 0;
	switch (term->termType)
	{
		case EXPRS_TERM_NULL:
			snprintf(dst,dstLen,"%s","NULL");
			break;
		case EXPRS_TERM_LINK:
			snprintf(dst,dstLen,"@(stack%ld)",sPtr-exprs->mStacks);
			break;
		case EXPRS_TERM_SYMBOL:
			snprintf(dst, dstLen, "Symbol: %s", term->term.string);
			break;
		case EXPRS_TERM_FUNCTION:
			snprintf(dst, dstLen, "Function: %s()", term->term.string);
			break;
		case EXPRS_TERM_STRING:
			snprintf(dst, dstLen, "String: '%s'", term->term.string);
			break;
		case EXPRS_TERM_FLOAT:
			snprintf(dst, dstLen, "FLOAT: '%g'", term->term.f64);
			break;
		case EXPRS_TERM_INTEGER:
			snprintf(dst, dstLen, "Integer: %ld", term->term.s64);
			break;
		case EXPRS_TERM_PLUS:
		case EXPRS_TERM_MINUS:
		case EXPRS_TERM_COM:
		case EXPRS_TERM_NOT:
		case EXPRS_TERM_HIGH_BYTE:
		case EXPRS_TERM_LOW_BYTE:
		case EXPRS_TERM_XCHG:
		case EXPRS_TERM_POW:
		case EXPRS_TERM_MUL:
		case EXPRS_TERM_DIV:
		case EXPRS_TERM_MOD:
		case EXPRS_TERM_ADD:
		case EXPRS_TERM_SUB:
		case EXPRS_TERM_ASL:
		case EXPRS_TERM_ASR:
		case EXPRS_TERM_GT:
		case EXPRS_TERM_GE:
		case EXPRS_TERM_LT:
		case EXPRS_TERM_LE:
		case EXPRS_TERM_EQ:
		case EXPRS_TERM_NE:
		case EXPRS_TERM_AND:
		case EXPRS_TERM_XOR:
		case EXPRS_TERM_OR:
		case EXPRS_TERM_LAND:
		case EXPRS_TERM_LOR:
			snprintf(dst, dstLen, "Operator: %s (precedence %d)", term->term.oper, exprs->precedencePtr[term->termType]);
			break;
		case EXPRS_TERM_ASSIGN:
			snprintf(dst, dstLen, "Assignment: %s", term->term.oper);
			break;
	}
	return dst;
}

static void showMsg(ExprsDef_t *exprs, ExprsMsgSeverity_t severity, const char *msg )
{
	exprs->mCallbacks.msgOut(exprs->mCallbacks.msgArg,severity,msg);
}

static char *getStringMem(ExprsDef_t *exprs, int amount)
{
	char *ans=NULL;
	if ( amount < exprs->mStringPoolAvailable-exprs->mStringPoolUsed )
	{
		ans = exprs->mStringPool+exprs->mStringPoolUsed;
		exprs->mStringPoolUsed += amount;
	}
	return ans;
}

static ExprsStack_t *getNextStack(ExprsDef_t *exprs)
{
	ExprsStack_t *ans = NULL;
	if ( exprs->mNumStacks < exprs->mNumAvailStacks )
	{
		ans = exprs->mStacks + exprs->mNumStacks;
		++exprs->mNumStacks;
		ans->mNumTerms = 0;
		ans->mTerms[0].chrPtr = NULL;
		ans->mTerms[0].termType = EXPRS_TERM_NULL;
		ans->mTerms[0].term.s64 = 0;
	}
	return ans;
}

ExprsErrs_t libExprsSetVerbose(ExprsDef_t *exprs, unsigned int newVal, unsigned int *oldValP)
{
	ExprsErrs_t err;
	unsigned int oldVal;
	if ( (err=libExprsLock(exprs)) )
		return err;
	oldVal = exprs->mVerbose;
	exprs->mVerbose = newVal;
	libExprsUnlock(exprs);
	if ( oldValP )
		*oldValP = oldVal;
	return EXPR_TERM_GOOD;
}

ExprsErrs_t libExprsSetFlags(ExprsDef_t *exprs, unsigned long newVal, unsigned long *oldValP)
{
	ExprsErrs_t err;
	unsigned long oldVal;
	if ( (err=libExprsLock(exprs)) )
		return err;
	oldVal = exprs->mFlags;
	exprs->mFlags = newVal;
	libExprsUnlock(exprs);
	if ( oldValP )
		*oldValP = oldVal;
	return EXPR_TERM_GOOD;
}

ExprsErrs_t libExprsSetRadix(ExprsDef_t *exprs, int newVal, int *oldValP)
{
	ExprsErrs_t err;
	int oldVal;
	if ( (err=libExprsLock(exprs)) )
		return err;
	oldVal = exprs->mRadix;
	if ( newVal == 0 || newVal == 2 || newVal == 8 || newVal == 10 || newVal == 16 )
		exprs->mRadix = newVal;
	libExprsUnlock(exprs);
	if ( oldValP )
		*oldValP = oldVal;
	return EXPR_TERM_GOOD;
}

ExprsErrs_t libExprsSetOpenDelimiter(ExprsDef_t *exprs, char newVal, char *oldValP)
{
	ExprsErrs_t err;
	int oldVal;
	if ( (err=libExprsLock(exprs)) )
		return err;
	oldVal = exprs->mOpenDelimiter;
	libExprsUnlock(exprs);
	if ( oldValP )
		*oldValP = oldVal;
	return EXPR_TERM_GOOD;
}

ExprsErrs_t libExprsSetCloseDelimiter(ExprsDef_t *exprs, char newVal, char *oldValP)
{
	ExprsErrs_t err;
	int oldVal;
	if ( (err=libExprsLock(exprs)) )
		return err;
	oldVal = exprs->mOpenDelimiter;
	libExprsUnlock(exprs);
	if ( oldValP )
		*oldValP = oldVal;
	return EXPR_TERM_GOOD;
}

static void reset(ExprsDef_t *exprs)
{
	exprs->mNumStacks = 0;
	exprs->mStringPoolUsed = 0;
}

static void dumpStacks(ExprsDef_t *exprs)
{
	int ii, jj;
	ExprsStack_t *nPtr,*sPtr;
	char eBuf[512];
	int len;
	
	sPtr = exprs->mStacks;
	for (jj=0; jj < exprs->mNumStacks; ++jj, ++sPtr)
	{
		len = snprintf(eBuf,sizeof(eBuf),"Stack %3d, %3d %s ", jj, sPtr->mNumTerms, sPtr->mNumTerms == 1 ? "term: ":"terms:");
		for (ii=0; ii < sPtr->mNumTerms; ++ii )
		{
			switch (sPtr->mTerms[ii].termType)
			{
			case EXPRS_TERM_NULL:
				len += snprintf(eBuf+len, sizeof(eBuf)-len, " NULL" );
				break;
			case EXPRS_TERM_LINK:	/* link to another stack */
				nPtr = (ExprsStack_t *)sPtr->mTerms[ii].term.link;
				len += snprintf(eBuf+len, sizeof(eBuf)-len, " (@stack%ld)", nPtr-exprs->mStacks);
				break;
			case EXPRS_TERM_STRING:
				{
					unsigned char *src = (unsigned char *)sPtr->mTerms[ii].term.string;
					len += snprintf(eBuf+len, sizeof(eBuf)-len, " \"");
					while ( *src )
					{
						if ( isprint(*src) )
							len += snprintf(eBuf+len, sizeof(eBuf)-len, "%c",*src++);
						else
							len += snprintf(eBuf+len, sizeof(eBuf)-len, "\\x%02X",*src++);
					}
					len += snprintf(eBuf+len, sizeof(eBuf)-len, "\"");
				}
				break;
			case EXPRS_TERM_SYMBOL:
			case EXPRS_TERM_FUNCTION:
				len += snprintf(eBuf+len, sizeof(eBuf)-len, " %s", sPtr->mTerms[ii].term.string);
				break;
			case EXPRS_TERM_FLOAT:
				len += snprintf(eBuf+len, sizeof(eBuf)-len, " %g", sPtr->mTerms[ii].term.f64);
				break;
			case EXPRS_TERM_INTEGER:
				len += snprintf(eBuf+len, sizeof(eBuf)-len, " %ld", sPtr->mTerms[ii].term.s64);
				break;
			case EXPRS_TERM_PLUS:	/* + */
			case EXPRS_TERM_MINUS:	/* - */
			case EXPRS_TERM_COM:	/* ~ */
			case EXPRS_TERM_NOT:	/* ! */
			case EXPRS_TERM_HIGH_BYTE:	/* high byte */
			case EXPRS_TERM_LOW_BYTE:	/* low byte */
			case EXPRS_TERM_XCHG:	/* exchange bytes */
			case EXPRS_TERM_POW:	/* ** */
			case EXPRS_TERM_MUL:	/* * */
			case EXPRS_TERM_DIV:	/* / */
			case EXPRS_TERM_MOD:	/* % */
			case EXPRS_TERM_ADD:	/* + */
			case EXPRS_TERM_SUB:	/* - */
			case EXPRS_TERM_ASL:	/* << */
			case EXPRS_TERM_ASR:	/* >> */
			case EXPRS_TERM_GT:		/* > */
			case EXPRS_TERM_GE:		/* >= */
			case EXPRS_TERM_LT:		/* < */
			case EXPRS_TERM_LE:		/* <= */
			case EXPRS_TERM_EQ:		/* == */
			case EXPRS_TERM_NE:		/* != */
			case EXPRS_TERM_AND:	/* & */
			case EXPRS_TERM_XOR:	/* ^ */
			case EXPRS_TERM_OR:		/* | */
			case EXPRS_TERM_LAND:	/* && */
			case EXPRS_TERM_LOR:	/* || */
			case EXPRS_TERM_ASSIGN:	/* = */
				len += snprintf(eBuf+len, sizeof(eBuf)-len, " %s", sPtr->mTerms[ii].term.oper);
				break;
			}
		}
		len += snprintf(eBuf+len, sizeof(eBuf)-len, "\n");
		showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
	}
}

static ExprsErrs_t badSyntax(ExprsDef_t *exprs, unsigned short chMask, char cc, ExprsErrs_t retErr)
{
	char eBuf[256];
	snprintf(eBuf,sizeof(eBuf),"parseExpression(): Syntax error. cc='%c', chMask=0x%04X\n", isprint(cc)?cc:'.',chMask);
	showMsg(exprs,EXPRS_SEVERITY_ERROR,eBuf);
	/* Not something we know how to handle */
	return retErr;
}

static int storeInteger(ExprsDef_t *exprs,
					   ExprsStack_t *sPtr,
					   ExprsTerm_t *term,
					   const char *startP,
					   int topOper,
					   char suffix,
					   bool eatSuffix,
					   bool *lastTermWasOperatorP,
					   int radix,
					   const char *rdxName )
{
	char *sEndp;
	char eBuf[512];

	sEndp = NULL;
	term->term.s64 = strtoll(startP,&sEndp,radix);
	if ( !sEndp || (eatSuffix && toupper(*sEndp) != suffix) )
		return EXPR_TERM_BAD_NUMBER;
	if ( exprs->mVerbose )
	{
		snprintf(eBuf,sizeof(eBuf),"parseExpression().storeInteger(): Pushed to terms[%ld][%d] a %s Integer %ld. topOper=%d, flags=0x%lX, radix=%d.\n",
				 sPtr-exprs->mStacks, sPtr->mNumTerms, rdxName, term->term.s64, topOper, exprs->mFlags, radix);
		showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
	}
	if ( eatSuffix )
		++sEndp;
	exprs->mCurrPtr = sEndp;
	term->termType = EXPRS_TERM_INTEGER;
	*lastTermWasOperatorP = false;
	++sPtr->mNumTerms;
	return 0;
}

static ExprsErrs_t handleString(ExprsDef_t *exprs, ExprsStack_t *sPtr, ExprsTerm_t *term, char cc )
{
	int strLen;
	const char *endP;
	char *sEndP, quoteChar = cc, *dst;
	unsigned short chMask;
	char eBuf[512];
	
	if ( (exprs->mFlags & (EXPRS_FLG_NO_STRING|EXPRS_FLG_SINGLE_QUOTE)) )
	{
		if ( cc == '"' || !(exprs->mFlags&EXPRS_FLG_SINGLE_QUOTE))
			return EXPR_TERM_BAD_STRINGS_NOT_SUPPORTED;
		strLen = 1;		/* can only have a single character in this type of string */
	}
	else
	{
		endP = exprs->mCurrPtr + 1;       /* Skip starting quote char */
		/* find end of string */
		while ( (cc = *endP) )
		{
			chMask = cttblNormal[(int)cc];
			if ( (chMask&CT_EOL) )
			{
				return EXPR_TERM_BAD_NO_STRING_TERM;
			}
			++endP;
			if ( cc == '\\' )
				++endP;
			else if ( cc == quoteChar )
				break;
		}
		strLen = endP-exprs->mCurrPtr;
	}
	if ( !(term->term.string = getStringMem(exprs,strLen+1)) )
		return EXPR_TERM_BAD_OUT_OF_MEMORY;
	term->term.string[strLen] = 0;
	endP = exprs->mCurrPtr+1;		/* Skip starting quote char */
	dst = term->term.string;
	while ( (cc = *endP) && dst < term->term.string+strLen )
	{
		chMask = cttblNormal[(int)cc];
		if ( (chMask&CT_EOL) )
			break;
		if ( cc == '\\' )
		{
			unsigned char cvt;
			cc = endP[1];
			cvt = cc;
			if ( cc >= '0' && cc <= '7' )
			{
				cvt = cc-'0';
				cc = endP[2];
				if ( cc >= '0' && cc <= '7' )
				{
					cvt <<= 3;
					cvt |= cc-'0';
					cc = endP[3];
					if ( cc >= '0' && cc <= '7' )
					{
						cvt <<= 3;
						cvt |= cc-'0';
						++endP;
					}
					++endP;
				}
			}
			else if ( cc == 'x' )
			{
				sEndP=NULL;
				cvt = strtol(endP+1,&sEndP,16);
				if ( sEndP )
				{
					*dst++ = cvt;
					endP = sEndP;
					continue;
				}
				cvt = cc;
			}
			else if ( cc == 'n' )
				cvt = '\n';
			else if ( cc == 'r' )
				cvt = '\r';
			else if ( cc == 't' )
				cvt = '\t';
			else if ( cc == 'f' )
				cvt = '\f';
			else if ( cc == 'a' )
				cvt = '\a';
			else if ( cc == 'b' )
				cvt = '\b';
			else if ( cc == 'e' )
				cvt = '\033';
			else if ( cc == 't' )
				cvt = '\t';
			else if ( cc == 'v' )
				cvt = '\v';
			endP += 2;		/* eat both the '\' and the following character */
			*dst++ = cvt;
			continue;
		}
		if ( cc == quoteChar )
		{
			++endP;			/* Eat terminator */
			break;
		}
		*dst++ = *endP++;	/* copy the char */
	}
	*dst = 0;			/* null terminate the string */
	if ( (exprs->mFlags&EXPRS_FLG_SINGLE_QUOTE) )
	{
		/* optionally eat a trailing quote */
		if ( *endP == '\'' )
			++endP;
		cc = term->term.string[0];
		term->term.s64 = cc;
		term->termType = EXPRS_TERM_INTEGER;
		++sPtr->mNumTerms;
		exprs->mCurrPtr = endP;
		if ( exprs->mVerbose )
		{
			snprintf(eBuf,sizeof(eBuf),"parseExpression().handleString(): Pushed to terms[%ld][%d] a string character=0x%02lX ('%c')\n",
					 sPtr-exprs->mStacks,sPtr->mNumTerms,term->term.s64,isprint(cc)?cc:'.');
			showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
		}
		return EXPR_TERM_GOOD;
	}
	if ( exprs->mVerbose )
	{
		snprintf(eBuf,sizeof(eBuf),"parseExpression().handleString(): Pushed to terms[%ld][%d] a string='%s'\n", sPtr-exprs->mStacks,sPtr->mNumTerms,term->term.string);
		showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
	}
	term->termType = EXPRS_TERM_STRING;
	++sPtr->mNumTerms;
	exprs->mCurrPtr = endP;
	return (cc == quoteChar) ? EXPR_TERM_GOOD : EXPR_TERM_BAD_NO_STRING_TERM;
}


static ExprsErrs_t parseExpression(ExprsDef_t *exprs, int nest, bool lastTermWasOperator, ExprsStack_t *sPtr);

static ExprsErrs_t openNewStack(ExprsDef_t *exprs, int nest, ExprsTerm_t *term, ExprsStack_t *sPtr, bool *lastTermWasOperatorP)
{
	ExprsErrs_t err;
	ExprsStack_t *nPtr = getNextStack(exprs);
	if ( !nPtr )
		return EXPR_TERM_BAD_TOO_MANY_TERMS;
	term->termType = EXPRS_TERM_LINK;
	term->term.link = nPtr;
	++sPtr->mNumTerms;
	++exprs->mCurrPtr;
	err = parseExpression(exprs,nest+1,*lastTermWasOperatorP,nPtr); 
	if ( err )
		return err;
	*lastTermWasOperatorP = false;
	return EXPR_TERM_GOOD;
}

/** parseExpression - parse the text of the expression
 *  At entry:
 *  @param exprs - pointer to ExprsDef_t returned from
 *  			 libExprsInit()
 *  @param nest - the expression nest level
 *  @param lastTermWasOperator - indicates the last term
 *  						   encountered was an expression
 *  						   operator
 *  @param sPtr - pointer to stack into which to build
 *  			expression.
 *  At exit:
 *  @return - error code (0 == no error)
 *
 *  Theory of operation. The text is walked through skipping
 *  white space. As an item is encountered it is decoded as
 *  being a simple term (number or symbol) or an operator. The
 *  operator can be either a "unary" or a "binary". A unary
 *  operator applies directly to the term immediately folling it
 *  (i.e. a '+', '-', etc.) and a binary operator does something
 *  with two terms: the one previously encountered and the one
 *  following (i.e. a+b, etc). Simple terms are "pushed" onto a
 *  stack as they are encountered. Operators have an execution
 *  precedence assigned to them. As operators are encountered
 *  they are pushed onto a stack separate from the one simple
 *  terms use, however, before that's done, the new operator's
 *  precedence is compared against what is currently in the
 *  operator stack, and the entry is sorted appropriately.
 **/
static ExprsErrs_t parseExpression(ExprsDef_t *exprs, int nest, bool lastTermWasOperator, ExprsStack_t *sPtr)
{
	unsigned short chMask=CT_EOL, *chMaskPtr;
	char cc, *operPtr, *sEndp;
	const char *startP, *endP;
	ExprsTerm_t *term;
	ExprsErrs_t err, peRetV;
	int topOper = -1; /* top of operator stack */
	bool exitOut=false;
	char eBuf[512];
	int retV;
	
	if ( exprs->mVerbose )
	{
		snprintf(eBuf,sizeof(eBuf),"parseExpression(): Entry. nest=%d, stackIdx=%ld, numTerms=%d, expr='%s'\n", nest, sPtr - exprs->mStacks, sPtr->mNumTerms, exprs->mCurrPtr);
		showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
	}
	if ( sPtr->mNumTerms >= exprs->mNumAvailTerms )
		return EXPR_TERM_BAD_TOO_MANY_TERMS;
	chMaskPtr = (exprs->mFlags & EXPRS_FLG_SPECIAL_UNARY) ? cttblSpecial : cttblNormal;
	peRetV = EXPR_TERM_GOOD;	
	while ( 1 )
	{
		cc = *exprs->mCurrPtr;
		chMask = chMaskPtr[(int)cc];
		if ( (chMask&(CT_EOL|CT_COM|CT_SMC)) )
		{
			peRetV = EXPR_TERM_END;
			break;
		}
		if ( (chMask&CT_WS) )
		{
			/* Eat whitespace */
			++exprs->mCurrPtr;
			continue;
		}
		if ( !(chMask&(CT_EALP|CT_NUM|CT_OPER|CT_QUO)) )
		{
			return badSyntax(exprs, chMask, cc, EXPR_TERM_BAD_SYNTAX);
		}
		if ( exprs->mVerbose )
		{
			snprintf(eBuf,sizeof(eBuf),"parseExpression(): Processing terms[%ld][%d], cc=%c, chMask=0x%04X, lastWasOper=%d:  %s\n",
					 sPtr - exprs->mStacks, sPtr->mNumTerms, isprint(cc) ? cc : '.', chMask, lastTermWasOperator, exprs->mCurrPtr);
			showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
		}
		/*  */
		if ( (exprs->mFlags & EXPRS_FLG_WS_DELIMIT) && !lastTermWasOperator && sPtr->mNumTerms )
		{
			if ( (chMask & (CT_EALP | CT_NUM | CT_QUO)) || cc == exprs->mOpenDelimiter )
			{
				peRetV = EXPR_TERM_END;
				break;
			}
		}
		term = sPtr->mTerms + sPtr->mNumTerms;
		term->term.s64 = 0;
		term->termType = EXPRS_TERM_NULL;
		term->chrPtr = exprs->mCurrPtr;
		if ( (chMask & CT_QUO) )
		{
			/* possibly a quoted string */
			err = handleString(exprs,sPtr,term,cc);
			if ( err )
				return err;
			lastTermWasOperator = false;
			continue;
		}
		if ( (chMask&CT_EALP) )
		{
			size_t symLen;

			/* symbol */
			endP = exprs->mCurrPtr;
			while ( (cc = *endP) )
			{
				chMask = cttblNormal[(int)cc];
				if ( !(chMask&(CT_EALP|CT_NUM))  )
					break;
				++endP;
			}
			if ( !exprs->mCallbacks.symGet )
			{
				snprintf(eBuf,sizeof(eBuf),"parseExpression(): No symbol table established. Cannot handle symbols at or near %s.\n", exprs->mCurrPtr);
				showMsg(exprs,EXPRS_SEVERITY_ERROR,eBuf);
				return EXPR_TERM_BAD_NO_SYMBOLS;
			}
			symLen = endP - exprs->mCurrPtr;
			if ( !(term->term.string = getStringMem(exprs,symLen+1)) )
				return EXPR_TERM_BAD_OUT_OF_MEMORY;
			memcpy(term->term.string,exprs->mCurrPtr,symLen);
			term->term.string[symLen] = 0;
			term->termType = EXPRS_TERM_SYMBOL;
			++sPtr->mNumTerms;
			exprs->mCurrPtr = endP;
			lastTermWasOperator = false;
			continue;
		}
		if ( (chMask&CT_NUM) )
		{
			/* number */
			startP = exprs->mCurrPtr;
			if (cc == '0' )
			{
				if ( exprs->mCurrPtr[1] == 'x' || exprs->mCurrPtr[1] == 'X' )
				{
					startP += 2;
					retV = storeInteger(exprs,sPtr,term,startP,topOper,0,false,&lastTermWasOperator,16,"HEX");
					if ( retV )
						return badSyntax(exprs,chMask,cc,retV);
					continue;
				}
				if ( exprs->mCurrPtr[1] == 'o' || exprs->mCurrPtr[1] == 'O' )
				{
					startP += 2;
					retV = storeInteger(exprs,sPtr,term,startP,topOper,0,false,&lastTermWasOperator,8,"Octal");
					if ( retV )
						return badSyntax(exprs,chMask,cc,retV);
					continue;
				}
				if ( exprs->mRadix != 16 )
				{
					if ( exprs->mCurrPtr[1] == 'd' || exprs->mCurrPtr[1] == 'D' )
					{
						startP += 2;
						retV = storeInteger(exprs,sPtr,term,startP,topOper,0,false,&lastTermWasOperator,10,"Decimal");
						if ( retV )
							return badSyntax(exprs,chMask,cc,retV);
						continue;
					}
					if ( exprs->mCurrPtr[1] == 'b' || exprs->mCurrPtr[1] == 'B' )
					{
						startP += 2;
						retV = storeInteger(exprs,sPtr,term,startP,topOper,0,false,&lastTermWasOperator,2,"Binary");
						if ( retV )
							return badSyntax(exprs,chMask,cc,retV);
						continue;
					}
				}
			}
			sEndp = NULL;
			if ( (exprs->mFlags & (EXPRS_FLG_DOLLAR_HEX | EXPRS_FLG_H_HEX)) && exprs->mRadix != 16 )
			{
				/* try the number using hex to see if a 'H' or '$' follows */
				term->term.s64 = strtoul(startP,&sEndp,16);
				if ( !sEndp )
					return badSyntax(exprs,chMask,cc, EXPR_TERM_BAD_NUMBER);
				cc = toupper(*sEndp);
				if (    ((exprs->mFlags&EXPRS_FLG_DOLLAR_HEX) && cc == '$')
					 || ((exprs->mFlags&EXPRS_FLG_H_HEX) && cc == 'H')
				   )
				{
					/* yep, legit */
					retV = storeInteger(exprs, sPtr, term, startP, topOper, cc, true, &lastTermWasOperator, 16, "Hex");
					if ( retV )
						return badSyntax(exprs,chMask,cc,retV);
					continue;
				}
			}
 			if ( (exprs->mFlags & (EXPRS_FLG_O_OCTAL | EXPRS_FLG_Q_OCTAL)) && exprs->mRadix != 8 )
			{
				/* try the number using octal to see if a 'O' or 'Q' follows */
				term->term.s64 = strtoul(startP,&sEndp,8);
				if ( !sEndp )
					return badSyntax(exprs,chMask,cc, EXPR_TERM_BAD_NUMBER);
				cc = toupper(*sEndp);
				if (    ((exprs->mFlags&EXPRS_FLG_O_OCTAL) && cc == 'O')
					 || ((exprs->mFlags&EXPRS_FLG_Q_OCTAL) && cc == 'Q')
				   )
				{
					/* yep, legit */
					retV = storeInteger(exprs,sPtr,term,startP,topOper,cc,true,&lastTermWasOperator,8,"Octal");
					if ( retV )
						return badSyntax(exprs,chMask,cc,retV);
					continue;
				}
			}
			/* not a number with a 'H' or '$' or 'O' or 'Q' suffix */
			term->term.s64 = strtoul(startP, &sEndp, (exprs->mFlags & EXPRS_FLG_USE_RADIX) ? exprs->mRadix : 0);
			if ( !sEndp )
				return badSyntax(exprs,chMask,cc,EXPR_TERM_BAD_NUMBER);
			if ( (exprs->mFlags&EXPRS_FLG_NO_FLOAT) && *sEndp == '.' && exprs->mRadix != 10 )
			{
				retV = storeInteger(exprs,sPtr,term,startP,topOper,'.',true,&lastTermWasOperator,10,"Decimal");
				if ( retV )
					return badSyntax(exprs,chMask,cc,retV);
				continue;
			}
			endP = sEndp;
			if ( !(exprs->mFlags&EXPRS_FLG_NO_FLOAT) && (*endP == '.' || (*endP == 'e' || *endP == 'E')) )
			{
				sEndp = NULL;
				term->term.f64 = strtod(startP,&sEndp);
				if ( sEndp )
				{
					endP = sEndp;
					if ( exprs->mVerbose )
					{
						snprintf(eBuf,sizeof(eBuf),"parseExpression(): Pushed to terms[%ld][%d] a FLOAT %g. topOper=%d.\n", sPtr-exprs->mStacks, sPtr->mNumTerms, term->term.f64, topOper);
						showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
					}
					exprs->mCurrPtr = endP;
					term->termType = EXPRS_TERM_FLOAT;
					++sPtr->mNumTerms;
					lastTermWasOperator = false;
					continue;
				}
			}
			if ( exprs->mVerbose )
			{
				snprintf(eBuf,sizeof(eBuf),"parseExpression(): Pushed to terms[%ld][%d] a plain Integer %ld. topOper=%d. mFlags=0x%lX, mRadix=%d\n",
						 sPtr-exprs->mStacks, sPtr->mNumTerms, term->term.s64, topOper, exprs->mFlags, exprs->mRadix);
				showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
			}
			exprs->mCurrPtr = endP;
			term->termType = EXPRS_TERM_INTEGER;
			++sPtr->mNumTerms;
			lastTermWasOperator = false;
			continue;
		}
		if ( cc == exprs->mOpenDelimiter )
		{
			/* recurse stuff new expression into new stack */
			err = openNewStack(exprs, nest, term, sPtr, &lastTermWasOperator);
			if ( err )
				return err;
			continue;
		}
		if ( cc == exprs->mCloseDelimiter )
		{
			if ( nest < 1 )
			{
				snprintf(eBuf,sizeof(eBuf),"parseExpression(): Syntax error. cc='%c', chMask=0x%04X, nest=%d\n", isprint(cc)?cc:'.',chMask,nest);
				showMsg(exprs,EXPRS_SEVERITY_ERROR,eBuf);
				return EXPR_TERM_BAD_SYNTAX;
			}
			--nest;
			++exprs->mCurrPtr;
			exitOut = true;
			cc = 0;		/* force following switch() to do nothing */
		}
		operPtr = term->term.oper;
		switch (cc)
		{
		case 0:
			break;
		case '+':
			term->termType = lastTermWasOperator ? EXPRS_TERM_PLUS : EXPRS_TERM_ADD;
			*operPtr++ = cc;
			break;
		case '-':
			term->termType = lastTermWasOperator ? EXPRS_TERM_MINUS : EXPRS_TERM_SUB;
			*operPtr++ = cc;
			break;
		case '*':
			*operPtr++ = cc;
			if ( exprs->mCurrPtr[1] == '*' )
			{
				if ( (exprs->mFlags&EXPRS_FLG_NO_POWER) )
					return badSyntax(exprs,chMask,cc, EXPR_TERM_BAD_SYNTAX);
				term->termType = EXPRS_TERM_POW;
				*operPtr++ = cc;
				++exprs->mCurrPtr;
			}
			else
				term->termType = EXPRS_TERM_MUL;
			break;
		case '/':
			term->termType = EXPRS_TERM_DIV;
			*operPtr++ = cc;
			break;
		case '%':
			term->termType = EXPRS_TERM_MOD;
			*operPtr++ = cc;
			break;
		case '|':
			*operPtr++ = cc;
			if ( exprs->mCurrPtr[1] == '|' )
			{
				term->termType = EXPRS_TERM_LOR;
				*operPtr++ = cc;
				++exprs->mCurrPtr;
			}
			else
				term->termType = EXPRS_TERM_OR;
			break;
		case '&':
			*operPtr++ = cc;
			if ( exprs->mCurrPtr[1] == '&' )
			{
				term->termType = EXPRS_TERM_LAND;
				*operPtr++ = cc;
				++exprs->mCurrPtr;
			}
			else
				term->termType = EXPRS_TERM_AND;
			break;
		case '^':
			if ( (exprs->mFlags & EXPRS_FLG_SPECIAL_UNARY) )
			{
				cc = exprs->mCurrPtr[1];
				cc = toupper(cc);
				exprs->mCurrPtr += 2;
				startP = exprs->mCurrPtr;
				switch(cc)
				{
				case 'B':
					retV = storeInteger(exprs,sPtr,term,startP,topOper,0,false,&lastTermWasOperator,2,"Binary");
					if ( retV )
						return badSyntax(exprs,chMask,cc,retV);
					break;
				case 'C':
					term->termType = EXPRS_TERM_COM;
					*operPtr++ = '~';
					break;
				case 'D':
					retV = storeInteger(exprs,sPtr,term,startP,topOper,0,false,&lastTermWasOperator,10,"Decimal");
					if ( retV )
						return badSyntax(exprs,chMask,cc,retV);
					break;
				case 'X':
				case 'H':
					retV = storeInteger(exprs,sPtr,term,startP,topOper,0,false,&lastTermWasOperator,16,"Hex");
					if ( retV )
						return badSyntax(exprs,chMask,cc,retV);
					break;
				case 'O':
					retV = storeInteger(exprs,sPtr,term,startP,topOper,0,false,&lastTermWasOperator,8,"Octal");
					if ( retV )
						return badSyntax(exprs,chMask,cc,retV);
					break;
				case 'V':
					term->termType = EXPRS_TERM_LOW_BYTE;
					*operPtr++ = '^';
					*operPtr++ = 'V';
					break;
				case '~':
					term->termType = EXPRS_TERM_XCHG;
					*operPtr++ = '^';
					*operPtr++ = '~';
					break;
				case '^':
					term->termType = EXPRS_TERM_HIGH_BYTE;
					*operPtr++ = '^';
					*operPtr++ = '^';
					break;
				default:
					return badSyntax(exprs,chMask,cc, EXPR_TERM_BAD_SYNTAX);
				}
				break;
			}
			term->termType = EXPRS_TERM_XOR;
			*operPtr++ = cc;
			break;
		case '?':
			if ( (exprs->mFlags & EXPRS_FLG_SPECIAL_UNARY) )
			{
				term->termType = EXPRS_TERM_XOR;
				*operPtr++ = cc;
			}
			else
				badSyntax(exprs,chMask,cc,EXPR_TERM_BAD_SYNTAX);
			break;
		case '~':
			term->termType = EXPRS_TERM_COM;
			*operPtr++ = cc;
			break;
		case '!':
			if ( (exprs->mFlags & EXPRS_FLG_SPECIAL_UNARY) )
			{
				*operPtr++ = '|';
				term->termType = EXPRS_TERM_OR;
			}
			else
			{
				*operPtr++ = cc;
				if ( exprs->mCurrPtr[1] == '=' )
				{
					term->termType = EXPRS_TERM_NE;
					*operPtr++ = '=';
					++exprs->mCurrPtr;
				}
				else
					term->termType = EXPRS_TERM_NOT;
			}
			break;
		case '=':
			*operPtr++ = cc;
			if (exprs->mCurrPtr[1] == '=')
			{
				if ( (exprs->mFlags & EXPRS_FLG_NO_LOGICALS) )
					return EXPR_TERM_BAD_SYNTAX;
				term->termType = EXPRS_TERM_EQ;
				*operPtr++ = cc;
				++exprs->mCurrPtr;
			}
			else
			{
				if ( (exprs->mFlags & EXPRS_FLG_NO_ASSIGNMENT) )
					return EXPR_TERM_BAD_SYNTAX;
				term->termType = EXPRS_TERM_ASSIGN;
			}
			break;
		case '<':
			*operPtr++ = cc;
			if ( (exprs->mFlags & EXPRS_FLG_NO_LOGICALS) )
				return EXPR_TERM_BAD_SYNTAX;
			if ( exprs->mCurrPtr[1] == '<' )
			{
				term->termType = EXPRS_TERM_ASL;
				*operPtr++ = cc;
				++exprs->mCurrPtr;
			}
			else if ( exprs->mCurrPtr[1] == '=' )
			{
				term->termType = EXPRS_TERM_LE;
				*operPtr++ = '=';
				++exprs->mCurrPtr;
			}
			else
				term->termType = EXPRS_TERM_LT;
			break;
		case '{':
			if ( !(exprs->mFlags & EXPRS_FLG_SPECIAL_UNARY) )
				return badSyntax(exprs,chMask,cc, EXPR_TERM_BAD_SYNTAX);
			term->termType = EXPRS_TERM_ASL;
			*operPtr++ = '<';
			break;
		case '}':
			if ( !(exprs->mFlags & EXPRS_FLG_SPECIAL_UNARY) )
				return badSyntax(exprs,chMask,cc, EXPR_TERM_BAD_SYNTAX);
			term->termType = EXPRS_TERM_ASR;
			*operPtr++ = '>';
			break;
		case '>':
			*operPtr++ = cc;
			if ( (exprs->mFlags & EXPRS_FLG_NO_LOGICALS) )
				return badSyntax(exprs,chMask,cc, EXPR_TERM_BAD_SYNTAX);
			if ( exprs->mCurrPtr[1] == '>' )
			{
				term->termType = EXPRS_TERM_ASR;
				*operPtr++ = cc;
				++exprs->mCurrPtr;
			}
			else if ( exprs->mCurrPtr[1] == '=' )
			{
				term->termType = EXPRS_TERM_GE;
				*operPtr++ = '=';
				++exprs->mCurrPtr;
			}
			else
				term->termType = EXPRS_TERM_GT;
			break;
		default:
			if ( exitOut )
				break;
			return badSyntax(exprs, chMask, cc, EXPR_TERM_BAD_SYNTAX);
		}
		if ( exitOut )
			break;
		if ( operPtr != term->term.oper )
		{
			ExprsTerm_t savTerm = *term;
			ExprsPrecedence_t oldPrecedence,currPrecedence = exprs->precedencePtr[savTerm.termType];
			*operPtr = 0;	/* null terminate the operator text string */
			/* check for the operators of higher precedence and then add them to stack */
			if ( exprs->mVerbose )
			{
				snprintf(eBuf,sizeof(eBuf),"parseExpression(): Before precedence. Checking type %d ('%s') precedence %d ...\n", term->termType, term->term.oper, currPrecedence);
				showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
				dumpStacks(exprs);
			}
			/* If what is currently in the operator stack (if anything) has a higher precedence than the operator to be pushed,
			 * then pop the item(s) off the operator stack onto the term stack. Then push the new operator onto the operator stack.
			 */
			while ( topOper >= 0 && (oldPrecedence = exprs->precedencePtr[sPtr->mOpers[topOper].mOperType]) >= currPrecedence )
			{
				term->termType = sPtr->mOpers[topOper].mOperType;
				term->chrPtr = sPtr->mOpers[topOper].chrPtr;
				term->term.oper[0] = sPtr->mOpers[topOper].mOper[0];
				term->term.oper[1] = sPtr->mOpers[topOper].mOper[1];
				term->term.oper[2] = 0;
				if ( exprs->mVerbose )
				{
					snprintf(eBuf,sizeof(eBuf),"parseExpression(): Precedence popped from operators[%ld][%d] a '%s'(%d) and pushed it to terms[%ld][%d]. Precedence: curr=%d, new=%d\n",
						   sPtr-exprs->mStacks, topOper, term->term.oper, term->termType, sPtr - exprs->mStacks, sPtr->mNumTerms,
						   currPrecedence, oldPrecedence);
					showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
				}
				--topOper;
				++sPtr->mNumTerms;
				++term;
			}
			*term = savTerm;
			++topOper;
			sPtr->mOpers[topOper].mOperType = term->termType;
			sPtr->mOpers[topOper].chrPtr = term->chrPtr;
			sPtr->mOpers[topOper].mOper[0] = term->term.oper[0];
			sPtr->mOpers[topOper].mOper[1] = term->term.oper[1];
			sPtr->mOpers[topOper].mOper[2] = 0;	/* make sure the string is null terminated (probably don't need this) */
			if ( exprs->mVerbose )
			{
				snprintf(eBuf,sizeof(eBuf),"parseExpression(): Pushed operator '%s'(%d) to operators[%ld][%d]\n",term->term.oper, term->termType, sPtr-exprs->mStacks, topOper);
				showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
			}
			/* because '+' and '-' are both unary and binary operators, they don't count worth remembering (their precendece decides for them) */
			if ( term->termType == EXPRS_TERM_PLUS || term->termType == EXPRS_TERM_MINUS )
				lastTermWasOperator = false;
			else
				lastTermWasOperator = true;
		}
		else
		{
			lastTermWasOperator = false;
			++sPtr->mNumTerms;
		}
		++exprs->mCurrPtr;
	}
	/* reached end of expression string */
	term = sPtr->mTerms + sPtr->mNumTerms;
	/* pop all the operators off the operator stack onto the term stack */
	while ( topOper >= 0 )
	{
		ExprsOpers_t *oper = sPtr->mOpers+topOper;
		term->termType = oper->mOperType;
		term->chrPtr = oper->chrPtr;
		term->term.oper[0] = oper->mOper[0];
		term->term.oper[1] = oper->mOper[1];
		term->term.oper[2] = 0;
		if ( exprs->mVerbose )
		{
			snprintf(eBuf,sizeof(eBuf),"parseExpression(): Popped '%s'(%d) from operators[%ld][%d] and pushed it to terms[%ld][%d]\n",term->term.oper, term->termType, sPtr-exprs->mStacks, topOper, sPtr - exprs->mStacks, sPtr->mNumTerms);
			showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
		}
		--topOper;
		++sPtr->mNumTerms;
		++term;
	}
	if ( exitOut && exprs->mVerbose )
	{
		snprintf(eBuf,sizeof(eBuf),"parseExpression(): Found ')'. Popping out of terms[%ld]. topOper=%d. mNumTerms=%d\n", sPtr-exprs->mStacks, topOper, sPtr->mNumTerms);
		showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
		dumpStacks(exprs);
	}
	return peRetV;
}

static ExprsErrs_t lookupSymbol(ExprsDef_t *exprs, ExprsTerm_t *src, ExprsTerm_t *dst)
{
	ExprsSymTerm_t ans;
	ExprsErrs_t err;
	char eBuf[512];
	int strLen;
	
	if ( !exprs->mCallbacks.symGet )
		return EXPR_TERM_BAD_NO_SYMBOLS;
	err = exprs->mCallbacks.symGet(exprs->mCallbacks.symArg, src->term.string, &ans);
	if ( !err )
	{
		switch (ans.termType)
		{
		default:
			snprintf(eBuf, sizeof(eBuf), "lookupSymbol(): Found symbol '%s' with illegal type %d.\n", src->term.string, ans.termType);
			showMsg(exprs,EXPRS_SEVERITY_ERROR,eBuf);
			return EXPR_TERM_BAD_UNSUPPORTED;
		case EXPRS_SYM_TERM_COMPLEX:
			return EXPR_TERM_COMPLEX_VALUE;
		case EXPRS_SYM_TERM_STRING:
			strLen = strlen(ans.term.string)+1;
			if ( !(dst->term.string = getStringMem(exprs,strLen)) )
				return EXPR_TERM_BAD_OUT_OF_MEMORY;
			strncpy(dst->term.string, ans.term.string, strLen);
			break;
		case EXPRS_SYM_TERM_FLOAT:
			dst->term.f64 = ans.term.f64;
			break;
		case EXPRS_SYM_TERM_INTEGER:
			dst->term.s64 = ans.term.s64;
			break;
		}
		dst->termType = (ExprsTermTypes_t)ans.termType;
		return EXPR_TERM_GOOD;
	}
	return EXPR_TERM_BAD_UNDEFINED_SYMBOL;
}

/* Computes dst = aa+bb */
static ExprsErrs_t doAdd( ExprsDef_t *exprs, ExprsTerm_t *dst, ExprsTerm_t *aa, ExprsTerm_t *bb )
{
	ExprsErrs_t err;
	int sLen;
	char *newStr;
	char eBuf[512];
	
	if ( exprs->mVerbose )
	{
		snprintf(eBuf,sizeof(eBuf),"doAdd(): entry aaType %d, aaValue 0x%lX, bbType %d, bbValue 0x%lX\n", aa->termType, aa->term.s64, bb->termType, bb->term.s64);
		showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
	}
	if ( aa->termType == EXPRS_TERM_SYMBOL )
	{
		err = lookupSymbol(exprs,aa,aa);
		if ( err != EXPR_TERM_GOOD )
			return err;
	}
	if ( bb->termType == EXPRS_TERM_SYMBOL )
	{
		err = lookupSymbol(exprs,bb,bb);
		if ( err != EXPR_TERM_GOOD )
			return err;
	}
	dst->termType = aa->termType;
	dst->chrPtr = aa->chrPtr;
	err = EXPR_TERM_BAD_SYNTAX;
	switch (aa->termType)
	{
	case EXPRS_TERM_FLOAT:
		switch (bb->termType)
		{
		case EXPRS_TERM_FLOAT:
			dst->term.f64 = aa->term.f64 + bb->term.f64;
			err = EXPR_TERM_GOOD;
			break;
		case EXPRS_TERM_INTEGER:
			dst->term.f64 = aa->term.f64 + bb->term.s64;
			err = EXPR_TERM_GOOD;
			break;
		case EXPRS_TERM_STRING:
			sLen = strlen(bb->term.string)+1+32;
			newStr = getStringMem(exprs,sLen);
			if ( !newStr )
			{
				err = EXPR_TERM_BAD_OUT_OF_MEMORY;
				break;
			}
			snprintf(newStr, sLen, "%g%s", aa->term.f64, bb->term.string);
			dst->termType = EXPRS_TERM_STRING;
			dst->term.string = newStr;
			err = EXPR_TERM_GOOD;
			break;
		default:
			break;
		}
		break;
	case EXPRS_TERM_INTEGER:
		switch (bb->termType)
		{
		case EXPRS_TERM_FLOAT:
			/* When adding a float to an int, convert to float */
			dst->term.f64 = aa->term.s64 + bb->term.f64;
			dst->termType = EXPRS_TERM_FLOAT;
			err = EXPR_TERM_GOOD;
			break;
		case EXPRS_TERM_INTEGER:
			dst->term.s64 = aa->term.s64 + bb->term.s64;
			err = EXPR_TERM_GOOD;
			break;
		case EXPRS_TERM_STRING:
			sLen = strlen(bb->term.string)+1+32;
			newStr = getStringMem(exprs,sLen);
			if ( !newStr )
			{
				err = EXPR_TERM_BAD_OUT_OF_MEMORY;
				break;
			}
			snprintf(newStr, sLen, "%ld%s", aa->term.s64, bb->term.string);
			dst->termType = EXPRS_TERM_STRING;
			dst->term.string = newStr;
			err = EXPR_TERM_GOOD;
			break;
		default:
			break;
		}
		break;
	case EXPRS_TERM_STRING:
		sLen = strlen(aa->term.string);
		switch (bb->termType)
		{
		case EXPRS_TERM_FLOAT:
		case EXPRS_TERM_INTEGER:
			sLen += 32+1;
			newStr = getStringMem(exprs,sLen);
			if ( !newStr )
			{
				err = EXPR_TERM_BAD_OUT_OF_MEMORY;
				break;
			}
			if ( bb->termType == EXPRS_TERM_FLOAT )
				snprintf(newStr, sLen, "%s%g", aa->term.string, bb->term.f64 );
			else
				snprintf(newStr, sLen, "%s%ld", aa->term.string, bb->term.s64 );
			dst->term.string = newStr;
			err = EXPR_TERM_GOOD;
			break;
		case EXPRS_TERM_STRING:
			sLen += strlen(bb->term.string)+1;
			newStr = getStringMem(exprs,sLen);
			if ( !newStr )
			{
				err = EXPR_TERM_BAD_OUT_OF_MEMORY;
				break;
			}
			snprintf(newStr, sLen, "%s%s", aa->term.string, bb->term.string);
			dst->term.string = newStr;
			err = EXPR_TERM_GOOD;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	if ( err != EXPR_TERM_GOOD )
	{
		snprintf(eBuf,sizeof(eBuf), "doAdd(): Syntax error. aaType=%d, bbType=%d. At '%s'\n", aa->termType, bb->termType, aa->chrPtr);
		showMsg(exprs,EXPRS_SEVERITY_ERROR,eBuf);
	}
	else if ( exprs->mVerbose )
	{
		snprintf(eBuf,sizeof(eBuf),"doAdd(): exit dstType %d, dstValue 0x%lX\n", dst->termType, dst->term.s64);
		showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
	}
	return err;
}

/*  Computes dst = aa-bb */
static ExprsErrs_t doSub( ExprsDef_t *exprs, ExprsTerm_t *dst, ExprsTerm_t *aa, ExprsTerm_t *bb )
{
	ExprsErrs_t err;
	char eBuf[512];
	
	if ( exprs->mVerbose )
	{
		snprintf(eBuf,sizeof(eBuf),"doSub(): entry aaType %d, aaValue 0x%lX, bbType %d, bbValue 0x%lX\n", aa->termType, aa->term.s64, bb->termType, bb->term.s64);
		showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
	}
	if ( aa->termType == EXPRS_TERM_SYMBOL )
	{
		err = lookupSymbol(exprs,aa,aa);
		if ( err != EXPR_TERM_GOOD )
			return err;
	}
	if ( bb->termType == EXPRS_TERM_SYMBOL )
	{
		err = lookupSymbol(exprs,bb,bb);
		if ( err != EXPR_TERM_GOOD )
			return err;
	}
	dst->termType = aa->termType;
	dst->chrPtr = aa->chrPtr;
	err = EXPR_TERM_BAD_SYNTAX;
	switch (aa->termType)
	{
	case EXPRS_TERM_FLOAT:
		switch (bb->termType)
		{
		case EXPRS_TERM_FLOAT:
			dst->term.f64 = aa->term.f64 - bb->term.f64;
			err = EXPR_TERM_GOOD;
			break;
		case EXPRS_TERM_INTEGER:
			dst->term.f64 = aa->term.f64 - bb->term.s64;
			err = EXPR_TERM_GOOD;
			break;
		default:
			break;
		}
		break;
	case EXPRS_TERM_INTEGER:
		switch (bb->termType)
		{
		case EXPRS_TERM_FLOAT:
			/*  When subtracting a float from an int, convert to float */
			dst->term.f64 = aa->term.s64 - bb->term.f64;
			dst->termType = EXPRS_TERM_FLOAT;
			err = EXPR_TERM_GOOD;
			break;
		case EXPRS_TERM_INTEGER:
			dst->term.s64 = aa->term.s64 - bb->term.s64;
			err = EXPR_TERM_GOOD;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	if ( err != EXPR_TERM_GOOD )
	{
		snprintf(eBuf,sizeof(eBuf), "doSub(): Syntax error. aaType=%d, bbType=%d. At '%s'\n", aa->termType, bb->termType, aa->chrPtr);
		showMsg(exprs,EXPRS_SEVERITY_ERROR,eBuf);
	}
	else if ( exprs->mVerbose )
	{
		snprintf(eBuf,sizeof(eBuf),"doSub(): exit dstType %d, dstValue 0x%lX\n", dst->termType, dst->term.s64);
		showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
	}
	return err;
}

/* Computes dst = aa**bb */
static ExprsErrs_t doPow( ExprsDef_t *exprs, ExprsTerm_t *dst, ExprsTerm_t *aa, ExprsTerm_t *bb )
{
	ExprsErrs_t err;
	char eBuf[512];
	
	if ( exprs->mVerbose )
	{
		snprintf(eBuf,sizeof(eBuf),"doPow(): entry aaType %d, aaValue 0x%lX, bbType %d, bbValue 0x%lX\n", aa->termType, aa->term.s64, bb->termType, bb->term.s64);
		showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
	}
	if ( aa->termType == EXPRS_TERM_SYMBOL )
	{
		err = lookupSymbol(exprs, aa,aa);
		if ( err != EXPR_TERM_GOOD )
			return err;
	}
	if ( bb->termType == EXPRS_TERM_SYMBOL )
	{
		err = lookupSymbol(exprs, bb,bb);
		if ( err != EXPR_TERM_GOOD )
			return err;
	}
	dst->termType = aa->termType;
	dst->chrPtr = aa->chrPtr;
	err = EXPR_TERM_BAD_SYNTAX;
	/* dst = aa raised to the power of bb */
	switch (dst->termType)
	{
	case EXPRS_TERM_FLOAT:
		switch (bb->termType)
		{
		case EXPRS_TERM_FLOAT:
			/* when raising a float by a float, result is float */
			dst->term.f64 = pow(aa->term.f64,bb->term.f64);
			err = EXPR_TERM_GOOD;
			break;
		case EXPRS_TERM_INTEGER:
			/* when raising a float by an integer, result is float */
			dst->term.f64 = pow(aa->term.f64,bb->term.s64);
			err = EXPR_TERM_GOOD;
			break;
		default:
			break;
		}
		break;
	case EXPRS_TERM_INTEGER:
		switch (bb->termType)
		{
		case EXPRS_TERM_FLOAT:
			/* when raising an integer by a float, result is float */
			dst->term.f64 = pow(aa->term.s64,bb->term.f64);
			dst->termType = EXPRS_TERM_FLOAT;
			err = EXPR_TERM_GOOD;
			break;
		case EXPRS_TERM_INTEGER:
			/* when raising an integer by an integer, result is an integer */
			dst->term.s64 = pow(aa->term.s64, bb->term.s64);
			err = EXPR_TERM_GOOD;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	if ( err != EXPR_TERM_GOOD )
	{
		snprintf(eBuf,sizeof(eBuf), "doPow(): Syntax error. aaType=%d, bbType=%d. At '%s'\n", aa->termType, bb->termType, aa->chrPtr);
		showMsg(exprs,EXPRS_SEVERITY_ERROR,eBuf);
	}
	else if ( exprs->mVerbose )
	{
		snprintf(eBuf,sizeof(eBuf),"doPow(): exit dstType %d, dstValue 0x%lX\n", dst->termType, dst->term.s64);
		showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
	}
	return err;
}

/* Computes dst = aa*bb */
static ExprsErrs_t doMul( ExprsDef_t *exprs, ExprsTerm_t *dst, ExprsTerm_t *aa, ExprsTerm_t *bb )
{
	ExprsErrs_t err;
	char eBuf[512];
	
	if ( exprs->mVerbose )
	{
		snprintf(eBuf,sizeof(eBuf),"doMul(): entry aaType %d, aaValue 0x%lX, bbType %d, bbValue 0x%lX\n", aa->termType, aa->term.s64, bb->termType, bb->term.s64);
		showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
	}
	if ( aa->termType == EXPRS_TERM_SYMBOL )
	{
		err = lookupSymbol(exprs, aa,aa);
		if ( err != EXPR_TERM_GOOD )
			return err;
	}
	if ( bb->termType == EXPRS_TERM_SYMBOL )
	{
		err = lookupSymbol(exprs, bb,bb);
		if ( err != EXPR_TERM_GOOD )
			return err;
	}
	dst->termType = aa->termType;
	dst->chrPtr = aa->chrPtr;
	err = EXPR_TERM_BAD_SYNTAX;
	switch (dst->termType)
	{
	case EXPRS_TERM_FLOAT:
		switch (bb->termType)
		{
		case EXPRS_TERM_FLOAT:
			dst->term.f64 = aa->term.f64 * bb->term.f64;
			err = EXPR_TERM_GOOD;
			break;
		case EXPRS_TERM_INTEGER:
			/*  When multiplying an int by a float, result becomes a float */
			dst->term.f64 = aa->term.f64 * bb->term.s64;
			err = EXPR_TERM_GOOD;
			break;
		default:
			break;
		}
		break;
	case EXPRS_TERM_INTEGER:
		switch (bb->termType)
		{
		case EXPRS_TERM_FLOAT:
			/*  When multiplying a float by an int, result becomes a float */
			dst->term.f64 = aa->term.s64 * bb->term.f64;
			dst->termType = EXPRS_TERM_FLOAT;
			err = EXPR_TERM_GOOD;
			break;
		case EXPRS_TERM_INTEGER:
			dst->term.s64 = aa->term.s64 * bb->term.s64;
			err = EXPR_TERM_GOOD;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	if ( err != EXPR_TERM_GOOD )
	{
		snprintf(eBuf,sizeof(eBuf), "doMul(): Syntax error. aaType=%d, bbType=%d. At '%s'\n", aa->termType, bb->termType, aa->chrPtr);
		showMsg(exprs,EXPRS_SEVERITY_ERROR,eBuf);
	}
	else if ( exprs->mVerbose )
	{
		snprintf(eBuf,sizeof(eBuf),"doMul(): exit dstType %d, dstValue 0x%lX\n", dst->termType, dst->term.s64);
		showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
	}
	return err;
}

/* Computes dst = aa/bb */
static ExprsErrs_t doDiv( ExprsDef_t *exprs, ExprsTerm_t *dst, ExprsTerm_t *aa, ExprsTerm_t *bb )
{
	ExprsErrs_t err;
	double bbV;
	char eBuf[512];
	
	if ( exprs->mVerbose )
	{
		snprintf(eBuf,sizeof(eBuf),"doDiv(): entry aaType %d, aaValue 0x%lX, bbType %d, bbValue 0x%lX\n", aa->termType, aa->term.s64, bb->termType, bb->term.s64);
		showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
	}
	if ( aa->termType == EXPRS_TERM_SYMBOL )
	{
		err = lookupSymbol(exprs,aa,aa);
		if ( err != EXPR_TERM_GOOD )
			return err;
	}
	if ( bb->termType == EXPRS_TERM_SYMBOL )
	{
		err = lookupSymbol(exprs,bb,bb);
		if ( err != EXPR_TERM_GOOD )
			return err;
	}
	bbV = 0.0;
	if ( bb->termType == EXPRS_TERM_FLOAT || bb->termType == EXPRS_TERM_INTEGER )
		bbV = (bb->termType == EXPRS_TERM_FLOAT) ? bb->term.f64 : bb->term.s64;
	if ( bbV == 0.0 )
	{
		double aaV;
		aaV = 0.0;
		if ( aa->termType == EXPRS_TERM_FLOAT || aa->termType == EXPRS_TERM_INTEGER )
			aaV = (bb->termType == EXPRS_TERM_FLOAT) ? aa->term.f64 : aa->term.s64;
		snprintf(eBuf,sizeof(eBuf),"doDiv(): bb term is 0.0, aa term is %g. Divide by 0 error at or near %s\n", aaV, aa->chrPtr);
		showMsg(exprs,EXPRS_SEVERITY_ERROR,eBuf);
		return EXPR_TERM_BAD_DIV_BY_0;
	}
	/* dst = aa/bb */
	dst->termType = aa->termType;
	dst->chrPtr = aa->chrPtr;
	err = EXPR_TERM_BAD_SYNTAX;
	switch (dst->termType)
	{
	case EXPRS_TERM_FLOAT:
		switch (bb->termType)
		{
		case EXPRS_TERM_FLOAT:
			dst->term.f64 = aa->term.f64 / bb->term.f64;
			err = EXPR_TERM_GOOD;
			break;
		case EXPRS_TERM_INTEGER:
			dst->term.f64 = aa->term.f64 / bb->term.s64;
			err = EXPR_TERM_GOOD;
			break;
		default:
			break;
		}
		break;
	case EXPRS_TERM_INTEGER:
		switch (bb->termType)
		{
		case EXPRS_TERM_FLOAT:
			/* When subtracting a float from an int, convert aa to float */
			dst->term.f64 = aa->term.s64 / bb->term.f64;
			dst->termType = EXPRS_TERM_FLOAT;
			err = EXPR_TERM_GOOD;
			break;
		case EXPRS_TERM_INTEGER:
			dst->term.s64 = aa->term.s64 / bb->term.s64;
			err = EXPR_TERM_GOOD;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	if ( err != EXPR_TERM_GOOD )
	{
		snprintf(eBuf,sizeof(eBuf), "doDiv(): Syntax error. aaType=%d, bbType=%d. At or near '%s'\n", aa->termType, bb->termType, aa->chrPtr);
		showMsg(exprs,EXPRS_SEVERITY_ERROR,eBuf);
	}
	else if ( exprs->mVerbose )
	{
		snprintf(eBuf,sizeof(eBuf),"doDiv(): exit dstType %d, dstValue 0x%lX\n", dst->termType, dst->term.s64);
		showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
	}
	return err;
}

/* Computes dst = aa%bb */
static ExprsErrs_t doMod( ExprsDef_t *exprs, ExprsTerm_t *dst, ExprsTerm_t *aa, ExprsTerm_t *bb )
{
	ExprsErrs_t err;
	double bbV;
	char eBuf[512];
	
	if ( exprs->mVerbose )
	{
		snprintf(eBuf,sizeof(eBuf),"doMod(): entry aaType %d, aaValue 0x%lX, bbType %d, bbValue 0x%lX\n", aa->termType, aa->term.s64, bb->termType, bb->term.s64);
		showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
	}
	if ( aa->termType == EXPRS_TERM_SYMBOL )
	{
		err = lookupSymbol(exprs,aa,aa);
		if ( err != EXPR_TERM_GOOD )
			return err;
	}
	if ( bb->termType == EXPRS_TERM_SYMBOL )
	{
		err = lookupSymbol(exprs,bb,bb);
		if ( err != EXPR_TERM_GOOD )
			return err;
	}
	dst->termType = aa->termType;
	dst->chrPtr = aa->chrPtr;
	err = EXPR_TERM_BAD_SYNTAX;
	bbV = 0.0;
	if ( bb->termType == EXPRS_TERM_FLOAT || bb->termType == EXPRS_TERM_INTEGER )
		bbV = (bb->termType == EXPRS_TERM_FLOAT) ? bb->term.f64 : bb->term.s64;
	if ( bbV == 0.0 )
	{
		double aaV;
		aaV = 0.0;
		if ( aa->termType == EXPRS_TERM_FLOAT || aa->termType == EXPRS_TERM_INTEGER )
			aaV = (bb->termType == EXPRS_TERM_FLOAT) ? aa->term.f64 : aa->term.s64;
		snprintf(eBuf,sizeof(eBuf),"doMod(): bb term is 0.0, aa term is %g. Divide by 0 error.\n", aaV);
		showMsg(exprs,EXPRS_SEVERITY_ERROR,eBuf);
		return EXPR_TERM_BAD_DIV_BY_0;
	}
	switch (aa->termType)
	{
	case EXPRS_TERM_FLOAT:
		switch (bb->termType)
		{
		case EXPRS_TERM_FLOAT:
			dst->term.f64 = fmod(aa->term.f64, bb->term.f64);
			err = EXPR_TERM_GOOD;
			break;
		case EXPRS_TERM_INTEGER:
			dst->term.f64 = fmod(aa->term.f64, bb->term.s64);
			err = EXPR_TERM_GOOD;
			break;
		default:
			break;
		}
		break;
	case EXPRS_TERM_INTEGER:
		switch (bb->termType)
		{
		case EXPRS_TERM_FLOAT:
			dst->term.f64 = fmod(aa->term.s64, bb->term.f64);
			dst->termType = EXPRS_TERM_FLOAT;
			err = EXPR_TERM_GOOD;
			break;
		case EXPRS_TERM_INTEGER:
			dst->term.s64 = aa->term.s64 % bb->term.s64;
			err = EXPR_TERM_GOOD;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	return err;
	if ( err != EXPR_TERM_GOOD )
	{
		snprintf(eBuf,sizeof(eBuf), "doMod(): Syntax error. aaType=%d, bbType=%d. At '%s'\n", aa->termType, bb->termType, aa->chrPtr);
		showMsg(exprs,EXPRS_SEVERITY_ERROR,eBuf);
	}
	else if ( exprs->mVerbose )
	{
		snprintf(eBuf,sizeof(eBuf),"doMod(): exit dstType %d, dstValue 0x%lX\n", dst->termType, dst->term.s64);
		showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
	}
}

typedef struct
{
	ExprsDef_t *exprs;
	ExprsTerm_t results[EXPRS_TERM_ASSIGN+1];
	ExprsStack_t *sPtr;
	ExprsTerm_t *term, *aa, *bb;
	int ii, rTop;
	char tmpBuf0[32],tmpBuf1[32],tmpBuf2[32];
} TermParams_t;

static ExprsErrs_t prepUnaryTerm(TermParams_t *params, bool toInteger)
{
	ExprsErrs_t err;
	
	if ( params->rTop < 0 )
		return EXPR_TERM_BAD_TOO_FEW_TERMS;
	if ( params->exprs->mVerbose )
	{
		char eBuf[512];
		snprintf(eBuf,sizeof(eBuf),"prepUnaryTerm(): Item %d: %s, aa=%s\n",
			   params->ii,
			   showTermType(params->exprs,params->sPtr,params->term,params->tmpBuf0,sizeof(params->tmpBuf0)-1),
			   showTermType(params->exprs,params->sPtr,params->aa,params->tmpBuf1,sizeof(params->tmpBuf1)-1));
		showMsg(params->exprs,EXPRS_SEVERITY_INFO,eBuf);
	}
	if ( params->aa->termType == EXPRS_TERM_SYMBOL )
	{
		err = lookupSymbol(params->exprs,params->aa,params->aa);
		if ( err != EXPR_TERM_GOOD )
			return err;
	}
	if ( toInteger && params->aa->termType == EXPRS_TERM_FLOAT )
	{
		params->aa->termType = EXPRS_TERM_INTEGER;
		params->aa->term.s64 = params->aa->term.f64;
	}
	return EXPR_TERM_GOOD;
}

static ExprsErrs_t prepBinaryTerms(TermParams_t *params, bool toInteger)
{
	if ( params->rTop < 1 )
		return EXPR_TERM_BAD_TOO_FEW_TERMS;
	params->bb = params->results + (params->rTop--);
	params->aa = params->results + params->rTop;
	if ( params->exprs->mVerbose )
	{
		char eBuf[512];
		snprintf(eBuf,sizeof(eBuf),"prepBinaryTerms(): Item %d: %s, aa=%s, bb=%s\n",
			   params->ii,
			   showTermType(params->exprs, params->sPtr, params->term, params->tmpBuf0, sizeof(params->tmpBuf0) - 1),
			   showTermType(params->exprs, params->sPtr, params->aa, params->tmpBuf1, sizeof(params->tmpBuf1) - 1),
			   showTermType(params->exprs, params->sPtr, params->bb, params->tmpBuf2, sizeof(params->tmpBuf2) - 1));
		showMsg(params->exprs,EXPRS_SEVERITY_INFO,eBuf);
	}
	if ( toInteger )
	{
		if ( params->aa->termType == EXPRS_TERM_FLOAT )
		{
			params->aa->termType = EXPRS_TERM_INTEGER;
			params->aa->term.s64 = params->aa->term.f64;
		}
		if ( params->bb->termType == EXPRS_TERM_FLOAT )
		{
			params->bb->termType = EXPRS_TERM_INTEGER;
			params->bb->term.s64 = params->bb->term.f64;
		}
	}
	return EXPR_TERM_GOOD;
}

static ExprsErrs_t computeViaRPN(ExprsDef_t *exprs, int nest, ExprsStack_t *sPtr, ExprsTerm_t *returnResult)
{
	TermParams_t params;
	ExprsTerm_t *term, *dst;
	ExprsErrs_t err;
	ExprsTermTypes_t tType;
	ExprsSymTerm_t ans;
	int ii;
	char eBuf[512];
	
	if ( exprs->mVerbose )
	{
		snprintf(eBuf,sizeof(eBuf),"Into computeViaRPN(): nest=%d, stack %ld. Items=%d\n", nest, sPtr - exprs->mStacks, sPtr->mNumTerms);
		showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
	}
	returnResult->termType = EXPRS_TERM_NULL;
	if ( !sPtr->mNumTerms )
		return EXPR_TERM_BAD_TOO_FEW_TERMS;
	params.exprs = exprs;
	params.sPtr = sPtr;
	params.rTop = -1;
	for( ii=0; ii < sPtr->mNumTerms; ++ii )
	{
		params.term = term = sPtr->mTerms+ii;
		params.ii = ii;
		returnResult->chrPtr = term->chrPtr;
		tType =  term->termType; 
		if ( exprs->mVerbose )
		{
			snprintf(eBuf,sizeof(eBuf),"computeViaRPN(): Item %d. type=%d. rTop=%d\n", ii, term->termType,params.rTop);
			showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
		}
		if ( params.rTop >= 0 )
			params.aa = params.results + params.rTop;
		else
			params.aa = NULL;
		params.bb = NULL;
		switch ( tType )
		{
		case EXPRS_TERM_NULL:
			continue;
		case EXPRS_TERM_LINK:
			{
				ExprsStack_t *nPtr;
				nPtr = (ExprsStack_t *)sPtr->mTerms[ii].term.link;
				if ( params.rTop >= n_elts(params.results)-1 )
					return EXPR_TERM_BAD_TOO_MANY_TERMS;
				if ( exprs->mVerbose )
				{
					snprintf(eBuf,sizeof(eBuf),"computeViaRPN(): Item %d: %s\n",
						   ii,
						   showTermType(exprs,nPtr,term,params.tmpBuf0,sizeof(params.tmpBuf0)-1));
					showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
				}
				++params.rTop;
				params.aa = params.results + params.rTop;
				err = computeViaRPN(exprs, nest+1, nPtr, params.aa);
				if ( err > EXPR_TERM_END )
					return err;
			}
			continue;
		case EXPRS_TERM_SYMBOL:
			if ( !exprs->mCallbacks.symGet )
				return EXPR_TERM_BAD_NO_SYMBOLS;
			/* Fall through to normal stack function */
		case EXPRS_TERM_FUNCTION:
		case EXPRS_TERM_STRING:
		case EXPRS_TERM_INTEGER:
		case EXPRS_TERM_FLOAT:
			if ( params.rTop >= n_elts(params.results)-1 )
				return EXPR_TERM_BAD_TOO_MANY_TERMS;
			if ( exprs->mVerbose )
			{
				snprintf(eBuf,sizeof(eBuf),"computeViaRPN(): Item %d: %s\n",
					   ii,
					   showTermType(exprs,sPtr,term,params.tmpBuf0,sizeof(params.tmpBuf0)-1));
				showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
			}
			params.results[++params.rTop] = *term;
			continue;
		case EXPRS_TERM_PLUS:	/* + */
			if ( (err=prepUnaryTerm(&params,false)) )
				return err;
			if ( exprs->mVerbose )
			{
				snprintf(eBuf,sizeof(eBuf),"computeViaRPN(): PLUS Item %d: %s, aa=%s\n",
					   ii,
					   showTermType(exprs,sPtr,term,params.tmpBuf0,sizeof(params.tmpBuf0)-1),
					   showTermType(exprs,sPtr,params.aa,params.tmpBuf1,sizeof(params.tmpBuf1)-1));
				showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
			}
			continue;			/* Just eat a extraneous plus sign */
		case EXPRS_TERM_MINUS:	/* - */
			if ( (err=prepUnaryTerm(&params,false)) )
				return err;
			if ( exprs->mVerbose )
			{
				snprintf(eBuf,sizeof(eBuf),"computeViaRPN(): MINUS Item %d: %s, aa=%s\n",
					   ii,
					   showTermType(exprs,sPtr,term,params.tmpBuf0,sizeof(params.tmpBuf0)-1),
					   showTermType(exprs,sPtr,params.aa,params.tmpBuf1,sizeof(params.tmpBuf1)-1));
				showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
			}
			if ( params.aa->termType == EXPRS_TERM_INTEGER )
				params.aa->term.s64 = -params.aa->term.s64;
			else if ( params.aa->termType == EXPRS_TERM_FLOAT )
				params.aa->term.f64 = -params.aa->term.f64;
			else
				return EXPR_TERM_BAD_SYNTAX;
			continue;
		case EXPRS_TERM_ADD:	/* + */
			if ( (err=prepBinaryTerms(&params,false)) )
				return err;
			if ( (err = doAdd(exprs, params.aa, params.aa, params.bb)) > EXPR_TERM_END )
				return err;
			continue;
		case EXPRS_TERM_SUB:	/* - */
			if ( (err=prepBinaryTerms(&params,false)) )
				return err;
			if ( (err = doSub(exprs, params.aa, params.aa, params.bb)) > EXPR_TERM_END )
				return err;
			continue;
		case EXPRS_TERM_POW:	/* ** */
			if ( (err=prepBinaryTerms(&params,false)) )
				return err;
			err = doPow(exprs, params.aa, params.aa, params.bb);
			if ( err > EXPR_TERM_END )
				return err;
			continue;
		case EXPRS_TERM_MUL:	/* * */
			if ( (err=prepBinaryTerms(&params,false)) )
				return err;
			err = doMul(exprs, params.aa, params.aa, params.bb);
			if ( err > EXPR_TERM_END )
				return err;
			continue;
		case EXPRS_TERM_DIV:	/* / */
			if ( (err=prepBinaryTerms(&params,false)) )
				return err;
			err = doDiv(exprs, params.aa, params.aa, params.bb);
			if ( err > EXPR_TERM_END )
				return err;
			continue;
		case EXPRS_TERM_MOD:	/* % */
			if ( (err=prepBinaryTerms(&params,false)) )
				return err;
			err = doMod(exprs, params.aa, params.aa, params.bb);
			if ( err > EXPR_TERM_END )
				return err;
			continue;
		case EXPRS_TERM_COM:	/* ~ */
			if ( (err=prepUnaryTerm(&params,true)) )
				return err;
			if ( params.aa->termType == EXPRS_TERM_INTEGER )
				params.aa->term.s64 = ~params.aa->term.s64;
			else
				return EXPR_TERM_BAD_SYNTAX;
			continue;
		case EXPRS_TERM_NOT:	/* ! */
			if ( (err=prepUnaryTerm(&params,true)) )
				return err;
			if ( params.aa->termType == EXPRS_TERM_INTEGER )
				params.aa->term.s64 = params.aa->term.s64 ? 0 : 1;
			else
				return EXPR_TERM_BAD_SYNTAX;
			continue;
		case EXPRS_TERM_LOW_BYTE:
			if ( (err=prepUnaryTerm(&params,true)) )
				return err;
			if ( params.aa->termType == EXPRS_TERM_INTEGER )
				params.aa->term.u64 = params.aa->term.u64 & 0xFF;
			else
				return EXPR_TERM_BAD_SYNTAX;
			continue;
		case EXPRS_TERM_HIGH_BYTE:
			if ( (err=prepUnaryTerm(&params,true)) )
				return err;
			if ( params.aa->termType == EXPRS_TERM_INTEGER )
				params.aa->term.u64 = (params.aa->term.u64 >> 8) & 0xFF;
			else
				return EXPR_TERM_BAD_SYNTAX;
			continue;
		case EXPRS_TERM_XCHG:
			if ( (err=prepUnaryTerm(&params,true)) )
				return err;
			if ( params.aa->termType == EXPRS_TERM_INTEGER )
			{
				unsigned long ul = params.aa->term.u64;
				params.aa->term.u64 = ((ul>>8)&0xFF) | ((ul<<8)&0xFF00);
			}
			else
				return EXPR_TERM_BAD_SYNTAX;
			continue;
		case EXPRS_TERM_ASL:	/* << */
			if ( (err=prepBinaryTerms(&params,true)) )
				return err;
			if ( params.aa->termType != EXPRS_TERM_INTEGER || params.bb->termType != EXPRS_TERM_INTEGER )
				return EXPR_TERM_BAD_SYNTAX;
			params.aa->term.s64 <<= params.bb->term.s64;
			continue;
		case EXPRS_TERM_ASR:	/* >> */
			if ( (err=prepBinaryTerms(&params,true)) )
				return err;
			if ( params.aa->termType != EXPRS_TERM_INTEGER || params.bb->termType != EXPRS_TERM_INTEGER )
				return EXPR_TERM_BAD_SYNTAX;
			params.aa->term.s64 >>= params.bb->term.s64;
			continue;
		case EXPRS_TERM_GT:		/* > */
			if ( (err=prepBinaryTerms(&params,false)) )
				return err;
			dst = params.aa;
			err = doSub(exprs, params.aa,params.aa,params.bb);
			if ( err )
				return err;
			if ( dst->termType == EXPRS_TERM_INTEGER )
			{
				if ( dst->term.s64 > 0 )
					dst->term.s64 = 1;
				else
					dst->term.s64 = 0;
			}
			else if ( dst->termType == EXPRS_TERM_FLOAT )
			{
				dst->termType = EXPRS_TERM_INTEGER;
				if ( dst->term.f64 > 0 )
					dst->term.s64 = 1;
				else
					dst->term.s64 = 0;
			}
			else
				return EXPR_TERM_BAD_SYNTAX;
			continue;
		case EXPRS_TERM_GE:		/* >= */
			if ( (err=prepBinaryTerms(&params,false)) )
				return err;
			dst = params.aa;
			err = doSub(exprs, dst, params.aa, params.bb);
			if ( err )
				return err;
			if ( dst->termType == EXPRS_TERM_INTEGER )
			{
				if ( dst->term.s64 >= 0 )
					dst->term.s64 = 1;
				else
					dst->term.s64 = 0;
			}
			else if ( dst->termType == EXPRS_TERM_FLOAT )
			{
				dst->termType = EXPRS_TERM_INTEGER;
				if ( dst->term.f64 >= 0 )
					dst->term.s64 = 1;
				else
					dst->term.s64 = 0;
			}
			else
				return EXPR_TERM_BAD_SYNTAX;
			continue;
		case EXPRS_TERM_LT:		/* < */
			if ( (err=prepBinaryTerms(&params,false)) )
				return err;
			dst = params.aa;
			err = doSub(exprs, dst, params.aa, params.bb);
			if ( err )
				return err;
			if ( dst->termType == EXPRS_TERM_INTEGER )
			{
				if ( dst->term.s64 < 0 )
					dst->term.s64 = 1;
				else
					dst->term.s64 = 0;
			}
			else if ( dst->termType == EXPRS_TERM_FLOAT )
			{
				dst->termType = EXPRS_TERM_INTEGER;
				if ( dst->term.f64 < 0 )
					dst->term.s64 = 1;
				else
					dst->term.s64 = 0;
			}
			else
				return EXPR_TERM_BAD_SYNTAX;
			continue;
		case EXPRS_TERM_LE:		/* <= */
			if ( (err=prepBinaryTerms(&params,false)) )
				return err;
			dst = params.aa;
			err = doSub(exprs, dst, params.aa, params.bb);
			if ( err )
				return err;
			if ( dst->termType == EXPRS_TERM_INTEGER )
			{
				if ( dst->term.s64 <= 0 )
					dst->term.s64 = 1;
				else
					dst->term.s64 = 0;
			}
			else if ( dst->termType == EXPRS_TERM_FLOAT )
			{
				dst->termType = EXPRS_TERM_INTEGER;
				if ( dst->term.f64 <= 0 )
					dst->term.s64 = 1;
				else
					dst->term.s64 = 0;
			}
			else
				return EXPR_TERM_BAD_SYNTAX;
			continue;
		case EXPRS_TERM_EQ:		/* == */
			if ( (err=prepBinaryTerms(&params,false)) )
				return err;
			dst = params.aa;
			err = doSub(exprs, dst, params.aa, params.bb);
			if ( err )
				return err;
			if ( dst->termType == EXPRS_TERM_INTEGER )
			{
				if ( dst->term.s64 == 0 )
					dst->term.s64 = 1;
				else
					dst->term.s64 = 0;
			}
			else if ( dst->termType == EXPRS_TERM_FLOAT )
			{
				dst->termType = EXPRS_TERM_INTEGER;
				if ( dst->term.f64 == 0 )
					dst->term.s64 = 1;
				else
					dst->term.s64 = 0;
			}
			else
				return EXPR_TERM_BAD_SYNTAX;
			continue;
		case EXPRS_TERM_NE:		/* != */
			if ( (err=prepBinaryTerms(&params,false)) )
				return err;
			dst = params.aa;
			err = doSub(exprs, dst, params.aa, params.bb);
			if ( err )
				return err;
			if ( dst->termType == EXPRS_TERM_INTEGER )
			{
				if ( dst->term.s64 != 0 )
					dst->term.s64 = 1;
				else
					dst->term.s64 = 0;
			}
			else if ( dst->termType == EXPRS_TERM_FLOAT )
			{
				dst->termType = EXPRS_TERM_INTEGER;
				if ( dst->term.f64 != 0 )
					dst->term.s64 = 1;
				else
					dst->term.s64 = 0;
			}
			else
				return EXPR_TERM_BAD_SYNTAX;
			continue;
		case EXPRS_TERM_AND:	/* & */
			if ( (err=prepBinaryTerms(&params,true)) )
				return err;
			if ( params.aa->termType != EXPRS_TERM_INTEGER || params.bb->termType != EXPRS_TERM_INTEGER )
				return EXPR_TERM_BAD_SYNTAX;
			params.aa->term.s64 &= params.bb->term.s64;
			continue;
		case EXPRS_TERM_XOR:	/* ^ */
			if ( (err=prepBinaryTerms(&params,true)) )
				return err;
			if ( params.aa->termType != EXPRS_TERM_INTEGER || params.bb->termType != EXPRS_TERM_INTEGER )
				return EXPR_TERM_BAD_SYNTAX;
			params.aa->term.s64 ^= params.bb->term.s64;
			continue;
		case EXPRS_TERM_OR:		/* | */
			if ( (err=prepBinaryTerms(&params,true)) )
				return err;
			if ( params.aa->termType != EXPRS_TERM_INTEGER || params.bb->termType != EXPRS_TERM_INTEGER )
				return EXPR_TERM_BAD_SYNTAX;
			params.aa->term.s64 |= params.bb->term.s64;
			continue;
		case EXPRS_TERM_LAND:	/* && */
			if ( (err=prepBinaryTerms(&params,true)) )
				return err;
			if ( params.aa->termType != EXPRS_TERM_INTEGER || params.bb->termType != EXPRS_TERM_INTEGER )
				return EXPR_TERM_BAD_SYNTAX;
			params.aa->term.s64 = (params.aa->term.s64 && params.bb->term.s64) ? 1 : 0;
			continue;
		case EXPRS_TERM_LOR:	/* || */
			if ( (err=prepBinaryTerms(&params,true)) )
				return err;
			if ( params.aa->termType != EXPRS_TERM_INTEGER || params.bb->termType != EXPRS_TERM_INTEGER )
				return EXPR_TERM_BAD_SYNTAX;
			params.aa->term.s64 = (params.aa->term.s64 || params.bb->term.s64) ? 1 : 0;
			continue;
		case EXPRS_TERM_ASSIGN:	/* = */
			if ( (err=prepBinaryTerms(&params,false)) )
				return err;
			/* bb is the value to assign to it */
			/* aa is the symbol to assign to */
			if ( !exprs->mCallbacks.symGet )
			{
				snprintf(eBuf,sizeof(eBuf),"computeViaRPN(): No symbol table. Assignment not possible: at or near %s\n", params.aa->chrPtr);
				showMsg(exprs,EXPRS_SEVERITY_ERROR,eBuf);
				return EXPR_TERM_BAD_NO_SYMBOLS;
			}
			if ( params.aa->termType != EXPRS_TERM_SYMBOL )
			{
				snprintf(eBuf,sizeof(eBuf), "computeViaRPN(): Assignment to non-symbol (%d) not possible: at or near %s\n", params.aa->termType, params.aa->chrPtr);
				showMsg(exprs,EXPRS_SEVERITY_ERROR,eBuf);
				return EXPR_TERM_BAD_LVALUE;
			}
			if ( params.bb->termType == EXPRS_TERM_SYMBOL )
			{
				err = lookupSymbol(exprs,params.bb,params.bb);
				if ( err != EXPR_TERM_GOOD )
					return err;
			}
			if ( params.bb->termType != EXPRS_TERM_STRING && params.bb->termType != EXPRS_TERM_FLOAT && params.bb->termType != EXPRS_TERM_INTEGER )
			{
				snprintf(eBuf,sizeof(eBuf),"computeViaRPN(): Assignment can only be type Integer, float or string (is %d) at or near %s\n",
						params.bb->termType, params.aa->chrPtr);
				showMsg(exprs,EXPRS_SEVERITY_ERROR,eBuf);
				return EXPR_TERM_BAD_LVALUE;
			}
			ans.termType = (ExprsSymTermTypes_t)params.bb->termType;
			ans.term.f64 = params.bb->term.f64;
			err = exprs->mCallbacks.symSet(exprs->mCallbacks.symArg,params.aa->term.string,&ans);
			if ( err )
			{
				snprintf(eBuf,sizeof(eBuf),"computeViaRPN(): Failed ('%s') to assign symbol '%s' at or near %s\n",
						 libExprsGetErrorStr(err),
						 params.aa->term.string,
						 params.aa->chrPtr);
				showMsg(exprs,EXPRS_SEVERITY_ERROR,eBuf);
				return err;
			}
			params.aa->chrPtr = params.bb->chrPtr;
			params.aa->termType = params.bb->termType;
			params.aa->term.u64 = params.bb->term.u64;
			continue;
		}
	}
	if ( params.rTop != 0 )
	{
		snprintf(eBuf,sizeof(eBuf),"computeViaRPN(): expression did not resolve to a single term. Found rTop=%d\n", params.rTop);
		showMsg(exprs,EXPRS_SEVERITY_ERROR,eBuf);
		if ( params.rTop > 1 )
			return EXPR_TERM_BAD_TOO_MANY_TERMS;
		else
			return EXPR_TERM_BAD_TOO_FEW_TERMS;
	}
	*returnResult = params.results[0];
	if ( exprs->mVerbose )
	{
		snprintf(eBuf,sizeof(eBuf),"computeViaRPN(): Finish. nest=%d, stack %ld. Items=%d. rTop=%d, %s\n",
			   nest, sPtr - exprs->mStacks, sPtr->mNumTerms, params.rTop,
			   showTermType(exprs,sPtr,returnResult,params.tmpBuf0,sizeof(params.tmpBuf0)-1));
		showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
	}
	return EXPR_TERM_GOOD;
}

typedef struct
{
	ExprsErrs_t err;
	const char *string;
} ErrMsgs_t;

static const ErrMsgs_t Errs[] =
{
	{ EXPR_TERM_GOOD, "Success" },
	{ EXPR_TERM_END, "End of text" },
	{ EXPR_TERM_BAD_OUT_OF_MEMORY, "Out of memory" },
	{ EXPR_TERM_BAD_NO_STRING_TERM, "Missing string terminator" },
	{ EXPR_TERM_BAD_STRINGS_NOT_SUPPORTED, "Strings not supported" },
	{ EXPR_TERM_BAD_SYMBOL_SYNTAX, "Invalid symbol syntax" },
	{ EXPR_TERM_BAD_SYMBOL_TOO_LONG, "Symbol string too long" },
	{ EXPR_TERM_BAD_NUMBER, "Invalid number syntax" },
	{ EXPR_TERM_BAD_UNARY, "Invalid unary operation" },
	{ EXPR_TERM_BAD_OPER, "Invalid binary operation" },
	{ EXPR_TERM_BAD_SYNTAX, "Invalid expression syntax" },
	{ EXPR_TERM_BAD_TOO_MANY_TERMS, "Too many terms" },
	{ EXPR_TERM_BAD_TOO_FEW_TERMS, "Too few terms" },
	{ EXPR_TERM_BAD_NO_TERMS, "No terms" },
	{ EXPR_TERM_BAD_NO_CLOSE, "Missing closing parenthesis" },
	{ EXPR_TERM_BAD_UNSUPPORTED, "Unsupported operation" },
	{ EXPR_TERM_BAD_DIV_BY_0, "Divide by 0 error" },
	{ EXPR_TERM_BAD_UNDEFINED_SYMBOL, "Undefined symbol" },
	{ EXPR_TERM_BAD_NO_SYMBOLS, "No symbol table available" },
	{ EXPR_TERM_BAD_SYMBOL_TABLE_FULL, "Symbol table is full" },
	{ EXPR_TERM_BAD_LVALUE, "lvalue is not a symbol" },
	{ EXPR_TERM_BAD_RVALUE, "result of expression is not an integer, float or string" },
	{ EXPR_TERM_BAD_PARAMETER, "Invalid parameter value" },
	{ EXPR_TERM_BAD_NOLOCK, "Failed to lock pthread mutex" },
	{ EXPR_TERM_BAD_NOUNLOCK, "Failed to unlock pthread mutex" },
};

const char *libExprsGetErrorStr(ExprsErrs_t errCode)
{
	int ii;
	for (ii=0; ii < n_elts(Errs); ++ii)
	{
		if ( Errs[ii].err == errCode )
			return Errs[ii].string;
	}
	return "Undefined errCode";
}

ExprsErrs_t libExprsEval(ExprsDef_t *exprs, const char *text, ExprsTerm_t *returnTerm, int alreadyLocked)
{
	ExprsErrs_t peErr, err = EXPR_TERM_BAD_SYNTAX, err2 = EXPR_TERM_GOOD;
	char eBuf[512], saveOpen=0, saveClose=0;
	int len;
	
	if ( !exprs || !returnTerm )
		return EXPR_TERM_BAD_PARAMETER;
	if ( !alreadyLocked && (err2=libExprsLock(exprs)) )
		return err2;
	returnTerm->termType = EXPRS_TERM_NULL;
	returnTerm->term.s64 = 0;
	returnTerm->chrPtr = NULL;
	exprs->precedencePtr = (exprs->mFlags & EXPRS_FLG_NO_PRECEDENCE) ? PrecedenceNone : PrecedenceNormal;
	if ( (exprs->mFlags&EXPRS_FLG_DOT_DECIMAL) || ((exprs->mFlags&EXPRS_FLG_USE_RADIX) && (exprs->mRadix != 0 && exprs->mRadix != 10)) )
		exprs->mFlags |= EXPRS_FLG_NO_FLOAT;
	if ( (exprs->mFlags & EXPRS_FLG_SPECIAL_UNARY) )
	{
		saveOpen = exprs->mOpenDelimiter;
		saveClose = exprs->mCloseDelimiter;
		exprs->mOpenDelimiter = '<';
		exprs->mCloseDelimiter = '>';
	}
	if ( exprs->mFlags && exprs->mVerbose )
	{
		snprintf(eBuf,sizeof(eBuf),"libExprsEval(): flags=0x%lX, radix=%d\n", exprs->mFlags, exprs->mRadix);
		showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
	}
	if ( text )
	{
		const char *ePtr;

		len = strlen(text);
		ePtr = text+len;
		exprs->mLineHead = exprs->mCurrPtr = text;
		while ( exprs->mCurrPtr < ePtr && *exprs->mCurrPtr )
		{
			reset(exprs); 				/* Clear any existing cruft */
			peErr = parseExpression(exprs, 0, true, getNextStack(exprs));
			err = peErr;
			if ( err <= EXPR_TERM_END )
			{
				if ( exprs->mVerbose )
				{
					showMsg(exprs,EXPRS_SEVERITY_INFO,"Stacks before computeViaRPN");
					dumpStacks(exprs);
				}
				err = computeViaRPN(exprs, 0, exprs->mStacks, returnTerm);
				if ( err <= EXPR_TERM_END )
				{
					if ( returnTerm->termType == EXPRS_TERM_SYMBOL )
					{
						ExprsTerm_t sym;
						err = lookupSymbol(exprs, returnTerm,&sym);
						if ( err )
						{
							len = snprintf(eBuf,sizeof(eBuf),"libExprsEval(): Undefined symbol: %s\n", returnTerm->term.string);
							if ( returnTerm->chrPtr )
								snprintf(eBuf+len,sizeof(eBuf)-len, "At or near: %s\n", returnTerm->chrPtr );
							showMsg(exprs,EXPRS_SEVERITY_ERROR,eBuf);
						}
						else
						{
							returnTerm->chrPtr = sym.chrPtr;
							returnTerm->termType = sym.termType;
							returnTerm->term.u64 = sym.term.u64;
						}
					}
					else if ( exprs->mVerbose )
					{
						showMsg(exprs,EXPRS_SEVERITY_INFO,"Stacks after computeViaRPN");
						dumpStacks(exprs);
						snprintf(eBuf,sizeof(eBuf),"The resulting term after computing RPN expression:\n");
						showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
						switch (returnTerm->termType)
						{
						case EXPRS_TERM_NULL:
						case EXPRS_TERM_LINK:	/* link to another stack */
						case EXPRS_TERM_SYMBOL:	/* Symbol */
						case EXPRS_TERM_FUNCTION:/* Function call */
						case EXPRS_TERM_STRING:	/* Text string */
							snprintf(eBuf,sizeof(eBuf),"Type %d: value: '%s'\n", returnTerm->termType, returnTerm->term.string);
							break;
						case EXPRS_TERM_FLOAT:	/* 64 bit floating point number */
							snprintf(eBuf,sizeof(eBuf),"Type %d: value: '%g'\n", returnTerm->termType, returnTerm->term.f64);
							break;
						case EXPRS_TERM_INTEGER:	/* 64 bit integer number */
							snprintf(eBuf,sizeof(eBuf),"Type %d: value: '%ld'\n", returnTerm->termType, returnTerm->term.s64);
							break;
						default:
							snprintf(eBuf,sizeof(eBuf),"Type %d: value: 0x%lX (undefined)\n", returnTerm->termType, returnTerm->term.s64);
							break;
						}
						showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
					}
				}
				else
				{
					len = snprintf(eBuf,sizeof(eBuf),"computeViaRPN() returned %d: %s\n", err, libExprsGetErrorStr(err));
					if ( returnTerm->chrPtr )
						snprintf(eBuf+len,sizeof(eBuf)-len, "At or near: %s\n", returnTerm->chrPtr );
					showMsg(exprs,EXPRS_SEVERITY_ERROR,eBuf);
					dumpStacks(exprs);
				}
				if ( (exprs->mFlags & EXPRS_FLG_WS_DELIMIT) && peErr == EXPR_TERM_END )
				{
					if ( exprs->mVerbose )
					{
						snprintf(eBuf, sizeof(eBuf), "Ending text: '%s'\n", exprs->mCurrPtr);
						showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
					}
					break;
				}
				if ( *exprs->mCurrPtr )
					++exprs->mCurrPtr;
			}
			else 
			{
				if ( exprs->mVerbose )				
				{
					snprintf(eBuf,sizeof(eBuf), "parseExpression() returned %d: %s\n", err, libExprsGetErrorStr(err));
					showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
					dumpStacks(exprs);
				}
				break;
			}
			if ( err > EXPR_TERM_END )
				break;
		}
	}
	if ( (exprs->mFlags & EXPRS_FLG_SPECIAL_UNARY) )
	{
		exprs->mOpenDelimiter = saveOpen;
		exprs->mCloseDelimiter = saveClose;
	}
	if ( !alreadyLocked )
		err2 = libExprsUnlock(exprs);
	return err ? err : err2;
}

static void lclMsgOut(void *msgArg, ExprsMsgSeverity_t severity, const char *msg)
{
	static const char *Severities[] = { "INFO", "WARN", "ERROR", "FATAL" };
	fprintf(severity > EXPRS_SEVERITY_INFO ? stderr:stdout,"%s-libExprs(): %s",Severities[severity],msg);
}

static void *lclMalloc(void *memArg, size_t size)
{
	return malloc(size);
}
static void lclFree(void *memArg, void *ptr)
{
	free(ptr);
}

static ExprsCallbacks_t *checkCallbacks(ExprsCallbacks_t *tCallbacks, const ExprsCallbacks_t *callbacks, ExprsMsgSeverity_t severity )
{
	memset(tCallbacks,0,sizeof(ExprsCallbacks_t));
	tCallbacks->memAlloc = lclMalloc;
	tCallbacks->memFree = lclFree;
	tCallbacks->msgOut = lclMsgOut;
	if ( callbacks )
	{
		if ( callbacks->msgOut )
		{
			tCallbacks->msgOut = callbacks->msgOut;
			tCallbacks->msgArg = callbacks->msgArg;
		}
		if ( callbacks->memAlloc || callbacks->memFree )
		{
			if ( !callbacks->memAlloc || !callbacks->memFree )
			{
				tCallbacks->msgOut(tCallbacks->msgArg,severity,"callbacks->memAlloc and callbacks->memFree must both be set\n");
				return NULL;
			}
			tCallbacks->memAlloc = callbacks->memAlloc;
			tCallbacks->memFree = callbacks->memFree;
			tCallbacks->memArg = callbacks->memArg;
		}
		if ( callbacks->symGet || callbacks->symSet )
		{
			if ( !callbacks->symGet || !callbacks->symSet )
			{
				tCallbacks->msgOut(tCallbacks->msgArg,severity,"callbacks->symGet and callbacks->symSet must both be set\n");
				return NULL;
			}
			tCallbacks->symArg = callbacks->symArg;
			tCallbacks->symGet = callbacks->symGet;
			tCallbacks->symSet = callbacks->symSet;
		}
	}
	return tCallbacks;
}

ExprsErrs_t libExprsSetCallbacks(ExprsDef_t *exprs, const ExprsCallbacks_t *newPtr, ExprsCallbacks_t *oldPtr)
{
	ExprsCallbacks_t tCallbacks;

	if ( !checkCallbacks(&tCallbacks,newPtr,EXPRS_SEVERITY_ERROR) )
		return EXPR_TERM_BAD_PARAMETER;
	if ( oldPtr )
		*oldPtr = exprs->mCallbacks;
	exprs->mCallbacks = tCallbacks;
	return EXPR_TERM_GOOD;
}

ExprsDef_t* libExprsInit(const ExprsCallbacks_t *callbacks, int maxStacks, int maxTerms, int stringPoolSize)
{
	int ii;
	ExprsStack_t *sPtr;
	size_t stackSize;
	size_t termPoolSize;
	size_t opersPoolSize;
	ExprsOpers_t *etPtr;
	ExprsTerm_t *tPtr;
	ExprsDef_t *exprs;
	ExprsCallbacks_t tCallbacks;
	char tBuf[512];
	
	if ( !maxStacks )
		maxStacks = 32;
	if ( !maxTerms )
		maxTerms = 32;
	if ( !stringPoolSize )
		stringPoolSize = 2048;
	if ( !checkCallbacks(&tCallbacks,callbacks,EXPRS_SEVERITY_FATAL) )
		return NULL;
	exprs = (ExprsDef_t *)tCallbacks.memAlloc(tCallbacks.memArg, sizeof(ExprsDef_t));
	if ( !exprs )
	{
		snprintf(tBuf,sizeof(tBuf),"libExprsInit(): Failed to allocate %ld bytes for ExprsDef_t: %s\n", sizeof(ExprsDef_t), strerror(errno));
		tCallbacks.msgOut(tCallbacks.msgArg, EXPRS_SEVERITY_FATAL, tBuf);
		return NULL;
	}
	memset(exprs, 0, sizeof(ExprsDef_t));
	exprs->mOpenDelimiter = '(';
	exprs->mCloseDelimiter = ')';
	exprs->mCallbacks = tCallbacks;
	exprs->mNumAvailStacks = maxStacks;
	exprs->mNumAvailTerms = maxTerms;
	exprs->mVerbose = 0;
	stackSize = exprs->mNumAvailStacks*sizeof(ExprsStack_t);
	exprs->mStacks = (ExprsStack_t *)tCallbacks.memAlloc(tCallbacks.memArg, stackSize);
	if ( !exprs->mStacks )
	{
		snprintf(tBuf,sizeof(tBuf),"libExprsInit(): unable to allocate %ld bytes for stacks. Out of memory.\n", stackSize );
		showMsg(exprs,EXPRS_SEVERITY_FATAL,tBuf);
		tCallbacks.memFree(tCallbacks.memArg, exprs);
		return NULL;
	}
	termPoolSize = (exprs->mNumAvailStacks*exprs->mNumAvailTerms)*sizeof(ExprsTerm_t);
	exprs->mTermPool = (ExprsTerm_t *)tCallbacks.memAlloc(tCallbacks.memArg, termPoolSize);
	if ( !exprs->mTermPool )
	{
		snprintf(tBuf,sizeof(tBuf), "libExprsInit(): unable to allocate %ld bytes for terms. Out of memory.\n", termPoolSize );
		showMsg(exprs,EXPRS_SEVERITY_FATAL,tBuf);
		tCallbacks.memFree(tCallbacks.memArg, exprs->mStacks);
		tCallbacks.memFree(tCallbacks.memArg, exprs);
		return NULL;
	}
	opersPoolSize = (exprs->mNumAvailStacks*exprs->mNumAvailTerms)*sizeof(ExprsOpers_t);
	exprs->mOpersPool = (ExprsOpers_t *)tCallbacks.memAlloc(tCallbacks.memArg, opersPoolSize);
	if ( !exprs->mOpersPool )
	{
		snprintf(tBuf,sizeof(tBuf),"libExprsInit(): unable to allocate %ld bytes for operators. Out of memory.\n", opersPoolSize );
		showMsg(exprs,EXPRS_SEVERITY_FATAL,tBuf);
		tCallbacks.memFree(tCallbacks.memArg, exprs->mTermPool);
		tCallbacks.memFree(tCallbacks.memArg, exprs->mStacks);
		tCallbacks.memFree(tCallbacks.memArg, exprs);
		return NULL;
	}
	exprs->mStringPoolAvailable = stringPoolSize;
	exprs->mStringPool = (char *)tCallbacks.memAlloc(tCallbacks.memArg, stringPoolSize+1);
	if ( !exprs->mStringPool )
	{
		snprintf(tBuf,sizeof(tBuf),"libExprsInit(): unable to allocate %d bytes for string pool. Out of memory.\n", stringPoolSize+1);
		showMsg(exprs,EXPRS_SEVERITY_FATAL,tBuf);
		tCallbacks.memFree(tCallbacks.memArg, exprs->mOpersPool);
		tCallbacks.memFree(tCallbacks.memArg, exprs->mTermPool);
		tCallbacks.memFree(tCallbacks.memArg, exprs->mStacks);
		tCallbacks.memFree(tCallbacks.memArg, exprs);
		return NULL;
	}
	sPtr = exprs->mStacks;
	tPtr = exprs->mTermPool;
	etPtr = exprs->mOpersPool;
	for (ii=0; ii < exprs->mNumAvailStacks; ++ii, ++sPtr)
	{
		sPtr->mNumTerms = 0;
		sPtr->mTerms = tPtr;
		sPtr->mOpers = etPtr;
		tPtr += exprs->mNumAvailTerms;
		etPtr += exprs->mNumAvailTerms;
	}
	exprs->mStringPool[0] = 0;
	pthread_mutex_init(&exprs->mMutex,NULL);
	return exprs;
}

ExprsErrs_t libExprsDestroy(ExprsDef_t *exprs)
{
	ExprsErrs_t err;
	void (*memFree)(void *memArg, void *memPtr) = exprs->mCallbacks.memFree;
	void *pArg = exprs->mCallbacks.memArg;
	
	err = libExprsLock(exprs);
	if ( exprs->mStringPool )
		memFree(pArg, exprs->mStringPool);
	if ( exprs->mOpersPool )
		memFree(pArg, exprs->mOpersPool);
	if ( exprs->mTermPool )
		memFree(pArg, exprs->mTermPool);
	if ( exprs->mStacks )
		memFree(pArg, exprs->mStacks);
	pthread_mutex_unlock(&exprs->mMutex);
	pthread_mutex_destroy(&exprs->mMutex);
	memFree(pArg, exprs);
	return err;
}

ExprsErrs_t libExprsLock(ExprsDef_t *exprs)
{
	if ( pthread_mutex_lock(&exprs->mMutex) )
	{
		if ( exprs->mVerbose )
		{
			char emsg[128];
			snprintf(emsg,sizeof(emsg),"Failed to lock: %s\n", strerror(errno));
			exprs->mCallbacks.msgOut(exprs->mCallbacks.msgArg,EXPRS_SEVERITY_ERROR,emsg);
		}
		return EXPR_TERM_BAD_NOLOCK;
	}
	return EXPR_TERM_GOOD;
}

ExprsErrs_t libExprsUnlock(ExprsDef_t *exprs)
{
	if ( pthread_mutex_unlock(&exprs->mMutex) )
	{
		if ( exprs->mVerbose )
		{
			char emsg[128];
			snprintf(emsg,sizeof(emsg),"Failed to unlock: %s\n", strerror(errno));
			exprs->mCallbacks.msgOut(exprs->mCallbacks.msgArg,EXPRS_SEVERITY_ERROR,emsg);
		}
		return EXPR_TERM_BAD_NOUNLOCK;
	}
	return EXPR_TERM_GOOD;
}

