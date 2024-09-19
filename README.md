# SL (Structuring Language)
SL aims to be a language agnostic metaprogramming language.

## Building
After cloning and entering the repo, run these commands
```
mkdir build
cmake -B build
cmake --build build
```
The program ought to be located in the `bin` folder of the cloned repo. 

## Syntax
### Macro Definition
Define a macro in a file with `## <macro_name>`, end the macro with `##`.
Currently macros are written in C, and must define a function `int replace(FILE* out, int argc, char** argv)`.
Code is assumed to be in Assembly, this will be changed to be language agnostic in future updates.
### File loading
Load another SL file with the `load` keyword and the file path in a string.
