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

   hb_cdpSelect( "UTF8" )

   INIT WINDOW oMain MAIN TITLE "HbEdit and PseudoGT" AT 100, 100 SIZE 200, 200 FONT oFont

   MENU OF oMain
      MENU TITLE "&File"
         MENUITEM "&Editor" ACTION RunEdi()
         SEPARATOR
         MENUITEM "&Exit" ACTION hwg_EndWindow()
      ENDMENU
   ENDMENU

   ACTIVATE WINDOW oMain

   RETURN Nil

STATIC FUNCTION RunEdi()

   LOCAL oDlg, oMain := HWindow():Getmain(), oTimer, lClose := .F.
   LOCAL oEdit
   LOCAL y1 := 0, x1 := 0, y2, x2, lTopPane := .T.
   LOCAL b1 := {||
      oPGT:SetFocus()
      y2 := oPGT:MaxY()
      x2 := oPGT:MaxX()
      oEdit := TEdit():New( "Start text editing", "$F", y1+1, x1+1, y2-1, x2-1, "N/W", .T. )

      oEdit:cColorSel := "N/BG"
      oEdit:lWrap := .T.
      oEdit:lIns  := .F.
      oEdit:nDefMode := -1

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
      lClose := .T.
      oDlg:Close()
      
      RETURN Nil
   }
   LOCAL bActivate := {||
      SET TIMER oTimer OF oDlg VALUE 100 ACTION b1 ONCE
      RETURN Nil
   }
   LOCAL bExit := {||
      oPGT:SendKey( Chr(27) )
      RETURN lClose
   }

   INIT DIALOG oDlg TITLE "Hbedit" AT 0, 0 SIZE 400, 300 ;
      FONT oMain:oFont ON EXIT bExit

   oPGT := HPseudoGT():New( , 0, 0, oDlg:nWidth, oDlg:nHeight, oDlg:oFont,,, CLR_DGRAY1, { 90,10,10,10 } )

   ACTIVATE DIALOG oDlg ON ACTIVATE bActivate

   RETURN Nil
