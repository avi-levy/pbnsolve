/* Copyright (c) 2007, Jan Wolter, All Rights Reserved */

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
 */

Cell *pick_a_cell(Puzzle *puz, Solution *sol)
{
    int i, j, k, v, maxv;
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
		    if (V3) printf("MAX CELL %d,%d SCORE=%d/%f\n",
		    	i,j,maxv,minrate);
		    favcell= cell;
	    	}
	    }
	}
    }

    if (maxv == -1)
    {
    	if (V1) printf("Called pick-a-cell on complete puzzle\n");
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
    for (k= 0; k < puz->nset; k++)
    	add_job(puz, k, cell->line[k]);
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
	if (V1) printf("*** %s %d\n",CLUENAME(puz->type,dir), i);
	if (V2) dump_line(stdout,puz,sol,dir,i);

	if (!apply_lro(puz, sol, dir, i))
	{
	    /* Found a contradiction */
	    return 0;
	}

	if (V3)
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
    int i, j, c, nleft, neigh;

    /* One color puzzles are already solved */
    if (puz->ncolor < 2)
    {
    	puz->nsolved= puz->ncells;
	return 1;
    }

    while (1)
    {
	if (logic_solve(puz,sol))
	{
	    /* Logical reasoning hit a dead end but not a contradiction */
	    if (!maybacktrack || puz->nsolved == puz->ncells)
	        /* Puzzle is done, or no guessing is allowed */
		return 1;

	    if (V1) printf("STUCK\n");
	    if (V3) print_solution(stdout,puz,sol);

	    if (mayprobe)
	    {
		if (probing)
		{
		    nleft= puz->ncells - puz->nsolved;
		    if (V3) printf("PROBE ON (%d,%d)%d COMPLETE WITH "
		    	"%d CELLS LEFT\n", i,j,c,nleft);
		    if (nleft < bestnleft)
		    {
		    	bestnleft= nleft;
			besti= i;
			bestj= j;
			bestc= c;
		    }
		    if (V3) printf("UNDOING PROBE\n");
		    undo(puz,sol,0);
		    if (V3) dump_history(stdout, puz, 0);
		    ++c;
		}
		else
		{
		    if (V2) printf("STARTING PROBE SEQUENCE\n");
		    i= j= c= 0;
		    bestnleft= INT_MAX;
		    probing= 1;
		}

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
				    if (V2) printf("PROBING (%d,%d) COLOR %d\n",
				    	i,j,c);
				    probes++;
				    guess_cell(puz,sol,cell,c);
				    goto loop;
				}
			    }
			    c= 0;
			}
		    }
		    j= 0;
		}

		/* Probe completed - guess on best */
		probing= 0;
		if (V2) print_solution(stdout,puz,sol);
		if (V2) printf("PROBE SEQUENCE COMPLETE - CHOSING (%d,%d)%d\n",
			besti, bestj, bestc);
		guess_cell(puz, sol, sol->line[0][besti][bestj], bestc);
		guesses++;
	    }
	    else
	    {
		/* Make a guess, and proceed from there */
		cell= pick_a_cell(puz, sol);
		if (cell == NULL)
		    return 0;

		if (V1)
		{
		    int k;
		    printf("GUESSING COLOR %d FOR CELL", c);
		    for (k= 0; k < puz->nset; k++)
			printf(" %d",cell->line[k]);
		    printf("\n");
		}

		guess_cell(puz, sol, cell, pick_color(puz,sol,cell));
		guesses++;
	    }
	}
	else
	{
	    /* We have hit a contradiction - try backtracking */
	    if (V1) printf("STUCK ON CONTRADICTION\n");

	    probing= 0;		/* If we were probing, we aren't any more */
	    guesses++;

	    if (backtrack(puz,sol))
		/* Nothing to backtrack to - puzzle has no solution */
		return 0;
	    if (V2) print_solution(stdout,puz,sol);
	    if (V2) dump_history(stdout, puz, verbose > 2);
	}
    loop:;
    }
}
