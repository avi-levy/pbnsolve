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


/* CLUE_INIT - Do some generic initialization to the Clue data structures.
 *
 *  (1) Store line lengths in every clue.
 *  (2) Compute slack for all lines.
 */

void clue_init(Puzzle *puz, Solution *sol)
{
    dir_t k;
    line_t i, j;
    Clue *clue;
    line_t fill, spaces;
    extern int count_colors;

    puz->nsolved= 0;

    for (k= 0; k < puz->nset; k++)
    {
	for (i= 0; i < puz->n[k]; i++)
	{
	    clue= &puz->clue[k][i];

	    /* Store the line length */
	    if (puz->type == PT_GRID)
		clue->linelen= puz->n[1-k];
	    else
	    {
		/* This case will only ever be used if we support triddlers */
		Cell **cell= sol->line[k][i];
		for (clue->linelen= 0;
			cell[clue->linelen] != NULL;
			clue->linelen++)
		    ;
	    }

	    /* Create color count array, if we are using it */
	    if (count_colors)
		clue->colorcnt= (line_t *)calloc(puz->ncolor, sizeof(line_t));

	    /* Compute slack */
	    fill= 0;
	    spaces= 0;
	    for (j= 0; j < clue->n; j++)
	    {
		if (j > 0 && clue->color[j-1] == clue->color[j]) spaces++;
		fill+= clue->length[j];
	    }
	    clue->slack= (clue->linelen - fill - spaces);
	}
    }
}
