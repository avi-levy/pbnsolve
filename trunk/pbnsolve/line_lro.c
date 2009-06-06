/* Copyright 2007 Jan Wolter
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "pbnsolve.h"

#define BGCOLOR 0

#define colbit(i) (col+(puz->colsize*i))

/* Allocate some arrays to be used in left_solve, right_solve, and
 * to a size appropriate for puzzle puz.
 */

static int *lpos, *rpos, *cov;
static bit_type *oldval;
static int *nbcolor;
static bit_type *col;

void line_prealloc(Puzzle *puz)
{
    int maxnclue= 0, maxdimension= 0;
    int i,k;

    /* Find maximum number of numbers in any clue in any direction and
     * maximum length of a line
     */
    for (k= 0; k < puz->nset; k++)
    {
	if (puz->n[k] > maxdimension) maxdimension= puz->n[k];

    	for (i= 0; i < puz->n[k]; i++)
	    if (puz->clue[k][i].n > maxnclue) maxnclue= puz->clue[k][i].n;
    }

    /* Allocate storage spaces for left_solve and right_solve arrays */
    lpos= (int *)malloc((maxnclue + 1) * sizeof(int));
    rpos= (int *)malloc((maxnclue + 1) * sizeof(int));
    cov= (int *)malloc(maxnclue * sizeof(int));

    /* Store size of bit arrays */
    puz->colsize= bit_size(puz->ncolor);
    /* An extra color bit map for apply_lro */
    oldval= (bit_type*)malloc(puz->colsize * sizeof(bit_type));

    col= (bit_type *)malloc(maxdimension * puz->colsize * sizeof(bit_type));
    if (puz->ncolor > 2)
	nbcolor= (int *)malloc(puz->ncolor * sizeof(int));
}


void dump_lro_solve(Puzzle *puz, Solution *sol, int k, int i, bit_type *col)
{
    int ncell= count_cells(puz, sol, k, i);
    int j, c;
    for (j= 0; j < ncell; j++)
    {
        printf("%-2d ",j);
	dump_bits(stdout, puz, colbit(j));
	putchar('\n');
    }
}


/* Finite State Machine States for the line solver */

#define NEWBLOCK	0
#define PLACEBLOCK	1
#define FINALSPACE	2
#define CHECKREST	3
#define BACKTRACK	4
#define ADVANCEBLOCK	5
#define HALT		6

/* Find leftmost solution for line i of clueset k
 *
 * On success, returns a pointer to an array of starting indexes for blocks.
 * This memory should not be freed.  On failure, returns NULL.
 *
 * This routine is structured as a finite state machine.  The state variable
 * 'state' selects between various states we might be in.  Each state has a
 * comment defining the precondition for that state.  This is basically a
 * trick to make old-fashioned spaghetti code look like it makes sense.
 */

int *left_solve(Puzzle *puz, Solution *sol, int k, int i)
{
    int b,j, currcolor, nextcolor, backtracking, state;
    Clue *clue= &puz->clue[k][i];
    Cell **cell= sol->line[k][i];

    /* Set a flag if the puzzle is multicolored.  If not, we can skip some
     * tests which will never be true and be just a bit more efficient.
     */
    int multicolor= (puz->ncolor > 2);

    /* The lpos array contains current position of each block, or more
     * specifically the first cell of each block.  It's terminated by a -1.
     */
    lpos[clue->n]= -1;

    /* The cov array contains the index of the left-most cell covered by the
     * block which CANNOT be white.  It is -1 if there is no such cell.
     */

    b= 0;
    state= NEWBLOCK;
    backtracking= 0;

    /* Finite State Machine */

    while (state != HALT)
    {
    	switch (state)
	{
	case NEWBLOCK:

	    /* Precondition: Blocks 0 through b-1 have been legally placed
	     *    or b = 0.  If b > 0, then j is the index of the first cell
	     *    after block b-1.
	     * Action:  Begin placing block b after the previous one.
	     */

	    if (b >= clue->n)
	    {
		/* No blocks left.  Check rest of line to make sure it can
		 * be blank.
		 */
		if (b-- == 0) j= 0;
	    	state= CHECKREST; goto next;
	    }

	    currcolor= clue->color[b];	/* Color of current block */
	    if (VL) printf("L: PLACING BLOCK %d COLOR %d LENGTH %d\n",
	    	b,currcolor,clue->length[b]);

	    /* Earliest possible position of block b, after previous blocks,
	     * leaving a space between the blocks if they are the same color.
	     */

	    lpos[b]= (b == 0) ? 0 :
			j + ((clue->color[b-1] == currcolor) ? 1 : 0);

	    if (cell[lpos[b]] == NULL)
		return NULL;

	    if (VL) printf("L: FIRST POS %d\n",lpos[b]);

	    state= PLACEBLOCK; goto next;

	case PLACEBLOCK:

	    /* Precondition: Blocks 0 through b-1 have been legally placed
	     *    or b = 0.  lpos[b] points points to a position less than or
	     *    equal to the position of block b.  currcolor is the color
	     *    of the current block b.
	     * Action:  Advance lpos[b] until it points to a position where
	     *    the next length[b] cells can all accomodate the block's
	     *    color.  Also compute cov[b], the index of the leftmost
	     *    cell covered by the block that cannot be background.
	     */

	    /* First find a place for the first cell of the block, skipping
	     * over white cells until we find one.
	     */

	    while (!may_be(cell[lpos[b]], currcolor))
	    {
		if (VL) printf("L: POS %d BLOCKED\n",lpos[b]);

		if (multicolor && !may_be(cell[lpos[b]], BGCOLOR))
		{
		    /* We hit a cell that must be some color other
		     * than the color of the current block.  Only hope is to
		     * find a previously placed block that can be advanced to
		     * cover it, so we backtrack.
		     */
		    if (VL) printf("L: BACKTRACKING ON WRONG COLOR\n",lpos[b]);
		    j= lpos[b];
		    while (b > 1 && !may_be(cell[j], clue->color[b-1]))
			b--;
		    state= BACKTRACK; goto next;
		}

		if (VL) printf("L: SHIFT BLOCK TO %d\n",lpos[b]+1);

		if (cell[++lpos[b]] == NULL)
		{
		    if (VL) printf("L: END OF THE LINE\n");
		    return NULL;	/* Hit end of line */
		}
	    }

	    /* First cell has found a home, can we place the rest of the block?
	     * While we are at at, compute cov[b].
	     */
	    if (VL) printf("L: FIRST CELL OF %d PLACED AT %d\n",b,lpos[b]);
	    j= lpos[b];
	    cov[b]= (may_be(cell[j], BGCOLOR) ? -1 : j);
	    for (j++; j - lpos[b] < clue->length[b]; j++)
	    {
		if (VL) printf("L: CHECKING CELL AT %d\n",j);
		if (cell[j] == NULL)
		{
		    if (VL) printf("L: END OF THE LINE\n");
		    return NULL;	/* Block runs off end of line */
		}

		if (!may_be(cell[j], currcolor))
		{
		    if (VL) printf("L: FAILED AT %d\n",j);
		    if (cov[b] == -1)
		    {
			/* block doesn't fit here, but it wasn't
			 * covering anything, so just keep shifting it
			 */
			if (VL) printf("L: SKIP AHEAD\n");
			lpos[b]= j;
			state= PLACEBLOCK; goto next;
		    }
		    else
		    {
			/* Block b cannot be placed.  Need to try advancing
			 * block b-1.
			 */
			if (VL) printf("L: BACKTRACK\n");
			state= BACKTRACK; goto next;
		    }
		}

		/* Update cov[b] */
		if (cov[b] == -1 && !may_be(cell[j], BGCOLOR))
		    cov[b]= j;

		if (VL) printf("L: OK AT %d (cov=%d)\n",j, cov[b]);
	    }

	    state= FINALSPACE; goto next;

	case FINALSPACE:

	    /* Precondition: Blocks 0 through b-1 have been legally placed
	     *    or b = 0.  lpos[b] points points to a position so that all
	     *    cells of the block can accomodate the block's color.  j
	     *    is the next of the first cell after the block.  currcolor
	     *    is the color of the current block b.
	     * Action:  Check if the first cell of the block is a legal
	     *    color.  If it is the same color as the block, try advancing
	     *    the block to cover that too.  If backtracking is true,
	     *    also ensure the block covers something, and advance it until
	     *	  it does.
	     */

	    if (VL) printf("L: CHECKING FINAL SPACE\n");

	    if (cell[j] != NULL)
	    {
		/* OK, we've found a place the block fits, now we check that
		 * first cell AFTER the block is able to be some color different
		 * than the block's color.
		 */
		nextcolor= (multicolor &&
			(b < clue->n - 1) &&
			(currcolor != clue->color[b+1])) ?
			    clue->color[b+1] : BGCOLOR;

		if (VL) printf("L:  NEXTCOLOR=%d\n",nextcolor);

		while (cell[j] != NULL && !may_be(cell[j], BGCOLOR) &&
			(nextcolor == BGCOLOR || !may_be(cell[j],nextcolor)))
		{
		    /* Next cell is wrong color, so try advancing the block one
		     * cell.  But we need to make sure that we don't uncover any
		     * cells, and we need to make sure that the cell at the end
		     * is one we can cover
		     */
		    if (VL) printf("L: NO FINAL SPACE\n");
		    if (cov[b] == lpos[b] ||
			(multicolor && !may_be(cell[j],currcolor)) )
		    {
			/* can't advance.  BACKTRACK */
			state= BACKTRACK; goto next;
		    }
		    /* All's OK - go ahead and advance the block */
		    lpos[b]++;
		    if (cov[b] == -1 && !may_be(cell[j], BGCOLOR))
			cov[b]= j;
		    j++;
		    if (VL) printf("L: ADVANCING BLOCK (cov=%d)\n",cov[b]);
		}
	    }

	    /* At this point, we have successfully placed the block */

	    /* If we are advancing a block after backtracking, it needs to
	     * cover something.
	     */
	    if (backtracking && cov[b] == -1)
	    {
		if (VL) printf("L: BACKTRACK BLOCK COVERS NOTHING\n",cov[b]);
		backtracking= 0;
		state= ADVANCEBLOCK; goto next;
	    }

	    if (cell[j] == NULL && b < clue->n - 1)
		/* Ran out of space, but still have blocks left */
		return NULL;

	    /* Successfully placed block b - Go on to next block */
	    b++;
	    backtracking= 0;
	    state= NEWBLOCK; goto next;

	case CHECKREST:

	    /* Precondition: All blocks have been legally placed.  j points
	     *   to first empty space after last block.  b is the index of
	     *   the last block, or -1 if there are no blocks.
	     * Action:  Check that there are no cells after the last block
	     *   which cannot be left background color.
	     */

	    if (cell[j] != NULL)
	    {
		if (VL) printf("L: PLACED LAST BLOCK - CHECK REST OF LINE\n");

		for (; cell[j] != NULL; j++)
		{
		    if (VL) printf("L: CELL %d ",j);
		    if (!may_be(cell[j], BGCOLOR))
		    {
			/* Check if we can cover the uncovered square by sliding
			 * the last block right far enough to cover it
			 */
			if (VL) printf("NEEDS COVERAGE\n");
			j= lpos[b] + clue->length[b];
			state= ADVANCEBLOCK; goto next;
		    }
		    else if (VL)
			printf("OK\n");
		}
	    }

	    state= HALT; goto next;

	case BACKTRACK:

	    /* Precondition: Blocks 0 through b-1 have been legally placed,
	     *   but block b cannot be.
	     * Action:  Try to advance block b-1 so that it covers something
	     *   new that can't be background color, without uncovering
	     *   anything that it previously covered.
	     */

	    if (--b < 0)
	    {
		if (VL) printf("L: NO BLOCKS LEFT TO BACKTRACK TO - FAIL\n");
		return NULL;
	    }

	    j= lpos[b] + clue->length[b];/* First cell after end of block */
	    currcolor= clue->color[b];	/* Color of current block */

	    if (VL) printf("L: BACKTRACKING: ");

	    state= ADVANCEBLOCK; goto next;

	case ADVANCEBLOCK:

	    /* Precondition: Blocks 0 through b have been legally placed.
	     *   j points to the first cell after block b.  currcolor is the
	     *   color of block b.
	     * Action:  Try to advance block b so that it covers at least one
	     *   new cell that can't be background color, without uncovering
	     *   anything that block b previously covered.
	     */

	    if (VL)
		printf("L: ADVANCE BLOCK %d (j=%d,cc=%d,pos=%d,cov=%d)\n",
		    b,j,currcolor,lpos[b],cov[b]);

	    while(cov[b] < 0 || lpos[b] < cov[b])
	    {
		if (!may_be(cell[j], currcolor))
		{
		    if (VL) printf("L: ADVANCE HIT OBSTACLE ");
		    if (cov[b] > 0 || (multicolor && !may_be(cell[j], BGCOLOR)))
		    {
			if (VL) printf("- BACKTRACKING\n");
			state= BACKTRACK; goto next;
		    }
		    else
		    {
			/* Hit something we can't advance over, but we aren't
			 * covering anything either, so jump past the obstacle.
			 */
			lpos[b]= j+1;
			if (VL) printf("- JUMPING pos=%d\n",lpos[b]);
			backtracking= 1;
			state= PLACEBLOCK; goto next;
		    }
		}

		/* Can advance.  Do it */
		lpos[b]++;
		if (VL) printf("- ADVANCING BLOCK %d TO %d\n",b,lpos[b]);

		/* Check if we have covered anything.  If we have, we can
		 * stop advancing
		 */
		if (!may_be(cell[j++], BGCOLOR))
		{
		    if (cov[b] == -1) cov[b]= j-1;
		    if (VL)
			printf("L: COVERED NEW TARGET AT %d - BLOCK AT %d\n",
			    j-1, lpos[b]);
		    state= FINALSPACE; goto next;
		}
		if (cell[j] == NULL)
		{
		    if (VL) printf("L: END OF LINE\n");
		    return NULL;
		}
	    }

	    /* If we drop out here, then we've advanced the block as far
	     * as we can, but we haven't succeeded in covering anything new.
	     * So backtrack further.
	     */
	    if (VL) printf("L: CAN'T ADVANCE\n");
	    state= BACKTRACK;
	    goto next;
	}
	next:;
    }

    if (VL) printf("L: DONE\n");
    return lpos;
}


/* Find rightmost solution for line i of clueset k
 *
 * On success, returns a pointer to an array of ending indexes for blocks.
 * This array should not be freed.  On failure, returns NULL.
 *
 * This routine is structured as a finite state machine.  The state variable
 * 'state' selects between various states we might be in.  Each state has a
 * comment defining the precondition for that state.  This is basically a
 * trick to make old-fashioned spaghetti code look like it makes sense.
 */

int *right_solve(Puzzle *puz, Solution *sol, int k, int i, int ncell)
{
    int b,j, currcolor, nextcolor, backtracking, state;
    Clue *clue= &puz->clue[k][i];
    Cell **cell= sol->line[k][i];
    int maxblock= clue->n - 1;

    /* Set a flag if the puzzle is multicolored.  If not, we can skip some
     * tests which will never be true and be just a bit more efficient.
     */
    int multicolor= (puz->ncolor > 2);

    /* The rpos array contains current position of each block, or more
     * specifically the rightmost cell of each block.  It's terminated by a -1.
     */
    rpos[clue->n]= -1;

    /* The cov array contains the index of the right-most cell covered by the
     * block which CANNOT be white.  It is -1 if there is no such cell.
     */

    b= maxblock;
    state= NEWBLOCK;
    backtracking= 0;

    /* Finite State Machine */

    while (state != HALT)
    {
    	switch (state)
	{
	case NEWBLOCK:

	    /* Precondition: Blocks b+1 through maxblock have been legally
	     *    placed or b = maxblock.  If b < maxblock, then j is the
	     *    index of the first cell left of block b+1.
	     * Action:  Begin placing block b after the previous one.
	     */

	    if (b < 0)
	    {
		/* No blocks left.  Check rest of line to make sure it can
		 * be blank.
		 */
		if (b++ == maxblock) j= ncell - 1;
	    	state= CHECKREST; goto next;
	    }

	    currcolor= clue->color[b];	/* Color of current block */
	    if (VL) printf("L: PLACING BLOCK %d COLOR %d LENGTH %d\n",
	    	b,currcolor,clue->length[b]);

	    /* Earliest possible position of block b, after previous blocks,
	     * leaving a space between the blocks if they are the same color.
	     */

	    rpos[b]= (b == maxblock) ? ncell - 1 :
			j - ((clue->color[b+1] == currcolor) ? 1 : 0);

	    if (rpos[b] - clue->length[b] + 1 < 0)
		return NULL;

	    if (VL) printf("L: FIRST POS %d\n",rpos[b]);

	    state= PLACEBLOCK; goto next;

	case PLACEBLOCK:

	    /* Precondition: Blocks b+1 through maxblock have been legally
	     *   placed or b = maxblock.  rpos[b] points points to a position
	     *   greater than or equal to the position of block b.  currcolor
	     *   is the color of the current block b.
	     * Action:  Advance rpos[b] until it points to a position where
	     *   the next length[b] cells can all accomodate the block's
	     *   color.  Also compute cov[b], the index of the rightmost
	     *   cell covered by the block that cannot be background.
	     */

	    /* First find a place for the first cell of the block, skipping
	     * over white cells until we find one.
	     */

	    while (!may_be(cell[rpos[b]], currcolor))
	    {
		if (VL) printf("L: POS %d BLOCKED\n",rpos[b]);

		if (multicolor && !may_be(cell[rpos[b]], BGCOLOR))
		{
		    /* We hit a cell that must be some color other
		     * than the color of the current block.  Only hope is to
		     * find a previously placed block that can be advanced to
		     * cover it, so we backtrack.
		     */
		    if (VL) printf("L: BACKTRACKING ON WRONG COLOR\n",rpos[b]);
		    j= rpos[b];
		    while (b < maxblock-1 && !may_be(cell[j], clue->color[b+1]))
			b++;
		    state= BACKTRACK; goto next;
		}

		if (VL) printf("L: SHIFT BLOCK TO %d\n",rpos[b]-1);

		if (--rpos[b] < 0)
		{
		    if (VL) printf("L: END OF THE LINE\n");
		    return NULL;	/* Hit end of line */
		}
	    }

	    /* First cell has found a home, can we place the rest of the block?
	     * While we are at at, compute cov[b].
	     */
	    if (VL) printf("L: FIRST CELL OF %d PLACED AT %d\n",b,rpos[b]);
	    j= rpos[b];
	    cov[b]= (may_be(cell[j], BGCOLOR) ? -1 : j);
	    for (j--; rpos[b] - j < clue->length[b]; j--)
	    {
		if (VL) printf("L: CHECKING CELL AT %d\n",j);
		if (j < 0)
		{
		    if (VL) printf("L: END OF THE LINE\n");
		    return NULL;	/* Block runs off end of line */
		}

		if (!may_be(cell[j], currcolor))
		{
		    if (VL) printf("L: FAILED AT %d\n",j);
		    if (cov[b] == -1)
		    {
			/* block doesn't fit here, but it wasn't
			 * covering anything, so just keep shifting it
			 */
			if (VL) printf("L: SKIP AHEAD\n");
			rpos[b]= j;
			state= PLACEBLOCK; goto next;
		    }
		    else
		    {
			/* Block b cannot be placed.  Need to try advancing
			 * block b+1.
			 */
			if (VL) printf("L: BACKTRACK\n");
			state= BACKTRACK; goto next;
		    }
		}

		/* Update cov[b] */
		if (cov[b] == -1 && !may_be(cell[j], BGCOLOR))
		    cov[b]= j;

		if (VL) printf("L: OK AT %d (cov=%d)\n",j, cov[b]);
	    }

	    state= FINALSPACE; goto next;

	case FINALSPACE:

	    /* Precondition: Blocks maxblock through b+1 have been legally
	     *    placed or b = maxblock.  rpos[b] points points to a position
	     *    so that all cells of the block can accomodate the block's
	     *    color.  j is the next of the first cell after the block.
	     *    currcolor is the color of the current block b.
	     * Action:  Check if the first cell of the block is a legal
	     *    color.  If it is the same color as the block, try advancing
	     *    the block to cover that too.  If backtracking is true,
	     *    also ensure the block covers something, and advance it until
	     *	  it does.
	     */

	    if (VL) printf("L: CHECKING FINAL SPACE\n");

	    if (j >= 0)
	    {
		/* OK, we've found a place the block fits, now we check that
		 * first cell AFTER the block is able to be some color different
		 * than the block's color.
		 */
		nextcolor= (multicolor &&
			(b > 0) &&
			(currcolor != clue->color[b-1])) ?
			    clue->color[b-1] : BGCOLOR;

		if (VL) printf("L:  NEXTCOLOR=%d\n",nextcolor);

		while (j >= 0 && !may_be(cell[j], BGCOLOR) &&
			(nextcolor == BGCOLOR || !may_be(cell[j],nextcolor)))
		{
		    /* Next cell is wrong color, so try advancing the block one
		     * cell.  But we need to make sure that we don't uncover any
		     * cells, and we need to make sure that the cell at the end
		     * is one we can cover
		     */
		    if (VL) printf("L: NO FINAL SPACE\n");
		    if (cov[b] == rpos[b] ||
			(multicolor && !may_be(cell[j],currcolor)) )
		    {
			/* can't advance.  BACKTRACK */
			state= BACKTRACK; goto next;
		    }
		    /* All's OK - go ahead and advance the block */
		    rpos[b]--;
		    if (cov[b] == -1 && !may_be(cell[j], BGCOLOR))
			cov[b]= j;
		    j--;
		    if (VL) printf("L: ADVANCING BLOCK (cov=%d)\n",cov[b]);
		}
	    }

	    /* At this point, we have successfully placed the block */

	    /* If we are advancing a block after backtracking, it needs to
	     * cover something.
	     */
	    if (backtracking && cov[b] == -1)
	    {
		if (VL)
		    printf("L: BACKTRACK BLOCK COVERS NOTHING\n",cov[b]);
		backtracking= 0;
		state= ADVANCEBLOCK; goto next;
	    }

	    if (j < 0 && b > 0)
	    {
		/* Ran out of space, but still have blocks left */
		return NULL;
	    }

	    /* Successfully placed block b - Go on to next block */
	    b--;
	    backtracking= 0;
	    state= NEWBLOCK; goto next;

	case CHECKREST:

	    /* Precondition: All blocks have been legally placed.  j points
	     *   to first empty space before last block.  b is the index of
	     *   the last block, or -1 if there are no blocks.
	     * Action:  Check that there are no cells before the last block
	     *   which cannot be left background color.
	     */

	    if (j >= 0)
	    {
		if (VL) printf("L: PLACED LAST BLOCK - CHECK REST OF LINE\n");

		for (; j >= 0; j--)
		{
		    if (VL) printf("L: CELL %d ",j);
		    if (!may_be(cell[j], BGCOLOR))
		    {
			/* Check if we can cover the uncovered square by sliding
			 * the last block right far enough to cover it
			 */
			if (VL)
			    printf("NEEDS COVERAGE (cov=%d j=%d) ",cov[b],j);
			j= rpos[b] - clue->length[b];
			state= ADVANCEBLOCK; goto next;
		    }
		    else if (VL)
			printf("OK\n");
		}
	    }

	    state= HALT; goto next;

	case BACKTRACK:

	    /* Precondition: Blocks maxblock through b+1 have been legally
	     *   placed, but block b cannot be.
	     * Action:  Try to advance block b+1 so that it covers something
	     *   new that can't be background color, without uncovering
	     *   anything that it previously covered.
	     */

	    if (++b > maxblock)
	    {
		if (VL) printf("L: NO BLOCKS LEFT TO BACKTRACK TO - FAIL\n");
		return NULL;
	    }

	    j= rpos[b] - clue->length[b];/* First cell before start of block */
	    currcolor= clue->color[b];	/* Color of current block */

	    if (VL) printf("L: BACKTRACKING: ");

	    state= ADVANCEBLOCK; goto next;

	case ADVANCEBLOCK:

	    /* Precondition: Blocks maxblock through b+1 have been legally
	     *   placed.  j points to the first cell before block b.
	     *   currcolor is the color of block b.
	     * Action:  Try to advance block b so that it covers at least one
	     *   new cell that can't be background color, without uncovering
	     *   anything that block b previously covered.
	     */

	    if (VL) printf("L: ADVANCE BLOCK %d (j=%d,cc=%d,pos=%d,cov=%d)\n",
		    b,j,currcolor,rpos[b],cov[b]);

	    while(cov[b] < 0 || rpos[b] > cov[b])
	    {
		if (!may_be(cell[j], currcolor))
		{
		    if (VL) printf("L: ADVANCE HIT OBSTACLE ");
		    if (cov[b] > 0 || (multicolor && !may_be(cell[j], BGCOLOR)))
		    {
			if (VL) printf("- BACKTRACKING\n");
			state= BACKTRACK; goto next;
		    }
		    else
		    {
			/* Hit something we can't advance over, but we aren't
			 * covering anything either, so jump past the obstacle.
			 */
			rpos[b]= j-1;
			if (VL) printf("- JUMPING pos=%d\n",rpos[b]);
			backtracking= 1;
			state= PLACEBLOCK; goto next;
		    }
		}

		/* Can advance.  Do it */
		rpos[b]--;
		if (VL) printf("- ADVANCING BLOCK %d TO %d\n",b,rpos[b]);

		/* Check if we have covered anything.  If we have, we can
		 * stop advancing
		 */
		if (!may_be(cell[j--], BGCOLOR))
		{
		    if (cov[b] == -1) cov[b]= j+1;
		    if (VL)
		    	printf("L: COVERED NEW TARGET AT %d - BLOCK AT %d\n",
				j+1, rpos[b]);
		    state= FINALSPACE; goto next;
		}
		if (j < 0)
		{
		    if (VL) printf("L: END OF LINE\n");
		    return NULL;
		}
	    }

	    /* If we drop out here, then we've advanced the block as far
	     * as we can, but we haven't succeeded in covering anything new.
	     * So backtrack further.
	     */
	    if (VL) printf("L: CAN'T ADVANCE\n");
	    state= BACKTRACK;
	    goto next;
	}
	next:;
    }

    if (VL) printf("L: DONE\n");
    return rpos;
}


/* LRO_SOLVE - Solve a line using the left-right overlap algorithm.
 *   (1) Find left-most solution.
 *   (2) Find right-most solution.
 *   (3) Paint any square that is in the same interval in both solutions.
 *
 * This returns a pointer to an array of bitstrings.  The calling program
 * should NOT free this array when it is done.  Returns NULL if there is no
 * solution.  If ncell is passed in less than or equal to zero, it is
 * computed.
 */

bit_type *lro_solve(Puzzle *puz, Solution *sol, int k, int i, int ncell)
{
    Clue *clue= &puz->clue[k][i];
    int multicolor= (puz->ncolor > 2);
    int nblock= clue->n;
    int *left, *right;
    int j, c;
    int lb, rb;		/* Index of a block in left[] or right[] */
    int lgap,rgap;	/* If true, we are in gap before the indexed block */


    /* Count the number of cells in the line */
    if (ncell <= 0) ncell= count_cells(puz, sol, k, i);

    if (VL) printf("-----------------LEFT------------------\n");
    left= left_solve(puz, sol, k, i);
    if (left == NULL) return NULL;

    if (VL) printf("-----------------RIGHT-----------------\n");
    right= right_solve(puz, sol, k, i, ncell);
    if (right == NULL)
    	fail("Left solution but no right solution for line %d direction %d\n",
		i, k);

    if (VL)
    {
	int i;

	printf("L: LEFT SOLUTION:\n");
	for (i= 0; left[i] >= 0; i++)
	    printf("Block %d at %d\n",i, left[i]);
	printf("L: RIGHT SOLUTION:\n");
	for (i= 0; right[i] >= 0; i++)
	       printf("Block %d at %d\n",i, right[i]);
    }

    /* col is the array of bitstrings used to return values.  The i-th bit
     * string starts at col[puz->colsize*i] and bits will be set to 1 for
     * each color that cell i could be.
     */
    memset(col, 0, ncell * puz->colsize * sizeof(bit_type));

    /* nbcolor is the number of blocks of each color we could be in */
    if (multicolor)
	memset(nbcolor, 0, puz->ncolor *  sizeof(int));

    lb= rb= 0;
    lgap= rgap= 1;
    for (j= 0; j < ncell; j++)
    {
	if (!lgap && left[lb]+clue->length[lb] == j)
	{
	    lgap= 1;
	    lb++;
	}
        if (lgap && lb < nblock && left[lb] == j)
	{
	    lgap= 0;
	    if (multicolor) nbcolor[clue->color[lb]]++;
	}

	if (!rgap && right[rb]+1 == j)
	{
	    if (multicolor) nbcolor[clue->color[rb]]--;
	    rgap= 1;
	    rb++;
	}
        if (rgap && rb < nblock && right[rb] - clue->length[rb] + 1 == j)
	{
	    rgap= 0;
	}

	if (lgap == rgap && lb == rb)
	{
	    /* If we are in the same interval both directions, then the
	     * cell MUST be the color of that interval.
	     */
	    bit_set(colbit(j),lgap ? BGCOLOR : clue->color[lb]);
	}
	else if (multicolor)
	{
	    bit_set(colbit(j),BGCOLOR);
	    for (c= 0; c < puz->ncolor; c++)
	    	if (nbcolor[c] > 0)
		    bit_set(colbit(j),c);
	}
	else
	{
	    /* Two color puzzles */
	    bit_setall(colbit(j),2);
	}
    }

    if (VL)
    {
	printf("L: SOLUTION TO LINE %d DIRECTION %d\n",i,k);
	dump_lro_solve(puz, sol, k, i, col);
    }

    return col;
}


/* Run the Left/Right Overlap algorithm on a line of the puzzle.  Update the
 * line to show the result, and create new jobs for crossing lines for changed
 * cells.  Returns 1 on success, 0 if there is a contradiction in the solution.
 */

int apply_lro(Puzzle *puz, Solution *sol, int k, int i, int depth)
{
    bit_type *col;
    int ncell= count_cells(puz, sol, k, i);
    Cell **cell= sol->line[k][i];
    int j, z, c, d;
    bit_type new, *old;

    if (VC && depth > 0)
    	printf("C: SOLVING %s %d at DEPTH %d\n",
	    cluename(puz->type,k),i,depth-1);

    col= lro_solve(puz, sol, k, i, ncell);

    if (col == NULL) return 0;

    if (VL) printf("L: UPDATING GRID\n");

    for (j= 0; j < ncell; j++)
    {
	/* Is the new value different from the old value? */
	for (z= 0; z < puz->colsize; z++)
	{
	    oldval[z]= cell[j]->bit[z];
	    new= (colbit(j)[z] & cell[j]->bit[z]);
	    if (cell[j]->bit[z] != new)
	    {
		/* Do probe merging (maybe) */
		merge_set(puz, cell[j], colbit(j));

		if (VS)
		{
		    if (VL)
			printf("L: CELL %d - BYTE %d - CHANGED FROM (",j,z);
		    else
			printf("S: CELL %d CHANGED FROM (", j);
		    dump_bits(stdout, puz, cell[j]->bit);
		}

		/* Save old value to history (maybe) */
		add_hist(puz, cell[j], 0);

		/* Copy new values into grid */
		cell[j]->bit[z] = new;
		for (z++; z < puz->colsize; z++)
		{
		    oldval[z]= cell[j]->bit[z];
		    cell[j]->bit[z]&= colbit(j)[z];
		}

		if (VS)
		{
		    printf(") TO (");
		    dump_bits(stdout, puz, cell[j]->bit);
		    printf(")\n");
		}

		if (VL) dump_history(stdout, puz, 0);

		if (puz->ncolor <= 2)
		    cell[j]->n= 1;
		else
		    count_cell(puz,cell[j]);

		if (cell[j]->n == 1) puz->nsolved++;

		/* Put other directions that use this cell on the job list */
		add_jobs_edgebonus(puz, sol, k, cell[j], depth, oldval);
		break;
	    }
	    else
		if (VL) printf("L: CELL %d - BYTE %d\n",j,z);
	}
    }

    return 1;
}
