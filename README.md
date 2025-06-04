-----

<div align="center"\>
<img src="./assets/banner.jpeg" width=90%/\>
</div\>

-----

# ✨ SCHADuler: Scheduling and Synchronization Algorithm Simulator ✨

This project simulates **scheduling** and **synchronization algorithms** within an operating system context. Dive deep into how tasks and processes interact and manage resources efficiently\!

-----

## 🚀 Simulated Scheduling Algorithms

Explore and compare the efficiency of various algorithms that manage CPU execution of processes:

  * **First Come First Served (FCFS)** ⏳: Processes are executed in the order they arrive.
  * **Shortest Job First (SJF)** 📉: Prioritizes processes with the shortest execution time.
  * **Shortest Remaining Time (SRT)** 🏃‍♂️: A preemptive version of SJF, prioritizing processes with the least time remaining.
  * **Round Robin (RR)** 🔄: Distributes CPU time slices fairly among processes.
  * **Priority Scheduling (PS)** 👑: Processes with higher priority are executed first.

-----

## 🤝 Simulated Synchronization Methods

Understand how processes safely share resources and avoid conflicts:

  * **Mutex** 🔒: Ensures exclusive access to critical sections of code.
  * **Semaphores** 🚦: Controls access to a shared resource by a limited number of processes.

-----

## ⚙️ Getting Started

### Cloning the Repository (with Submodules\!) 🌳

This repository uses **Git Submodules** to manage its dependencies. For a one-liner to clone everything at once, use this command:

```bash
git clone --recursive https://github.com/DanielRasho/sCHADuler
```

Already cloned the repo and forgot the submodules? No worries\! You can initialize and update them anytime:

```bash
git submodule update --init --recursive
```

### Development Environment (Nix Recommended\! ❄️)

For a smooth development experience, we highly recommend using **Nix**. Ensure you have [Nix](https://nixos.org/download/) installed and [Flakes](https://nixos.wiki/wiki/Flakes) enabled.

With Nix set up, simply type:

```bash
nix develop
```

This command will enter you into a shell environment with all the necessary dependencies to build and run the project.

#### Manual Dependency Installation (No Nix) 🛠️

If you're not using Nix, you'll need to install these dependencies manually:

  * `clang`
  * `GTK4`
  * `pkg-config`
  * `bear` (Find it at [rizsotto/Bear](https://github.com/rizsotto/Bear))

First, compile the `./nob.c` file into an executable named `nob`:

```bash
clang -o nob ./nob.c
```

Then, you can run `nob -h` to see all available actions:

```bash
# For example, to compile the project in debug mode:
./nob
```

-----

## 🏃‍♀️ Running the Program

You can always run the program normally, but if you need to open the **GTK interactive debugger**, use this command:

```bash
GTK_DEBUG=interactive ./build/main
```

-----

## 📂 Project Structure

The project is organized into two primary files for clear separation of concerns:

  * **`main.c`**: Contains all application logic and UI interactions.
  * **`lib.c`**: Houses all library code, designed to be as platform-independent as possible.

We leverage the robust **GTK4** development toolkit for all UI rendering needs.

-----

## 💡 Scheduling Key Points

### How Do We Represent a Process? 🧠

The foundation of our simulation is how we define a process. You can view the detailed structure in the source code:

[https://github.com/DanielRasho/sCHADuler/blob/790afd28e445a97b888ea8db64b450ebe01ade29/src/lib.c\#L566-L572](https://github.com/DanielRasho/sCHADuler/blob/790afd28e445a97b888ea8db64b450ebe01ade29/src/lib.c#L566-L572)

### How Do We Store a Simulation? 💾

A scheduling simulation consists of a list of steps, where each step holds all the necessary data to display a complete frame or screen of information. This space is pre-allocated by the UI once the simulation file loads.

[https://github.com/DanielRasho/sCHADuler/blob/790afd28e445a97b888ea8db64b450ebe01ade29/src/lib.c\#L664-L669](https://github.com/DanielRasho/sCHADuler/blob/790afd28e445a97b888ea8db64b450ebe01ade29/src/lib.c#L664-L669)

[https://github.com/DanielRasho/sCHADuler/blob/790afd28e445a97b888ea8db64b450ebe01ade29/src/lib.c\#L649-L659](https://github.com/DanielRasho/sCHADuler/blob/790afd28e445a97b888ea8db64b450ebe01ade29/src/lib.c#L649-L659)

### How Do We Compute a Simulation? ➕

Each simulation is defined by a single function:

```
<initial inputs> -> simulation_steps
```

Most of the time, the initial inputs are just the initial list of processes. However, for **Round Robin**, the initial inputs must also include the `quantum`.

An example simulation function can be found here:

[https://github.com/DanielRasho/sCHADuler/blob/790afd28e445a97b888ea8db64b450ebe01ade29/src/lib.c\#L677-L770](https://github.com/DanielRasho/sCHADuler/blob/790afd28e445a97b888ea8db64b450ebe01ade29/src/lib.c#L677-L770)

-----

## 🔑 Synchronization Key Points

### Simulation Structure 🏗️

This structure stores all necessary data for the synchronization simulation, including loaded processes, simulation resources, actions that modify process states, and more:

[https://github.com/DanielRasho/sCHADuler/blob/40305ab3a37d0e25d14125c45199a681eda49802/src/lib.c\#L1389-L1415](https://github.com/DanielRasho/sCHADuler/blob/40305ab3a37d0e25d14125c45199a681eda49802/src/lib.c#L1389-L1415)

### States of a Process 🚥

Each process in the simulation has a state. This state can change based on the action occurring at a specific time. The possible states for a process are:

[https://github.com/DanielRasho/sCHADuler/blob/40305ab3a37d0e25d14125c45199a681eda49802/src/lib.c\#L1278-L1289](https://github.com/DanielRasho/sCHADuler/blob/40305ab3a37d0e25d14125c45199a681eda49802/src/lib.c#L1278-L1289)

### Visualizing Steps 🎬

To display the steps taken in the simulation, we save them within the `timeline` structure. The simulation then accesses this data to present it to the user.

[https://github.com/DanielRasho/sCHADuler/blob/40305ab3a37d0e25d14125c45199a681eda49802/src/lib.c\#L1358-L1383](https://github.com/DanielRasho/sCHADuler/blob/40305ab3a37d0e25d14125c45199a681eda49802/src/lib.c#L1358-L1383)
