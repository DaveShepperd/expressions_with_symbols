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
#include <inttypes.h>

#include "lib_exprs.h"
#define OPERSTUFF_GET_OTHERS 1
#include "lib_operstuff.h"

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
#define CT_AT   (0x4000)	/* at sign (@) */
#define CT_COL	(0x8000)	/* colon (:) */

#undef EOL
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
#define AT		CT_AT		/* at sign */
#define COL		CT_COL		/* colon */

static const uint16_t CttblNormal[] =
{
	EOL, EOL, EOL, EOL, EOL, EOL, EOL, EOL,  /* NUL,SOH,STX,ETX,EOT,ENQ,ACK,BEL */
	EOL,  WS, EOL, EOL, EOL, EOL, EOL, EOL,  /* BS,HT,LF,VT,FF,CR,SO,SI */
	EOL, EOL, EOL, EOL, EOL, EOL, EOL, EOL,  /* DLE,DC1,DC2,DC3,DC4,NAK,SYN,ETB */
	EOL, EOL, EOL, EOL, EOL, EOL, EOL, EOL,  /* CAN,EM,SUB,ESC,FS,GS,RS,US */

	WS, EXP, QUO, PCX, XALP, EXP, BOP, QUO,  /*   ! " # $ % & ' */
	UOP, UOP, BOP, EXP, COM, EXP, DOT, BOP,  /* ( ) * + , - . / */
	NUM, NUM, NUM, NUM, NUM, NUM, NUM, NUM,  /* 0 1 2 3 4 5 6 7 */
	NUM, NUM, COL, SMC, BOP, BOP, BOP, PCX,  /* 8 9 : ; < = > ? */

	AT,  HEX, HEX, HEX, HEX, HEX, HEX, ALP,  /* @ A B C D E F G */
	ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP,  /* H I J K L M N O */
	ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP,  /* P Q R S T U V W */
	ALP, ALP, ALP, PCX, BSL, PCX, EXP, XALP,  /* X Y Z [ \ ] ^ _ */

	QUO, LHEX, LHEX, LHEX, LHEX, LHEX, LHEX, LC,  /* ` a b c d e f g */
	LC, LC, LC, LC, LC, LC, LC, LC,  /* h i j k l m n o */
	LC, LC, LC, LC, LC, LC, LC, LC,  /* p q r s t u v w */
	LC, LC, LC, PCX, BOP, PCX, UOP, EOL   /* x y z { | } ~ DEL */
};

static const uint16_t CttblSpecial[] =
{
	EOL, EOL, EOL, EOL, EOL, EOL, EOL, EOL,  /* NUL,SOH,STX,ETX,EOT,ENQ,ACK,BEL */
	EOL,  WS, EOL, EOL, EOL, EOL, EOL, EOL,  /* BS,HT,LF,VT,FF,CR,SO,SI */
	EOL, EOL, EOL, EOL, EOL, EOL, EOL, EOL,  /* DLE,DC1,DC2,DC3,DC4,NAK,SYN,ETB */
	EOL, EOL, EOL, EOL, EOL, EOL, EOL, EOL,  /* CAN,EM,SUB,ESC,FS,GS,RS,US */

	WS, BOP, QUO, PCX, XALP, EXP, BOP, QUO,  /*   ! " # $ % & ' */
	UOP, UOP, BOP, EXP, COM, EXP, DOT, BOP,  /* ( ) * + , - . / */
	NUM, NUM, NUM, NUM, NUM, NUM, NUM, NUM,  /* 0 1 2 3 4 5 6 7 */
	NUM, NUM, COL, SMC, UOP, BOP, UOP, BOP,  /* 8 9 : ; < = > ? */

	AT,  HEX, HEX, HEX, HEX, HEX, HEX, ALP,  /* @ A B C D E F G */
	ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP,  /* H I J K L M N O */
	ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP,  /* P Q R S T U V W */
	ALP, ALP, ALP, PCX, BSL, PCX, EXP, XALP,  /* X Y Z [ \ ] ^ _ */

	QUO, LHEX, LHEX, LHEX, LHEX, LHEX, LHEX, LC,  /* ` a b c d e f g */
	LC, LC, LC, LC, LC, LC, LC, LC,  /* h i j k l m n o */
	LC, LC, LC, LC, LC, LC, LC, LC,  /* p q r s t u v w */
	LC, LC, LC, BOP, BOP, BOP, UOP, EOL   /* x y z { | } ~ DEL */
};

typedef uint8_t bool;
enum
{
	false,
	true
};

static const char *PoolNames[] =
{
	"Undefined",
	"Stacks",
	"Terms",
	"Opers",
	"String"
};

static void* pointToNextInPool(ExprsDef_t *exprs, ExprsPool_t *pool, int increment, size_t numItems)
{
	void *ans = NULL;
	if ( pool->mNumUsed + numItems >= pool->mNumAvailable )
	{
		void *newPtr;
		const char *name;
		int newNum;
		int newInc = increment;
		if ( newInc < numItems )
			newInc = numItems + 1;
		newNum = pool->mNumAvailable + newInc;
		if ( pool->mNumAvailable && (exprs->mFlags & EXPRS_FLG_SANITY) )
		{
			char eBuf[128];
			name = PoolNames[0];
			if ( pool->mPoolID >= 0 && pool->mPoolID < n_elts(PoolNames) )
				name = PoolNames[pool->mPoolID];
			snprintf(eBuf, sizeof(eBuf), "nextPool %d(%s) due to sanity check, bump from %d items (" FMT_SZ " bytes) to %d items (" FMT_SZ " bytes) failed\n",
					 pool->mPoolID, name,
					 pool->mNumAvailable, pool->mNumAvailable * pool->mEntrySize,
					 newNum, newNum * pool->mEntrySize);
			exprs->mCallbacks.msgOut(exprs->mCallbacks.msgArg, EXPRS_SEVERITY_ERROR, eBuf);
			return NULL;
		}
		newPtr = (void *)exprs->mCallbacks.memAlloc(exprs->mCallbacks.memArg, newNum * pool->mEntrySize);
		if ( !newPtr )
		{
			char tBuf[128];
			name = PoolNames[0];
			if ( pool->mPoolID >= 0 && pool->mPoolID < n_elts(PoolNames) )
				name = PoolNames[pool->mPoolID];
			snprintf(tBuf, sizeof(tBuf), "lib_exprs().getMemoryItem(): Failed to allocate " FMT_SZ " bytes for pool %d(%s): %s\n",
					 newNum * pool->mEntrySize, pool->mPoolID, name, strerror(errno));
			exprs->mCallbacks.msgOut(exprs->mCallbacks.msgArg, EXPRS_SEVERITY_FATAL, tBuf);
			return NULL;
		}
		if ( pool->mPoolTop )
		{
			/* If existing memory, need to do a realloc */
			if ( pool->mNumUsed )
				memcpy(newPtr, pool->mPoolTop, pool->mNumUsed * pool->mEntrySize);
			exprs->mCallbacks.memFree(exprs->mCallbacks.memArg, pool->mPoolTop);
		}
		pool->mPoolTop = newPtr;
		pool->mNumAvailable = newNum;
		memset((char *)newPtr + (pool->mNumUsed * pool->mEntrySize), 0, (pool->mNumAvailable - pool->mNumUsed) * pool->mEntrySize);
	}
	ans = (void *)((char *)pool->mPoolTop + pool->mEntrySize * pool->mNumUsed);
	return ans;
}

static void* getNextFromPool(ExprsDef_t *exprs, ExprsPool_t *pool, int increment, size_t numItems)
{
	void *ans = pointToNextInPool(exprs, pool, increment, numItems);
	if ( ans )
		pool->mNumUsed += numItems;
	return ans;
}

static ExprsTerm_t* pointToNextTerm(ExprsDef_t *exprs, ExprsStack_t *stack)
{
	ExprsTerm_t *ans;
	ans = (ExprsTerm_t *)pointToNextInPool(exprs, &stack->mTermsPool, exprs->mTermsPoolInc, 1);
	if ( !ans )
		return ans;
	ans->term.s64 = 0;
	ans->termType = EXPRS_TERM_NULL;
	ans->chrPtr = NULL;
	ans->flags = 0;
	return ans;
}

static char* getFromStringPool(ExprsDef_t *exprs, int len)
{
	char *ans = (char *)getNextFromPool(exprs, &exprs->mStringPool, exprs->mStringPoolInc, len);
	*ans = 0;
	return ans;
}

char* libExprsStringPoolTop(ExprsDef_t *exprs)
{
	return (char *)exprs->mStringPool.mPoolTop;
}

ExprsTerm_t* libExprsTermPoolTop(ExprsDef_t *exprs, ExprsStack_t *stack)
{
	return (ExprsTerm_t *)stack->mTermsPool.mPoolTop;
}

static char* showTermType(ExprsDef_t *exprs, ExprsStack_t *sPtr, ExprsTerm_t *term, char *dst, int dstLen)
{
	int len;

	dst[0] = 0;
	switch (term->termType)
	{
	case EXPRS_TERM_NULL:
		snprintf(dst, dstLen, "%s", "NULL");
		break;
	case EXPRS_TERM_SYMBOL_COMPLEX:
	case EXPRS_TERM_SYMBOL:
		len = snprintf(dst, dstLen, "Symbol: ");
		if ( (term->flags & EXPRS_TERM_FLAG_LOCAL_SYMBOL) )
			len += snprintf(dst + len, dstLen - len, "(local)");
		if ( (term->flags & EXPRS_TERM_FLAG_REGISTER) )
			len += snprintf(dst + len, dstLen - len, "(register)");
		if ( (term->flags & EXPRS_TERM_FLAG_COMPLEX) )
			len += snprintf(dst + len, dstLen - len, "(complex)");
		snprintf(dst + len, dstLen - len, "%s", libExprsStringPoolTop(exprs) + term->term.string);
		break;
	case EXPRS_TERM_FUNCTION:
		snprintf(dst, dstLen, "Function: %s()", libExprsStringPoolTop(exprs) + term->term.string);
		break;
	case EXPRS_TERM_STRING:
		snprintf(dst, dstLen, "String: '%s'", libExprsStringPoolTop(exprs) + term->term.string);
		break;
	case EXPRS_TERM_FLOAT:
		snprintf(dst, dstLen, "FLOAT: '%g'", term->term.f64);
		break;
	case EXPRS_TERM_INTEGER:
		len = snprintf(dst, dstLen, "Integer: ");
		if ( (term->flags & EXPRS_TERM_FLAG_REGISTER) )
			len += snprintf(dst + len, dstLen - len, "(register)");
		snprintf(dst, dstLen, "%ld", term->term.s64);
		break;
	case EXPRS_TERM_POS:
	case EXPRS_TERM_NEG:
	case EXPRS_TERM_COM:
	case EXPRS_TERM_NOT:
	case EXPRS_TERM_HIGH_BYTE:
	case EXPRS_TERM_LOW_BYTE:
	case EXPRS_TERM_SWAP:
	case EXPRS_TERM_POW:
	case EXPRS_TERM_MUL:
	case EXPRS_TERM_DIV:
	case EXPRS_TERM_MOD:
	case EXPRS_TERM_ADD:
	case EXPRS_TERM_SUB:
	case EXPRS_TERM_SHL:
	case EXPRS_TERM_SHR:
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

static void showMsg(ExprsDef_t *exprs, ExprsMsgSeverity_t severity, const char *msg)
{
	exprs->mCallbacks.msgOut(exprs->mCallbacks.msgArg, severity, msg);
}

ExprsErrs_t libExprsSetVerbose(ExprsDef_t *exprs, unsigned int newVal, unsigned int *oldValP)
{
	ExprsErrs_t err;
	unsigned int oldVal;
	if ( (err = libExprsLock(exprs)) )
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
	if ( (err = libExprsLock(exprs)) )
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
	if ( (err = libExprsLock(exprs)) )
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
	if ( (err = libExprsLock(exprs)) )
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
	if ( (err = libExprsLock(exprs)) )
		return err;
	oldVal = exprs->mOpenDelimiter;
	libExprsUnlock(exprs);
	if ( oldValP )
		*oldValP = oldVal;
	return EXPR_TERM_GOOD;
}

static void dumpStack(ExprsDef_t *exprs, ExprsTerm_t *opers, int numOpers)
{
	int ii;
	ExprsStack_t *sPtr;
	ExprsTerm_t *term;
	char eBuf[512];
	int len;

	sPtr = &exprs->mStack;
	len = snprintf(eBuf, sizeof(eBuf), "Term stack has %3d %s ", sPtr->mTermsPool.mNumUsed, sPtr->mTermsPool.mNumUsed == 1 ? "term: " : "terms:");
	term = (ExprsTerm_t *)sPtr->mTermsPool.mPoolTop;
	for ( ii = 0; ii < sPtr->mTermsPool.mNumUsed; ++ii, ++term )
	{
		switch (term->termType)
		{
		case EXPRS_TERM_NULL:
			len += snprintf(eBuf + len, sizeof(eBuf) - len, " NULL");
			break;
		case EXPRS_TERM_STRING:
			{
				unsigned char *src;
				src = (unsigned char *)libExprsStringPoolTop(exprs) + term->term.string;
				len += snprintf(eBuf + len, sizeof(eBuf) - len, " \"");
				while ( *src )
				{
					if ( isprint(*src) )
						len += snprintf(eBuf + len, sizeof(eBuf) - len, "%c", *src++);
					else
						len += snprintf(eBuf + len, sizeof(eBuf) - len, "\\x%02X", *src++);
				}
				len += snprintf(eBuf + len, sizeof(eBuf) - len, "\"");
			}
			break;
		case EXPRS_TERM_SYMBOL_COMPLEX:
		case EXPRS_TERM_SYMBOL:
			if ( len < (int)sizeof(eBuf) - 1 )
			{
				eBuf[len++] = ' ';
				if ( (term->flags & EXPRS_TERM_FLAG_LOCAL_SYMBOL) )
					len += snprintf(eBuf + len, sizeof(eBuf) - len, "(local)");
				if ( (term->flags & EXPRS_TERM_FLAG_REGISTER) )
					len += snprintf(eBuf + len, sizeof(eBuf) - len, "(register)");
				if ( (term->flags & EXPRS_TERM_FLAG_COMPLEX) )
					len += snprintf(eBuf + len, sizeof(eBuf) - len, "(complex)");
				len += snprintf(eBuf + len, sizeof(eBuf) - len, "%s", libExprsStringPoolTop(exprs) + term->term.string);
			}
			break;
		case EXPRS_TERM_FUNCTION:
			len += snprintf(eBuf + len, sizeof(eBuf) - len, " %s", libExprsStringPoolTop(exprs) + term->term.string);
			break;
		case EXPRS_TERM_FLOAT:
			len += snprintf(eBuf + len, sizeof(eBuf) - len, " %g", term->term.f64);
			break;
		case EXPRS_TERM_INTEGER:
			if ( len < (int)sizeof(eBuf) - 1 )
			{
				eBuf[len++] = ' ';
				if ( (term->flags & EXPRS_TERM_FLAG_REGISTER) )
					len += snprintf(eBuf + len, sizeof(eBuf) - len, "(register)");
				len += snprintf(eBuf + len, sizeof(eBuf) - len, "%ld", term->term.s64);
			}
			break;
		case EXPRS_TERM_POS:    /* + */
		case EXPRS_TERM_NEG:   /* - */
			len += snprintf(eBuf + len, sizeof(eBuf) - len, " (unary)%s", term->term.oper);
			break;
		case EXPRS_TERM_COM:    /* ~ */
		case EXPRS_TERM_NOT:    /* ! */
		case EXPRS_TERM_HIGH_BYTE:  /* high byte */
		case EXPRS_TERM_LOW_BYTE:   /* low byte */
		case EXPRS_TERM_SWAP:   /* exchange bytes */
		case EXPRS_TERM_POW:    /* ** */
		case EXPRS_TERM_MUL:    /* * */
		case EXPRS_TERM_DIV:    /* / */
		case EXPRS_TERM_MOD:    /* % */
		case EXPRS_TERM_ADD:    /* + */
		case EXPRS_TERM_SUB:    /* - */
		case EXPRS_TERM_SHL:    /* << */
		case EXPRS_TERM_SHR:    /* >> */
		case EXPRS_TERM_GT:     /* > */
		case EXPRS_TERM_GE:     /* >= */
		case EXPRS_TERM_LT:     /* < */
		case EXPRS_TERM_LE:     /* <= */
		case EXPRS_TERM_EQ:     /* == */
		case EXPRS_TERM_NE:     /* != */
		case EXPRS_TERM_AND:    /* & */
		case EXPRS_TERM_XOR:    /* ^ */
		case EXPRS_TERM_OR:     /* | */
		case EXPRS_TERM_LAND:   /* && */
		case EXPRS_TERM_LOR:    /* || */
		case EXPRS_TERM_ASSIGN: /* = */
			len += snprintf(eBuf + len, sizeof(eBuf) - len, " %s", OperDescriptions[term->termType]);
			break;
		}
	}
	len += snprintf(eBuf + len, sizeof(eBuf) - len, "\n");
	showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
	if ( opers && numOpers )
	{
		len = snprintf(eBuf, sizeof(eBuf), "Opers stack has %d term%s:", numOpers, numOpers == 1 ? "" : "s");
		for ( ii = 0; ii < numOpers; ++ii, ++opers )
		{
			len += snprintf(eBuf + len, sizeof(eBuf) - len, " %s(%d:%d)",
							opers->term.oper,
							opers->termType,
							exprs->precedencePtr[opers->termType]);
		}
		eBuf[len++] = '\n';
		eBuf[len] = 0;
	}
	else
		snprintf(eBuf, sizeof(eBuf), "Opers stack has 0 terms\n");
	showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
}

static ExprsErrs_t badSyntax(ExprsDef_t *exprs, uint16_t chMask, char cc, ExprsErrs_t retErr)
{
	char eBuf[256];
	snprintf(eBuf, sizeof(eBuf), "parseExpression(): Syntax error. cc='%c', chMask=0x%04X\n", isprint(cc) ? cc : '.', chMask);
	showMsg(exprs, EXPRS_SEVERITY_ERROR, eBuf);
	/* Not something we know how to handle */
	return retErr;
}

static char* handleLengthQualifiers(ExprsDef_t *exprs, ExprsTerm_t *term, char *sEndp, bool *eatSuffixP)
{
	char cc;
	uint16_t chMask;
	cc = toupper(sEndp[1]);
	chMask = exprs->chMaskPtr[(int)sEndp[2]];
	if ( !(chMask & (CT_ALP | CT_XALP | CT_HEX)) )
	{
		if ( cc == 'B' )
			term->flags |= EXPRS_TERM_FLAG_BYTE;
		else if ( cc == 'W' )
			term->flags |= EXPRS_TERM_FLAG_WORD;
		else if ( cc == 'L' )
			term->flags |= EXPRS_TERM_FLAG_LONG;
		if ( (term->flags & (EXPRS_TERM_FLAG_BYTE | EXPRS_TERM_FLAG_WORD | EXPRS_TERM_FLAG_LONG)) )
		{
			sEndp += 2;
			if ( eatSuffixP )
				*eatSuffixP = false;
		}
	}
	return sEndp;
}

static ExprsErrs_t storeInteger(ExprsDef_t *exprs,
								ExprsStack_t *sPtr,
								ExprsTerm_t *term,
								const char *startP,
								char suffix,
								bool eatSuffix,
								int radix,
								const char *rdxName,
								bool skipStrtol)
{
	char *sEndp;
	char eBuf[512];

	if ( !skipStrtol )
	{
		sEndp = NULL;
		term->term.s64 = strtoll(startP, &sEndp, radix);
		if ( !sEndp || sEndp == startP || (eatSuffix && toupper(*sEndp) != suffix) )
			return EXPR_TERM_BAD_NUMBER;
		if ( (exprs->mFlags & EXPRS_FLG_LEN_QUALIFIERS) && *sEndp == '.' )
			sEndp = handleLengthQualifiers(exprs, term, sEndp, &eatSuffix);
		if ( eatSuffix )
			++sEndp;
		exprs->mCurrPtr = sEndp;
	}
	term->termType = EXPRS_TERM_INTEGER;
	if ( exprs->mVerbose )
	{
		snprintf(eBuf, sizeof(eBuf), "parseExpression().storeInteger(): Pushed to terms[%d] a %s Integer %ld. flags=0x%X, radix=%d.\n",
				 sPtr->mTermsPool.mNumUsed, rdxName, term->term.s64, term->flags, radix);
		showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
	}
	++sPtr->mTermsPool.mNumUsed;
	return EXPR_TERM_GOOD;
}

static ExprsErrs_t handleString(ExprsDef_t *exprs, ExprsStack_t *sPtr, ExprsTerm_t *term, char cc)
{
	int strLen;
	const char *endP;
	char *newPtr, *sEndP, quoteChar = cc, *dst;
	uint16_t chMask;
	char eBuf[512];

	if ( (exprs->mFlags & (EXPRS_FLG_NO_STRING | EXPRS_FLG_SINGLE_QUOTE)) )
	{
		if ( quoteChar != '"' && (exprs->mFlags & EXPRS_FLG_SINGLE_QUOTE) )
		{
			endP = exprs->mCurrPtr + 1;       /* Skip starting quote char */
			cc = *endP++;
			/* optionally eat a trailing quote */
			if ( *endP == '\'' )
				++endP;
			term->term.s64 = cc;
			term->termType = EXPRS_TERM_INTEGER;
			++sPtr->mTermsPool.mNumUsed;
			exprs->mCurrPtr = endP;
			if ( exprs->mVerbose )
			{
				snprintf(eBuf, sizeof(eBuf), "parseExpression().handleString(): Pushed to terms[%d] a string character=0x%02lX ('%c')\n",
						 sPtr->mTermsPool.mNumUsed - 1, term->term.s64, isprint(cc) ? cc : '.');
				showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
			}
			return EXPR_TERM_GOOD;
		}
		return EXPR_TERM_BAD_STRINGS_NOT_SUPPORTED;
	}
	else
	{
		endP = exprs->mCurrPtr + 1;       /* Skip starting quote char */
		/* find end of string */
		while ( (cc = *endP) )
		{
			chMask = exprs->chMaskPtr[(int)cc];
			if ( (chMask & CT_EOL) )
				return EXPR_TERM_BAD_NO_STRING_TERM;
			++endP;	/* advance over the char */
			if ( cc == '\\' )
			{
				/* skip over whatever char is following the backslash */
				++endP;
				continue;
			}
			if ( cc == quoteChar )
				break;
		}
		strLen = endP - exprs->mCurrPtr;
	}
	newPtr = getFromStringPool(exprs, strLen + 1);
	if ( !newPtr )
		return EXPR_TERM_BAD_OUT_OF_MEMORY;
	newPtr[strLen] = 0;
	endP = exprs->mCurrPtr + 1;       /* Skip starting quote char */
	dst = newPtr;
	term->term.string = newPtr - libExprsStringPoolTop(exprs);
	*newPtr = 0;
	while ( (cc = *endP) && dst < newPtr + strLen )
	{
		chMask = exprs->chMaskPtr[(int)cc];
		if ( (chMask & CT_EOL) )
			break;
		if ( cc == '\\' )
		{
			unsigned char cvt;
			cc = endP[1];
			cvt = cc;
			if ( cc >= '0' && cc <= '7' )
			{
				cvt = cc - '0';
				cc = endP[2];
				if ( cc >= '0' && cc <= '7' )
				{
					cvt <<= 3;
					cvt |= cc - '0';
					cc = endP[3];
					if ( cc >= '0' && cc <= '7' )
					{
						cvt <<= 3;
						cvt |= cc - '0';
						++endP;
					}
					++endP;
				}
			}
			else if ( cc == 'x' )
			{
				sEndP = NULL;
				cvt = strtol(endP + 1, &sEndP, 16);
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
			endP += 2;      /* eat both the '\' and the following character */
			*dst++ = cvt;
			continue;
		}
		if ( cc == quoteChar )
		{
			++endP;         /* Eat terminator */
			break;
		}
		*dst++ = *endP++;   /* copy the char */
	}
	*dst = 0;           /* null terminate the string */
	if ( exprs->mVerbose )
	{
		snprintf(eBuf, sizeof(eBuf), "parseExpression().handleString(): Pushed to terms[%d] a string='%s'\n",
				 sPtr->mTermsPool.mNumUsed - 1,
				 libExprsStringPoolTop(exprs) + term->term.string);
		showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
	}
	term->termType = EXPRS_TERM_STRING;
	++sPtr->mTermsPool.mNumUsed;
	exprs->mCurrPtr = endP;
	return (cc == quoteChar) ? EXPR_TERM_GOOD : EXPR_TERM_BAD_NO_STRING_TERM;
}

typedef enum
{
	TermIsPlain,
	TermIsUnary,
	TermIsBinary
} TermType_t;

static ExprsErrs_t parseExpression(ExprsDef_t *exprs, int nest, TermType_t lastTermType);

static ExprsErrs_t handleSymbol(ExprsDef_t *exprs, ExprsTerm_t *term, ExprsStack_t *sPtr, ExprsTermTypes_t ttype)
{
	size_t symLen;
	const char *endP;
	char cc, *strPtr;
	uint16_t chMask, chChk;

	/* symbol */
	endP = exprs->mCurrPtr;
	chChk = (CT_EALP | CT_NUM | CT_DOT);
	if ( (exprs->mFlags & EXPRS_FLG_LEN_QUALIFIERS) )
		chChk |= CT_COL;
	while ( (cc = *endP) )
	{
		chMask = exprs->chMaskPtr[(int)cc];
		if ( !(chMask & chChk) )
			break;
		++endP;
	}
	symLen = endP - exprs->mCurrPtr;
	if ( !symLen )
		return EXPR_TERM_BAD_SYMBOL_SYNTAX;
	strPtr = getFromStringPool(exprs, symLen + 1);
	if ( !strPtr )
		return EXPR_TERM_BAD_OUT_OF_MEMORY;
	term->term.string = strPtr - libExprsStringPoolTop(exprs);
	memcpy(strPtr, exprs->mCurrPtr, symLen);
	strPtr[symLen] = 0;
	if ( (exprs->mFlags & EXPRS_FLG_LEN_QUALIFIERS) )
	{
		char cc, *cp, *col;
		cp = col = strrchr(strPtr, ':');
		if ( !col )
			cp = strrchr(strPtr, '.');
		if ( cp )
		{
			if ( cp && strlen(cp) == 2 )
			{
				cc = toupper(cp[1]);
				if ( cc == 'B' )
					term->flags |= EXPRS_TERM_FLAG_BYTE;
				else if ( cc == 'W' )
					term->flags |= EXPRS_TERM_FLAG_WORD;
				else if ( cc == 'L' )
					term->flags |= EXPRS_TERM_FLAG_LONG;
				if ( (term->flags & (EXPRS_TERM_FLAG_BYTE | EXPRS_TERM_FLAG_WORD | EXPRS_TERM_FLAG_LONG)) )
					*cp = 0;
			}
		}
		if ( col && !(term->flags & (EXPRS_TERM_FLAG_BYTE | EXPRS_TERM_FLAG_WORD | EXPRS_TERM_FLAG_LONG)) )
		{
			*cp = 0;
			endP = exprs->mCurrPtr + strlen(strPtr);
		}
	}
	term->termType = ttype; /* EXPRS_TERM_SYMBOL; */
	++sPtr->mTermsPool.mNumUsed;
	exprs->mCurrPtr = endP;
	return EXPR_TERM_GOOD;
}

static ExprsTerm_t* getAnotherTerm(ExprsDef_t *exprs, ExprsStack_t *sPtr, int operUsed, int maxOpers)
{
	ExprsTerm_t *term;
	if ( operUsed >= maxOpers )
	{
		char eBuf[256];
		snprintf(eBuf, sizeof(eBuf), "Too many operators for operator stack of %d entries\n", maxOpers);
		showMsg(exprs, EXPRS_SEVERITY_FATAL, eBuf);
		return NULL;
	}
	term = pointToNextTerm(exprs, sPtr);
	memset(term, 0, sizeof(ExprsTerm_t));
	term->chrPtr = exprs->mCurrPtr;
	return term;
}

static void removeCrLf(char *dst, int dstLen, const char *src)
{
	if ( dst && dstLen > 1 )
	{
		while ( dstLen > 1 && *src )
		{
			if ( (*dst = *src++) == '\n' )
				break;
			++dst;
			--dstLen;
		}
		*dst = 0;
	}
}

/** parseExpression - parse the text of the expression
 *  At entry:
 *  @param exprs - pointer to ExprsDef_t returned from
 *  			 libExprsInit()
 *  @param nest - the expression nest level
 *  @param lastTermType - indicates the type of last term
 *                (0=plain, 1=unary, 2=binary)
 *
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
static ExprsErrs_t parseExpression(ExprsDef_t *exprs, int nest, TermType_t lastTermType)
{
	uint16_t chMask = CT_EOL;
	char cc, *operPtr, *sEndp;
	const char *startP, *endP;
	ExprsTerm_t * term, operStack[8];
	ExprsErrs_t err, peRetV;
	bool exitOut = false;
	char eBuf[512];
	int retV;
	TermType_t currTermType;
	ExprsStack_t *sPtr = &exprs->mStack;
	int operUsed; /*, baseStackPtr = sPtr->mTermsPool.mNumUsed; */
	bool getNewTerm;

	if ( exprs->mVerbose )
	{
		char noLf[128 + 64];
		removeCrLf(noLf, sizeof(noLf), exprs->mCurrPtr);
		snprintf(eBuf, sizeof(eBuf), "parseExpression(): Entry. nest=%d, lastTermType=%d, numTerms=%d, expr='%s'\n",
				 nest,
				 lastTermType,
				 sPtr->mTermsPool.mNumUsed,
				 noLf);
		showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
	}
	if ( nest > EXPRS_MAX_NEST )
	{
		/* Sanity check */
		snprintf(eBuf, sizeof(eBuf), "parseExpression(): nest > %d. Something bad must have happened.\n", EXPRS_MAX_NEST);
		showMsg(exprs, EXPRS_SEVERITY_FATAL, eBuf);
		return EXPR_TERM_BAD_TOO_MANY_TERMS;
	}
	exprs->chMaskPtr = (exprs->mFlags & EXPRS_FLG_SPECIAL_UNARY) ? CttblSpecial : CttblNormal;
	peRetV = EXPR_TERM_GOOD;
	operUsed = 0;   /* nothing on the operator stack */
	getNewTerm = true;
	currTermType = TermIsPlain;
	term = NULL;
	while ( 1 )
	{
		int tMask;

		cc = *exprs->mCurrPtr;
		chMask = exprs->chMaskPtr[(int)cc];
		if ( (chMask & (CT_EOL | CT_COM | CT_SMC)) )
		{
			peRetV = EXPR_TERM_END;
			break;
		}
		if ( (chMask & CT_WS) )
		{
			if ( (exprs->mFlags & EXPRS_FLG_WS_DELIMIT) )
			{
				peRetV = EXPR_TERM_END;
				break;
			}
			/* Eat whitespace */
			++exprs->mCurrPtr;
			continue;
		}
		if ( (exprs->mOpenDelimiter != '(' && cc == '(') || (exprs->mCloseDelimiter != ')' && cc == ')') )
		{
			peRetV = EXPR_TERM_END;
			break;
		}
		tMask = (exprs->mFlags & EXPRS_FLG_DOT_SYMBOL) ? (CT_EALP | CT_NUM | CT_OPER | CT_QUO | CT_DOT | CT_AT) : (CT_EALP | CT_NUM | CT_OPER | CT_QUO | CT_AT);
		if ( !(chMask & tMask) )
		{
			return badSyntax(exprs, chMask, cc, EXPR_TERM_BAD_SYNTAX);
		}
		if ( exprs->mVerbose )
		{
			char noLf[128 + 64];
			removeCrLf(noLf, sizeof(noLf), exprs->mCurrPtr);
			snprintf(eBuf, sizeof(eBuf), "parseExpression(): Processing term[%d], cc=%c, chMask=0x%04X, flags=0x%X, operUsed=%d, lastTermType=%d: '%s'\n",
					 sPtr->mTermsPool.mNumUsed,
					 isprint(cc) ? cc : '.',
					 chMask,
					 (getNewTerm || !term) ? 0 : term->flags,
					 operUsed,
					 lastTermType,
					 noLf);
			showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
		}
		if ( (chMask & CT_AT) || ((exprs->mFlags & EXPRS_FLG_WS_DELIMIT) && lastTermType == TermIsPlain && sPtr->mTermsPool.mNumUsed) )
		{
			if ( (chMask & (CT_EALP | CT_NUM | CT_QUO | CT_AT)) || cc == exprs->mOpenDelimiter )
			{
				peRetV = EXPR_TERM_END;
				break;
			}
		}
		if ( getNewTerm )
		{
			term = getAnotherTerm(exprs, sPtr, operUsed, n_elts(operStack));
			if ( !term )
				return EXPR_TERM_BAD_OUT_OF_MEMORY;
			currTermType = TermIsPlain;
			if ( cc == '%' )
			{
				if ( (exprs->mFlags & EXPRS_FLG_PCNT_REGISTER) && lastTermType != TermIsPlain )
				{
					term->flags |= EXPRS_TERM_FLAG_REGISTER;
					++exprs->mCurrPtr;
					getNewTerm = false;
					continue;
				}
			}
		}
		getNewTerm = true;
		if ( (chMask & CT_QUO) )
		{
			/* possibly a quoted string */
			if ( (exprs->mFlags & EXPRS_FLG_NO_DOUBLE_PLAIN) && lastTermType == TermIsPlain )
			{
				peRetV = EXPR_TERM_END;
				break;
			}
			err = handleString(exprs, sPtr, term, cc);
			if ( err )
				return err;
			lastTermType = TermIsPlain;
			continue;
		}
		if ( cc == '$' && (exprs->mFlags & EXPRS_FLG_PRE_DOLLAR_HEX) )
		{
			if ( (exprs->mFlags & EXPRS_FLG_NO_DOUBLE_PLAIN) && lastTermType == TermIsPlain )
			{
				peRetV = EXPR_TERM_END;
				break;
			}
			startP = exprs->mCurrPtr + 1;
			lastTermType = TermIsPlain;
			retV = storeInteger(exprs, sPtr, term, startP, 0, false, 16, "HEX", 0);
			if ( retV )
				return badSyntax(exprs, chMask, cc, retV);
			continue;
		}
		if (    (chMask & CT_EALP)
			 || ( cc == '$' && (exprs->mFlags&EXPRS_FLG_SYM_LEAD_DOLLAR))
			 || ((chMask & CT_DOT) && (exprs->mFlags & EXPRS_FLG_DOT_SYMBOL))
		   )
		{
			if ( (exprs->mFlags & EXPRS_FLG_NO_DOUBLE_PLAIN) && lastTermType == TermIsPlain )
			{
				peRetV = EXPR_TERM_END;
				break;
			}
			retV = handleSymbol(exprs, term, sPtr, EXPRS_TERM_SYMBOL);
			if ( retV )
				return badSyntax(exprs, chMask, cc, retV);
			lastTermType = TermIsPlain;
			continue;
		}
		if ( (chMask & CT_NUM) )
		{
			int lRadix;
			/* number */

			if ( (exprs->mFlags & EXPRS_FLG_NO_DOUBLE_PLAIN) && lastTermType == TermIsPlain )
			{
				peRetV = EXPR_TERM_END;
				break;
			}
			startP = exprs->mCurrPtr;
			if ( cc == '0' )
			{
				char ucc = toupper(exprs->mCurrPtr[1]);
				if ( ucc == 'X' )
				{
					startP += 2;
					lastTermType = TermIsPlain;
					retV = storeInteger(exprs, sPtr, term, startP, 0, false, 16, "HEX", 0);
					if ( retV )
						return badSyntax(exprs, chMask, cc, retV);
					continue;
				}
				if ( ucc == 'O' )
				{
					startP += 2;
					lastTermType = TermIsPlain;
					retV = storeInteger(exprs, sPtr, term, startP, 0, false, 8, "Octal", 0);
					if ( retV )
						return badSyntax(exprs, chMask, cc, retV);
					continue;
				}
#if 0	/* Cannot have these. They conflict with hex numbers. Use ^D or ^B instead */
				if ( ucc == 'D' )
				{
					startP += 2;
					lastTermType = TermIsPlain;
					retV = storeInteger(exprs,sPtr,term,startP,0,false,10,"Decimal",0);
					if ( retV )
					return badSyntax(exprs,chMask,cc,retV);
					continue;
				}
				if ( ucc == 'B' )
				{
					startP += 2;
					lastTermType = TermIsPlain;
					retV = storeInteger(exprs,sPtr,term,startP,0,false,2,"Binary",0);
					if ( retV )
					return badSyntax(exprs,chMask,cc,retV);
					continue;
				}
#endif
			}
			sEndp = NULL;
			term->term.s64 = strtoul(startP, &sEndp, 16);
			if ( !sEndp )
				return badSyntax(exprs, chMask, cc, EXPR_TERM_BAD_NUMBER);
			cc = toupper(*sEndp);
			if ( (exprs->mFlags & EXPRS_FLG_POST_DOLLAR_HEX) && cc == '$' )
			{
				lastTermType = TermIsPlain;
				retV = storeInteger(exprs, sPtr, term, startP, 0, false, 16, "HEX", true);
				if ( retV )
					return badSyntax(exprs, chMask, cc, retV);
				exprs->mCurrPtr = sEndp + 1;
				continue;
			}
			if ( (exprs->mFlags & EXPRS_FLG_H_HEX) && cc == 'H' )
			{
				lastTermType = TermIsPlain;
				retV = storeInteger(exprs, sPtr, term, startP, 0, false, 16, "HEX", true);
				if ( retV )
					return badSyntax(exprs, chMask, cc, retV);
				exprs->mCurrPtr = sEndp + 1;
				continue;
			}
			if (    ((exprs->mFlags & EXPRS_FLG_O_OCTAL) && (cc == 'O'))
				 || ((exprs->mFlags & EXPRS_FLG_Q_OCTAL) && (cc == 'Q'))
			   )
			{
				/* try the number using octal to see if a 'O' or 'Q' follows */
				term->term.s64 = strtoul(startP, &sEndp, 8);
				if ( !sEndp )
					return badSyntax(exprs, chMask, cc, EXPR_TERM_BAD_NUMBER);
				cc = toupper(*sEndp);
				if (    ((exprs->mFlags & EXPRS_FLG_O_OCTAL) && cc == 'O')
					 || ((exprs->mFlags & EXPRS_FLG_Q_OCTAL) && cc == 'Q')
				   )
				{
					/* yep, legit */
					lastTermType = TermIsPlain;
					retV = storeInteger(exprs, sPtr, term, startP, 0, false, 8, "Octal", true);
					if ( retV )
						return badSyntax(exprs, chMask, cc, retV);
					exprs->mCurrPtr = sEndp + 1;
					continue;
				}
			}
			if ( (exprs->mFlags & EXPRS_FLG_LOCAL_SYMBOLS) && cc == '$' )
			{
				term->flags = EXPRS_TERM_FLAG_LOCAL_SYMBOL;
				retV = handleSymbol(exprs, term, sPtr, EXPRS_TERM_SYMBOL);
				if ( retV )
					return badSyntax(exprs, chMask, *startP, retV);
				lastTermType = TermIsPlain;
				continue;
			}
			lRadix = (exprs->mFlags & EXPRS_FLG_USE_RADIX) ? exprs->mRadix : 10;
			if ( (exprs->mFlags & EXPRS_FLG_NO_FLOAT) && cc == '.' )
			{
				lastTermType = TermIsPlain;
				retV = storeInteger(exprs, sPtr, term, startP, '.', true, 10, "Decimal", 0);
				if ( retV )
					return badSyntax(exprs, chMask, cc, retV);
				continue;
			}
			if ( !(exprs->mFlags & EXPRS_FLG_NO_FLOAT) && (lRadix == 10) && (cc == '.' || cc == 'E') )
			{
				sEndp = NULL;
				term->term.f64 = strtod(startP, &sEndp);
				if ( sEndp )
				{
					endP = sEndp;
					if ( exprs->mVerbose )
					{
						snprintf(eBuf, sizeof(eBuf), "parseExpression(): Pushed to terms[%d] a FLOAT %g. operUsed=%d.\n",
								 sPtr->mTermsPool.mNumUsed, term->term.f64, operUsed);
						showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
					}
					exprs->mCurrPtr = endP;
					term->termType = EXPRS_TERM_FLOAT;
					++sPtr->mTermsPool.mNumUsed;
					lastTermType = TermIsPlain;
					continue;
				}
			}
			/* not a number with a 'H' or '$' or 'O' or 'Q' or '.' or 'E' suffix */
			if ( lRadix == 16 )
			{
				/* already converted text to hex */
				lastTermType = TermIsPlain;
				retV = storeInteger(exprs, sPtr, term, startP, 0, false, 16, "HEX", true);
				if ( retV )
					return badSyntax(exprs, chMask, *startP, retV);
				exprs->mCurrPtr = sEndp;
				continue;
			}
			/* re-convert according to desired radix */
			term->term.s64 = strtoul(startP, &sEndp, lRadix);
			if ( !sEndp )
				return badSyntax(exprs, chMask, cc, EXPR_TERM_BAD_NUMBER);
			endP = sEndp;
			if ( exprs->mVerbose )
			{
				snprintf(eBuf, sizeof(eBuf), "parseExpression(): Pushed to terms[%d] a plain Integer %ld. operUsed=%d. flags=0x%X, mRadix=%d\n",
						 sPtr->mTermsPool.mNumUsed, term->term.s64, operUsed, term->flags, exprs->mRadix);
				showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
			}
			exprs->mCurrPtr = endP;
			term->termType = EXPRS_TERM_INTEGER;
			++sPtr->mTermsPool.mNumUsed;
			lastTermType = TermIsPlain;
			continue;
		}
		if ( cc == exprs->mOpenDelimiter )
		{
			if ( nest < 1 && (exprs->mFlags & EXPRS_FLG_OPEN_IS_END) && !lastTermType && exprs->mStack.mTermsPool.mNumUsed > 0 )
			{
				peRetV = EXPR_TERM_END;
				break;
			}
			if ( (exprs->mFlags & EXPRS_FLG_NO_DOUBLE_PLAIN) && lastTermType == TermIsPlain )
			{
				peRetV = EXPR_TERM_END;
				break;
			}
			/* recurse stuff new expression into new stack */
			++exprs->mCurrPtr;  /* eat delimiter */
			err = parseExpression(exprs, nest + 1, lastTermType);
			if ( err )
				return err;
			lastTermType = TermIsPlain;
			continue;
		}
		if ( cc == exprs->mCloseDelimiter )
		{
			if ( nest < 1 )
			{
				if ( (exprs->mFlags & EXPRS_FLG_CLOSE_IS_END) )
				{
					peRetV = EXPR_TERM_END;
					break;
				}
				snprintf(eBuf, sizeof(eBuf), "parseExpression(): Syntax error. cc='%c', chMask=0x%04X, nest=%d\n", isprint(cc) ? cc : '.', chMask, nest);
				showMsg(exprs, EXPRS_SEVERITY_ERROR, eBuf);
				return EXPR_TERM_BAD_SYNTAX;
			}
			--nest;
			++exprs->mCurrPtr;  /* eat delimiter */
			exitOut = true;
			cc = 0;     /* force following switch() to do nothing */
			lastTermType = TermIsPlain;
		}
		operPtr = term->term.oper;
		switch (cc)
		{
		case 0:
			break;
		case '+':
			if ( lastTermType == TermIsPlain )
			{
				term->termType = EXPRS_TERM_ADD;
				currTermType = TermIsBinary;
			}
			else
			{
				term->termType = EXPRS_TERM_POS;
				currTermType = TermIsUnary;
			}
			*operPtr++ = cc;
			break;
		case '-':
			if ( lastTermType == TermIsPlain )
			{
				term->termType = EXPRS_TERM_SUB;
				currTermType = TermIsBinary;
			}
			else
			{
				term->termType = EXPRS_TERM_NEG;
				currTermType = TermIsUnary;
			}
			*operPtr++ = cc;
			break;
		case '*':
			*operPtr++ = cc;
			if ( exprs->mCurrPtr[1] == '*' )
			{
				if ( (exprs->mFlags & EXPRS_FLG_NO_POWER) )
					return badSyntax(exprs, chMask, cc, EXPR_TERM_BAD_SYNTAX);
				term->termType = EXPRS_TERM_POW;
				*operPtr++ = cc;
				++exprs->mCurrPtr;
			}
			else
				term->termType = EXPRS_TERM_MUL;
			currTermType = TermIsBinary;
			break;
		case '/':
			term->termType = EXPRS_TERM_DIV;
			*operPtr++ = cc;
			currTermType = TermIsBinary;
			break;
		case '%':
			term->termType = EXPRS_TERM_MOD;
			*operPtr++ = cc;
			currTermType = TermIsBinary;
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
			currTermType = TermIsBinary;
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
			currTermType = TermIsBinary;
			break;
		case '^':
			if ( lastTermType != TermIsPlain )
			{
				++exprs->mCurrPtr;  /* eat the up arrow*/
				cc = *exprs->mCurrPtr;
				cc = toupper(cc);   /* Expecting one of B, C, D, H, O, V, X, ~ or ^ */
				startP = exprs->mCurrPtr + 1;
				switch (cc)
				{
				case 'B':
					retV = storeInteger(exprs, sPtr, term, startP, 0, false, 2, "Binary", 0);
					if ( retV )
						return badSyntax(exprs, chMask, cc, retV);
					lastTermType = TermIsPlain;
					continue;
				case 'C':
					term->termType = EXPRS_TERM_COM;
					*operPtr++ = '~';
					currTermType = TermIsUnary;
					break;
				case 'D':
					retV = storeInteger(exprs, sPtr, term, startP, 0, false, 10, "Decimal", 0);
					if ( retV )
						return badSyntax(exprs, chMask, cc, retV);
					lastTermType = TermIsPlain;
					continue;
				case 'X':
				case 'H':
					retV = storeInteger(exprs, sPtr, term, startP, 0, false, 16, "Hex", 0);
					if ( retV )
						return badSyntax(exprs, chMask, cc, retV);
					lastTermType = TermIsPlain;
					continue;
				case 'O':
					retV = storeInteger(exprs, sPtr, term, startP, 0, false, 8, "Octal", 0);
					if ( retV )
						return badSyntax(exprs, chMask, cc, retV);
					lastTermType = TermIsPlain;
					continue;
				case 'V':
					term->termType = EXPRS_TERM_LOW_BYTE;
					*operPtr++ = '^';
					*operPtr++ = 'V';
					currTermType = TermIsUnary;
					break;
				case '~':
					term->termType = EXPRS_TERM_SWAP;
					*operPtr++ = '^';
					*operPtr++ = '~';
					currTermType = TermIsUnary;
					break;
				case '^':
					term->termType = EXPRS_TERM_HIGH_BYTE;
					*operPtr++ = '^';
					*operPtr++ = '^';
					currTermType = TermIsUnary;
					break;
				default:
					return badSyntax(exprs, chMask, cc, EXPR_TERM_BAD_SYNTAX);
				}
				break;
			}
			if ( (exprs->mFlags&EXPRS_FLG_SPECIAL_UNARY) )
				return badSyntax(exprs, chMask, cc, EXPR_TERM_BAD_SYNTAX);
			term->termType = EXPRS_TERM_XOR;
			*operPtr++ = cc;
			currTermType = TermIsBinary;
			break;
		case '?':
			if ( !(exprs->mFlags & EXPRS_FLG_SPECIAL_UNARY) )
				return badSyntax(exprs, chMask, cc, EXPR_TERM_BAD_SYNTAX);
			term->termType = EXPRS_TERM_XOR;
			*operPtr++ = cc;
			currTermType = TermIsBinary;
			break;
		case '~':
			term->termType = EXPRS_TERM_COM;
			*operPtr++ = cc;
			currTermType = TermIsUnary;
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
			currTermType = TermIsBinary;
			break;
		case '=':
			*operPtr++ = cc;
			if ( exprs->mCurrPtr[1] == '=' )
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
			if ( (exprs->mFlags & EXPRS_FLG_NO_LOGICALS) )
				return EXPR_TERM_BAD_SYNTAX;
			*operPtr++ = cc;
			if ( exprs->mCurrPtr[1] == '<' )
			{
				term->termType = EXPRS_TERM_SHL;
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
			currTermType = TermIsBinary;
			break;
		case '{':
			if ( !(exprs->mFlags & EXPRS_FLG_SPECIAL_UNARY) )
				return badSyntax(exprs, chMask, cc, EXPR_TERM_BAD_SYNTAX);
			term->termType = EXPRS_TERM_SHL;
			*operPtr++ = '<';
			currTermType = TermIsBinary;
			break;
		case '}':
			if ( !(exprs->mFlags & EXPRS_FLG_SPECIAL_UNARY) )
				return badSyntax(exprs, chMask, cc, EXPR_TERM_BAD_SYNTAX);
			term->termType = EXPRS_TERM_SHR;
			*operPtr++ = '>';
			currTermType = TermIsBinary;
			break;
		case '>':
			*operPtr++ = cc;
			if ( (exprs->mFlags & EXPRS_FLG_NO_LOGICALS) )
				return badSyntax(exprs, chMask, cc, EXPR_TERM_BAD_SYNTAX);
			if ( exprs->mCurrPtr[1] == '>' )
			{
				term->termType = EXPRS_TERM_SHR;
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
			currTermType = TermIsBinary;
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
			ExprsTerm_t saveTerm, *operTop;
			ExprsPrecedence_t oldPrecedence, currPrecedence;

			/* Found an actual operator */
			*operPtr = 0;       /* null terminate the operator text string */
			currPrecedence = exprs->precedencePtr[term->termType];
			if ( exprs->mVerbose )
			{
				snprintf(eBuf, sizeof(eBuf), "parseExpression(): Before precedence. Checking type %d ('%s') precedence %d, operUsed=%d, currTermType=%d, lastTermType=%d ...\n",
						 term->termType, term->term.oper, currPrecedence, operUsed, currTermType, lastTermType);
				showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
				dumpStack(exprs, operStack, operUsed);
			}
#if 0
			/* check for the operators of higher precedence and then add them to stack */
			if ( sPtr->mTermsPool.mNumUsed && (lastTermType == TermIsPlain || lastTermType == TermIsUnary) && currTermType == TermIsUnary )
			{
				/* Unary operators always have precedence */
				/* If the last one was plain or unary and this one is unary, then just keep it on the stack */
				if ( exprs->mVerbose )
				{
					snprintf(eBuf,sizeof(eBuf),"parseExpression(): Pushed unary operator '%s'(%d) to stack[%d]\n",
							 term->term.oper,
							 term->termType,
							 sPtr->mTermsPool.mNumUsed);
					showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
				}
				++sPtr->mTermsPool.mNumUsed;
				lastTermType = TermIsUnary; /* make a note we stuffed a unary */
				if ( *exprs->mCurrPtr )
				++exprs->mCurrPtr;
				continue;
			}
#endif
			/* If what is currently in the operator stack (if anything) has a higher precedence than the operator to be pushed,
			 * then pop the item(s) off the operator stack onto the term stack. Then push the new operator onto the operator stack.
			 */
			saveTerm = *term;   /* save the term contents */
			operTop = operStack + operUsed;
			while ( sPtr->mTermsPool.mNumUsed && operUsed > 0 )
			{
				--operTop;
				if ( (oldPrecedence = exprs->precedencePtr[operTop->termType]) < currPrecedence )
					break;
				*term = *operTop;
				if ( exprs->mVerbose )
				{
					snprintf(eBuf, sizeof(eBuf), "parseExpression(): Precedence popped from operators[%d] a '%s'(%d) and pushed it to terms[%d]. Precedence: curr=%d, new=%d\n",
							 operUsed,
							 term->term.oper,
							 term->termType,
							 sPtr->mTermsPool.mNumUsed,
							 currPrecedence, oldPrecedence);
					showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
				}
				--operUsed;
				++sPtr->mTermsPool.mNumUsed;
				term = pointToNextTerm(exprs, sPtr);
			}
			operTop = operStack + operUsed;
			*operTop = saveTerm;
			if ( exprs->mVerbose )
			{
				snprintf(eBuf, sizeof(eBuf), "parseExpression(): Pushed operator '%s'(%d) to operators[%d]\n",
						 term->term.oper,
						 term->termType,
						 operUsed);
				showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
			}
			++operUsed;
			lastTermType = currTermType;
		}
		else
		{
			lastTermType = TermIsPlain;
			++sPtr->mTermsPool.mNumUsed;
		}
		if ( *exprs->mCurrPtr )
			++exprs->mCurrPtr;
	}
	/* reached end of expression string */
	/* pop all the operators off the operator stack onto the term stack */
	while ( operUsed > 0 )
	{
		ExprsTerm_t *oper;
		--operUsed;
		oper = operStack + operUsed;
		term = pointToNextTerm(exprs, sPtr);
		*term = *oper;
		if ( exprs->mVerbose )
		{
			snprintf(eBuf, sizeof(eBuf), "parseExpression(): Popped '%s'(%d) from operStack[%d] and pushed it to terms[%d]\n",
					 term->term.oper,
					 term->termType,
					 operUsed,
					 sPtr->mTermsPool.mNumUsed);
			showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
		}
		++sPtr->mTermsPool.mNumUsed;
	}
	if ( exitOut && exprs->mVerbose )
	{
		snprintf(eBuf, sizeof(eBuf), "parseExpression(): Found ')'. mNumTermsUsed now is %d\n",
				 sPtr->mTermsPool.mNumUsed);
		showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
		dumpStack(exprs, operStack, operUsed);
	}
	return peRetV;
}

static ExprsErrs_t lookupSymbol(ExprsDef_t *exprs, const ExprsTerm_t *src, ExprsTerm_t *dst)
{
	ExprsSymTerm_t ans;
	ExprsErrs_t err;
	char *fromPtr, *toPtr, eBuf[512];
	int strLen;

	dst->chrPtr = NULL;
	dst->flags = 0;
	dst->term.f64 = 0;
	dst->user1 = NULL;
	if ( !exprs->mCallbacks.symGet )
		return EXPR_TERM_BAD_NO_SYMBOLS;
	fromPtr = libExprsStringPoolTop(exprs) + src->term.string;
	err = exprs->mCallbacks.symGet(exprs->mCallbacks.symArg, fromPtr, &ans);
	if ( !err )
	{
		switch (ans.termType)
		{
		default:
			snprintf(eBuf, sizeof(eBuf), "lookupSymbol(): Found symbol '%s' with illegal term type %d.\n",
					 fromPtr, ans.termType);
			showMsg(exprs, EXPRS_SEVERITY_ERROR, eBuf);
			return EXPR_TERM_BAD_UNSUPPORTED;
		case EXPRS_SYM_TERM_COMPLEX:
			dst->user1 = ans.user1;
			dst->term.s64 = ans.value.s64;
			dst->flags |= EXPRS_TERM_FLAG_COMPLEX;
			break;
		case EXPRS_SYM_TERM_STRING:
			strLen = strlen(ans.value.string) + 1;
			if ( !(toPtr = getFromStringPool(exprs, strLen)) )
				return EXPR_TERM_BAD_OUT_OF_MEMORY;
			strncpy(toPtr, ans.value.string, strLen);
			dst->term.string = toPtr - libExprsStringPoolTop(exprs);
			break;
		case EXPRS_SYM_TERM_FLOAT:
			dst->term.f64 = ans.value.f64;
			break;
		case EXPRS_SYM_TERM_INTEGER:
			dst->term.s64 = ans.value.s64;
			break;
		}
		dst->termType = (ExprsTermTypes_t)ans.termType;
		dst->flags = ans.flags;
		return EXPR_TERM_GOOD;
	}
	return EXPR_TERM_BAD_UNDEFINED_SYMBOL;
}

static ExprsErrs_t procSingleSymbol(ExprsDef_t *exprs, ExprsTerm_t *term)
{
	ExprsErrs_t err;
	if ( term->termType == EXPRS_TERM_SYMBOL )
	{
		err = lookupSymbol(exprs, term, term);
		if ( err != EXPR_TERM_GOOD )
			return err;
	}
	return EXPR_TERM_GOOD;
}

static ExprsErrs_t procSymbols(ExprsDef_t *exprs, ExprsTerm_t *aa, ExprsTerm_t *bb)
{
	ExprsErrs_t err;
	err = procSingleSymbol(exprs, aa);
	if ( !err )
		err = procSingleSymbol(exprs, bb);
	return err;
}

/* Computes dst = aa+bb */
static ExprsErrs_t doAdd(ExprsDef_t *exprs, ExprsTerm_t *dst, ExprsTerm_t *aa, ExprsTerm_t *bb)
{
	ExprsErrs_t err;
	int sLen;
	char *newStr;
	char eBuf[512];

	if ( exprs->mVerbose )
	{
		snprintf(eBuf, sizeof(eBuf), "doAdd(): entry aaType %d, aaValue 0x%lX, bbType %d, bbValue 0x%lX\n",
				 aa->termType, aa->term.s64, bb->termType, bb->term.s64);
		showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
	}
	if ( (err = procSymbols(exprs, aa, bb)) )
		return err;
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
			sLen = strlen(libExprsStringPoolTop(exprs) + bb->term.string) + 1 + 32;
			newStr = getFromStringPool(exprs, sLen);
			if ( !newStr )
			{
				err = EXPR_TERM_BAD_OUT_OF_MEMORY;
				break;
			}
			snprintf(newStr, sLen, "%g%s", aa->term.f64, libExprsStringPoolTop(exprs) + bb->term.string);
			dst->termType = EXPRS_TERM_STRING;
			dst->term.string = newStr - libExprsStringPoolTop(exprs);
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
			sLen = strlen(libExprsStringPoolTop(exprs) + bb->term.string) + 1 + 32;
			newStr = getFromStringPool(exprs, sLen);
			if ( !newStr )
			{
				err = EXPR_TERM_BAD_OUT_OF_MEMORY;
				break;
			}
			snprintf(newStr, sLen, "%ld%s", aa->term.s64, libExprsStringPoolTop(exprs) + bb->term.string);
			dst->termType = EXPRS_TERM_STRING;
			dst->term.string = newStr - libExprsStringPoolTop(exprs);
			err = EXPR_TERM_GOOD;
			break;
		default:
			break;
		}
		break;
	case EXPRS_TERM_STRING:
		sLen = strlen(libExprsStringPoolTop(exprs) + aa->term.string);
		switch (bb->termType)
		{
		case EXPRS_TERM_FLOAT:
		case EXPRS_TERM_INTEGER:
			sLen += 32 + 1;
			newStr = getFromStringPool(exprs, sLen);
			if ( !newStr )
			{
				err = EXPR_TERM_BAD_OUT_OF_MEMORY;
				break;
			}
			if ( bb->termType == EXPRS_TERM_FLOAT )
				snprintf(newStr, sLen, "%s%g", libExprsStringPoolTop(exprs) + aa->term.string, bb->term.f64);
			else
				snprintf(newStr, sLen, "%s%ld", libExprsStringPoolTop(exprs) + aa->term.string, bb->term.s64);
			dst->term.string = newStr - libExprsStringPoolTop(exprs);
			err = EXPR_TERM_GOOD;
			break;
		case EXPRS_TERM_STRING:
			sLen += strlen(libExprsStringPoolTop(exprs) + bb->term.string) + 1;
			newStr = getFromStringPool(exprs, sLen);
			if ( !newStr )
			{
				err = EXPR_TERM_BAD_OUT_OF_MEMORY;
				break;
			}
			snprintf(newStr, sLen, "%s%s",
					 libExprsStringPoolTop(exprs) + aa->term.string,
					 libExprsStringPoolTop(exprs) + bb->term.string);
			dst->term.string = newStr - libExprsStringPoolTop(exprs);
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
		snprintf(eBuf, sizeof(eBuf), "doAdd(): Syntax error. aaType=%d, bbType=%d. At '%s'\n", aa->termType, bb->termType, aa->chrPtr);
		showMsg(exprs, EXPRS_SEVERITY_ERROR, eBuf);
	}
	else if ( exprs->mVerbose )
	{
		snprintf(eBuf, sizeof(eBuf), "doAdd(): exit dstType %d, dstValue 0x%lX\n", dst->termType, dst->term.s64);
		showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
	}
	return err;
}

/*  Computes dst = aa-bb */
static ExprsErrs_t doSub(ExprsDef_t *exprs, ExprsTerm_t *dst, ExprsTerm_t *aa, ExprsTerm_t *bb)
{
	ExprsErrs_t err;
	char eBuf[512];

	if ( exprs->mVerbose )
	{
		snprintf(eBuf, sizeof(eBuf), "doSub(): entry aaType %d, aaValue 0x%lX, bbType %d, bbValue 0x%lX\n", aa->termType, aa->term.s64, bb->termType, bb->term.s64);
		showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
	}
	if ( (err = procSymbols(exprs, aa, bb)) )
		return err;
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
		snprintf(eBuf, sizeof(eBuf), "doSub(): Syntax error. aaType=%d, bbType=%d. At '%s'\n", aa->termType, bb->termType, aa->chrPtr);
		showMsg(exprs, EXPRS_SEVERITY_ERROR, eBuf);
	}
	else if ( exprs->mVerbose )
	{
		snprintf(eBuf, sizeof(eBuf), "doSub(): exit dstType %d, dstValue 0x%lX\n", dst->termType, dst->term.s64);
		showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
	}
	return err;
}

/* Computes dst = aa**bb */
static ExprsErrs_t doPow(ExprsDef_t *exprs, ExprsTerm_t *dst, ExprsTerm_t *aa, ExprsTerm_t *bb)
{
	ExprsErrs_t err;
	char eBuf[512];

	if ( exprs->mVerbose )
	{
		snprintf(eBuf, sizeof(eBuf), "doPow(): entry aaType %d, aaValue 0x%lX, bbType %d, bbValue 0x%lX\n", aa->termType, aa->term.s64, bb->termType, bb->term.s64);
		showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
	}
#if !NO_FLOATING_POINT
	if ( (err = procSymbols(exprs, aa, bb)) )
		return err;
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
			dst->term.f64 = pow(aa->term.f64, bb->term.f64);
			err = EXPR_TERM_GOOD;
			break;
		case EXPRS_TERM_INTEGER:
			/* when raising a float by an integer, result is float */
			dst->term.f64 = pow(aa->term.f64, bb->term.s64);
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
			dst->term.f64 = pow(aa->term.s64, bb->term.f64);
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
#else
	err = EXPR_TERM_BAD_UNSUPPORTED;
#endif
	if ( err != EXPR_TERM_GOOD )
	{
#if !NO_FLOATING_POINT
#define POW_MSG "Syntax"
#else
#define POW_MSG "Unsupported pow()"
#endif
		snprintf(eBuf, sizeof(eBuf), "doPow(): " POW_MSG " error. aaType=%d, bbType=%d. At '%s'\n", aa->termType, bb->termType, aa->chrPtr);
		showMsg(exprs, EXPRS_SEVERITY_ERROR, eBuf);
	}
#if !NO_FLOATING_POINT
	else if ( exprs->mVerbose )
	{
		snprintf(eBuf, sizeof(eBuf), "doPow(): exit dstType %d, dstValue 0x%lX\n", dst->termType, dst->term.s64);
		showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
	}
#endif
	return err;
}

/* Computes dst = aa*bb */
static ExprsErrs_t doMul(ExprsDef_t *exprs, ExprsTerm_t *dst, ExprsTerm_t *aa, ExprsTerm_t *bb)
{
	ExprsErrs_t err;
	char eBuf[512];

	if ( exprs->mVerbose )
	{
		snprintf(eBuf, sizeof(eBuf), "doMul(): entry aaType %d, aaValue 0x%lX, bbType %d, bbValue 0x%lX\n", aa->termType, aa->term.s64, bb->termType, bb->term.s64);
		showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
	}
	if ( (err = procSymbols(exprs, aa, bb)) )
		return err;
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
		snprintf(eBuf, sizeof(eBuf), "doMul(): Syntax error. aaType=%d, bbType=%d. At '%s'\n", aa->termType, bb->termType, aa->chrPtr);
		showMsg(exprs, EXPRS_SEVERITY_ERROR, eBuf);
	}
	else if ( exprs->mVerbose )
	{
		snprintf(eBuf, sizeof(eBuf), "doMul(): exit dstType %d, dstValue 0x%lX\n", dst->termType, dst->term.s64);
		showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
	}
	return err;
}

/* Computes dst = aa/bb */
static ExprsErrs_t doDiv(ExprsDef_t *exprs, ExprsTerm_t *dst, ExprsTerm_t *aa, ExprsTerm_t *bb)
{
	ExprsErrs_t err;
	double bbV;
	char eBuf[512];

	if ( exprs->mVerbose )
	{
		snprintf(eBuf, sizeof(eBuf), "doDiv(): entry aaType %d, aaValue 0x%lX, bbType %d, bbValue 0x%lX\n", aa->termType, aa->term.s64, bb->termType, bb->term.s64);
		showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
	}
	if ( (err = procSymbols(exprs, aa, bb)) )
		return err;
	bbV = 0.0;
	if ( bb->termType == EXPRS_TERM_FLOAT || bb->termType == EXPRS_TERM_INTEGER )
		bbV = (bb->termType == EXPRS_TERM_FLOAT) ? bb->term.f64 : bb->term.s64;
	if ( bbV == 0.0 )
	{
		double aaV;
		aaV = 0.0;
		if ( aa->termType == EXPRS_TERM_FLOAT || aa->termType == EXPRS_TERM_INTEGER )
			aaV = (bb->termType == EXPRS_TERM_FLOAT) ? aa->term.f64 : aa->term.s64;
		snprintf(eBuf, sizeof(eBuf), "doDiv(): bb term is 0.0, aa term is %g. Divide by 0 error at or near %s\n", aaV, aa->chrPtr);
		showMsg(exprs, EXPRS_SEVERITY_ERROR, eBuf);
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
		snprintf(eBuf, sizeof(eBuf), "doDiv(): Syntax error. aaType=%d, bbType=%d. At or near '%s'\n", aa->termType, bb->termType, aa->chrPtr);
		showMsg(exprs, EXPRS_SEVERITY_ERROR, eBuf);
	}
	else if ( exprs->mVerbose )
	{
		snprintf(eBuf, sizeof(eBuf), "doDiv(): exit dstType %d, dstValue 0x%lX\n", dst->termType, dst->term.s64);
		showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
	}
	return err;
}

/* Computes dst = aa%bb */
static ExprsErrs_t doMod(ExprsDef_t *exprs, ExprsTerm_t *dst, ExprsTerm_t *aa, ExprsTerm_t *bb)
{
	ExprsErrs_t err;
	double bbV;
	char eBuf[512];

	if ( exprs->mVerbose )
	{
		snprintf(eBuf, sizeof(eBuf), "doMod(): entry aaType %d, aaValue 0x%lX, bbType %d, bbValue 0x%lX\n", aa->termType, aa->term.s64, bb->termType, bb->term.s64);
		showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
	}
	if ( (err = procSymbols(exprs, aa, bb)) )
		return err;
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
		snprintf(eBuf, sizeof(eBuf), "doMod(): bb term is 0.0, aa term is %g. Divide by 0 error.\n", aaV);
		showMsg(exprs, EXPRS_SEVERITY_ERROR, eBuf);
		return EXPR_TERM_BAD_DIV_BY_0;
	}
	switch (aa->termType)
	{
	case EXPRS_TERM_FLOAT:
#if !NO_FLOATING_POINT
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
#else
		err = EXPR_TERM_BAD_UNSUPPORTED;
#endif
		break;
	case EXPRS_TERM_INTEGER:
		switch (bb->termType)
		{
		case EXPRS_TERM_FLOAT:
#if !NO_FLOATING_POINT
			dst->term.f64 = fmod(aa->term.s64, bb->term.f64);
			dst->termType = EXPRS_TERM_FLOAT;
			err = EXPR_TERM_GOOD;
#else
			err = EXPR_TERM_BAD_UNSUPPORTED;
#endif
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
		snprintf(eBuf, sizeof(eBuf), "doMod(): Syntax error. aaType=%d, bbType=%d. At '%s'\n", aa->termType, bb->termType, aa->chrPtr);
		showMsg(exprs, EXPRS_SEVERITY_ERROR, eBuf);
	}
	else if ( exprs->mVerbose )
	{
		snprintf(eBuf, sizeof(eBuf), "doMod(): exit dstType %d, dstValue 0x%lX\n", dst->termType, dst->term.s64);
		showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
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
		snprintf(eBuf, sizeof(eBuf), "prepUnaryTerm(): Item %d: %s, aa=%s\n",
				 params->ii,
				 showTermType(params->exprs, params->sPtr, params->term, params->tmpBuf0, sizeof(params->tmpBuf0) - 1),
				 showTermType(params->exprs, params->sPtr, params->aa, params->tmpBuf1, sizeof(params->tmpBuf1) - 1));
		showMsg(params->exprs, EXPRS_SEVERITY_INFO, eBuf);
	}
	if ( params->aa->termType == EXPRS_TERM_SYMBOL )
	{
		err = lookupSymbol(params->exprs, params->aa, params->aa);
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
		snprintf(eBuf, sizeof(eBuf), "prepBinaryTerms(): Item %d: %s, aa=%s, bb=%s\n",
				 params->ii,
				 showTermType(params->exprs, params->sPtr, params->term, params->tmpBuf0, sizeof(params->tmpBuf0) - 1),
				 showTermType(params->exprs, params->sPtr, params->aa, params->tmpBuf1, sizeof(params->tmpBuf1) - 1),
				 showTermType(params->exprs, params->sPtr, params->bb, params->tmpBuf2, sizeof(params->tmpBuf2) - 1));
		showMsg(params->exprs, EXPRS_SEVERITY_INFO, eBuf);
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

static ExprsErrs_t computeViaRPN(ExprsDef_t *exprs, int nest, ExprsTerm_t *returnResult)
{
	TermParams_t params;
	ExprsTerm_t * term,*dst;
	ExprsErrs_t err;
	ExprsTermTypes_t tType;
	ExprsSymTerm_t ans;
	ExprsStack_t *sPtr = &exprs->mStack;
	int ii;
	char eBuf[512];

	if ( exprs->mVerbose )
	{
		snprintf(eBuf, sizeof(eBuf), "Into computeViaRPN(): nest=%d. Terms=%d\n",
				 nest, sPtr->mTermsPool.mNumUsed);
		showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
	}
	returnResult->termType = EXPRS_TERM_NULL;
	if ( !sPtr->mTermsPool.mNumUsed )
		return EXPR_TERM_BAD_TOO_FEW_TERMS;
	params.exprs = exprs;
	params.sPtr = sPtr;
	params.rTop = -1;
	for ( ii = 0; ii < sPtr->mTermsPool.mNumUsed; ++ii )
	{
		params.term = term = libExprsTermPoolTop(exprs, sPtr) + ii;
		params.ii = ii;
		returnResult->chrPtr = term->chrPtr;
		tType =  term->termType;
		if ( exprs->mVerbose )
		{
			snprintf(eBuf, sizeof(eBuf), "computeViaRPN(): Item %d. type=%d. rTop=%d\n", ii, term->termType, params.rTop);
			showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
		}
		if ( params.rTop >= 0 )
			params.aa = params.results + params.rTop;
		else
			params.aa = NULL;
		params.bb = NULL;
		switch (tType)
		{
		case EXPRS_TERM_NULL:
			continue;
		case EXPRS_TERM_SYMBOL_COMPLEX:
		case EXPRS_TERM_SYMBOL:
			if ( !exprs->mCallbacks.symGet )
			{
				snprintf(eBuf, sizeof(eBuf), "computeViaRPN(): No symbol table established. Cannot handle symbols at or near %s.\n", term->chrPtr);
				showMsg(exprs, EXPRS_SEVERITY_ERROR, eBuf);
				return EXPR_TERM_BAD_NO_SYMBOLS;
			}
			/* Fall through to normal stack function */
		case EXPRS_TERM_FUNCTION:
		case EXPRS_TERM_STRING:
		case EXPRS_TERM_INTEGER:
		case EXPRS_TERM_FLOAT:
			if ( params.rTop >= n_elts(params.results) - 1 )
				return EXPR_TERM_BAD_TOO_MANY_TERMS;
			if ( exprs->mVerbose )
			{
				snprintf(eBuf, sizeof(eBuf), "computeViaRPN(): Item %d: %s\n",
						 ii,
						 showTermType(exprs, sPtr, term, params.tmpBuf0, sizeof(params.tmpBuf0) - 1));
				showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
			}
			params.results[++params.rTop] = *term;
			continue;
		case EXPRS_TERM_POS:   /* + */
			if ( (err = prepUnaryTerm(&params, false)) )
				return err;
			if ( exprs->mVerbose )
			{
				snprintf(eBuf, sizeof(eBuf), "computeViaRPN(): PLUS Item %d: %s, aa=%s\n",
						 ii,
						 showTermType(exprs, sPtr, term, params.tmpBuf0, sizeof(params.tmpBuf0) - 1),
						 showTermType(exprs, sPtr, params.aa, params.tmpBuf1, sizeof(params.tmpBuf1) - 1));
				showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
			}
			continue;           /* Just eat a extraneous plus sign */
		case EXPRS_TERM_NEG:  /* - */
			if ( (err = prepUnaryTerm(&params, false)) )
				return err;
			if ( exprs->mVerbose )
			{
				snprintf(eBuf, sizeof(eBuf), "computeViaRPN(): MINUS Item %d: %s, aa=%s\n",
						 ii,
						 showTermType(exprs, sPtr, term, params.tmpBuf0, sizeof(params.tmpBuf0) - 1),
						 showTermType(exprs, sPtr, params.aa, params.tmpBuf1, sizeof(params.tmpBuf1) - 1));
				showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
			}
			if ( params.aa->termType == EXPRS_TERM_INTEGER )
				params.aa->term.s64 = -params.aa->term.s64;
			else if ( params.aa->termType == EXPRS_TERM_FLOAT )
				params.aa->term.f64 = -params.aa->term.f64;
			else
				return EXPR_TERM_BAD_SYNTAX;
			continue;
		case EXPRS_TERM_ADD:    /* + */
			if ( (err = prepBinaryTerms(&params, false)) )
				return err;
			if ( (err = doAdd(exprs, params.aa, params.aa, params.bb)) > EXPR_TERM_END )
				return err;
			continue;
		case EXPRS_TERM_SUB:    /* - */
			if ( (err = prepBinaryTerms(&params, false)) )
				return err;
			if ( (err = doSub(exprs, params.aa, params.aa, params.bb)) > EXPR_TERM_END )
				return err;
			continue;
		case EXPRS_TERM_POW:    /* ** */
			if ( (err = prepBinaryTerms(&params, false)) )
				return err;
			err = doPow(exprs, params.aa, params.aa, params.bb);
			if ( err > EXPR_TERM_END )
				return err;
			continue;
		case EXPRS_TERM_MUL:    /* * */
			if ( (err = prepBinaryTerms(&params, false)) )
				return err;
			err = doMul(exprs, params.aa, params.aa, params.bb);
			if ( err > EXPR_TERM_END )
				return err;
			continue;
		case EXPRS_TERM_DIV:    /* / */
			if ( (err = prepBinaryTerms(&params, false)) )
				return err;
			err = doDiv(exprs, params.aa, params.aa, params.bb);
			if ( err > EXPR_TERM_END )
				return err;
			continue;
		case EXPRS_TERM_MOD:    /* % */
			if ( (err = prepBinaryTerms(&params, false)) )
				return err;
			err = doMod(exprs, params.aa, params.aa, params.bb);
			if ( err > EXPR_TERM_END )
				return err;
			continue;
		case EXPRS_TERM_COM:    /* ~ */
			if ( (err = prepUnaryTerm(&params, true)) )
				return err;
			if ( params.aa->termType == EXPRS_TERM_INTEGER )
				params.aa->term.s64 = ~params.aa->term.s64;
			else
				return EXPR_TERM_BAD_SYNTAX;
			continue;
		case EXPRS_TERM_NOT:    /* ! */
			if ( (err = prepUnaryTerm(&params, true)) )
				return err;
			if ( params.aa->termType == EXPRS_TERM_INTEGER )
				params.aa->term.s64 = params.aa->term.s64 ? 0 : 1;
			else
				return EXPR_TERM_BAD_SYNTAX;
			continue;
		case EXPRS_TERM_LOW_BYTE:
			if ( (err = prepUnaryTerm(&params, true)) )
				return err;
			if ( params.aa->termType == EXPRS_TERM_INTEGER )
				params.aa->term.u64 = params.aa->term.u64 & 0xFF;
			else
				return EXPR_TERM_BAD_SYNTAX;
			continue;
		case EXPRS_TERM_HIGH_BYTE:
			if ( (err = prepUnaryTerm(&params, true)) )
				return err;
			if ( params.aa->termType == EXPRS_TERM_INTEGER )
				params.aa->term.u64 = (params.aa->term.u64 >> 8) & 0xFF;
			else
				return EXPR_TERM_BAD_SYNTAX;
			continue;
		case EXPRS_TERM_SWAP:
			if ( (err = prepUnaryTerm(&params, true)) )
				return err;
			if ( params.aa->termType == EXPRS_TERM_INTEGER )
			{
				unsigned long ul = params.aa->term.u64;
				params.aa->term.u64 = ((ul >> 8) & 0xFF) | ((ul << 8) & 0xFF00);
			}
			else
				return EXPR_TERM_BAD_SYNTAX;
			continue;
		case EXPRS_TERM_SHL:    /* << */
			if ( (err = prepBinaryTerms(&params, true)) )
				return err;
			if ( params.aa->termType != EXPRS_TERM_INTEGER || params.bb->termType != EXPRS_TERM_INTEGER )
				return EXPR_TERM_BAD_SYNTAX;
			params.aa->term.s64 <<= params.bb->term.s64;
			continue;
		case EXPRS_TERM_SHR:    /* >> */
			if ( (err = prepBinaryTerms(&params, true)) )
				return err;
			if ( params.aa->termType != EXPRS_TERM_INTEGER || params.bb->termType != EXPRS_TERM_INTEGER )
				return EXPR_TERM_BAD_SYNTAX;
			params.aa->term.s64 >>= params.bb->term.s64;
			continue;
		case EXPRS_TERM_GT:     /* > */
			if ( (err = prepBinaryTerms(&params, false)) )
				return err;
			dst = params.aa;
			err = doSub(exprs, params.aa, params.aa, params.bb);
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
		case EXPRS_TERM_GE:     /* >= */
			if ( (err = prepBinaryTerms(&params, false)) )
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
		case EXPRS_TERM_LT:     /* < */
			if ( (err = prepBinaryTerms(&params, false)) )
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
		case EXPRS_TERM_LE:     /* <= */
			if ( (err = prepBinaryTerms(&params, false)) )
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
		case EXPRS_TERM_EQ:     /* == */
			if ( (err = prepBinaryTerms(&params, false)) )
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
		case EXPRS_TERM_NE:     /* != */
			if ( (err = prepBinaryTerms(&params, false)) )
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
		case EXPRS_TERM_AND:    /* & */
			if ( (err = prepBinaryTerms(&params, true)) )
				return err;
			if ( params.aa->termType != EXPRS_TERM_INTEGER || params.bb->termType != EXPRS_TERM_INTEGER )
				return EXPR_TERM_BAD_SYNTAX;
			params.aa->term.s64 &= params.bb->term.s64;
			continue;
		case EXPRS_TERM_XOR:    /* ^ */
			if ( (err = prepBinaryTerms(&params, true)) )
				return err;
			if ( params.aa->termType != EXPRS_TERM_INTEGER || params.bb->termType != EXPRS_TERM_INTEGER )
				return EXPR_TERM_BAD_SYNTAX;
			params.aa->term.s64 ^= params.bb->term.s64;
			continue;
		case EXPRS_TERM_OR:     /* | */
			if ( (err = prepBinaryTerms(&params, true)) )
				return err;
			if ( params.aa->termType != EXPRS_TERM_INTEGER || params.bb->termType != EXPRS_TERM_INTEGER )
				return EXPR_TERM_BAD_SYNTAX;
			params.aa->term.s64 |= params.bb->term.s64;
			continue;
		case EXPRS_TERM_LAND:   /* && */
			if ( (err = prepBinaryTerms(&params, true)) )
				return err;
			if ( params.aa->termType != EXPRS_TERM_INTEGER || params.bb->termType != EXPRS_TERM_INTEGER )
				return EXPR_TERM_BAD_SYNTAX;
			params.aa->term.s64 = (params.aa->term.s64 && params.bb->term.s64) ? 1 : 0;
			continue;
		case EXPRS_TERM_LOR:    /* || */
			if ( (err = prepBinaryTerms(&params, true)) )
				return err;
			if ( params.aa->termType != EXPRS_TERM_INTEGER || params.bb->termType != EXPRS_TERM_INTEGER )
				return EXPR_TERM_BAD_SYNTAX;
			params.aa->term.s64 = (params.aa->term.s64 || params.bb->term.s64) ? 1 : 0;
			continue;
		case EXPRS_TERM_ASSIGN: /* = */
			if ( (err = prepBinaryTerms(&params, false)) )
				return err;
			/* bb is the value to assign to it */
			/* aa is the symbol to assign to */
			if ( !exprs->mCallbacks.symGet )
			{
				snprintf(eBuf, sizeof(eBuf), "computeViaRPN(): No symbol table. Assignment not possible: at or near %s\n", params.aa->chrPtr);
				showMsg(exprs, EXPRS_SEVERITY_ERROR, eBuf);
				return EXPR_TERM_BAD_NO_SYMBOLS;
			}
			if ( params.aa->termType != EXPRS_TERM_SYMBOL )
			{
				snprintf(eBuf, sizeof(eBuf), "computeViaRPN(): Assignment to non-symbol (%d) not possible: at or near %s\n", params.aa->termType, params.aa->chrPtr);
				showMsg(exprs, EXPRS_SEVERITY_ERROR, eBuf);
				return EXPR_TERM_BAD_LVALUE;
			}
			if ( params.bb->termType == EXPRS_TERM_SYMBOL )
			{
				err = lookupSymbol(exprs, params.bb, params.bb);
				if ( err != EXPR_TERM_GOOD )
					return err;
			}
			if ( params.bb->termType != EXPRS_TERM_STRING && params.bb->termType != EXPRS_TERM_FLOAT && params.bb->termType != EXPRS_TERM_INTEGER )
			{
				snprintf(eBuf, sizeof(eBuf), "computeViaRPN(): Assignment can only be type Integer, float or string (is %d) at or near %s\n",
						 params.bb->termType, params.aa->chrPtr);
				showMsg(exprs, EXPRS_SEVERITY_ERROR, eBuf);
				return EXPR_TERM_BAD_LVALUE;
			}
			ans.termType = (ExprsSymTermTypes_t)params.bb->termType;
			ans.value.f64 = params.bb->term.f64;
			err = exprs->mCallbacks.symSet(exprs->mCallbacks.symArg, libExprsStringPoolTop(exprs) + params.aa->term.string, &ans);
			if ( err )
			{
				snprintf(eBuf, sizeof(eBuf), "computeViaRPN(): Failed ('%s') to assign symbol '%s' at or near %s\n",
						 libExprsGetErrorStr(err),
						 libExprsStringPoolTop(exprs) + params.aa->term.string,
						 params.aa->chrPtr);
				showMsg(exprs, EXPRS_SEVERITY_ERROR, eBuf);
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
		snprintf(eBuf, sizeof(eBuf), "computeViaRPN(): expression did not resolve to a single term. Found rTop=%d\n", params.rTop);
		showMsg(exprs, EXPRS_SEVERITY_ERROR, eBuf);
		if ( params.rTop > 1 )
			return EXPR_TERM_BAD_TOO_MANY_TERMS;
		else
			return EXPR_TERM_BAD_TOO_FEW_TERMS;
	}
	*returnResult = params.results[0];
	if ( exprs->mVerbose )
	{
		snprintf(eBuf, sizeof(eBuf), "computeViaRPN(): Finish. nest=%d. Terms=%d. rTop=%d, %s\n",
				 nest, sPtr->mTermsPool.mNumUsed, params.rTop,
				 showTermType(exprs, sPtr, returnResult, params.tmpBuf0, sizeof(params.tmpBuf0) - 1));
		showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
	}
	return EXPR_TERM_GOOD;
}

#if 0
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
#endif

const char* libExprsGetErrorStr(ExprsErrs_t errCode)
{
	if ( errCode < n_elts(ErrorDescriptions) )
		return ErrorDescriptions[errCode];
	return "Undefined errCode";
}

static void setup(ExprsDef_t *exprs)
{
	exprs->precedencePtr = (exprs->mFlags & EXPRS_FLG_NO_PRECEDENCE) ? PrecedenceNone : PrecedenceNormal;
	if ( (exprs->mFlags & EXPRS_FLG_DOT_DECIMAL) || ((exprs->mFlags & EXPRS_FLG_USE_RADIX) && (exprs->mRadix != 0 && exprs->mRadix != 10)) )
		exprs->mFlags |= EXPRS_FLG_NO_FLOAT;
	if ( (exprs->mFlags & EXPRS_FLG_SPECIAL_UNARY) )
	{
		exprs->mOpenDelimiter = '<';
		exprs->mCloseDelimiter = '>';
	}
	if ( exprs->mFlags && exprs->mVerbose )
	{
		char eBuf[128];
		snprintf(eBuf, sizeof(eBuf), "libExprsEval(): flags=0x%lX, radix=%d\n", exprs->mFlags, exprs->mRadix);
		showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
	}
#if NO_FLOATING_POINT
	exprs->mFlags |= EXPRS_FLG_NO_FLOAT;
#endif
}

static void reset(ExprsDef_t *exprs, int stringsToo)
{
	ExprsStack_t *stack = &exprs->mStack;
	if ( stack->mTermsPool.mNumUsed > stack->mTermsPool.mMaxUsed )
		stack->mTermsPool.mMaxUsed = stack->mTermsPool.mNumUsed;
	stack->mTermsPool.mNumUsed = 0;
	if ( exprs->mStringPool.mNumUsed > exprs->mStringPool.mMaxUsed )
		exprs->mStringPool.mMaxUsed = exprs->mStringPool.mNumUsed;
	if ( stringsToo )
		exprs->mStringPool.mNumUsed = 0;
}

static void textToPrint(char *eBuf, size_t eBufSize, const char *header, const char *trailer, const char *txt)
{
	char cc, *dst;
	size_t len;
	len = snprintf(eBuf, eBufSize, "%s", header);
	dst = eBuf + len;
	while ( (cc = *txt++) && len < eBufSize - 5 )
	{
		if ( isprint(cc) )
		{
			*dst++ = cc;
			++len;
		}
		else
		{
			switch (cc)
			{
			case 012:
				*dst++ = '\\';
				*dst++ = 'n';
				len += 2;
				break;
			case 015:
				*dst++ = '\\';
				*dst++ = 'r';
				len += 2;
				break;
			case 033:
				*dst++ = '\\';
				*dst++ = 'e';
				len += 2;
				break;
			default:
				*dst++ = '\\';
				*dst++ = '0' + ((cc >> 6) & 3);
				*dst++ = '0' + ((cc >> 3) & 7);
				*dst++ = '0' + (cc & 7);
				len += 4;
			}
		}
	}
	snprintf(eBuf + len, eBufSize - len, "%s", trailer);
}

ExprsErrs_t libExprsEval(ExprsDef_t *exprs, const char *text, ExprsTerm_t *returnTerm, int alreadyLocked)
{
	ExprsErrs_t peErr, err = EXPR_TERM_BAD_SYNTAX, err2 = EXPR_TERM_GOOD;
	char eBuf[512], saveOpen, saveClose;
	int len;
	const char *ePtr;

	if ( !exprs || !returnTerm || !text )
		return EXPR_TERM_BAD_PARAMETER;
	if ( !alreadyLocked && (err2 = libExprsLock(exprs)) )
		return err2;
	saveOpen = exprs->mOpenDelimiter;
	saveClose = exprs->mCloseDelimiter;
	returnTerm->termType = EXPRS_TERM_NULL;
	returnTerm->term.s64 = 0;
	returnTerm->chrPtr = NULL;
	setup(exprs);
	len = strlen(text);
	ePtr = text + len;
	exprs->mLineHead = exprs->mCurrPtr = text;
	while ( exprs->mCurrPtr < ePtr && *exprs->mCurrPtr )
	{
		reset(exprs, false);                /* Clear any existing stacks (keep string pool) */
		peErr = parseExpression(exprs, 0, true);
		err = peErr;
		if ( err <= EXPR_TERM_END )
		{
			if ( exprs->mVerbose )
			{
				showMsg(exprs, EXPRS_SEVERITY_INFO, "Stacks before computeViaRPN");
				dumpStack(exprs, NULL, 0);
			}
			err = computeViaRPN(exprs, 0, returnTerm);
			if ( err <= EXPR_TERM_END )
			{
				if ( returnTerm->termType == EXPRS_TERM_SYMBOL )
				{
					ExprsTerm_t sym;
					err = lookupSymbol(exprs, returnTerm, &sym);
					if ( err )
					{
						len = snprintf(eBuf, sizeof(eBuf), "libExprsEval(): Undefined symbol: %s\n",
									   libExprsStringPoolTop(exprs) + returnTerm->term.string);
						if ( returnTerm->chrPtr )
							snprintf(eBuf + len, sizeof(eBuf) - len, "At or near: %s\n", returnTerm->chrPtr);
						showMsg(exprs, EXPRS_SEVERITY_ERROR, eBuf);
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
					showMsg(exprs, EXPRS_SEVERITY_INFO, "Stacks after computeViaRPN");
					dumpStack(exprs, NULL, 0);
					snprintf(eBuf, sizeof(eBuf), "The resulting term after computing RPN expression:\n");
					showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
					switch (returnTerm->termType)
					{
					case EXPRS_TERM_NULL:
					case EXPRS_TERM_SYMBOL: /* Symbol */
					case EXPRS_TERM_FUNCTION: /* Function call */
					case EXPRS_TERM_STRING: /* Text string */
						snprintf(eBuf, sizeof(eBuf), "Type %d: flags: 0x%X, value: '%s'\n",
								 returnTerm->termType,
								 returnTerm->flags,
								 libExprsStringPoolTop(exprs) + returnTerm->term.string);
						break;
					case EXPRS_TERM_FLOAT:  /* 64 bit floating point number */
						snprintf(eBuf, sizeof(eBuf), "Type %d: flags: 0x%X, value: '%g'\n",
								 returnTerm->termType,
								 returnTerm->flags,
								 returnTerm->term.f64);
						break;
					case EXPRS_TERM_INTEGER:    /* 64 bit integer number */
						snprintf(eBuf, sizeof(eBuf), "Type %d: flags: 0x%X, value: '%ld'\n",
								 returnTerm->termType,
								 returnTerm->flags,
								 returnTerm->term.s64);
						break;
					default:
						snprintf(eBuf, sizeof(eBuf), "Type %d: flags: 0x%X, value: 0x%lX (undefined)\n",
								 returnTerm->termType,
								 returnTerm->flags,
								 returnTerm->term.s64);
						break;
					}
					showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
				}
			}
			else
			{
				len = snprintf(eBuf, sizeof(eBuf), "computeViaRPN() returned %d: %s\n", err, libExprsGetErrorStr(err));
				if ( returnTerm->chrPtr )
					snprintf(eBuf + len, sizeof(eBuf) - len, "At or near: %s\n", returnTerm->chrPtr);
				showMsg(exprs, EXPRS_SEVERITY_ERROR, eBuf);
				dumpStack(exprs, NULL, 0);
			}
			if ( (exprs->mFlags & EXPRS_FLG_WS_DELIMIT) && peErr == EXPR_TERM_END )
			{
				if ( exprs->mVerbose )
				{
					textToPrint(eBuf, sizeof(eBuf), "Ending text: '", "'\n", exprs->mCurrPtr);
					showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
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
				snprintf(eBuf, sizeof(eBuf), "parseExpression() returned %d: %s\n", err, libExprsGetErrorStr(err));
				showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
				dumpStack(exprs, NULL, 0);
			}
			break;
		}
		if ( err > EXPR_TERM_END )
			break;
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

ExprsErrs_t libExprsParseToRPN(ExprsDef_t *exprs, const char *text, int alreadyLocked)
{
	ExprsErrs_t peErr, err, err2 = EXPR_TERM_GOOD;
	char eBuf[512], saveOpen, saveClose;

	if ( !exprs || !text )
		return EXPR_TERM_BAD_PARAMETER;
	if ( !alreadyLocked && (err2 = libExprsLock(exprs)) )
		return err2;
	saveOpen = exprs->mOpenDelimiter;
	saveClose = exprs->mCloseDelimiter;
	setup(exprs);
	exprs->mLineHead = exprs->mCurrPtr = text;
	reset(exprs, true);                 /* Clear any existing stacks and string pool */
	peErr = parseExpression(exprs, 0, TermIsUnary);
	err = peErr;
	if ( err <= EXPR_TERM_END )
	{
		if ( exprs->mVerbose )
		{
			showMsg(exprs, EXPRS_SEVERITY_INFO, "Stacks after libExprsParseToRPN()");
			dumpStack(exprs, NULL, 0);
		}
		if ( exprs->mVerbose )
		{
			char noLf[128 + 64];
			removeCrLf(noLf, sizeof(noLf), exprs->mCurrPtr);
			textToPrint(eBuf, sizeof(eBuf), "Ending text: '", "'\n", noLf);
			showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
		}
	}
	else
	{
		if ( exprs->mVerbose )
		{
			snprintf(eBuf, sizeof(eBuf), "parseExpression() returned %d: %s\n", err, libExprsGetErrorStr(err));
			showMsg(exprs, EXPRS_SEVERITY_INFO, eBuf);
			dumpStack(exprs, NULL, 0);
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

static ExprsErrs_t walkStack(ExprsDef_t *exprs, ExprsErrs_t(*walkCallback)(ExprsDef_t *exprs, const ExprsTerm_t *term))
{
	ExprsTerm_t *term;
	ExprsErrs_t err;
	ExprsStack_t *stack = &exprs->mStack;
	int ii;

	term = libExprsTermPoolTop(exprs, stack);
	for ( ii = 0; ii < stack->mTermsPool.mNumUsed; ++ii, ++term )
	{
		err = walkCallback(exprs, term);
		if ( err != EXPR_TERM_GOOD )
			return err;
	}
	return EXPR_TERM_GOOD;
}

ExprsErrs_t libExprsWalkParsedStack(ExprsDef_t *exprs, ExprsErrs_t(*walkCallback)(ExprsDef_t *exprs, const ExprsTerm_t *term), int alreadyLocked)
{
	if ( !exprs || !walkCallback )
		return EXPR_TERM_BAD_PARAMETER;
	return walkStack(exprs, walkCallback);
}

static void lclMsgOut(void *msgArg, ExprsMsgSeverity_t severity, const char *msg)
{
	static const char *Severities[] = { "INFO", "WARN", "ERROR", "FATAL" };
	fprintf(severity > EXPRS_SEVERITY_INFO ? stderr : stdout, "%s-libExprs(): %s", Severities[severity], msg);
}

static void* lclMalloc(void *memArg, size_t size)
{
	if ( !size )
	{
		fprintf(stderr, "libExprs(): FATAL error trying to malloc 0 bytes \n");
		exit(1);
	}
	return malloc(size);
}

static void lclFree(void *memArg, void *ptr)
{
#if 0
	printf("lclFree(%p)\n", ptr);
	fflush(stdout);
#endif
	free(ptr);
}

static ExprsCallbacks_t* checkCallbacks(ExprsCallbacks_t *tCallbacks, const ExprsCallbacks_t *callbacks, ExprsMsgSeverity_t severity)
{
	memset(tCallbacks, 0, sizeof(ExprsCallbacks_t));
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
				tCallbacks->msgOut(tCallbacks->msgArg, severity, "callbacks->memAlloc and callbacks->memFree must both be set\n");
				return NULL;
			}
			tCallbacks->memAlloc = callbacks->memAlloc;
			tCallbacks->memFree = callbacks->memFree;
			tCallbacks->memArg = callbacks->memArg;
		}
		if ( callbacks->symGet )
			tCallbacks->symGet = callbacks->symGet;
		if ( callbacks->symSet )
			tCallbacks->symSet = callbacks->symSet;
		if ( callbacks->symGet || callbacks->symSet )
			tCallbacks->symArg = callbacks->symArg;
	}
	return tCallbacks;
}

ExprsErrs_t libExprsSetCallbacks(ExprsDef_t *exprs, const ExprsCallbacks_t *newPtr, ExprsCallbacks_t *oldPtr)
{
	ExprsCallbacks_t tCallbacks;

	if ( !checkCallbacks(&tCallbacks, newPtr, EXPRS_SEVERITY_ERROR) )
		return EXPR_TERM_BAD_PARAMETER;
	if ( oldPtr )
		*oldPtr = exprs->mCallbacks;
	exprs->mCallbacks = tCallbacks;
	return EXPR_TERM_GOOD;
}

ExprsDef_t* libExprsInit(const ExprsCallbacks_t *callbacks, int termIncs, int stringIncs)
{
	ExprsDef_t *exprs;
	ExprsCallbacks_t tCallbacks;
	char tBuf[512];

	if ( !checkCallbacks(&tCallbacks, callbacks, EXPRS_SEVERITY_FATAL) )
		return NULL;
	if ( !termIncs )
		termIncs = 32;
	if ( !stringIncs )
		stringIncs = 2048;
	if (    n_elts(PrecedenceNormal) != EXPRS_TERM_ASSIGN + 1
		 || n_elts(PrecedenceNone) != EXPRS_TERM_ASSIGN + 1 )
	{
		/* Sanity check */
		snprintf(tBuf, sizeof(tBuf),
				 "libExprsInit(): n_elts(PrecedenceNormal) s/b %d is %d\n"
				 "libExprsInit(): n_elts(PrecedenceNone)   s/b %d is %d\n",
				 n_elts(PrecedenceNormal), EXPRS_TERM_ASSIGN + 1,
				 n_elts(PrecedenceNone), EXPRS_TERM_ASSIGN + 1);
		tCallbacks.msgOut(tCallbacks.msgArg, EXPRS_SEVERITY_FATAL, tBuf);
		return NULL;
	}
	exprs = (ExprsDef_t *)tCallbacks.memAlloc(tCallbacks.memArg, sizeof(ExprsDef_t));
	if ( !exprs )
	{
		snprintf(tBuf, sizeof(tBuf), "libExprsInit(): Failed to allocate " FMT_SZ " bytes for ExprsDef_t: %s\n",
				 sizeof(ExprsDef_t), strerror(errno));
		tCallbacks.msgOut(tCallbacks.msgArg, EXPRS_SEVERITY_FATAL, tBuf);
		return NULL;
	}
	memset(exprs, 0, sizeof(ExprsDef_t));
	exprs->mOpenDelimiter = '(';
	exprs->mCloseDelimiter = ')';
	exprs->mCallbacks = tCallbacks;
	exprs->mVerbose = 0;
	exprs->mStringPoolInc = stringIncs;
	exprs->mTermsPoolInc = termIncs;
	exprs->mStack.mTermsPool.mPoolID = ExprsPoolTerms;
	exprs->mStack.mTermsPool.mEntrySize = sizeof(ExprsTerm_t);
	exprs->mStringPool.mPoolID = ExprsPoolString;
	exprs->mStringPool.mEntrySize = sizeof(char);
	pthread_mutex_init(&exprs->mMutex, NULL);
	return exprs;
}

ExprsErrs_t libExprsDestroy(ExprsDef_t *exprs)
{
	ExprsErrs_t err;
	void (*memFree)(void *memArg, void *memPtr) = exprs->mCallbacks.memFree;
	void *pArg = exprs->mCallbacks.memArg;
	ExprsStack_t *stack;

	err = libExprsLock(exprs);
	stack = &exprs->mStack;
	if ( stack->mTermsPool.mPoolTop )
		memFree(pArg, stack->mTermsPool.mPoolTop);
	if ( exprs->mStringPool.mPoolTop )
		memFree(pArg, exprs->mStringPool.mPoolTop);
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
			snprintf(emsg, sizeof(emsg), "Failed to lock: %s\n", strerror(errno));
			exprs->mCallbacks.msgOut(exprs->mCallbacks.msgArg, EXPRS_SEVERITY_ERROR, emsg);
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
			snprintf(emsg, sizeof(emsg), "Failed to unlock: %s\n", strerror(errno));
			exprs->mCallbacks.msgOut(exprs->mCallbacks.msgArg, EXPRS_SEVERITY_ERROR, emsg);
		}
		return EXPR_TERM_BAD_NOUNLOCK;
	}
	return EXPR_TERM_GOOD;
}

