# epiC C Lib

## Dependencies

 - Rust Compiler
	```sh
	curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
	```

## Getting Started

 - Add to path in order to be able to use the tools:
	```sh
	export PATH=$PATH:/path/to/cclib/tools
	```
 - Create project (in your project dir, not in cclib)
	```sh
	ccproject
	```

## Building

If you have `src` foulder with `*.c` files in it, it should build when you run `make` in your directory.

## Preprocessor Defines

### Predefined
 - `__ccname__` -> `$(application_name)`
 - `__ccversion__` -> `$(application_version)`
 - `__ccauthor__` ->  `$(application_author)`

### Options
 - `__ccnotitle__`, disable title print
