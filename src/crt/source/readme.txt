INFORMATION FOR EXTRACTING SOURCE CODE FROM VIDEO.SRC AND COMPILING THEM INTO
LIBRARIES, AND ALSO SOME INFORMATION ABOUT THE LIBRARIES IN LIB\ SUBDIR.

This subdirectory contains:

 - EXTRAIA.EXE  => Utility to ungroup source code files grouped in VIDEO.SRC
                   Documentation about EXTRAIA is found in UTIL\EXTRAIA.TXT
 - VIDEO.SRC    => Archive that holds all source code files and some batch
                   files (an ASCII file, editable by any text editor)
 - README.TXT   => This file
 - VIDEO.PRJ    => Project file to compile functions with Turbo C IDE
 - VIDEO.DSK    => Desktop configurations for VIDEO.PRJ

To extract source code files and batch files from VIDEO.SRC type:
  EXTRAIA VIDEO

The line above will ungroup all .C and .H source code files and plus the
following files:
 - COMPILE.BAT  => compiles all .C files and assembles a library, you must
                   specify the memory model (s,m,c,l,h). Used by ASMLIBS
 - ASMLIBS.BAT  => compiles all .C files in small, medium, compact, large and
                   huge memory models.
 - VIDEOLIB.DAT => used by compile.bat, list of all modules to be added to
                   libraries

To compile the source code and assemble them in libraries use: 
 (after extracting files from VIDEO.SRC with EXTRAIA)
  ASMLIBS [options]      => to compile in small,medium,compact,large and huge
                            memory models (designed for TC/TC++)
    or
  COMPILE <model> [options]  => to compile only in <model> memory model
                                (s,m,c,l,h) (designed for TC/TC++)
  [options] are TCC options
  These batch files have been created for Turbo C 3.0, so if your compiler is
another, probably you will have to edit COMPILE.BAT
  COMPILE.BAT and ASMLIBS assume that TLIB and TCC are in the path.
  After being executed these batch files will create .LIB files, that can
replace the existing files in LIB subdirectory.
  You may also try to compile with VIDEO.PRJ in Turbo C++ 3.0 (or better/
compatible) IDE. (It creates VIDEO.LIB in small memory model, but you can
choose another memory model in menu Options/Compiler/Code Generation.) 

SOME WORDS ABOUT THE LIBRARY FILES STORED IN LIB\ SUBDIRECTORY

   Oddly to lib files that you create using ASMLIBS.BAT, the libraries stored
 in LIB\ subdir have been compiled with the IDE (Turbo C++ 3.0) using
 VIDEO.PRJ.
   For each library, first I've deleted all .OBJ files (to prevent the library
 from getting some functions in one memory model and other functions in other
 memory models), then I've chosen a memory model and I've compiled the
 functions. The IDE has created then a file named VIDEO.LIB which I've
 renamed to VIDEO?.LIB, where ? is the first letter of the memory model.
   After repeating this procedure for Small, Medium, Compact, Large and Huge
 memory models, I've moved all libraries to LIB\

   To choose a memory model in the IDE, go to menu
 Options|Compiler|Code generation.

   To produce smaller code and smaller library files, the options
 SUPRESS REDUNDANT LOADS (-Z in TCC) and JUMP OPTIMIZATION (-O in TCC)
 have been switched on. All comment records and debug information have been
 purged.
   To increase linking speed with these libraries, each one has a extended
 dictionary.

   To enable the functions stored in these libraries to run in every IBM
 compatible computer, they have been compiled using 8086/8088 instruction set.
 However for a better performance you can recompile them using instruction
 sets for newer processors, such as 80386, 80486, pentiums, etc.

   To recompile them with a newer instruction set with the IDE, simply
 open the VIDEO.PRJ project file and choose in
 Options/Compiler/Advanced code generation/Instruction Set
 the desired instruction set.
   To recompile them with the command line compiler, choose the option
 related to the choosen instruction set (usually -1 for 80186, -2 for 80286,
 etc).
   The draw-back of recompiling these functions with instruction sets for
 newer processors is that older processors will be unable to run these
 functions. That's why I've choose 8086/8088 instruction set when compiling
 them.

   The compiler was Turbo C++ 3.0 IDE.
