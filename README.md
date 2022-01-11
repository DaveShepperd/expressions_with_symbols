# expressions_with_symbols
A simple example of an expression parser and symbol handler. The symbols can be handled with a balanced tree or a hash table or none at all.

The last project I worked on had a terrible expression parser. It had no operator precedence and a syntax error sent it into an infinite loop.

Having nothing better to do and to prove to myself it can't be that hard to do, I decided to write my own expression parser from scratch just for the fun of it. This code is just example code. It isn't used in a production system (yet) so it likely has some yet to be discovered and failed dusty corner conditions. Use at your own risk. Or better still, just use it as a starting point and make something better.

It consists of three separate independent subsystems I believe suitable for use in a common library:

lib_hashtbl - a hash table library subsystem. Contained entirely in lib_hashtbl.[ch].
lib_btree - a balanced binary tree using the AVL algorithm I just copied what was in Wikipedia. Contained entirely in lib_btree.[ch]
lib_exprs - an expression parser. Contained entirely in lib_exprs.[ch]

They can be found in the libs folder. There is a Makefile but it has only been built with gcc. Good luck building with other compilers.
