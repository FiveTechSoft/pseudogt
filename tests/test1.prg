#include "inkey.ch"
#include "hwgui.ch"
#include "..\hwpgt.ch"

#define CLR_WHITE    0xffffff
#define CLR_BLACK    0x000000
#define CLR_DGRAY1   0x333333
#define CLR_DGRAY2   0x606060
#define CLR_DGRAY3   0x888888
#define CLR_DGRAY4   0xdddddd

REQUEST HB_CODEPAGE_RU866
REQUEST HB_CODEPAGE_RU1251
REQUEST HB_CODEPAGE_UTF8

STATIC oPGT

FUNCTION Main()

   LOCAL oMain, oFont := HFont():Add( "Liberation Mono", 0, 18,, 204 )
   LOCAL i, oFontBtn := HFont():Add( "Georgia", 0, 18,, 204 )
   LOCAL aCorners := { 4,4,4,4 }
   LOCAL aStylesBtn := { HStyle():New( { CLR_DGRAY2 }, 1, aCorners ), ;
      HStyle():New( { CLR_WHITE }, 2, aCorners ), ;
      HStyle():New( { CLR_DGRAY3 }, 1, aCorners ) }

   //hb_cdpSelect( "RU866" )
   hb_cdpSelect( "RU1251" )

   INIT WINDOW oMain MAIN TITLE "My First HwGUI sample" AT 100, 100 SIZE 200, 200

   oPGT := HPseudoGT():New( , 0, 0, oMain:nWidth, oMain:nHeight, oFont,,, CLR_DGRAY1, { 90,10,10,10 } )

   @ 10, 10 DRAWN OF oPGT SIZE 70, 32 COLOR CLR_WHITE ;
      HSTYLES aStylesBtn TEXT 'Exit' FONT oFontBtn ON CLICK {||oMain:Close()}
   @ 10, 50 DRAWN OF oPGT SIZE 70, 32 COLOR CLR_WHITE ;
      HSTYLES aStylesBtn TEXT 'Test1' FONT oFontBtn ON CLICK {||Start()}
   @ 10, 90 DRAWN OF oPGT SIZE 70, 32 COLOR CLR_WHITE ;
      HSTYLES aStylesBtn TEXT 'Test2' FONT oFontBtn ON CLICK {||Test2()}
   @ 10, 130 DRAWN OF oPGT SIZE 70, 32 COLOR CLR_WHITE ;
      HSTYLES aStylesBtn TEXT 'QOut' FONT oFontBtn ON CLICK {||DoQout()}

   hb_cdpSelect( "RU866" )
   oPGT:Box( 0, 0, oPGT:MaxY(), oPGT:MaxX(), "ÚÄ¿³ÙÄÀ³ " ) //, "W+/B" )
   //hb_cdpSelect( "RU1251" )

   ACTIVATE WINDOW oMain

   RETURN Nil

STATIC FUNCTION Start()

   LOCAL y1 := 4, x1 := 5, y2 := 12, x2 := 75
   LOCAL cBuff := Savescreen( y1, x1, y2, x2 )
   LOCAL nKey, nKeyExt

   @ y1-1, x1-1 TO y2+1, x2+1
   @ y1, x1 TO y2, x2

   DevPos( 5, 8 )
   DevOut( "Test!" )
   oPGT:Pos( 1, 8 )
   oPGT:Out( "Press key: " )
   nKeyExt := oPGT:In( 0 )
   nKey := hb_keyStd( nKeyExt )
   IF nKey > 31 .AND. nKey < 250
      oPGT:Pos( 1, 19 )
      oPGT:Out( Chr( nKey ) )
   ENDIF

   Restscreen( y1, x1, y2, x2, cBuff )

   //hwg_writelog( str( hwpgt_test1( Asc('ï¿½') ) ) )
   //hwg_writelog( str( hwpgt_test1( Asc('ï¿½') ) ) )
   //hwg_writelog( str( hwpgt_test1( Asc('Q') ) ) )

   //hwg_writelog( str( hwpgt_test2( 53392 ) ) )
   //hwg_writelog( str( hwpgt_test2( 37072 ) ) )
   //hwg_writelog( str( hwpgt_test2( 53424 ) ) )
   //hwg_writelog( str( hwpgt_test2( Asc('Q') ) ) )

   RETURN Nil

STATIC FUNCTION Test2()

   LOCAL y1 := 4, x1 := 5, y2 := 12, x2 := 75
   LOCAL nKey, nKeyExt, my, mx

   DevPos( 5, 8 ); DevOut( "Test!" )
   DevPos( 6, 8 ); DevOut( "Press key: " )
   DO WHILE .T.
      IF ( nKeyExt := Inkey( 0, HB_INKEY_ALL + HB_INKEY_EXT ) ) == Nil
         RETURN Nil
      ENDIF
      nKey := hb_keyStd( nKeyExt )
      hwg_writelog( str( nKeyExt ) + " " + str(nKey) )
      //hwg_writelog( str( nKeyExt ) + " " + valtype( HPseudoGT():oCurrent ) )
      IF nKey == 1002 
         my := MRow()
         mx := MCol()
         IF my > 0 .AND. mx > 0
            DevPos( my, mx )
         ENDIF
         //hwg_writelog( str(my) + " " + str(mx) )
      ENDIF
      IF nKey == 27
         EXIT
      ENDIF
      IF nKey > 31 .AND. nKey < 250
         //oPGT:Pos( 6, 19 )
         oPGT:Out( Chr( nKey ) )
      ENDIF
   ENDDO
   
   RETURN Nil

STATIC FUNCTION DoQout()

   ? Date(), Time()
   ? "-------------"
   ?

   RETURN Nil
