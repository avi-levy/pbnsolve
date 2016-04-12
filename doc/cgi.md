CGI Mode:
---------

If the program is renamed "pbnsolve.cgi" then it will work as an AJAX
puzzle validator.  It ignores command line arguments and run as if the options
-hcx1 were set, so it checks if the given goal is unique, returns results in
xml format, and times out after one second of CPU time is used.  (The actual
timeout value is configurable at compile time).

A CGI variable named "image" should contain the puzzle description (basically
the contents of the input file).  The CGI variable "format" may contain the
one of the suffix strings specified below to indicate the format of the input
file.  If the format is not given explicitly, a half-hearted attempt will be
made to guess it from the contents of the file.

In CGI mode, output will be something like this for a puzzle with a unique
solution:

    Content-type: application/xml

    <data>
    <status>OK</status>
    <unique>1</unique>
    <logic>1</logic>
    <difficulty>510</difficulty>
    </data>

or like this for a puzzle with multiple solutions:

    Content-type: application/xml

    <data>
    <status>OK</status>
    <unique>0</unique>
    <alt>
    X.
    .X
    </alt>
    <difficulty>300</difficulty>
    </data>

The <status> will be "FAIL:" followed by an error message if the
solver was for any reason unable to run, or "TIMEOUT" if the solver
exceeded it's runtime limit, or "OK" otherwise.  <Unique> is 1 if
the puzzle has only one solution, 0 if otherwise.  If there are
multiple solutions, it will give one that differs from the goal
solution in the <alt> tag.  <Logic> is 1 if it was solvable by line
and color logic alone, 2 if two-level lookahead was needed.  If the
solver had to guess, no <logic> line appears.  <Difficulty> is a
measure of how hard the solver had to work to solve the puzzle.
It's -1 if the puzzle was blank, 100 if the puzzle had so little
white space that it was trivial to solve, and a larger number if
it was harder.