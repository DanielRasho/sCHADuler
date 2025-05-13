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
git submodules init --recursive
```

## Developing

All commands to develop on this project must be run on the shell created by
`nix develop`. Please install [Nix](https://nixos.org/download/) and enable
[Flakes](https://nixos.wiki/wiki/Flakes).
