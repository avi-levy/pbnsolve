To Compile:
-----------

This program was developed for Unix systems.  It would probably not be
difficult to port to other operating systems.  Libxml2 is available for
most operating systems.  The only-unix specific code that comes to mind
is the resource limitation stuff.  If you do a port, I'd be interested in
your diffs.

On Unix:

   edit config.h

      Might want to change some of the settings here, though likely the
      ones in the distribution are OK.

   edit Makefile

      Mainly to get include file paths for libxml2 right and decide between
      -O2 (for production) and -g (for development).

      NOTE: -O2 makes a HUGE difference.  It can make pbnsolve runs more
      than twice as fast.

   "make pbnsolve"

      Should compile without warnings.