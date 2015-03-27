/***************************************************
  This is a multitouch example for the Adafruit ILI9341
  captouch shield
  ----> http://www.adafruit.com/products/1947

  Check out the links above for our tutorials and wiring diagrams

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by David Williams
  MIT license, all text above must be included in any redistribution
 ****************************************************/

#include <Adafruit_GFX.h>    // Core graphics library
#include <SPI.h>       // this is needed for display
#include <Adafruit_ILI9341.h>
#include <Wire.h>      // this is needed for FT6206
#include <Adafruit_FT6206.h>

// The FT6206 uses hardware I2C (SCL/SDA)
Adafruit_FT6206 ctp = Adafruit_FT6206();

// The display also uses hardware SPI, plus #9 & #10
#define TFT_CS 10
#define TFT_DC 9
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

#define XSIZE 240
#define YSIZE 320
#define COLOR_COUNT 3

#define TOUCH_RADIUS 30
#define TOUCH_TRIANGLE_TOP 40
#define TOUCH_TRIANGLE_X 32 // 40 * cos( 30 ) = 32
#define TOUCH_TRIANGLE_Y 20 // 40 * sin( 30 ) = 20

int Colors[ ] = { ILI9341_DARKGREEN, ILI9341_BLUE, ILI9341_RED };
int Touch = ILI9341_DARKGREY;
int Background = ILI9341_BLACK;

// Going to have a thing
#define OBJECT_SIZE_MINIMUM XSIZE / 32
#define OBJECT_SIZE_MAXIMUM XSIZE / 2

int ObjectX = XSIZE / 2;
int ObjectY = YSIZE / 2;
int ObjectSize = XSIZE / 8;
uint8_t ObjectColor = 0;

int XOriginal[ 2 ];
int YOriginal[ 2 ];
int XCurrent[ 2 ];
int YCurrent[ 2 ];
int XPrevious[ 2 ];
int YPrevious[ 2 ];

void setup(void) {
  while (!Serial);     // used for leonardo debugging

  Serial.begin(115200);
  Serial.println(F("Multitouch!"));

  delay(500);

  tft.begin();

  ctp.begin(40);   // pass in 'sensitivity' coefficient

  Serial.println("Capacitive touchscreen started");

  tft.fillScreen( Background );

  for ( uint8_t i = 0; i < 2; i++) {
    XCurrent[ i ] = -1;
    YCurrent[ i ] = -1;
    XPrevious[ i ] = -1;
    YPrevious[ i ] = -1;
  }
}

bool Dragging = false;
int DraggingOffsetX = 0;
int DraggingOffsetY = 0;

void touchDown( int x, int y ) {
  int dx = x - ObjectX;
  int dy = y - ObjectY;
  Serial.println( "Touch Down" );
  int d2 = ( dx * dx + dy * dy );
  if ( d2 < ObjectSize * ObjectSize ) {
    Serial.println( "  Object Hit!" );
    Serial.print( "    dx " ); Serial.print( dx ); Serial.print( " dy" ); Serial.println( dy );

    Dragging = true;
    DraggingOffsetX = dx;
    DraggingOffsetY = dy;
  }
}

void touchMove( int x, int y ) {
  Serial.println( "Touch Move" );
  if ( Dragging ) {
    Serial.println( "  Object Move" );
  }
}

void touchUp( int x, int y ) {
  Serial.println( "Touch Up" );
  if ( Dragging ) {
    Serial.println( "  Object Release" );
    Dragging = false;

    int newX = x - DraggingOffsetX;
    int newY = y - DraggingOffsetY;
    if ( ObjectX != newX || ObjectY != newY )   {
      tft.fillCircle( ObjectX, ObjectY, ObjectSize, Background );

      Serial.print( "    x " ); Serial.print( x ); Serial.print( " OY " ); Serial.println( y );
      Serial.print( "    Ox " ); Serial.print( ObjectX ); Serial.print( " OY " ); Serial.println( ObjectY );
      ObjectX = x - DraggingOffsetX;
      ObjectY = y - DraggingOffsetY;

      Serial.print( "    dx " ); Serial.print( DraggingOffsetX ); Serial.print( " dy" ); Serial.println( DraggingOffsetY );
      Serial.print( "    Ox " ); Serial.print( ObjectX ); Serial.print( " OY " ); Serial.println( ObjectY );

      if ( ObjectX < 0 ) ObjectX = 0;
      if ( ObjectY < 0 ) ObjectY = 0;
      if ( ObjectX >= XSIZE ) ObjectX = XSIZE - 1;
      if ( ObjectY >= YSIZE ) ObjectY = YSIZE - 1;

      tft.fillCircle( ObjectX, ObjectY, ObjectSize, Background );
    }

  }
}

void loop() {

  touch();

  tft.fillCircle( ObjectX, ObjectY, ObjectSize, Colors[ ObjectColor ] );

}

void touch() {

  uint8_t n, id[ 2 ];
  uint16_t x[ 2 ], y[ 2 ];

  n = ctp.readMultiData( &id[ 0 ], &x[ 0 ], &y[ 0 ], &id[ 1 ], &x[ 1 ], &y[ 1 ] );

  if ( n > 0 ){
    for ( uint8_t i = 0; i < n; i++) {
      int idi = id[ i ];
      int xi = map(x[ i ], 0, 240, 240, 0);
      int yi = map(y[ i ], 0, 320, 320, 0);
      //Serial.print( "T" ); Serial.print( i ); Serial.print( " " ); Serial.print( idi ); Serial.print( " " ); Serial.print( xi ); Serial.print( "," ); Serial.println( yi );
      if ( idi == 0 ) {
         XCurrent[ 0 ] = xi;
         YCurrent[ 0 ] = yi;
         if ( n == 1 ) {
           XCurrent[ 1 ] = -1;
           YCurrent[ 1 ] = -1;
         }
      }
      else {
         XCurrent[ 1 ] = xi;
         YCurrent[ 1 ] = yi;
         if ( n == 1 ) {
           XCurrent[ 0 ] = -1;
           YCurrent[ 0 ] = -1;
         }
      }
    }
  }

  if ( n == 0 ) {
    XCurrent[ 0 ] = -1;
    YCurrent[ 0 ] = -1;
    XCurrent[ 1 ] = -1;
    YCurrent[ 1 ] = -1;
  }

  if ( XCurrent[ 0 ] != -1 && XPrevious[ 0 ] == -1 ) {
    touchDown( XCurrent[ 0 ], YCurrent[ 0 ] );
    XOriginal[ 0 ] = XCurrent[ 0 ];
    YOriginal[ 0 ] = YCurrent[ 0 ];
  }
  else {
    if ( XCurrent[ 0 ] == -1 && XPrevious[ 0 ] != -1 ) {
      touchUp( XPrevious[ 0 ], YPrevious[ 0 ] );
    } else {
      if ( XCurrent[ 0 ] != XPrevious[ 0 ] || YCurrent[ 0 ] != YPrevious[ 1 ] )
        touchMove( XCurrent[ 0 ], YCurrent[ 0 ] );
    }
  }

  for ( uint8_t i = 0; i < 2; i++) {
    int xp = XPrevious[ i ];
    int yp = YPrevious[ i ];
    int xc = XCurrent[ i ];
    int yc = YCurrent[ i ];
    if ( xp != -1 && ( xp != xc || yp != yc ) ) {
      if ( i == 0 )
        tft.drawCircle( xp, yp, TOUCH_RADIUS, Background );
      else
        tft.drawTriangle( xp, yp - TOUCH_RADIUS, xp - TOUCH_TRIANGLE_X, yp + TOUCH_TRIANGLE_Y, xp + TOUCH_TRIANGLE_X, yp + TOUCH_TRIANGLE_Y, Background  );
    }
    if ( xc != -1 && ( xp != xc || yp != yc ) ) {
      if ( i == 0 )
        tft.drawCircle( xc, yc, TOUCH_RADIUS, Touch );
      else
        tft.drawTriangle( xc, yc - TOUCH_RADIUS, xc - TOUCH_TRIANGLE_X, yc + TOUCH_TRIANGLE_Y, xc + TOUCH_TRIANGLE_X, yc + TOUCH_TRIANGLE_Y, Touch  );
    }

    XPrevious[ i ] = XCurrent[ i ];
    YPrevious[ i ] = YCurrent[ i ];
  }

}
