# EntryLogger for C

A plain C89, single source/header, library for easily working with EntryLogger
documents.

## Generating a Compilation Database

If you want to have your Visual Studio Code fully setup to work with this
project it's highly recommended that you setup a compilation database to work
with [SonarLint](https://www.sonarsource.com/products/sonarlint/) and other
tools. In order to automate things a bit you'll be required to have
[Bear](https://github.com/rizsotto/Bear) installed in your system. After that,
all that you need to do is run:

```bash
make compiledb
```

## Testing

In order for you to ensure tha the library is working correctly and also to have
an example file to test/play around with, after cloning this repository simply
run:

```bash
make run
```

This will compile the library into a static library (could've been a simple
object, we've opted for a static library just to be fancy) and also compile an
example program that links against the library and creates a brand new document
with some test data using the library functions, and later also reads the file
back as a sort of self-test.

After all of that's done you'll have a couple of important files in your `build`
directory:

- `libentrylogger.a` A static library for linking against if desired.
- `entrylogger_test` The example/test program that can create/edit/read ELD files.
- `example.eld` An example document to play around with.

## Including in Projects

Including this library in your projects is extremely simple and given its
C89-compliance it'll literally be compatible with any project you might have,
even if it's not written in C!

### Linking Statically

This is the simplest way to work with this library. You can either add the whole
project into yours in a separate folder or just copy the source/header files
over to the source folder of your project. After that's done you can easily just
include a new directive in your `Makefile` to compile this source either as an
object or as a static library and at the end link it together with your other
sources/objects.

## License

This project is licensed under the [MIT License](/LICENSE).
