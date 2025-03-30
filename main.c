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

enum
{
	OPT_BTREE=1,
	OPT_HASH_SIZE,
	OPT_TEST,
	OPT_VERBOSE,
	OPT_MAX
};

static struct option long_options[] =
{
                   {"btree",      required_argument, 0, OPT_BTREE },
                   {"hash_size",  required_argument, 0, OPT_HASH_SIZE },
				   {"test",		  no_argument,		0, OPT_TEST },
				   {"verbose",    no_argument,       0, OPT_VERBOSE },
				   {0,         0,                 0,  0 }
               };

static int helpEm(const char *ourName)
{
	fprintf(stderr, "Usage: %s [-b num] [-s hashSize] [-tv] expression\n",
		   ourName);
	fprintf(stderr,"Where:\n"
			"-b num  [or --btree=num]  test using btree symbols. num=maxSize.\n"
			"-s size [or --hash=size]  set hash table size (default=0)\n"
			"-t      [or --test]       execute the full expressin parser tester\n"
			"-v      [or --verbose]    increment verbose mode\n"
			);
	return 1;
}

int main(int argc, char *argv[])
{
	int opt_index, opt;
	int tblSize=0;
	int verbose=0;
	int btree_size=0;
	int testOnly=0;
	
	opt_index = 0;
	while ((opt = getopt_long_only(argc, argv, "b:s:tv", long_options, &opt_index)) != -1)
	{
		switch (opt)
		{
		case OPT_BTREE:
		case 'b':
			btree_size=atoi(optarg);
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
		default: /* '?' */
			return helpEm(argv[0]);
		}
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
			return exprsTestBtree(btree_size, exprs, verbose);
		else if ( tblSize )
			return exprsTestHashTbl(tblSize, exprs, verbose);
		return exprsTestNoSym(exprs,verbose);
	}
	return helpEm(argv[0]);
}

