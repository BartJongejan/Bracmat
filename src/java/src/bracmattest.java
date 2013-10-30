//package dk.clarin.tools.rest;
// Make sure that bracmatjni.dll is in java.library.path
// Compile with
//      javac bracmatjni.java
// Create header file with
//      javah -jni bracmatjni
// compile and link project D:\projects\Bracmat\vc\bracmatdll\bracmatdll.vcproj
// Run with
//      java bracmatjni
import dk.cst.*;

class bracmattest
    {
    public static void main(String[] args) {
        bracmat BracMat = new bracmat("");
	if(BracMat.loaded())
	    {
            String result = BracMat.Eval("tay$((x+1)^x,x,5)");
            System.out.println("result = " + result);
            result = BracMat.Eval("!v");
            System.out.println("result = " + result);
	    }
	else
	    System.out.println("eval could not be called. Reason:" + BracMat.reason());
        }
    }
