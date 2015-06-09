#!/bin/bash 

# Run these lines on devtools, not infra!
# After running these lines, go to infra and continue with
# deploying bracmat.jar. See bottom of this file.
# WARNING! deploying bracmat.jar 
#
# Edit the scp commandos to copy stuff to your own home directory on infra.
#
# CWD must contain bracmat.c, e.g. 
# /home/bart/trunk/dkclarin-services/tools/Bracmat
# Copy one block of text (consecutive lines without intervening blank lines)
# at a time to the command line and check that the test at the end of a 
# block went ok. (If there is a test.)

# Create stand-alone bracmat.
gcc -std=c99 -pedantic -Wall -O2 -DNDEBUG bracmat.c xml.c json.c
mv a.out bracmat
# Test the stand-alone version of Bracmat.
# Expect a line only saying 'bracmat alife and kicking'.
./bracmat 'put$"bracmat alife and kicking\n"'

# Compile bracmat.c as relocatable code for shared object
# Create bracmatso.o, xml.o and json.o
gcc -std=c99 -pedantic -Wall -O2 -c -fPIC -DNDEBUG bracmatso.c xml.c json.c
cd java

JDK_DIRS="/usr/lib/jvm/java /usr/lib/jvm/default-java ${OPENJDKS} /usr/lib/jvm/java-6-openjdk /usr/lib/jvm/java-6-sun /usr/lib/jvm/java-7-oracle"
# Look for the right JVM to use
for jdir in $JDK_DIRS; do
    if [ -r "$jdir/bin/java" -a -z "${JAVA_HOME}" ]; then
	JAVA_HOME="$jdir"
    fi
done
export JAVA_HOME
# Compile java class that loads shared object libbracmat.so.
javac ./dk/cst/*.java

# Make header file for C code that exposes methods to Java, 
# store header file between C code one level up.
javah -d ../ dk.cst.bracmat
# Jar the java class that interfaces the native code.
jar cfv bracmat.jar dk/cst/bracmat.class

cd ..
# Compile the C code that exposes methods to Java.
gcc -std=c99 -pedantic -Wall -pthread -c -fPIC -DNDEBUG -I$JAVA_HOME/include -I$JAVA_HOME/include/linux/ dk_cst_bracmat.c -o dk_cst_bracmat.o    
# Link the two object files into a shared library (REALNAME).
gcc -shared -Wl,-soname,libbracmat.so.1 -o libbracmat.so.1.0 bracmatso.o xml.o json.o dk_cst_bracmat.o -lpthread
# Link REALNAME to SONAME.
ln -sf libbracmat.so.1.0 libbracmat.so.1
# Link SONAME to LINKERNAME.
ln -sf libbracmat.so.1 libbracmat.so
# Compile and link test proggram that uses shared object.
gcc -std=c99 -pedantic -Wall -L. -DNDEBUG bracmattest.c -lbracmat -o bracmattest
# Set up library path.
LD_LIBRARY_PATH=`pwd`:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH
# Test shared library.
# Expect {?} prompt. Type e.g. 1+3 <Enter> Answer {!} 4  
# Exit with Ctrl-C.
# (Uncomment line below if you don't 'source' this script, but copy it block-wise)
#./bracmattest 

# move shared lib to a place in the java.library.path of JBoss.
# (You may have to enter your password, so we make this a separate block.)
sudo cp libbracmat.so.1.0 /usr/lib
# The normal thing to do would be
#   sudo /sbin/ldconfig
# But we define the symbolic links by hand.
# Link REALNAME to SONAME.
sudo ln -sf /usr/lib/libbracmat.so.1.0 /usr/lib/libbracmat.so.1
# Link SONAME to LINKERNAME.
sudo ln -sf /usr/lib/libbracmat.so.1 /usr/lib/libbracmat.so
# Delete local symbolic links.
rm libbracmat.so
rm libbracmat.so.1
# Compile and link test proggram that uses shared object in offical place.
gcc -std=c99 -pedantic -Wall -DNDEBUG bracmattest.c -lbracmat -o bracmattest
# Test Java-app using global JNI.
# Expect {?} prompt. Type e.g. 1+3 <Enter> Answer {!} 4 
# Exit with Ctrl-C.
# (Uncomment line below if you don't 'source' this script, but copy it block-wise)
#./bracmattest

cd java
#compile java test app
javac -classpath bracmat.jar ./bracmattest.java

#run the test app
#sudo, because bracmat may want to write /var/log/clarin/tools.log
# Expect:
# result = 1+x^2+-1/2*x^3+5/6*x^4+-3/4*x^5
# result = Bracmat version 6, build 156 (6 May 2013)
# Version should be in accordance with what is stated in /home/bart/bracmat/bracmat.c +/- line 47
# #define DATUM "6 May 2013"
# #define VERSION "6"
# #define BUILD "156"
sudo java -classpath bracmat.jar:. bracmattest

#copy bracmat.jar to final destination(s) (Platform-dependend!)
if [ -d /usr/local/jboss ]; then
    #devtools:
    sudo cp -p ./bracmat.jar /usr/local/jboss/server/all/lib/
    sudo cp -p ./bracmat.jar /usr/local/jboss/server/default/lib/
else 
    if [ -d /usr/share/tomcat7/lib ]; then
        sudo cp -p ./bracmat.jar /usr/share/tomcat7/lib/
    fi
fi

# Windows, Visual Studio C++ (2008, 2010 Express):
# CREATE A DLL
# ------------
#       (Warning: not thread safe in Windows!)
# Create a Win32 Project "bracmatdll"
# In the Win32 Application Wizard, choose Application type "DLL", Additional option "Empty project". (Later we are going to kill off the precompiled header, which you cannot deselect in the wizard.)
# Add existing items bracmatdll.cpp, bracmatso.c, dk_cst_bracmat.c, xml.c, json.c to source files
# Edit the properties for the project:
#   General
#       Target Name
#-->        bracmat
#   C/C++
#       General
#           Additional Include Directories
#-->          C:\Program Files (x86)\Java\jdk1.7.0_10\include;C:\Program Files (x86)\Java\jdk1.7.0_10\include\win32
#-->          (Make sure to point to the installed version of the JDK, C:\Program Files (x86)\Java\jdk1.7.0_10 is just an example.)
#       Preprocessor
#           Preprocessor Definitions: 
#-->            Make sure that BRACMATDLL_EXPORTS is defined (If you called the project something different from "bracmatdll" you will have to change the <projectname>_EXPORTS string)
#               To turn off warnings against using certain standard C functions, add _CRT_SECURE_NO_DEPRECATE
#       Code generation
#           Runtime library: to make the DLL less dependent on other DLLs, you can change to
#               Multi-threaded (/MT)
#   Linker
#       Debugging
#           Generate Debug Info
#               No
#
# To test DLL with Java app
# -------------------------
#
# cd java
# // next lines are in java\compileme.bat as well
# javac ./dk/cst/*.java
# jar cfv "%CATALINA_HOME%/lib/bracmat.jar" dk/cst/bracmat.class
# javac -classpath "%CATALINA_HOME%/lib/bracmat.jar" ./bracmattest.java
# java -D"java.library.path=%CATALINA_HOME%/bin" -classpath "%CATALINA_HOME%/lib/bracmat.jar";. bracmattest
