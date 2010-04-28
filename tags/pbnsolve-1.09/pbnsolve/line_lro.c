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

#define colbit(i) (col+(fbit_size*(i)))

#ifdef LINEWATCH
#define WL (clue->watch)
#define DW(k,i) (puz->clue[k][i].watch)
#else
#define WL 0
#define DW(k,i) 0
#endif

#define D (WL || VL)
#define DU (WL || (VL && VU))

/* Allocate some arrays to be used in left_solve(), right_solve(), and
 * lro_solve() to a size appropriate for puzzle puz.  Also creates the
 * saved position arrays that go in the puz->clue data structure.
 */

static line_t *lpos, *rpos, *gcov;
static line_t *nbcolor;
static bit_type *col;
static int multicolor;
bit_type *oldval;

void init_line(Puzzle *puz)
{
    line_t maxnclue= 0, maxdimension= 0;
    line_t i;
    dir_t k;

    /* Set a flag if the puzzle is multicolored.  If not, we can skip some
     * tests which will never be true and be just a bit more efficient.
     */
    multicolor= (puz->ncolor > 2);

    /* Find maximum number of numbers in any clue in any direction and
     * maximum length of a line
     */
    for (k= 0; k < puz->nset; k++)
    {
	if (puz->n[k] > maxdimension) maxdimension= puz->n[k];

    	for (i= 0; i < puz->n[k]; i++)
	{
	    if (puz->clue[k][i].n > maxnclue) maxnclue= puz->clue[k][i].n;

	    /* Allocate a left and right saved position array for each clue.
	     * Note that these are -1 terminated, so they have 1 added to
	     * their size.  */
	    puz->clue[k][i].lpos=
		(line_t *)malloc((puz->clue[k][i].n + 1) * sizeof(line_t));
	    puz->clue[k][i].lpos[puz->clue[k][i].n]= -1;
	    puz->clue[k][i].rpos=
		(line_t *)malloc((puz->clue[k][i].n + 1) * sizeof(line_t));
	    puz->clue[k][i].rpos[puz->clue[k][i].n]= -1;

	    /* Allocate a left and right coverage array for each clue */
	    puz->clue[k][i].lcov=
		(line_t *)malloc(puz->clue[k][i].n * sizeof(line_t));
	    puz->clue[k][i].rcov=
		(line_t *)malloc(puz->clue[k][i].n * sizeof(line_t));

	    /* Setting lbadb and rbadb to -1 means no saved solutions yet.
	     * Setting them to MAXINT means a valid solution.
	     */
	    puz->clue[k][i].lbadb= -1;
	    puz->clue[k][i].rbadb= -1;
	    puz->clue[k][i].lbadi= MAXLINE;
	    puz->clue[k][i].rbadi= -1;
	    puz->clue[k][i].lstamp= MAXLINE;
	    puz->clue[k][i].rstamp= MAXLINE;
	}
    }

    /* Allocate storage spaces for left_solve and right_solve arrays.  We
     * use these instead of the ones in the Clue structure if we don't want
     * to save the results of the solution.
     */
    lpos= (line_t *)malloc((maxnclue + 1) * sizeof(line_t));
    rpos= (line_t *)malloc((maxnclue + 1) * sizeof(line_t));
    gcov= (line_t *)malloc(maxnclue * sizeof(int));

    /* An extra color bit map for apply_lro */
    oldval= (bit_type*)malloc(fbit_size * sizeof(bit_type));

    col= (bit_type *)malloc(maxdimension * fbit_size * sizeof(bit_type));
    if (puz->ncolor > 2)
	nbcolor= (line_t *)malloc(puz->ncolor * sizeof(line_t));
}


void dump_lro_solve(Puzzle *puz, dir_t k, line_t i, bit_type *col)
{
    line_t ncell= puz->clue[k][i].linelen;
    line_t j;

    for (j= 0; j < ncell; j++)
    {
        printf("%-2d ",j);
	dump_bits(stdout, puz, colbit(j));
	putchar('\n');
    }
}


/* A cell i of the line for the given clue has been changed to the given new
 * bit string value.  Check to what degree the stored left solution for that
 * line has been invalidated.  Returns 0 if old solution is still OK, 1 if
 * this cell change, or a previous cell change, invalidated it.  If it's newly
 * invalidated, store some information on how much is invalidated.
 */

int left_check(Clue *clue, line_t i, bit_type *bit)
{
    line_t b;

    /* If there is no saved solution, don't check it */
    if (clue->lbadb == -1) return 1;

    /* If the line is already marked bad because of the same cell or a cell
     * to the left of this cell, then we don't need to do more checking.
     */
    if (clue->lbadi <= i) return 1;

    /* Find the interval containing the changed cell */
    for (b= clue->n - 1; b >= 0; b--)
    	if (i >= clue->lpos[b])
	{
	    if (i < clue->lpos[b] + clue->length[b])
	    {
	    	/* changed cell is inside block b */
		if (bit_test(bit, clue->color[b]))
		{
		    /* If we've set the cell non-white and it is left of
		     * anything the block was previously covering, update cov */
		    if ((clue->lcov[b] == -1 || i < clue->lcov[b]) &&
		        (!multicolor || !bit_bg(bit)))
			    clue->lcov[b]= i;
		    return clue->lbadb != MAXLINE;
		}
		clue->lbadb= 2*b + 1;
		if (D)
		    printf("L:  OLD LEFT SOLN BAD - BLOCK %d INT %d\n",
			b, clue->lbadb);
	    }
	    else
	    {
	    	/* changed cell is between block b and the next block */
		if (bit_bg(bit))
		    return clue->lbadb != MAXLINE;
		clue->lbadb= 2*b + 2;
		if (D)
		    printf("L:  OLD LEFT SOLN BAD - GAP %d INT %d\n",
			b, clue->lbadb);
	    }
	    clue->lbadi= i;
	    return 1;
	}
    /* If we drop out of loop, then we invalidated a cell left of the
     * leftmost block.  This should never really happen, as all those cells
     * should have been painted with the background color long ago.  However,
     * if it happens anyway, invalidate the whole line.
     */
    clue->lbadb= -1;
    clue->lbadi= i;
    if (D)
	printf("L:  OLD LEFT SOLN BAD BEFORE FIRST BLOCK\n");
    return  1;
}


int right_check(Clue *clue, line_t i, bit_type *bit)
{
    line_t b;

    /* If there is no saved solution, don't check it */
    if (clue->rbadb == -1) return 1;

    /* If the line is already marked bad because of the same cell or a cell
     * to the right of this cell, then we don't need to do more checking.
     */
    if (clue->rbadi >= i) return 1;

    /* Find the interval containing the changed cell */
    for (b= 0; b < clue->n; b++)
    	if (i <= clue->rpos[b])
	{
	    if (i > clue->rpos[b] - clue->length[b])
	    {
	    	/* changed cell is inside block b */
		if (bit_test(bit, clue->color[b]))
		{
		    /* If we've set the cell non-white and it is right of
		     * anything the block was previously covering, update cov */
		    if (i > clue->rcov[b] &&
		        (!multicolor || !bit_bg(bit)))
			    clue->rcov[b]= i;
		    return clue->rbadb != MAXLINE;
		}
		clue->rbadb= 2*b + 1;
		if (D)
		    printf("L:  OLD RIGHT SOLN BAD - BLOCK %d INT %d\n",
			b, clue->rbadb);
	    }
	    else
	    {
	    	/* changed cell is in gap left of block b */
		if (bit_bg(bit))
		    return clue->rbadb != MAXLINE;
		clue->rbadb= 2*b;
		if (D)
		    printf("L:  OLD RIGHT SOLN BAD - GAP %d INT %d\n",
			b, clue->rbadb);
	    }
	    clue->rbadi= i;
	    return 1;
	}
    /* If we drop out of loop, then we invalidated a cell right of the
     * rightmost block.  This should never really happen, as all those cells
     * should have been painted with the background color long ago.  However,
     * if it happens anyway, invalidate the whole line.
     */
    clue->rbadb= -1;
    clue->rbadi= i;
    if (D)
	printf("L:  OLD RIGHT SOLN BAD BEFORE FIRST BLOCK\n");
    return  1;
}


/* Update the solution and coverage arrays after undoing a cell.  We are
 * given the clue, the solution line, the index of the cell in the line, the
 * new value of the cell.
 */

void left_undo(Puzzle *puz, Clue *clue, Cell **line, line_t i, bit_type *new)
{
    line_t b, e;

    /* If we are backtracking past the point where this solution was first
     * found, then discard it entirely.  */
    if (puz->nhist <= clue->lstamp)
    {
    	clue->lbadb= -1;
	if (DU)
	    printf(" -- LEFT INVALIDATED");
	return;
    }

    /* If there is no saved solution, don't check it */
    if (clue->lbadb == -1) return;

    /* It is possible that the line is marked bad at this point, if a failure
     * in some other line caused us to start backtracking before this line
     * could be rechecked.  But back at the last guess point it must have been
     * OK, because there were no jobs on the list.  So since we are going to
     * backtrack until the last guess point, it's safe to mark the line OK.
     */
    clue->lbadb= MAXLINE;
    clue->lbadi= MAXLINE;

    /* If this cell is the leftmost can't-be-white cell in some block and
     * we are undoing it so it can-be-white, then lcov for that block must
     * change.  Note that this is the only case where lcov needs to be
     * undated.  If the cell is in a gap it doesn't effect lcov.  If it is
     * in block b and left of lcov[b], then it already can-be-white and
     * undoing won't change that. If it is block b and right of lcov[b], then
     * lcov[b] will remain the leftmost can't-be-white cell in the block
     * whatever we do to this cell.
     */

    /* Look for an interval with lcov[b] == our cell */
    for (b= 0;  b < clue->n; b++)
    	if (i == clue->lcov[b])
	{
	    /* We were the leftmost can't-be-white cell of block b */

	    /* If we still can't be white, no change is needed */
	    if (multicolor && !bit_bg(new)) return;

	    /* Scan right from the current position through the block,
	     * looking for another can't-be-white cell */
	    e= clue->lpos[b] + clue->length[b];
	    for (i++; i < e; i++)
	    	if (!may_be_bg(line[i]))
		{
		    /* Found one */
		    clue->lcov[b]= i;
		    if (DU)
			printf(" -- LCOV[%d] SET TO %d",b,i);
		    return;
		}
	    /* Found none */
	    clue->lcov[b]= -1;
	    if (DU)
		printf(" -- LCOV[%d] SET TO -1",b);
	}

    /* Cell is not the lcov of any block */
    return;
}

void right_undo(Puzzle *puz, Clue *clue, Cell **line, line_t i, bit_type *new)
{
    line_t b, e;

    /* If we are backtracking past the point where this solution was first
     * found, then discard it entirely.  */
    if (puz->nhist <= clue->rstamp)
    {
    	clue->rbadb= -1;
	if (DU)
	    printf(" -- RIGHT INVALIDATED");
	return;
    }

    /* If there is no saved solution, don't check it */
    if (clue->rbadb == -1) return;

    /* It is possible that the line is marked bad at this point, if a failure
     * in some other line caused us to start backtracking before this line
     * could be rechecked.  But back at the last guess point it must have been
     * OK, because there were no jobs on the list.  So since we are going to
     * backtrack until the last guess point, it's safe to mark the line OK.
     */
    clue->rbadb= MAXLINE;
    clue->rbadi= -1;

    /* If this cell is the rightmost can't-be-white cell in some block and
     * we are undoing it so it can-be-white, then rcov for that block must
     * change.  Note that this is the only case where rcov needs to be
     * undated.  If the cell is in a gap it doesn't effect rcov.  If it is
     * in block b and right of rcov[b], then it already can-be-white and
     * undoing won't change that. If it is block b and left of rcov[b], then
     * rcov[b] will remain the rightmost can't-be-white cell in the block
     * whatever we do to this cell.
     */

    /* Look for an interval with rcov[b] == our cell */
    for (b= 0;  b < clue->n; b++)
    	if (i == clue->rcov[b])
	{
	    /* We were the rightmost can't-be-white cell of block b */

	    /* If we still can't be white, no change is needed */
	    if (multicolor && !bit_bg(new)) return;

	    /* Scan left from the current position through the block,
	     * looking for another can't-be-white cell */
	    e= clue->rpos[b];
	    for (i--; i >= e; i--)
	    	if (!may_be_bg(line[i]))
		{
		    /* Found one */
		    clue->rcov[b]= i;
		    if (DU)
			printf(" -- RCOV[%d] SET TO %d",b,i);
		    return;
		}
	    /* Found none */
	    clue->rcov[b]= -1;
	    if (DU)
		printf(" -- RCOV[%d] SET TO -1",b);
	}

    /* Cell is not the rcov of any block */
    return;
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

line_t *left_solve(Puzzle *puz, Solution *sol, dir_t k, line_t i, int savepos)
{
    line_t b,j;
    color_t currcolor, nextcolor;
    int backtracking, state;
    Clue *clue= &puz->clue[k][i];
    Cell **cell= sol->line[k][i];
    line_t *pos, *cov;

    /* The pos array contains current position of each block, or more
     * specifically the first cell of each block.  It's terminated by a -1.
     */
    if (savepos)
	pos= clue->lpos;
    else
    {
    	pos= lpos;
	pos[clue->n]= -1;
    }

    /* The cov array contains the index of the left-most cell covered by the
     * block which CANNOT be white.  It is -1 if there is no such cell.
     */
    cov= savepos ? clue->lcov : gcov;

    /* If we have a saved solution to start with, initialize off that.
     * Otherwise, just start from scratch.
     */
    
    if (!savepos || clue->lbadb == -1)
    {
	/* no usable saved solution.  Start fresh. */
	b= 0;
	state= NEWBLOCK;
	if (D)
	    printf("L: NO OLD -- FRESH START\n");
    }
    else if (clue->lbadb == MAXLINE)
    {
    	/* Can reuse old solution completely. Note that we don't update the
	 * time stamp in this case.
	 */
	if (D)
	{
	    printf("L: OLD STILL VALID -- REUSING: ");
	    dump_pos(stdout, pos, clue->length);
	}
	return pos;
    }
    else if (clue->lbadb % 2 == 0)
    {
	/* A cell in a gap was set to a non-background color.  Point b to
	 * the preceding block, and start backtracking.  */
	b= clue->lbadb/2;
	state= BACKTRACK;
	if (D)
	{
	    printf("L: OLD INVALID AT %d INT %d (GAP) "
		"-- BACKTRACK FROM BLOCK %d\n   ", clue->lbadi,clue->lbadb,b);
	    dump_pos(stdout, pos, clue->length);
        }
    }
    else
    {
    	/* A cell in a block has been set to different color. */
	b= clue->lbadb/2;
	if (D)
	    printf("L: OLD INVALID AT %d INT %d (BLOCK) -- ",
		clue->lbadi,clue->lbadb);

	if ((cov[b] < 0 || cov[b] > clue->lbadi) &&
		(!multicolor || may_be_bg(cell[clue->lbadi])))
	{
	    /* Block covers nothing or covered cells are right of changed cell,
	     * and changed cell can be background color, so we just skip past
	     * it */
	    pos[b]= clue->lbadi + 1;
	    if (cell[pos[b]] == NULL)
	    {
		clue->lbadb= -1;
		return NULL;
	    }
	    currcolor= clue->color[b];
	    state= PLACEBLOCK;
	    if (D)
		printf("PLACEBLOCK %d AT %d\n   ",b,pos[b]);
	}
	else
	{
	    /* Block covers something, so we can't skip it ahead */
	    state= BACKTRACK;
	    if (D)
		printf("BACKTRACK FROM BLOCK %d\n   ",b);
	}
	if (D)
	    dump_pos(stdout, pos, clue->length);
    }

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
	    if (D)
		printf("L: PLACING BLOCK %d COLOR %d LENGTH %d\n",
	    	b,currcolor,clue->length[b]);

	    /* Earliest possible position of block b, after previous blocks,
	     * leaving a space between the blocks if they are the same color.
	     */

	    pos[b]= (b == 0) ? 0 :
			j + ((clue->color[b-1] == currcolor) ? 1 : 0);

	    if (cell[pos[b]] == NULL)
	    {
		clue->lbadb= -1;
		return NULL;
	    }

	    if (D)
		printf("L: FIRST POS %d\n",pos[b]);

	    state= PLACEBLOCK; goto next;

	case PLACEBLOCK:

	    /* Precondition: Blocks 0 through b-1 have been legally placed
	     *    or b = 0.  pos[b] points points to a position less than or
	     *    equal to the position of block b.  currcolor is the color
	     *    of the current block b.
	     * Action:  Advance pos[b] until it points to a position where
	     *    the next length[b] cells can all accomodate the block's
	     *    color.  Also compute cov[b], the index of the leftmost
	     *    cell covered by the block that cannot be background.
	     */

	    /* First find a place for the first cell of the block, skipping
	     * over white cells until we find one.
	     */

	    while (!may_be(cell[pos[b]], currcolor))
	    {
		if (D)
		    printf("L: POS %d BLOCKED\n",pos[b]);

		if (multicolor && !may_be_bg(cell[pos[b]]))
		{
		    /* We hit a cell that must be some color other
		     * than the color of the current block.  Only hope is to
		     * find a previously placed block that can be advanced to
		     * cover it, so we backtrack.
		     */
		    if (D)
			printf("L: BACKTRACKING ON WRONG COLOR\n",pos[b]);
		    j= pos[b];
		    while (b > 1 && !may_be(cell[j], clue->color[b-1]))
			b--;
		    state= BACKTRACK; goto next;
		}

		if (D)
		    printf("L: SHIFT BLOCK TO %d\n",pos[b]+1);

		if (cell[++pos[b]] == NULL)
		{
		    if (D)
			printf("L: END OF THE LINE\n");
		    clue->lbadb= -1;
		    return NULL;	/* Hit end of line */
		}
	    }

	    /* First cell has found a home, can we place the rest of the block?
	     * While we are at at, compute cov[b].
	     */
	    if (D)
		printf("L: FIRST CELL OF %d PLACED AT %d\n",b,pos[b]);
	    j= pos[b];
	    cov[b]= (may_be_bg(cell[j]) ? -1 : j);
	    for (j++; j - pos[b] < clue->length[b]; j++)
	    {
		if (D)
		    printf("L: CHECKING CELL AT %d\n",j);
		if (cell[j] == NULL)
		{
		    if (D)
			printf("L: END OF THE LINE\n");
		    clue->lbadb= -1;
		    return NULL;	/* Block runs off end of line */
		}

		if (!may_be(cell[j], currcolor))
		{
		    if (D)
			printf("L: FAILED AT %d\n",j);
		    if (cov[b] == -1)
		    {
			/* block doesn't fit here, but it wasn't
			 * covering anything, so just keep shifting it
			 */
			if (D)
			    printf("L: SKIP AHEAD\n");
			pos[b]= j;
			state= PLACEBLOCK; goto next;
		    }
		    else
		    {
			/* Block b cannot be placed.  Need to try advancing
			 * block b-1.
			 */
			if (D)
			    printf("L: BACKTRACK\n");
			state= BACKTRACK; goto next;
		    }
		}

		/* Update cov[b] */
		if (cov[b] == -1 && !may_be_bg(cell[j]))
		    cov[b]= j;

		if (D)
		    printf("L: OK AT %d (cov=%d)\n",j, cov[b]);
	    }

	    state= FINALSPACE; goto next;

	case FINALSPACE:

	    /* Precondition: Blocks 0 through b-1 have been legally placed
	     *    or b = 0.  pos[b] points points to a position so that all
	     *    cells of the block can accomodate the block's color.  j
	     *    is the index of the first cell after the block.  currcolor
	     *    is the color of the current block b.
	     * Action:  Check if the first cell of the block is a legal
	     *    color.  If it is the same color as the block, try advancing
	     *    the block to cover that too.  If backtracking is true,
	     *    also ensure the block covers something, and advance it until
	     *	  it does.
	     */

	    if (D)
		printf("L: CHECKING FINAL SPACE\n");

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

		if (D)
		    printf("L:  NEXTCOLOR=%d\n",nextcolor);

		while (cell[j] != NULL && !may_be_bg(cell[j]) &&
			(nextcolor == BGCOLOR || !may_be(cell[j],nextcolor)))
		{
		    /* Next cell is wrong color, so try advancing the block one
		     * cell.  But we need to make sure that we don't uncover any
		     * cells, and we need to make sure that the cell at the end
		     * is one we can cover
		     */
		    if (D)
			printf("L: NO FINAL SPACE\n");
		    if (cov[b] == pos[b] ||
			(multicolor && !may_be(cell[j],currcolor)) )
		    {
			/* can't advance.  BACKTRACK */
			state= BACKTRACK; goto next;
		    }
		    /* All's OK - go ahead and advance the block */
		    pos[b]++;
		    if (cov[b] == -1 && !may_be_bg(cell[j]))
			cov[b]= j;
		    j++;
		    if (D)
			printf("L: ADVANCING BLOCK (cov=%d)\n",cov[b]);
		}
	    }

	    /* At this point, we have successfully placed the block */

	    /* If we are advancing a block after backtracking, it needs to
	     * cover something.
	     */
	    if (backtracking && cov[b] == -1)
	    {
		if (D)
		    printf("L: BACKTRACK BLOCK COVERS NOTHING\n",cov[b]);
		backtracking= 0;
		state= ADVANCEBLOCK; goto next;
	    }

	    if (cell[j] == NULL && b < clue->n - 1)
	    {
		/* Ran out of space, but still have blocks left */
		clue->lbadb= -1;
		return NULL;
	    }

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
		if (D)
		    printf("L: PLACED LAST BLOCK - CHECK REST OF LINE\n");

		for (; cell[j] != NULL; j++)
		{
		    if (D)
			printf("L: CELL %d ",j);
		    if (!may_be_bg(cell[j]))
		    {
			/* Check if we can cover the uncovered square by sliding
			 * the last block right far enough to cover it
			 */
			if (D)
			    printf("NEEDS COVERAGE\n");
			j= pos[b] + clue->length[b];
			state= ADVANCEBLOCK; goto next;
		    }
		    else if (D)
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
		if (D)
		    printf("L: NO BLOCKS LEFT TO BACKTRACK TO - FAIL\n");
		clue->lbadb= -1;
		return NULL;
	    }

	    j= pos[b] + clue->length[b];/* First cell after end of block */
	    currcolor= clue->color[b];	/* Color of current block */

	    if (D)
		printf("L: BACKTRACKING: ");

	    state= ADVANCEBLOCK; goto next;

	case ADVANCEBLOCK:

	    /* Precondition: Blocks 0 through b have been legally placed.
	     *   j points to the first cell after block b.  currcolor is the
	     *   color of block b.
	     * Action:  Try to advance block b so that it covers at least one
	     *   new cell that can't be background color, without uncovering
	     *   anything that block b previously covered.
	     */

	    if (D)
		printf("L: ADVANCE BLOCK %d (j=%d,cc=%d,pos=%d,cov=%d)\n",
		    b,j,currcolor,pos[b],cov[b]);

	    while(cov[b] < 0 || pos[b] < cov[b])
	    {
		if (!may_be(cell[j], currcolor))
		{
		    if (D)
			printf("L: ADVANCE HIT OBSTACLE ");
		    if (cov[b] > 0 || (multicolor && !may_be_bg(cell[j])))
		    {
			if (D)
			    printf("- BACKTRACKING\n");
			state= BACKTRACK; goto next;
		    }
		    else
		    {
			/* Hit something we can't advance over, but we aren't
			 * covering anything either, so jump past the obstacle.
			 */
			pos[b]= j+1;
			if (D)
			    printf("- JUMPING pos=%d\n",pos[b]);
			backtracking= 1;
			state= PLACEBLOCK; goto next;
		    }
		}

		/* Can advance.  Do it */
		pos[b]++;
		if (D)
		    printf("- ADVANCING BLOCK %d TO %d\n",b,pos[b]);

		/* Check if we have covered anything.  If we have, we can
		 * stop advancing
		 */
		if (!may_be_bg(cell[j++]))
		{
		    if (cov[b] == -1) cov[b]= j-1;
		    if (D)
			printf("L: COVERED NEW TARGET AT %d - BLOCK AT %d\n",
			    j-1, pos[b]);
		    state= FINALSPACE; goto next;
		}
		if (cell[j] == NULL)
		{
		    if (D)
			printf("L: END OF LINE\n");
		    clue->lbadb= -1;
		    return NULL;
		}
	    }

	    /* If we drop out here, then we've advanced the block as far
	     * as we can, but we haven't succeeded in covering anything new.
	     * So backtrack further.
	     */
	    if (D)
		printf("L: CAN'T ADVANCE\n");
	    state= BACKTRACK;
	    goto next;
	}
	next:;
    }

    if (D)
	printf("L: DONE\n");

    if (savepos)
    {
    	clue->lstamp= puz->nhist;
	clue->lbadi= MAXLINE;
	clue->lbadb= MAXLINE;
	if (DU)
	    printf("L: SAVING AT %d\n",clue->lstamp);
    }

    return pos;
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

line_t *right_solve(Puzzle *puz, Solution *sol, dir_t k, line_t i, int savepos)
{
    line_t b,j;
    color_t currcolor, nextcolor;
    int backtracking, state;
    Clue *clue= &puz->clue[k][i];
    line_t ncell= clue->linelen;
    Cell **cell= sol->line[k][i];
    line_t maxblock= clue->n - 1;
    line_t *pos, *cov;

    /* The pos array contains current position of each block, or more
     * specifically the rightmost cell of each block.  It's terminated by a -1.
     */
    if (savepos)
    	pos= clue->rpos;
    else
    {
    	pos= rpos;
	pos[clue->n]= -1;
    }

    /* The cov array contains the index of the right-most cell covered by the
     * block which CANNOT be white.  It is -1 if there is no such cell.
     */
    cov= savepos ? clue->rcov : gcov;

    if (!savepos || clue->rbadb == -1)
    {
    	/* no usable saved solution.  Start fresh. */
	b= maxblock;
	state= NEWBLOCK;
	if (D)
	    printf("L: NO OLD -- FRESH START\n");
    }
    else if (clue->rbadb == MAXLINE)
    {
    	/* Can reuse old solution completely.  Note that we don't update the
	 * time stamp in this case.  */
	if (D)
	{
	    printf("L: OLD STILL VALID -- REUSING: ");
	    dump_pos(stdout, pos, clue->length);
	}
	return pos;
    }
    else if (clue->rbadb % 2 == 0)
    {
    	/* A cell in a gap was set to a non-background color.  Point b to
	 * the preceding block, and start backtracking.  */
	b= clue->rbadb/2 - 1;
	state= BACKTRACK;
	if (D)
	{
	    printf("L: OLD INVALID AT %d INT %d (GAP) "
		"-- BACKTRACK FROM BLOCK %d\n   ", clue->rbadi,clue->rbadb,b);
	    dump_pos(stdout, pos, clue->length);
	}
    }
    else
    {
    	/* A cell in a block has been set to different color. */
	b= clue->rbadb/2;
	if (D)
	    printf("L: OLD INVALID AT %d INT %d (BLOCK) -- ",
		clue->rbadi,clue->rbadb);

	if (cov[b] < clue->rbadi &&
	    (!multicolor || may_be_bg(cell[clue->rbadi])))
	{
	    /* Block covers nothing, or covered cells are left of changed cell,
	     * and changed cell can be background color, so we just skip past
	     * it */
	    pos[b]= clue->rbadi - 1;
	    if (pos[b] < 0)
	    {
		clue->rbadb= -1;
		return NULL;
	    }
	    currcolor= clue->color[b];
	    state= PLACEBLOCK;
	    if (D)
		printf("PLACEBLOCK %d AT %d\n   ",b,pos[b]);
	}
	else
	{
	    /* Block covers something, so we can't skip it ahead */
	    state= BACKTRACK;
	    if (D)
		printf("BACKTRACK FROM BLOCK %d\n   ",b);
	}
	if (D)
	    dump_pos(stdout, pos, clue->length);
    }

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
	    if (D)
		printf("L: PLACING BLOCK %d COLOR %d LENGTH %d\n",
	    	b,currcolor,clue->length[b]);

	    /* Earliest possible position of block b, after previous blocks,
	     * leaving a space between the blocks if they are the same color.
	     */

	    pos[b]= (b == maxblock) ? ncell - 1 :
			j - ((clue->color[b+1] == currcolor) ? 1 : 0);

	    if (pos[b] - clue->length[b] + 1 < 0)
	    {
		clue->rbadb= -1;
		return NULL;
	    }

	    if (D)
		printf("L: FIRST POS %d\n",pos[b]);

	    state= PLACEBLOCK; goto next;

	case PLACEBLOCK:

	    /* Precondition: Blocks b+1 through maxblock have been legally
	     *   placed or b = maxblock.  pos[b] points points to a position
	     *   greater than or equal to the position of block b.  currcolor
	     *   is the color of the current block b.
	     * Action:  Advance pos[b] until it points to a position where
	     *   the next length[b] cells can all accomodate the block's
	     *   color.  Also compute cov[b], the index of the rightmost
	     *   cell covered by the block that cannot be background.
	     */

	    /* First find a place for the first cell of the block, skipping
	     * over white cells until we find one.
	     */

	    while (!may_be(cell[pos[b]], currcolor))
	    {
		if (D)
		    printf("L: POS %d BLOCKED\n",pos[b]);

		if (multicolor && !may_be_bg(cell[pos[b]]))
		{
		    /* We hit a cell that must be some color other
		     * than the color of the current block.  Only hope is to
		     * find a previously placed block that can be advanced to
		     * cover it, so we backtrack.
		     */
		    if (D)
			printf("L: BACKTRACKING ON WRONG COLOR\n",pos[b]);
		    j= pos[b];
		    while (b < maxblock-1 && !may_be(cell[j], clue->color[b+1]))
			b++;
		    state= BACKTRACK; goto next;
		}

		if (D)
		    printf("L: SHIFT BLOCK TO %d\n",pos[b]-1);

		if (--pos[b] < 0)
		{
		    if (D)
			printf("L: END OF THE LINE\n");
		    clue->rbadb= -1;
		    return NULL;	/* Hit end of line */
		}
	    }

	    /* First cell has found a home, can we place the rest of the block?
	     * While we are at at, compute cov[b].
	     */
	    if (D)
		printf("L: FIRST CELL OF %d PLACED AT %d\n",b,pos[b]);
	    j= pos[b];
	    cov[b]= (may_be_bg(cell[j]) ? -1 : j);
	    for (j--; pos[b] - j < clue->length[b]; j--)
	    {
		if (D)
		    printf("L: CHECKING CELL AT %d\n",j);
		if (j < 0)
		{
		    if (D)
			printf("L: END OF THE LINE\n");
		    clue->rbadb= -1;
		    return NULL;	/* Block runs off end of line */
		}

		if (!may_be(cell[j], currcolor))
		{
		    if (D)
			printf("L: FAILED AT %d\n",j);
		    if (cov[b] == -1)
		    {
			/* block doesn't fit here, but it wasn't
			 * covering anything, so just keep shifting it
			 */
			if (D)
			    printf("L: SKIP AHEAD\n");
			pos[b]= j;
			state= PLACEBLOCK; goto next;
		    }
		    else
		    {
			/* Block b cannot be placed.  Need to try advancing
			 * block b+1.
			 */
			if (D)
			    printf("L: BACKTRACK\n");
			state= BACKTRACK; goto next;
		    }
		}

		/* Update cov[b] */
		if (cov[b] == -1 && !may_be_bg(cell[j]))
		    cov[b]= j;

		if (D)
		    printf("L: OK AT %d (cov=%d)\n",j, cov[b]);
	    }

	    state= FINALSPACE; goto next;

	case FINALSPACE:

	    /* Precondition: Blocks maxblock through b+1 have been legally
	     *    placed or b = maxblock.  pos[b] points points to a position
	     *    so that all cells of the block can accomodate the block's
	     *    color.  j is the next of the first cell after the block.
	     *    currcolor is the color of the current block b.
	     * Action:  Check if the first cell of the block is a legal
	     *    color.  If it is the same color as the block, try advancing
	     *    the block to cover that too.  If backtracking is true,
	     *    also ensure the block covers something, and advance it until
	     *	  it does.
	     */

	    if (D)
		printf("L: CHECKING FINAL SPACE\n");

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

		if (D)
		    printf("L:  NEXTCOLOR=%d\n",nextcolor);

		while (j >= 0 && !may_be_bg(cell[j]) &&
			(nextcolor == BGCOLOR || !may_be(cell[j],nextcolor)))
		{
		    /* Next cell is wrong color, so try advancing the block one
		     * cell.  But we need to make sure that we don't uncover any
		     * cells, and we need to make sure that the cell at the end
		     * is one we can cover
		     */
		    if (D)
			printf("L: NO FINAL SPACE\n");
		    if (cov[b] == pos[b] ||
			(multicolor && !may_be(cell[j],currcolor)) )
		    {
			/* can't advance.  BACKTRACK */
			state= BACKTRACK; goto next;
		    }
		    /* All's OK - go ahead and advance the block */
		    pos[b]--;
		    if (cov[b] == -1 && !may_be_bg(cell[j]))
			cov[b]= j;
		    j--;
		    if (D)
			printf("L: ADVANCING BLOCK (cov=%d)\n",cov[b]);
		}
	    }

	    /* At this point, we have successfully placed the block */

	    /* If we are advancing a block after backtracking, it needs to
	     * cover something.
	     */
	    if (backtracking && cov[b] == -1)
	    {
		if (D)
		    printf("L: BACKTRACK BLOCK COVERS NOTHING\n",cov[b]);
		backtracking= 0;
		state= ADVANCEBLOCK; goto next;
	    }

	    if (j < 0 && b > 0)
	    {
		/* Ran out of space, but still have blocks left */
		clue->rbadb= -1;
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
		if (D)
		    printf("L: PLACED LAST BLOCK - CHECK REST OF LINE\n");

		for (; j >= 0; j--)
		{
		    if (D)
			printf("L: CELL %d ",j);
		    if (!may_be_bg(cell[j]))
		    {
			/* Check if we can cover the uncovered square by sliding
			 * the last block right far enough to cover it
			 */
			if (D)
			    printf("NEEDS COVERAGE (cov=%d j=%d) ",cov[b],j);
			j= pos[b] - clue->length[b];
			state= ADVANCEBLOCK; goto next;
		    }
		    else if (D)
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
		if (D)
		    printf("L: NO BLOCKS LEFT TO BACKTRACK TO - FAIL\n");
		clue->rbadb= -1;
		return NULL;
	    }

	    j= pos[b] - clue->length[b];/* First cell before start of block */
	    currcolor= clue->color[b];	/* Color of current block */

	    if (D)
		printf("L: BACKTRACKING: ");

	    state= ADVANCEBLOCK; goto next;

	case ADVANCEBLOCK:

	    /* Precondition: Blocks maxblock through b+1 have been legally
	     *   placed.  j points to the first cell before block b.
	     *   currcolor is the color of block b.
	     * Action:  Try to advance block b so that it covers at least one
	     *   new cell that can't be background color, without uncovering
	     *   anything that block b previously covered.
	     */

	    if (D)
		printf("L: ADVANCE BLOCK %d (j=%d,cc=%d,pos=%d,cov=%d)\n",
		    b,j,currcolor,pos[b],cov[b]);

	    while(cov[b] < 0 || pos[b] > cov[b])
	    {
		if (!may_be(cell[j], currcolor))
		{
		    if (D)
			printf("L: ADVANCE HIT OBSTACLE ");
		    if (cov[b] > 0 || (multicolor && !may_be_bg(cell[j])))
		    {
			if (D)
			    printf("- BACKTRACKING\n");
			state= BACKTRACK; goto next;
		    }
		    else
		    {
			/* Hit something we can't advance over, but we aren't
			 * covering anything either, so jump past the obstacle.
			 */
			pos[b]= j-1;
			if (D)
			    printf("- JUMPING pos=%d\n",pos[b]);
			backtracking= 1;
			state= PLACEBLOCK; goto next;
		    }
		}

		/* Can advance.  Do it */
		pos[b]--;
		if (D)
		    printf("- ADVANCING BLOCK %d TO %d\n",b,pos[b]);

		/* Check if we have covered anything.  If we have, we can
		 * stop advancing
		 */
		if (!may_be_bg(cell[j--]))
		{
		    if (cov[b] == -1) cov[b]= j+1;
		    if (D)
		    	printf("L: COVERED NEW TARGET AT %d - BLOCK AT %d\n",
				j+1, pos[b]);
		    state= FINALSPACE; goto next;
		}
		if (j < 0)
		{
		    if (D)
			printf("L: END OF LINE\n");
		    clue->rbadb= -1;
		    return NULL;
		}
	    }

	    /* If we drop out here, then we've advanced the block as far
	     * as we can, but we haven't succeeded in covering anything new.
	     * So backtrack further.
	     */
	    if (D)
		printf("L: CAN'T ADVANCE\n");
	    state= BACKTRACK;
	    goto next;
	}
	next:;
    }

    if (D)
	printf("L: DONE\n");

    if (savepos)
    {
    	clue->rstamp= puz->nhist;
	clue->rbadi= -1;
	clue->rbadb= MAXLINE;
	if (DU)
	    printf("L: SAVING AT %d\n",clue->rstamp);

    }
    return pos;
}


/* LRO_SOLVE - Solve a line using the left-right overlap algorithm.
 *   (1) Find left-most solution.
 *   (2) Find right-most solution.
 *   (3) Paint any square that is in the same interval in both solutions.
 *
 * This returns a pointer to an array of bitstrings.  The calling program
 * should NOT free this array when it is done.  Returns NULL if there is no
 * solution.
 */

bit_type *lro_solve(Puzzle *puz, Solution *sol, dir_t k, line_t i)
{
    Clue *clue= &puz->clue[k][i];
    line_t ncell= clue->linelen;
    line_t nblock= clue->n;
    line_t *left, *right;
    line_t j;
    color_t c;
    line_t lb, rb;		/* Index of a block in left[] or right[] */
    int lgap,rgap;	/* If true, we are in gap before the indexed block */

    if (D)
	printf("-----------------%s %d-LEFT------------------\n",
		CLUENAME(puz->type,k),i);
    left= left_solve(puz, sol, k, i, 1);
    if (left == NULL) return NULL;

    if (D)
	printf("-----------------%s %d-RIGHT-----------------\n",
		CLUENAME(puz->type,k),i);
    right= right_solve(puz, sol, k, i, 1);
    if (right == NULL)
    	fail("Left solution but no right solution for %s %d\n",
		cluename(puz->type,k), i);

    if (D)
    {
	line_t i;

	printf("L: LEFT  SOLUTION: ");
	dump_pos(stdout, left, clue->length);
	printf("L: RIGHT SOLUTION:\n");
	dump_pos(stdout, right, clue->length);
    }

    /* col is the array of bitstrings used to return values.  The i-th bit
     * string starts at col[fbit_size*i] and bits will be set to 1 for
     * each color that cell i could be.
     */
    memset(col, 0, ncell * fbit_size * sizeof(bit_type));

    /* nbcolor is the number of blocks of each color we could be in */
    if (multicolor)
	memset(nbcolor, 0, puz->ncolor *  sizeof(line_t));

    lb= rb= 0;  /* index of current block in left[] and right[] */
    lgap= rgap= 1; /* true if we are in gap before block, not block */
    for (j= 0; j < ncell; j++)
    {
	/* Do we leave a left[] block ? */
	if (!lgap && left[lb]+clue->length[lb] == j)
	{
	    lgap= 1;
	    lb++;
	}
	/* Do we enter a left[] block ? */
        if (lgap && lb < nblock && left[lb] == j)
	{
	    lgap= 0;
	    if (multicolor) nbcolor[clue->color[lb]]++;
	}

	/* Do we leave a right[] block ? */
	if (!rgap && right[rb]+1 == j)
	{
	    if (multicolor) nbcolor[clue->color[rb]]--;
	    rgap= 1;
	    rb++;
	}
	/* Do we enter a right[] block ? */
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
	    fbit_setall(colbit(j));
	}
    }

    if (D)
    {
	printf("L: SOLUTION TO LINE %d DIRECTION %d\n",i,k);
	dump_lro_solve(puz, k, i, col);
    }

    return col;
}


/* Run the Left/Right Overlap algorithm on a line of the puzzle.  Update the
 * line to show the result, and create new jobs for crossing lines for changed
 * cells.  Returns 1 on success, 0 if there is a contradiction in the solution.
 */

int apply_lro(Puzzle *puz, Solution *sol, dir_t k, line_t i, int depth)
{
    bit_type *col;
    line_t ncell= puz->clue[k][i].linelen;
    Cell **cell= sol->line[k][i];
    line_t j;
    color_t z;
    bit_type new, *old;
    int newsol= 0;

    if ((VC && VV) && depth > 0)
    	printf("C: SOLVING %s %d at DEPTH %d\n",
	    CLUENAME(puz->type,k),i,depth-1);

    /* First try finding the solution in the cache */
    if (cachelines && (col= line_cache(puz, sol, k, i)) != NULL)
    {
	/* If found a cached solution invalidate old left/right solutions
	 * They might still be valid, or they might not.
	 */
	puz->clue[k][i].lbadb= -1;
	puz->clue[k][i].rbadb= -1;
    }
    else
    {
	/* If didn't find a solution from cache, Compute it */
	col= lro_solve(puz, sol, k, i);
	if (col == NULL) return 0;
	newsol= cachelines;
    }

    if (DW(k,i))
	printf("L: UPDATING GRID\n");

    for (j= 0; j < ncell; j++)
    {
	/* Is the new value different from the old value? */
	for (z= 0; z < fbit_size; z++)
	{
	    oldval[z]= cell[j]->bit[z];
	    new= (colbit(j)[z] & cell[j]->bit[z]);
	    if (cell[j]->bit[z] != new)
	    {
		/* Do probe merging (maybe) */
		if (merging) merge_set(puz, cell[j], colbit(j));

		if (VS || DW(k,i))
		{
		    if (DW(k,i))
			printf("L: CELL %d,%d - BYTE %d - CHANGED FROM (",
				k == D_ROW ? i : j, k == D_ROW ? j : i, z);
		    else
			printf("S: CELL %d,%d CHANGED FROM (",
				k == D_ROW ? i : j, k == D_ROW ? j : i);
		    dump_bits(stdout, puz, cell[j]->bit);
		}

		/* Save old value to history (maybe) */
		add_hist(puz, cell[j], 0);

		/* Copy new values into grid */
		cell[j]->bit[z] = new;
		for (z++; z < fbit_size; z++)
		{
		    oldval[z]= cell[j]->bit[z];
		    cell[j]->bit[z]&= colbit(j)[z];
		}

		if (VS || DW(k,i))
		{
		    printf(") TO (");
		    dump_bits(stdout, puz, cell[j]->bit);
		    printf(")\n");
		}

		if (DW(k,i) && VJ)
		    dump_history(stdout, puz, 0);

		if (puz->ncolor <= 2)
		    cell[j]->n= 1;
		else
		    count_cell(puz,cell[j]);

		if (cell[j]->n == 1)
		    solved_a_cell(puz,cell[j],1);

		/* Put other directions that use this cell on the job list */
		add_jobs(puz, sol, k, cell[j], depth, oldval);
		break;
	    }
	    else
		if (DW(k,i))
		    printf("L: CELL %d - BYTE %d\n",j,z);
	}
    }

    /* If we are caching and computed a new solution, cache it */
    if (newsol) add_cache(puz, sol, k, i);

    return 1;
}
