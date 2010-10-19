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


/* Find leftmost solution for line i of clueset k
 *
 * On success, returns a pointer to an array of starting indexes for blocks.
 * This memory should not be freed.  On failure, returns NULL.
 */

#undef RIGHT
#define LEFT
#include "line_generic.c"

/* Find rightmost solution for line i of clueset k
 *
 * On success, returns a pointer to an array of ending indexes for blocks.
 * This array should not be freed.  On failure, returns NULL.
 */

#undef LEFT
#define RIGHT
#include "line_generic.c"

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
