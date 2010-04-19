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


#ifdef LINEWATCH
#define WL(k,i) (puz->clue[k][i].watch)
#define WC(i,j) (puz->clue[0][i].watch || puz->clue[1][j].watch)
#else
#define WL(k,i) 0
#define WC(i,j) 0
#endif


#ifdef GR_SIMPLE
/* Simple version.  Just choose cells based on neighborlyness */
int ratecell(Puzzle *puz, Solution *sol, line_t i, line_t j)
{
    return 0;
}
#endif

#ifdef GR_ADHOC
/* First 'smart' version.  Prefer low slack, and low numbers of clues. */
int ratecell(Puzzle *puz, Solution *sol, line_t i, line_t j)
{
    int si= puz->clue[0][i].slack + 2*puz->clue[0][i].n;
    int sj= puz->clue[1][j].slack + 2*puz->clue[1][j].n;
    return (si < sj) ? 3*si+sj : 3*sj+si;
}
#endif

#ifdef GR_MATH
/* Mathematical version.  Prefer to work on rows with fewer solutions. */
float ratecell(Puzzle *puz, Solution *sol, line_t i, line_t j)
{
    float si= bicoln(puz->clue[0][i].slack + puz->clue[0][i].n,
		puz->clue[0][i].n);
    float sj= bicoln(puz->clue[1][j].slack + puz->clue[1][j].n,
		puz->clue[1][j].n);
    return (si < sj) ? si : sj;
}
#endif

/* Count neighbors of a cell which are either solved or edges */

int count_neighbors(Solution *sol, line_t i, line_t j)
{
    int count= 0;

    /* Count number of solved neighbors or edges */
    if (i == 0 || sol->line[0][i-1][j]->n == 1) count++;
    if (i == sol->n[0]-1 || sol->line[0][i+1][j]->n == 1) count++;
    if (j == 0 || sol->line[0][i][j-1]->n == 1) count++;
    if (sol->line[0][i][j+1]==NULL || sol->line[0][i][j+1]->n == 1) count++;

    return count;
}

/* Pick a cell to make a guess on.  Currently we prefer cells with lots of
 * solved neighbors.  A cell with all neighbors set is our first choice.
 * Among clues with equal numbers of neighbors, we prefer ones in rows or
 * columns with low slack and low numbers of clues.
 *
 * This is used only in the heuristic guessing algorithm, not the probing
 * algorithm.
 */

Cell *pick_a_cell(Puzzle *puz, Solution *sol)
{
    line_t i, j;
    int v, maxv;
    float s, minrate;
    Cell *cell, *favcell;

    if (puz->type != PT_GRID)
    	fail("pick_a_cell() only works for grid puzzles");

    maxv= -1;
    for (i= 0; i < sol->n[0]; i++)
    {
    	for (j= 0; (cell= sol->line[0][i][j]) != NULL; j++)
	{
	    /* Not interested in solved cells */
	    if (cell->n == 1) continue;

	    /* Count number of solved neighbors or edges */
	    v= count_neighbors(sol, i, j);

	    /* If all neighbors are set, this looks good */
	    if (v == 2*sol->nset) return cell;

	    if (v >= maxv)
	    {
		s= (float)ratecell(puz,sol,i,j);

		if (v > maxv || s < minrate)
		{
		    maxv= v;
		    minrate= s;
		    if (VG) printf("G: MAX CELL %d,%d SCORE=%d/%f\n",
		    	i,j,maxv,minrate);
		    favcell= cell;
	    	}
	    }
	}
    }

    if (maxv == -1)
    {
    	if (VA) printf("Called pick-a-cell on complete puzzle\n");
	return NULL;
    }

    return favcell;
}



#ifdef GC_MAX
/* Pick max color as guess */
color_t pick_color(Puzzle *puz, Solution *sol, Cell *cell)
{
    color_t c;
    for(c= puz->ncolor-1; c >= 0 && !may_be(cell,c); c--)
	    ;
    if (c <= 0) fail("Picked a cell to guess on with one color\n");
    return c;
}
#endif

#ifdef GC_MIN
/* Pick min color as guess */
color_t pick_color(Puzzle *puz, Solution *sol, Cell *cell)
{
    color_t c;
    for(c= 0; c < puz->ncolor && !may_be(cell,c); c++)
	    ;
    if (c >= puz->ncolor-1) fail("Picked a cell to guess on with one color\n");
    return c;
}
#endif

#ifdef GC_RAND
/* Pick random color as guess */
color_t pick_color(Puzzle *puz, Solution *sol, Cell *cell)
{
    color_t c, bestc, n= 0;

    for(c= 0; c < puz->ncolor; c++)
	if (may_be(cell,c))
	{
	    n++;
	    if (rand() < RAND_MAX/n)
	    	bestc= c;
	}
    if (n <= 1) fail("Picked a cell to guess on with one color\n");
    return bestc;
}
#endif

#ifdef GC_CONTRAST
/* Pick pick a color different from the neighboring colors */
color_t pick_color(Puzzle *puz, Solution *sol, Cell *cell)
{
    color_t c, bestc, n, bestn= -1;
    line_t i= cell->line[0];
    line_t j= cell->line[1];

    for(c= 0; c < puz->ncolor; c++)
	if (may_be(cell,c))
	{
	    n= 0;
	    if (i > 0)
	    {
		if (!may_be(sol->line[0][i-1][j], c)) n++;
	    }
	    else if (c != 0) n++;

	    if (i < sol->n[0]-2)
	    {
	    	if (!may_be(sol->line[0][i+1][j], c)) n++;
	    }
	    else if (c != 0) n++;

	    if (j > 0)
	    {
	    	if (!may_be(sol->line[0][i][j-1], c)) n++;
	    }
	    else if (c != 0) n++;

	    if (j < sol->n[1]-2)
	    {
	    	if (!may_be(sol->line[0][i][j+1], c)) n++;
	    }
	    else if (c != 0) n++;

	    if (n > bestn)
	    {
		bestc= c;
	     	bestn= n;
	    }
	}
    return bestc;
}
#endif


/* Guess the given color for the given cell.  Mark this as a branch point in
 * the history list (and start keeping history if we weren't up to now).
 * Put all lines crossing the given cell on the job list.
 */

void guess_cell(Puzzle *puz, Solution *sol, Cell *cell, color_t c)
{
    dir_t k;
    Hist *h;

    /* Save old cell in backtrack history */
    h= add_hist(puz, cell, 1);

    /* Set just that one color */
    cell->n= 1;
    fbit_setonly(cell->bit,c);
    puz->nsolved++;

    /* Put all crossing lines onto the job list */
    add_jobs(puz, sol, -1, cell, 0, h->bit);
}


/* Find logical consequences from a current puzzle state using the line solver.
 * There must be at least one job on the job-list for this to get started.
 * Returns 0 if a contradiction was found, one otherwise.
 */

int line_solve(Puzzle *puz, Solution *sol, int contradicting)
{
    dir_t dir;
    line_t i;
    int depth;

    while (next_job(puz, &dir, &i, &depth))
    {
	nlines++;
	if ((VB && !VC) || WL(dir,i))
	    printf("*** %s %d\n",CLUENAME(puz->type,dir), i);
	if (VB || WL(dir,i))
	    dump_line(stdout,puz,sol,dir,i);

	if (contradicting && depth >= contradepth)
	{
	    /* At max depth we just check if the line is solvable */
	    line_t *soln= left_solve(puz, sol, dir, i, 0);
	    if (soln != NULL)
	    {
		if ((VC&&VV) || WL(dir,i))
		    printf("C: %s %d OK AT DEPTH %d\n",
			cluename(puz->type,dir),i,depth);
	    }
	    else
	    {
		if ((VC&&VV) || WL(dir,i))
		    printf("C: %s %d FAILED AT DEPTH %d\n",
			cluename(puz->type,dir),i,depth);
		return 0;
	    }
	}
	else if (!apply_lro(puz, sol, dir, i, depth + 1))
	{
	    /* Found a contradiction */
	    return 0;
	}

	if (VJ)
	{
	    printf("CURRENT JOBS:\n");
	    dump_jobs(stdout,puz);
	}
    }
    return 1;
}


/* Find ALL logical consequences from a current puzzle state using the line
 * solver and/or the exhaustive search algorithm.  Returns:
 *
 * negative = hit a contradiction - initial state was inconsistent
 *        0 = stalled without completing the puzzle.  Will need to search.
 *        1 = completely solved the puzzle.
 */

int logic_solve(Puzzle *puz, Solution *sol, int contradicting)
{
    int stalled;
    int rc;

    while (1)
    {
	if (maylinesolve)
	{
	    /* Run the line solver - exit if it finds a contradiction  */
	    if (!line_solve(puz,sol,contradicting))
		return -1;

	    /* Check if puzzle is done */
	    if (puz->nsolved == puz->ncells) return 1;

	    /* If we are contradicting, don't do the exhaustive test.
	     * Too expensive. */
	    if (contradicting) return 0;

	    /* If we have no other algorithm to try, we are stalled */
	    if (!mayexhaust) return 0;
	}

	/* Look for logically markable squares that the LRO line solver
	 * may have missed - if we find any resume line solving.  We
	 * stop doing this once we have started searching, because it
	 * doesn't generally speed us up, it's only done to make us more
	 * sure about things being logically solvable.
	 */

	rc= try_everything(puz,sol, puz->nhist > 0 || puz->found != NULL);

	if (rc > 0)
	{
	    /* Found some cells.  Exit if we are done */
	    if (puz->nsolved == puz->ncells)
		return 1;
	    /* If we are not done, try line solving again */
	}
	else
	{
	   /* Found no cells, or hit a contradiction */
	   return rc;
	}
    }
}


/* Solve a puzzle.  Return 0 if a contradiction was found, 1 otherwise */

int solve(Puzzle *puz, Solution *sol)
{
    Cell *cell;
    line_t besti, bestj;
    color_t bestc;
    int bestnleft;
    int rc;

    /* One color puzzles are already solved */
    if (puz->ncolor < 2)
    {
    	puz->nsolved= puz->ncells;
	return 1;
    }

    while (1)
    {
	/* Always start with logical solving */
	if (VA) printf("A: LINE SOLVING\n");
	rc= logic_solve(puz, sol, 0);

	if (rc > 0) return 1;   /* Exit if the puzzle is complete */

	if (rc == 0)
	{
	    /* Logical solving has stalled. */

	    if (VA)
	    {
		printf("A: STUCK - Line solving failed\n");
		print_solution(stdout,puz,sol);
	    }

	    if (maycontradict)
	    {
		/* Try a depth-limited search for logical contradictions */
		if (VA) printf("A: SEARCHING FOR CONTRADICTIONS\n");
		rc= contradict(puz,sol);

		if (rc > 0) return 1; /* puzzle complete - stop */

		if (rc < 0) continue; /* found some - resume logic solving */

		/* otherwise, try something else */
	    }

	    /* Stop if no guessing is allowed */
	    if (!maybacktrack) return 1;
	    
	    /* Shut down the exhaustive search once we start searching */
	    if (maylinesolve) mayexhaust= 0;
	    
	    /* Turn on caching when we first start searching */
	    if (maycache && !cachelines)
	    {
		cachelines= 1;
		init_cache(puz);
	    }

	    if (mayprobe)
	    {
		/* Do probing to find best guess to make */
		if (VA) printf("A: PROBING\n");
	    	rc= probe(puz, sol, &besti, &bestj, &bestc);

		if (rc > 0)
		    return 1; /* Stop if accidentally completed the puzzle */
		if (rc < 0)
		    continue; /* Resume logic solving if found contradiction */

		/* Otherwise, use the guess returned from the probe */
		cell= sol->line[0][besti][bestj];
		if (VA)
		{
		    printf("A: PROBING SELECTED ");
		    print_coord(stdout,puz,cell);
		    printf(" COLOR %d\n",bestc);
		}
	    }
	    else
	    {
		/* Old guessing algorithm.  Use heuristics to make a guess */
		cell= pick_a_cell(puz, sol);
		if (cell == NULL)
		    return 0;

		bestc= pick_color(puz,sol,cell);
		if (VA || WC(cell->line[0],cell->line[1]))
		{
		    printf("A: GUESSING SELECTED ");
		    print_coord(stdout,puz,cell);
		    printf(" COLOR %d\n",bestc);
		}
	    }
	    guess_cell(puz, sol, cell, bestc);
	    guesses++;
	}
	else
	{
	    /* We have hit a contradiction - try backtracking */
	    if (VA) printf("A: STUCK ON CONTRADICTION - BACKTRACKING\n");

	    guesses++;

	    /* Back up to last guess point, and invert that guess */
	    if (backtrack(puz,sol))
		/* Nothing to backtrack to - puzzle has no solution */
		return 0;
	    if (VB) print_solution(stdout,puz,sol);
	    if (VB) dump_history(stdout, puz, VV);
	}
    }
}
