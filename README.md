# SCHADuling

Final project of the Operating Systems course.

## Cloning the repository

This repository uses git submodules to track dependencies! If you want a
oneliner to clone the whole thing just type:

```bash
git clone --recursive https://github.com/DanielRasho/sCHADuler
```

If you already cloned the repo and now want to initialize the submodules simply
run:

```bash
git submodules update --init --recursive
```

## Developing

All commands to develop on this project must be run on the shell created by
`nix develop`. Please install [Nix](https://nixos.org/download/) and enable
[Flakes](https://nixos.wiki/wiki/Flakes).

You only need the clang compiler to run this project.

First compile the `./nob.c` file into an executable like `nob`:

```bash
clang -o nob ./nob.c
```

Then you can simply run the `nob -h` command to check the available actions you
can take.

## Running the program

You can always run the program in the normal way, but if you need to open the
debugger simply run it with:

```bash
GTK_DEBUG=interactive ./build/main
```
