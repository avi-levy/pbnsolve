/* Copyright (c) 2007, Jan Wolter, All Rights Reserved */

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

/* SENSE_FMT - Guess the puzzle file format based on the first hunk of the
 * file.
 */

int sense_fmt()
{
#if 1
    /* For now, the only format we support is the XML format */
    return FF_XML;
#else
    int ch= skipwhite();

    if (ch == '<') return FF_XML;

    return FF_UNKNOWN;
#endif
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
    case FF_XML:
    	puz= load_xml_puzzle(index);
	break;

    case FF_UNKNOWN:
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

    puz= load_puzzle(fmt, index);

    fclose(srcfp);

    return puz;
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
