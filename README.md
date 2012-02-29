stripesolver
============

About
-----

stripesolver is a timing-based string guesser intended to solve level 6 of the Stripe CTF. It is written in C and uses a heuristic to determine matching characters.

Compiling
---------

To compile stripesolver, simply run `make` from inside the source directory. The `solver` binary will be created and can then be used to guess a string.

Usage
-----

To run stripesolver, pass it two paths -- the first to the checker binary to target, the second to the file to find the contents of. There is currently no feedback on solving progress, so do not worry if stripesolver takes a long time to run.
