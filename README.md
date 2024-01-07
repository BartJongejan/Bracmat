# Bracmat

Bracmat is software for exploration and transformation of uncharted and
complex data. Bracmat employs a pattern matching technique that can
handle string data (text) as well as structured data (XML, HTML, JSON,
algebraic expressions, email, program code, ...).

The programming language construct for pattern matching supports associative
pattern matching (known from regular expressions), and expression evaluation
during pattern matching (comparable to guards in functional languages). The
combination of pattern matching in (semi-)structured data with associative
patterns that allow embedded expression evaluation is almost unique.

Bracmat is a good choice for tasks that require dynamic programming.

Since 2023, Bracmat can do floating point calculations. The motivation for
implementing the 'UFP' object is the long fancied ability to create
illustrations in e.g. SVG or PGM format from within Bracmat code. Therefore
its name: 'UnIfancyfied Floating Point'.
      
The only data type in UFP objects is the 64 bit double. UFP objects support
multidimensional arrays, do not allow recursion allowed and do not support
pattern matching.

[More than three hundred examples of Bracmat code](http://rosettacode.org/wiki/Category:Bracmat) can be found at
[Rosetta Code](http://rosettacode.org/wiki/Rosetta_Code)

**This distribution contains the following directories and files:**

* demo
    Folder containing Mandelbrot set generating code in four languages.
    * Mandelbrot.java
    
    * mandel.c
    
    * mandel.py
    
    * mandelbrot.bra

    * readme.md
    
* doc
    * CLIN26-poster.pdf
    
      Poster presented at the 26th Meeting of Computational Linguistics in the
      Netherlands (CLIN26) in Amsterdam on December 18, 2015.

    * LT4DH14.pdf and LT4DH-2016-poster.pdf
    
      Paper and poster presented at the Language Technology four Digital 
      Humanities workshop in Osaka, December 2016
      
    * NLPwithBracmat.pdf
    
      Tour through Bracmat for computational linguists.

    * bracmat.html _(deleted, but uploaded to latest release)_

      The documentation in HTML format, automatically generated from the file
      'help'. Run bracmat. At the prompt, enter 'get$help'. Choose 'html', 'htmltable',
      or 'htmllatex' to generate this file.

    * bracmat.pdf _(deleted, but uploaded to latest release)_
    
      Documentation in PDF format, generated from bracmat.tex, which, in turn,
      can be generated from the bracmat script 'help'.
      Run bracmat. At the prompt, enter 'get$help'. Choose 'htmllatex' to generate
      bracmat.tex. Use e.g. TeXstudio to create bracmat.pdf from bracmat.tex.
      Remember to rerun 'makeindex bracmat.idx' from the command line to update
      the index.

    * help

      The Bracmat documentation is maintained in this file. The file is in Bracmat format.
      You have to run Bracmat and issue 'get$help' to use it as for on-line help. Edits have to
      take place in a text editor.


* epoc
    * bracmat.SIS
    
      Installation program for Psion 5MX and Ericsson MC218 PDAs. This file is
      not always up to date.

* java-JNI
    
  Java and C source code for creating a JNI (Java Native Interface) so the
  Bracmat evaluator can be called from the Java programming language.
  
    * java

    Java code

    * java/bracmattest.java
        
      Example program showing how to evaluate a Bracmat expression from
      within a Java program
          
    * jva/dk/cst/bracmat.java
                
      Java code that loads the Bracmat dynamic library.
    
  * bracmatdll.cpp, 
    bracmatdll.h
    
      Source and header files that are needed for building the Windows version
      of a Bracmat JNI, which requires Bracmat to be in a dynamic linked
      library.

  * bracmattest.c
    
      Program source that links with a Bracmat dynamic library. For Linux.
      
  * compileAndTestJNI.sh
    
      Linux script to create a Bracmat JNI. This file contains a comment that
      describes the steps to create a Bracmat JNI for the Windows platform
      using Visual C++.
      
  * dk_cst_bracmat.c,
    dk_cst_bracmat.h
      
      Source and header files that are needed for building a Bracmat JNI,
      which requires Bracmat to be in a dynamic linked
      library.
      
  * makeJNI.bat
    
      Batch file for Windows to create a Bracmat JNI. Should work with a number
      of versions of Microsoft's C-compiler.

* Linux _(deleted, executable uploaded to latest release)_
    * bracmat 

      64 bit statically linked executable for Linux (tested with Ubuntu 16.04, 18.04 and 20.04)

* macOS _(deleted, executable uploaded to latest release)_
    * bracmat

      executable for Apple computers

* Python-module

    Python and Cython source code for creating a Python module 'prythat' so
    the Bracmat evaluator can be called from the Python programming language

    * build-launch.bat

      Windows batch file for creating a Bracmat module that can be included in Python programs.
      Contains two Python commands. The first compiles and links the module, the second tests the result.

    * build-launch.sh

      Linux bash script for creating a Bracmat module that can be included in Python programs.
      Contains two Python commands. The first compiles and links the module, the second tests the result.

    * launch.py

      Python program that demonstrates evaluation of Bracmat expressions, also call back to Python. 
      Tests the prythat module.  

    * prythat.pyx

      Cython source code that interfaces Python with Bracmat and vv.

    * setup.py

      Makefile for building the module.

* safe

    * bracmatso.c
    
      Source file that includes bracmat.c after turning off functionality
      that we don't want in a JNI or Python module running in a production
      system: low level file manipulations, system() calls, and exit() which
      would bring the application container down.

* singlesource
    * Makefile
      
      Builds a 'standard' edition of bracmat, a 'safe' version of bracmat,
      a version for profiling, and a version for measuring code coverage.
      
    * bracmat.c _(deleted, created when building using 'make')_
    
      All of the source code of the program in a single file. This file is generated and should not be permanently redacted by hand.
      
    * bracmat.h
    
      A header file, needed when compiling Bracmat as a library.

    * one.bra
     
      A bracmat script that combines all source code in the 'src' folder into a compilable single file, bracmat.c
      This bracmat.c replaces the original bracmat.c that was maintained by hand.
  
    * CONFIGURE.COM

      Script that configures DESCRIP.MMS

    * DESCRIP.MMS
     
      OpenVMS makefile.

* src
    * many .c and .h files
    * Makefile

      Builds bracmat in two ways: either by compiling each .c file separately before linking, or by compiling
      potu.c, which, if the SINGLESOURCE is #defined, #includes all .c files in an order that necessitates a
      minimum of declarations. (This is also the order in which the source code in bracmat.c is organized.)

      Both ways should be kept working in future development.

      * SINGLESOURCE defined
      
        All .c files (except potu.c) #included in potu.c, while making all .h
        files that contain function or global variable declarations
        ineffective.
        This way of compilation forces a (partial) order on the inclusion of
        the .c files. In general, the lower level functions are toward the top
        of the list, while the higher level functions are closer to the bottom
        of the list. This order mimics the order in the original bracmat.c
        file.

      * SINGLESOURCE not defined
      
        All .c files compiled separately and then linked. In this way, we
        ensure that each .c file includes all needed header files.

      The name 'potu', like 'bracmat', stems from the 1741 novel
      Nicolai Klimii Iter Subterraneum by Ludvig Holberg. Both are places on
      the planet Nazar that is inside the Earth. Nazar is inhabited by
      intelligent trees.

    * potu.c
    
      The main source file. Includes all other .c files if SINGLESOURCE is #defined.
    
    * unicaseconv.c + unicaseconv.h

      Conversion to lowercase or uppercase, based on data extracted from https://unicode.org/Public/UNIDATA/UnicodeData.txt
      You can update these files by cloning https://github.com/kuhumcst/letterfunc or https://github.com/BartJongejan/letterfunc 
      and running letterfunc/updateTables/UnicodeData.bra

    * unichartypes.c + unichartypes.h

      Categorization of characters in any of the general classes defined by unicode.org. Data extracted from https://unicode.org/Public/UNIDATA/UnicodeData.txt
      You can update these files by cloning https://github.com/kuhumcst/letterfunc or https://github.com/BartJongejan/letterfunc 
      and running letterfunc/updateTables/UnicodeData.bra

    * several more .c and .h files

* WebAssembly

   All that is needed to create a version of Bracmat that runs in a browser.  

    * bracmatJS.html _(deleted, executable uploaded to latest release)_
    
      Bracmat compiled to WebAssembly and Javascript using emscripten, embedded
      in a single HTML-page. Nice for toy scripts, slow.
      
      
    * edit-potuC.html.bra
    
      A Bracmat script that edits the output of the emscripten C-to-Javascript
      compiler and creates a HTML page that also contains a large amount of
      WebAssembly and a sliver of Javascript. The result is bracmatJS.html.
      
    * emscriptenHowToHTML.sh
    
      Linux batch file that runs emscripten and does some postprocessing,
      creating a Javascript version of Bracmat, bracmatJS.html.
      Requirement: emscripten 1.38.0
      emcc and bracmat must be in PATH to run this script.

   * pre.js

     Javascript file needed as argument for the --pre-js command line option of the emcc command 

* Windows _(deleted, executables uploaded to latest release)_
    * bracmat.exe

      32 bit executable for Windows

    * bracmat64.exe

      64 bit executable for Windows
      
* Changelog
  
  A document describing changes between versions.

* howto.md

  Detailed explanation of how to use the wizzard project.bra to create a Bracmat
  program.
  
* lex.bra

  Utility to spot unused variables and functions.
  It also tells you which variables are used outside their lexical scope.
  (Which is not an error, since Bracmat is dynamically scoped.)

* LICENSE

  The full text of the GNU public licence.
  
* pr-xml-utf-8.xml

  XML test file.
  
* project.bra

  A tool to initialise a new Bracmat program.
  Usage: Read this file ( get'"project.bra" ) and answer the questions.
  The created program stub has a comment, a class for the program and
  a reread procedure that reads the file, renames the file to a backup
  and saves it, thereby nicely reformatting the code. (Most comments are lost!)
  Some editors discover that the file has been changed and propose to reread
  the file. In this way, the user doesn't need to bother about formatting.
  Cyclus: edit program, save from editor, reread in  Bracmat, reload in editor,
  edit program.

* README.md

  This file.
  
* UnicodeData.bra

  A utility that generates UNICODE-related tables from UnicodeData.txt 
  (https://unicode.org/Public/UNIDATA/UnicodeData.txt).
  These tables have to be updated every few years, when UnicodeData.txt is updated.
  Conversion between lower and upper casing is based on these tables.

* valid.bra

  A test suite. Whenever possible, one or more tests are added for every new
  functionality and every bug fix.

**Usage**

* To start an interactive session

    bracmat
  
  * Quit by typing a closing parenthesis after the {?} prompt.
* To run Bracmat in batch/mode

        bracmat parm1 parm2 ...
  
  * Bracmat evaluates each parameter, from left to right. When the last parameter is evaluated, bracmat exits.
  * Parameters can be enclosed in apostrophes (Linux, Mac) or quotes (Windows). These characters must be escaped with a backslash if they are part of a parameter

        bracmat $'out$Hello\\nWorld!&out\'"Here I am!\\nWhat about you?"'    (bash)
    
        bracmat "out$Hello\nWorld!&out'\"Here I am!\nWhat about you?\""    (Windows)
    
    Notice the $ in front of the Linux parameter. This is a bash extension that makes it possible to escape apostrophes. You should see this output:
    
        Hello
        World!
        Here I am!
        What about you?
        
    In Linux, enclosing parameters in apostrophes or quotes is not necessary if you are willing to escape more characters. For example, this works:
    
        bracmat out\'Hello\&out\$world    (bash)

    On Windows, escaping is not possible in every case.
   
  * At least the first parameter must be a valid bracmat expression, appropriately quoted and 'escaped' to pass unscathed through the shell's own parameter evaluation mechanism.

**Download**

You can download Bracmat from GitHub.

    mkdir bracmat
    cd bracmat
    git init
    git remote add origin https://github.com/BartJongejan/Bracmat.git
    git pull origin master
    cd ..

**Installation**

The Bracmat source code has no other dependencies than what is provided by
Standard C. Building bracmat from source is extremely simple, e.g.:

    cd singlesource
    gcc bracmat.c -lm

or, if bracmat.c does not (yet) exist:

    cd singlesource
    make
    
You can also run 'make' in the src directory.

Bracmat requires a C99 compatible compiler since 2023. The older compilers
mentioned below will no longer be able to compile Bracmat.

Bracmat has been compiled and run on the following platforms:

* Windows 
    * Compiled with 32 bit VC 6.0 and newer, BCC 5.02, tcc. 
    * A 16 bit version existed until version 2.8
    * Bracmat can be compiled for 64 bit platforms. This gives faster multiplication of very large numbers.
* RISC-OS (Norcroft C ver 3)
* EPOC 5  (Psion 5mx, Ericsson MC218: gcc)
* HP-UNIX (gcc, cc)
* Solaris (gcc)
* Linux   (gcc)
* macOS   (xcode, gcc)

If you have plans to run Bracmat from Python, you need Python and Cython besides a C-compiler.
The software has been tested on Windows 10, 64 bit, using _Cython version 0.28.2_ and 
_Python 3.6.3_. 

**Testing**

The file valid.bra contains enough tests to get in all corners of the source code,
except those that lead to a controled exit condition.

For each error correction and for each new or changed feature a test must be added to valid.bra.

The testsuite valid.bra can also be used to find out how things can be programmed, although there is
no good way to quickly search for what you are looking for.

usage: start Bracmat in interactve mode and write

    get$"valid.bra"
    !r

**What others say**

From https://chat.stackoverflow.com/transcript/10/2015/10/8/19-20

    this programming language Bracmat

    & (   !table:? (!country.?len) ?
       & :?N
       & ( @( !arg
            :   ?
                ( %@?c ?
                & ( !c:#
                  |   !c:~<A:~>Z
                    & asc$!c+-1*asc$A+10:?c
                    & 1+!len:?len
                  | !c:" "&:?c
                  |
                  )
                & !N !c:?N
                & ~
               
    beats Perl in terms of line noise

 
The code cited in this citation can be found in full length here:  http://rosettacode.org/wiki/IBAN#Bracmat
