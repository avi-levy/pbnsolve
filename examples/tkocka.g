
This is a triddler "cat" from www.griddlers.net

#triddlers ... the #t ot #T at the first column declares triddlers.

We need to write six data groups of block length. Each data group
correspods to one side of the hexagonal:

         F
      -------
   A /       \ E
    /        /
    \       / D
   B \     /
      -----
        C
The following have to be carried out:
  E = A + B - D ,  F = C + D - A ,
else the program prints the error message: "wrong hexagonal sides".

Note empty lines in data groups below. They mean: no block
in this row/column.

: ========================== the side A:
1 1
9
2 2
2 2 2 2
2 2
2 3 2
2 2 5
2 2 2 2
2 2 2 2
: ------------------------ the side B:
2 2 2
2 2 2
2 2 2
2 10
2 2
2 3 3 2
23

: ======================== the side C:
2
11
3 2
4 2
3 2
4 2
3 6
2 2 2
2 2 2 2 2
2 3 1 3
: ----------------------- the side D:
2 2 12 1 2
1 2 2 1 2
2 8
2 3
2 2
2 2
7

: ======================= the side E:
2
9 2
2 2 2
2 2 2
2 1 2 2
5 11
2 3
2 4
2 3
: ----------------------- the side F:
2 4
6 3
2 2 1
2 2 2 2
3 1 2
2 1 12
2 1 2
8

: ====================== the End

See gridoce.pdf, kocka.g, ruze.g, oko.g, aladin.g, brontik.g 
and vcely.g for more information about input file format 
(colors, triangles, colored triddlers).


Enjoy.




