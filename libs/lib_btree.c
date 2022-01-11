/*
    lib_btree.c - a generic AVL balanced tree symbol table example.
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

/* Coded with what was found at Wikipedia:
 * Here: https://en.wikipedia.org/wiki/Binary_search_tree and https://en.wikipedia.org/wiki/AVL_tree
 */

/* Look in the .h file for documentation on how to use this stuff. */
#include "lib_btree.h"
#include <errno.h>

typedef struct
{
	BtreeErrors_t err;
	const char *msg;
} ErrorMsgs_t;
static const ErrorMsgs_t ErrorMsgs[] =
{
	{ BtreeSuccess, "Success" },
	{ BtreeInvalidParam, "Invalid parameter" },
	{ BtreeNoSuchSymbol, "No such symbol" },
	{ BtreeDuplicateSymbol, "Duplicate symbol" },
	{ BtreeOutOfMemory, "Out of memory" },
	{ BtreeEndOfTable, "End of hash table" },
	{ BtreeNotSupported, "Not yet supported" },
	{ BtreeLockFail, "Mutex lock/unlock failure" },
};
const char* btreeErrorString(BtreeErrors_t error)
{
	int ii;
	for (ii = 0; ii < n_elts(ErrorMsgs); ++ii) {
		if ( ErrorMsgs[ii].err == error )
			return ErrorMsgs[ii].msg;
	}
	return "Undefined error code";
}

static void* lclAlloc(void *arg, size_t size)
{
	/* Since we keep the balance factor in the low order
	   two bits of the parent address, make sure the size
	   is a always at least a multiple of 4.
	 */
	size = (size+3)&-4;
	return malloc(size);
}

static void lclFree(void *arg, void *ptr)
{
	free(ptr);
}

static void lclMsg(void *msgArg, BtreeMsgSeverity_t severity, const char *msg)
{
	static const char *Severities[] = { "INFO", "WARN", "ERROR", "FATAL" };
	fprintf(severity > BTREE_SEVERITY_INFO ? stderr:stdout,"%s-libBtree: %s",Severities[severity],msg);
}

BtreeControl_t* libBtreeInit(const BtreeCallbacks_t *callbacks)
{
	BtreeControl_t *tbl;
	BtreeCallbacks_t tCallbacks;
	char emsg[128];
	
	memset(&tCallbacks,0,sizeof(BtreeCallbacks_t));
	if ( callbacks && callbacks->msgOut )
	{
		tCallbacks.msgArg = callbacks->msgArg;
		tCallbacks.msgOut = callbacks->msgOut;
	}
	else
		tCallbacks.msgOut = lclMsg;
	if ( !callbacks || !callbacks->symCmp )
	{
		tCallbacks.msgOut(tCallbacks.msgArg,BTREE_SEVERITY_FATAL,"Must provide a symCmp function\n");
		return NULL;
	}
	tCallbacks.symArg = callbacks->symArg;
	tCallbacks.symCmp = callbacks->symCmp;
	if ( !callbacks->memAlloc && !callbacks->memFree )
	{
		tCallbacks.memAlloc = lclAlloc;
		tCallbacks.memFree = lclFree;
	}
	else if ( !callbacks->memAlloc || !callbacks->memFree )
	{
		tCallbacks.msgOut(tCallbacks.msgArg,BTREE_SEVERITY_FATAL,"Cannot provide just memAlloc or just memFree functions. Must provide both.\n");
		return NULL;
	}
	else
	{
		tCallbacks.memAlloc = callbacks->memAlloc;
		tCallbacks.memFree = callbacks->memFree;
	}
	tCallbacks.memArg = callbacks->memArg;
	tbl = (BtreeControl_t *)tCallbacks.memAlloc(tCallbacks.memArg, sizeof(BtreeControl_t));
	if ( !tbl )
	{
		snprintf(emsg,sizeof(emsg),"Not enough memory to allocate %ld bytes for BtreeControl_t.\n", sizeof(BtreeControl_t));
		tCallbacks.msgOut(tCallbacks.msgArg,BTREE_SEVERITY_FATAL,emsg);
		return NULL;
	}
	memset(tbl, 0, sizeof(BtreeControl_t));
	tbl->callbacks = tCallbacks;
	pthread_mutex_init(&tbl->lock, NULL);
	tbl->numEntries = 0;
	tbl->root = NULL;
	return tbl;
}

static BtreeNode_t* newNode(BtreeControl_t *pTable)
{
	BtreeNode_t *retv;
	retv = (BtreeNode_t *)pTable->callbacks.memAlloc(pTable->callbacks.memArg, sizeof(BtreeNode_t));
	if ( retv )
	{
		retv->entry = NULL;
		retv->upb.parent = NULL;
		retv->leftPtr = NULL;
		retv->rightPtr = NULL;
	}
	return retv;
}

static void destroyUtil(BtreeControl_t *pTable, BtreeNode_t *pNode, void (*entry_free_fn)(void *freeArg, BtreeEntry_t entry), void *freeArg)
{
	if ( pNode )
	{
		if ( entry_free_fn )
			entry_free_fn(freeArg, pNode->entry);
		destroyUtil(pTable, pNode->leftPtr, entry_free_fn, freeArg);
		destroyUtil(pTable, pNode->rightPtr, entry_free_fn, freeArg);
		pTable->callbacks.memFree(pTable->callbacks.memArg, pNode);
	}
}

BtreeErrors_t libBtreeDestroy(BtreeControl_t *pTable, void (*entry_free_fn)(void *freeArg, BtreeEntry_t entry), void *freeArg)
{
	BtreeErrors_t err1;
	
	if ( !pTable )
		return BtreeInvalidParam;
	if ( !(err1=libBtreeLock(pTable)) )
	{
		destroyUtil(pTable, pTable->root, entry_free_fn, freeArg);
		pthread_mutex_unlock(&pTable->lock);
		pthread_mutex_destroy(&pTable->lock);
		pTable->callbacks.memFree(pTable->callbacks.memArg, pTable);
	}
	return err1;
}

/* From Wikipedia at: https://en.wikipedia.org/wiki/AVL_tree */

#define left_child(x) x->leftPtr
#define right_child(x) x->rightPtr

static int BFr(BtreeNode_t *pNode)
{
	/* Return signed balance factor */
	int bf = pNode->upb.balance&3;
	if ( (bf&2) )
		bf |= -2;	/* sign extend */
	return bf;
}

static void BFw(BtreeNode_t *pNode, int balance)
{
	/* write a balance factor */
	pNode->upb.balance = (pNode->upb.balance&-4) | (balance&3);
}

static BtreeNode_t *parentR(BtreeNode_t *pNode)
{
	/* Return parent pointer sans the balance factor */
 	return (BtreeNode_t *)(pNode->upb.balance & -4);
}

static void parentW(BtreeNode_t *pNode, BtreeNode_t *newParent)
{
	/* Keep the balance factor in the low order 2 bits of the parent pointer */
	int oldBF = pNode->upb.balance&3;
	pNode->upb.balance = 0;
	pNode->upb.parent = newParent;
	pNode->upb.balance |= oldBF;
}

static BtreeNode_t* rotate_RightLeft(BtreeNode_t *X, BtreeNode_t *Z)
{
	BtreeNode_t * Y,*t2,*t3;
	/* Z is by 2 higher than its sibling */
	Y = left_child(Z); /* Inner child of Z */
	/* Y is by 1 higher than sibling */
	t3 = right_child(Y);
	left_child(Z) = t3;
	if ( t3 != NULL )
		parentW(t3,Z);
	right_child(Y) = Z;
	parentW(Z,Y);
	t2 = left_child(Y);
	right_child(X) = t2;
	if ( t2 != NULL )
		parentW(t2,X);
	left_child(Y) = X;
	parentW(X,Y);
	/* 1st case, BF(Y) == 0, */
	/*   only happens with deletion, not insertion: */
	if ( BFr(Y) == 0 )
	{
		BFw(X,0);
		BFw(Z,0);
	}
	else
	{
		/* other cases happen with insertion or deletion: */
		if ( BFr(Y) > 0 )
		{
			/* t3 was higher */
			BFw(X,-1);	/* t1 now higher */
			BFw(Z,0);
		}
		else
		{
			/* t2 was higher */
			BFw(X,0);
			BFw(Z,+1);  /* t4 now higher */
		}
	}
	BFw(Y,0);
	return Y; /* return new root of rotated subtree */
}

static BtreeNode_t* rotate_LeftRight(BtreeNode_t *X, BtreeNode_t *Z)
{
	BtreeNode_t * Y,*t2,*t3;
	/* Z is by 2 higher than its sibling */
	Y = right_child(Z); /* Inner child of Z */
	/* Y is by 1 higher than sibling */
	t3 = left_child(Y);
	right_child(Z) = t3;
	if ( t3 != NULL )
		parentW(t3,Z);
	left_child(Y) = Z;
	parentW(Z,Y);
	t2 = right_child(Y);
	left_child(X) = t2;
	if ( t2 != NULL )
		parentW(t2,X);
	right_child(Y) = X;
	parentW(X,Y);
	/* 1st case, BF(Y) == 0, */
	/*   only happens with deletion, not insertion: */
	if ( BFr(Y) == 0 )
	{
		BFw(X,0);
		BFw(Z,0);
	}
	else
	{
		/* other cases happen with insertion or deletion: */
		if ( BFr(Y) > 0 )
		{
			/* t3 was higher */
			BFw(X,-1);	/* t1 now higher */
			BFw(Z,0);
		}
		else
		{
			/* t2 was higher */
			BFw(X,0);
			BFw(Z,+1);  /* t4 now higher */
		}
	}
	BFw(Y,0);
	return Y; /* return new root of rotated subtree */
}

static BtreeNode_t *rotate_Left(BtreeNode_t *X, BtreeNode_t *Z)
{
	BtreeNode_t *t23;
	/* Z is by 2 higher than its sibling */
	t23 = left_child(Z); /* Inner child of Z */
	right_child(X) = t23;
	if ( t23 != NULL )
		parentW(t23,X);
	left_child(Z) = X;
	parentW(X,Z);
	/* 1st case, BF(Z) == 0, */
	/*   only happens with deletion, not insertion: */
	if ( BFr(Z) == 0 ) /* t23 has been of same height as t4 */
	{
		BFw(X,+1);	/* t23 now higher */
		BFw(Z,-1);	/* t4 now lower than X */
	}
	else
	{ /* 2nd case happens with insertion or deletion: */
		BFw(X,0);
		BFw(Z,0);
	}
	return Z; /* return new root of rotated subtree */
}

static BtreeNode_t *rotate_Right(BtreeNode_t *X, BtreeNode_t *Z)
{
	BtreeNode_t *t23;
	/* Z is by 2 higher than its sibling */
	t23 = right_child(Z); /* Inner child of Z */
	left_child(X) = t23;
	if ( t23 != NULL )
		parentW(t23,X);
	right_child(Z) = X;
	parentW(X,Z);
	/* 1st case, BF(Z) == 0, */
	/*   only happens with deletion, not insertion: */
	if ( BFr(Z) == 0 ) /* t23 has been of same height as t4 */
	{
		BFw(X,+1);   /* t23 now higher */
		BFw(Z,-1);	/* t4 now lower than X */
	}
	else
	{ /* 2nd case happens with insertion or deletion: */
		BFw(X,0);
		BFw(Z,0);
	}
	return Z; /* return new root of rotated subtree */
}

/* Z is the newly added node */
static void reBalanceAfterInsert(BtreeControl_t *pTable, BtreeNode_t *Z)
{
	BtreeNode_t *G,*N,*X;
	
	/* Loop (possibly up to the root) */
	for (X=parentR(Z); X != NULL; X = parentR(Z))
	{
		/* BF(X) has to be updated: */
		if ( Z == right_child(X) ) /* The right subtree increases */
		{
			if ( BFr(X) > 0 ) /* X is right-heavy */
			{
				/* ==> the temporary BF(X) == +2 */
				/* ==> rebalancing is required. */
				G = parentR(X); /* Save parent of X around rotations */
				if ( BFr(Z) < 0 )   /* Right Left Case  (see figure 3) */
					N = rotate_RightLeft(X, Z); /* Double rotation: Right(Z) then Left(X) */
				else                            /* Right Right Case (see figure 2) */
					N = rotate_Left(X, Z);      /* Single rotation Left(X) */
				/* After rotation adapt parent link */
			}
			else
			{
				if ( BFr(X) < 0 )
				{
					BFw(X,0); /* Z's height increase is absorbed at X */
					break; /* Leave the loop */
				}
				BFw(X,+1);
				Z = X; /* Height(Z) increases by 1 */
				continue;
			}
		}
		else
		{ /* Z == left_child(X): the left subtree increases */
			if ( BFr(X) < 0 )
			{ /* X is left-heavy */
			  /* ==> the temporary BF(X) == -2 */
				/* ==> rebalancing is required. */
				G = parentR(X); /* Save parent of X around rotations */
				if ( BFr(Z) > 0 )    /* Left Right Case */
					N = rotate_LeftRight(X, Z); /* Double rotation: Left(Z) then Right(X) */
				else                            /* Left Left Case */
					N = rotate_Right(X, Z);     /* Single rotation Right(X) */
				/* After rotation adapt parent link */
			}
			else
			{
				if ( BFr(X) > 0 )
				{
					BFw(X,0); /* Z’s height increase is absorbed at X. */
					break; /* Leave the loop */
				}
				BFw(X,-1);
				Z = X; /* Height(Z) increases by 1 */
				continue;
			}
		}
		/* After a rotation adapt parent link: */
		/* N is the new root of the rotated subtree */
		/* Height does not change: Height(N) == old Height(X) */
		parentW(N,G);
		if ( G != NULL )
		{
			if ( X == left_child(G) )
				left_child(G) = N;
			else
				right_child(G) = N;
		}
		else
			pTable->root = N; /* N is the new root of the total tree */
		break;
		/* There is no fall thru, only break; or continue; */
	}
	/* Unless loop is left via break, the height of the total tree increases by 1. */
}

/* N is the parent of the deleted node */
static void reBalanceAfterDelete(BtreeControl_t *pTable, BtreeNode_t *N)
{
	BtreeNode_t *G, *X, *Z;
	int b;
	
	/* Loop (possibly up to the root) */
	for (X = parentR(N); X != NULL; X = G)
	{
		G = parentR(X); /* Save parent of X around rotations */
		/* BF(X) has not yet been updated! */
		if ( N == left_child(X) )
		{ /* the left subtree decreases */
			if ( BFr(X) > 0 )
			{
			  /* X is right-heavy */
			  /* ==> the temporary BF(X) == +2 */
			  /* ==> rebalancing is required. */
				Z = right_child(X); /* Sibling of N (higher by 2) */
				b = BFr(Z);
				if ( b < 0 )                    /* Right Left Case  (see figure 3) */
					N = rotate_RightLeft(X, Z); /* Double rotation: Right(Z) then Left(X) */
				else                            /* Right Right Case (see figure 2) */
					N = rotate_Left(X, Z);      /* Single rotation Left(X) */
				/* After rotation adapt parent link */
			}
			else
			{
				if ( BFr(X) == 0 )
				{
					BFw(X,+1); /* N’s height decrease is absorbed at X. */
					break; /* Leave the loop */
				}
				N = X;
				BFw(N,0); /* Height(N) decreases by 1 */
				continue;
			}
		}
		else
		{ /* (N == right_child(X)): The right subtree decreases */
			if ( BFr(X) < 0 )
			{ /* X is left-heavy */
			  /* ==> the temporary BF(X) == -2 */
				/* ==> rebalancing is required. */
				Z = left_child(X); /* Sibling of N (higher by 2) */
				b = BFr(Z);
				if ( b > 0 )                      /*/ Left Right Case */
					N = rotate_LeftRight(X, Z); /* Double rotation: Left(Z) then Right(X) */
				else                            /* Left Left Case */
					N = rotate_Right(X, Z);     /* Single rotation Right(X) */
				/* After rotation adapt parent link */
			}
			else
			{
				if ( BFr(X) == 0 )
				{
					BFw(X,-1); /* N’s height decrease is absorbed at X. */
					break; /* Leave the loop */
				}
				N = X;
				BFw(N,0); /* Height(N) decreases by 1 */
				continue;
			}
		}
		/* After a rotation adapt parent link: */
		/* N is the new root of the rotated subtree */
		parentW(N,G);
		if ( G != NULL )
		{
			if ( X == left_child(G) )
				left_child(G) = N;
			else
				right_child(G) = N;
		}
		else
			pTable->root = N; /* N is the new root of the total tree */

		if ( b == 0 )
			break; /* Height does not change: Leave the loop */

		/* Height(N) decreases by 1 (== old Height(X)-1) */
	}
	/* If (b != 0) the height of the total tree decreases by 1. */
}

static BtreeErrors_t insert(BtreeControl_t *pTable, BtreeEntry_t xx, BtreeNode_t *ptr, BtreeNode_t **ppInsertedNode)
{
	BtreeNode_t *temp;
	int diff;

	if ( !ptr )
	{
		/* empty root */
		temp = newNode(pTable);
		temp->entry = xx;
		pTable->root = temp;
		if ( ppInsertedNode )
			*ppInsertedNode = temp;
		return BtreeSuccess;
	}
	diff = pTable->callbacks.symCmp(pTable->callbacks.symArg, xx, ptr->entry);
	if ( diff == 0 )
	{
		if ( ppInsertedNode )
			*ppInsertedNode = NULL;
		return BtreeDuplicateSymbol;
	}
	if ( diff > 0 )
	{
		if ( ptr->rightPtr )
			return insert(pTable, xx, ptr->rightPtr,ppInsertedNode);
		temp = newNode(pTable);
		temp->entry = xx;
		temp->upb.parent = ptr;
		ptr->rightPtr = temp;
		if ( ppInsertedNode )
			*ppInsertedNode = temp;
		return BtreeSuccess;
	}
	if ( ptr->leftPtr )
		return insert(pTable, xx, ptr->leftPtr, ppInsertedNode);
	temp = newNode(pTable);
	temp->entry = xx;
	temp->upb.parent = ptr;
	ptr->leftPtr = temp;
	if ( ppInsertedNode )
		*ppInsertedNode = temp;
	return BtreeSuccess;
}

static BtreeErrors_t replace(BtreeControl_t *pTable, BtreeEntry_t xx, BtreeNode_t *ptr, BtreeEntry_t *pExisting)
{
	BtreeNode_t *temp;
	int diff;

	if ( !ptr )
	{
		/* root is NULL so empty tree */
		temp = newNode(pTable);
		temp->entry = xx;
		pTable->root = temp;
		return BtreeSuccess;
	}
	diff = pTable->callbacks.symCmp(pTable->callbacks.symArg, xx, ptr->entry);
	if ( diff == 0 )
	{
		/*  Found the entry. Just replace the data. Return the existing data to caller */
		if ( pExisting )
			*pExisting = ptr->entry;
		ptr->entry = xx;
		return BtreeSuccess;
	}
	if ( diff > 0 )
	{
		/* The new one is greater than the existing so put it in the right tree */
		if ( ptr->rightPtr )
			return replace(pTable, xx, ptr->rightPtr, pExisting);
		temp = newNode(pTable);
		temp->entry = xx;
		temp->upb.parent = ptr;
		ptr->rightPtr = temp;
		return BtreeSuccess;
	}
	/* else the new one is less then the existing so put it in the left one */
	if ( ptr->leftPtr )
		return replace(pTable, xx, ptr->leftPtr, pExisting);
	temp = newNode(pTable);
	temp->entry = xx;
	temp->upb.parent = ptr;
	ptr->leftPtr = temp;
	return BtreeSuccess;
}

static int preorder(BtreeControl_t *pTable, BtreeNode_t *ptr, BtreeWalkCallback_t callback_fn, void *userData)
{
	int err=0;
	
	if ( ptr )
	{
		err = callback_fn(userData,ptr->entry);
		if ( !err )
			err = preorder(pTable, ptr->leftPtr, callback_fn, userData);
		if ( !err )
			err = preorder(pTable, ptr->rightPtr, callback_fn, userData );
	}
	return err;
}

static int postorder(BtreeControl_t *pTable, BtreeNode_t *ptr, BtreeWalkCallback_t callback_fn, void *userData)
{
	int err=0;
	
	if ( ptr )
	{
		err = postorder(pTable, ptr->leftPtr, callback_fn, userData);
		if ( !err )
			err = postorder(pTable, ptr->rightPtr, callback_fn, userData);
		if ( !err )
			err = callback_fn(userData,ptr->entry);
	}
	return err;
}

static int inorder(BtreeControl_t *pTable, BtreeNode_t *ptr, BtreeWalkCallback_t callback_fn, void *userData)
{
	int err=0;
	
	if ( ptr )
	{
		err = inorder(pTable, ptr->leftPtr, callback_fn, userData);
		if ( !err )
			callback_fn(userData, ptr->entry);
		if ( !err )
			err = inorder(pTable, ptr->rightPtr, callback_fn, userData);
	}
	return err;
}

static int endorder(BtreeControl_t *pTable, BtreeNode_t *ptr, BtreeWalkCallback_t callback_fn, void *userData)
{
	int err=0;
	
	if ( ptr )
	{
		err = endorder(pTable, ptr->rightPtr, callback_fn, userData);
		if ( !err )
			err = callback_fn(userData, ptr->entry);
		if ( !err )
			err = endorder(pTable, ptr->leftPtr, callback_fn, userData);
	}
	return err;
}

static BtreeNode_t* search(BtreeControl_t *pTable, BtreeEntry_t xx, BtreeNode_t *ptr)
{
	int diff;

	while ( ptr && (diff = pTable->callbacks.symCmp(pTable->callbacks.symArg, xx, ptr->entry)) )
	{
		if ( diff > 0 )
			ptr = ptr->rightPtr;
		else
			ptr = ptr->leftPtr;
	}
	return ptr;
}

#if 0	/* not used by anybody. But if they were, this is what they'd do */
static BtreeNode_t *treeMax(BtreeNode_t *pNode)
{
	while ( (pNode->rightPtr) )
		pNode = pNode->rightPtr;
	return pNode;
}

#if 0 /* from wiki */
Tree-Predecessor(x)
   if x.left ≠ NIL then
     return Tree-Maximum(x.left)
   end if
   y := x.parent
   while y ≠ NIL and x = y.left then
     x := y
     y := y.parent
   repeat
   return y
#endif

static BtreeNode_t *predecessor(BtreeNode_t **pNode)
{
	BtreeNode_t *xx,*yy;
	
	xx = *pNode;
	if (xx->leftPtr != NULL )
		return treeMax(xx->leftPtr);
	yy = parentR(xx);
	while ( yy && (xx=yy->leftPtr) )
	{
		xx = yy;
		yy = parentR(yy);
	}
	*pNode = xx;
	return yy;
}
#endif
	
static BtreeNode_t* treeMin(BtreeNode_t *pNode)
{
	while ( (pNode->leftPtr) )
		pNode = pNode->leftPtr;
	return pNode;
}

#if 0	/* from wiki */
Tree-Successor(x)
   if x.right ≠ NIL then
     return Tree-Minimum(x.right)
   end if
   y := x.parent
   while y ≠ NIL and x = y.right then
     x := y
     y := y.parent
   repeat
   return y
#endif

static BtreeNode_t* successor(BtreeNode_t *xx)
{
	BtreeNode_t *yy;

	if ( xx->rightPtr )
		return treeMin(xx->rightPtr);
	yy = parentR(xx);
	while ( yy && yy->rightPtr )
		yy = parentR(yy);
	return yy;
}

#if 0	/* from wiki */
1    Subtree-Shift(T, u, v)
2      if u.parent = NIL then
3        T.root := v
4      else if u = u.parent.left then
5        u.parent.left := v
5      else
6        u.parent.right := v
7      end if
8      if v ≠ NIL then
9        v.parent := u.parent
10     end if
#endif

/* Replace the node uu with vv in the process of deletion */
static void treeShift(BtreeControl_t *pTable, BtreeNode_t *uu, BtreeNode_t *vv)
{
	BtreeNode_t *puu = parentR(uu);
	if ( !puu )
		pTable->root = vv;
	else if ( uu == puu->leftPtr )
		puu->leftPtr = vv;
	else
		puu->rightPtr = vv;
	if ( vv )
		parentW(vv,puu);
}

#if 0	/* from Wiki */
T = root of tree
z = node to delete

1    Tree-Delete(T, z)
2      if z.left = NIL then
3        Subtree-Shift(T, z, z.right)
4      else if z.right = NIL then
5        Subtree-Shift(T, z, z.left)
6      else
7        y := Tree-Successor(z)
8        if y.parent ≠ z then
9          Subtree-Shift(T, y, y.right)
10         y.right := z.right
11         y.right.parent := y
12       end if
13       Subtree-Shift(T, z, y)
14       y.left := z.left
15       y.left.parent := y
16     end if
#endif

static BtreeErrors_t del(BtreeControl_t *pTable, BtreeEntry_t xx, BtreeEntry_t *pExisting)
{
	BtreeNode_t *yy, *zz, *ww=NULL;

	if ( pExisting )
		*pExisting = 0;
	if ( !(zz = search(pTable, xx, pTable->root)) )
		return BtreeNoSuchSymbol;
	if ( !zz->leftPtr )
		treeShift(pTable, zz, (ww=zz->rightPtr));
	else if ( !zz->rightPtr )
		treeShift(pTable, zz, (ww=zz->leftPtr));
	else
	{
		yy = successor(zz);
		if ( parentR(yy) != zz )
		{
			treeShift(pTable, yy, yy->rightPtr);
			yy->rightPtr = zz->rightPtr;
			parentW(parentR(yy->rightPtr),yy);
		}
		treeShift(pTable, zz, (ww=yy));
		yy->leftPtr = zz->leftPtr;
		parentW(parentR(yy->leftPtr),yy);
	}
	if ( pExisting )
		*pExisting = zz->entry;
#if 1
	if ( ww )
		reBalanceAfterDelete(pTable, ww);
#endif
	pTable->callbacks.memFree(pTable->callbacks.memArg, zz);
	return BtreeSuccess;
}

static int lclHeight(BtreeControl_t *pTable, BtreeNode_t *ptr, int count)
{
	if ( ptr )
	{
		int xx, yy;
		xx = lclHeight(pTable, ptr->leftPtr, count + 1);
		yy = lclHeight(pTable, ptr->rightPtr, count + 1);
		return (xx > yy ? xx : yy);
	}
	return count;
}

int libBtreeHeight(BtreeControl_t *pTable)
{
	return lclHeight(pTable,pTable->root,0);
}

BtreeErrors_t libBtreeInsert(BtreeControl_t *pTable, const BtreeEntry_t entry)
{
	BtreeErrors_t err1, err2=BtreeSuccess;
	BtreeNode_t *pInsertedNode;
	
	if ( !(err1=libBtreeLock(pTable)) )
	{
		err1 = insert(pTable, entry, pTable->root, &pInsertedNode);
		if ( err1 == BtreeSuccess )
			reBalanceAfterInsert(pTable,pInsertedNode);
		err2 = libBtreeUnlock(pTable);
	}
	return err1 ? err1 : err2;
}

BtreeErrors_t libBtreeReplace(BtreeControl_t *pTable, const BtreeEntry_t entry, BtreeEntry_t *pExisting)
{
	BtreeErrors_t err1, err2=BtreeSuccess;
	
	if ( pExisting )
		*pExisting = 0;
	if ( !(err1=libBtreeLock(pTable)) )
	{
		err1 = replace(pTable, entry, pTable->root, pExisting);
		err2 = libBtreeUnlock(pTable);
	}
	return err1 ? err1 : err2;
}

BtreeErrors_t libBtreeDelete(BtreeControl_t *pTable, const BtreeEntry_t entry, BtreeEntry_t *pExisting)
{
	BtreeErrors_t err1, err2=BtreeSuccess;
	
	if ( pExisting )
		*pExisting = 0;
	if ( !(err1=libBtreeLock(pTable)) )
	{
		err1 = del(pTable, entry, pExisting);
		err2 = libBtreeUnlock(pTable);
	}
	return err1 ? err1 : err2;
}

BtreeErrors_t libBtreeFind(BtreeControl_t *pTable, const BtreeEntry_t entry, BtreeEntry_t *pResult, int alreadyLocked)
{
	BtreeNode_t *old;
	BtreeErrors_t err1=BtreeSuccess, err2=BtreeSuccess;
	
	if ( pResult )
		*pResult = NULL;
	if ( !alreadyLocked )
		err1 = libBtreeLock(pTable);
	if ( err1 == BtreeSuccess )
	{
		old = search(pTable, entry, pTable->root);
		if ( old )
		{
			if ( pResult )
				*pResult = old->entry;
		}
		else
			err1 = BtreeNoSuchSymbol;
		if ( !alreadyLocked )
			err2 = libBtreeUnlock(pTable);
	}
	return err1 ? err1 : err2;
}

int libBtreeWalk(BtreeControl_t *pTable, BtreeOrders_t order, BtreeWalkCallback_t callback_fn, void *pUserData)
{
	int err1, err2=BtreeSuccess;
	
	if ( !(err1=libBtreeLock(pTable)) )
	{
		switch (order)
		{
		case BtreeInorder:
			err1 = inorder(pTable,pTable->root,callback_fn,pUserData);
			break;
		case BtreePreorder:
			err1 = preorder(pTable,pTable->root,callback_fn,pUserData);
			break;
		case BtreePostorder:
			err1 = postorder(pTable,pTable->root,callback_fn,pUserData);
			break;
		case BtreeEndorder:
			err1 = endorder(pTable,pTable->root,callback_fn,pUserData);
			break;
		}
		if ( err1 )
			err1 += BtreeMaxError;
		err2 = libBtreeUnlock(pTable);
	}
	return err1 ? err1 : err2;
}

BtreeErrors_t libBtreeLock(BtreeControl_t *pTable)
{
	if ( pthread_mutex_lock(&pTable->lock) )
	{
		if ( (pTable->verbose&BTREE_VERBOSE_ERROR) )
		{
			char emsg[128];
			snprintf(emsg,sizeof(emsg),"Failed to lock mutex: %s\n", strerror(errno));
			pTable->callbacks.msgOut(pTable->callbacks.msgArg, BTREE_SEVERITY_ERROR, emsg);
		}
		return BtreeLockFail;
	}
	return BtreeSuccess;
}

BtreeErrors_t  libBtreeUnlock(BtreeControl_t *pTable)
{
	if ( pthread_mutex_unlock(&pTable->lock) )
	{
		if ( (pTable->verbose&BTREE_VERBOSE_ERROR) )
		{
			char emsg[128];
			snprintf(emsg,sizeof(emsg),"Failed to unlock mutex: %s\n", strerror(errno));
			pTable->callbacks.msgOut(pTable->callbacks.msgArg, BTREE_SEVERITY_ERROR, emsg);
		}
		return BtreeLockFail;
	}
	return BtreeSuccess;
}

