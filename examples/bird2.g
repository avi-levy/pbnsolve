Lines of color declarations have the following format:

<spaces><inchar><colon><outchar><spaces><word_XPM><spaces><comment>

<inchar> is a character used in block declaration after numbers in
input file (see "m" for "blue color" in this example). You cannot use
digit, space, comma or tabelator for <inchar> declaration.
The "0" and "1" are exceptions, see bellow.

<outchar> is a character which will be used for terminal printing by
grid program for presentation of this color.

<word_XPM> is the word (without spaces) used in XPM format for color declaration.
You have two possibilities: use the "natural" word for color, if this
word is present in rgb.txt in your X window system. 
The second possibility: use #<six_hex_digits> format for RGB declaration.

If <inchar> is "0", then this line declares the color for background
of the image. You can omit this declaration. The white color will be
used in such case.

If <inchar> is "1", then this line declares "default" color of blocks.
This color is used if no <inchar> follows the block declaration.
If you omit this line then you have to specify the color by <inchar>
for each block, see ruze.g for such example.

#declaration of the colors:
   m:%  blue   
   0:   white  
   1:*  black  
: rows
21
26
5 14
4 12
3 11
9
8
7
6
1 1 1 1 1 5
1 1 1 1 1 3 4
1 1 1 1 1 1 2 2
1 1 1 1 1 1
1 1 1 1 1 1 1 1
1 13 1
1 17
20
16 5
18 3
20 2
8 1m 12 2
4 1m 1 1m 15 1
3 1m 1 2m 6 2m 1 4 2
3 4m 4 2m 1 4 1
3 7m 1 3 2
9 4
5 1 2
8 1
7
1
: columns
2
3
4
3
3
3
2 1 1 4
2 1 1 6
2 1 1 8
2 1 1 10
2 1 1 11
2 1 1 7 2m 2
2 1 1 10 1m 2
2 1 1 7 3m 2
2 1 1 7 1m 1 3m 1
2 1 1 9 2m 2
3 1 1 11 1m 2
3 1 1 10 1m 2
3 1 1 11 1m 2
3 1 10 1m 3
4 1 10 2m 1 1
4 1 8 2m 1 1
4 1 2 4 1m 1 1
5 1 3 5 1
5 3 3 1
4 2 3 1
5 2 3 2
5 1 3 1
5 2 3 1
5 1 5 1
5 2 3 2
5 2 2 1
5 1 4
5 2 1
5 3
4 1
4
3
3
2
: end