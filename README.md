# PDLL: Playdate Dynamic Library Linker

A header-only runtime dynamic library system for the Playdate.

(Note: host binary needs a zlib implementation, [uzlib](https://github.com/pfalcon/uzlib) is currently what's used; uzlib is not header-only.)

## Example

In the dynamic library (compile to `mylibrary.bin` with pdc):

```c
int foo(void);
int bar(int arg);

// Required, even if empty.
// Can list as many functions as desired.
PDLL_EXPORT(
    foo,
    bar
);

int eventHandler(PlaydateAPI* playdate, PDSystemEvent event, uint32_t arg)
{
    // must be first line in handler!
    PDLL_EVENT(playdate, event, arg);
    
    // ...
}
```

Main executable:

```c
// open mylibrary.{bin, so, dll dylib} from
// pdx directory and align it to 256 bytes
pdll_t* mylib = pdll_open("mylibrary", PDLL_FILE_PDX | PDLL_ALIGN_256);
int (*foo)(void) = pdll_symbol(mylib, "foo");
foo();
pdll_close(mylib);
```

## Limitations

- On the simulator, `dlopen` returns one handle per file (can't have two copies of the library in memory simultaneously)
- Encrypted `pdex.bin` not supported (but this shouldn't be a problem anyway.)

## Dependencies

- [uzlib](./uzlib) ([source](https://github.com/pfalcon/uzlib)) — zlib license,
  device only.

## License

MIT (see [LICENSE](./LICENSE)). uzlib is under the zlib license.
