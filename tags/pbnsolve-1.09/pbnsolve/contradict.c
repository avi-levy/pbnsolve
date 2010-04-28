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

/* Do a depth-limited search for contradictions on every vaguely interesting
 * cell on the grid.  If one is found, set that cells, put jobs for them on
 * the job list, and return -1, unless we should happen to complete the
 * puzzle, in which case we return 1.  If no contradictions are found return
 * zero.
 */

int contradict(Puzzle *puz, Solution *sol)
{
    line_t i,j, nlast;
    static line_t n= -1;
    color_t c;
    Cell *cell;
    int rc;

    if (VC)
    	printf("C: **** STARTING CONTRADICTION SEARCH ****\n");

    if (sol->spiral == NULL)
    	make_spiral(sol);

    /* Point nlast to the last cell we'd process if we were going to proces
     * all cells.  If we've run before, then that is whatever cell we
     * processed on the last call.  We'll then start processing with that
     * cells successor.
     */
    nlast= ((n == -1) ? puz->ncells-1 : n);
    if (sol->spiral[++n] == NULL) n= 0;

    while(n != nlast)
    {
	cell= sol->spiral[n];

	if (cell->n < 2) goto next;

	i= cell->line[D_ROW];
	j= cell->line[D_COL];

	if (count_neighbors(sol, i, j) < 1) goto next;

	for (c= 0; c < puz->ncolor; c++)
	{
	    if (may_be(cell, c))
	    {
		/* Found a cell - go do contradiction on it */
		if (VC || VB || WC(i,j))
		    printf("C: ==== TRYING (%d,%d) COLOR %d ====\n", i,j,c);
		contratests++;

		guess_cell(puz,sol,cell,c);
		rc= logic_solve(puz, sol, 1);

		if (rc > 0 && !checkunique)
		{
		    /* by wild luck, we solved it.  Note that we only quit
		     * at this point if we aren't in checkunique mode,
		     * because, although we've found a solution, we found
		     * it by a guess.  We are still hoping we'll be able
		     * to prove the puzzle solvable without guessing.
		     */
		    return 1;
		}
		else if (rc >= 0)
		{
		    /* No contradiction found - learned nothing - undo it */
		    if (VC)
			printf("C: NO CONTRADICTION ON (%d,%d)%d\n",i,j,c);
		    undo(puz,sol,0);
		}
		else if (rc < 0)
		{
		    /* Found a contradiction - yippee! */
		    contrafound++;
		    if (VC)
			printf("C: CONTRADICTION ON (%d,%d)%d\n",i,j,c);
		    
		    /* Backtrack to the guess point, invert that */
		    if (backtrack(puz,sol))
		    {
			/* There should always be a guess to backtrack to,
			 * since we just made one a couple a lines back */
			fprintf(stderr, "Failed to Backtrack after finding"
			    " a contradiction\n");
			exit(1);
		    }
		    if (VB)
		    {
			print_solution(stdout,puz,sol);
			dump_history(stdout, puz, VV);
		    }
		    return -1;
		}
	    }
	}
	next:
	if (sol->spiral[++n] == NULL) n= 0;
    }

    return 0;
}
