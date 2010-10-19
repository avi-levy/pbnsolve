/* Generic Line Solver - this does both leftmost and rightmost solutions,
 * with different substitutions for various macros.  The comments are based
 * on the leftmost algorithm.
 *
 * Note that the pos[] array points to starting indexes when we are doing
 * leftmost, and ending indexes when we are doing rightmost, but the pbit
 * array is always based on starting indexes.  The macros clean this up.
 */

#ifdef LEFT
#define LS_NAME left_solve
#define pos lpos;
#define FIRSTB 0
#define LASTB (clue->n-1)
#define OVERFLOWB(b) ((b) >= clue->n)
#define ENDBLOCK(b) (pos[b] + clue->length[b])
#define FIRSTC 0
#define OVERFLOWC(j) (cell[j] == NULL)
#define PMIN(b) clue->pmin[b]
#define PMAX(b) clue->pmax[b]
#define MAY_PLACE(b,p) may_place(clue,b,p)
#define MAY_PLACEbj(p) may_place(clue,b,p)
#define INC(x) ((x)++)
#define DEC(x) ((x)--)
#define PINC(x) (++(x))
#define PDEC(x) (--(x))
#define NEXT(x) ((x)+1)
#define PREV(x) ((x)-1)
#define DIST(x,y) ((x) - (y))
#define LT <
#define GT >
#endif

#ifdef RIGHT
#define LS_NAME right_solve
#define pos rpos;
#define FIRSTB maxblock
#define LASTB 0
#define OVERFLOWB(b) ((b) < 0)
#define FIRSTC (ncell - 1)
#define OVERFLOWC(j) (j < 0)
#define ENDBLOCK(b) (pos[b] - clue->length[b])
#define PMIN(b) (clue->pmax[b]+clue->length[b]-1)
#define PMAX(b) (clue->pmin[b]+clue->length[b]-1)
#define MAY_PLACE(b,p) may_place(clue,b,p-clue->length[b]+1)
#define MAY_PLACEbj(p) may_place(clue,b,j+1)
#define INC(x) ((x)--)
#define DEC(x) ((x)++)
#define PINC(x) (--(x))
#define PDEC(x) (++(x))
#define NEXT(x) ((x)-1)
#define PREV(x) ((x)+1)
#define DIST(x,y) ((y) - (x))
#define LT >
#define GT <
#endif

/* 
 * This routine is structured as a finite state machine.  The state variable
 * 'state' selects between various states we might be in.  Each state has a
 * comment defining the precondition for that state.  This is basically a
 * trick to make old-fashioned spaghetti code look like it makes sense.
 */

/* Finite State Machine States for the line solver */

#define NEWBLOCK        0
#define PLACEBLOCK      1
#define FINALSPACE      2
#define CHECKPOS        3
#define CHECKREST       4
#define BACKTRACK       5
#define ADVANCEBLOCK    6
#define HALT            7

line_t *LS_NAME(Puzzle *puz, Solution *sol, dir_t k, line_t i)
{
    line_t b,j;
    color_t currcolor, nextcolor;
    int backtracking, state;
    Clue *clue= &puz->clue[k][i];
    Cell **cell= sol->line[k][i];
#if RIGHT
    line_t ncell= clue->linelen;
    line_t maxblock= clue->n - 1;
#endif
    line_t *pos, *cov;

    /* The position array gives the current position of each block.  It is
     * terminated by a -1.  The cov array contains the index of the left-most
     * cell covered by the block which CANNOT be white.  It is -1 if there is
     * no such cell.  */
    pos[clue->n]= -1;

    /* Initialize finite state machine */
    b= FIRSTB;
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

	    if (OVERFLOWB(b))
	    {
		/* No blocks left.  Check rest of line to make sure it can
		 * be blank.
		 */
		if (DEC(b) == FIRSTB) j= FIRSTC;
	    	state= CHECKREST; goto next;
	    }

	    currcolor= clue->color[b];	/* Color of current block */
	    if (D)
		printf("L: PLACING BLOCK %d COLOR %d LENGTH %d\n",
	    	b,currcolor,clue->length[b]);

	    if (b == FIRSTB)
	    {
		/* Get earliest possible position from pmin array */
		pos[b]= PMIN(0);
	    }
	    else
	    {
		/* Earliest possible position of block b is after previous
		 * block, leaving a space between the blocks if they are
		 * the same color. */
		pos[b]= j;
		if (clue->color[PREV(b)] == currcolor) INC(pos[b]);

		/* Make sure this is consistent with pbit */
		if (pos[b] LT PMIN(b))
		    pos[b]= PMIN(b);
		else if (pos[b] GT PMAX(b))
		    return NULL;
	    }

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

	    while (!may_be(cell[pos[b]], currcolor) || !MAY_PLACE(b,pos[b]));
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
		    while (b LT NEXT(FIRSTB) &&
			    !may_be(cell[j], clue->color[PREV(b)]))
			DEC(b);
		    state= BACKTRACK; goto next;
		}

		if (D)
		    printf("L: SHIFT BLOCK TO %d\n",NEXT(pos[b]));

		if (PINC(pos[b]) GT PMAX(b))
		{
		    /* Advanced the block beyond it last legal position */
		    if (D)
			printf("L: END OF THE LINE\n");
		    return NULL;
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
	    for (INC(j); DIST(j,pos[b]) < clue->length[b]; INC(j))
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

	    if (!OVERFLOWC(j))
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
			!OVERFLOWB(NEXT(b)) &&
			(currcolor != clue->color[NEXT(b)])) ?
			    clue->color[NEXT(b)] : BGCOLOR;

		if (D)
		    printf("L:  NEXTCOLOR=%d\n",nextcolor);

		while (!OVERFLOWC(j) && !may_be_bg(cell[j]) &&
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
			if (pos[b] == PMAX(b))
			{
			    return NULL;
			}
			/* All's OK - go ahead and advance the block */
			INC(pos[b]);
			if (cov[b] == -1 && !may_be_bg(cell[j]))
			    cov[b]= j;
			INC(j);
			if (D)
			    printf("L: ADVANCING BLOCK (cov=%d)\n",cov[b]);
		    } while (!MAY_PLACEbj(pos[b]));
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

	    if (OVERFLOWC(j) && b != LASTB)
	    {
		/* Ran out of space, but still have blocks left */
		return NULL;
	    }

	    /* Successfully placed block b - Go on to next block */
	    INC(b);
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

	    while (!MAY_PLACEbj(pos[b]))
	    {
	    	/* Current position is fine except for being banned by pbit 
	    	 * Can we advance? */
	    	
	    	if (pos[b] == PMAX(b) ||   /* Cell already as far as it goes */
		    pos[b] == cov[b] ||	   /* First cell covers something */
		    !may_be(cell[j],currcolor))/*Next cell cannot be our color */
		{
		    /* can't advance.  BACKTRACK */
		    state= BACKTRACK; goto next;
		}
		/* Can advance.  Do it */
		INC(pos[b]);
		INC(j);
	    }
	    state= FINALSPACE; goto next;


	case CHECKREST:

	    /* Precondition: All blocks have been legally placed.  j points
	     *   to first empty space after last block.  b is the index of
	     *   the last block, or -1 if there are no blocks.
	     * Action:  Check that there are no cells after the last block
	     *   which cannot be left background color.
	     */

	    if (!OVERFLOWC(j))
	    {
		if (D)
		    printf("L: PLACED LAST BLOCK - CHECK REST OF LINE\n");

		for (; !OVERFLOWC(j); INC(j))
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
			j= ENDBLOCK(b);
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

	    if (PDEC(b) LT FIRSTB)
	    {
		if (D)
		    printf("L: NO BLOCKS LEFT TO BACKTRACK TO - FAIL\n");
		return NULL;
	    }

	    j= BLOCKEND(b);		/* First cell after end of block */
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

	    while(cov[b] < 0 || pos[b] LT cov[b])
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
			pos[b]= NEXT(j);
			if (D)
			    printf("- JUMPING pos=%d\n",pos[b]);
			backtracking= 1;
			state= PLACEBLOCK; goto next;
		    }
		}

		/* Can advance.  Do it */
		INC(pos[b]);
		if (D)
		    printf("- ADVANCING BLOCK %d TO %d\n",b,pos[b]);

		if (pos[b] GT PMAX(b))
		{
		    if (D)
			printf("L: END OF LINE\n");
		    return NULL;
		}

		/* Check if we have covered anything.  If we have, we can
		 * stop advancing
		 */
		if (!may_be_bg(cell[INC(j)]))
		{
		    if (cov[b] == -1) cov[b]= PREV(j);
		    if (D)
			printf("L: COVERED NEW TARGET AT %d - BLOCK AT %d\n",
			    PREV(j), pos[b]);

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

    return pos;
}
