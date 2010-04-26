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

/* MAKE_CLUES - Given a goal solution, generate corresponding puzzle clues.
 */

void append_clue(Clue *clue, line_t length, color_t color)
{
    if (clue->n >= clue->s)
    {
    	clue->s += 8;
	clue->length=
	    (line_t *) realloc(clue->length, clue->s * sizeof(line_t));
	clue->color=
	    (color_t *) realloc(clue->color, clue->s * sizeof(color_t));
    }
    clue->length[clue->n]= length;
    clue->color[clue->n]= color;
    clue->n++;
}


void make_clues(Puzzle *puz, Solution *sol)
{
    Clue *clue;
    Cell *cell;
    dir_t k;
    line_t i, j;
    color_t c;
    color_t color, newcolor;
    line_t count;

    if (puz->type != PT_GRID)
    	fail("Don't know how to generate clues for non-grid puzzles\n");

    for (k= 0; k < puz->nset; k++)
    {
	/* Allocate array for this direction */
	puz->n[k]= sol->n[k];
	puz->clue[k]= (Clue *)malloc(puz->n[k] * sizeof(Clue));

	for (i= 0; i < puz->n[k]; i++)
	{
	    clue= &puz->clue[k][i];

	    /* Allocate clue arrays for this line */
	    clue->n= 0;
	    clue->s= 8;		/* Initial size - may resize later */
	    clue->length= (line_t *) malloc(clue->s * sizeof(line_t));
	    clue->color= (color_t *) malloc(clue->s * sizeof(color_t));
	    clue->jobindex= -1;
#ifdef LINEWATCH
	    clue->watch= 0;
#endif

	    /* Scan the line of the solution - this scans one past EOL */
	    color= 0;
	    j= 0;
	    do {
		cell= sol->line[k][i][j];

		/* Get color of current cell */
		newcolor= 0;
		if (cell != NULL)
		    for (c= 0; c < puz->ncolor; c++)
		    	if (bit_test(cell->bit,c))
			{
			    newcolor= c;
			    break;
			}
		if (color > 0)
		{
		    if (newcolor != color)
		    {
		    	append_clue(clue, count, color);
			color= newcolor;
			count= 1;
		    }
		    else
		    	count++;
		}
		else if (newcolor > 0)
		{
		    count= 1;
		    color= newcolor;
		}

		j++;
	    } while (cell != NULL);	/* Testing previous cell! */
	}
    }
}
