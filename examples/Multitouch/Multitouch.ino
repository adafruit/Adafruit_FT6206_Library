/***************************************************
  This is a multitouch example for the Adafruit ILI9341
  captouch shield
  ----> http://www.adafruit.com/products/1947

  Check out the link above for tutorials and wiring diagrams

  Written by David Williams, based on original by Lady Ada.
  MIT license, all text above must be included in any redistribution
 ****************************************************/

#include <Adafruit_GFX.h>    // Core graphics library
#include <SPI.h>       // this is needed for display
#include <Adafruit_ILI9341.h>
#include <Wire.h>      // this is needed for FT6206
#include <Adafruit_FT6206.h>

// The FT6206 uses hardware I2C (SCL/SDA)
Adafruit_FT6206 ctp = Adafruit_FT6206();

// Display Section
// The display also uses hardware SPI, plus #9 & #10
#define TFT_CS 10
#define TFT_DC 9
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
#define XSIZE 240
#define YSIZE 320

// Touch constants
#define TOUCH_TAP_TIME 300
#define TOUCH_DOUBLE_TAP_TIME 500
#define TOUCH_ZOOM_MINIMUM 10
#define TOUCH_ZOOM_MINIMUM_SQ ( TOUCH_ZOOM_MINIMUM * TOUCH_ZOOM_MINIMUM )
#define TOUCH_TAP_MAXIMUM_DISTANCE 10
#define TOUCH_TAP_MAXIMUM_DISTANCE_SQ TOUCH_TAP_MAXIMUM_DISTANCE * TOUCH_TAP_MAXIMUM_DISTANCE
#define TOUCH_DOUBLE_TAP_MAXIMUM_DISTANCE 20
#define TOUCH_DOUBLE_TAP_MAXIMUM_DISTANCE_SQ TOUCH_DOUBLE_TAP_MAXIMUM_DISTANCE * TOUCH_DOUBLE_TAP_MAXIMUM_DISTANCE
#define TOUCH_SCROLL_COSTHETA 0.8
#define TOUCH_SCROLL_MINIMUM_DISTANCE 10

// Touch glyphs
#define TOUCH_RADIUS 30
#define TOUCH_TRIANGLE_TOP 40
#define TOUCH_TRIANGLE_X 32 // 40 * cos( 30 ) = 32
#define TOUCH_TRIANGLE_Y 20 // 40 * sin( 30 ) = 20


// Constants and variables for rendering the main Object
// Rendering constants for the main display
#define OBJECT_SIZE_MINIMUM XSIZE / 6
#define OBJECT_SIZE_MAXIMUM XSIZE / 2

#define COLOR_COUNT 4

int Colors[ ] = { ILI9341_BLUE, ILI9341_RED, ILI9341_ORANGE, ILI9341_PINK };
int Touch = ILI9341_DARKGREY;
int Background = ILI9341_BLACK;

#define OBJECT_TYPE_COUNT 3
#define OBJECT_SCROLL_THRESHOLD 30

int ObjectX = XSIZE / 2;
int ObjectY = YSIZE / 2;
int ObjectSize = XSIZE / 4;
int ObjectColor = 0;
uint8_t ObjectType = 0;
int ObjectZoom = 0;
int ObjectColorScroll = 0;

// Touch event code records Dragging information here
bool Dragging = false;
uint8_t DraggingIndex = 0;
int DraggingOffsetX = 0;
int DraggingOffsetY = 0;

// Helper point class with several vector functions
// elements are set to -1 to signify no touch ("released" state)
class CtpPoint {
public:
  int x;
  int y;
  CtpPoint() { release(); }
  CtpPoint( int x_, int y_ ): x( x_ ), y( y_ ) {}
  void set( int x_, int y_ ) { x = x_; y = y_; }
  void release() { x = -1; y = -1; }
  bool isReleased() { return ( ( x == -1 ) && ( y == -1 ) ); }
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
  int dotProduct( CtpPoint p ) {
    return x * p.x + y * p.y;
  }
  CtpPoint mean( CtpPoint p ) {
    return CtpPoint( (x + p.x )/ 2, ( y + p.y ) / 2 );
  }
};

// Class for recording touch events
// Ordered by touch ID, not location in the touch array
// I.e. if the first (0) slot of the touch array occupies a touch event from touch id 1, it would
// transfer information into CtpTouches[ 1 ], not CtpTouches[ 0 ]
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


// Draw our main onscreen object - one of three different shapes,
// in a specified color
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

// To Erase the shape, just draw it again in the background color
void eraseObject() {
  drawObject();
}

void setup(void) {

  delay(2000);

  //while (!Serial);     // used for leonardo debugging

  Serial.begin(115200);
  Serial.println(F("Multitouch!"));

  tft.begin();

  ctp.begin(40);   // pass in 'sensitivity' coefficient

  Serial.println("Capacitive touchscreen started");

  tft.fillScreen( Background );

  // Set the touch structure up,
  for ( uint8_t i = 0; i < 2; i++) {
    CtpTouch* t = &CtpTouches[ i ];
    t->tap.release();
    t->original.release();
    t->current.release();
    t->previous.release();
    t->touchDownTime = 0;
    t->touchUpTime = 0;
  }

  // Draw the object for the first time
  drawObject( Colors[ ObjectColor ] );
}

// Event called when a touch is detected
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

// Event called when a touch point is moving
void touchMove( int x, int y, int i ) {
  //Serial.println( "Touch Move" );
  if ( Dragging ) {
    Serial.println( "  Object Move" );
  }
}

// Event called when a finger is lifted ioff
// Does quite a lot:
// - changes color if there's been a scroll
// - if there was dragging, move the object
// - if there was zooming, resize the object

void touchUp( int x, int y, int i ) {
  //Serial.println( "Touch Up" );
        // In case there was a Scroll
  if ( ObjectColorScroll != 0 ) {
    Serial.print( "Color Shift" );
    Serial.print( ObjectColorScroll );
    Serial.print( " Color " );
    Serial.println( ObjectColor );
    if ( ObjectColorScroll > OBJECT_SCROLL_THRESHOLD )
      ObjectColor = ( ObjectColor + 1 ) % COLOR_COUNT;
    if ( ObjectColorScroll < -OBJECT_SCROLL_THRESHOLD )
      ObjectColor = ( ObjectColor - 1 ) % COLOR_COUNT;
    ObjectColorScroll = 0;
  }

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

// Event called when someone taps the screen
void touchTap( int x, int y, int i ) {
  Serial.print( "Tap" );
}

// Event called when someone double taps the screen
void touchDoubleTap( int x, int y, int i ) {
  Serial.println( "DoubleTap" );
  eraseObject();
  ObjectType = ( ObjectType + 1 ) % OBJECT_TYPE_COUNT;
  drawObject( Colors[ ObjectColor ] );
}


// Called when a Pinch or Zoom Gesture is detected
void TouchZoom( int d ) {
  //Serial.print( "ZOOOM " );
  //Serial.println( d );
  if ( Dragging )
    ObjectZoom = d;
}

// Called when a Scroll is detected, we'll just attend to the
// scrolls in the X direction
void TouchScroll( int dx, int dy ) {
  Serial.print( "SCROLL " );
  Serial.print( dx );
  Serial.print( ", " );
  Serial.println( dy );
  ObjectColorScroll = dx;
}

void loop() {
  // Do the touch processing
  touch();

  // draw the object, for fun, again
  drawObject( Colors[ ObjectColor ] );
}

// The meat of the touch() processing
void touch() {

  uint8_t n, id[ 2 ];
  uint16_t x[ 2 ], y[ 2 ];

  // Use the new readMultiData() function
  n = ctp.readMultiData( &id[ 0 ], &x[ 0 ], &y[ 0 ], &id[ 1 ], &x[ 1 ], &y[ 1 ] );

  // Extract and update the touch information
  if ( n > 0 ){
    for ( uint8_t i = 0; i < n; i++) {
      int idi = id[ i ];
      int xi = map(x[ i ], 0, 240, 240, 0);
      int yi = map(y[ i ], 0, 320, 320, 0);
      //Serial.print( "T" ); Serial.print( i ); Serial.print( " " ); Serial.print( idi ); Serial.print( " " ); Serial.print( xi ); Serial.print( "," ); Serial.println( yi );
      // Save the touch data away
      if ( idi == 0 ) {
         CtpTouches[ 0 ].current.set( xi, yi );
         if ( n == 1 ) {
           // if this is the only touch, release the other
           CtpTouches[ 1 ].current.release( );
         }
      }
      else {
         CtpTouches[ 1 ].current.set( xi, yi );
         if ( n == 1 ) {
           // if this is the only touch, release the other
           CtpTouches[ 0 ].current.release();
         }
      }
    }
  }

  // If there were no touches, release all the current position records
  if ( n == 0 ) {
    CtpTouches[ 0 ].current.release();
    CtpTouches[ 1 ].current.release();
  }

  // Two Finger Gestures
  CtpTouch *t0 = &CtpTouches[ 0 ];
  CtpTouch *t1 = &CtpTouches[ 1 ];
  // If there are 2 touches, and the position has changed
  if ( n == 2 && ( t0->current != t0->previous || t1->current != t1->previous  ) ) {
    // Pinch Zoom
    // Calculate the distance between the original mouse positions (when fingers were placed)
    int dOriginalSq = t0->original.distanceSq( t1->original );
    // Calculate the interdigit distance now
    int dCurrentSq = t0->current.distanceSq( t1->current );
    // WOrk out the difference
    int dZoomSq = dCurrentSq - dOriginalSq;
    // If the change in positions excedes a threshold, call it a Zoom, sending the change in
    // distance between finger tips
    if ( abs( dZoomSq ) > TOUCH_ZOOM_MINIMUM_SQ ) {
      TouchZoom( sqrt( dCurrentSq ) - sqrt( dOriginalSq ) );
    }

    // Scroll
    // Find the vectors from each touch's original to current positions
    CtpPoint d0 = t0->current-t0->original;
    CtpPoint d1 = t1->current-t1->original;
    int d0l = d0.length();
    int d1l = d1.length();
    // See if they're in the same direction
    float dp = (float)d0.dotProduct( d1 ) / ( d0l * d1l );
    // If so, might be a Scroll
    if ( dp > 0.8 ) {
      // Work out the mean direction
      CtpPoint m = d0.mean( d1 );
      int ml = m.length();
      // If the mean vector is long enough, notify the scroll, sending the current scroll vector
      if ( ml > TOUCH_SCROLL_MINIMUM_DISTANCE )
        TouchScroll( m.x, m.y );
    }
    Serial.print( "Scroll cos theta " );
    Serial.println( dp );

  }

  // Now just the regular touch events
  for ( uint8_t i = 0; i < 2; i++) {
    CtpTouch* t = &CtpTouches[ i ];

    // Check for touchDown (previously was released, now not)
    if ( !t->current.isReleased() && t->previous.isReleased() ) {
      touchDown( t->current.x, t->current.y, i );
      t->original = t->current;
      t->touchDownTime = millis();
    }
    else {
      // Check for touchUp (previously, was not released, now is)
      if ( t->current.isReleased() && !t->previous.isReleased() ) {
        touchUp( t->previous.x, t->previous.y, i );

        // Check for a Tap event, need to know the distance from the original
        // touchDown location to the touchUp location
        int dSq = t->original.distanceSq( t->previous );
        // Serial.print( "Tap Distance Sq " );
        // Serial.println( dSq );
        // For a legit Tap event, the down-up distance has to be small
        if ( dSq < TOUCH_TAP_MAXIMUM_DISTANCE_SQ ) {
          // Also a legit tap event has to occur within a narrow time window
          unsigned long up = millis();
          if ( ( up - t->touchDownTime ) < TOUCH_TAP_TIME ) {
            // OK.  So that was a tap.
            touchTap( t->current.x, t->current.y, i );
            // Was that a double tap?
            // The second tap needs to occur soon after the firt
            if ( ( up - t->touchUpTime ) < TOUCH_DOUBLE_TAP_TIME ) {
              // And within a short distance from the first
              int dCSq = t->tap.distanceSq( t->previous );
              // Serial.print( "Double Tap Distance Sq " );
              // Serial.println( dCSq );
              if ( dCSq < TOUCH_DOUBLE_TAP_MAXIMUM_DISTANCE_SQ ) {
                // OK signal teh double tap
                touchDoubleTap( t->current.x, t->current.y, i );
              }
              // Prevent an ongoing sequence of double taps on each subsequent tap
              up = 0;
            }
            // Save the up time
            t->touchUpTime = up;
          }
          // Save last tap location for a potential future double tap
          t->tap = t->previous;
        }
      } else {
        // Wasn't a TouchDown or TouchUp, was it a touchMove()?
        if ( !t->current.isReleased() && !t->previous.isReleased() ) {
          touchMove( t->current.x, t->current.y, i );
        }
      }
    }

    // Drawing the touch
    // This is just for touch feedback.  Normally you wouldn't render the
    // touch points at all.
    if ( !t->previous.isReleased() && t->previous != t->current ) {
      int x = t->previous.x;
      int y = t->previous.y;
      if ( i == 0 )
        tft.drawCircle( x, y, TOUCH_RADIUS, Background );
      else {
        tft.drawTriangle( x, y - TOUCH_TRIANGLE_TOP, x - TOUCH_TRIANGLE_X, y + TOUCH_TRIANGLE_Y, x + TOUCH_TRIANGLE_X, y + TOUCH_TRIANGLE_Y, Background  );
      }
    }
    if ( !t->current.isReleased() && t->previous != t->current ) {
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
