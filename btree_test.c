#include "btree_test.h"

static int dataCmp(void *arg, BtreeEntry_t aa, BtreeEntry_t bb)
{
	int *paa = (int *)aa;
	int *pbb = (int *)bb;
	return *paa - *pbb;
}

static int showData(void *userArg, BtreeEntry_t data)
{
	int *pdata = (int *)data;
	printf("%d ", *pdata);
	return 0;
}

static int *memBasedData;
static int mbdAvailable,mbdUsed;

static int *getNextData(int init)
{
	int *ans;

	if ( mbdUsed > mbdAvailable )
	{
		fprintf(stderr,"Ran out of values. vUsed=%d\n", mbdUsed);
		exit(1);
	}
	ans = memBasedData + mbdUsed;
	++mbdUsed;
	*ans = init;
	return ans;
}

int btreeTest(int maxSize)
{
	BtreeControl_t *pTable;
	int ii;
	BtreeEntry_t oldData;
	BtreeErrors_t err;
	BtreeCallbacks_t callbacks;
	
	printf("Testing Btree with %d max items\n", maxSize);
	memBasedData = (int *)malloc(maxSize*sizeof(int));
	if ( !memBasedData )
	{
		fprintf(stderr,"Not enough memory to allocate %ld bytes for libBtree data\n", maxSize*sizeof(int));
		return -1;
	}
	mbdAvailable = maxSize;
	mbdUsed = 0;
	memset(&callbacks,0,sizeof(BtreeCallbacks_t));
	callbacks.symCmp = dataCmp;
#if 1
	pTable = libBtreeInit(&callbacks);
	if ( !pTable )
	{
		fprintf(stderr, "libBtreeInit(): Out of memory\n");
		return 1;
	}
	libBtreeInsert(pTable, getNextData(10));
	libBtreeInsert(pTable, getNextData(20));
	libBtreeInsert(pTable, getNextData(30));
	libBtreeInsert(pTable, getNextData(40));
	libBtreeInsert(pTable, getNextData(50));
	libBtreeInsert(pTable, getNextData(60));
	printf("height 10 through 60: %d\nInorder:\n", libBtreeHeight(pTable));
	libBtreeWalk(pTable, BtreeInorder, showData, NULL);
	printf("\npostorder:\n");
	libBtreeWalk(pTable, BtreePostorder, showData, NULL);
	printf("\npreorder:\n");
	libBtreeWalk(pTable, BtreePreorder, showData, NULL);
	printf("\nendorder:\n");
	libBtreeWalk(pTable, BtreeEndorder, showData, NULL);
	printf("\n");

	libBtreeInsert(pTable, getNextData(15));
	libBtreeInsert(pTable, getNextData(25));
	libBtreeInsert(pTable, getNextData(35));
	libBtreeInsert(pTable, getNextData(45));
	libBtreeInsert(pTable, getNextData(55));
	libBtreeInsert(pTable, getNextData(65));
	printf("height 15 through 65: %d\nInorder:\n", libBtreeHeight(pTable));
	libBtreeWalk(pTable, BtreeInorder, showData, NULL);
	printf("\npostorder:\n");
	libBtreeWalk(pTable, BtreePostorder, showData, NULL);
	printf("\npreorder:\n");
	libBtreeWalk(pTable, BtreePreorder, showData, NULL);
	printf("\nendorder:\n");
	libBtreeWalk(pTable, BtreeEndorder, showData, NULL);
	printf("\n");

	libBtreeInsert(pTable, getNextData(100));
	libBtreeInsert(pTable, getNextData(95));
	libBtreeInsert(pTable, getNextData(45));
	libBtreeInsert(pTable, getNextData(195));
	libBtreeInsert(pTable, getNextData(145));
	printf("height 100 through 145: %d\nInorder:\n", libBtreeHeight(pTable));
	libBtreeWalk(pTable, BtreeInorder, showData, NULL);
	printf("\npostorder:\n");
	libBtreeWalk(pTable, BtreePostorder, showData, NULL);
	printf("\npreorder:\n");
	libBtreeWalk(pTable, BtreePreorder, showData, NULL);
	printf("\nendorder:\n");
	libBtreeWalk(pTable, BtreeEndorder, showData, NULL);
	printf("\n");
	libBtreeFind(pTable,getNextData(95),&oldData,0);
	printf("Search(95,): %d\n", *(int *)oldData);
	printf("left(100,): %d\n", pTable->root->leftPtr ? *(int *)pTable->root->leftPtr->entry : 0 );
	printf("right(100,): %d\n", pTable->root->rightPtr ? *(int *)pTable->root->rightPtr->entry : 0);
	err = libBtreeDelete(pTable, getNextData(100), &oldData);
	printf("del(%d,): (err=%d) (old=%d)\n", 100, err, *(int *)oldData);
	printf("inorder:\n");
	libBtreeWalk(pTable, BtreeInorder, showData, NULL);
	printf("\n");
	err = libBtreeDelete(pTable, getNextData(10), &oldData);
	printf("del(%d,): (err=%d) (old=%d)\n", 10, err, *(int *)oldData);
	printf("inorder:\n");
	libBtreeWalk(pTable, BtreeInorder, showData, NULL);
	printf("\nheight %d\n", libBtreeHeight(pTable));
	printf("Part II\n");
	libBtreeDestroy(pTable,NULL,NULL);
#endif
	mbdUsed = 0;
	pTable = libBtreeInit(&callbacks);
	if ( !pTable )
	{
		fprintf(stderr, "libBtreeInit(): Out of memory for second init\n");
		return 1;
	}
	libBtreeInsert(pTable, getNextData(20));
	libBtreeInsert(pTable, getNextData(10));
	libBtreeInsert(pTable, getNextData(30));
	printf("\ninorder:\n");
	libBtreeWalk(pTable, BtreeInorder, showData, NULL);
	printf("\nDelete of 20\n");
	libBtreeDelete(pTable, getNextData(20), NULL);	/* delete the root item */
	printf("\ninorder:\n");
	libBtreeWalk(pTable, BtreeInorder, showData, NULL);
	printf("\nDelete of 30\n");
	libBtreeDelete(pTable, getNextData(30), NULL);
	printf("\ninorder:\n");
	libBtreeWalk(pTable, BtreeInorder, showData, NULL);
	printf("\nDelete of 10\n");
	libBtreeDelete(pTable, getNextData(10), NULL);
	printf("\ninorder:\n");
	libBtreeWalk(pTable, BtreeInorder, showData, NULL);
	printf("\n");
	mbdUsed = 0;
	{
		int lim = mbdAvailable - 2;
		for (ii=0; ii < lim; ++ii)
		{
			libBtreeInsert(pTable, getNextData(ii));
		}
		printf("Height: %d with %d nodes inserted in order\n", libBtreeHeight(pTable), mbdUsed);
		libBtreeDestroy(pTable,NULL,NULL);
		pTable = libBtreeInit(&callbacks);
		if ( !pTable )
		{
			fprintf(stderr, "libBtreeInit(): Out of memory for third init\n");
			return 1;
		}
		mbdUsed = 0;
		for (ii=0; ii < lim; )
		{
			int vv = rand() % lim;
			if ( libBtreeInsert(pTable, getNextData(vv)) == BtreeSuccess )
				++ii;
			else
				--mbdUsed;
		}
		printf("Height: %d with %d nodes inserted randomly\n", libBtreeHeight(pTable), mbdUsed);
	}
	if ( mbdUsed <= 200 )
	{
		printf("\npre-order:\n");
		libBtreeWalk(pTable, BtreePreorder, showData, NULL);
		printf("\npost-order:\n");
		libBtreeWalk(pTable, BtreePostorder, showData, NULL);
		printf("\nin-order:\n");
		libBtreeWalk(pTable, BtreeInorder, showData, NULL);
		printf("\n");
	}
	libBtreeDestroy(pTable,NULL,NULL);
	return 0;
}


