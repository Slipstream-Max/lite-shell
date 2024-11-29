# Lite-Shell

Lite-Shell is a lightweight shell implementation written in C. This project demonstrates fundamental concepts of shell programming, including process creation, command parsing, and execution.

## Features

- Execute basic shell commands
- Support for built-in commands
- Customizable through source code modifications

## Project Structure
```python
lite-shell/
│
├── src/ # Source files
│ ├── main.c # Main entry point
│ ├── command_exec.c # Command execution logic
│ ├── history_log.c # History Log logic
│ └── pipe_redirect.c # Pipe and redirect symbol logic
│
├── include/ # Header files
│ ├── command_exec.h
│ ├── history_log.h
│ └── pipe_redirect.h
│
├── build/ # Build directory (generated)
│ ├── lite-shell # Compiled executable
│ └── obj/ # Object files
│
├── Makefile # Makefile for the project
└── README.md
```
## Requirements

- GCC (GNU Compiler Collection)

## Building the Project

To build the project, simply run this at repo's root directory:

```bash
make
```

This command will compile the source files and create an executable named lite-shell in the build directory.

## Running Lite-Shell
After building the project, you can run the shell using:

```bash
./build/lite-shell
```
Cleaning the Build
To clean up the build files, run:

```bash
make clean
```
This command will remove the build directory and all compiled files.    