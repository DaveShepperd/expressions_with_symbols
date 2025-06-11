#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>
#include <getopt.h>

#include "lib_hashtbl.h"
#include "lib_btree.h"
#include "lib_exprs.h"
#include "exprs_test.h"
#include "exprs_test_bt.h"
#include "exprs_test_ht.h"
#include "exprs_test_nos.h"
#include "exprs_test_walk.h"

enum
{
	OPT_BTREE=1,
	OPT_HASH_SIZE,
	OPT_INCS,
	OPT_TEST,
	OPT_VERBOSE,
	OPT_FLAGS,
	OPT_RADIX,
	OPT_WALK,
	OPT_MAX
};

static struct option long_options[] =
{
                   {"btree",      required_argument, 0, OPT_BTREE },
				   {"incs",       required_argument, 0, OPT_INCS },
				   {"flags",      required_argument, 0, OPT_FLAGS },
				   {"radix",      required_argument, 0, OPT_RADIX },
                   {"hash_size",  required_argument, 0, OPT_HASH_SIZE },
				   {"test",		  no_argument,		0, OPT_TEST },
				   {"verbose",    no_argument,       0, OPT_VERBOSE },
				   {"walk",       no_argument,       0, OPT_WALK },
				   {0,         0,                 0,  0 }
               };

static const char FlagsDescription[] =
"The flags option is a bit mask to control parser:\n" 
"0x00000001	= Use radix to figure out numbers\n"
"0x00000002	= No floating point allowed\n"
"0x00000004	= No quoted strings allowed\n"
"0x00000008	= No operator precedence\n"
"0x00000010	= Hex can be expressed with trailing 'h' or 'H'\n"
"0x00000020	= Hex can be expressed with leading '$'\n"
"0x00000040	= Hex can be expressed with trailing '$'\n"
"0x00000080	= Octal can be expressed with trailing 'o' or 'O'\n"
"0x00000100	= Octal can be expressed with trailing 'q' or 'Q'\n"
"0x00000200	= Decimal can be expressed with trailing '.' (forces flag 0x2 = NO_FLOAT)\n"
"0x00000400	= No exponent allowed ('**' construct)\n"
"0x00000800	= Allow single quoted chars (i.e. 'a vs. 'a'; forces flag 0x4 = NO_STRINGS)\n"
"0x00001000	= No logical operators allowed (i.e. those not found in mac6x, mac11, etc.)\n"
"0x00002000	= Enable special unary operators (i.e. those found in mac6x, mac11, etc.)\n"
"0x00004000	= No symbol assignments\n"
"0x00008000	= White space delimits all terms\n"
"0x00010000	= Don't allow more than one bump in pool increments\n"
"0x00020000	= Local symbols are expressed via decimalNumber$ (cannot be combined with POST_DOLLAR_HEX)\n"
"0x00040000	= Symbols can begin with leading period (.) (forces flag 0x2 = NO_FLOAT)\n"
"0x00080000	= Symbols can begin with leading dollar sign ($) (cannot be combined with flag 0x20)\n"
;

static int helpEm(const char *ourName)
{
	fprintf(stderr, "Usage: %s [-b num] [-s hashSize] [-tv] expression\n",
		   ourName);
	fprintf(stderr,"Where:\n"
			"-b num   [or --btree=num]    test using btree symbols. num=maxSize.\n"
			"-i incs  [or --incs=num]     set all the pool increments\n"
			"-f flags [or --flags=flgs]   set flag bits\n"
			"-r radix [or --radix=rad]    set the default radix (also sets 0x1 in flags)\n"
			"-s size  [or --hash=size]    set hash table size (default=0)\n"
			"-t       [or --test]         execute the full expressin parser tester\n"
			"-v       [or --verbose]      increment verbose mode\n"
			"-w       [or --walk]         use the walk feature\n"
			"\n"
			);
	fputs(FlagsDescription, stderr);
	return 1;
}

int main(int argc, char *argv[])
{
	int opt_index, opt;
	int tblSize=0;
	int verbose=0;
	int btree_size=0;
	int testOnly=0;
	int radix=0;
	int incs=0;
	int walk=0;
	unsigned long flags=0;
	char *endp;
	
	opt_index = 0;
	while ((opt = getopt_long_only(argc, argv, "b:i:f:r:s:tvw", long_options, &opt_index)) != -1)
	{
		switch (opt)
		{
		case OPT_BTREE:
		case 'b':
			btree_size = atoi(optarg);
			break;
		case OPT_INCS:
		case 'i':
			incs = atoi(optarg);
			break;
		case OPT_FLAGS:
		case 'f':
			endp = NULL;
			flags = strtoul(optarg,&endp,0);
			if ( !endp || *endp )
			{
				fprintf(stderr,"Invalid argument to --flags: '%s'\n", optarg);
				return 1;
			}
			break;
		case OPT_RADIX:
		case 'r':
			endp = NULL;
			radix = strtoul(optarg,&endp,0);
			if ( !endp || *endp || (radix != 2 && radix != 8 && radix != 10 && radix != 16))
			{
				fprintf(stderr,"Invalid argument to --radix: '%s' (can only be 2, 8, 10 or 16)\n", optarg);
				return 1;
			}
			flags |= EXPRS_FLG_USE_RADIX;
			break;
		case OPT_HASH_SIZE:
		case 's':
			tblSize = atoi(optarg);
			break;
		case 't':
		case OPT_TEST:
			testOnly = 1;
			break;
		case OPT_VERBOSE:
		case 'v':
			++verbose;
			break;
		case 'w':
		case OPT_WALK:
			walk = 1;
			break;
		default: /* '?' */
			return helpEm(argv[0]);
		}
	}
	if ( verbose )
	{
		printf("sizeof: char=%ld, short=%ld, int=%ld, long=%ld, double=%ld, pointer=%ld, ExprsTerm_t=%ld, ExprsDef_t=%ld\n",
			   sizeof(char),
			   sizeof(short),
			   sizeof(int),
			   sizeof(long),
			   sizeof(double),
			   sizeof(void *),
			   sizeof(ExprsTerm_t),
			   sizeof(ExprsDef_t));
			   
	}
	if ( testOnly )
	{
		return exprsTest(verbose);
	}
	if ( optind < argc )
	{
		char *exprs;
		exprs = argv[optind];
		if ( btree_size )
			return exprsTestBtree(incs, btree_size, exprs, flags, radix, verbose);
		if ( tblSize )
			return exprsTestHashTbl(incs, tblSize, exprs, flags, radix, verbose);
		if ( walk )
			return exprsTestWalk(incs,exprs,flags,radix,verbose);
		return exprsTestNoSym(incs, exprs, flags, radix, verbose);
	}
	return helpEm(argv[0]);
}

