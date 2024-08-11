/*
 *
 */

#include "hwgui.ch"
#include "hbclass.ch"
#include "hbgtinfo.ch"
#include "inkey.ch"

#define MAXKEYS      128

CLASS HPseudoGT INHERIT HBoard

   CLASS VAR winclass  INIT "HBOARD"

   CLASS VAR aPGT   INIT {}
   CLASS VAR oCurrent

   DATA hPGT
   DATA nRows       INIT 25
   DATA nCols       INIT 80
   DATA aMargin
   DATA cColor      INIT "W+/N,N/W,N/N,N/N,N/W"
   DATA aKeys
   DATA nKeys       INIT 0

   DATA lCancel     INIT .F.
   DATA lIn         INIT .F.
   DATA lCursor     INIT .T.

   METHOD New( oWndParent, nLeft, nTop, nWidth, nHeight, oFont, nRows, nCols, bColor, aMargin )
   METHOD Activate()
   METHOD Init()
   METHOD onEvent( msg, wParam, lParam )
   METHOD SetFocus()          INLINE (::oCurrent := Self, hwg_SetFocus( ::handle ))
   METHOD SetKeyCP( cp )      INLINE hwpgt_SetKeyCP( ::hPGT, cp )

   METHOD Pos( nRow, nCol )   INLINE hwpgt_SetPos( ::hPGT, nRow, nCol )
   METHOD X()                 INLINE hwpgt_Col( ::hPGT )
   METHOD Y()                 INLINE hwpgt_Row( ::hPGT )
   METHOD MaxX()              INLINE ::nCols - 1
   METHOD MaxY()              INLINE ::nRows - 1
   METHOD MX()                INLINE hwpgt_MCol( ::hPGT )
   METHOD MY()                INLINE hwpgt_MRow( ::hPGT )
   METHOD Color( cColor )
   METHOD In( nMS, nFlags )
   METHOD Out( cLine, cColor )
   METHOD Box( nTop, nLeft, nBottom, nRight, xFill, cColor )
   METHOD Q_Out( l, ... )
   METHOD Scro( nTop, nLeft, nBottom, nRight, nVert, nHoriz )
   METHOD SendKey( x )
   METHOD Info( nType, xValue )
   METHOD Savescr( nTop, nLeft, nBottom, nRight )
   METHOD Restscr( nTop, nLeft, nBottom, nRight, cBuff )
   METHOD Getscr( nRow, nCol, cClr )
   METHOD GetLine()

   METHOD Paint( wParam, lParam )
   METHOD ErrMsg()
   METHOD End()

ENDCLASS

METHOD New( oWndParent, nLeft, nTop, nWidth, nHeight, oFont, nRows, nCols, bColor, aMargin ) CLASS HPseudoGT

   IF Empty( oFont )
      oFont := HFont():Add( "Courier New", 0, 16 )
   ENDIF
   IF !Empty( nRows )
      ::nRows := nRows
   ENDIF
   IF !Empty( nCols )
      ::nCols := nCols
   ENDIF
   IF Valtype( aMargin ) == "A" .AND. Len( aMargin ) == 4
      ::aMargin := aMargin
   ENDIF
   ::aKeys := Array( MAXKEYS )

   ::hPGT := hwpgt_Create( ::nRows, ::nCols )

   ::Super:New( oWndParent,, nLeft, nTop, nWidth, nHeight, oFont,,,,,, Iif( bColor==Nil,0,bColor ) )

   AAdd( ::aPGT, Self )

   RETURN Self

METHOD Activate() CLASS HPseudoGT

   IF !Empty( ::oParent:handle )
      ::handle := hwg_CreateBoard( ::oParent:handle, ::id, ::style, ;
         ::nLeft, ::nTop, ::nWidth, ::nHeight )

      ::Init()
   ENDIF

   RETURN Nil

METHOD Init() CLASS HPseudoGT

#ifndef __GTK__
   LOCAL arr
#endif

   IF ! ::lInit

      hwpgt_SetCoors( ::hPGT, ::nLeft, ::nTop, ::nWidth, ::nHeight, ::oParent:nWidth, ::oParent:nHeight )
      IF !Empty( ::aMargin )
         hwpgt_SetMargin( ::hPGT, ::aMargin[1], ::aMargin[2], ::aMargin[3], ::aMargin[4] )
      ENDIF
      hwpgt_Init( ::hPGT, ::oParent:handle, ::handle, ::oFont:handle )
#ifndef __GTK__
      arr := hwg_GetWindowRect( ::handle )
      ::nLeft := arr[1]
      ::nTop := arr[2]
      ::nWidth := arr[3] - arr[1] + 1
      ::nHeight := arr[4] - arr[2] + 1
#endif
      ::Super:Init()
   ENDIF

   RETURN Nil

METHOD onEvent( msg, wParam, lParam ) CLASS HPseudoGT

   LOCAL i, ym, xm, arr
   //hwg_writelog( "msg: " + ltrim(str(msg)) )

   SWITCH msg
   CASE WM_PAINT
      ::Paint( wParam, lParam )
      RETURN -1

   CASE WM_SIZE
      arr := hwg_GetWindowRect( ::handle )
      ::nLeft := arr[1]
      ::nTop := arr[2]
      ::nWidth := arr[3] - arr[1] + 1
      ::nHeight := arr[4] - arr[2] + 1
      //hwg_writelog( "onsize " + str( ::nWidth ) + " " + str( ::nHeight ) )
      EXIT

   CASE WM_SETFOCUS
      ::oCurrent := Self
      EXIT

   CASE WM_KILLFOCUS
      IF ::oCurrent == Self
         //::oCurrent := Nil
      ENDIF
      EXIT

   CASE WM_KEYDOWN
   CASE WM_SYSKEYDOWN
   CASE WM_CHAR

         IF ::nKeys < MAXKEYS .AND. ( i := hwpgt_KeyEvent( ::hPGT, msg, ;
            hwg_PtrToUlong(wParam), hwg_PtrToUlong(lParam) ) ) != 0
            ::aKeys[++ ::nKeys] := i
         ENDIF
         //hwg_writelog( "Key " + str(i) + " / " + str(hwg_PtrToUlong(wParam)) + " " + str(hwg_PtrToUlong(lParam)) )
         RETURN -1

   CASE WM_RBUTTONDOWN
   CASE WM_LBUTTONDOWN
   CASE WM_RBUTTONUP
   CASE WM_LBUTTONUP
   CASE WM_LBUTTONDBLCLK
   CASE WM_MBUTTONUP
   CASE WM_MOUSEWHEEL

         ym := hwg_Hiword( lParam )
         xm := hwg_Loword( lParam )
         //hwg_writelog( "msg1: " + ltrim(str(msg)) + str(xm) + " " + str(ym) )
         //hwg_writelog( "msg1a " + str(::nLeft)+" "+str(::nHeight) )
         //hwg_writelog( "msg2: " + str(::aMargin[1]) + str(::nLeft + ::nWidth - ::aMargin[3]) + " / " + ;
         //   str(::aMargin[2]) + str(::nTop + ::nHeight - ::aMargin[4]) )
         IF xm < ::aMargin[1] .OR. xm > ( ::nLeft + ::nWidth - ::aMargin[3] ) ;
            .OR. ym < ::aMargin[2] .OR. ym > ( ::nTop + ::nHeight - ::aMargin[4] )
            EXIT
         ENDIF
         IF ::nKeys < MAXKEYS .AND. ( i := hwpgt_MouseEvent( ::hPGT, msg, ;
            hwg_PtrToUlong(wParam), hwg_PtrToUlong(lParam) ) ) != 0
            ::aKeys[++ ::nKeys] := i
         ENDIF

         hwg_SetFocus( ::handle )
         EXIT
   END

   RETURN ::Super:onEvent( msg, wParam, lParam )

METHOD Color( cColor ) CLASS HPseudoGT

   LOCAL arrc, arr, lPlus := .F., iClr, iBack := 0, oldColor := ::cColor, arrcOld, i
   STATIC aClr := { "N", "B", "G", "BG", "R", "RB", "GR", "W" }

   IF !Empty( cColor )
      arrc := hb_aTokens( cColor, ',' )
      arr := hb_aTokens( arrc[1], '/' )
      IF Left( arr[1],1 ) == "+"
         arr[1] := Substr( arr[1], 2 )
         lPlus := .T.
      ELSEIF Right( arr[1],1 ) == "+"
         arr[1] := hb_strShrink( arr[1], 1 )
         lPlus := .T.
      ENDIF

      IF ( iClr := hb_Ascan( aClr, arr[1],,, .T. ) ) > 0
         iClr --
      ENDIF
      IF lPlus
         iClr += 8
      ENDIF

      IF Len( arr ) > 1
         IF "+" $ arr[2]
            arr[2] := StrTran( arr[2], "+", "" )
         ENDIF
         IF ( iBack := hb_Ascan( aClr, arr[2],,, .T. ) ) > 0
            iBack --
         ENDIF
      ENDIF

      iClr += iBack * 16
      hwpgt_SetColor( ::hPGT, iClr )

      //hwg_writelog( oldColor + " == " + cColor )
      arrcOld := hb_aTokens( oldColor, ',' )
      FOR i := 1 TO Len( arrc )
         arrcOld[i] := arrc[i]
      NEXT
      cColor := ""
      FOR i := 1 TO Len( arrcOld )
         cColor := cColor + Iif( i>1,',','' ) + arrcOld[i]
      NEXT
      ::cColor := cColor
   ENDIF

   RETURN oldColor

METHOD In( nMS, nFlags ) CLASS HPseudoGT

   LOCAL nKeyExt := 0, nKey := 0, nStart := Seconds(), nSecCurs, nSec, lCursorOn := .F.

   IF ::lIn
      hwg_MsgStop( "Inkey error" )
      RETURN Nil
   ENDIF

   ::lIn := .T.

   IF Empty( nFlags ); nFlags := 0; ENDIF
   hwg_SetFocus( ::handle )
   IF ::lCursor
      nSecCurs := nStart
      hwpgt_Cursor( ::hPGT, lCursorOn := .T. )
   ENDIF
   DO WHILE !::lCancel
      IF ::nKeys > 0
         nKeyExt := ::aKeys[1]
         nKey := hb_keyStd( nKeyExt )
         ADel( ::aKeys, 1 )
         ::nKeys --
         ::lIn := .F.
         IF nKey > K_MOUSEMOVE .AND. nKey < K_NCMOUSEMOVE .AND. ;
            hb_bitand( nFlags, INKEY_MOVE + INKEY_LDOWN + INKEY_LUP + ;
            INKEY_RDOWN + INKEY_RUP + INKEY_MMIDDLE + INKEY_MWHEEL ) == 0
            LOOP
         ENDIF
         EXIT
      ENDIF
      IF nMS == Nil
         nKeyExt := 0
         EXIT
      ELSEIF nMS > 0 .AND. (Seconds() - nStart) * 1000 >= nMS
         nKeyExt := 0
         EXIT
      ENDIF
      hwg_ProcessMessage()
      hb_gcStep()
      hwg_Sleep( 1 )
      IF ::lCursor .AND. ( ( nSec := Seconds() ) - nSecCurs ) > 0.6 .AND. !Empty( ::hPGT )
         nSecCurs := nSec
         hwpgt_Cursor( ::hPGT, lCursorOn := !lCursorOn )
      ENDIF
   ENDDO
   ::lIn := .F.

   IF ::lCursor .AND. lCursorOn .AND. !Empty( ::hPGT )
      hwpgt_Cursor( ::hPGT, .F. )
   ENDIF

   nKey := hb_keyStd( nKeyExt )

   RETURN Iif( hb_bitand( nFlags, HB_INKEY_EXT ) > 0, nKeyExt, nKey )

METHOD Paint( wParam, lParam) CLASS HPseudoGT

   LOCAL pps, hDC, i

   pps := hwg_Definepaintstru()
   hDC := hwg_Beginpaint( ::handle, pps )

   IF !Empty( ::aMargin )
      //hwg_writelog( "paint1 " + str( ::nWidth ) + " " + str( ::nHeight ) )
      //hwg_writelog( str( ::oParent:nWidth ) + "/" + str( ::oParent:nHeight ) )
      hwg_Fillrect( hDC, 0, 0, ::aMargin[1], ::nHeight, ::brush:handle )
      hwg_Fillrect( hDC, ::nWidth-::aMargin[3]-1, 0, ::nWidth, ::nHeight, ::brush:handle )
      hwg_Fillrect( hDC, ::aMargin[1], 0, ::nWidth-::aMargin[3], ::aMargin[2], ::brush:handle )
      hwg_Fillrect( hDC, ::aMargin[1], ::nHeight-::aMargin[4]-1, ::nWidth-::aMargin[3], ::nHeight, ::brush:handle )
   ENDIF

   hwpgt_Paint( ::hPGT, pps, hDC, wParam, lParam )

   FOR i := 1 TO Len( ::aDrawn )
      ::aDrawn[i]:Paint( hDC )
   NEXT

   hwg_Endpaint( ::handle, pps )

   hwg_SetFocus( ::handle )

   RETURN Nil

METHOD Out( cLine, cColor ) CLASS HPseudoGT

   LOCAL oldColor

   IF !Empty( cColor )
      oldColor := ::Color( cColor )
   ENDIF

   hwpgt_Out( ::hPGT, cLine )

   IF !Empty( oldColor )
      ::Color( oldColor )
   ENDIF

   RETURN Nil

METHOD Box( nTop, nLeft, nBottom, nRight, xFill, cColor ) CLASS HPseudoGT

   LOCAL oldColor

   IF !Empty( cColor )
      oldColor := ::Color( cColor )
   ENDIF

   IF Valtype( xFill ) == "N"
      xFill := Iif( nTop==nBottom .OR. nLeft == nRight, ;
         Iif( xFill == 1, Iif( nTop==nBottom, "Ä", "³" ), Iif( nTop==nBottom, "Í", "º" ) ), ;
         Iif( xFill == 1, "ÚÄ¿³ÙÄÀ³ ", "ÉÍ»º¼ÍÈº " ) )
   ELSEIF Len( xFill ) < 9
      xFill := PAdr( xFill, 9 )
   ENDIF

   hwpgt_Box( ::hPGT, nTop, nLeft, nBottom, nRight, xFill )

   IF !Empty( oldColor )
      ::Color( oldColor )
   ENDIF

   RETURN Nil

METHOD Q_Out( l, ... ) CLASS HPseudoGT

   LOCAL aParams := hb_aParams(), i

   IF l
      IF hwpgt_MaxRow( ::hPGT ) == hwpgt_Row( ::hPGT )
         hwpgt_Scroll( ::hPGT, 0, 0, hwpgt_Maxrow(::hPGT), hwpgt_Maxcol(::hPGT), 1, 0 )
         hwpgt_SetPos( ::hPGT, hwpgt_MaxRow( ::hPGT ), 0 )
      ELSE
         hwpgt_SetPos( ::hPGT, hwpgt_Row( ::hPGT ) + 1, 0 )
      ENDIF
   ENDIF

   FOR i := 2 TO Len( aParams )
      hwpgt_Out( ::hPGT, Iif( i == 2, hb_ValToStr(aParams[i]), " " + hb_ValToStr(aParams[i]) ) )
   NEXT
   RETURN Nil

METHOD Scro( nTop, nLeft, nBottom, nRight, nVert, nHoriz ) CLASS HPseudoGT

   IF nTop == Nil; nTop := 0; ENDIF
   IF nLeft == Nil; nLeft := 0; ENDIF
   IF nBottom == Nil; nBottom := hwpgt_MaxRow(::hPGT); ENDIF
   IF nRight == Nil; nRight := hwpgt_MaxCol(::hPGT); ENDIF
   IF nVert == Nil; nVert := 0; ENDIF
   IF nHoriz == Nil; nHoriz := 0; ENDIF

   hwpgt_Scroll( ::hPGT, nTop, nLeft, nBottom, nRight, nVert, nHoriz )
   RETURN Nil

METHOD SendKey( x ) CLASS HPseudoGT

   LOCAL i

   //hwg_writelog( "sendk-0 " + valtype(x) )
   IF x == Nil
      ::nKeys := 0
   ELSEIF Valtype( x ) == "C"
      FOR i := 1 TO Len( x )
         IF ::nKeys < MAXKEYS
            ::aKeys[++ ::nKeys] := Asc( Substr( x, i, 1 ) )
         ENDIF
      NEXT
   ELSE
      IF ::nKeys < MAXKEYS
         ::aKeys[++ ::nKeys] := x
      ENDIF
   ENDIF

   RETURN Nil

METHOD Info( nType, xValue ) CLASS HPseudoGT

   LOCAL xOldVal

   IF nType == HB_GTI_FONTNAME
      IF !Empty( xValue )
      ENDIF
      RETURN ::oFont:name

   ELSEIF nType == HB_GTI_DESKTOPWIDTH
      RETURN hwg_GetDesktopWidth()

   ELSEIF nType == HB_GTI_DESKTOPHEIGHT
      RETURN hwg_GetDesktopHeight()

   ELSEIF nType == HB_GTI_DESKTOPCOLS
      RETURN hwpgt_Info( ::hPGT, HB_GTI_DESKTOPCOLS )

   ELSEIF nType == HB_GTI_DESKTOPROWS
      RETURN hwpgt_Info( ::hPGT, HB_GTI_DESKTOPROWS )

   ELSEIF nType == HB_GTI_CLIPBOARDDATA
      xOldVal := hwg_GetClipboardText()
      IF !Empty( xValue )
         hwg_CopyStringToClipboard( xValue )
      ENDIF

   ELSEIF nType == HB_GTI_PALETTE
      RETURN hwpgt_Info( ::hPGT, HB_GTI_PALETTE, xValue )

   ELSEIF nType == HB_GTI_WINTITLE
      IF __ObjHasMsg( ::oParent, "SETTITLE" )
         xOldVal := hwg_GetWindowText( ::oParent:handle )
         IF Valtype( xValue ) == "C"
            ::oParent:SetTitle( xValue )
         ENDIF
      ELSE
         xOldVal := ""
      ENDIF
      RETURN xOldVal
   ENDIF

   RETURN Nil

METHOD Savescr( nTop, nLeft, nBottom, nRight ) CLASS HPseudoGT

   IF nRight < nLeft .OR. nBottom < nTop
      RETURN Nil
   ENDIF
   IF nRight >= ::nCols
      nRight := ::nCols - 1
   ENDIF
   IF nBottom >= ::nRows
      nBottom := ::nRows - 1
   ENDIF
   RETURN hwpgt_Savescr( ::hPGT, nTop, nLeft, nBottom, nRight )

METHOD Restscr( nTop, nLeft, nBottom, nRight, cBuff ) CLASS HPseudoGT

   IF nRight < nLeft .OR. nBottom < nTop .OR. Empty( cBuff )
      RETURN Nil
   ENDIF
   IF nRight >= ::nCols
      nRight := ::nCols - 1
   ENDIF
   IF nBottom >= ::nRows
      nBottom := ::nRows - 1
   ENDIF
   hwpgt_Restscr( ::hPGT, nTop, nLeft, nBottom, nRight, cBuff )

   RETURN Nil

METHOD Getscr( nRow, nCol, cClr ) CLASS HPseudoGT

   LOCAL c

   RETURN c

METHOD GetLine() CLASS HPseudoGT

   LOCAL nKey, s := ""

   DO WHILE ( nKey := hb_keyStd(::In( 0 )) ) != 0 .AND. nKey != K_ENTER .AND. nKey != K_ESC

      IF nKey == K_BS
         IF Len( s ) > 0
            s := hb_strShrink( s, 1 )
            ::Pos( ::Row(), ::Col() -1 )
            ::Out( " " )
            ::Pos( ::Row(), ::Col() -1 )
         ENDIF
      ELSE
         ::Out( Chr(nKey) )
         s += Chr(nKey)
      ENDIF
   ENDDO

   IF nKey == K_ENTER
      RETURN s
   ENDIF

   RETURN ""

METHOD ErrMsg() CLASS HPseudoGT

   IF ::hPGT == Nil
      hwg_writelog( ":ErrMsg()" )
      RETURN Nil
   ENDIF

   hwg_MsgStop( "PseudoGT isn't in focus" )
   RETURN 0

METHOD End() CLASS HPseudoGT

   LOCAL i

   hwpgt_Exit( ::hPGT )
   ::hPGT := Nil
   ::lCancel := .T.

   IF ::oCurrent == Self
      ::oCurrent := Nil
   ENDIF

   FOR i := 1 TO Len( ::aPGT )
      IF ::aPGT[i] == Self
         hb_ADel( ::aPGT, i, .T. )
         EXIT
      ENDIF
   NEXT

   RETURN Nil
