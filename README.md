About cpp-mython
----------------

This is a runtime for Mython programming language.

How to build
------------

To build this app you need:
cmake version 3.10 or higher (https://cmake.org/)

To build the app follow steps:

0. mkdir ./build
1. cmake ../src -DCMAKE_BUILD_TYPE=Release
2. cmake --build ./
If you need Debug version, use -DCMAKE_BUILD_TYPE=Debug flag.
Make sure that you have permissions to create files in your working
directory.
Program has been built successfully on Ubuntu/Linux 22.04 with
gcc version 11.2.0, but other gcc versions, that are compatible with C++17
standard should work properly.

How t use
---------

Program reads Mython source code from standard input, run it and print 
result to standard out.

Usage: ./interpreter [OPTIONS]

Supported options:
-h - print help and exit;
-t - run tests before start;

You can also run "example.my" to see simmple interpreter work:

$ ./interpreter < example.my

If everything is OK, you will see "C++ love Mython" in the terminal.
