![Banner](./assets/banner.jpeg)

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

So if you have Nix simply type:

```bash
nix develop
```

This will enter you into a shell with all the required dependencies to run the
project.

If you don't have Nix installed on your system, you'll need to install the
dependencies manually:

- clang
- GTK4
- pkg-config
- bear (https://github.com/rizsotto/Bear)

First compile the `./nob.c` file into an executable like `nob`:

```bash
clang -o nob ./nob.c
```

Then you can simply run the `nob -h` command to check the available actions you
can take.

```bash
# For example you can compile the project in debug mode like so:
./nob
```

## Running the program

You can always run the program in the normal way, but if you need to open the
debugger simply run it with:

```bash
GTK_DEBUG=interactive ./build/main
```

## Project Structure

The project contains two main files:

- main.c: All application logic lives here, this file knows about the existence
  of a UI.
- lib.c: All library code lives here, we tried to keep this code platform
  independent.

For UI rendering and needs we used the GTK development toolkit.

## Key Points

### How do we represent a Process?

https://github.com/DanielRasho/sCHADuler/blob/790afd28e445a97b888ea8db64b450ebe01ade29/src/lib.c#L566-L572

### How do we store a Simulation?

A sheduling simulation consists of a list of steps, each step has all the
necessary data to display a whole frame or screen of information. This space is
preallocated by the UI once the file is loaded.

https://github.com/DanielRasho/sCHADuler/blob/790afd28e445a97b888ea8db64b450ebe01ade29/src/lib.c#L664-L669

https://github.com/DanielRasho/sCHADuler/blob/790afd28e445a97b888ea8db64b450ebe01ade29/src/lib.c#L649-L659

### How do we compute a Simulation?

Each simulation is defined by a single function:

```
<initial inputs> -> simulation_steps
```

Most of the time, initial inputs is only the initial list of processes, but in
the case of `Round Robin`, the initial inputs must also contain the `quantum`.

An example simulation function is:

https://github.com/DanielRasho/sCHADuler/blob/790afd28e445a97b888ea8db64b450ebe01ade29/src/lib.c#L677-L770
