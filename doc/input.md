Input Formats:
--------------

Pbnsolve can read several input formats.  Some input formats include only
the clues, not the goal image, and some include only the goal image but not
the clues, and some can contain both.  Pbnsolve will construct clues from a
goal image if the clues are not given.

Note that the -c option (which is always on in CGI mode) requires a goal
image, so only formats that can include a goal image can be used with that.
Uniqueness checking on other formats needs to be done with the -u option.

Some formats can include saved solutions.  These are images of incompletely
solved puzzles which pbnsolve can take as a starting point instead of starting
with a blank grid.

Recognized formats are listed below:

  PBNSOLVE XML FORMAT
    Suffix:  "xml"
    Contains:  clues, goal, saved
    Color:  any number of colors
    Documentation: http://webpbn.com/pbn_fmt.html

    This is our native format, but if libxml2 is not available and you built
    pbnsolve with the NOXML flag, then it isn't available and pbnsolve is
    crippled because none of the other formats currently available support
    all of pbnsolve's features.

  STEVE SIMPSON'S .NON FORMAT (EXTENDED)
    Suffix:  "non"
    Contains:  clues, goal, saved
    Color:  black & white only
    Documentation: http://www.comp.lancs.ac.uk/~ss/nonogram/fmt2

    Pbnsolve is slightly incompatible with the spec in that it will treat a
    blank line in the clues as a blank clue.  The official way to mark a blank
    clue is with a '0'.  Pbnsolve accepts those as blank clues too.

    The official format allows arbitrary additional properties to be defined,
    and we use two of these for goal and saved puzzle images.

    The "goal" keyword is followed by a image of the puzzle goal, row-by-row
    with 1's for blacks and 0's for whites.  Any other characters (usually
    whitespace) are ignored.  If prefer human readability to adherence to
    Simpson's spec, you could enter the grid for a 5x10 puzzle like:

        goal
  01100
  01101
  00101
  01110
  10100
  10100
  00110
  01010
  01011
  11000

    or you can use the less readable, but more compliant format of:

        goal 01100011010010101110101001010000110010100101111000
    or 
        goal "01100011010010101110101001010000110010100101111000"

    Saved grids can be entered in the same way, using the keyword "saved"
    instead of "goal" and with '?' characters marking cells that are unknown,
    '0' for white and '1' for black.  There can be multiple saved solutions
    in a file.  The -s flag can be used to select one to start from.

    These files lack any sort of "magic number" at the beginning, and so
    pbnsolve is not generally able to identify them by sniffing the file
    content.

    This is the best option to use if you can't use the xml format, but
    it doesn't support color puzzles.

  OLSAK .MK FORMAT
    Suffix:  "mk"
    Contains:  clues
    Color:  black & white only

    This is a very simple format.  Here's an example:

  10 5
  2
  2 1
  1 1
  3
  1 1
  1 1
  2
  1 1
  1 2
  2
  #
  2 1
  2 1 3
  7
  1 3
  2 1

    The first two numbers are height and width.  The next numbers, up to the
    '#' are row clues.  The rest are column clues.  As in the .non files, a
    blank clue is indicated by a line with just a zero on it, but pbnsolve
    will also treat blank lines as blank clues.

  OLSAK .G FORMAT
    Suffix:  "g"
    Contains:  clues
    Color:  multicolor

    This rather complex format is documented inside the sample files that
    come with the olsak solver.  The format can support triddlers and triangle
    puzzles, but pbnsolve can't read those variations.

    Pbnsolve will not be able to automatically identify these files unless
    they have a .g suffix or the -fg flag is given.

  JAKUB WILK'S .NIN FORMAT
    Suffix:  "non"
    Contains:  clues
    Color:  black & white only
    Documentation: http://jwilk.nfshost.com/software/nonogram.html

    Pretty much the same as the .mk format, except the height and width
    values are given in the opposite order, and there is no '#' between the
    row clues and the column clues.

    Pbnsolve cannot "sniff" the format of these files.  If you give it one
    without specifying the format, it will probably guess that it is an "mk"
    file and fail miserably.

  MESHCHERYAKOV AND SUKHOY'S .CWD FORMAT
    Suffix:  "cwd"
    Contains:  clues
    Color:  black & white only

    Alexander Meshcheryakov's QNonograms program and Vladimir Sukhoy's C++
    solver both use the "CWD" format, so I suppose it is something of a
    Russian standard.  Unfortunately, the ".cwd" filename suffix is a poor
    choice since a standard file format for crossword puzzles uses that
    suffix too.

    This is another variation on the MK format, differing only in that the
    height and width are on separate lines, and there is always a blank
    line between the row clues and the column clues.

  ROBERT BOSCH'S LP SOLVER FORMAT
    Suffix:  "lp"
    Contains:  clues
    Color:  black & white only
    Documentation: http://www.oberlin.edu/math/faculty/bosch/pbn-page.html

    This is a file format with little to recommend it.  Basically a wordier
    version of "mk" or "nin".  So why'd we bother to support?  Dunno.

  NETPBM-STYLE PBM FILES
    Suffix:  "pbm"
    Contains:  goal, clues computed by pbnsolve
    Color:  black & white only
    Documentation: http://netpbm.sourceforge.net/doc/pbm.html

    This is a 2-color bitmap image format used by the netpbm package, which
    includes tools to convert just about any other image format (GIF, PNG,
    JPEG, etc) to this one.  We should probably use libnetpbm to read it,
    but we don't.  We can take plain or raw files.  The plain files are
    pretty danged easy to generate from other programs, especially since
    pbnsolve doesn't care about the 70 characters-per-line limit.

    Some PBM files can contain multiple images.  It would be good if the
    -n flag could be used to select which one to solve, but this hasn't
    been implemented.  We always solve the first one.
