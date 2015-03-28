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

#define TOUCH_TAP_TIME 300
#define TOUCH_DOUBLE_TAP_TIME 500
#define TOUCH_ZOOM_MINIMUM 10
#define TOUCH_ZOOM_MINIMUM_SQ ( TOUCH_ZOOM_MINIMUM * TOUCH_ZOOM_MINIMUM )
#define TOUCH_TAP_MAXIMUM_DISTANCE 10
#define TOUCH_TAP_MAXIMUM_DISTANCE_SQ TOUCH_TAP_MAXIMUM_DISTANCE * TOUCH_TAP_MAXIMUM_DISTANCE
#define TOUCH_DOUBLE_TAP_MAXIMUM_DISTANCE 20
#define TOUCH_DOUBLE_TAP_MAXIMUM_DISTANCE_SQ TOUCH_DOUBLE_TAP_MAXIMUM_DISTANCE * TOUCH_DOUBLE_TAP_MAXIMUM_DISTANCE

#define XSIZE 240
#define YSIZE 320
#define COLOR_COUNT 3

#define TOUCH_RADIUS 30
#define TOUCH_TRIANGLE_TOP 40
#define TOUCH_TRIANGLE_X 32 // 40 * cos( 30 ) = 32
#define TOUCH_TRIANGLE_Y 20 // 40 * sin( 30 ) = 20

int Colors[ ] = { ILI9341_BLUE, ILI9341_RED };
int Touch = ILI9341_DARKGREY;
int Background = ILI9341_BLACK;

// Going to have a thing
#define OBJECT_SIZE_MINIMUM XSIZE / 6
#define OBJECT_SIZE_MAXIMUM XSIZE / 2

#define OBJECT_TYPE_COUNT 3

int ObjectX = XSIZE / 2;
int ObjectY = YSIZE / 2;
int ObjectSize = XSIZE / 4;
uint8_t ObjectColor = 0;
uint8_t ObjectType = 0;
int ObjectZoom = 0;

class CtpPoint {
public:
  int x;
  int y;
  CtpPoint() { clear(); }
  CtpPoint( int x_, int y_ ): x( x_ ), y( y_ ) {}
  void set( int x_, int y_ ) { x = x_; y = y_; }
  void clear() { x = -1; y = -1; }
  bool isClear() { return ( ( x == -1 ) && ( y == -1 ) ); }
  CtpPoint operator-(CtpPoint p ) {
    return CtpPoint( x - p.x, y - p.y );
  }
  bool operator==(CtpPoint p) {
    return  ( ( p.x == x ) && ( p.y == y ) );
  }
  bool operator!=(CtpPoint p) {
    return  ( ( p.x != x ) || ( p.y != y ) );
  }
  int lengthSq() {
    return x * x + y * y;
  }
  int length() {
    return sqrt( lengthSq() );
  }
  int distanceSq( CtpPoint p ) {
    int dx = p.x - x;
    int dy = p.y - y;
    return ( dx * dx ) + ( dy * dy );
  }
  int distance( CtpPoint p ) {
    return sqrt( distanceSq( p ) );
  }
};

class CtpTouch {
public:
  CtpTouch() {}
  CtpPoint tap;
  CtpPoint original;
  CtpPoint current;
  CtpPoint previous;
  unsigned long touchDownTime;
  unsigned long touchUpTime;
} CtpTouches[ 2 ];


void drawObject( int color = Background ) {
  switch ( ObjectType ) {
    case 0:
      tft.drawCircle( ObjectX, ObjectY, ObjectSize, color );
      tft.drawCircle( ObjectX, ObjectY, ObjectSize - 4, color );
      break;
    case 1:
      tft.drawRect( ObjectX - ObjectSize, ObjectY - ObjectSize, 2 * ObjectSize, 2 * ObjectSize, color );
      tft.drawRect( ObjectX - ObjectSize + 4, ObjectY - ObjectSize + 4, 2 * ObjectSize - 8, 2 * ObjectSize - 8, color );
      break;
    case 2:
      tft.drawTriangle( ObjectX, ObjectY - 3 * ObjectSize / 2, ObjectX - 21 * ObjectSize / 16, ObjectY + 3 * ObjectSize / 4 , ObjectX + 21 * ObjectSize / 16, ObjectY + 3 * ObjectSize / 4, color );
      tft.drawTriangle( ObjectX, ObjectY - 3 * ObjectSize / 2 + 5,
                        ObjectX - 21 * ObjectSize / 16 + 5, ObjectY + 3 * ObjectSize / 4 -3,
                        ObjectX + 21 * ObjectSize / 16 -5, ObjectY + 3 * ObjectSize / 4 -3, color );
      break;
  }
}

void eraseObject() {
  drawObject();
}

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
    CtpTouch* t = &CtpTouches[ i ];
    t->tap.clear();
    t->original.clear();
    t->current.clear();
    t->previous.clear();
    t->touchDownTime = 0;
    t->touchUpTime = 0;
  }

  drawObject( Colors[ ObjectColor ] );
}

bool Dragging = false;
uint8_t DraggingIndex = 0;
int DraggingOffsetX = 0;
int DraggingOffsetY = 0;

void touchDown( int x, int y, int i ) {
  int dx = x - ObjectX;
  int dy = y - ObjectY;
  //Serial.println( "Touch Down" );
  int d2 = ( dx * dx + dy * dy );
  if ( d2 < ObjectSize * ObjectSize && Dragging == false ) {
    Serial.println( "  Object Hit!" );
    Serial.print( "    dx " ); Serial.print( dx ); Serial.print( " dy" ); Serial.println( dy );

    Dragging = true;
    DraggingIndex = i;
    DraggingOffsetX = dx;
    DraggingOffsetY = dy;
  }
}

void touchMove( int x, int y, int i ) {
  //Serial.println( "Touch Move" );
  if ( Dragging ) {
    Serial.println( "  Object Move" );
  }
}

void touchUp( int x, int y, int i ) {
  //Serial.println( "Touch Up" );
  if ( Dragging && DraggingIndex == i) {
    Serial.println( "  Object Release" );
    Dragging = false;

    int newX = x - DraggingOffsetX;
    int newY = y - DraggingOffsetY;
    if ( ObjectX != newX || ObjectY != newY )   {
      eraseObject();

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

      // In case there was a zoom
      ObjectSize += ObjectZoom;
      if ( ObjectSize < OBJECT_SIZE_MINIMUM )
        ObjectSize = OBJECT_SIZE_MINIMUM;
      if ( ObjectSize > OBJECT_SIZE_MAXIMUM )
        ObjectSize = OBJECT_SIZE_MAXIMUM;
      ObjectZoom = 0;

      drawObject( Colors[ ObjectColor ] );
    }

  }
}

void touchTap( int x, int y, int i ) {
  Serial.print( "Tap" );
}

void touchDoubleTap( int x, int y, int i ) {
  Serial.println( "DoubleTap" );
  eraseObject();
  ObjectType = ( ObjectType + 1 ) % OBJECT_TYPE_COUNT;
  drawObject( Colors[ ObjectColor ] );
}

void TouchZoom( int d ) {
  Serial.print( "ZOOOM " );
  Serial.println( d );
  if ( Dragging )
    ObjectZoom = d;
}

void loop() {

  touch();

  drawObject( Colors[ ObjectColor ] );
}


void touch() {

  uint8_t n, id[ 2 ];
  uint16_t x[ 2 ], y[ 2 ];

  n = ctp.readMultiData( &id[ 0 ], &x[ 0 ], &y[ 0 ], &id[ 1 ], &x[ 1 ], &y[ 1 ] );

  // Extract and update the touch information
  if ( n > 0 ){
    for ( uint8_t i = 0; i < n; i++) {
      int idi = id[ i ];
      int xi = map(x[ i ], 0, 240, 240, 0);
      int yi = map(y[ i ], 0, 320, 320, 0);
      //Serial.print( "T" ); Serial.print( i ); Serial.print( " " ); Serial.print( idi ); Serial.print( " " ); Serial.print( xi ); Serial.print( "," ); Serial.println( yi );
      if ( idi == 0 ) {
         CtpTouches[ 0 ].current.set( xi, yi );
         if ( n == 1 ) {
           CtpTouches[ 1 ].current.clear( );
         }
      }
      else {
         CtpTouches[ 1 ].current.set( xi, yi );
         if ( n == 1 ) {
           CtpTouches[ 0 ].current.clear();
         }
      }
    }
  }

  if ( n == 0 ) {
    CtpTouches[ 0 ].current.clear();
    CtpTouches[ 1 ].current.clear();
  }

  // Two Finger Gestures
  CtpTouch *t0 = &CtpTouches[ 0 ];
  CtpTouch *t1 = &CtpTouches[ 1 ];
  if ( n == 2 && ( t0->current != t0->previous || t1->current != t1->previous  ) ) {
    // Pinch Zoom
    int dOriginalSq = t0->original.distanceSq( t1->original );
    int dCurrentSq = t0->current.distanceSq( t1->current );
    int dZoomSq = dCurrentSq - dOriginalSq;
    if ( abs( dZoomSq ) > TOUCH_ZOOM_MINIMUM_SQ ) {
      TouchZoom( sqrt( dCurrentSq ) - sqrt( dOriginalSq ) );
    }

    // Scroll
    CtpPoint d0 = t0->current-t0->previous;
    CtpPoint d1 = t0->current-t0->previous;

  }

  for ( uint8_t i = 0; i < 2; i++) {
    CtpTouch* t = &CtpTouches[ i ];

    // Create single touch events
    if ( !t->current.isClear() && t->previous.isClear() ) {
      touchDown( t->current.x, t->current.y, i );
      t->original = t->current;
      t->touchDownTime = millis();
    }
    else {
      if ( t->current.isClear() && !t->previous.isClear() ) {
        touchUp( t->previous.x, t->previous.y, i );
        int dSq = t->original.distanceSq( t->previous );
        Serial.print( "Tap Distance Sq " );
        Serial.println( dSq );
        if ( dSq < TOUCH_TAP_MAXIMUM_DISTANCE_SQ ) {
          unsigned long up = millis();
          if ( ( up - t->touchDownTime ) < TOUCH_TAP_TIME ) {
            touchTap( t->current.x, t->current.y, i );
            if ( ( up - t->touchUpTime ) < TOUCH_DOUBLE_TAP_TIME ) {
              int dCSq = t->tap.distanceSq( t->previous );
        Serial.print( "Double Tap Distance Sq " );
        Serial.println( dCSq );
              if ( dCSq < TOUCH_DOUBLE_TAP_MAXIMUM_DISTANCE_SQ )
                touchDoubleTap( t->current.x, t->current.y, i );
              up = 0;
            }
            t->touchUpTime = up;
          }
          t->tap = t->previous;
        }
      } else {
        if ( !t->current.isClear() && !t->previous.isClear() ) {
          touchMove( t->current.x, t->current.y, i );
        }
      }
    }

    // Drawing the touch
    if ( !t->previous.isClear() && t->previous != t->current ) {
      int x = t->previous.x;
      int y = t->previous.y;
      if ( i == 0 )
        tft.drawCircle( x, y, TOUCH_RADIUS, Background );
      else {
        tft.drawTriangle( x, y - TOUCH_TRIANGLE_TOP, x - TOUCH_TRIANGLE_X, y + TOUCH_TRIANGLE_Y, x + TOUCH_TRIANGLE_X, y + TOUCH_TRIANGLE_Y, Background  );
      }
    }
    if ( !t->current.isClear() && t->previous != t->current ) {
      int x = t->current.x;
      int y = t->current.y;
      if ( i == 0 )
        tft.drawCircle( x, y, TOUCH_RADIUS, Touch );
      else {
        tft.drawTriangle( x, y - TOUCH_TRIANGLE_TOP, x - TOUCH_TRIANGLE_X, y + TOUCH_TRIANGLE_Y, x + TOUCH_TRIANGLE_X, y + TOUCH_TRIANGLE_Y, Touch  );
      }
    }

    // What is new will be old
    t->previous = t->current;
  }

}
