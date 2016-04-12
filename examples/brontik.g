This file declares the task 52 from the Journal "Malovane krizovky 10/2003",
Silentium s.r.o., Bratislava.

The tasks from this journal are somewhat different than the task with
triangles from www.griddlers.net. There are three differences:

- The triangles are connected to its main "body" block
  and no space is allowed between them.
- Two same triangles can be followed without space between them.
- The lenght of main body block is declared including the length
  of the triangle-items surrounding them.

We define somewhat different format for this type of tasks. The block
declaration can be prefixed by left-glue-triangle-char and it can be
suffixed by right-grue-triangle-char. These characters are daclared as
a normal triangle, but the comment of the declaration begins with two
special characters "<" or ">". The "<" means, that it is a
left-glue-triangle and ">" means that it is a right-glue-triangle.
The first character is used for rows declaration and the second for
columns declaration because the "left-glue" can be changed to
"right-glue" and vice versa if we go from rows to columns reading.

The two same <inchars> can be declared if the first is declared as 
left-glue-triangle and second as right-glue-triangle.

#d
  1:*   black        rc  cerna
  /:a   white/black  <<  trojuhelnik
  /:b   black/white  >>  trojuhelnik
  \:c   white\black  <>  trojuhelnik
  \:d   black\white  ><  trojuhelnik
: -------------------------------------
/3\
/7\  /2  2
/4/  \4\  5
/3/  \3\  3
/3/  \4\  1  1
/2/  1/  \4\  1  1
/2/  /1  \5/  1
1  1/  \2\  /2/
/2/  /1  1  3\  /4/
1  1/  2\  \3  /1  /4/
1/  /1  2  2  /2  2/  /3\
/1  1  /3  5/  /2/  4
1  1  /5  /5  1  2
1  1  /6/  5  1  /3
1  1  5  \5  \8/
1  1  22/
1  \2\  1  \18/
\2\  \4\  \12/
\3\  \4\
\9\
: -----------------------------------
/7\
/3/  \2\
/3/  /7\  1
/2/  /2/  \2\  \2\
/2/  /2/  /5  1
/3/  /4  1  1
/3/  5\  \2\  1
2/  /6  2
/2  9  2
2/  \5/  3\  \2
1  3  1
1  1\  3  \1
1  \3\  /3\  3
2\  10
\2  \10
2\  7
\3\  /8
\2  /3/  3
2\  3
\2  /4\  3
1  /3/  4/
\2\  2/  3
1  1  3
1  /2/  3/
1  1  /3
/6/  1  /6/
/4  /2/  5/
1  6/  \2
\3  1
2
: ------------------------------------

See gridoce.pdf, kocka.g, oko.g, ruze.g, alladin.g,
tkocka.g and vcely.g for more information
about input file format (black+white, colors, triangles, triddlers).

Enjoy.

