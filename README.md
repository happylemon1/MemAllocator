# Dynamic Memory Allocator

A custom heap allocator featuring explicit free-list management with LIFO allocation strategy, developed in C. Contains comprehensive test traces and performance benchmarking tools.

## Core Implementation:
- **mm.{c,h}**: Primary memory allocator with malloc/free functionality. The mm.c file contains the core allocation algorithms
- **mdriver.c**: Performance testing and validation framework for the allocator
- **Makefile**: Build configuration for compilation

## Advanced Features:
- **mm-realloc.c**: Implementation of realloc that avoids malloc/free when possible
- **mm-gc.c**: Mark-and-sweep garbage collector for freeing unreachable blocks

## Supporting Infrastructure:
- **config.h**: Driver configuration parameters and settings
- **fsecs.{c,h}**: Cross-platform timing utility wrappers
- **clock.{c,h}**: Hardware cycle counter access for precise timing
- **fcyc.{c,h}**: Cycle-based timing measurement functions
- **ftimer.{c,h}**: Interval timer utilities using system calls
- **memlib.{c,h}**: Simulated heap environment and memory management

## Usage

Build the testing framework:
```bash
make
```

Execute full test suite:
```bash
./mdriver
```

Run with detailed output on specific trace:
```bash
./mdriver -V -f short1-bal.rep
```

View available command-line options:
```bash
./mdriver -h
```

## Advanced Testing

Test realloc performance:
```bash
make mdriver-realloc
./mdriver-realloc -f traces/realloc-bal.rep
./mdriver-realloc -f traces/realloc2-bal.rep
```

Validate garbage collection:
```bash
make mdriver-garbage
./mdriver-garbage
```