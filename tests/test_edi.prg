#include "hwgui.ch"
#include "hwpgt.ch"

#define CLR_WHITE    0xffffff
#define CLR_BLACK    0x000000
#define CLR_DGRAY1   0x333333
#define CLR_DGRAY2   0x606060
#define CLR_DGRAY3   0x888888
#define CLR_DGRAY4   0xdddddd

REQUEST HB_CODEPAGE_RU866

STATIC oPGT

FUNCTION Main()

   LOCAL oMain, oFont := HFont():Add( "Liberation Mono", 0, 18,, 204 )
   LOCAL i, oFontBtn := HFont():Add( "Georgia", 0, 18,, 204 )
   LOCAL aCorners := { 4,4,4,4 }
   LOCAL aStylesBtn := { HStyle():New( { CLR_DGRAY2 }, 1, aCorners ), ;
      HStyle():New( { CLR_WHITE }, 2, aCorners ), ;
      HStyle():New( { CLR_DGRAY3 }, 1, aCorners ) }

   hb_cdpSelect( "RU866" )

   INIT WINDOW oMain MAIN TITLE "HbEdit and PseudoGT" AT 100, 100 SIZE 200, 200
   //INIT DIALOG oMain TITLE "HbEdit and PseudoGT" AT 100, 100 SIZE 200, 200

   oPGT := HPseudoGT():New( , 0, 0, oMain:nWidth, oMain:nHeight, oFont,,, CLR_DGRAY1, { 90,10,10,10 } )

   @ 10, 10 DRAWN OF oPGT SIZE 70, 32 COLOR CLR_WHITE ;
      HSTYLES aStylesBtn TEXT 'Exit' FONT oFontBtn ON CLICK {||oMain:Close()}
   @ 10, 50 DRAWN OF oPGT SIZE 70, 32 COLOR CLR_WHITE ;
      HSTYLES aStylesBtn TEXT 'Start' FONT oFontBtn ON CLICK {||Start()}

   oPGT:Box( 0, 0, oPGT:MaxY(), oPGT:MaxX(), "ÚÄ¿³ÙÄÀ³ ", "W+/B" )

   ACTIVATE WINDOW oMain
   //ACTIVATE DIALOG oMain

   RETURN Nil

STATIC FUNCTION Start()

   LOCAL oEdit, cBuff
#ifdef __BUILT_IN
   LOCAL y1 := 6, x1 := 10, y2 := 15, x2 := 70, lTopPane := .F.
#else
   LOCAL y1 := 0, x1 := 0, y2 := oPGT:MaxY(), x2 := oPGT:MaxX(), lTopPane := .T.
#endif

   oPGT:SetFocus()
   cBuff := Savescreen( y1, x1, y2, x2 )
#ifdef __BUILT_IN
   oPGT:Box( y1, x1, y2, x2, "ÚÄ¿³ÙÄÀ³ ", "W+/B" )
#endif
   oEdit := TEdit():New( "Start text editing", "$F", y1+1, x1+1, y2-1, x2-1, "N/W", lTopPane )

   oEdit:cColorSel := "N/BG"
   oEdit:lWrap := .T.
   oEdit:lIns  := .F.
   oEdit:nDefMode := -1

#ifdef __BUILT_IN
   oEdit:Edit()
#else
   TEdit():nCurr := 1
   DO WHILE !Empty( TEdit():aWindows )
      IF TEdit():nCurr > Len(TEdit():aWindows)
         TEdit():nCurr := 1
      ELSEIF TEdit():nCurr <= 0
         TEdit():nCurr := Len(TEdit():aWindows)
      ENDIF
      TEdit():aWindows[TEdit():nCurr]:Edit()
   ENDDO
   TEdit():onExit()
   FilePane():onExit()
#endif
   Restscreen( y1, x1, y2, x2, cBuff )

   RETURN Nil
