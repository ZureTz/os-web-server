# os-web-server

## Requirements

### 1. Install glib

```bash
sudo pacman -S glib
```

- or Follow the [instructions](https://github.com/GNOME/glib/blob/main/INSTALL.md).

### 2. Edit `.clangd`

Replace Include path to yourselves (this differs by operating systems)
 - Run `pkg-config --cflags glib-2.0` to see include paths to add.
 - Paste given path to `.clangd` file like below: (the specific inlude paths may differ from below).
 - Note that **what you only need to do** is to replace the path **AFTER** the `-std=gnu17,` and **BEFORE** `[]` ends.
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

## Run the program

```bash
make
```

```bash
./webserver 8088 web/
```