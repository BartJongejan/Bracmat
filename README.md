# Bracmat

Bracmat is software for exploration and transformation of uncharted and
complex data. Bracmat employs a pattern matching technique that can
handle string data (text) as well as structured data (XML, HTML, JSON,
algebraic expressions, email, program code, ...).

Over hundred and fifty examples of Bracmat code can be found at
http://rosettacode.org/wiki/Rosetta_Code

**This distribution contains the following directories and files:**
* doc
    * bracmat.html

      The documentation in HTML format, automatically generated from the file
      'help'.

    * bracmat-table.html

      The documentation in HTML format, automatically generated from the file
      'help' and using an alternative lay-out with tables.

    * CLIN26-poster.pdf
    
      Poster presented at the 26th Meeting of Computational Linguistics in the
      Netherlands (CLIN26) in Amsterdam on December 18, 2015.

    * help

      The documentation in Bracmat format. (You have to run Bracmat and issue
      'get$help' to see it properly.)
      
    * LT4DH14.pdf and LT4DH-2016-poster.pdf
    
      Paper and poster presented at the Language Technology four Digital 
      Humanities workshop in Osaka, December 2016
      
    * NLPwithBracmat.pdf
    
      Tour through Bracmat for computational linguists.
      

* epoc
    * bracmat.SIS
    
      Installation program for Psion 5MX and Ericsson MC218 PDAs. This file is
      not always up to date.
      
* src
    * java
    
      Java source code for creating a JNI (Java Native Interface) so the
      Bracmat evaluator can be called from the Java programming language.
      
        * bracmattest.java
        
          Example program showing how to evaluate a Bracmat expression from
          within a Java program
          
        * dk
            * cst
                * bracmat.java
                
                  Java code that loads the Bracmat dynamic library.
    
    * Python
      
      Python and Cython source code for creating a Python module 'prythat' so
      the Bracmat evaluator can be called from the Python programming language
        
        * build-launch.bat

          Contains two lines. The first line compiles and links the module,
          the second tests the result.
        
        * launch.py
        
          Tests the prythat module.  

        * prythat.pyx

          Cython source code that interfaces Python with Bracmat and vv.

        * setup.py

          Makefile for buidling the module.
                      
    * Makefile
      
      Builds the standard edition of bracmat, a "safe" version of bracmat,
      a version for profiling, and a version for code coverage.
      
    * bracmat.c
    
      Most of the source code of the program.
      
    * bracmat.h
    
      An optional header file, if you want to compile Bracmat as a library.
      
    * bracmatJS.html
    
      Bracmat compiled to Javascript using emscripten, embedded in a single
      HTML-page. Nice for toy scripts, slow.
      
    * bracmatdll.cpp, 
    * bracmatdll.h
    
      Source and header files that are needed for building the Windows version
      of a Bracmat JNI, which requires Bracmat to be in a dynamic linked
      library.
      
    * bracmatso.c
    
      Source file that includes bracmat.c after turning off functionality
      that we don't want in a JNI running in a production system: low level
      file manipulations, system() calls, and exit() which would bring the
      application container down.
      
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
      
    * dlltest.cpp,
      dlltest.h
      
      Program source that links with a Bracmat dynamic library. For Windows.
      
    * editbracmatjs.bra
    
      A Bracmat script that edits the output of the emscripten C-to-Javascript
      compiler and creates a HTML page containing Javascript embodying Bracmat.
      
    * emscriptenHowToHTML.bat
    
      Windows batch file that runs emscripten and does some postprocessing,
      creating a Javascript version of Bracmat.
      
    * json.c
    
      Source code that implements support for reading JSON-files
      
    * makeJNI.bat
    
      Batch file for Windows to create a Bracmat JNI. Should work with a number
      of versions of Microsoft's C-compiler.
      
    * xml.c
    
      Source code that implements support for reading XML-files
    
* Changelog
  
  A document describing changes between versions.

* README.md

  This file.
  
* gpl.txt

  The full text of the GNU public licence.

* howto.md

  Detailed explanation of how to use the wizzard project.bra to create a Bracmat
  program.
  
* lex.bra

  Utility to spot unused variables and functions.
  It also tells you which variables are used outside their lexical scope.
  (Which is not an error, since Bracmat is dynamically scoped.)
  
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

* uni.bra

  A utility that generates UNICODE-related tables from UnicodeData.txt 
  (http://unicode.org/Public/UNIDATA/UnicodeData.txt).
  These tables, which are also found in bracmat.c, have to be updated every
  few years, when UnicodeData.txt is updated.
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

    gcc bracmat.c xml.c json.c
    
You can also use the Makefile, which is in the src directory.    

Bracmat has been compiled and run on the following platforms:

* Windows 
    * All versions, compiled with 32 bit VC 6.0 and newer, BCC 5.02, tcc. 
    * A 16 bit version existed until version 2.8
    * Bracmat can be compiled to 64 bits. This gives faster multiplication of very large numbers.
* RISC-OS (Norcroft C ver 3)
* EPOC 5  (Psion 5mx, Ericsson MC218: gcc)
* HP-UNIX (gcc, cc)
* Solaris (gcc)
* Linux   (gcc)
* MacOS   (xcode)

If you have plans to run Bracmat from Python, you need Python and Cython besides a C-compiler.
If you want to do this on a Windows platform, you need Visual Studio C++ 2008 (Correct me if there
are ways to use other C++ compilers). The software has been tested on Windows 7 and 8.1, 
both 64 bit platforms, using _Cython version 0.22.1_ and 
_Python 2.7.3 (default, Apr 10 2012, 23:31:26) [MSC v.1500 32 bit (Intel)] on win32_ 
(or _Python 2.7.10 (default, May 23 2015, 09:40:32) [MSC v.1500 32 bit (Intel)] on win32_).
On Linux ( _Ubuntu 14.04.2 LTS_ 64 bit), the software is tested with _Cython version 0.22_, 
_Python 2.7.6 (default, Mar 22 2014, 22:59:56)_ and _gcc version 4.8.2_.
Contact me if you want a compiled module for Windows and do not have the compiler to build it.

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

(https://chat.stackoverflow.com/transcript/10/2015/10/8/19-20)
 
This code in full length is found here:  http://rosettacode.org/wiki/IBAN#Bracmat