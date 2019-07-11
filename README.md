# Guile GI

This is a library for GNU Guile.  GNU Guile is an implementation of
Scheme, which is a Lisp-like language.

This library hopes to allow Guile to use GObject-based libraries, such
as GTK+3.  GObject libraries are shared object libraries that have
been written in a standardized way to make them easy to use from other
languages.  GObject libraries come with metadata that describes the
functions and procedures in the library.

This is pre-alpha code.  It barely works.  The API is in flux.

Guile GI has two primary components.

* `(gi)` aka `gi.scm`: a guile module that provides functions to parse
  GObject typelib files

* `libguile-gi.so` or `libguile-gi.dll`: a compiled module that
  contains glue code to interface with GObject

You use the provided `use-typelibs` syntax or `typelib-load` procedure
to introspect the GObject libraries to create a Guile binding.  Also,
this project comes with some scheme libraries that bind common GObject
libraries.  For example, `(gi gtk-3)` and `(gi glib-2)` are provided.

For the moment, the docs are at
[spk121.github.io/guile-gi](https://spk121.github.io/guile-gi/)

Try:

    guix environment --ad-hoc -l guix.scm guile
    guile-gi test/example-1.scm
    guile-gi test/browser.scm

Or, create and run in a development environment

    guix environment -l guix.scm
    ./bootstrap && ./configure && make
    tools/uninstalled-env tools/guile-gi test/browser.scm
    tools/uninstalled-env tools/guile-gi test/editor.scm