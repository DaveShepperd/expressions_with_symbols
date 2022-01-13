# expressions_with_symbols
An example of an expression parser with optional symbol handlers. The symbols, if desired, can be handled with either a balanced tree or a hash table.

The last project I worked on came with a terrible expression parser. It had no operator precedence and a syntax error sent it into an infinite loop. There was no time available to swap it out with something better.

With that project long since put to rest and with me having nothing better to do and to prove to myself it can't be that hard to do, I decided to write my own expression parser from scratch just for the fun of it. This code is just example code. It isn't used in a production system (yet) so it likely has some yet to be discovered and failing dusty corner conditions. Use at your own risk. Or better still, just use it as a starting point and make something better.

It consists of three separate independent subsystems I believe suitable for use in a common library:

#### lib_hashtbl - a hash table library subsystem. Contained entirely in lib_hashtbl.[ch].

#### lib_btree - a balanced binary tree using the AVL algorithm coded with what was in Wikipedia. Contained entirely in lib_btree.[ch].

#### lib_exprs - an expression parser. Contained entirely in lib_exprs.[ch].

They can be found in the **_libs_** folder. There are **_Makefiles_** but they have only been built with gcc. Good luck building with other compilers.

The API's to each of the three subsystems are described via comments in the corresponding **_.h_** files.

The symbols can have a value type of string, integer or double. If a string term appears anywhere in the expression, all terms are converted to strings and the final result is a string. String operands in an expression are concatinated and the only legal operator between terms where one or more term is a string can only be a **_+_**. I.e. **_"foo"+10/2_** results in **_"foo5"_** or **_"foo"+"bar"_** becomes **_"foobar"_**.

For an example on how to use **_lib_exprs_** without needing or wanting symbols, see **_exprs_test_nos.c_**. I.e. **_./main '1+2*3/4'_**

For an example on how to use both **_lib_exprs_** and **_lib_hashtbl_**, see **_exprs_test_ht.c_**.

For an example on how to use both **_lib_exprs_** and **_lib_btree_**, see **_exprs_test_bt.c_**.

I.e. when using symbols: **_./main 'sym1=10;sym2=20;sym3=sym1+sym2;sym3*3.14159'_**.

Although the standard libc btree and hash functions no doubt work just fine, I was disappointed they had no user provided mechanism for memory management nor anything that might help with statistics collection. So these provided subsystems have optional callbacks for that as well as internal message reporting.
