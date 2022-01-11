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
#include "btree_test.h"
#include "hashtbl_test.h"
#include "exprs_test_bt.h"
#include "exprs_test_ht.h"
#include "exprs_test_nos.h"

enum
{
	OPT_BTREE=1,
	OPT_HASH_SIZE,
	OPT_HASH_ITEMS,
	OPT_EXPRS,
	OPT_VERBOSE,
	OPT_MAX
};

static struct option long_options[] =
{
                   {"btree",      required_argument, 0, OPT_BTREE },
				   {"exprs",	  required_argument, 0, OPT_EXPRS },
                   {"hash_size",  required_argument, 0, OPT_HASH_SIZE },
                   {"hash_items", required_argument, 0, OPT_HASH_ITEMS  },
				   {"verbose",    no_argument,       0, OPT_VERBOSE },
				   {0,         0,                 0,  0 }
               };

static int helpEm(const char *ourName)
{
	fprintf(stderr, "Usage: %s [-b num][--btree_size=num] [-e exp] [-n numItems] [-p poolSize] [-s tableSize] [-v][--verbose]\n",
		   ourName);
	fprintf(stderr,"Where:\n"
			"-b num  [or --btree=num]       test using btree symbols. num=maxSize.\n"
			"-e exp  [or --exprs=exp]       evaluate expression 'exp' (if also -b, use btree symbols; if also -n use hash symbols)\n"
			"-n num  [or --hash_items=num]  set number of hash items to install (default=0)\n"
			"-s size [or --hash_size=size]  set hash table size (default=0)\n"
			"-v      [or --verbose]         increment verbose mode\n"
			);
	return 1;
}

int main(int argc, char *argv[])
{
	int opt_index, opt;
	int tblSize=0, numItems=0;
	int verbose=0;
	int btree=0, btree_size=0;
	char *exprs=NULL;
	
	opt_index = 0;
	while ((opt = getopt_long_only(argc, argv, "b:e:n:s:v", long_options, &opt_index)) != -1)
	{
		switch (opt)
		{
		case OPT_BTREE:
		case 'b':
			++btree;
			if ( optarg )
				btree_size=atoi(optarg);
			break;
		case OPT_EXPRS:
		case 'e':
			exprs = optarg;
			break;
		case OPT_HASH_ITEMS:
		case 'n':
			numItems = atoi(optarg);
			break;
		case OPT_HASH_SIZE:
		case 's':
			tblSize = atoi(optarg);
			break;
		case OPT_VERBOSE:
		case 'v':
			++verbose;
			break;
		default: /* '?' */
			return helpEm(argv[0]);
		}
	}
	if ( exprs )
	{
		if ( btree_size )
			return exprsTestBtree(btree_size, exprs, verbose);
		if ( numItems )
			return exprsTestHashTbl(numItems, exprs, verbose);
		return exprsTestNoSym(exprs,verbose);
	}
	if ( btree )
	{
		return btreeTest(btree_size);
	}
	if ( tblSize )
	{
		return hashtblTest(tblSize, numItems);
	}
	return helpEm(argv[0]);
}

