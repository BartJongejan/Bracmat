cd src
javac -d ../classes/ ./dk/cst/*.java
cd ../classes
jar cfv ../jar/bracmat.jar dk/cst/bracmat.class
cd ..
javac -d ./jar/ -classpath ./jar/bracmat.jar ./src/bracmattest.java
REM java -Djava.library.path="C:/Users/zgk261/Documents/Visual Studio 2010/Projects/Bracmat/Release/" -classpath ./jar/bracmat.jar;./jar bracmattest
java -Djava.library.path="C:/utils/" -classpath ./jar/bracmat.jar;./jar bracmattest
