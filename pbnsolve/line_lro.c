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
 * block position bit strings in the clue data structures.
 */

static line_t *lpos, *rpos, *cov;
static line_t *nbcolor;
static bit_type *col;
static int multicolor;
bit_type *oldval;

void init_line(Puzzle *puz)
{
    line_t maxcluelen= 0, maxdimension= 0;
    line_t i, j, m;
    dir_t k;
    Clue *clue;

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
	    clue= &(puz->clue[k][i]);
	    if (clue->n > maxcluelen) maxcluelen= clue->n;

	    clue->pbit= (bit_type **)malloc(sizeof(bit_type *)*clue->n);
	    clue->pmin= (line_t *)malloc(sizeof(line_t)*(clue->n+1));
	    clue->pmax= (line_t *)malloc(sizeof(line_t)*(clue->n+1));

	    for (j= 0; j < clue->n; j++)
	    {
		clue->pmin[j]= 0;
		clue->pmax[j]= m= clue->linelen - clue->length[j];
		clue->pbit[j]=
		    (bit_type *)calloc(bit_size(m+1),sizeof(bit_type));
		bit_setall(clue->pbit[j], m);
	    }
	}
    }

    /* Allocate storage spaces for left_solve and right_solve arrays.  We
     * use these instead of the ones in the Clue structure if we don't want
     * to save the results of the solution.
     */
    lpos= (line_t *)malloc((maxcluelen + 1) * sizeof(line_t));
    rpos= (line_t *)malloc((maxcluelen + 1) * sizeof(line_t));
    cov= (line_t *)malloc(maxcluelen * sizeof(int));

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


/* Finite State Machine States for the line solver */

#define NEWBLOCK	0
#define PLACEBLOCK	1
#define FINALSPACE	2
#define CHECKPOS	3
#define CHECKREST	4
#define BACKTRACK	5
#define ADVANCEBLOCK	6
#define HALT		7

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

line_t *left_solve(Puzzle *puz, Solution *sol, dir_t k, line_t i)
{
    line_t b,j;
    color_t currcolor, nextcolor;
    int backtracking, state;
    Clue *clue= &puz->clue[k][i];
    Cell **cell= sol->line[k][i];
    line_t *pos, *cov;

    /* The position array gives the current position of each block.  It is
     * terminated by a -1.  The cov array contains the index of the left-most
     * cell covered by the block which CANNOT be white.  It is -1 if there is
     * no such cell.  */
    pos= lpos;
    pos[clue->n]= -1;

    /* Initialize finite state machine */
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
	    if (D)
		printf("L: PLACING BLOCK %d COLOR %d LENGTH %d\n",
	    	b,currcolor,clue->length[b]);

	    if (b == 0)
	    {
		/* Get earliest possible position from pmin array */
		pos[b]= clue->pmin[0];:
	    }
	    else
	    {
		/* Earliest possible position of block b is after previous
		 * block, leaving a space between the blocks if they are
		 * the same color. */
		pos[b]= j + ((clue->color[b-1] == currcolor) ? 1 : 0);

		/* Make sure this is consistent with pbit */
		if (pos[b] < clue->pmin[b])
		    pos[b]= clue->pmin[b];
		else if (pos[b] > clue->pmax[b])
		    return NULL;
	    }

	    if (D)
		printf("L: FIRST POS %d\n",pos[b]);

	    state= PLACEBLOCK; goto next;

	case PLACEBLOCK:

	    /* Precondition: Blocks 0 through b-1 have been legally placed
	     *    or b = 0.  pos[b] points points to a position less than or
	     *    equal to the leftmost position of block b.  currcolor is
	     *    the color of the current block b.
	     * Action:  Advance pos[b] until it points to a position where
	     *    the next length[b] cells can all accomodate the block's
	     *    color.  Also compute cov[b], the index of the leftmost
	     *    cell covered by the block that cannot be background.
	     */

	    /* First find a place for the first cell of the block, skipping
	     * over positions banned by pbit and over wrong colored cells
	     * until we find an acceptable starting position.
	     */

	    while (!may_be(cell[pos[b]], currcolor) ||
		   !may_place(clue,b,pos[b]));
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

		if (++pos[b] > clue->pmax[b])
		{
		    if (D)
			printf("L: END OF THE LINE\n");
		    return NULL;	/* Hit end of line */
		}
	    }

	    /* First cell has found a home. Can we place the rest of the block?
	     * While we are at at, compute cov[b].  pbit was initialized so
	     * that it didn't allow blocks to run off the end of the line, so
	     * we don't need to worry about that case here.
	     */
	    if (D)
		printf("L: FIRST CELL OF %d PLACED AT %d\n",b,pos[b]);
	    j= pos[b];
	    cov[b]= (may_be_bg(cell[j]) ? -1 : j);
	    for (j++; j - pos[b] < clue->length[b]; j++)
	    {
		if (D)
		    printf("L: CHECKING CELL AT %d\n",j);

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
	     *    cells of the block can accomodate the block's color and which
	     *    is has not been banned by pbit.  j is the index of the first
	     *    cell after the block.  currcolor is the color of the current
	     *    block b.
	     * Action:  Check if the first cell after the block is a legal
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
		 * first cell AFTER the block is able to be some color
		 * different than the block's color.
		 */

		/* The next cell can always be background, but in multicolor
		 * it can also be the color of the next block, if the next
		 * block is a different color than this block.  So nextcolor
		 * is that color, if there is such a color.  Otherwise it is
		 * background color */
		nextcolor= (multicolor &&
			(b < clue->n - 1) &&
			(currcolor != clue->color[b+1])) ?
			    clue->color[b+1] : BGCOLOR;

		if (D)
		    printf("L:  NEXTCOLOR=%d\n",nextcolor);

		while (cell[j] != NULL && !may_be_bg(cell[j]) &&
			(nextcolor == BGCOLOR || !may_be(cell[j],nextcolor)))
		{
		    do {
			/* Next cell is wrong color or we just advanced to a
			 * position banned by pbit. So try advancing the
			 * block one cell.  But we need to make sure that
			 * we don't uncover any cells, and we need to make
			 * sure that the cell at the end is one we can cover.
			 */
			if (D)
			    printf("L: NO FINAL SPACE\n");
			if (cov[b] == pos[b] ||
			    (multicolor && !may_be(cell[j],currcolor)))
			{
			    /* can't advance.  BACKTRACK */
			    state= BACKTRACK; goto next;
			}
			/* If this cell is already as far right as it may
			 * be then we are stuck */
			if (pos[b] == pmax[b])
			{
			    return NULL;
			}
			/* All's OK - go ahead and advance the block */
			pos[b]++;
			if (cov[b] == -1 && !may_be_bg(cell[j]))
			    cov[b]= j;
			j++;
			if (D)
			    printf("L: ADVANCING BLOCK (cov=%d)\n",cov[b]);
		    } while (!may_place(cell,b,pos[b]));
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
		return NULL;
	    }

	    /* Successfully placed block b - Go on to next block */
	    b++;
	    backtracking= 0;
	    state= NEWBLOCK; goto next;

	case CHECKPOS:

	    /* Precondition: Blocks 0 through b-1 have been legally placed
	     *    or b = 0.  pos[b] points points to a position so that all
	     *    cells of the block can accomodate the block's color, but
	     *    which might have been banned by pbit.  j is the index of the
	     *    first cell after the block.  currcolor is the color of the
	     *    current block b. (This is just like FINALSPACE except we
	     *    aren't assured that pbit allows the current position.)
	     * Action:  If the current position is banned by pbit, try advancing
	     *    without uncovering anything until we reach an acceptable
	     *    position. If it works, check FINALSPACE.  If it fails,
	     *    BACTRACK.
	     */

	    if (D)
		printf("L: CHECKING POSITION\n");

	    while (!may_place(cell,b,pos[b]))
	    {
	    	/* Current position is fine except for being banned by pbit 
	    	 * Can we advance? */
	    	
	    	if (pos[b] == pmax[b] ||   /* Cell already as far as it goes */
		    pos[b] == cov[b] ||	   /* First cell covers something */
		    !may_be(cell[j],currcolor))/*Next cell cannot be our color */
		{
		    /* can't advance.  BACKTRACK */
		    state= BACKTRACK; goto next;
		}
		/* Can advance.  Do it */
		pos[b]++;
		j++;
	    }
	    state= FINALSPACE; goto next;


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
		if (pos[b] > clue->pmax[b])
		{
		    if (D)
			printf("L: END OF LINE\n");
		    return NULL;
		}

		/* Check if we have covered anything.  If we have, we can
		 * stop advancing
		 */
		if (!may_be_bg(cell[j++]))
		{
		    if (cov[b] == -1) cov[b]= j-1;
		    if (D)
			printf("L: COVERED NEW TARGET AT %d - BLOCK AT %d\n",
			    j-1, pos[b]);

		    state= CHECKPOS; goto next;
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
    line_t nchange= 0;

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
		nchange++;

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

    if (hintlog && nchange > 0)
    {
	printf("LINESOLVER: %s %d - update %d cells\n",
	    cluename(puz->type,k),i+1,nchange);
	hintsnapshot(puz,sol);
    }

    return 1;
}
