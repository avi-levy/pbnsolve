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

/* EXHAUSTIVE STRATEGY - This is a desparate last gasp to try before giving up
 * on logical solving.  It tries every cell in every color possible to it
 * and checks if it's row nd column has become insolvable.  This is a
 * cover-up for the inadequacies of the left-right-overlap linesolving
 * algorithm and would be completely useless if that didn't miss some things.
 * As it is, it serves to ensure that we don't start guessing unless
 * we really can't get further with logical solving, so it makes pbnsolve
 * more reliable at identifying line-solvable puzzles.  Whether it makes
 * pbnsolve faster is doubtful.
 */

#include "pbnsolve.h"

extern bit_type *oldval;

/* SCRATCHPAD - This is a two dimensional array of size (n x ncolor),
 * which stores information about a row or a column.  The array is initialized
 * to all zero. A zero means that it has not been shown that that cell can be
 * that color.  The macro PAD(p,i,c) references the value for cell i, color
 * c in pad p.
 */

#define PAD(p,i,c) p[(i)*puz->ncolor + (c)]


/* Given a solution, mark it into the given scratch pad.  This will be used to
 * avoid redundant checks in the future. i=row, j=column
 */

void mark_soln(Puzzle *puz, byte *p, line_t *soln, line_t i, line_t j, dir_t d)
{
    /* d=0=D_ROW i=row index j=col index  KEEP i fixed, move j */
    line_t jc= (d == D_ROW) ? i : j;
    Clue *clue= &(puz->clue[d][jc]);
    line_t ic, ir;

    /* Start our index into the line one past the cell we did the check
     * on.  We aren't interested in marking cells to the left (above) the
     * cell we checked, because we already checked those.  If d is D_ROW,
     * ir is a column index, otherwise it is row index.
     */
    ir= ((d == D_ROW) ? j : i) + 1;


    /* Loop through the clues */
    for (ic= 0; ic < clue->n; ic++)
    {
	/* Mark white space before the block */
	while (ir < soln[ic])
	    PAD(p,ir++,0)= 1;

	/* Mark colored cells in the block */
	while (ir < soln[ic] + clue->length[ic])
	    PAD(p,ir++,clue->color[ic])= 1;
    }

    /* Mark white space after the last block */
    while (ir < puz->n[1-d])
	PAD(p,ir++,0)= 1;
}

/* TRY_EVERYTHING - Implements the check all strategy.  The original version
 * Just tried setting every cell whose color had not been determined to each
 * of it's possible colors, and then for each color doing a left_solve() on
 * the row and column of the cell to see if there is a contradiction.  This
 * worked, but made poor use of the information coming back from the solver.
 * It gives you a whole line solution, which tells you possible colors for
 * more than just the one cell you called it on.  So now we use a scratch
 * pad array to keep track of all the possibilities that we accidentally
 * proved while checking previous cells, so we don't need to check those
 * possibilities again on future cells.
 *
 * If check is false, we assume that everything on the puzzle grid is OK and
 * we do not look for contradictions.  If check is true, we look for
 * errors and return a negative number if one is found.
 *
 * We return zero if we learned nothing, and a positive number if we set at
 * least one cell.
 */

int try_everything(Puzzle *puz, Solution *sol, int check)
{
    line_t i, j;
    color_t c, realn;
    dir_t k;
    line_t *soln;
    int hits= 0, setcell;
    Cell *cell;
    bit_type *realbit= (bit_type *) malloc(fbit_size * sizeof(bit_type));
    byte *rowpad, **colpad, *pad;
    Hist *h;

    exh_runs++;

    /* Make the scratch pads - one for current row, and one for each column */
    rowpad= (byte *)calloc(puz->n[D_COL] * puz->ncolor, 1);
    colpad= (byte **)malloc(puz->n[D_COL] * sizeof(byte *));
    for (j= 0; j < puz->n[D_COL]; j++)
    	colpad[j]= (byte *)calloc(puz->n[D_ROW], puz->ncolor);

    if (VE) printf("E: TRYING EVERYTHING check=%d\n",check);
    if (VE&&VV) print_solution(stdout, puz, sol);

    for (i= 0; i < sol->n[D_ROW]; i++)
    {
	/* Clear row pad, which we reuse for each row */
	if (i > 0) memset(rowpad, 0, puz->n[D_COL] * puz->ncolor);

    	for (j= 0; (cell= sol->line[0][i][j]) != NULL; j++)
	{
	    /* Not interested in solved cells */
	    if (cell->n == 1) continue;

	    /* Save current settings of cell */
	    fbit_cpy(realbit, cell->bit);
	    realn= cell->n;
	    setcell= 0;

	    /* Loop through possible colors */
	    for (c= 0; c < puz->ncolor; c++)
	    {
		/* Skip color already known impossible */
	    	if (!bit_test(realbit,c)) continue;

		if (VE&&VV)
		    printf("E: Trying (%d,%d)=%d\n", i,j, c);

		/* Temporarily set that cell to the color */
		cell->n= 1;
		fbit_setonly(cell->bit, c);

		/* Check all lines that cross the cell */
		for (k= 0; k < puz->nset; k++)
		{
		    pad= (k == D_ROW) ? rowpad : colpad[j];

		    /* If we already know that this cell being this color
		     * does not contradict the clue for this direction, skip
		     * ahead
		     */
		    if (PAD(pad,(k == D_ROW) ? j : i, c))
			continue;

		    if (!VL && VE && VV)
		    {
			printf("E: %s %d: ",
				CLUENAME(puz->type,k),cell->line[k]);
			dump_line(stdout,puz,sol,k,cell->line[k]);
		    }

		    soln= left_solve(puz,sol,k,cell->line[k], 0);
		    if (soln)
		    {
		    	/* It worked.  We learned nothing about our cell,
			 * but the solution we got back includes possible
			 * colors for some cells we still need to check.
			 * Mark them in the scratch pad.
			 */
			mark_soln(puz,pad,soln,i,j,k);
		    }
		    else
		    {
		    	/* Contradiction!  Eliminate that color possibility */
			if (VS||VE)
			    printf("%c: CELL (%d,%d) CAN'T BE COLOR %d\n",
				VS?'S':'E', i,j, c);

			/* If this was the last possible color for the cell,
			 * then we have hit a contradiction, and halt */
			if (realn == 1)
			{
			    if (VE) printf("E: Contradiction! Quitting.\n");
			    exh_cells+= hits;
			    return -1;
			}

			/* If this is the first change we've made to the cell
			 * put the old state on the history, or if we aren't
			 * keeping history, save the old state in oldval.
			 */
			if (setcell == 0 &&
			    !(h= add_hist2(puz, cell, realn, realbit, 0)) )
			    	fbit_cpy(oldval, realbit);

			setcell= 1;
			hits++;

			/* Modify our saved state, which will be put back into
			 * the cell later */
			bit_clear(realbit,c);
			realn--;

			/* Unless we are checking, halt when we are done to
			 * the last color.  Otherwise, we need to test that
			 * too.
			 */
			if (realn == 1)
			{
			    solved_a_cell(puz,cell,1);
			    if (!check) goto celldone;
			}
			break;	/* Don't check more directions on this cell */
		    }
		}
	    }
	    celldone:;

	    /* Restore the saved bits (possibly changed) to the cell */
	    fbit_cpy(cell->bit, realbit);
	    cell->n= realn;

	    /* If we changed anything, add crossing jobs to job list */
	    if (setcell > 0)
		add_jobs(puz, sol, -1, cell, 0, h ? h->bit : oldval);
	}
    }

    /* Discard the scratchpads */
    for (j= 0; j < puz->n[D_COL]; j++) free(colpad[j]);
    free(colpad);
    free(rowpad);

    exh_cells+= hits;

    return hits;
}
