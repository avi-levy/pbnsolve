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

/* These global variables are used during puzzle loading only.  If we are
 * loading from a file, srcfp is the file pointer.  If we are loading from
 * a memory image of the file, srcimg points to it.  Whichever we are not using
 * is NULL.  
 */

FILE *srcfp;
char *srcimg;
int srcptr;	/* next character to read from srcimg */
char *srcname;	/* Name of input, used in error messages */


/* SGETC() - Read a character from current source.  Returns EOF on end of file.
 */

int sgetc()
{
    if (srcfp != NULL)
    	return fgetc(srcfp);
    else if (srcimg != NULL && srcimg[srcptr] != '\0')
    	return srcimg[srcptr++];
    else
    	return EOF;
}


/* SUNGETC() - Return a character to the current input stream.  Returns EOF
 * on failure.
 */

int sungetc(int c)
{
    if (srcfp != NULL)
    	return ungetc(c,srcfp);
    else if (srcimg != NULL && srcptr > 0)
    	return srcimg[--srcptr]= c;
    else
    	return EOF;
}


/* SREWIND() - Rewind the current input stream.
 */

void srewind()
{
    if (srcfp != NULL)
    	rewind(srcfp);
    else
    	srcptr= 0;
}


/* SKIPWHITE() - Skip past a bunch of white space, and return the next
 * non-white space character.
 */

int skipwhite()
{
    int ch;

    while ((ch= sgetc()) != EOF && isspace(ch))
    	;
    return ch;
}


/* SKIPTOEOL() - discard charactors until we hit a newline or an EOF */

void skiptoeol()
{
    int ch;
    while ((ch= sgetc()) != EOF && ch != '\n')
    	;
}


/* SREAD_PINT() - Skip some white space, and then read in a non-negative
 * integer.  If newline is true, then we don't skip newlines, but rather
 * return -3 if there is a newline before the next digit.  In this case the
 * newline is NOT ungot.  If the next non-white space character is not a
 * digit, then -1 is returned and the character is ungot.  If there is no
 * next-non-whitespace character, then -2 is returned.
 */

int sread_pint(int newline)
{
    int ch, n= 0;

    /* Skip leading spaces and commas, but not newlines if newline is true */
    while ((ch= sgetc()) != EOF && (isspace(ch) || ch == ','))
	if (newline && ch == '\n') return -3;
    if (ch == EOF) return -2;
    if (!isdigit(ch)) return -1;

    /* Read in the number */
    do {
	if (ch == EOF) return -2;
    	n= n*10 + ch - '0';
    } while (isdigit(ch= sgetc()));
    if (ch != EOF) sungetc(ch);

    return n;
}


/* SREAD_KEYWORD() - Skip some white space and then read in an unquoted
 * sequence of characters, terminated by a space.  If the string is longer
 * than MAXBUF, read and discard the extra characters.  We return a
 * pointer into static memory or NULL if EOF was hit without finding a keyword
 */

char *sread_keyword()
{
    static char buf[MAXBUF+1];
    int i= 0;

    int ch= skipwhite();
    if (ch == EOF) return NULL;

    do {
    	if (i < MAXBUF) buf[i++]= ch;
    } while ((ch= sgetc()) != EOF && !isspace(ch));
    if (ch != EOF) sungetc(ch);
    buf[i]= '\0';

    return buf;
}

/* SENSE_FMT - Guess the puzzle file format based on the first hunk of the
 * file.  Currently this only works if it can do it based on the first
 * character.  We could tell many more apart if we read more of the file.
 */

int sense_fmt()
{
    int ch= skipwhite();

#ifndef NOXML
    if (ch == '<') return FF_XML;
#endif

    if (ch == 'P') return FF_PBM;
    
    if (isdigit(ch)) return FF_MK;

    if (ch == 'n') return FF_LP;

    return FF_UNKNOWN;
}


/* FMT_CODE - Given one of the format names in the list below, return the
 * format code, or FF_UNKNOWN if it doesn't match any.
 */

struct {
    char *str;
    int fmt;
    } fmtlist[]= {
#ifndef NOXML
	{"xml", FF_XML},
#endif
	{"mk", FF_MK},
	{"g", FF_OLSAK},
	{"nin", FF_NIN},
	{"non", FF_NON},
	{"pbm", FF_PBM},
	{"lp", FF_LP},
	{NULL, FF_UNKNOWN}
    };

int fmt_code(char *fmt)
{
    int i;
    for (i= 0; fmtlist[i].str != NULL; i++)
	if (!strcasecmp(fmt, fmtlist[i].str)) return fmtlist[i].fmt;
    return FF_UNKNOWN;
}


/* SUFFIX_FMT - Guess the format of a puzzle file based on the suffix on the
 * file name.
 */

int suffix_fmt(char *filename)
{
    char *suf= rindex(filename,'.');

    if (suf == NULL)
    	return FF_UNKNOWN;
    else
    	return fmt_code(suf+1);
}


/* LOAD_PUZZLE - load a puzzle from the current source.  Fmt is a FF_*
 * file type.  If it is FF_UNKNOWN, we guess the source type.  If the source
 * contains multiple puzzle, index tells which to load (1 is the first one).
 */

Puzzle *load_puzzle(int fmt, int index)
{
    Puzzle *puz;

    if (fmt == FF_UNKNOWN)
    {
    	fmt= sense_fmt();
	srewind();
    }

    switch (fmt)
    {
#ifndef NOXML
    case FF_XML:
    	puz= load_xml_puzzle(index);
	break;
#endif

    case FF_MK:
    	puz= load_mk_puzzle();
	break;

    case FF_NIN:
    	puz= load_nin_puzzle();
	break;

    case FF_NON:
    	puz= load_non_puzzle();
	break;

    case FF_PBM:
    	puz= load_pbm_puzzle();
	break;

    case FF_LP:
    	puz= load_lp_puzzle();
	break;

    case FF_OLSAK:
    	puz= load_g_puzzle();
	break;

    default:
	fail("Input format not recognized\n");
    }

    return puz;
}


/* LOAD_PUZZLE_FILE - load a puzzle from the given file.  Fmt is a FF_*
 * file type.  If it is FF_UNKNOWN, we guess the file type.  If the file
 * contains multiple puzzle, index tells which to load (1 is the first one).
 */

Puzzle *load_puzzle_file(char *filename, int fmt, int index)
{
    Puzzle *puz;

    if ((srcfp= fopen(filename,"r")) == NULL)
    	fail("Cannot open input file %s\n",filename);

    srcimg= NULL;
    srcname= filename;

    /* Try guessing file format from filename suffix */
    if (fmt == FF_UNKNOWN) fmt= suffix_fmt(filename);

    puz= load_puzzle(fmt, index);

    fclose(srcfp);

    return puz;
}


/* LOAD_PUZZLE_STDIN - load a puzzle from standard input.  Fmt is a FF_*
 * file type.  If it is FF_UNKNOWN, it defaults to FF_XML.  If the file
 * contains multiple puzzle, index tells which to load (1 is the first one).
 */

Puzzle *load_puzzle_stdin(int fmt, int index)
{
    srcfp= stdin;
    srcimg= NULL;
    srcname= "STDIN";

    if (fmt == FF_UNKNOWN)
#ifdef NOXML
	fmt= FF_NON;
#else
	fmt= FF_XML;
#endif

    return load_puzzle(fmt, index);
}


/* LOAD_PUZZLE_MEM - load a puzzle from a file image in memory.  Fmt is a
 * FF_* file type.  If it is FF_UNKNOWN, we guess the file type.  If the image
 * contains multiple puzzle, index tells which to load (1 is the first one).
 */

Puzzle *load_puzzle_mem(char *image, int fmt, int index)
{
    srcimg= image;
    srcfp= NULL;
    srcname= "INPUT";

    return load_puzzle(fmt, index);
}
