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


/* SREAD_NONSTR() - Return the rest of the line.  Dropping leading and
 * tailing white space.  If the result is a quoted string, drop the quotes.
 * Returns the result in a static buffer, which is truncated if the string
 * exceeds MAXBUF characters.  Returns NULL if we hit EOF before reading
 * any non-space characters.
 */

char *sread_nonstr()
{
    static char buf[MAXBUF+1];
    int quote= 0, ch, i= 0;

    while ((ch= sgetc()) != EOF && ch != '\n' && isspace(ch))
    	;
    if (ch == EOF) return NULL;
    if (ch == '\'' || ch == '"') { quote= ch; ch= sgetc(); }

    while (ch != EOF && ch != '\n')
    {
	if (ch == quote)
	{
	    skiptoeol();
	    break;
	}

    	if (i < MAXBUF) buf[i++]= ch;

	ch= sgetc();
    }
    buf[i]= '\0';

    return buf;
}


/* Read a set of MK, NIN, or NON clues.  There is one line for each clue set,
 * which just contains the clue lengths.  Numbers on a line can be separated
 * by any number of spaces or commas.  Either a blank line or a line containing
 * just a zero will be treated as a blank clue.
 */

int read_bw_clues(Clue *clue, line_t nclue)
{
    line_t i, n;

    for (i= 0; i < nclue; i++)
    {
    	clue[i].n= 0;
	clue[i].s= 10;	/* Just a guess */
	clue[i].length= (line_t *)malloc(clue[i].s * sizeof(line_t));
	clue[i].color= (color_t *)malloc(clue[i].s * sizeof(color_t));
	clue[i].jobindex= -1;
#ifdef LINEWATCH
	clue[i].watch= 0;
#endif

	while ((n= sread_pint(1)) >= 0)
	{
	    /* Expand arrays if we need to */
	    if (clue[i].n >= clue[i].s)
	    {
		clue[i].s= clue[i].n + 10;
	    	clue[i].length= (line_t *)
		    realloc(clue[i].length, clue[i].s * sizeof(line_t));
	    	clue[i].color= (color_t *)
		    realloc(clue[i].color, clue[i].s * sizeof(color_t));
	    }
	    if (n == 0)
	    {
		if (clue[i].n > 0) return 1;
		skiptoeol();
		break;
	    }
	    clue[i].length[clue[i].n]= n;
	    clue[i].color[clue[i].n]= 1;
	    clue[i].n++;
	}
	if (n == -1 || (n == -2 && i != nclue - 1)) return 1;
    }
    return 0;
}


/* Create color definitions for a black and white puzzle */

void init_bw_colors(Puzzle *puz)
{
    /* Color table, including just black as 1 and white as 0 */
    puz->ncolor= puz->scolor= 2;
    puz->color= (ColorDef *)malloc(2*sizeof(ColorDef));
    puz->color[0].name= strdup("white");
    puz->color[0].rgb= strdup("ffffff");
    puz->color[0].ch= '.';
    puz->color[1].name= strdup("black");
    puz->color[1].rgb= strdup("000000");
    puz->color[1].ch= 'X';
}


/* Initialize Puzzle Data structure for a two-color grid puzzle  of the given
 * size.
 */

Puzzle *init_bw_puzzle()
{
    Puzzle *puz;

    puz= new_puzzle();

    /* Grid puzzle */
    puz->type= PT_GRID;
    puz->nset= 2;

    init_bw_colors(puz);
    
    return puz;
}

void init_clues(Puzzle *puz, int nrow, int ncol)
{

    /* Constuct top-level clue arrays */
    puz->n[D_ROW]= nrow;
    puz->n[D_COL]= ncol;
    puz->ncells= nrow*ncol;

    puz->clue[D_ROW]= (Clue *)malloc(nrow*sizeof(Clue));
    puz->clue[D_COL]= (Clue *)malloc(ncol*sizeof(Clue));
}


/* LOAD_MK_PUZZLE - load a puzzle in .mk format from the current source.
 */

Puzzle *load_mk_puzzle()
{
    Puzzle *puz;
    int nrow,ncol;
    char *badfmt= "Input is not in MK format, as expected\n";


    nrow= sread_pint(0);
    if (nrow == -2) fail("Input is empty\n");
    if (nrow <= 0) fail("load_mk: Could not read number of rows\n");

    ncol= sread_pint(0);
    if (ncol <= 0) fail("load_mk: Could not read number of columns\n");

    puz= init_bw_puzzle();
    init_clues(puz, nrow, ncol);

    skiptoeol();

    if (read_bw_clues(puz->clue[D_ROW], nrow)) fail(badfmt);

    if (skipwhite() != '#') fail(badfmt);
    skiptoeol();

    if (read_bw_clues(puz->clue[D_COL], ncol)) fail(badfmt);

    return puz;
}


/* LOAD_NIN_PUZZLE - load a puzzle in .nin format from the current source.
 */

Puzzle *load_nin_puzzle()
{
    Puzzle *puz;
    int nrow,ncol;
    char *badfmt= "Input is not in NIN format, as expected\n";

    ncol= sread_pint(0);
    if (ncol == -2) fail("Input is empty");
    if (ncol <= 0) fail(badfmt);

    nrow= sread_pint(0);
    if (nrow <= 0) fail(badfmt);

    puz= init_bw_puzzle();
    init_clues(puz, nrow, ncol);

    skiptoeol();

    if (read_bw_clues(puz->clue[D_ROW], nrow)) fail(badfmt);
    if (read_bw_clues(puz->clue[D_COL], ncol)) fail(badfmt);

    return puz;
}



/* LOAD_NON_PUZZLE - load a puzzle in .non format from the current source.
 */

Puzzle *load_non_puzzle()
{
    Puzzle *puz= init_bw_puzzle();
    SolutionList *sl, *lastsl= NULL;
    dir_t d;
    int n;
    char *badfmt= "Input is not in NON format, as expected\n";
    char *word, *arg;

    puz->n[D_ROW]= -1;
    puz->n[D_COL]= -1;

    while ((word= sread_keyword()) != NULL)
    {
    	if (!strcmp(word, "catalogue"))
	{
	    if (arg= sread_nonstr()) puz->id= strdup(arg);
	}
	else if (!strcmp(word, "title"))
	{
	    if (arg= sread_nonstr()) puz->title= strdup(arg);
	}
	else if (!strcmp(word, "by"))
	{
	    if (arg= sread_nonstr()) puz->author= strdup(arg);
	}
	else if (!strcmp(word, "copyright"))
	{
	    if (arg= sread_nonstr()) puz->copyright= strdup(arg);
	}
	else if (!strcmp(word, "width"))
	{
	    if ((puz->n[D_COL]= sread_pint(1)) <= 0) fail(badfmt);
	}
	else if (!strcmp(word, "height"))
	{
	    if ((puz->n[D_ROW]= sread_pint(1)) <= 0) fail(badfmt);
	}
	else if (!strcmp(word, "rows") || !strcmp(word, "columns"))
	{
	    d= (word[0] == 'r') ? D_ROW : D_COL;
	    if ((n= sread_pint(1)) == -2 || n == -1)
	    	fail(badfmt);
	    else if (n > 0)
	    {
	    	puz->n[d]= n;
		skiptoeol();
	    }

	    if (puz->n[d] < 0)
	    	fail("width and height lines must preceed rows and columns");

	    puz->clue[d]= (Clue *)malloc(puz->n[d]*sizeof(Clue));

	    if (read_bw_clues(puz->clue[d], puz->n[d]))
	    	fail(badfmt);
	}
	else if (!strcmp(word, "goal") || !strcmp(word, "saved"))
	{
	    sl= (SolutionList *)malloc(sizeof(SolutionList));
	    sl->type= (word[0] == 'g') ? STYPE_GOAL: STYPE_SAVED;
	    sl->id= NULL;
	    sl->note= NULL;
	    sl->next= NULL;
	    if (lastsl == NULL)
	    	puz->sol= sl;
	    else
	    	lastsl->next= sl;
	    lastsl= sl;
	
	     parse_grid(puz, &sl->s, "01", (word[0] == 'g') ? EOF : '?');
	     skiptoeol();
	}
	else
	{
	    skiptoeol();
	}
    }

    puz->ncells= puz->n[D_ROW] * puz->n[D_COL];

    return puz;
}



/* LOAD_LP_PUZZLE - load a puzzle in LP format from the current source.
 * Bosch's examples aren't consistent about what keywords they use and his
 * program ignores them and just assumes they will be in the right order.
 * We're sloppy enough to read his files, but not as sloppy as he is.
 */

Puzzle *load_lp_puzzle()
{
    Puzzle *puz= init_bw_puzzle();
    dir_t d;
    line_t i, j, n, c;
    char *badfmt= "Input is not in LP format, as expected\n";
    char *word;
    line_t nrow, ncol;
    Clue *clue;

    if ((word= sread_keyword()) == NULL) fail(badfmt);
    if (strcmp(word, "number_of_rows:")) fail(badfmt);
    if ((nrow= sread_pint(1)) <= 0) fail(badfmt);

    if ((word= sread_keyword()) == NULL) fail(badfmt);
    if (strcmp(word, "number_of_columns:")) fail(badfmt);
    if ((ncol= sread_pint(1)) <= 0) fail(badfmt);

    init_clues(puz, nrow, ncol);

    while ((word= sread_keyword()) != NULL)
    {
	if (!strncmp(word, "row_", 4))
	{
	    d= D_ROW;
	    i= atoi(word+4);
	}
	else if (!strncmp(word, "column_", 7))
	{
	    d= D_COL;
	    i= atoi(word+7);
	}
	if (i-- == 0) fail(badfmt);

	if ((word= sread_keyword()) == NULL) fail(badfmt);
	/*
	if (strcmp(word, "number_of_tiles:") &&
	    strcmp(word, "number_of_clusters:")) fail(badfmt);
	*/
	if ((n= sread_pint(1)) < 0) fail(badfmt);

	clue= puz->clue[d];
    	clue[i].n= n;
	clue[i].s= n;
	if (n > 0)
	{
	    clue[i].length= (line_t *)malloc(clue[i].s*sizeof(line_t));
	    clue[i].color= (color_t *)malloc(clue[i].s*sizeof(color_t));
	}
	clue[i].jobindex= -1;
#ifdef LINEWATCH
	clue[i].watch= 0;
#endif

	if ((word= sread_keyword()) == NULL) fail(badfmt);
	/*
	if (strcmp(word, "width(s):") && strcmp(word, "size(s):"))
	    fail(badfmt);
        */

	for (j= 0; j < n; j++)
	{
	    if ((c= sread_pint(1)) < 0) fail(badfmt);
	    clue[i].length[j]= c;
	    clue[i].color[j]= 1;
	}
    }

    return puz;
}
