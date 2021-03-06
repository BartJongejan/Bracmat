$!
$! CONFIGURE MMS FILE
$!
$ 	FLAGS = "NDEBUG"
$! 	INCS1 = "/INCL=(DKA100:[USERS.FAUSAP.ILISP1060SRC.XML],"
$!	INCS2 = " DKA100:[USERS.FAUSAP.ILISP1060SRC.BIGNUM])"
$!	INCS = INCS1 + INCS2
$ 	CF1 = "/DEFINE=(''FLAGS')/FLOAT=IEEE_FLOAT"
$      	CF2 = "/L_DOUBLE_SIZE=128"
$	CF = CF1 + CF2
$!	SW = CF + INCS
$	SW = CF
$ 	WRITE SYS$OUTPUT "FEATURES USED: " + FLAGS
$ 	WRITE SYS$OUTPUT "FLAG USED: " + CF
$! 	WRITE SYS$OUTPUT "INCLUDE DIRS: " + INCS
$	WRITE SYS$OUTPUT "SWITCHES: " + SW
$ 	MMS/GENERATE/SWITCHES=COMPILE="''SW'" - 
	    /TARGET=BRACMAT.EXE BRACMAT.C,XML.C,JSON.C
$!
$! END OF FILE
$!
