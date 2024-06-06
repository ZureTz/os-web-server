# os-web-server

## Requirements

### 1. Install glib

```bash
sudo pacman -S glib
```

- or Follow the [instructions](https://github.com/GNOME/glib/blob/main/INSTALL.md)

### 2. Edit `.clangd` and `makefile`

1. Replace Include path to yourselves (this differs by operating systems)
 - Run `pkg-config --cflags glib-2.0` to see include paths to add
 - Paste given path to `.clangd` file like below: (inlude paths may differ)
 - Note that **what you only need to do** is to replace the path **AFTER** the `-std=gnu17,` argument
  ```clangd
  If:
  PathMatch: 
    - .*\.c
    - .*\.h

CompileFlags:
  Add: [
    -xc, 
    -std=gnu17, 
    -I/usr/local/include/glib-2.0, 
    -I/usr/local/lib/aarch64-linux-gnu/glib-2.0/include, 
    -I/usr/local/include
  ]
  Compiler: gcc
  ```

2. Change makefile arguments
```makefile
CFLAGS  = ${SFLAGS} ${GFLAGS} ${OFLAGS} ${WFLAGS} ${UFLAGS} `pkg-config --cflags glib-2.0`
LDFLAGS = 
LDLIBS  = `pkg-config --libs glib-2.0`
```

## Run the program

```bash
make
```

```bash
./webserver 8088 web/
```