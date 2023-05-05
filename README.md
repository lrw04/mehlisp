# mehlisp

A Lisp dialect worthy of a meh.

## Design

Values in mehlisp are the following:

- Atoms: numbers, nil, and t
- Conses
- Symbols
- Lambdas
- Macros

The list of special forms is the following:

- quote
- set!
- if
- simple-lambda
- error
- progn

The list of builtin functions:
- cons
- +p, -p, *p, /p, div, rem
- =p, <p, >p
