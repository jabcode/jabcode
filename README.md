#JAB Code

JAB Code (Just Another Bar Code) is a 2D color bar code, which can encode more data than traditional black/white codes.

##Project Structure
    .
    ├── docs                  # Documentation
    ├── spec                  # JAB Code specification
    └── src                   # Source code
         ├── jabcode          # JAB Code core library
         ├── jabcodeReader    # JAB Code reader application
         └── jabcodeWriter    # JAB Code writer application

##Build Instructions
The JAB Code core library, reader and writer applications are written in C (C11) and tested under Ubuntu 14.04 with gcc 4.8.4 and GNU Make 3.8.1. 

Follow the following steps to build the core library and applications. 

Step 1: Build the JAB Code core library by running make command in src/jabcode.

Step 2: Build the JAB Code writer by running make command in src/jabcodeWriter.

Step 3: Build the JAB Code reader by running make command in src/jabcodeReader.

The built library can be found in src/jabcode/build. The built reader and writer applications can be found in src/jabcodeReader/bin and src/jabcodeWriter/bin.

##Usage
The usage of jabcodeWriter and jabcodeReader can be obtained by running the programs with the argument "--help".

#####jabcodeReader
run "jabcodeReader --help" for the detailed usage

#####jabcodeWriter
run "jabcodeWriter --help" for the detailed usage

##Documentation
See https://jabcode.github.io/jabcode/



