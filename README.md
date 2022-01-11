# expressions_with_symbols
An example of an expression parser with optional symbol handlers. The symbols, if desired, can be handled with either a balanced tree or a hash table.

The last project I worked on had a terrible expression parser. It had no operator precedence and a syntax error sent it into an infinite loop.

Having nothing better to do and to prove to myself it can't be that hard to do, I decided to write my own expression parser from scratch just for the fun of it. This code is just example code. It isn't used in a production system (yet) so it likely has some yet to be discovered and failed dusty corner conditions. Use at your own risk. Or better still, just use it as a starting point and make something better.

It consists of three separate independent subsystems I believe suitable for use in a common library:

#### lib_hashtbl - a hash table library subsystem. Contained entirely in lib_hashtbl.[ch].

#### lib_btree - a balanced binary tree using the AVL algorithm coded with what was in Wikipedia. Contained entirely in lib_btree.[ch]

#### lib_exprs - an expression parser. Contained entirely in lib_exprs.[ch]

They can be found in the **_libs_** folder. There are **_Makefiles_** but they have only been built with gcc. Good luck building with other compilers.

The API's to each of the subsystems are described via comments in the corresponding .h files. Examples of how to use them can be found in the **_xxx_test.[ch]_** files.

Although the standard libc btree and hash functions no doubt work just fine, I was always a bit disappointed they had no user provided mechanism for memory management nor anything that might help with statistics gathering. So these provided subsystems have callbacks for that as well as message reporting.
