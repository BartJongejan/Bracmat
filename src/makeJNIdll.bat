set "JAVA_HOME=C:\Program Files\Java\jdk1.7.0_45"
set "TOMCAT_HOME=C:\Program Files\Apache Software Foundation\Tomcat 7.0"
call "C:\Program Files\Microsoft Visual Studio 10.0\VC\vcvarsall.bat"
cl /I"%JAVA_HOME%\include" /I"%JAVA_HOME%\include\win32" /O2 /Oi /GL /DWIN32 /DNDEBUG /D_WINDOWS /D_USRDLL /DBRACMATDLL_EXPORTS /D_WINDLL /MT /Gy /Gd bracmatdll.cpp bracmatso.c dk_cst_bracmat.c json.c xml.c /link /OUT:"bracmat.dll" /DLL /OPT:REF /OPT:ICF
move bracmat.dll "%TOMCAT_HOME%\bin\bracmat.dll"
