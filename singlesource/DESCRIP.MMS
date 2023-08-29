!++
!     Define symbols according to macro values
!==

.IFDEF DEBUG
DBG = /DEBUG
DBGOPT = /NOOPTIMIZE/DEBUG
.ELSE
DBG = /NODEBUG
DBGOPT = /OPTIMIZE/NODEBUG
.ENDIF

.IFDEF LIST
LST = /LIST
.ELSE
LST = /NOLIST
.ENDIF

.IFDEF DIAG
DIA = /DIAGNOSTICS
.ELSE
DIA = /NODIAGNOSTICS
.ENDIF

.IFDEF PCA
PCAOPT = /DEBUG
.ENDIF

!++
!      List of tools used and required symbols
!
!      !C used
!      !Executables used
!==


!++
!     Missing sources catch-all
!==

.DEFAULT
   ! No source found for $(MMS$TARGET)
   ! - Attempting to continue



!++
!   Complete application - default build item
!==

COMPLETE_APPLICATION depends_on -
     BRACMAT.EXE
        CONTINUE


!++
!   C
!==

MMS$OLB.OLB(BRACMAT=BRACMAT.OBJ) depends_on -
      BRACMAT.C -
     ,BRACMAT.H -
-!   ,DDE.H -
     ,ALPHA$LIBRARY:DECC$RTLDEF.TLB(ASSERT=ASSERT.H) -
-!   ,OS.H -
-!   ,SYS:SYS.H -
-!   ,WINDOWS.H -
     !
       $(CC) $(CFLAGS) /DEFINE=(NDEBUG)/FLOAT=IEEE_FLOAT/L_DOUBLE_SIZE=128 $(LST) $(DBGOPT) $(DIA) /OBJ=BRACMAT.OBJ BRACMAT.C
       LIBRARY/REPLACE MMS$OLB.OLB BRACMAT.OBJ
       DELETE BRACMAT.OBJ;*

MMS$OLB.OLB(JSON=JSON.OBJ) depends_on -
      JSON.C -
     ,ALPHA$LIBRARY:DECC$RTLDEF.TLB(ASSERT=ASSERT.H) -
     !
       $(CC) $(CFLAGS) /DEFINE=(NDEBUG)/FLOAT=IEEE_FLOAT/L_DOUBLE_SIZE=128 $(LST) $(DBGOPT) $(DIA) /OBJ=JSON.OBJ JSON.C
       LIBRARY/REPLACE MMS$OLB.OLB JSON.OBJ
       DELETE JSON.OBJ;*

MMS$OLB.OLB(XML=XML.OBJ) depends_on -
      XML.C -
     ,ALPHA$LIBRARY:DECC$RTLDEF.TLB(ASSERT=ASSERT.H) -
     !
       $(CC) $(CFLAGS) /DEFINE=(NDEBUG)/FLOAT=IEEE_FLOAT/L_DOUBLE_SIZE=128 $(LST) $(DBGOPT) $(DIA) /OBJ=XML.OBJ XML.C
       LIBRARY/REPLACE MMS$OLB.OLB XML.OBJ
       DELETE XML.OBJ;*


!++
!   Links
!==

BRACMAT.EXE depends_on -
      MMS$OLB.OLB(BRACMAT=BRACMAT.OBJ) -
     ,MMS$OLB.OLB(JSON=JSON.OBJ) -
     ,MMS$OLB.OLB(XML=XML.OBJ) -
-!   ,MMS$OLB.OLB(BOOLEAN=BOOLEAN.OBJ) -
-!   ,MMS$OLB.OLB(BUILD=BUILD.OBJ) -
-!   ,MMS$OLB.OLB(COMBINE=COMBINE.OBJ) -
-!   ,MMS$OLB.OLB(DISPATCHMESSAGE=DISPATCHMESSAGE.OBJ) -
-!   ,MMS$OLB.OLB(EXPAND=EXPAND.OBJ) -
-!   ,MMS$OLB.OLB(GETCURRENTTHREADID=GETCURRENTTHREADID.OBJ) -
-!   ,MMS$OLB.OLB(INTR=INTR.OBJ) -
-!   ,MMS$OLB.OLB(JSONTEST=JSONTEST.OBJ) -
-!   ,MMS$OLB.OLB(NI=NI.OBJ) -
-!   ,MMS$OLB.OLB(NII=NII.OBJ) -
-!   ,MMS$OLB.OLB(OS_SWIX=OS_SWIX.OBJ) -
-!   ,MMS$OLB.OLB(PFNC=PFNC.OBJ) -
-!   ,MMS$OLB.OLB(POSTTHREADMESSAGE=POSTTHREADMESSAGE.OBJ) -
-!   ,MMS$OLB.OLB(THEMETHOD=THEMETHOD.OBJ) -
-!   ,MMS$OLB.OLB(TRANSLATEMESSAGE=TRANSLATEMESSAGE.OBJ) -
-!   ,MMS$OLB.OLB(UNLINK=UNLINK.OBJ) -
-!   ,MMS$OLB.OLB(WININ=WININ.OBJ) -
-!   ,MMS$OLB.OLB(WINOUT=WINOUT.OBJ) -
-!   ,MMS$OLB.OLB(ACTION=ACTION.OBJ) -
-!   ,MMS$OLB.OLB(JSTATE=JSTATE.OBJ) -
-!   ,MMS$OLB.OLB(ESTATE=ESTATE.OBJ) -
-!   ,MMS$OLB.OLB(XPUT=XPUT.OBJ) -
     !
       LINK $(DBG) $(PCAOPT) /EXE=BRACMAT.EXE MMS$OLB.OLB/LIBRARY/INCLUDE=(BRACMAT) -
          ! End of Link




!++
!   Files not found.  MMS references to these files will generate errors.
!==

!     DDE.H
!     OS.H
!     SYS:SYS.H
!     WINDOWS.H



!++
!   Objects not found.  The following symbols may be undefined.
!==

!     ACTION
!     BOOLEAN
!     BUILD
!     COMBINE
!     DISPATCHMESSAGE
!     ESTATE
!     EXPAND
!     GETCURRENTTHREADID
!     INTR
!     JSONTEST
!     JSTATE
!     NI
!     NII
!     OS_SWIX
!     PFNC
!     POSTTHREADMESSAGE
!     THEMETHOD
!     TRANSLATEMESSAGE
!     UNLINK
!     WININ
!     WINOUT
!     XPUT

!++
!  Create object library if it doesn't already exist
!==

.FIRST
	! MMS $(mmsqualifiers) $(mmstargets)
       IF F$SEARCH( "MMS$OLB.OLB" ) .EQS. "" -
		THEN $(LIBR)/CREATE MMS$OLB.OLB

!++
!  End of build cleanup work
!==

.LAST
       CONTINUE

