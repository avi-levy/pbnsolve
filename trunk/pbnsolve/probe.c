/* Copyright 2009 Jan Wolter
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

#ifndef NO_VQ
long nprobe= 0;
#endif

/* SCRATCHPAD - An array of bitstrings for every cell.  Every color that is
 * set for a cell in the course of the current probe sequence is ORed into it.
 * Any setting which has been part of a previous probe will not be probed on,
 * because the consequences of that can only be a subset of the consequences
 * of the previous probe.
 */

bit_type *probepad= NULL;
int probing= 0;

/* Create or clear the probe pad */
void init_probepad(Puzzle *puz)
{
    if (!probepad)
	probepad= (bit_type *)
	    calloc(puz->ncells, fbit_size * sizeof(bit_type));
    else
    	memset(probepad, 0, puz->ncells * fbit_size * sizeof(bit_type));
}


/* Search energetically for the guess that lets us make the most progress
 * toward solving the puzzle, by trying lots of guesses and search on each
 * until it stalls.
 *
 * Normally it returns 0, with besti,bestj,bestc containing our favorite guess.
 *
 * If we accidentally solve the puzzle when we were just trying to probe,
 * return 1.
 *
 * If we discover a logically necessary cell, then we set it, add jobs to the
 * job list, and return -1.
 */

int probe(Puzzle *puz, Solution *sol,
    line_t *besti, line_t *bestj, color_t *bestc)
{
    line_t i, j;
    color_t c;
    Cell *cell;
    int rc, neigh;
    int nleft, bestnleft= INT_MAX;

    /* Starting a new probe sequence - initialize stuff */
    if (VP) printf("P: STARTING PROBE SEQUENCE\n");
    init_probepad(puz);
    probing= 1;
#ifndef NO_VQ
    nprobe++;
#endif

    for (i= 0; i < sol->n[0]; i++)
    {
	for (j= 0; (cell= sol->line[0][i][j]) != NULL; j++)
	{
	    if (cell->n < 2) continue;
	    neigh= count_neighbors(sol, i, j);
	    if (neigh < 2) continue;
	    merging= mergeprobe;

	    for (c= 0; c < puz->ncolor; c++)
	    {
		if (may_be(cell, c))
		{
		    if (bit_test(propad(cell),c))
		    {
			/* We can skip this probe because it was a consequence
			 * of a previous probe.  However, if we do that, then
			 * we can't do merging on this cell.
			 */
		    	if (merging) merge_cancel();
		    }
		    else
		    {
			/* Found a cell - go probe on it */
			if (VP || VB || WC(i,j))
			    printf("P: PROBING (%d,%d) COLOR %d\n", i,j,c);
			probes++;

			if (merging) merge_guess();

			guess_cell(puz,sol,cell,c);
			rc= logic_solve(puz, sol, 0);

			if (rc == 0)
			{
			    /* Probe complete - save it's rating and undo it */
			    nleft= puz->ncells - puz->nsolved;
			    if (VQ)
				printf("P: PROBE #%d ON (%d,%d)%d COMPLETE "
				    "WITH %d CELLS LEFT\n", nprobe,i,j,c,nleft);
			    else if (VP || WC(i,j))
				printf("P: PROBE ON (%d,%d)%d COMPLETE "
				    "WITH %d CELLS LEFT\n", i,j,c,nleft);
			    if (nleft < bestnleft)
			    {
				bestnleft= nleft;
				*besti= i;
				*bestj= j;
				*bestc= c;
			    }
			    if (VP)
				printf("P: UNDOING PROBE\n");

			    undo(puz, sol, 0);
			}
			else if (rc < 0)
			{
			    /* Found a contradiction - what luck! */
			    if (VP)
				printf("P: PROBE ON (%d,%d)%d "
					"HIT CONTRADICTION\n", i,j,c);
			    
			    if (merging) merge_cancel();
			    guesses++;

			    /* Backtrack to the guess point, invert that */
			    if (backtrack(puz, sol))
			    {
				/* Nothing to backtrack to.  This should never
				 * happen, because we made a guess a few lines
				 * ago.
				 */
				printf("ERROR: "
				    "Could not backtrack after probe\n");
				exit(1);
			    }
			    if (VP)
			    {
				print_solution(stdout,puz,sol);
				dump_history(stdout, puz, VV);
			    }
			    probing= 0;
			    return -1;
			}
			else
			{
			    /* by wild luck, we solved it */
			    if (merging) merge_cancel();
			    probing= 0;
			    return 1;
			}
		    }
		}
	    }

	    /* Finished all probes on a cell.  Check if there is anything that
	     * was a consequence of all alternatives.  If so, set that as a
	     * fact, cancel probing and proceed.
	     */

	    if (merging && merge_check(puz, sol))
	    {
	    	merges++;
		probing= 0;
		return -1;
	    }

	}
    }
    stop:;

    /* completed probing all cells - select best as our guess */
    if (bestnleft == INT_MAX)
    {
    	print_solution(stdout,puz,sol);
	printf("ERROR: found no cells to prob on.  Puzzle done?\n");
	printf("solved=%d cells=%d\n",puz->nsolved, puz->ncells);
	exit(1);
    }

    if (VP && VV) print_solution(stdout,puz,sol);
    if (VP || WC(*besti,*bestj))
	printf("P: PROBE SEQUENCE COMPLETE - CHOSING (%d,%d)%d\n",
	    *besti, *bestj, *bestc);

    probing= 0;
    return 0;
}
