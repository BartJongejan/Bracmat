*******
* Run *
*******

emscriptenHowToHTML.bat

This generates bracmat.html. 



********
* Edit *
********

bracmat.html



***************************************************************************************
* Insert this between <body> and <script type="text/javascript">, replacing old text: *
***************************************************************************************

	<h1>Bracmat ... in your browser ...</h1>
	<p>This is Bracmat, translated to JavaScript, with almost complete functionality.<br>
	Read the <a href="http://jongejan.dk/bracmat.html">documentation</a>. 
	Find the full, more than 100 times faster version of the program on <a href="https://github.com/BartJongejan/Bracmat">GitHub</a>.<br>
	See some examples on <a href="http://rosettacode.org/wiki/Category:Bracmat">rosettacode.org</a>.</p>
    <textarea class="emscripten" id="input" value="" rows="30">({Dinesman's
 multiple-dwelling problem. See 
rosettacode.org/wiki/Dinesman's_multiple-dwelling_problem}
    
    Baker Cooper Fletcher Miller Smith:?people
  &amp; ( constraints
    =   
      .   !arg
        : ~(? Baker)
        : ~(Cooper ?)
        : ~(Fletcher ?|? Fletcher)
        : ? Cooper ? Miller ?
        : ~(? Smith Fletcher ?|? Fletcher Smith ?)
        : ~(? Cooper Fletcher ?|? Fletcher Cooper ?)
    )
  &amp; ( solution
    =   floors persons A Z person
      .   !arg:(?floors.?persons)
        &amp; (   !persons:
            &amp; constraints$!floors
            &amp; out$("Inhabitants, from bottom to top:" !floors)
            &amp; ~     { The ~ always fails on evaluation. Here, 
failure forces Bracmat to backtrack and find all solutions, not just the
 first one. }
          |   !persons
            :   ?A
                %?`person
                (?Z&amp;solution$(!floors !person.!A !Z))
          )
    )
  &amp; solution$(.!people)
|                   { After outputting all solutions, the lhs of the | 
operator fails. The rhs of the | operator, here an empty string, is the 
final result. }
);</textarea>
    <input value="evaluate" onclick="takeinput()" type="button">
    <textarea class="emscripten" id="output" rows="24"></textarea>



******************************************************************
* Near the start of the JavaScript, in function 'print', replace *
******************************************************************

            element.value += text + "\n";

by
            element.value += text;



***********************************
* Replace the function _myputc by *
***********************************

    function _myputc($c) {
     var label = 0;
     var $1;
     $1=$c;
     var $2=$1;
     var $3=HEAP32[((20352)>>2)];
     Module.print(String.fromCharCode($2));
     //var $4=_fputc($2, $3);
     return;
    }




*********************************************
* Insert this script before the </body> tag *
*********************************************


    <script>
    function execute(text) {
      var out;
      var cstyle_ptr = allocate(intArrayFromString(text), 'i8', ALLOC_NORMAL);
      Module.print("{?} "+text+"\n{!} ");
      out = _oneShot(cstyle_ptr);
    }
    function takeinput() {
    var text = document.getElementById('input').value;
    execute(text);
    }
    _startProc();
    //execute("4:?a;!a^2+1;");
    </script> 