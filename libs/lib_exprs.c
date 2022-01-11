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

static unsigned short cttbl[] =
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

typedef unsigned char bool;
enum
{
	false,
	true
};
typedef unsigned char Precedence_t;
/* This establishes the precedence of the various operators. */
static const Precedence_t Precedence[] =
{
	9,	/* EXPRS_TERM_NULL, */
	9,	/* EXPRS_TERM_LINK,  *//* link to another stack */
	9,	/* EXPRS_TERM_SYMBOL, *//* Symbol */
	9,	/* EXPRS_TERM_FUNCTION, *//* Function call */
	9,	/* EXPRS_TERM_STRING, *//* Text string */
	9,	/* EXPRS_TERM_FLOAT, *//* 64 bit floating point number */
	9,	/* EXPRS_TERM_INTEGER, *//* 64 bit integer number */
	8,	/* EXPRS_TERM_PLUS,  *//* + (unary term) */
	8,	/* EXPRS_TERM_MINUS, *//* - (unary term) */
	8,	/* EXPRS_TERM_COM,	 *//* ~ */
	8,	/* EXPRS_TERM_NOT,	 *//* ! */
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
			snprintf(dst, dstLen, "Operator: %s (precedence %d)", term->term.oper, Precedence[term->termType]);
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

int libExprsSetVerbose(ExprsDef_t *exprs, int newVal)
{
	int oldVal = exprs->mVerbose;
	exprs->mVerbose = newVal;
	return oldVal;
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

static ExprsErrs_t parseExpression(ExprsDef_t *exprs, int nest, bool lastTermWasOperator, ExprsStack_t *sPtr)
{
	unsigned short chMask=CT_EOL;
	char cc, *operPtr, *sEndp;
	const char *startP, *endP;
	ExprsTerm_t *term;
	ExprsErrs_t err;
	int topOper = -1; /* top of operator stack */
	bool exitOut=false;
	char eBuf[512];
	
	if ( exprs->mVerbose )
	{
		snprintf(eBuf,sizeof(eBuf),"parseExpression(): Entry. nest=%d, stackIdx=%ld, numTerms=%d, expr='%s'\n", nest, sPtr - exprs->mStacks, sPtr->mNumTerms, exprs->mCurrPtr);
		showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
	}
	if ( sPtr->mNumTerms >= exprs->mNumAvailTerms )
		return EXPR_TERM_BAD_TOO_MANY_TERMS;
	while ( (cc = *exprs->mCurrPtr) )
	{
		chMask = cttbl[(int)cc];
		if ( (chMask&(CT_EOL|CT_COM|CT_SMC)) )
		{
			if ( topOper >= 0 )
				break;
			return EXPR_TERM_END;
		}
		if ( (chMask&CT_WS) )
		{
			/* Eat whitespace */
			++exprs->mCurrPtr;
			continue;
		}
		if ( !(chMask&(CT_EALP|CT_NUM|CT_OPER|CT_QUO)) )
		{
			snprintf(eBuf,sizeof(eBuf),"parseExpression(): Syntax error. cc='%c', chMask=0x%04X\n", isprint(cc)?cc:'.',chMask);
			showMsg(exprs,EXPRS_SEVERITY_ERROR,eBuf);
			/* Not something we know how to handle */
			return EXPR_TERM_BAD_SYNTAX;
		}
		if ( exprs->mVerbose )
		{
			snprintf(eBuf,sizeof(eBuf),"parseExpression(): Processing terms[%ld][%d], cc=%c, chMask=0x%04X:  %s\n", sPtr - exprs->mStacks, sPtr->mNumTerms, isprint(cc) ? cc : '.', chMask, exprs->mCurrPtr);
			showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
		}
		term = sPtr->mTerms + sPtr->mNumTerms;
		term->term.s64 = 0;
		term->termType = EXPRS_TERM_NULL;
		term->chrPtr = exprs->mCurrPtr;
		if ( (chMask & CT_QUO) )
		{
			/* quoted string */
			int strLen;
			char quoteChar = cc, *dst;
			
			endP = exprs->mCurrPtr+1;		/* Skip starting quote char */
			while ( (cc = *endP) )
			{
				chMask = cttbl[(int)cc];
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
			if ( !(term->term.string = getStringMem(exprs,strLen+1)) )
				return EXPR_TERM_BAD_OUT_OF_MEMORY;
			term->term.string[strLen] = 0;
			endP = exprs->mCurrPtr+1;		/* Skip starting quote char */
			dst = term->term.string;
			while ( (cc = *endP) )
			{
				chMask = cttbl[(int)cc];
				if ( (chMask&CT_EOL) )
					return EXPR_TERM_BAD_NO_STRING_TERM;
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
						sEndp=NULL;
						cvt = strtol(endP+1,&sEndp,16);
						if ( sEndp )
						{
							*dst++ = cvt;
							endP = sEndp;
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
				*dst++ = *endP++;
			}
			*dst = 0;
			if ( exprs->mVerbose )
			{
				snprintf(eBuf,sizeof(eBuf),"parseExpression(): Pushed to terms[%ld][%d] a string='%s'\n", sPtr-exprs->mStacks,sPtr->mNumTerms,term->term.string);
				showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
			}
/*			memcpy(term->term.string,mCurrPtr,strLen); */
			term->termType = EXPRS_TERM_STRING;
			++sPtr->mNumTerms;
			exprs->mCurrPtr = endP;
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
				chMask = cttbl[(int)cc];
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
					char *sEndp;
					sEndp = NULL;
					term->term.s64 = strtoll(startP,&sEndp,16);
					if ( !sEndp )
						return EXPR_TERM_BAD_NUMBER;
					if ( exprs->mVerbose )
					{
						snprintf(eBuf,sizeof(eBuf),"parseExpression(): Pushed to terms[%ld][%d] a HEX Integer %ld. topOper=%d.\n", sPtr-exprs->mStacks, sPtr->mNumTerms, term->term.s64, topOper);
						showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
					}
					endP = sEndp;
					exprs->mCurrPtr = endP;
					term->termType = EXPRS_TERM_INTEGER;
					lastTermWasOperator = false;
					++sPtr->mNumTerms;
					continue;
				}
				if ( exprs->mCurrPtr[1] == 'd' || exprs->mCurrPtr[1] == 'D' )
				{
					startP += 2;
					sEndp = NULL;
					term->term.s64 = strtoll(startP,&sEndp,10);
					if ( !sEndp )
						return EXPR_TERM_BAD_NUMBER;
					endP = sEndp;
					if ( exprs->mVerbose )
					{
						snprintf(eBuf,sizeof(eBuf),"parseExpression(): Pushed to terms[%ld][%d] a DECIMAL Integer %ld. topOper=%d.\n", sPtr-exprs->mStacks, sPtr->mNumTerms, term->term.s64, topOper);
						showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
					}
					exprs->mCurrPtr = endP;
					term->termType = EXPRS_TERM_INTEGER;
					lastTermWasOperator = false;
					++sPtr->mNumTerms;
					continue;
				}
			}
			sEndp = NULL;
			term->term.s64 = strtoll(startP,&sEndp,0);
			if ( !sEndp )
				return EXPR_TERM_BAD_NUMBER;
			endP = sEndp;
			if ( *endP == '.' || (*endP == 'e' || *endP == 'E'))
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
				snprintf(eBuf,sizeof(eBuf),"parseExpression(): Pushed to terms[%ld][%d] a plain Integer %ld. topOper=%d.\n", sPtr-exprs->mStacks, sPtr->mNumTerms, term->term.s64, topOper);
				showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
			}
			exprs->mCurrPtr = endP;
			term->termType = EXPRS_TERM_INTEGER;
			++sPtr->mNumTerms;
			lastTermWasOperator = false;
			continue;
		}
		operPtr = term->term.oper;
		switch (cc)
		{
		case '(':
			{
				ExprsStack_t *nPtr = getNextStack(exprs);
				if ( !nPtr )
					return EXPR_TERM_BAD_TOO_MANY_TERMS;
				term->termType = EXPRS_TERM_LINK;
				term->term.link = nPtr;
				++sPtr->mNumTerms;
				++exprs->mCurrPtr;
				err = parseExpression(exprs,nest+1,lastTermWasOperator,nPtr); 
				if ( err )
					return err;
				lastTermWasOperator = false;
			}
			continue;
		case ')':
			if ( nest < 1 )
			{
				snprintf(eBuf,sizeof(eBuf),"parseExpression(): Syntax error. cc='%c', chMask=0x%04X, nest=%d\n", isprint(cc)?cc:'.',chMask,nest);
				showMsg(exprs,EXPRS_SEVERITY_ERROR,eBuf);
				return EXPR_TERM_BAD_SYNTAX;
			}
			--nest;
			++exprs->mCurrPtr;
			exitOut = true;
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
			term->termType = EXPRS_TERM_XOR;
			*operPtr++ = cc;
			break;
		case '~':
			term->termType = EXPRS_TERM_COM;
			*operPtr++ = cc;
			break;
		case '!':
			*operPtr++ = cc;
			if ( exprs->mCurrPtr[1] == '=' )
			{
				term->termType = EXPRS_TERM_NE;
				*operPtr++ = '=';
				++exprs->mCurrPtr;
			}
			else
				term->termType = EXPRS_TERM_NOT;
			break;
		case '=':
			*operPtr++ = cc;
			if (exprs->mCurrPtr[1] == '=')
			{
				term->termType = EXPRS_TERM_EQ;
				*operPtr++ = cc;
				++exprs->mCurrPtr;
			}
			else
				term->termType = EXPRS_TERM_ASSIGN;
			break;
		case '<':
			*operPtr++ = cc;
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
		case '>':
			*operPtr++ = cc;
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
			snprintf(eBuf,sizeof(eBuf),"parseExpression(): Syntax error. cc='%c', chMask=0x%04X\n",isprint(cc)?cc:'.',chMask);
			showMsg(exprs,EXPRS_SEVERITY_ERROR,eBuf);
			return EXPR_TERM_BAD_SYNTAX;
		}
		if ( exitOut )
			break;
		if ( operPtr != term->term.oper )
		{
			ExprsTerm_t savTerm = *term;
			Precedence_t oldPrecedence,currPrecedence = Precedence[savTerm.termType];
			*operPtr = 0;
			/* check for the operators of higher precedence and then add them to stack */
			if ( exprs->mVerbose )
			{
				snprintf(eBuf,sizeof(eBuf),"parseExpression(): Before precedence. Checking type %d ('%s') precedence %d ...\n", term->termType, term->term.oper, currPrecedence);
				showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
				dumpStacks(exprs);
			}
			while ( topOper >= 0 && (oldPrecedence=Precedence[sPtr->mOpers[topOper].mOperType]) >= currPrecedence )
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
			if ( exprs->mVerbose )
			{
				snprintf(eBuf,sizeof(eBuf),"parseExpression(): Pushed operator '%s'(%d) to operators[%ld][%d]\n",term->term.oper, term->termType, sPtr-exprs->mStacks, topOper);
				showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
			}
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
	term = sPtr->mTerms + sPtr->mNumTerms;
	while ( topOper >= 0 )
	{
		/* add remaining operators to stack */
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
	return EXPR_TERM_GOOD;
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
			dst->term.s64 = fmod(aa->term.s64, bb->term.f64);
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
			if ( exprs->mVerbose )
			{
				snprintf(eBuf,sizeof(eBuf),"computeViaRPN(): Item %d: %s, aa=%s\n",
					   ii,
					   showTermType(exprs,sPtr,term,params.tmpBuf0,sizeof(params.tmpBuf0)-1),
					   showTermType(exprs,sPtr,params.aa,params.tmpBuf1,sizeof(params.tmpBuf1)-1));
				showMsg(exprs,EXPRS_SEVERITY_INFO,eBuf);
			}
			continue;			/* Just eat a extraneous plus sign */
		case EXPRS_TERM_MINUS:	/* - */
			if ( (err=prepUnaryTerm(&params,false)) )
				return err;
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

ExprsErrs_t libExprsEval(ExprsDef_t *exprs, const char *text, ExprsTerm_t *returnTerm)
{
	ExprsErrs_t err=EXPR_TERM_BAD_SYNTAX,err2;
	char eBuf[512];
	int len;
	
	if ( !exprs || !returnTerm )
		return EXPR_TERM_BAD_PARAMETER;
	if ( (err2=libExprsLock(exprs)) )
		return err2;
	returnTerm->termType = EXPRS_TERM_NULL;
	returnTerm->term.s64 = 0;
	returnTerm->chrPtr = NULL;
	if ( text )
	{
		const char *ePtr;

		len = strlen(text);
		ePtr = text+len;
		exprs->mLineHead = exprs->mCurrPtr = text;
		while ( exprs->mCurrPtr < ePtr && *exprs->mCurrPtr )
		{
			reset(exprs); 				/* Clear any existing cruft */
			err = parseExpression(exprs, 0, false, getNextStack(exprs));
			if ( err <= EXPR_TERM_END )
			{
/*				dumpStacks(); */
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

