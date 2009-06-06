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

#include <limits.h>
#include "pbnsolve.h"

#ifdef GR_SIMPLE
/* Simple version.  Just choose cells based on neighborlyness */
int ratecell(Puzzle *puz, Solution *sol, int i, int j)
{
    return 0;
}
#endif

#ifdef GR_ADHOC
/* First 'smart' version.  Prefer low slack, and low numbers of clues. */
int ratecell(Puzzle *puz, Solution *sol, int i, int j)
{
    int si= puz->clue[0][i].slack + 2*puz->clue[0][i].n;
    int sj= puz->clue[0][i].slack + 2*puz->clue[0][i].n;
    return (si < sj) ? 3*si+sj : 3*sj+si;
}
#endif

#ifdef GR_MATH
/* Mathematical version.  Prefer to work on rows with fewer solutions. */
float ratecell(Puzzle *puz, Solution *sol, int i, int j)
{
    float si= bicoln(puz->clue[0][i].slack + puz->clue[0][i].n,
		puz->clue[0][i].n);
    float sj= bicoln(puz->clue[0][i].slack + puz->clue[0][i].n,
		puz->clue[0][i].n);
    return (si < sj) ? si : sj;
}
#endif

/* Count neighbors of a cell which are either solved or edges */

int count_neighbors(Solution *sol, int i, int j)
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
    int i, j, v, maxv;
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
int pick_color(Puzzle *puz, Solution *sol, Cell *cell)
{
    int c;
    for(c= puz->ncolor-1; c >= 0 && !may_be(cell,c); c--)
	    ;
    if (c <= 0) fail("Picked a cell to guess on with one color\n");
    return c;
}
#endif

#ifdef GC_MIN
/* Pick min color as guess */
int pick_color(Puzzle *puz, Solution *sol, Cell *cell)
{
    int c;
    for(c= 0; c < puz->ncolor && !may_be(cell,c); c++)
	    ;
    if (c >= puz->ncolor-1) fail("Picked a cell to guess on with one color\n");
    return c;
}
#endif

#ifdef GC_RAND
/* Pick random color as guess */
int pick_color(Puzzle *puz, Solution *sol, Cell *cell)
{
    int c, bestc, n= 0;

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
int pick_color(Puzzle *puz, Solution *sol, Cell *cell)
{
    int c, bestc, n, bestn= -1;
    int i= cell->line[0];
    int j= cell->line[1];

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

void guess_cell(Puzzle *puz, Solution *sol, Cell *cell, int c)
{
    int k;

    /* Save old cell in backtrack history */
    add_hist(puz,cell,1);

    /* Set just that one color */
    cell->n= 1;
    bit_clearall(cell->bit,puz->ncolor);
    bit_set(cell->bit,c);
    puz->nsolved++;

    /* Put all crossing lines onto the job list */
    add_jobs(puz, cell);
}


/* Find all logical consequences from a current puzzle state.  There must be
 * at least one job on the job-list for this to get started.  Returns 0 if
 * a contradiction was found, one otherwise.
 */

int logic_solve(Puzzle *puz, Solution *sol)
{
    int dir, i;

    while (next_job(puz, &dir, &i))
    {
	nlines++;
	if (VB) printf("*** %s %d\n",CLUENAME(puz->type,dir), i);
	if (VB & VV) dump_line(stdout,puz,sol,dir,i);

	if (!apply_lro(puz, sol, dir, i))
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


/* Solve a puzzle.  Return 0 if a contradiction was found, 1 otherwise */


int solve(Puzzle *puz, Solution *sol)
{
    Cell *cell;
    int probing= 0;
    int besti, bestj, bestc, bestnleft;
    int i, j, c, nleft, neigh, stalled, rc;

    /* One color puzzles are already solved */
    if (puz->ncolor < 2)
    {
    	puz->nsolved= puz->ncells;
	return 1;
    }

    while (1)
    {
	if (!maylinesolve || (stalled= logic_solve(puz,sol)))
	{
	    /* line solving hit a dead end but not a contradiction */

	    /* Stop if puzzle is done */
	    if (puz->nsolved == puz->ncells) return 1;

	    /* Look for logically markable squares that the LRO line solver
	     * may have missed - if we find any resume line solving.  We
	     * stop doing this once we have started searching, because it
	     * doesn't generally speed us up, it's only done to make us more
	     * sure about things being logically solvable.
	     */
	    if (mayexhaust)
	    {
	    	rc= try_everything(puz,sol,
			puz->history != NULL || puz->found != NULL);

		if (rc > 0)
		{
		    /* Found some cells.  If we are not done, try
		     * line solving again */
		    if (puz->nsolved == puz->ncells) return 1;
		    continue;
		}
		else
		{
		   /* Found no cells, or hit a contradiction */
		   stalled= (rc == 0);
		}
	    }
    	}

	if (stalled)
	{
	    /* Logical solving has stalled.  Try searching */

	    /* Stop if no guessing is allowed */
	    if (!maybacktrack) return 1;
	    
	    /* Shut down the exhaustive search once we start searching */
	    if (maylinesolve) mayexhaust= 0;

	    if (VB) printf("B: STUCK\n");
	    if (VB) print_solution(stdout,puz,sol);

	    if (mayprobe)
	    {
		/* Probing algorithm */
		if (!probing)
		{
		    /* Starting a new probe sequence - initialize stuff */
		    if (VP) printf("P: STARTING PROBE SEQUENCE\n");
		    i= j= c= 0;
		    bestnleft= INT_MAX;
		    probing= 1;
		}
		else
		{
		    /* Completed a probe - save it's rating and undo it */
		    nleft= puz->ncells - puz->nsolved;
		    if (VP) printf("P: PROBE ON (%d,%d)%d COMPLETE WITH "
		    	"%d CELLS LEFT\n", i,j,c,nleft);
		    if (nleft < bestnleft)
		    {
		    	bestnleft= nleft;
			besti= i;
			bestj= j;
			bestc= c;
		    }
		    if (VP) printf("P: UNDOING PROBE\n");
		    undo(puz,sol,0);
		    if (VP) dump_history(stdout, puz, 0);
		    ++c;
		}

		/* Scan for the next cell to probe on */
		for (; i < sol->n[0]; i++)
		{
		    for (; (cell= sol->line[0][i][j]) != NULL; j++)
		    {
			if (cell->n < 2) continue;
		    	neigh= count_neighbors(sol, i, j);
			if (neigh >= 2)
			{
			    for (; c < puz->ncolor; c++)
			    {
			    	if (may_be(cell, c))
				{
				    /* Found a cell - go probe on it */
				    if (VP)
					printf("P: PROBING (%d,%d) COLOR %d\n",
				    	i,j,c);
				    probes++;
				    merge_guess();
				    guess_cell(puz,sol,cell,c);
				    goto loop;
				}
			    }
			    c= 0;
			    /* Finished all probes on a cell.  Check if there
			     * is anything that was a consequence of all
			     * alternatives.  If so, set that as a fact,
			     * cancel probing and proceed.
			     */
			    if (merge_check(puz))
			    {
				merges++;
				probing= 0;
				goto loop;
			    }
			}
		    }
		    j= 0;
		}

		/* completed probing all cells - select best as our guess */
		probing= 0;
		if (bestnleft == INT_MAX)
		{
		    print_solution(stdout,puz,sol);
		    printf("ERROR: found no cells to prob on.  Puzzle done?\n");
		    printf("solved=%d cells=%d\n",puz->nsolved, puz->ncells);
		    exit(1);
		}

		if (VP && VV) print_solution(stdout,puz,sol);
		if (VP)
		    printf("P: PROBE SEQUENCE COMPLETE - CHOSING (%d,%d)%d\n",
			besti, bestj, bestc);

		guess_cell(puz, sol, sol->line[0][besti][bestj], bestc);
		guesses++;
	    }
	    else
	    {
		/* Old guessing algorithm.  Use heuristics to make a guess */
		cell= pick_a_cell(puz, sol);
		if (cell == NULL)
		    return 0;

		c= pick_color(puz,sol,cell);
		if (VB)
		{
		    int k;
		    printf("B: GUESSING COLOR %d FOR CELL", c);
		    for (k= 0; k < puz->nset; k++)
			printf(" %d",cell->line[k]);
		    printf("\n");
		}

		guess_cell(puz, sol, cell, c);
		guesses++;
	    }
	}
	else
	{
	    /* We have hit a contradiction - try backtracking */
	    if (VB) printf("B: STUCK ON CONTRADICTION\n");

	    probing= 0;		/* If we were probing, we aren't any more */
	    guesses++;

	    /* Back up to last guess point, and invert that guess */
	    if (backtrack(puz,sol))
		/* Nothing to backtrack to - puzzle has no solution */
		return 0;
	    if (VB) print_solution(stdout,puz,sol);
	    if (VB) dump_history(stdout, puz, VV);
	}
    loop:;
    }
}
