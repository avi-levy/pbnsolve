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
    int i,k;
    SolutionList *sl, *nsl;

    safefree(puz->source);
    safefree(puz->id);
    safefree(puz->title);
    safefree(puz->seriestitle);
    safefree(puz->author);
    safefree(puz->copyright);

    for (i= 0; i < puz->ncolor; i++)
    {
    	safefree(puz->color[i].name);
    	safefree(puz->color[i].rgb);
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

int find_color(Puzzle *puz, char *name)
{
    int i;

    for (i= 0; i < puz->ncolor; i++)
    	if (!strcasecmp(name, puz->color[i].name))
	    return i;

    return -1;
}


/* FIND_COLOR_CHAR - Given a color character, find it in the current color
 * definitions.  Return it's index if found or -1 if not.
 */

int find_color_char(Puzzle *puz, char ch)
{
    int i;

    for (i= 0; i < puz->ncolor; i++)
    	if (ch == puz->color[i].ch)
	    return i;

    return -1;
}


/* FIND_OR_ADD_COLOR - Find the named color in the color definition list.
 * If it is not there, add it with rgb and character codes undefined.
 * Return the index of the color.
 */

int find_or_add_color(Puzzle *puz, char *name)
{
    int i;
    ColorDef *c;

    if ((i= find_color(puz,name)) >= 0)
	return i;

    /* If necessary, enlarge the color array */
    if (puz->ncolor >= puz->scolor)
    {
	puz->scolor+= 3;
	puz->color= (ColorDef *)realloc(puz->color,
	    puz->scolor * sizeof(ColorDef));
    }

    c= &puz->color[i= puz->ncolor++];
    c->name= strdup(name);
    c->rgb= NULL;
    c->ch= '\0';

    return i;
}


/* ADD_COLOR - Add a color to a puzzle.  If the colorname is already in the
 * puzzle, just update the definitions.  If values are not known, rgb can be
 * NULL and ch can be '\0'.
 */

void add_color(Puzzle *puz, char *name, char *rgb, char ch)
{
    int i= find_or_add_color(puz,name);
    ColorDef *c= &puz->color[i];

    if (c->rgb != NULL) free(c->rgb);
    c->rgb= safedup(rgb);
    c->ch= ch;
}
