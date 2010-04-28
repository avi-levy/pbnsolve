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
#include "read.h"

/* Skip white space, and also any comments which start with '#' and continue
 * the next newline.
 */

void skipcomments()
{
    int ch;
    int incomment= 0;

    while ((ch= sgetc()) != EOF)
    	if (incomment)
	{
	    if (ch == '\n') incomment= 0;
	}
	else
	{
	    if (ch == '#')
	    	incomment= 1;
	    else if (!isspace(ch))
	    {
		sungetc(ch);
	    	break;
	    }
	}
}


/* PARSE_GRID - This is used to parse a solution grid from our current input
 * stream.  The size of the grid is presumed to be already known and
 * stored in puz->n[D_ROW] and puz->n[D_COL] and the number of colors is in
 * puz->ncolor.  The 'colorchar' string is an array of puz->ncolor characters
 * used in the grid to encode the different colors.  If there are any
 * characters not in 'colorchar' that appear in 'grid' (like white space), then
 * they are ignored.  If the 'grid' string contains fewer than
 * puz->n[D_ROW] * puz->n[D_COL] color characters, it is padded out with white.
 * If it is longer it is truncated.  The resulting grid is stored in 'sol',
 * which should have been allocated but can be entirely uninitialized.  If
 * 'unknown' is a character, then that character indicates an "unknown" cell.
 * If it is EOF then no unknown cells are allowed.
 */

void parse_grid(Puzzle *puz, Solution *sol, char *colorchar, int unknown)
{
    int i,j, ch;
    color_t color;
    char *p;
    Cell *cell;

    sol->n[D_ROW]= puz->n[D_ROW];
    sol->n[D_COL]= puz->n[D_COL];

    init_solution(puz, sol, 0);

    for (i= 0; i < sol->n[D_ROW]; i++)
	for (j= 0; j < sol->n[D_COL]; j++)
	{
	    while ((ch= sgetc()) != EOF &&
	           (p= index(colorchar, ch)) == NULL &&
		   ch != unknown)
		;
	    cell= sol->line[D_ROW][i][j];
	    if (ch == unknown)
	    {
		for (color= 0; color < puz->ncolor; color++)
		    bit_set(cell->bit, color);
		cell->n= puz->ncolor;
	    }
	    else
	    {
		bit_set(cell->bit, (ch == EOF) ? 0 : p - colorchar);
		cell->n= 1;
	    }
	}
}

/* LOAD_PBM_PUZZLE - load a plain portable bitmap file as a puzzle.  This can
 * read either plain PBM or raw PBM files.  Raw PBM files can actually contain
 * multiple images, so ideally we would pass an index into this to select which
 * image to load, but that is not implemented.  Probably we should use
 * libnetpbm and handle all types of netpbm files.
 */

Puzzle *load_pbm_puzzle()
{
    Puzzle *puz;
    Solution *sol;
    int i, j, ch;
    Cell *cell;
    int nrow,ncol;
    int ver;
    char *badfmt= "Input is not in PBM format, as expected\n";

    if (sgetc() != 'P') fail(badfmt);
    if ((ver= sgetc()) != '1' && ver != '4') fail(badfmt);

    skipcomments();

    ncol= sread_pint(0);
    if (ncol <= 0) fail("load_pbm: Could not read width\n");

    nrow= sread_pint(0);
    if (ncol <= 0) fail("load_pbm: Could not read height\n");

    puz= init_bw_puzzle();

    /* Construct a "goal" entry in the solution list */
    puz->sol= (SolutionList *)malloc(sizeof(SolutionList));
    puz->sol->id= NULL;
    puz->sol->type= STYPE_GOAL;
    puz->sol->note= NULL;
    puz->sol->next= NULL;

    sol= &puz->sol->s;
    sol->n[D_ROW]= nrow;
    sol->n[D_COL]= ncol;
    puz->ncells= nrow*ncol;

    /* Build the grid data structure, initiallizing all cells to no-color */
    init_solution(puz, sol, 0);

    if (ver == '1')
    {
    	/* Plain PBM file - just nrow*ncol 1's and 0's */
	for (i= 0; i < sol->n[D_ROW]; i++)
	    for (j= 0; j < sol->n[D_COL]; j++)
	    {
		ch= skipwhite();
		cell= sol->line[D_ROW][i][j];
		bit_set(cell->bit, ch != EOF && ch != '0');
		cell->n= 1;
	    }
    }
    else
    {
    	/* Raw PBM file */
	int b;

	for (i= 0; i < sol->n[D_ROW]; i++)
	{
	    j= 0;
	    while (j < sol->n[D_COL])
	    {
		ch= sgetc();
		if (ch == EOF) ch= 0;

		for (b= 0; b < 8; b++)
		{
		    cell= sol->line[D_ROW][i][j];
		    bit_set(cell->bit, (ch & 0x80) != 0);
		    cell->n= 1;

		    ch= ch << 1;
		    if (++j >= sol->n[D_COL]) break;
		}
	    }
	}
    }

    /* Generate the clues from the image */
    make_clues(puz, sol);

    return puz;
}
