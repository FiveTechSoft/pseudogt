/*
 *
 */

#include "hbsetup.h"
#include "windows.h"

#include "hbgtcore.h"
#include "hbinit.h"
#include "hbapiitm.h"
#include "hbvm.h"
#include "hbwinuni.h"
#include "hwingui.h"
#include "inkey.ch"

//#define WM_MY_UPDATE_CARET  ( WM_USER + 0x0101 )
#define MSG_USER_SIZE  0x502

#define HWG_EXTKEY_FLAG              ( 1 << 24 )

#define BLACK            RGB( 0x00, 0x00, 0x00 )
#define BLUE             RGB( 0x00, 0x00, 0xAA )
#define GREEN            RGB( 0x00, 0xAA, 0x00 )
#define CYAN             RGB( 0x00, 0xAA, 0xAA )
#define RED              RGB( 0xAA, 0x00, 0x00 )
#define MAGENTA          RGB( 0xAA, 0x00, 0xAA )
#define BROWN            RGB( 0xAA, 0x55, 0x00 )
#define LIGHT_GRAY       RGB( 0xAA, 0xAA, 0xAA )
#define GRAY             RGB( 0x55, 0x55, 0x55 )
#define BRIGHT_BLUE      RGB( 0x55, 0x55, 0xFF )
#define BRIGHT_GREEN     RGB( 0x55, 0xFF, 0x55 )
#define BRIGHT_CYAN      RGB( 0x55, 0xFF, 0xFF )
#define BRIGHT_RED       RGB( 0xFF, 0x55, 0x55 )
#define BRIGHT_MAGENTA   RGB( 0xFF, 0x55, 0xFF )
#define YELLOW           RGB( 0xFF, 0xFF, 0x55 )
#define WHITE            RGB( 0xFF, 0xFF, 0xFF )

#define INIT_COLOR       15
#define HB_KF_ALTGR    0x10

#define VK__BACKQUOTE   430
#define VK__CTRL_1      431
#define VK__CTRL_2      432
#define VK__CTRL_3      433
#define VK__CTRL_4      434
#define VK__CTRL_5      435
#define VK__CTRL_6      436
#define VK__CTRL_7      437
#define VK__CTRL_8      438
#define VK__CTRL_9      439
#define VK__CTRL_0      440
#define VK__TILDA       192

extern void hwg_writelog( const char *, const char *, ... );

typedef struct
{
   unsigned int iColor;
   unsigned int iChar;
} HWCELL, * PHWCELL;

typedef struct
{
   HWND     hParent;
   HWND     handle;

   HFONT    hFont;

   int      iBtop, iBleft, iBwidth, iBheight, iWwidth, iWheight;
   int      iMarginL, iMarginT, iMarginR, iMarginB;
   POINT    PTEXTSIZE;      /* size of the fixed width font */

   int      keyFlags;       /* keyboard modifiers */

   int      iHeight;        /* window height */
   int      iWidth;         /* window width */
   int      iRow;           /* cursor row position */
   int      iCol;           /* cursor column position */
   int      ROWS;           /* number of displayable rows in window */
   int      COLS;           /* number of displayable columns in window */

   wchar_t *  TextLine;
   COLORREF COLORS[ 16 ];   /* colors */

   int      iColor;

   PHWCELL  screenBuf;

   HB_BOOL  bCursorOn;
   HPEN     hPen;

   POINT    MousePos;       /* the last mouse position */
   POINT    ExactMousePos;  /* the last mouse position in pixels*/

   PHB_CODEPAGE   cdpIn;
   int      CodePage;       /* Code page to use for display characters */
   HB_BOOL  bFontChanged;

   HB_BOOL  IgnoreWM_SYSCHAR;

} HWPGT, * PHWPGT;

#if ! defined( UNICODE )
static int hwpgt_key_ansi_to_oem( int c )
{
   BYTE pszSrc[ 2 ];
   wchar_t pszWide[ 1 ];
   BYTE pszDst[ 2 ];

   pszSrc[ 0 ] = ( CHAR ) c;
   pszSrc[ 1 ] =
   pszDst[ 0 ] =
   pszDst[ 1 ] = 0;

   if( MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, ( LPCSTR ) pszSrc, 1, ( LPWSTR ) pszWide, 1 ) &&
       WideCharToMultiByte( CP_OEMCP, 0, ( LPCWSTR ) pszWide, 1, ( LPSTR ) pszDst, 1, NULL, NULL ) )
      return pszDst[ 0 ];
   else
      return c;
}
#endif

wchar_t utf8str_to_widechar( const char *utf8_char, unsigned int *piPos )
{
    wchar_t wide_char = 0;
    char * ptr = (char*) utf8_char + *piPos;

    if ((*ptr & 0x80) == 0) {
        // Single-byte character
        wide_char = (wchar_t)(*ptr);
        (*piPos) ++;
    } else if ((*ptr & 0xE0) == 0xC0) {
        // Two-byte character
        wide_char = (wchar_t)(((ptr[0] & 0x1F) << 6) | (ptr[1] & 0x3F));
        (*piPos) += 2;
    } else if ((*ptr & 0xF0) == 0xE0) {
        // Three-byte character
        wide_char = (wchar_t)(((ptr[0] & 0x0F) << 12) | ((ptr[1] & 0x3F) << 6) | (ptr[2] & 0x3F));
        (*piPos) += 3;
    } else if ((*ptr & 0xF8) == 0xF0) {
        // Four-byte character
        wide_char = (wchar_t)(((ptr[0] & 0x07) << 18) | ((ptr[1] & 0x3F) << 12) |
           ((ptr[2] & 0x3F) << 6) | (ptr[3] & 0x3F));
        (*piPos) += 4;
    }

    return wide_char;
}

static int hwpgt_GetKeyFlags( void )
{
   int iFlags = 0;
   if( GetKeyState( VK_SHIFT ) & 0x8000 )
      iFlags |= HB_KF_SHIFT;
   if( GetKeyState( VK_CONTROL ) & 0x8000 )
      iFlags |= HB_KF_CTRL;
   if( GetKeyState( VK_LMENU ) & 0x8000 )
      iFlags |= HB_KF_ALT;
   if( GetKeyState( VK_RMENU ) & 0x8000 )
      iFlags |= HB_KF_ALTGR;

   return iFlags;
}

static int hwpgt_UpdateKeyFlags( int iFlags )
{
   if( iFlags & HB_KF_ALTGR )
   {
      iFlags |= HB_KF_ALT;
      iFlags &= ~HB_KF_ALTGR;
   }

   return iFlags;
}

static void hwpgt_ResetWindowSize( PHWPGT pHWG )
{
   HDC        hdc;
   TEXTMETRIC tm;
   RECT       wi, pi, ci;
   int        height, width, iWinHeight, iWinWidth;

   //hwg_writelog( NULL, "ResetSize-1\r\n" );
   hdc = GetDC( pHWG->handle );
   SelectObject( hdc, pHWG->hFont );
   GetTextMetrics( hdc, &tm );
   SetTextCharacterExtra( hdc, 0 ); /* do not add extra char spacing even if bold */
   ReleaseDC( pHWG->handle, hdc );

   pHWG->PTEXTSIZE.x = tm.tmAveCharWidth;
   pHWG->PTEXTSIZE.y = tm.tmHeight;

   GetWindowRect( pHWG->handle, &wi );
   GetWindowRect( pHWG->hParent, &pi );
   GetClientRect( pHWG->hParent, &ci );

   height = ( int ) ( pHWG->PTEXTSIZE.y * pHWG->ROWS + pHWG->iMarginT + pHWG->iMarginB );
   width  = ( int ) ( pHWG->PTEXTSIZE.x * pHWG->COLS + pHWG->iMarginL + pHWG->iMarginR );

   //hwg_writelog( NULL, " size1 %d %d %d %d\r\n", pHWG->iMarginL, pHWG->iMarginT, pHWG->iMarginR, pHWG->iMarginB );
   //hwg_writelog( NULL, " size1 %d %d %d %d\r\n", wi.left, wi.top, wi.right, wi.bottom );
   //hwg_writelog( NULL, " size2 %d %d %d %d\r\n", pi.left, pi.top, pi.right, pi.bottom );
   //hwg_writelog( NULL, " size3 %d %d %d %d\r\n", ci.left, ci.top, ci.right, ci.bottom );

   iWinHeight = height + (pHWG->iWheight-pHWG->iBheight) + (pi.bottom-pi.top-ci.bottom);
   iWinWidth = width + (pHWG->iWwidth-pHWG->iBwidth) + (pi.right-pi.left-ci.right);

   //hwg_writelog( NULL, "iWinHeight %d %d %d %d\r\n", iWinHeight, height, pHWG->iWheight-pHWG->iBheight, pi.bottom-pi.top-ci.bottom );

   SetWindowPos( pHWG->hParent, NULL, pi.left, pi.top, iWinWidth, iWinHeight, SWP_NOZORDER );
   MoveWindow( pHWG->handle, pHWG->iBleft, pHWG->iBtop, width, height, TRUE );

}

static POINT hwpgt_GetXYFromColRow( PHWPGT pHWG, int col, int row )
{
   POINT xy;

   xy.x = col * pHWG->PTEXTSIZE.x + pHWG->iMarginL;
   xy.y = row * pHWG->PTEXTSIZE.y + pHWG->iMarginT;

   return xy;
}

static RECT hwpgt_GetXYFromColRowRect( PHWPGT pHWG, RECT colrow )
{
   RECT xy;

   xy.left   = colrow.left * pHWG->PTEXTSIZE.x + pHWG->iMarginL;
   xy.top    = colrow.top  * pHWG->PTEXTSIZE.y + pHWG->iMarginT;
   xy.right  = ( colrow.right  + 1 ) * pHWG->PTEXTSIZE.x + pHWG->iMarginL;
   xy.bottom = ( colrow.bottom + 1 ) * pHWG->PTEXTSIZE.y + pHWG->iMarginT;

   return xy;
}

static POINT hwpgt_GetColRowFromXY( PHWPGT pHWG, LONG x, LONG y )
{
   POINT colrow;

   colrow.x = ( x - pHWG->iMarginL ) / pHWG->PTEXTSIZE.x;
   colrow.y = ( y - pHWG->iMarginT ) / pHWG->PTEXTSIZE.y;

   return colrow;
}

static RECT hwpgt_GetColRowFromXYRect( PHWPGT pHWG, RECT xy )
{
   RECT colrow;

   colrow.left   = (xy.left - pHWG->iMarginL)  / pHWG->PTEXTSIZE.x;
   colrow.top    = (xy.top - pHWG->iMarginT)   / pHWG->PTEXTSIZE.y;
   colrow.right  = (xy.right - pHWG->iMarginL) / pHWG->PTEXTSIZE.x -
      ( ( (xy.right - pHWG->iMarginL)  % pHWG->PTEXTSIZE.x ) ? 0 : 1 ); /* Adjust for when rectangle */
   colrow.bottom = (xy.bottom - pHWG->iMarginT) / pHWG->PTEXTSIZE.y -
      ( ( (xy.bottom - pHWG->iMarginT) % pHWG->PTEXTSIZE.y ) ? 0 : 1 ); /* EXACTLY overlaps characters */

   return colrow;
}

static HB_BOOL hwpgt_SetMousePos( PHWPGT pHWG, int iRow, int iCol )
{
   if( pHWG->MousePos.y != iRow || pHWG->MousePos.x != iCol )
   {
      pHWG->MousePos.y = iRow;
      pHWG->MousePos.x = iCol;
      return HB_TRUE;
   }
   else
      return HB_FALSE;
}

HB_FUNC( HWPGT_CREATE )
{
   PHWPGT pHWG = (PHWPGT) hb_xgrab( sizeof( HWPGT ) );
   int i, iSize;

   memset( pHWG, 0, sizeof(HWPGT) );

   pHWG->ROWS = hb_parni( 1 );
   pHWG->COLS = hb_parni( 2 );

   pHWG->TextLine = ( wchar_t * ) hb_xgrab( pHWG->COLS * sizeof( wchar_t ) );

   pHWG->COLORS[ 0 ]       = BLACK;
   pHWG->COLORS[ 1 ]       = BLUE;
   pHWG->COLORS[ 2 ]       = GREEN;
   pHWG->COLORS[ 3 ]       = CYAN;
   pHWG->COLORS[ 4 ]       = RED;
   pHWG->COLORS[ 5 ]       = MAGENTA;
   pHWG->COLORS[ 6 ]       = BROWN;
   pHWG->COLORS[ 7 ]       = LIGHT_GRAY;
   pHWG->COLORS[ 8 ]       = GRAY;
   pHWG->COLORS[ 9 ]       = BRIGHT_BLUE;
   pHWG->COLORS[ 10 ]      = BRIGHT_GREEN;
   pHWG->COLORS[ 11 ]      = BRIGHT_CYAN;
   pHWG->COLORS[ 12 ]      = BRIGHT_RED;
   pHWG->COLORS[ 13 ]      = BRIGHT_MAGENTA;
   pHWG->COLORS[ 14 ]      = YELLOW;
   pHWG->COLORS[ 15 ]      = WHITE;

   pHWG->iColor    = INIT_COLOR;
   pHWG->iHeight   = pHWG->ROWS;
   pHWG->iWidth    = pHWG->COLS;
   iSize = pHWG->iHeight * pHWG->iWidth;
   pHWG->screenBuf = ( PHWCELL ) hb_xgrab( sizeof( HWCELL ) * iSize );
   for( i = 0; i < iSize; i++ )
   {
      pHWG->screenBuf[i].iColor = INIT_COLOR;
      pHWG->screenBuf[i].iChar = 32;
   }

   pHWG->cdpIn         = hb_vmCDP();
   pHWG->CodePage      = OEM_CHARSET;     /* GetACP(); - set code page to default system */

   HB_RETHANDLE( pHWG );
}

HB_FUNC( HWPGT_SETCOORS )
{

   PHWPGT pHWG = (PHWPGT) HB_PARHANDLE( 1 );

   pHWG->iBtop     = hb_parni( 2 );
   pHWG->iBleft    = hb_parni( 3 );
   pHWG->iBwidth   = hb_parni( 4 );
   pHWG->iBheight  = hb_parni( 5 );
   pHWG->iWwidth   = hb_parni( 6 );
   pHWG->iWheight  = hb_parni( 7 );
}

HB_FUNC( HWPGT_SETMARGIN )
{

   PHWPGT pHWG = (PHWPGT) HB_PARHANDLE( 1 );

   pHWG->iMarginL  = hb_parni( 2 );
   pHWG->iMarginT  = hb_parni( 3 );
   pHWG->iMarginR  = hb_parni( 4 );
   pHWG->iMarginB  = hb_parni( 5 );
}

HB_FUNC( HWPGT_INIT )
{

   PHWPGT pHWG = (PHWPGT) HB_PARHANDLE( 1 );

   pHWG->hParent = (HWND) HB_PARHANDLE( 2 );
   pHWG->handle = (HWND) HB_PARHANDLE( 3 );
   pHWG->hFont = (HFONT) HB_PARHANDLE( 4 );

   pHWG->hPen = CreatePen( PS_SOLID, 3, 0xffffff );
   hwpgt_ResetWindowSize( pHWG );

}

HB_FUNC( HWPGT_EXIT )
{
   PHWPGT pHWG = (PHWPGT) HB_PARHANDLE( 1 );

   if( pHWG )
   {
      if( pHWG->TextLine )
         hb_xfree( pHWG->TextLine );

      if( pHWG->screenBuf )
         hb_xfree( pHWG->screenBuf );

      hb_xfree( pHWG );
   }

}

//static void hwpgt_TextOut( PHWPGT pHWG, HDC hdc, int col, int row, int iColor,
//   LPCTSTR lpString, UINT uiLen )
static void hwpgt_TextOut( PHWPGT pHWG, HDC hdc, int col, int row, int iColor,
   LPCWSTR lpString, UINT uiLen )
{
   POINT xy;
   RECT  rClip;

   //hwg_writelog( NULL, "_TextOut-1\r\n" );
   xy = hwpgt_GetXYFromColRow( pHWG, col, row );
   SetRect( &rClip, xy.x, xy.y, xy.x + uiLen * pHWG->PTEXTSIZE.x, xy.y + pHWG->PTEXTSIZE.y );

   SetBkColor( hdc, pHWG->COLORS[ ( iColor >> 4 ) & 0x0F ] );
   SetTextColor( hdc, pHWG->COLORS[ iColor & 0x0F ] );

   SetTextAlign( hdc, TA_LEFT );

   //ExtTextOut( hdc, xy.x, xy.y, ETO_CLIPPED | ETO_OPAQUE, &rClip,
   //            lpString, uiLen, NULL );
   ExtTextOutW( hdc, xy.x, xy.y, ETO_CLIPPED | ETO_OPAQUE, &rClip,
               (LPCWSTR)lpString, uiLen, NULL );

   //hwg_writelog( NULL, "tout %d %d %d %d : %u %u %u %u\r\n", xy.x, xy.y, iColor, uiLen,
   //    ((unsigned char*)lpString)[0], ((unsigned char*)lpString)[1], ((unsigned char*)lpString)[2], ((unsigned char*)lpString)[3] );

}

HB_FUNC( HWPGT_PAINT )
{
   PHWPGT pHWG = (PHWPGT) HB_PARHANDLE( 1 );
   PAINTSTRUCT* pps = (PAINTSTRUCT*) HB_PARHANDLE( 2 );
   HDC hdc = (HDC) HB_PARHANDLE( 3 );
   RECT rcRect;
   int  iRow;
   int  iColor, iOldColor = 0;

   if( !(pHWG->PTEXTSIZE.x) )
      return;
   //hwg_writelog( NULL, "paint-1 %d %d\r\n", pHWG->PTEXTSIZE.x, pHWG->PTEXTSIZE.y );
   SelectObject( hdc, pHWG->hFont );

   if( pHWG->bFontChanged )
   {
      TEXTMETRIC tm;
      GetTextMetrics( hdc, &tm );
      SetTextCharacterExtra( hdc, 0 ); /* do not add extra char spacing even if bold */

      pHWG->PTEXTSIZE.x = tm.tmAveCharWidth;
      pHWG->PTEXTSIZE.y = tm.tmHeight;
      pHWG->bFontChanged = 0;
   }
   //hwg_writelog( NULL, "paint-1a\r\n" );

   rcRect = hwpgt_GetColRowFromXYRect( pHWG, pps->rcPaint );
   //hwg_writelog( NULL, "paint-1b\r\n" );
   rcRect.top = (rcRect.top >= pHWG->iHeight-1)? pHWG->iHeight-1 : rcRect.top;
   rcRect.bottom = (rcRect.bottom >= pHWG->iHeight-1)? pHWG->iHeight-1 : rcRect.bottom;
   rcRect.left = (rcRect.left >= pHWG->iWidth-1)? pHWG->iWidth-1 : ( (rcRect.left<0)? 0 : rcRect.left );
   rcRect.right = (rcRect.right >= pHWG->iWidth-1)? pHWG->iWidth-1 : ( (rcRect.right<0)? 0 : rcRect.right ) ;

   //hwg_writelog( NULL, "paint-1 %d %d -> %d %d\r\n", rcRect.top, rcRect.left, rcRect.bottom, rcRect.right );
   for( iRow = rcRect.top; iRow <= rcRect.bottom; ++iRow )
   {
      int iCol, startCol, len;

      iCol = startCol = rcRect.left;
      len = 0;

      while( iCol <= rcRect.right )
      {
         int iPos = pHWG->iWidth * iRow + iCol;
         //unsigned char uc = (unsigned char) pHWG->screenBuf[iPos].iChar;
         wchar_t uc = (wchar_t) pHWG->screenBuf[iPos].iChar;

         iColor = pHWG->screenBuf[iPos].iColor;
         //hwg_writelog( NULL, "%d %d : %d %d \r\n", iCol, iPos, iColor, pHWG->screenBuf[iPos].iChar );
         if( len == 0 )
         {
            iOldColor = iColor;
         }
         else if( iColor != iOldColor )
         {
            //hwg_writelog( NULL, " p-1\r\n" );
            hwpgt_TextOut( pHWG, hdc, startCol, iRow, iOldColor, pHWG->TextLine, ( UINT ) len );
            iOldColor = iColor;
            startCol = iCol;
            len = 0;
         }
         //pHWG->TextLine[ len++ ] = ( TCHAR ) uc;
         pHWG->TextLine[ len++ ] = ( wchar_t ) uc;
         iCol++;
      }
      if( len > 0 ) {
         //hwg_writelog( NULL, " p-2 %d\r\n", iRow );
         hwpgt_TextOut( pHWG, hdc, startCol, iRow, iOldColor, pHWG->TextLine, ( UINT ) len );
      }
      if( pHWG->bCursorOn && iRow == pHWG->iRow && rcRect.left<=pHWG->iCol && rcRect.right>=pHWG->iCol )
      {
         POINT xy = hwpgt_GetXYFromColRow( pHWG, pHWG->iCol, pHWG->iRow );
         SelectObject( hdc, ( HGDIOBJ ) pHWG->hPen );
         MoveToEx( hdc, xy.x, xy.y + pHWG->PTEXTSIZE.y - 2, NULL );
         LineTo( hdc, xy.x + pHWG->PTEXTSIZE.x, xy.y + pHWG->PTEXTSIZE.y - 2 );
         //hwg_writelog( NULL, "Cursor: %d %d\r\n", pHWG->iCol, pHWG->iRow );
      }
   }

   //hwg_writelog( NULL, "paint-10\r\n" );

}

static unsigned int getFromStr( char ** ptr, PHB_CODEPAGE cdp, int bUtf8 )
{
   unsigned int iRes, i = 0;

   if( bUtf8 )
   {
      iRes = (unsigned int) utf8str_to_widechar( *ptr, &i );
      (*ptr) += i;
   } else
   {
      iRes = hb_cdpGetU16( cdp, (HB_UCHAR)**ptr );
      (*ptr) ++;
   }
   return iRes;
}

HB_FUNC( HWPGT_OUT )
{
   PHWPGT pHWG = (PHWPGT) HB_PARHANDLE( 1 );
   char * pText = (char*) hb_parc( 2 ), *ptr = pText;
   int iPos = pHWG->iWidth * pHWG->iRow + pHWG->iCol, iPos0 = iPos;
   int iPosLast = pHWG->iWidth * pHWG->iRow + pHWG->iWidth;
   int bUtf8 = hb_cdpIsUTF8( NULL );
   PHB_CODEPAGE cdp = hb_vmCDP();
   RECT rect;

   //hwg_writelog( NULL, "Out0: %d %d %s\r\n", pHWG->iRow, pHWG->iCol, pText );
   //hwg_writelog( NULL, "Out1: %d %d\r\n", iPos, iPosLast );
   while( *ptr && iPos < iPosLast )
   {
      pHWG->screenBuf[iPos].iColor = pHWG->iColor;
      pHWG->screenBuf[iPos].iChar = getFromStr( &ptr, cdp, bUtf8 );
      iPos ++;
   }

   rect.top = rect.bottom = pHWG->iRow;
   rect.left = pHWG->iCol;
   pHWG->iCol += (iPos - iPos0);
   rect.right = pHWG->iCol - 1;

   rect = hwpgt_GetXYFromColRowRect( pHWG, rect );
   InvalidateRect( pHWG->handle, &rect, FALSE );

}

HB_FUNC( HWPGT_BOX )
{
   PHWPGT pHWG = (PHWPGT) HB_PARHANDLE( 1 );
   int iTop = hb_parni(2), iLeft = hb_parni(3), iBottom = hb_parni(4), iRight = hb_parni(5);
   char * ptr = (char*) hb_parc( 6 );
   unsigned int uic;
   int bUtf8 = hb_cdpIsUTF8( NULL );
   int iPos = pHWG->iWidth * iTop + iLeft, i = 0, j, is, iLen;
   PHB_CODEPAGE cdp = hb_vmCDP();
   RECT rect;

   if( iTop == iBottom )
   {
      // Horizontal line
      iLen = iRight - iLeft;
      uic = getFromStr( &ptr, cdp, bUtf8 );
      while( i < iLen )
      {
         pHWG->screenBuf[iPos].iColor = pHWG->iColor;
         pHWG->screenBuf[iPos].iChar = uic;
         iPos ++; i++;
      }
   }
   else if( iLeft == iRight )
   {
      // Vertical line
      iLen = iBottom - iTop;
      uic = getFromStr( &ptr, cdp, bUtf8 );
      while( i < iLen )
      {
         pHWG->screenBuf[iPos].iColor = pHWG->iColor;
         pHWG->screenBuf[iPos].iChar = uic;
         iPos += pHWG->iWidth; i++;
      }
   }
   else
   {
      // Left upper
      pHWG->screenBuf[iPos].iColor = pHWG->iColor;
      pHWG->screenBuf[iPos].iChar = getFromStr( &ptr, cdp, bUtf8 );
      iPos ++;
      // Horizontal line
      iLen = iRight - iLeft - 2;
      uic = getFromStr( &ptr, cdp, bUtf8 );
      while( i <= iLen )
      {
         pHWG->screenBuf[iPos].iColor = pHWG->iColor;
         pHWG->screenBuf[iPos].iChar = uic;
         iPos ++; i++;
      }
      // Right upper
      uic = getFromStr( &ptr, cdp, bUtf8 );
      pHWG->screenBuf[iPos].iColor = pHWG->iColor;
      pHWG->screenBuf[iPos].iChar = uic;

      // Vertical line
      i = 0;
      iPos += pHWG->iWidth;
      iLen = iBottom - iTop - 2;
      uic = getFromStr( &ptr, cdp, bUtf8 );
      while( i <= iLen )
      {
         pHWG->screenBuf[iPos].iColor = pHWG->iColor;
         pHWG->screenBuf[iPos].iChar = uic;
         iPos += pHWG->iWidth; i++;
      }

      // Right lower
      uic = getFromStr( &ptr, cdp, bUtf8 );
      pHWG->screenBuf[iPos].iColor = pHWG->iColor;
      pHWG->screenBuf[iPos].iChar = uic;

      // Horizontal line
      i = 0;
      iLen = iRight - iLeft - 2;
      iPos --;
      uic = getFromStr( &ptr, cdp, bUtf8 );
      while( i <= iLen )
      {
         pHWG->screenBuf[iPos].iColor = pHWG->iColor;
         pHWG->screenBuf[iPos].iChar = uic;
         iPos --; i++;
      }

      // Left lower
      uic = getFromStr( &ptr, cdp, bUtf8 );
      pHWG->screenBuf[iPos].iColor = pHWG->iColor;
      pHWG->screenBuf[iPos].iChar = uic;

      // Vertical line
      i = 0;
      iPos -= pHWG->iWidth;
      iLen = iBottom - iTop - 2;
      uic = getFromStr( &ptr, cdp, bUtf8 );
      while( i <= iLen )
      {
         pHWG->screenBuf[iPos].iColor = pHWG->iColor;
         pHWG->screenBuf[iPos].iChar = uic;
         iPos -= pHWG->iWidth; i++;
      }

      // Fill space inside box
      uic = getFromStr( &ptr, cdp, bUtf8 );
      for( i = iTop+1; i < iBottom; i ++ )
         for( j = iLeft+1, is = pHWG->iWidth * i; j < iRight; j ++ )
         {
            pHWG->screenBuf[is + j].iColor = pHWG->iColor;
            pHWG->screenBuf[is + j].iChar = uic;
         }

   }

   rect.top = iTop;
   rect.bottom = iBottom;
   rect.left = iLeft;
   rect.right = iRight;

   rect = hwpgt_GetXYFromColRowRect( pHWG, rect );
   InvalidateRect( pHWG->handle, &rect, FALSE );

}

HB_FUNC( HWPGT_SCROLL )
{
   PHWPGT pHWG = (PHWPGT) HB_PARHANDLE( 1 );
   int iTop = hb_parni(2), iLeft = hb_parni(3), iBottom = hb_parni(4), iRight = hb_parni(5);
   int iVert = hb_parni(6), iHoriz = hb_parni(7);
   int i, j;
   int iWidth = pHWG->iWidth, is, is2;
   RECT rect;

   if( !iVert && !iHoriz )
   {
      for( i = iTop; i <= iBottom; i ++ )
         for( j = iLeft, is = iWidth * i; j <= iRight; j ++ )
         {
            pHWG->screenBuf[is + j].iColor = pHWG->iColor;
            pHWG->screenBuf[is + j].iChar = 32;
         }
   }
   else if( iVert )
   {
      for( i = iTop; i < iBottom; i ++ )
         for( j = iLeft, is = iWidth * i, is2 = iWidth * (i+1); j <= iRight; j ++ )
         {
            pHWG->screenBuf[is + j].iColor = pHWG->screenBuf[is2 + j].iColor;
            pHWG->screenBuf[is + j].iChar = pHWG->screenBuf[is2 + j].iChar;
         }
      for( j = iLeft, is = iWidth * iBottom; j <= iRight; j ++ )
      {
         pHWG->screenBuf[is + j].iColor = pHWG->iColor;
         pHWG->screenBuf[is + j].iChar = 32;
      }
   }
   else if( iHoriz )
   {
   }

   rect.top = iTop;
   rect.bottom = iBottom;
   rect.left = iLeft;
   rect.right = iRight;

   rect = hwpgt_GetXYFromColRowRect( pHWG, rect );
   InvalidateRect( pHWG->handle, &rect, FALSE );

}

HB_FUNC( HWPGT_SAVESCR )
{
   PHWPGT pHWG = (PHWPGT) HB_PARHANDLE( 1 );
   int iTop = hb_parni(2), iLeft = hb_parni(3), iBottom = hb_parni(4), iRight = hb_parni(5);
   int iLen = sizeof( HWCELL ) * (iRight-iLeft+1) * (iBottom-iTop+1);
   int i, j, is, iPos = 0;
   PHWCELL scrbuf = ( PHWCELL ) hb_xgrab( iLen + 1 );

   for( i = iTop; i <= iBottom; i ++ )
      for( j = iLeft, is = pHWG->iWidth * i; j <= iRight; j ++ )
      {
         scrbuf[iPos].iColor = pHWG->screenBuf[is + j].iColor;
         scrbuf[iPos].iChar = pHWG->screenBuf[is + j].iChar;
         iPos ++;
      }

   //hwg_writelog( NULL, "save10: %d %d\r\n", iLen, iPos );
   hb_retclen_buffer( (char*) scrbuf, iLen );
}

HB_FUNC( HWPGT_RESTSCR )
{
   PHWPGT pHWG = (PHWPGT) HB_PARHANDLE( 1 );
   int iTop = hb_parni(2), iLeft = hb_parni(3), iBottom = hb_parni(4), iRight = hb_parni(5);
   PHWCELL scrbuf = (PHWCELL) hb_parc( 6 );
   int i, j, is, iPos = 0;
   RECT rect;

   //hwg_writelog( NULL, "rest1: %d %d %d %d %d \r\n", hb_parclen(6), iTop, iLeft, iBottom, iRight );
   for( i = iTop; i <= iBottom; i ++ )
      for( j = iLeft, is = pHWG->iWidth * i; j <= iRight; j ++ )
      {
         pHWG->screenBuf[is + j].iColor = scrbuf[iPos].iColor;
         pHWG->screenBuf[is + j].iChar = scrbuf[iPos].iChar;
         //hwg_writelog( NULL, "rest2: %d\r\n", is+j );
         iPos ++;
      }

   //hwg_writelog( NULL, "rest3: %d %d %ul %ul\r\n", iPos, is+j, (unsigned long) &(pHWG->screenBuf[0]), (unsigned long) &(pHWG->screenBuf[is + j]) );

   rect.top = iTop;
   rect.bottom = iBottom;
   rect.left = iLeft;
   rect.right = iRight;

   rect = hwpgt_GetXYFromColRowRect( pHWG, rect );
   InvalidateRect( pHWG->handle, &rect, FALSE );

}

HB_FUNC( HWPGT_SETPOS )
{
   PHWPGT pHWG = (PHWPGT) HB_PARHANDLE( 1 );

   pHWG->iRow = hb_parni( 2 );
   pHWG->iCol = hb_parni( 3 );
}

HB_FUNC( HWPGT_SETKEYCP )
{
   PHWPGT pHWG = (PHWPGT) HB_PARHANDLE( 1 );

   pHWG->cdpIn = hb_cdpFind( hb_parc(2) );
}

HB_FUNC( HWPGT_SETCOLOR )
{
   PHWPGT pHWG = (PHWPGT) HB_PARHANDLE( 1 );

   pHWG->iColor = hb_parni( 2 );
}

HB_FUNC( HWPGT_MCOL )
{
   hb_retni( ( (PHWPGT) HB_PARHANDLE( 1 ) )->MousePos.x );
}

HB_FUNC( HWPGT_MROW )
{
   hb_retni( ( (PHWPGT) HB_PARHANDLE( 1 ) )->MousePos.y );
}

HB_FUNC( HWPGT_COL )
{
   hb_retni( ( (PHWPGT) HB_PARHANDLE( 1 ) )->iCol );
}

HB_FUNC( HWPGT_ROW )
{
   hb_retni( ( (PHWPGT) HB_PARHANDLE( 1 ) )->iRow );
}

HB_FUNC( HWPGT_MAXCOL )
{
   hb_retni( ( (PHWPGT) HB_PARHANDLE( 1 ) )->iWidth - 1 );
}

HB_FUNC( HWPGT_MAXROW )
{
   hb_retni( ( (PHWPGT) HB_PARHANDLE( 1 ) )->iHeight - 1 );
}

HB_FUNC( HWPGT_CURSOR )
{
   PHWPGT pHWG = (PHWPGT) HB_PARHANDLE( 1 );
   RECT rect;

   pHWG->bCursorOn = hb_parl( 2 );

   rect.top = rect.bottom = pHWG->iRow;
   rect.left = rect.right = pHWG->iCol;

   rect = hwpgt_GetXYFromColRowRect( pHWG, rect );
   InvalidateRect( pHWG->handle, &rect, FALSE );

}

HB_FUNC( HWPGT_KEYEVENT )
{
   PHWPGT pHWG = (PHWPGT) HB_PARHANDLE( 1 );
   UINT message = (UINT) hb_parni(2);
   WPARAM wParam = (WPARAM) hb_parnl( 3 );
   LPARAM lParam = (LPARAM) hb_parnl( 4 );
   int iKey = 0, iFlags = pHWG->keyFlags, iKeyPad = 0;

   //hwg_writelog( NULL, "msg %d %lu %lu %d\r\n", message, (unsigned long) wParam, (unsigned long) lParam, hwpgt_GetKeyFlags() );
   switch( message )
   {
      case WM_KEYDOWN:
      case WM_SYSKEYDOWN:
         pHWG->IgnoreWM_SYSCHAR = HB_FALSE;
         iFlags = hwpgt_GetKeyFlags();
         switch( wParam )
         {
            case VK_BACK:
               pHWG->IgnoreWM_SYSCHAR = HB_TRUE;
               iKey = HB_KX_BS;
               break;
            case VK_TAB:
               pHWG->IgnoreWM_SYSCHAR = HB_TRUE;
               iKey = HB_KX_TAB;
               break;
            case VK_RETURN:
               pHWG->IgnoreWM_SYSCHAR = HB_TRUE;
                  iKey = HB_KX_ENTER;
                  if( lParam & HWG_EXTKEY_FLAG )
                     iFlags |= HB_KF_KEYPAD;
               break;
            case VK_ESCAPE:
               pHWG->IgnoreWM_SYSCHAR = HB_TRUE;
               iKey = HB_KX_ESC;
               break;

            case VK_PRIOR:
               iKeyPad = HB_KX_PGUP;
               break;
            case VK_NEXT:
               iKeyPad = HB_KX_PGDN;
               break;
            case VK_END:
               iKeyPad = HB_KX_END;
               break;
            case VK_HOME:
               iKeyPad = HB_KX_HOME;
               break;
            case VK_LEFT:
               iKeyPad = HB_KX_LEFT;
               break;
            case VK_UP:
               iKeyPad = HB_KX_UP;
               break;
            case VK_RIGHT:
               iKeyPad = HB_KX_RIGHT;
               break;
            case VK_DOWN:
               iKeyPad = HB_KX_DOWN;
               break;
            case VK_INSERT:
               iKeyPad = HB_KX_INS;
               break;
            case VK_DELETE:
               iKey = HB_KX_DEL;
               if( ( lParam & HWG_EXTKEY_FLAG ) == 0 )
                  iFlags |= HB_KF_KEYPAD;
               break;

            case VK_F1:
               iKey = HB_KX_F1;
               break;
            case VK_F2:
               iKey = HB_KX_F2;
               break;
            case VK_F3:
               iKey = HB_KX_F3;
               break;
            case VK_F4:
               iKey = HB_KX_F4;
               break;
            case VK_F5:
               iKey = HB_KX_F5;
               break;
            case VK_F6:
               iKey = HB_KX_F6;
               break;
            case VK_F7:
               iKey = HB_KX_F7;
               break;
            case VK_F8:
               iKey = HB_KX_F8;
               break;
            case VK_F9:
               iKey = HB_KX_F9;
               break;
            case VK_F10:
               iKey = HB_KX_F10;
               break;
            case VK_F11:
               iKey = HB_KX_F11;
               break;
            case VK_F12:
               iKey = HB_KX_F12;
               break;

            case VK_SNAPSHOT:
               iKey = HB_KX_PRTSCR;
               break;
            case VK_CANCEL:
               if( ( lParam & HWG_EXTKEY_FLAG ) == 0 )
                  break;
               iFlags |= HB_KF_CTRL;
               /* fallthrough */
            case VK_PAUSE:
               pHWG->IgnoreWM_SYSCHAR = HB_TRUE;
               iKey = HB_KX_PAUSE;
               break;

            case VK_CLEAR:
               iKeyPad = HB_KX_CENTER;
               break;

            case VK_NUMPAD0:
            case VK_NUMPAD1:
            case VK_NUMPAD2:
            case VK_NUMPAD3:
            case VK_NUMPAD4:
            case VK_NUMPAD5:
            case VK_NUMPAD6:
            case VK_NUMPAD7:
            case VK_NUMPAD8:
            case VK_NUMPAD9:
               if( iFlags & HB_KF_CTRL )
               {
                  pHWG->IgnoreWM_SYSCHAR = HB_TRUE;
                  iKey = ( int ) wParam - VK_NUMPAD0 + '0';
               }
               else if( iFlags == HB_KF_ALT || iFlags == HB_KF_ALTGR )
                  iFlags = 0; /* for ALT + <ASCII_VALUE_FROM_KEYPAD> */
               iFlags |= HB_KF_KEYPAD;
               break;
            case VK_DECIMAL:
            case VK_SEPARATOR:
               iFlags |= HB_KF_KEYPAD;
               if( iFlags & HB_KF_CTRL )
               {
                  pHWG->IgnoreWM_SYSCHAR = HB_TRUE;
                  iKey = '.';
               }
               break;

            case VK_DIVIDE:
               iFlags |= HB_KF_KEYPAD;
               if( iFlags & HB_KF_CTRL )
                  iKey = '/';
               break;
            case VK_MULTIPLY:
               iFlags |= HB_KF_KEYPAD;
               if( iFlags & HB_KF_CTRL )
                  iKey = '*';
               break;
            case VK_SUBTRACT:
               iFlags |= HB_KF_KEYPAD;
               if( iFlags & HB_KF_CTRL )
                  iKey = '-';
               break;
            case VK_ADD:
               iFlags |= HB_KF_KEYPAD;
               if( iFlags & HB_KF_CTRL )
                  iKey = '+';
               break;
#ifdef VK_OEM_2
            case VK_OEM_2:
               if( ( iFlags & HB_KF_CTRL ) != 0 && ( iFlags & HB_KF_SHIFT ) != 0 )
                  iKey = '?';
               break;
#endif
#ifdef VK_APPS
            case VK_APPS:
               iKey = HB_K_MENU;
               break;
#endif
            case VK__TILDA:
               if( iFlags & HB_KF_CTRL )
                  iKey = VK__BACKQUOTE;
               break;
            case 48:
               if( iFlags & HB_KF_CTRL )
                  iKey = VK__CTRL_0;
               break;
            case 49:
               if( iFlags & HB_KF_CTRL )
                  iKey = VK__CTRL_1;
               break;
            case 50:
               if( iFlags & HB_KF_CTRL )
                  iKey = VK__CTRL_2;
               break;
            case 51:
               if( iFlags & HB_KF_CTRL )
                  iKey = VK__CTRL_3;
               break;
            case 53:
               if( iFlags & HB_KF_CTRL )
                  iKey = VK__CTRL_4;
               break;
            case 54:
               if( iFlags & HB_KF_CTRL )
                  iKey = VK__CTRL_5;
               break;
            case 55:
               if( iFlags & HB_KF_CTRL )
                  iKey = VK__CTRL_6;
               break;
            case 56:
               if( iFlags & HB_KF_CTRL )
                  iKey = VK__CTRL_7;
               break;
            case 57:
               if( iFlags & HB_KF_CTRL )
                  iKey = VK__CTRL_8;
               break;
            case 58:
               if( iFlags & HB_KF_CTRL )
                  iKey = VK__CTRL_9;
               break;
         }
         if( iKeyPad != 0 )
         {
            iKey = iKeyPad;
            if( ( lParam & HWG_EXTKEY_FLAG ) == 0 )
            {
               if( iFlags == HB_KF_ALT || iFlags == HB_KF_ALTGR )
                  iFlags = iKey = 0; /* for ALT + <ASCII_VALUE_FROM_KEYPAD> */
               else
                  iFlags |= HB_KF_KEYPAD;
            }
         }
         pHWG->keyFlags = iFlags;
         if( iKey != 0 )
            iKey = HB_INKEY_NEW_KEY( iKey, hwpgt_UpdateKeyFlags( iFlags ) );
         break;

      case WM_CHAR:
         //hwg_writelog( NULL, "wm_char %lu\r\n", (unsigned long) wParam );
         if( ( ( iFlags & HB_KF_CTRL ) != 0 && ( iFlags & HB_KF_ALT ) != 0 ) ||
             ( iFlags & HB_KF_ALTGR ) != 0 )
            /* workaround for AltGR and some German/Italian keyboard */
            iFlags &= ~( HB_KF_CTRL | HB_KF_ALT | HB_KF_ALTGR );
         /* fallthrough */
      case WM_SYSCHAR:
         iFlags = hwpgt_UpdateKeyFlags( iFlags );
         if( ! pHWG->IgnoreWM_SYSCHAR )
         {
            iKey = ( int ) wParam;

            if( ( iFlags & HB_KF_CTRL ) != 0 && iKey >= 0 && iKey < 32 )
            {
               iKey += 'A' - 1;
               iKey = HB_INKEY_NEW_KEY( iKey, iFlags );
            }
            else
            {
               if( message == WM_SYSCHAR && ( iFlags & HB_KF_ALT ) != 0 )
               {
                  switch( HIWORD( lParam ) & 0xFF )
                  {
                     case  2:
                        iKey = '1';
                        break;
                     case  3:
                        iKey = '2';
                        break;
                     case  4:
                        iKey = '3';
                        break;
                     case  5:
                        iKey = '4';
                        break;
                     case  6:
                        iKey = '5';
                        break;
                     case  7:
                        iKey = '6';
                        break;
                     case  8:
                        iKey = '7';
                        break;
                     case  9:
                        iKey = '8';
                        break;
                     case 10:
                        iKey = '9';
                        break;
                     case 11:
                        iKey = '0';
                        break;
                     case 13:
                        iKey = '=';
                        break;
                     case 14:
                        iKey = HB_KX_BS;
                        break;
                     case 16:
                        iKey = 'Q';
                        break;
                     case 17:
                        iKey = 'W';
                        break;
                     case 18:
                        iKey = 'E';
                        break;
                     case 19:
                        iKey = 'R';
                        break;
                     case 20:
                        iKey = 'T';
                        break;
                     case 21:
                        iKey = 'Y';
                        break;
                     case 22:
                        iKey = 'U';
                        break;
                     case 23:
                        iKey = 'I';
                        break;
                     case 24:
                        iKey = 'O';
                        break;
                     case 25:
                        iKey = 'P';
                        break;
                     case 30:
                        iKey = 'A';
                        break;
                     case 31:
                        iKey = 'S';
                        break;
                     case 32:
                        iKey = 'D';
                        break;
                     case 33:
                        iKey = 'F';
                        break;
                     case 34:
                        iKey = 'G';
                        break;
                     case 35:
                        iKey = 'H';
                        break;
                     case 36:
                        iKey = 'J';
                        break;
                     case 37:
                        iKey = 'K';
                        break;
                     case 38:
                        iKey = 'L';
                        break;
                     case 44:
                        iKey = 'Z';
                        break;
                     case 45:
                        iKey = 'X';
                        break;
                     case 46:
                        iKey = 'C';
                        break;
                     case 47:
                        iKey = 'V';
                        break;
                     case 48:
                        iKey = 'B';
                        break;
                     case 49:
                        iKey = 'N';
                        break;
                     case 50:
                        iKey = 'M';
                        break;
                  }
               }
               if( iKey>=127 && iKey<=255 )
               {
                  PHB_CODEPAGE cdp = hb_vmCDP();
                  if( !(pHWG->cdpIn == cdp) )
                  {
                     //hwg_writelog( NULL, "cp: %s %s\r\n", pHWG->cdpIn->id, cdp->id );
                     if( HB_CDP_ISUTF8( cdp ) )
                     {
                        iKey = hb_cdpGetU16( pHWG->cdpIn, (HB_UCHAR)iKey );
                        if( iKey > 255 )
                           iKey |= 0x01000000;
                     } else
                        iKey = hb_cdpTranslateChar( iKey, pHWG->cdpIn, cdp );
                  }
                  iKey |= 0x42000000;
               }
               else if( iKey < 127 && ( iFlags & ( HB_KF_CTRL | HB_KF_ALT ) ) )
                  iKey = HB_INKEY_NEW_KEY( iKey, iFlags );
               else
               {
                  if( pHWG->CodePage == OEM_CHARSET )
                     iKey = hwpgt_key_ansi_to_oem( iKey );
                  iKey = HB_INKEY_NEW_CHARF( iKey, iFlags );
               }
            }
         }
         pHWG->IgnoreWM_SYSCHAR = HB_FALSE;
         break;
   }

   //if( message == WM_CHAR )
   //   hwg_writelog( NULL, "iKey: %d \r\n\r\n", iKey );
   hb_retni( iKey );
}

HB_FUNC( HWPGT_MOUSEEVENT )
{
   PHWPGT pHWG = (PHWPGT) HB_PARHANDLE( 1 );
   UINT message = (UINT) hb_parni(2);
   WPARAM wParam = (WPARAM) hb_parnl( 3 );
   LPARAM lParam = (LPARAM) hb_parnl( 4 );
   SHORT keyCode = 0;
   POINT xy, colrow;

   xy.x = LOWORD( lParam );
   xy.y = HIWORD( lParam );

   pHWG->ExactMousePos.y = xy.y;
   pHWG->ExactMousePos.x = xy.x;

   colrow = hwpgt_GetColRowFromXY( pHWG, xy.x, xy.y );
   hwpgt_SetMousePos( pHWG, colrow.y, colrow.x );
   //hwg_writelog( NULL, "%d : %d %d | %d %d\r\n", message, xy.y, xy.x, colrow.y, colrow.x );

   switch( message )
   {
      case WM_LBUTTONDBLCLK:
         keyCode = K_LDBLCLK;
         break;

      case WM_RBUTTONDBLCLK:
         keyCode = K_RDBLCLK;
         break;

      case WM_LBUTTONDOWN:
         keyCode = K_LBUTTONDOWN;
         break;

      case WM_RBUTTONDOWN:
         keyCode = K_RBUTTONDOWN;
         break;

      case WM_RBUTTONUP:
         keyCode = K_RBUTTONUP;
         break;

      case WM_LBUTTONUP:
         keyCode = K_LBUTTONUP;
         break;

      case WM_MBUTTONDOWN:
         keyCode = K_MBUTTONDOWN;
         break;

      case WM_MBUTTONUP:
         keyCode = K_MBUTTONUP;
         break;

      case WM_MBUTTONDBLCLK:
         keyCode = K_MDBLCLK;
         break;

      case WM_MOUSEWHEEL:
         keyCode = ( SHORT ) ( HIWORD( wParam ) > 0 ? K_MWFORWARD : K_MWBACKWARD );
         break;
   }

   hb_retni( keyCode );

}

HB_FUNC( HWPGT_INFO )
{
   PHWPGT pHWG = (PHWPGT) HB_PARHANDLE( 1 );
   int iType = hb_parni( 2 ), i;
   PHB_ITEM pItem;

   switch( iType )
   {
      case HB_GTI_DESKTOPCOLS:
         hb_retni( GetSystemMetrics( SM_CXSCREEN ) / pHWG->PTEXTSIZE.x );
         break;

      case HB_GTI_DESKTOPROWS:
         hb_retni( GetSystemMetrics( SM_CYSCREEN ) / pHWG->PTEXTSIZE.y );
         break;

      case HB_GTI_PALETTE:
         pItem = hb_itemNew( NULL );
         hb_arrayNew( pItem, 16 );
         for( i = 0; i < 16; i++ )
            hb_arraySetNL( pItem, i + 1, pHWG->COLORS[ i ] );
         hb_itemRelease( hb_itemReturn( pItem ) );
         //hwg_writelog( NULL, "palette\r\n" );
         if( HB_ISARRAY( 3 ) )
         {
            pItem = hb_param( 3, HB_IT_ARRAY );
            if( hb_arrayLen( pItem ) == 16 )
            {
               for( i = 0; i < 16; i++ )
                  pHWG->COLORS[ i ] = hb_arrayGetNL( pItem, i + 1 );

               if( pHWG->handle )
                  InvalidateRect( pHWG->handle, NULL, FALSE );
            }
         }
         break;

      default:
         hb_ret();

   }
}

HB_FUNC( HWPGT_TEST1 )
{
   hb_retni( hb_cdpGetU16( hb_vmCDP(), (HB_UCHAR) hb_parni(1) ) );
}

wchar_t utf8_to_widechar(int utf8_char) {
    wchar_t wide_char = 0;

    if ((utf8_char & 0x80) == 0) {
        // Single-byte character
        wide_char = (wchar_t)utf8_char;
    } else if ((utf8_char & 0xE0) == 0xC0) {
        // Two-byte character
        wide_char = (wchar_t)(((utf8_char & 0x1F) << 6) | (utf8_char & 0x3F));
    } else if ((utf8_char & 0xF0) == 0xE0) {
        // Three-byte character
        wide_char = (wchar_t)(((utf8_char & 0x0F) << 12) | ((utf8_char & 0x3F) << 6) | (utf8_char & 0x3F));
    } else if ((utf8_char & 0xF8) == 0xF0) {
        // Four-byte character
        wide_char = (wchar_t)(((utf8_char & 0x07) << 18) | ((utf8_char & 0x3F) << 12) | ((utf8_char & 0x3F) << 6) | (utf8_char & 0x3F));
    }

    return wide_char;
}

HB_FUNC( HWPGT_TEST2 )
{
   wchar_t wc = utf8_to_widechar( hb_parni(1) );
   hwg_writelog( NULL, "wc %u %u %d\r\n", ((unsigned char*)(&wc))[0], ((unsigned char*)(&wc))[1], sizeof(wc) );

   hb_retni( (int) wc );
}

HB_FUNC( HWPGT_TEST3 )
{
   const char * szUtf8 = hb_parc(1);
   unsigned int iLen = strlen( szUtf8 ), i = 0, iSch = 0;
   wchar_t wstr[100];

   while( i < iLen && iSch < 100 )
   {
      wstr[iSch] = utf8str_to_widechar( szUtf8, &i );
      iSch ++;
   }
   //hwg_writelog( NULL, "wc %u %u %d\r\n", ((unsigned char*)(&wc))[0], ((unsigned char*)(&wc))[1], sizeof(wc) );

   hb_retclen( (char*) wstr, iSch*2 );
}
