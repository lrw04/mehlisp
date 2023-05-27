# mehlisp

A Lisp dialect worthy of a meh.

## Design

Features of mehlisp:
- Lisp-1
- Properly tail recursive
- Bearable minimum amount of C++ code

Values in mehlisp are the following:

- Atoms: numbers, symbols, lambdas, macros, environment
- Conses

The list of special forms is the following:

- quote
- set!
- if
- lambda
- syntax

The list of builtin functions:
- cons, consp, car, cdr, null
- +, -, *, /, div, rem
- =p, <p, >p
