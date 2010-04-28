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
#include "read.h"

/* NEW_PUZZLE - Allocate an empty puzzle, initializing everything to suitable
 * null values
 */

Puzzle *new_puzzle()
{
    Puzzle *puz= (Puzzle *)calloc(1, sizeof(Puzzle));
    return puz;
}


/* FREE_PUZZLE - Gets you a puzzle you don't have to pay for.  No, wait,
 * this just deallocates memory associated with a puzzle.
 */

void free_puzzle(Puzzle *puz)
{
    line_t i,k;
    color_t c;
    SolutionList *sl, *nsl;

    safefree(puz->source);
    safefree(puz->id);
    safefree(puz->title);
    safefree(puz->seriestitle);
    safefree(puz->author);
    safefree(puz->copyright);

    for (c= 0; c < puz->ncolor; c++)
    {
    	safefree(puz->color[c].name);
    	safefree(puz->color[c].rgb);
    }
    safefree(puz->color);

    for (k= 0; k < puz->nset; k++)
    {
	for (i= 0; i < puz->n[k]; i++)
	{
	    safefree(puz->clue[k][i].length);
	    safefree(puz->clue[k][i].color);
	}
	safefree(puz->clue[k]);
    }

    for (sl= puz->sol; sl != NULL; sl= nsl)
    {
    	nsl= sl->next;
	free_solution_list(sl);
    }

    free(puz);
}


/* FIND_COLOR - Given a color name, find it in the current color definitions.
 * Return it's index if found or -1 if not.
 */

color_t find_color(Puzzle *puz, char *name)
{
    color_t i;

    for (i= 0; i < puz->ncolor; i++)
    	if (!strcasecmp(name, puz->color[i].name))
	    return i;

    return -1;
}


/* FIND_COLOR_CHAR - Given a color character, find it in the current color
 * definitions.  Return it's index if found or -1 if not.
 */

color_t find_color_char(Puzzle *puz, char ch)
{
    color_t i;

    for (i= 0; i < puz->ncolor; i++)
    	if (ch == puz->color[i].ch)
	    return i;

    return -1;
}

/* NEW_COLOR - Create a new entry in the color table, and return it's index.
 * Doesn't initialize the new entry at all.
 */

color_t new_color(Puzzle *puz)
{
    /* If necessary, enlarge the color array */
    if (puz->ncolor >= puz->scolor)
    {
	puz->scolor+= 3;
	puz->color= (ColorDef *)realloc(puz->color,
	    puz->scolor * sizeof(ColorDef));
    }

    return puz->ncolor++;
}


/* FIND_OR_ADD_COLOR - Find the named color in the color definition list.
 * If it is not there, add it with rgb and character codes undefined.
 * Return the index of the color.
 */

color_t find_or_add_color(Puzzle *puz, char *name)
{
    color_t i;
    ColorDef *c;

    if ((i= find_color(puz,name)) >= 0)
	return i;

    /* If necessary, enlarge the color array */
    i= new_color(puz);
    c= &puz->color[i];
    c->name= strdup(name);
    c->rgb= NULL;
    c->ch= '\0';

    return i;
}


/* ADD_COLOR - Add a color to a puzzle.  If the colorname is already in the
 * puzzle, just update the definitions.  If values are not known, rgb can be
 * NULL and ch can be '\0'.  Returns the index of the new color.
 */

int add_color(Puzzle *puz, char *name, char *rgb, char ch)
{
    color_t i= find_or_add_color(puz,name);
    ColorDef *c= &puz->color[i];

    if (c->rgb != NULL) free(c->rgb);
    c->rgb= safedup(rgb);
    c->ch= ch;
    return i;
}
