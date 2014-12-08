javac ./dk/cst/*.java
jar cfv "%TOMCAT_HOME%/lib/bracmat.jar" dk/cst/bracmat.class
javac -classpath "%TOMCAT_HOME%/lib/bracmat.jar" ./bracmattest.java
java -D"java.library.path=%TOMCAT_HOME%/bin" -classpath "%TOMCAT_HOME%/lib/bracmat.jar";. bracmattest
