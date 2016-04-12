Usage:
------

Run syntax is:

   `pbnsolve -[bdhlopt] -[v<msgflags>] [-n<n>] [-s<n>] [-x<n>] [-d<depth>]
    [-f<fmt>] [-a<algorithm>] [<datafile>]`

Input files may be in any of too many formats, described in the "Input Format"
section below.  Pbnsolve will try to guess the file format based on the
filename suffix.  If that doesn't work, it will try to guess it based on
the content of the file, though it doesn't do that terribly well.

If the <datafile> path is omitted, then it reads from standard input.  In
this case it will always expect "xml" format.

# Command line options:
##   -b 
    Brief output.  Error messages are as usual, but normal output is just
    one line containing one or more of the following words separated by
    spaces:
   +  unique        - puzzle had a unique solution.
   +  multiple      - puzzle had multiple solutions.
   +  line          - puzzle was solvable with line & color logic alone.
   +  depth-2       - puzzle was solvable with two-level lookahead.
   +  trivial       - puzzle was too easy, with little or no whitespace.
   +  solvable      - found solution but none of the above was proven.
   +  contradiction - puzzle had no solutions.
   +  timeout       - cpu limit exceeded without a solution.
   +  stalled       - only partially solved puzzle.
  Without the -b flag you get wordier output along with ASCII images
  of one or two puzzle solutions.  You don't get triviality reported
  in that case, unless you give the -t flag and notice a difficulty
  rating of 100.
  
  Which outputs are possible depends to some extent to the algorithms
  selected with the -a flag.  "depth-2" occurs only if algorithm C
  (Contradiction Check) is enabled.  "stalled" occurs only if no
  search algorithm is enabled (G, P or M).
  
  Note that with the -u or -c flags, we will always get one of
  'unique' or 'multiple' if we find any solution at all.  If the puzzle
  can be solved by line and color logic alone, it will be flagged
  "line", but since other solution techniques are possible, it should
  not be assumed that other puzzles are not logically solvable in a
  broader sense.
##   -u
  Check uniqueness.  If we had to do any searching (that is we actually
  needed to invoke algorithms G, P or M) then we don't necessarily know
  that the first solution we find is the only solution to the puzzle.
  If the -u flag is given, then in such cases we proceed to try to find
  a second solution.  If none are found, the puzzle is unique.

##   -c
        Input puzzle is expected to include a goal solution grid.  We try
  to see if we can find a solution different from that one, and if
  so, report it.  This is sometimes faster than -u, and in the case
  of multiple solutions always reports back a non-goal solution, but
  is otherwise similar.

##   -o  
        Print a description of the puzzle data structure before starting
  to solve it.  This is mainly for debugging the puzzle reading
  code.  It's not pretty.

   -a<algflags>

        Use the listed algorithms.  Possible values are listed below.  The
  order in which the options are given is immaterial and does not
        determine the order in which they are tried.  Default is -aLHEGP.

     L - LRO Line Solving.  This is normally the first thing we try,
         examining rows and columns one at a time, comparing the leftmost
         possible position of the blocks to their rightmost possible
         position and marking all cells that fall in the same block
         either way.  Keep doing this until the puzzle is done or it
         we stall.

           H - Cache Line Solver Solutions.  This is a supplement to the
               Line solver.  It stores line solver results in a hash table
         so they can be reused it the same situation comes up again,
         which occurs surprisingly often.  Cache hit rates of 80% to
               90% are not unusual with large puzzles, so this can speed
         up the solver substantially, reducing run times by 30% to
               50%, but it increases memory consumption substantially.

     E - Exhaustive Check.  After line solving stalls, but before 
         trying anything else, double check every cell on the board
         to make sure it really can have all of it's listed values.
         Doing this this is probably as likely to slow us down as speed
         us up, but it ensures that any puzzle solvable by line and color
         logic alone will be flagged as 'logical'.  You can use this
         as a substitute for the line solver instead of as a backup, but
         it is slower.

     C - Contradiction Check.  Try every possible color for every cell
         that has more than one possible color left, and search to see
         if that leads to a contradiction, but only search a couple
         levels down.  If a contradiction is found, eliminate that
         color possiblity.  Otherwise forget it.  This should solve
         most cells that humans can readily solve, using techniques
         like edge logic, because humans only have limited look-ahead.
         The depth limit is set by the -d option.  The contradiction
         check is done after the line solving algorithms (L and E)
         have stalled, but before trying heuristic search algorithms
         (GPM).

     G - Guessing.  Start a depth-first search for a solution, using
         heuristics to guess colors for cells, continuing forward until
         we find either a solution or a contradiction, and then back
         tracking to the last guess to try something else.  This was
         the default in older versions of pbnsolve.  If both guessing
               and probing (P or M) are enabled, then probing will be used
               first, but if it seems to be stalling, guessing will be tried
         for a while.

         You can further choose between three different heuristic
         functions, by suffixing the flag with a digit.  The G1 and G2
         algorithms are old and bad.  G3 is the one used in older
         version of pbnsolve.  G4 is the default now.  It's based on
         the heuristic functions used by Steve Simpson's solver.  G5
               and G6 are experimental functions that try to use information
               from a known solution to make guesses.  They work very badly.

     P - Probing.  Similar to guessing but instead of using heuristics
         to choose a guess, we actually explore the consequences of
         each possibility, and choose the one that makes the most
         progress for us.  Thus we invest more computation in making
         a good choice.  This can slow us down on many relatively easy
         puzzles, where the quality of the choice doesn't matter much,
         but speeds us up substantially on harder puzzles.  If this is
         set M is unset, but it can be combined with guessing (G).  In
         that case we mostly probe, but if it seems to be stalling,
         we try guessing.
         
         Different variations of probing can selected by suffixing the
         flag with a digit.  P1 is our old probing algorithm, which
         probed on cells with two or more solved neighbors.  P2 adds
         probing on all cells adjacent to cells set since the last
         guess.  P3 instead adds probing on cells who are given a good
         rating by the heuristic function from the guessing algorithm.
         P4 does all three.  P2 is the default.

           M - Merged Probing.  Similar to probing but whenever we probe on
         different possible settings for a cell, we check to see if
         all the alternatives set some other cells to the same values.
         If they do, set those values.  This was implemented in the
         hopes that it would improve performance, but the payoff is
         usually less than the overhead, so it seems to be a dud.
         Setting this unsets P.

   -f<fmt>
        Explicitly set the input file format.  The argument should be
  on of the "suffixes" listed in the "Input Formats" section below.
  If this is not given, pbnsolve will first look at the actual
  suffix of the input filename.  If there is no recognizable suffix,
  then it will try to guess the format of the file from it's first
  few bytes.

   -n<n>
        For file formats, like the XML format, in which the file can contain
  more than one puzzle, this specifies which puzzle to solve.  The
  default is 1, which means the first puzzle.

   -s<n>
        Start solving from one of the "saved" solutions in the input file.
  <n> is the number of the input to use, if there is more than one.
  If the number is omitted it defaults to 1.

   -m or -m<cnt>
        Turns on printing of explanation messages, which try to explain
        how pbnsolve solved the puzzle.  Explanations are printed for
        linesolving (-aL), exhaustive search (-aE) and contradicting (-aC)
        but not for heuristic search, because that would typically result
        in far too much output.  Note that in explanation messages we
        refer to rows and columns by one-based numbers, while in debug
        messages we use zero-based columns, which is confusing if you turn
        both on at once.  If a count is given, it tells how often to print
        a snapshot of the board.  If no count is given, a snapshot will
        be printed after every tenth message.

   -x<secs>
        Set a limit on the CPU time used by pbnsolve.  If <secs> is zero
  or omitted, then there is no CPU limit, otherwise pbnsolve will
  abruptly terminate if it uses more than that number of CPU seconds.
  Normally the default is zero (no limit) but it's possible to build
  pbnsolve with a different default CPU limit.

   -d<n>
        Set a depth limit for the contradiction checking algorithm.  Since
        contradiction checking is not enabled by default, this has no effect
        unless the -aC flag is also given.  If this option is omitted, the
        default depth is 2.

   -t  
        After run is completed, print out run time and various other
  statistics.

   -i  
    If interupted, pause execution, print out statistics, and ask
  the user if we should terminate or continue.  This gives a way
  to monitor performance of long-running solves.

   -h  
        Run in http mode.  Output is XML-formatted in a way suitable for
  use in an AJAX-application.  This doesn't work right with the
  -t, -v or -b flags.


   -v<msgflags>
        Write debugging messages to standard output.  The <msgflags> are
  flags that indicate what kinds of debugging messages should be output.
  Some or all of these flags may be disabled by compile time options.
  Recognized flags include:
  
     A - Top level messages.
     B - Backtracking messages.
           C - Contradiction search message (with -aC)
     E - Messages from exhaustive check (with -aE)
     G - Messages from guessing.
     H - Messages from hashing.
     J - Job management messages.
     L - Linesolver messages.  (disabled by default)
     M - Merging Messages.
     P - Probing Messages.
     Q - Probing Statistics.
     U - Messages from Undo code used when backtracking.
     S - Cell State change messages.
     V - Extraverbosity when used with any of the above.

   -w<dir><number>
    Print debugging messages relevant to a particular row or column.
  You can say -wR12 for row twelve or -wC0 for column zero.  This option
  is normally disabled by a compile time option, since constantly
  checking if the current row needs logging slows things down.  You
  can watch as many as 20 rows or columns.
