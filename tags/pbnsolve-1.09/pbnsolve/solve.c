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
    solved_a_cell(puz,cell, 1);

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
    int sprint_clock= 0, plod_clock= PLOD_INIT;

    /* One color puzzles are already solved */
    if (puz->ncolor < 2)
    {
    	puz->nsolved= puz->ncells;
	return 1;
    }

    /* Start bookkeeping, if we need it */
    if (mayprobe)
	probe_init(puz,sol);
    else
	bookkeeping_on(puz,sol);

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

	    if (mayprobe && (!mayguess || sprint_clock <= 0))
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
		
		/* If a lot of probes have not been finding contradictions,
		 * consider triggering sprint mode
		 */
		if (mayguess && --plod_clock <= 0)
		{
		    float rate= probe_rate();
		    if (rate >= .12)
		    {
			/* More than 10% have failed to find contradiction,
			 * so try heuristic searching for a while */
			bookkeeping_on(puz,sol);
			sprint_clock= SPRINT_LENGTH;
			nsprint++;
			/*printf("STARTING SPRINT - probe rate=%.4f\n",rate);*/
		    }
		    else
		    {
			/* Success rate of probing is still high. Keep on
			 * trucking. */
			plod_clock= PLOD_LENGTH;
			nplod++;
			/*printf("CONTINUING PLOD - probe rate=%.4f\n", rate);*/
		    }
		}
	    }
	    else
	    {
		/* Old guessing algorithm.  Use heuristics to make a guess */
		cell= pick_a_cell(puz, sol);
		if (cell == NULL)
		    return 0;

		bestc= (*pick_color)(puz,sol,cell);
		if (VA || WC(cell->line[0],cell->line[1]))
		{
		    printf("A: GUESSING SELECTED ");
		    print_coord(stdout,puz,cell);
		    printf(" COLOR %d\n",bestc);
		}

		if (mayprobe && --sprint_clock <= 0)
		{
		    /* If we have reached the end of our sprint, try plodding
		     * again.  */
		    probe_init(puz,sol);
		    plod_clock= PLOD_LENGTH;
		    nplod++;
		    /*printf("ENDING SPRINT\n");*/
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
