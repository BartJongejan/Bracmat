# Bracmat

Bracmat is software for exploration and transformation of uncharted and
complex data. Bracmat employs a pattern matching technique that can
handle string data (text) as well as structured data (XML, HTML, algebraic
expressions, email, program code, ...).

Over hundred and fifty examples of Bracmat code can be found at
http://rosettacode.org/wiki/Rosetta_Code

**This distribution contains the following essential directories and files:**
* src
    * Makefile
    * bracmat.c     
      Most of the source code of the program.
    * bracmat.h
      An optional header file, if you want to compile Bracmat as a library.
    * xml.c
      Source code that implements support for reading XML-files
* Changelog
  
  A document describing changes between versions.

* README.md

  This file.
  
* bracmat.html

  The documentation in HTML format, automatically generated from the file 'help'.

* gpl.txt

  The full text of the GNU public licence.

* help

  The documentation in Bracmat format. (You have to run Bracmat and issue 'get$help' to see it properly.)
  
* howto.md

  Detailed explaination of how to use the wizzard project.bra to create a Bracmat program.
  
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
  Cyclus: edit program, save from editor, reread in  Bracmat, reload in editor, edit program.

* uni.bra

  A utility that generates UNICODE-related tables from UnicodeData.txt (http://unicode.org/Public/UNIDATA/UnicodeData.txt).
  These tables, which are also found in bracmat.c, have to be updated every few years, when UnicodeData.txt is updated.
  Conversion between lower and upper casing is based on these tables.

* valid.bra

  A test suite.

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
    cd ..

**Installation**

The Bracmat source code has no other dependencies than what is provided by
Standard C. Building bracmat from source is extremely simple, e.g.:

    gcc bracmat.c xml.c
    
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

**Testing**

The file valid.bra contains enough tests to get in all corners of the source code,
except those that lead to a controled exit condition.

For each error correction and for each new or changed feature a test must be added to valid.bra.

The testsuite valid.bra can also be used to find out how things can be programmed, although there is
no good way to quickly search for what you are looking for.

usage: start Bracmat in interactve mode and write

    get$"valid.bra"
    !r
