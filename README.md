# mehlisp

A Lisp dialect worthy of a meh.

## Design

Values in mehlisp are the following:

- Atoms: numbers, symbols, vector, lambdas, macros, environment
- Conses

The list of special forms is the following:

- quote
- set!
- if
- lambda
- error

The list of builtin functions:
- cons
- +p, -p, *p, /p, div, rem
- =p, <p, >p
