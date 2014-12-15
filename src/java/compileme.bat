javac ./dk/cst/*.java
jar cfv "%CATALINA_HOME%/lib/bracmat.jar" dk/cst/bracmat.class
javac -classpath "%CATALINA_HOME%/lib/bracmat.jar" ./bracmattest.java
java -D"java.library.path=%CATALINA_HOME%/bin" -classpath "%CATALINA_HOME%/lib/bracmat.jar";. bracmattest
