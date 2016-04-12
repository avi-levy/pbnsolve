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


/* A reader for the Olsak .g file format.  Fairly hideous, but it supports
 * multicolor puzzles.
 */

#include "pbnsolve.h"
#include "read.h"

/* Max different colors that we'll allow in an Olsak  G file */
#define MAXCOLOR 100

Puzzle *load_g_puzzle()
{
    Puzzle *puz= new_puzzle();
    char inchar[MAXCOLOR];
    int ch, bgcolor, dfltcolor= 0, outch, cn;
    char *wordxpm, *name, *rgb, *cp;
    ColorDef *c;
    Clue *clue;
    dir_t dir;
    line_t i, n, sclue;

    puz->type= PT_GRID;
    puz->nset= 2;

    /* Look for a line starting with a # or a : */
    while ((ch =sgetc()) != ':')
    {
    	if (ch == EOF)
	    fail("Input is not in Olsak G format, as expected\n");
	else if (ch == '#')
	{
	    ch= sgetc();
	    if (ch == 'd' || ch == 'D')
	    {
		/* Found a '#d' line, which starts color declarations */
		skiptoeol();

		/* Add the default background color */
		bgcolor= add_color(puz,"white","ffffff",'.');
		inchar[0]= ' ';

		/* Each line defines a color until a line starting with : */
		while ((ch= skipwhite()) != ':' && ch != EOF)
		{
		    /* Read in inchar, outchar and name or rgb */
		    if (sgetc() != ':')
			fail("Bad color declaration\n");
		    outch= sgetc();
		    if ((wordxpm= sread_keyword()) == NULL)
			fail("Bad color declaration\n");
		    skiptoeol();
		    if (wordxpm[0] == '#')
			{ rgb= wordxpm+1, name= NULL; }
		    else
			{ rgb= NULL, name= wordxpm; }

		    if (ch == '0')
		    {
			/* redefinition of background color */
			c= &puz->color[bgcolor];
			if (c->rgb) free(c->rgb);
			c->rgb= safedup(rgb);
			if (c->name) free(c->name);
			c->name= safedup(name);
			c->ch= outch;
		    }
		    else
		    {
			cn= new_color(puz);
			if (cn >= MAXCOLOR)
			    fail("Cannot read files with more than %d colors\n",
				    MAXCOLOR);
			if (ch == '1') dfltcolor= cn;
			inchar[cn]= ch;
			c= &puz->color[cn];
			c->name= safedup(name);
			c->rgb= safedup(rgb);
			c->ch= outch;
		    }
		}
		/* Terminate input character array */
		inchar[puz->ncolor]= '\0';
		break;
	    }
	    else if (ch == 't' || ch == 'T')
	    {
	    	fail("Triddlers not yet supported by pbnsolve\n");
	    }
	}
	else if (ch != '\n')
	    skiptoeol();
    }

    /* Found a line starting with a colon.  This is the start of a clue set */
    skiptoeol();

    /* If we never saw a color declaration, then this is a black and white
     * puzzle.
     */
    if (puz->color == NULL)
    {
	init_bw_colors(puz);
	dfltcolor= 1;
    }

    /* Start reading the clues */
    for (dir= 0; dir < 2; dir++)
    {
	sclue= 20;
	puz->clue[dir]= (Clue *)malloc(sizeof(Clue) * sclue);

	i= 0;
	while ((ch= sgetc()) != ':' && ch != EOF)
	{
	    sungetc(ch);

	    /* Enlarge clue array, if need be */
	    if (i >= sclue)
	    {
		sclue+= 10;
	    	puz->clue[dir]= (Clue *)realloc(puz->clue[dir],
			sizeof(Clue) * sclue);
	    }
	    clue= &puz->clue[dir][i];

	    /* Initialized the clue */
	    clue->n= 0;
	    clue->s= 10;	/* Just a guess */
	    clue->length= (line_t *)malloc(clue->s * sizeof(line_t));
	    clue->color= (color_t *)malloc(clue->s * sizeof(color_t));
	    clue->jobindex= -1;
#ifdef LINEWATCH
	    clue->watch= 0;
#endif
	    while ((n= sread_pint(1)) >= 0)
	    {
		/* Get color character */
		if (isspace(ch= sgetc()))
		{
		    /* None.  Use default color */
		    if (dfltcolor == 0)
		    	fail("No color specifier on clue,"
				" and no default color");
		    sungetc(ch);
		    cn= dfltcolor;
		}
		else if ((cp= index(inchar, ch)) != NULL)
		{
		    cn= cp - inchar;
		}
		else
		    fail("Unknown color specifier on clue\n");

	    	/* Expand arrays if we need to */
		if (clue->n >= clue->s)
		{
		    clue->s= clue->n + 10;
		    clue->length= (line_t *)
		    	realloc(clue->length, clue->s * sizeof(line_t));
		    clue->color= (color_t *)
		    	realloc(clue->color, clue->s * sizeof(color_t));
		}
		if (n == 0)
		    fail("Zero clue in input file\n");

		/* Store the clue number and color */
		clue->length[clue->n]= n;
		clue->color[clue->n]= cn;
		clue->n++;
	    }
	    if (n == -1)
	    {
	    	fail("Bad clue number in %s %d - "
		   "Cannot handle left-glue characters\n",
		   cluename(PT_GRID,dir),i);
	    }

	    i++;
	}

	/* Save the number of clues */
	puz->n[dir]= i;
	skiptoeol();
    }

    return puz;
}
