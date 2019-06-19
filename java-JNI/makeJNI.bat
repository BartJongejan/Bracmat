REM TODO:
REM Edit JAVA_HOME and CATALINA_HOME as needed
REM Stop the Tomcat service
REM Run this script as administrator. (Required when the script copies bracmat.dll to the Tomcat bin catalogue.)

REM First parameter: x64 | x86 | amd64 | x86_amd64 | x86_arm | x86_arm64 | amd64_x86 | amd64_arm | amd64_arm64
REM second parameter: 10 | 12 | 2017 | 2019
REM For example:  makeJNI.bat x64 2019

@echo off
SET dirvar=%cd%
if "%1" == "" goto :usage

del *.obj
del *.dll
del *.exp
del *.lib
del dk_cst_bracmat.h
del java\*.class
del java\*.jar
del java\dk\cst\*.class

rem set "JAVA_HOME=C:\Program Files\Java\jdk1.7.0_45"
rem set "JAVA_HOME=C:\Program Files\Java\jdk1.7.0_10"
rem set "JAVA_HOME=C:\Program Files\Java\jdk1.8.0_31"
rem set "CATALINA_HOME=C:\Program Files\Apache Software Foundation\Tomcat 7.0"
rem set "CATALINA_HOME=C:\Program Files\Apache Software Foundation\Tomcat 8.0"
rem set "CATALINA_HOME=C:\Program Files\Apache Software Foundation\Tomcat 8.5"
if "%1" == "x86" goto :w32 
if "%2" == "12" goto :v12
if "%2" == "10" goto :v10
if "%2" == "2017" goto :v2017
if "%2" == "2019" goto :v2019
if "%2" == "" goto :compile
call "C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\vcvarsall.bat" %1
goto :compile

:v10
call "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\vcvarsall.bat" %1
goto :compile

:v12
call "C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\vcvarsall.bat" %1
goto :compile

:w32
if "%2" == "2017" goto :v2017
if "%2" == "2019" goto :v2019
call "C:\Program Files\Microsoft Visual Studio 10.0\VC\vcvarsall.bat" %1
goto :compile

:v2017
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" %1
goto :compile

:v2019
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" %1
goto :compile

:compile
cd %dirvar%
cd java
javac ./dk/cst/*.java -h ..  -Xlint
cd %dirvar%

cl /I"%JAVA_HOME%\include" /I"%JAVA_HOME%\include\win32" /O2 /Oi /GL /DWIN32 /DNDEBUG /D_WINDOWS /D_USRDLL /DBRACMATDLL_EXPORTS /D_WINDLL /MT /Gy /Gd bracmatdll.cpp ../safe/bracmatso.c dk_cst_bracmat.c mutex.cpp ../src/json.c ../src/xml.c /link /OUT:"bracmat.dll" /DLL /OPT:REF /OPT:ICF /MACHINE:%1

SET dirvar=%cd%
cd java
REM *** obsolete javah *** javah -d ../ dk.cst.bracmat ----> Now: -h option in javac
jar cfv bracmat.jar dk/cst/bracmat.class
javac -classpath bracmat.jar ./bracmattest.java
java -D"java.library.path=../" -classpath bracmat.jar;. bracmattest
copy /Y bracmat.jar "%CATALINA_HOME%\lib\bracmat.jar"
cd %dirvar%

move /Y bracmat.dll "%CATALINA_HOME%\bin\bracmat.dll"
goto :eof

:usage
echo Options required: x86 or x64 and 9, 10 or 12 or v2017 or v2019
goto :eof
