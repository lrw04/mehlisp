# mehlisp

A Lisp dialect worthy of a meh.

## Design

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
- cons, consp
- +p, -p, *p, /p, div, rem
- =p, <p, >p
