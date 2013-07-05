**Create a Bracmat program in a text file**

Start Bracmat in interactive mode

    > bracmat

From the Bracmat prompt, read the file project.bra

    {?} get$"project.bra"  { Mind the double quotes }

Start the program just loaded

    {?} new$project

Follow the directions. 

    This is an example:
    Enter name for program [prog.bra]:hello
    Enter name for class [X]:
    Enter description of program (finish with two empty lines):
    A demonstration


    Enter the names of member variables for class X. Finish with an empty line.
    geoentity
    version

    Enter the names of methods for class X. Finish with an empty line.
    do-it

    done

Then flows a lot of code over the screen that you just ignore.
Leave bracmat with a closing parenthesis and your note of approval:

    {?} )y
    
Reopen bracmat

    > bracmat
    
Read the project you just created. The program file has got the extension .bra!

    {?} get$"hello.bra"
    
Do a first reread (this step is not necessary, but is good for getting the
coding rithm right.)

    {?} !r
    
Open the file in an editor. Do not leave bracmat! By far the handiest is an
editor that senses when the loaded text has been changed outside the editor,
as happens when you issue the !r command.

The file looks like this:

    {hello.bra

    A demonstration}

    X=
      (geoentity=)
      (version=)
      (do-it=.)
      (new=.);

    r=
      get'"hello.bra"
    & rmv$(str$(hello ".bak"))
    & ren$("hello.bra".str$(hello ".bak"))
    & put$("{hello.bra

    A demonstration}

    ","hello.bra",NEW)
    & lst'(X,"hello.bra",APP)
    & put'(\n,"hello.bra",APP)
    & lst'(r,"hello.bra",APP)
    & put$(str$("\nnew'" X ";\n"),"hello.bra",APP);

    new$X;

When you issue the !r command, four things happen:
* hello.bra is read again
* the hello.bra that is on disk is renamed to hello.bak
  If hello.bak already existed it is replaced
* Rewrite all that is in hello.bra, employing bracmat's built-in facility to
  nicely format the code.
* execute the code by creating an object like X and calling the method called
 'new'
 
As you see, the body of the new method is still empty, and no local variables
are declared. Let us create a local variable i and let us call the method do-it
a few times. The method do-it does not much, so just let it say "hello".

        ...
      (do-it=.out$hello)
      (new=i.0:?i & whl'(!i+1:<10:?i&(its.do-it)$));
        ...
        
The code already has become a bit terse, so we save the file and do the !r
command after the bracmat prompt in the other window.

    {?} !r
    hello
    hello
    hello
    hello
    hello
    hello
    hello
    hello
    hello
    {!}
    new'X;
    
Apart from all these greetings, the program file is already reformatted. Look
in the editor window.

       ...
      (do-it=.out$hello)
      ( new
      =   i
        .   0:?i
          & whl'(!i+1:<10:?i&(its.do-it)$)
      );
       ...

Now we want to change the code but are not so sure that the program should be
rerun in full length every time !r is issued. A handy trick is to use the OR
operator | in the 'new' method:

       ...
      ( new
      =   i
        .  | 0:?i
          & whl'(!i+1:<10:?i&(its.do-it)$)
      );
       ...

Now when 'new' is evaluated, the left hand side of the | operator is just
nothing, which is a perfectly valid thing to do, and which is done with
success. Therefore, the right hand side of the | operator is not evaluated at
all. When the time has come to let the code run in full length, we can either
remove the | operator or let the left hand side of the | operator fail, like so

       ...
      ( new
      =   i
        .  ~ | 0:?i
          & whl'(!i+1:<10:?i&(its.do-it)$)
      );
       ...

Let's do the !r command to see what we have got:

       ...
      ( new
      =   i
        .   ~
          |   0:?i
            &   whl
              ' (!i+1:<10:?i&(its.do-it)$)
      );
       ...

There are also a few member variables. Let us see how they could be used.
One of the variables we set directly in the source code to something else than
an empty string expression. The other variable we set in the 'new' method.
Both member variables are consulted.

       ...
    X=
      (geoentity=)
      (version=1.0)
      (do-it=.out$(hello !(its.geoentity)))
      ( new
      =   i
        . 
         out$!(its.version) &
          (~
          |   0:?i
            &   whl
              ' (!i+1:<10:?i&str$(!i (!i:1&st|!i:2&nd|!i:3&d|th)) world:?(its.geoentity)&(its.do-it)$)    )
      );
       ...

The source code looks terrible, so we save the source code and do the !r
command:

    {?} !r
    1.0
    hello 1st world
    hello 2nd world
    hello 3d world
    hello 4th world
    hello 5th world
    hello 6th world
    hello 7th world
    hello 8th world
    hello 9th world
    {!}
    new'X;

The source code now looks like this:

       ...
    X=
      (geoentity=)
      (version=1.0)
      (do-it=.out$(hello !(its.geoentity)))
      ( new
      =   i
        .   out$!(its.version)
          & ( ~
            |   0:?i
              &   whl
                ' ( !i+1:<10:?i
                  &       str
                        $ ( !i
                            ( !i:1&st
                            | !i:2&nd
                            | !i:3&d
                            | th
                            )
                          )
                        world
                    : ?(its.geoentity)
                  & (its.do-it)$
                  )
            )
      ); 
       ...

If you make a mistake by not correctly matching opening and closing parentheses
or braces or not pairing double quotes, you should watch out. Bracmat will
complain and perhaps propose that it writes the erroneous input to a file for
closer inspection. Ignore that. Instead, read carefully what it says is wrong.
You may have too few closing parenthesis or to many. Perhaps you copied some
text in from somewhere that contains a unpaired double quote. Or perhaps you
wrote a brace instead of a parenthesis. Find the error in the text and do !r
again. If a closing parenthesis is missing but you cannot find out where, put
one somewhere near the end of where you have been editing the source code and
do an !r. If you use the trick with the | operator and had the ~ as its left
hand side, remove the ~ before issuing the !r command, so nothing really will
happen. After the !r command, check the source code and see whether the source
code is indented in the way you expected, and otherwise move that parenthesis
to another point.

As you may have noticed, bracmat tries to not let lines grow too long. When it,
heuristically, thinks a line is too long to be appreciated by the human eye, it
spreads the expression over more than one lines in a way so that noen of the
lines are 'too long'. Also notice that bracmat always puts corresponding
parentheses on the same line or in the same column, and that the operator that
is at the head of the expression between parentheses, is in the same column as
the parentheses, if the expression is spread out. Also, in many cases Bracmat
deems it not necessary to use parentheses and leaves them out. You can see
several examples in the source code above. If you are not sure whether
parentheses are needed to group some expresions, use them and then wait and see
what Bracmat does to them.

The comments that you might write in your program code get lost when using the
approach describe above. Comments are always discarded by Bracmat upon reading,
so there is no way to get them back in. The comment at the top of the program
code is recreated each time when you do the !r command from the contents of
a string near the bottom. If you want to change the comment, you should do that
in the string representation near the bottom of the file, not in the comment
itself. If you really need to comment a piece of code, you can put some text
in a string that sits somewhere in the code without being of any programmatical
use. Do not overuse this, because the evaluation of such strings eats some
cycles and slows the program a bit.

    & ...
    & "This is my comment."
    & ...
    
    