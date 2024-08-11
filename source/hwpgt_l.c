/*
 *
 */

#include "hbapi.h"
#include "hbapiitm.h"
#include "hbapifs.h"
#include "hbgtcore.h"
#include "hbinit.h"
#include "hbvm.h"
#include "hbstack.h"
#include "hbdate.h"
#include "guilib.h"

#include <cairo.h>
#include "gtk/gtk.h"
#include "gdk/gdkkeysyms.h"
#include "hwgtk.h"

#include "windows.ch"

#define UNICODE

#define HWND          GtkWidget*
#define HFONT         PHWGUI_FONT
#define TCHAR         char
#define COLORREF      long int
#define LONG          long int
#define OEM_CHARSET   255

#define HWG_DEFAULT_FONT_HEIGHT  20
#define HWG_DEFAULT_FONT_WIDTH   10

#define WM_MY_UPDATE_CARET  ( WM_USER + 0x0101 )
#define MSG_USER_SIZE  0x502

#define HWG_EXTKEY_FLAG              ( 1 << 24 )

typedef struct tagRECT
{
    gint    left;
    gint    top;
    gint    right;
    gint    bottom;
} RECT, *PRECT, *LPRECT;

typedef struct tagPOINT
{
    gint    x;
    gint    y;
} POINT;

extern void hwg_writelog( const char *, const char *, ... );
extern void hwg_setcolor( cairo_t *, long int );

typedef struct
{
   int iColor;
   int iChar;
} HWCELL, * PHWCELL;

typedef struct
{
   HWND     hParent;
   HWND     handle;

   HFONT    hFont;

   int      iBtop, iBleft, iBwidth, iBheight, iWwidth, iWheight;
   int      iMarginL, iMarginT, iMarginR, iMarginB;
   POINT    PTEXTSIZE;      /* size of the fixed width font */

   int      iHeight;        /* window height */
   int      iWidth;         /* window width */
   int      iRow;           /* cursor row position */
   int      iCol;           /* cursor column position */
   int      ROWS;           /* number of displayable rows in window */
   int      COLS;           /* number of displayable columns in window */

   TCHAR *  TextLine;
   COLORREF COLORS[ 16 ];   /* colors */

   int      iColor;

   PHWCELL  screenBuf;

   HB_BOOL  bCursorOn;
   //HPEN     hPen;

   POINT    MousePos;       /* the last mouse position */
   POINT    ExactMousePos;  /* the last mouse position in pixels*/

   PHB_CODEPAGE   cdpIn;
   int      CodePage;       /* Code page to use for display characters */
   HB_BOOL  bFontChanged;

} HWPGT, * PHWPGT;

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

static void hwpgt_CalcFontSize( PHWGUI_HDC hdc, PangoFontDescription * hFont, int * pWidth, int * pHeight, int bm )
{
   if( !hdc->cr )
   {
      *pWidth = *pHeight = -1;
      return;
   }
   if( bm )
   {
      PangoContext * context;
      PangoFontMetrics * metrics;

      context = pango_layout_get_context( hdc->layout );
      metrics = pango_context_get_metrics( context, hFont, NULL );

      *pHeight = ( pango_font_metrics_get_ascent( metrics ) +
         pango_font_metrics_get_descent( metrics ) ) / PANGO_SCALE;
      *pWidth = pango_font_metrics_get_approximate_char_width(metrics) / PANGO_SCALE;

      pango_font_metrics_unref( metrics );
   }
   else
   {
      PangoRectangle rc;

      pango_layout_set_font_description( hdc->layout, hFont );
      pango_layout_set_text( hdc->layout, "gA", 2 );
      pango_layout_get_pixel_extents( hdc->layout, &rc, NULL );
      *pWidth = rc.width/2;
      *pHeight = PANGO_DESCENT( rc );
   }
}

static void hwpgt_ResetWindowSize( PHWPGT pHWG )
{
   //RECT  wi;
   //GtkAllocation alloc;
   int height = -1, width = -1, iWinHeight, iWinWidth;
   HWGUI_HDC hdc;

   hdc.window = gtk_widget_get_window( pHWG->handle );
   hdc.cr = gdk_cairo_create( hdc.window );
   hdc.layout = pango_cairo_create_layout( hdc.cr );

   hwpgt_CalcFontSize( &hdc, pHWG->hFont->hFont, &width, &height, 0 );
   pHWG->PTEXTSIZE.x = width;
   pHWG->PTEXTSIZE.y = height;

   g_object_unref( hdc.layout );
   cairo_destroy( hdc.cr );
   //hwg_writelog( NULL, "ResetSize-1 %d %d\r\n", width, height );

   //hwpgt_GetWindowRect( pHWG->hWnd, &wi );
   //gtk_widget_get_allocation( pHWG->handle, &alloc );

   height = ( int ) ( pHWG->PTEXTSIZE.y * pHWG->ROWS + pHWG->iMarginT + pHWG->iMarginB );
   width  = ( int ) ( pHWG->PTEXTSIZE.x * pHWG->COLS + pHWG->iMarginL + pHWG->iMarginR );

   iWinHeight = height + (pHWG->iWheight-pHWG->iBheight); // + (pi.bottom-pi.top-ci.bottom);
   iWinWidth = width + (pHWG->iWwidth-pHWG->iBwidth); // + (pi.right-pi.left-ci.right);

   gtk_window_resize( GTK_WINDOW( pHWG->hParent ), iWinWidth+2, iWinHeight + 2 );
   gtk_widget_set_size_request( pHWG->handle, width, height );

   //hwg_writelog( NULL, "ResetSize-1 %d %d %d %d\r\n", width, height, iWinWidth, iWinHeight );
   //hwg_writelog( NULL, "ResetSize-10\r\n" );

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

   pHWG->TextLine = ( TCHAR * ) hb_xgrab( pHWG->COLS * sizeof( TCHAR ) );

   pHWG->COLORS[ 0 ]       = 0x000000;   //BLACK
   pHWG->COLORS[ 1 ]       = 0xAA0000;   //BLUE
   pHWG->COLORS[ 2 ]       = 0x00AA00;   //GREEN
   pHWG->COLORS[ 3 ]       = 0xAAAA00;   //CYAN
   pHWG->COLORS[ 4 ]       = 0x0000AA;   //RED
   pHWG->COLORS[ 5 ]       = 0xAA00AA;   //MAGENTA
   pHWG->COLORS[ 6 ]       = 0x0055AA;   //BROWN
   pHWG->COLORS[ 7 ]       = 0xAAAAAA;   //LIGHT_GRAY
   pHWG->COLORS[ 8 ]       = 0x555555;   //GRAY
   pHWG->COLORS[ 9 ]       = 0xFF5555;   //BRIGHT_BLUE
   pHWG->COLORS[ 10 ]      = 0x55FF55;   //BRIGHT_GREEN
   pHWG->COLORS[ 11 ]      = 0xFFFF55;   //BRIGHT_CYAN
   pHWG->COLORS[ 12 ]      = 0x5555FF;   //BRIGHT_RED
   pHWG->COLORS[ 13 ]      = 0xFF55FF;   //BRIGHT_MAGENTA
   pHWG->COLORS[ 14 ]      = 0x55FFFF;   //YELLOW
   pHWG->COLORS[ 15 ]      = 0xFFFFFF;   //WHITE

   pHWG->iHeight   = pHWG->ROWS;
   pHWG->iWidth    = pHWG->COLS;
   iSize = pHWG->iHeight * pHWG->iWidth;
   pHWG->screenBuf = ( PHWCELL ) hb_xgrab( sizeof( HWCELL ) * iSize );
   for( i = 0; i < iSize; i++ )
   {
      pHWG->screenBuf[i].iColor = 16;
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

   pHWG->bFontChanged  = 1;

   //hwpgt_ResetWindowSize( pHWG );
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

static int i2string( int is, unsigned char* s )
{
   int i = 4, iLen = 0;

   while( i && (is & ( 0xff << ((i-1)*8) )) == 0 )
      i --;
   while( i )
   {
      *s ++ = ( is >> ((i-1)*8) ) & 0xff;
      i --;
      iLen ++;
   }
   return iLen;
}

HB_FUNC( HWPGT_PAINT )
{
   PHWPGT pHWG = (PHWPGT) HB_PARHANDLE( 1 );
   PHWGUI_HDC hdc = (PHWGUI_HDC) HB_PARHANDLE( 3 );
   cairo_t * cr = hdc->cr;
   PangoLayout * layout = hdc->layout;
   long int lxy = hb_parnl( 4 ), lwh = hb_parnl( 5 );

   int iRow;
   int iColor, iOldColor = 0; // iCursorColor
   GdkRectangle area;

   //hwg_writelog( NULL, "_PaintText-0\r\n" );

   if( pHWG->bFontChanged )
   {
      pHWG->bFontChanged = 0;
      hwpgt_ResetWindowSize( pHWG );
   }
   area.x = ( ( lxy & 0xffff ) - pHWG->iMarginL ) / pHWG->PTEXTSIZE.x;
   area.y = ( ( (lxy >> 16) & 0xffff ) - pHWG->iMarginT ) / pHWG->PTEXTSIZE.y;
   area.width = ( lwh & 0xffff ) / pHWG->PTEXTSIZE.x;
   area.height = ( (lwh >> 16) & 0xffff ) / pHWG->PTEXTSIZE.y;

   //hwg_writelog( NULL, "_PaintText-1 %d %d %d %d @ %lu %lu @ %d %d\r\n",
   //   area.x, area.y, area.width, area.height, lxy, lwh, ( lwh & 0xffff ), ( (lwh >> 16) & 0xffff ) );

   area.y = (area.y >= pHWG->iHeight-1)? pHWG->iHeight-1 : area.y;
   area.height = (area.height >= pHWG->iHeight-1)? pHWG->iHeight-1 : area.height;
   area.x = (area.x >= pHWG->iWidth-1)? pHWG->iWidth-1 : ( (area.x<0)? 0 : area.x );
   area.width = (area.width >= pHWG->iWidth-1)? pHWG->iWidth-1 : ( (area.width<0)? 0 : area.width ) ;

   //hwg_writelog( NULL, "_PaintText-1a %d %d %d %d\r\n", area.x, area.y, area.width, area.height );

   if( pHWG->hFont )
      pango_layout_set_font_description( layout, ((PHWGUI_FONT)(pHWG->hFont))->hFont );

   for( iRow = area.y; iRow <= area.y+area.height; ++iRow )
   {
      int iCol, startCol, iLastCol, len;
      unsigned int x1, y1, x2, y2;
      int iPos, iStart, iEnd;

      y1 = iRow * pHWG->PTEXTSIZE.y + pHWG->iMarginT;
      y2 = y1 + pHWG->PTEXTSIZE.y;

      iCol = startCol = area.x;
      iLastCol = iCol + area.width;
      len = 0;
      iStart = iEnd = pHWG->iWidth * iRow + iCol;

      while( 1 )
      {
         int iChar;
         char buf[8];
         int i, iCharLen;

         if( iCol <= iLastCol )
         {
            iPos = pHWG->iWidth * iRow + iCol;
            iColor = (HB_UCHAR) pHWG->screenBuf[iPos].iColor;
         }
         if( len == 0 )
            iOldColor = iColor;
         else if( iColor != iOldColor || iCol == iLastCol )
         {
            x1 = startCol * pHWG->PTEXTSIZE.x + pHWG->iMarginL;
            x2 = x1 + (iCol-startCol+1) * pHWG->PTEXTSIZE.x;
            // draw background
            hwg_setcolor( cr, pHWG->COLORS[ ( iOldColor >> 4 ) & 0x0F ] );
            cairo_rectangle( cr, (gdouble)x1, (gdouble)y1, (gdouble)(x2-x1+1), (gdouble)(y2-y1) );
            cairo_fill( cr );

            // draw text
            hwg_setcolor( cr, pHWG->COLORS[ iOldColor & 0x0F ] );
            pango_layout_set_width( layout, pHWG->PTEXTSIZE.x*PANGO_SCALE );
            pango_layout_set_justify( layout, 1 );
            //hwg_writelog( NULL, "PaintText-2 %d %d\r\n", iStart, len );
            //for( i = 0, iPos = iStart; i < len; i++, iPos++ )
            for( i = 0, iPos = iStart; iPos <= iEnd; i++, iPos++ )
            {
               iChar = pHWG->screenBuf[iPos].iChar;
               if( iChar != 32 )
               {
                  x1 = (startCol+i) * pHWG->PTEXTSIZE.x + pHWG->iMarginL;
                  iCharLen = i2string( pHWG->screenBuf[iPos].iChar, (unsigned char*) buf );
                  pango_layout_set_text( layout, buf, iCharLen );
                  cairo_move_to( cr, (gdouble)x1, (gdouble)y1 );
                  pango_cairo_show_layout( cr, layout );
                  //hwg_writelog( NULL, "PaintText-3 %d %d %s %d\r\n", x1, y1, buf, iCharLen );
               }
            }
            //hwg_writelog( NULL, "PaintText-4 %d %d %d %d %d\r\n", iRow, startCol, iCol, x1, x2 );
            iOldColor = iColor;
            startCol = iCol;
            iStart = iEnd = pHWG->iWidth * iRow + iCol;
            len = 0;
         }
         if( iCol == iLastCol )
            break;
         len++;
         iCol++; iEnd++;
      }
      cairo_stroke( cr );
      if( pHWG->bCursorOn && iRow == pHWG->iRow && area.x<=pHWG->iCol && (area.x+area.width)>=pHWG->iCol )
      {
         x1 = pHWG->iCol * pHWG->PTEXTSIZE.x + pHWG->iMarginL;
         x2 = x1 + pHWG->PTEXTSIZE.x;
         //hwg_writelog( NULL, "TextOut-draw cursor %lu\r\n", iCaretMs );
         hwg_setcolor( cr, 0xffffff );
         cairo_rectangle( cr, (gdouble)x1+1, (gdouble)y2-2, (gdouble)(x2-x1-1), (gdouble)2 );
         cairo_stroke( cr );
      }
   }

   //hwg_writelog( NULL, "_PaintText-10\r\n" );

}

unsigned int utf8str_to_int( const char *utf8_char, unsigned int *piPos )
{
    unsigned int uiRes = 0;
    char * ptr = (char*) utf8_char + *piPos;

    if ((*ptr & 0x80) == 0) {
        // Single-byte character
        uiRes = (unsigned int )(*ptr);
        (*piPos) ++;
    } else if ((*ptr & 0xE0) == 0xC0) {
        // Two-byte character
        uiRes = (unsigned int )(((ptr[0] & 0xFF) << 8) | (ptr[1] & 0xFF));
        (*piPos) += 2;
    } else if ((*ptr & 0xF0) == 0xE0) {
        // Three-byte character
        uiRes = (unsigned int )(((ptr[0] & 0xFF) << 16) | ((ptr[1] & 0xFF) << 8) |
           (ptr[2] & 0xFF));
        (*piPos) += 3;
    } else if ((*ptr & 0xF8) == 0xF0) {
        // Four-byte character
        uiRes = (unsigned int )(((ptr[0] & 0xFF) << 24) | ((ptr[1] & 0xFF) << 16) |
           ((ptr[2] & 0xFF) << 8) | (ptr[3] & 0xFF));
        (*piPos) += 4;
    }

    return uiRes;
}

static int wtoutf8( unsigned int iUCS )
{
   int iPos, im;
   int iUtf8 = 0;
   int iCharLength;
   unsigned int iUTF8Prefix;

   /* ASCII characters. */
   if( ( iUCS & ~0x0000007F ) == 0 )
   {
      /* Modified UTF-8, special case */
      if( iUCS == 0 )
      {
         return 0xC080;
      }

      return iUCS;
   }
   if( ( iUCS & ~0x000007FF ) == 0 )
   {
      iUTF8Prefix = 0xC0;       // 11000000b
      iCharLength = 2;
   }
   else if( ( iUCS & ~0x0000FFFF ) == 0 )
   {
      iUTF8Prefix = 0xE0;       // 11100000b
      iCharLength = 3;
   }
   else if( ( iUCS & ~0x001FFFFF ) == 0 )
   {
      iUTF8Prefix = 0xF0;       // 11110000b
      iCharLength = 4;
   }
   else if( ( iUCS & ~0x03FFFFFF ) == 0 )
   {
      iUTF8Prefix = 0xF8;       // 11111000b
      iCharLength = 5;
   }
   else if( ( iUCS & ~0x7FFFFFFF ) == 0 )
   {
      iUTF8Prefix = 0xFC;       // 11111100b
      iCharLength = 6;
   }
   /* Incorrect multibyte character */
   else
   {
      return -1;
   }

   /*
    * Convert UCS character to UTF8. Split value in 6-bit chunks and
    * move to UTF8 string
    */
   for( iPos = iCharLength - 1, im = 0; iPos > 0; --iPos, im++ )
   {
      iUtf8 += (( iUCS & 0x0000003F ) | 0x80) << (im * 8);
      iUCS >>= 6;
   }

   /* UTF8 prefix, special case */
   iUtf8 += ( ( iUCS & 0x000000FF ) | iUTF8Prefix ) << (im * 8);

   return iUtf8;
}

int widechar_to_utf8(int wide_char) {
    if (wide_char <= 0x7F) {
        // Single-byte character
        return wide_char;
    } else if (wide_char <= 0x7FF) {
        // Two-byte character
        return ((wide_char & 0x1F) << 6) | (0x80 | (wide_char & 0x3F)) | 0xC000;
    } else if (wide_char <= 0xFFFF) {
        // Three-byte character
        return ((wide_char & 0x0F) << 12) | (0x800 | ((wide_char >> 6) & 0x3F) << 6) |
           (0x80 | (wide_char & 0x3F)) | 0xE00000;
    } else if (wide_char <= 0x10FFFF) {
        // Four-byte character
        return ((wide_char & 0x07) << 18) | (0x10000 | ((wide_char >> 12) & 0x3F) << 12) |
           (0x800 | ((wide_char >> 6) & 0x3F) << 6) | (0x80 | (wide_char & 0x3F)) | 0xF00000;
    } else {
        // Invalid wide character
        return -1;
    }
}

static unsigned int getFromStr( char ** ptr, PHB_CODEPAGE cdp, int bUtf8 )
{
   unsigned int iRes, i = 0;

   if( bUtf8 )
   {
      iRes = (unsigned int) utf8str_to_int( *ptr, &i );
      (*ptr) += i;
   } else
   {
      //iRes = widechar_to_utf8( hb_cdpGetU16( cdp, (HB_UCHAR)**ptr ) );
      iRes = wtoutf8( hb_cdpGetU16( cdp, (HB_UCHAR)**ptr ) );
      //hwg_writelog( NULL, "getf %s %u %u\r\n", cdp->id, (HB_UCHAR)**ptr, (unsigned int)iRes );
      (*ptr) ++;
   }
   return iRes;
}

HB_FUNC( HWPGT_OUT )
{
   PHWPGT pHWG = (PHWPGT) HB_PARHANDLE( 1 );
   char * pText = (char*) hb_parc( 2 ), *ptr = pText;
   int iPos = pHWG->iWidth * pHWG->iRow + pHWG->iCol;
   int iPosStart = iPos, iPosLast = pHWG->iWidth * pHWG->iRow + pHWG->iWidth;
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
   pHWG->iCol += (iPos - iPosStart);
   rect.right = pHWG->iCol - 1;
   rect = hwpgt_GetXYFromColRowRect( pHWG, rect );

   gtk_widget_queue_draw_area( pHWG->handle, rect.left, rect.top,
      rect.right - rect.left + 1, rect.bottom - rect.top + 1 );

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

   gtk_widget_queue_draw_area( pHWG->handle, rect.left, rect.top,
      rect.right - rect.left + 1, rect.bottom - rect.top + 1 );

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

   gtk_widget_queue_draw_area( pHWG->handle, rect.left, rect.top,
      rect.right - rect.left + 1, rect.bottom - rect.top + 1 );

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

   for( i = iTop; i <= iBottom; i ++ )
      for( j = iLeft, is = pHWG->iWidth * i; j <= iRight; j ++ )
      {
         pHWG->screenBuf[is + j].iColor = scrbuf[iPos].iColor;
         pHWG->screenBuf[is + j].iChar = scrbuf[iPos].iChar;
         iPos ++;
      }

   rect.top = iTop;
   rect.bottom = iBottom;
   rect.left = iLeft;
   rect.right = iRight;
   rect = hwpgt_GetXYFromColRowRect( pHWG, rect );

   gtk_widget_queue_draw_area( pHWG->handle, rect.left, rect.top,
      rect.right - rect.left + 1, rect.bottom - rect.top + 1 );

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

   gtk_widget_queue_draw_area( pHWG->handle, rect.left, rect.top,
      rect.right - rect.left + 1, rect.bottom - rect.top + 1 );

}

HB_FUNC( HWPGT_KEYEVENT )
{
   //PHWPGT pHWG = (PHWPGT) HB_PARHANDLE( 1 );
   //UINT message = (UINT) hb_parni(2);
   HB_LONG ulKeyRaw = (HB_LONG) hb_parnl( 3 );
   HB_LONG ulFlags = (HB_LONG) hb_parnl( 4 );

   HB_LONG ulKey = 0;

   //hwg_writelog( NULL, "_KeyConvert-1 %lu %x\r\n",ulKeyRaw,ulKeyRaw );
   switch( ulKeyRaw )
   {
      case GDK_Return:
         ulKey = HB_KX_ENTER;
         break;
      case GDK_Escape:
         ulKey = HB_KX_ESC;
         break;
      case GDK_Right:
         ulKey = HB_KX_RIGHT;
         break;
      case GDK_Left:
         ulKey = HB_KX_LEFT;
         break;
      case GDK_Up:
         ulKey = HB_KX_UP;
         break;
      case GDK_Down:
         ulKey = HB_KX_DOWN;
         break;
      case GDK_Home:
         ulKey = HB_KX_HOME;
         break;
      case GDK_End:
         ulKey = HB_KX_END;
         break;
      case GDK_Prior:
         ulKey = HB_KX_PGUP;
         break;
      case GDK_Next:
         ulKey = HB_KX_PGDN;
         break;
      case GDK_BackSpace:
         ulKey = HB_KX_BS;
         break;
      case GDK_Tab:
      case GDK_ISO_Left_Tab:
         ulKey = HB_KX_TAB;
         break;
      case GDK_Insert:
         ulKey = HB_KX_INS;
         break;
      case GDK_Delete:
         ulKey = HB_KX_DEL;
         break;
      case GDK_Clear:
         ulKey = HB_KX_CENTER;
         break;
      case GDK_Pause:
         ulKey = HB_KX_PAUSE;
         break;
      case GDK_F1:
         ulKey = HB_KX_F1;
         break;
      case GDK_F2:
         ulKey = HB_KX_F2;
         break;
      case GDK_F3:
         ulKey = HB_KX_F3;
         break;
      case GDK_F4:
         ulKey = HB_KX_F4;
         break;
      case GDK_F5:
         ulKey = HB_KX_F5;
         break;
      case GDK_F6:
         ulKey = HB_KX_F6;
         break;
      case GDK_F7:
         ulKey = HB_KX_F7;
         break;
      case GDK_F8:
         ulKey = HB_KX_F8;
         break;
      case GDK_F9:
         ulKey = HB_KX_F9;
         break;
      case GDK_F10:
         ulKey = HB_KX_F10;
         break;
      case GDK_F11:
         ulKey = HB_KX_F11;
         break;
      case GDK_F12:
         ulKey = HB_KX_F12;
         break;
      case GDK_KP_Add:
      case GDK_KP_Divide:
      case GDK_KP_Multiply:
      case GDK_KP_Subtract:
      case GDK_KP_Enter:
      case GDK_KP_0:
      case GDK_KP_1:
      case GDK_KP_2:
      case GDK_KP_3:
      case GDK_KP_4:
      case GDK_KP_5:
      case GDK_KP_6:
      case GDK_KP_7:
      case GDK_KP_8:
      case GDK_KP_9:
         ulKey = 48 + ulKeyRaw - GDK_KP_0;
         ulFlags |= HB_KF_KEYPAD;
         break;
   }
   if( ulKey != 0 )
      ulKey = HB_INKEY_NEW_KEY( ulKey, ulFlags );
   else if( ulKeyRaw <= 127 )
   {
      if( ulFlags & HB_KF_CTRL && ulKeyRaw >= 97 && ulKeyRaw <= 122 )
         ulKey = HB_INKEY_NEW_KEY( ulKeyRaw-32, ulFlags );
      else
         ulKey = HB_INKEY_NEW_KEY( ulKeyRaw, ulFlags );
   }
   else if( ulKeyRaw < 0xFE00 )
   {
      char utf8char[8], *ptr;
      char cdpchar[4];
      int iLen;
      HB_BOOL fUtf8 = ( strncmp( hb_cdpID(), "UTF", 3 ) == 0 );

      iLen = g_unichar_to_utf8( gdk_keyval_to_unicode( ulKeyRaw ), utf8char );
      utf8char[iLen] = '\0';
      //hwg_writelog( NULL, "_KeyConvert-2 %s %x\r\n",utf8char,ulKeyRaw  );
      if( fUtf8 )
      {
         int n = 0;
         HB_WCHAR wc = 0;
         ptr = utf8char;
         while( iLen )
         {
            if( ! hb_cdpUTF8ToU16NextChar( (unsigned char) *ptr, &n, &wc ) )
               break;
            if( n == 0 )
               break;
            ptr++;
            iLen--;
         }
         ulKey = HB_INKEY_NEW_UNICODEF( ((unsigned int) wc), ulFlags );
      }
      else
      {
         hb_cdpUTF8ToStr( hb_vmCDP(), utf8char, iLen, cdpchar, 3 );
         cdpchar[1] = '\0';
         ulKey = HB_INKEY_NEW_KEY( ((unsigned int) *cdpchar) & 0xff, ulFlags );
      }
   }
   //hwg_writelog( NULL, "Convert: %x %x\r\n",ulKeyRaw, ulKey );
   hb_retnl( ulKey );
}

HB_FUNC( HWPGT_MOUSEEVENT )
{
   PHWPGT pHWG = (PHWPGT) HB_PARHANDLE( 1 );
   unsigned int message = (unsigned int) hb_parni(2);
   //WPARAM wParam = (WPARAM) hb_parnl( 3 );
   unsigned long int lParam = (unsigned long int) hb_parnl( 4 );
   int keyCode = 0;
   POINT xy, colrow;

   xy.x = lParam & 0xFFFF;
   xy.y = ( lParam >> 16 ) & 0xFFFF;

   pHWG->ExactMousePos.y = xy.y;
   pHWG->ExactMousePos.x = xy.x;

   colrow = hwpgt_GetColRowFromXY( pHWG, xy.x, xy.y );
   hwpgt_SetMousePos( pHWG, colrow.y, colrow.x );
   hwg_writelog( NULL, "%d : %d %d | %d %d\r\n", message, xy.y, xy.x, colrow.y, colrow.x );

   switch( message )
   {
      case WM_LBUTTONDBLCLK:
         keyCode = K_LDBLCLK;
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

      //case WM_MOUSEWHEEL:
      //   keyCode = ( SHORT ) ( HIWORD( wParam ) > 0 ? K_MWFORWARD : K_MWBACKWARD );
      //   break;
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
         hb_retni( gdk_screen_width() / pHWG->PTEXTSIZE.x );
         break;

      case HB_GTI_DESKTOPROWS:
         hb_retni( gdk_screen_height() / pHWG->PTEXTSIZE.y );
         break;

      case HB_GTI_PALETTE:
         pItem = hb_itemNew( NULL );
         hb_arrayNew( pItem, 16 );
         for( i = 0; i < 16; i++ )
            hb_arraySetNL( pItem, i + 1, pHWG->COLORS[ i ] );
         hb_itemRelease( hb_itemReturn( pItem ) );

         if( HB_ISARRAY( 3 ) )
         {
            pItem = hb_param( 3, HB_IT_ARRAY );
            if( hb_arrayLen( pItem ) == 16 )
            {
               for( i = 0; i < 16; i++ )
                  pHWG->COLORS[ i ] = hb_arrayGetNL( pItem, i + 1 );

               if( pHWG->handle )
               {
                  GtkAllocation alloc;

                  gtk_widget_get_allocation( pHWG->handle, &alloc );
                  gtk_widget_queue_draw_area( pHWG->handle, 0, 0, alloc.width, alloc.height );
               }
            }
         }
         break;

      default:
         hb_ret();
   }

}
