# Sinkhole

A vibrant, interactive terminal application to leave running in the background.

## Requirements

`sinkhole` should build on any modern UNIX-like system supporting the
`xterm-1006` terminal with the curses library. This must be modern enough for
`poll(3)`.

## Installation

```sh
$ make
$ make install
````

The following environment variables may be set:

- `PREFIX`

    The installation prefix (`/usr/local` by default).

- `CC`

    The compiler to use (`cc` by default).

- `RM`

    The program with which to remove files (`rm` by default).

- `INSTALL`

    The program with which to install files (`install` by default).

## Copyright

Copyright 2021 Ben Davies
