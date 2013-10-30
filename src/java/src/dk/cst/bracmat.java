//package dk.clarin.tools.rest;
// Make sure that bracmat.dll is in java.library.path
// Compile with
//      javac dk\cst\bracmat.java
// Create header file with
//      javah -jni bracmat
// compile and link project D:\projects\Bracmat\vc\bracmatdll\bracmatdll.vcproj
// Run with
//      java bracmat
package dk.cst;

//import java.util.logging.*;

public class bracmat
    {
    //private static Logger logger = Logger.getLogger("dk.cst.bracmat");
    //private static FileHandler fh;

    private static boolean Loaded;
    private static String Reason;
    private native String eval(String expr);
    private native int init(String expr);
    private native void end();
    public String Eval(String expr)
        {
        if(Loaded)
            return eval(expr);
        else
            return Reason;
        }
    public bracmat(String expr)
        {
        //logger.info("bracmat(" + expr + ") called !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");

        if(Loaded)
            {
            //logger.info("bracmat is loaded");
            init(expr);
            }
        else
            {
            ;//logger.info(Reason);
            }
        }
    public boolean loaded()
        {
        return Loaded;
        }
    public String reason()
        {
        return Reason;
        }
    protected void finalize() throws Throwable
        {
        //logger.info("finalize() called !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        if(Loaded)
            {
            //logger.info("bracmat is loaded");
            //end();/* end() should only be called if Bracmat is going to exit. However, a JNI-library does not exit! */
            }
        else
            {
            ;//logger.info(Reason);
            }
        }
    static 
        {
        try
            {
	    //fh = new FileHandler("/var/log/clarin/tools.log");
            //fh = new FileHandler("/var/log/clarin/dk.cst.bracmat.log");
            //fh = new FileHandler("/home/bart/tools.log");
            }
        catch(Throwable t) 
            {
            ;
            }
        //logger.addHandler(fh);
        // Request that every detail gets logged.
        //logger.setLevel(Level.ALL);
        // Log a simple INFO message.
        //logger.info("doing loadBracmatLibrary");
        if(!loadBracmatLibrary())
            ;//logger.info("Sorry: no bracmat this time!");

        //logger.fine("loadBracmatLibrary done");
        /*
        Loaded = true;
        Reason = "";
        try 
        {
        System.loadLibrary("bracmat");
        }
        catch (UnsatisfiedLinkError e) 
        {
        Loaded = false;
        Reason = "UnsatisfiedLinkError: Native code library failed to load.\n" + e + " java.library.path=" +  System.getProperty("java.library.path");
        }
        catch (SecurityException e)
        {
        Loaded = false;
        Reason = "SecurityException: Native code library failed to load.\n" + e + " java.library.path=" +  System.getProperty("java.library.path");
        }
        */ 
        }
    public synchronized static boolean loadBracmatLibrary() 
        {
        // Prevent multiple initialization.
        if (BracmatInitialized) 
            {
            return true;
            }

        //logger.info("Loading Bracmat Library...");
        //logger.info("java.library.path: " + System.getProperty("java.library.path"));
        //logger.info("java.class.path: " + System.getProperty("java.class.path"));
        //log.info("Loading Bracmat Library...");
        //log.info("System PATH: " + System.getProperty("java.library.path"));

        Loaded = true;
        Reason = "";
        try {
            System.loadLibrary("bracmat");
            //logger.info("System.loadLibrary(\"bracmat\") is successful.");
            //logger.info("About to check Classes in bracmat.jar ...");
            //log.info("System.loadLibrary(\"bracmat\") is successful.");
            //log.info("About to check Classes in bracmat.jar ...");
            checkClass(bracmat.class);
            //logger.info("bracmat passed.");
            //log.info("bracmat passed.");
            }
        catch (UnsatisfiedLinkError e)
            {
            Loaded = false;
            Reason = "Native code library failed to load.\n" + e + ", java.library.path=" +  System.getProperty("java.library.path");
            //logger.log(Level.SEVERE , "Bracmat API initialization failed: " + e.getMessage(), e);
            return false;
            }
        catch (SecurityException e)
            {
            Loaded = false;
            Reason = "Native code library failed to load.\n" + e + ", java.library.path=" +  System.getProperty("java.library.path");
            //logger.log(Level.SEVERE , "Bracmat API initialization failed: " + e.getMessage(), e);
            return false;
            }

        catch(Throwable t) 
            {
            Loaded = false;
            Reason = "Bracmat API initialization failed: " + t.getMessage();
            //logger.log(Level.SEVERE , "Bracmat API initialization failed: " + t.getMessage(), t);
            //log.error("Bracmat API initialization failed: " + t.getMessage(), t);
            return false;
            }

        BracmatInitialized = true;
        return true;
        }

    private static void checkClass(Class c) throws Exception 
        {
        //logger.info("checking Class");
        if (c.getClassLoader() != bracmat.class.getClassLoader()) 
            {
            throw new Exception ("Class " + c.getName() + " not loaded by correct class loader");
            }
        //logger.info("Class checked");
        }

    //private static Log log = LogFactory.getLog(bracmat.class);


    private static boolean BracmatInitialized = false;
    }
