/*******************************************************************************
*
* E M B E D D E D   W I Z A R D   P R O J E C T
*
*                                                Copyright (c) TARA Systems GmbH
*                                    written by Paul Banach and Manfred Schweyer
*
********************************************************************************
*
* This software is delivered "as is" and shows the usage of other software
* components. It is provided as an example software which is intended to be
* modified and extended according to particular requirements.
*
* TARA Systems hereby disclaims all warranties and conditions with regard to the
* software, including all implied warranties and conditions of merchantability
* and non-infringement of any third party IPR or other rights which may result
* from the use or the inability to use the software.
*
********************************************************************************
*
* DESCRIPTION:
*   This file is part of the interface (glue layer) between an Embedded Wizard
*   generated UI application and the board support package (BSP) of a dedicated
*   target.
*   This template is responsible to initialize the touch driver of the display
*   hardware and to receive the touch events for the UI application.
*
*   Important: This file is intended to be used as a template. Please adapt the
*   implementation and declarations according your particular hardware.
*   In order to provide a starting point for you, the current implementation
*   is prepared for using an embedded Linux system.
*   The touch positions are read in a separate thread from the touch input device
*   (e.g. /dev/input/event0).
*
*******************************************************************************/

#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>

#include <linux/input.h>

#include "ewrte.h"

#include "ew_bsp_touch.h"


#define DEFAULT_TOUCH_DEVICE  "/dev/input/event0"

#define NO_OF_FINGERS                   10
#define DELTA_TOUCH                     16
#define DELTA_TIME                      500

/* additional touch flag to indicate idle state */
#define EW_BSP_TOUCH_IDLE               0

/* additional touch flag to indicate hold state */
#define EW_BSP_TOUCH_HOLD               4

/* structure to store internal touch information for one finger */
typedef struct
{
  int           XPos;      /* horizontal position in pixel */
  int           YPos;      /* vertical position in pixel */
  unsigned long Ticks;     /* time of recent touch event */
  int           TouchId;   /* constant touch ID provided by touch controller */
  unsigned char State;     /* current state within a touch cycle */
} XTouchData;

static int           ShutDown        = 0;

static int           GuiSizeWidth    = 0; /* width of the GUI application (EwScreenSize.X) */
static int           GuiSizeHeight   = 0; /* height of the GUI application (EwScreenSize.Y) */
static int           TouchAreaWidth  = 0; /* width of the window or framebuffer */
static int           TouchAreaHeight = 0; /* height of the window or framebuffer */

static XTouchEvent   TouchEvent[ NO_OF_FINGERS ];
static XTouchData    TouchData[ NO_OF_FINGERS ];

/* current touch information read from input device within touch event thread */
static int           TouchX[ NO_OF_FINGERS ];
static int           TouchY[ NO_OF_FINGERS ];
static int           TouchId[ NO_OF_FINGERS ];


/*******************************************************************************
* FUNCTION:
*   TouchEventThread
*
* DESCRIPTION:
*   The function TouchEventThread implements an independend thread to read data
*   from the touch controller and to provide them as touch events for the
*   EmWi application. The thread is running until the flag ShutDown is set.
*   This function may be adapted to the touch driver of your hardware.
*
* ARGUMENTS:
*   None
*
* RETURN VALUE:
*   Zero.
*
*******************************************************************************/
static void* TouchEventThread( void* aArg )
{
  struct input_event events[ 64 ];
  int                i;
  int                rd;
  int                touchDev;
  char*              touchDevName = NULL;
  int                touchX = 0;
  int                touchY = 0;
  int                touchSlot = 0;
  int                touchId = 0;

  if (( touchDevName = getenv( "EW_TOUCHDEVICE" )) == NULL )
    touchDevName = DEFAULT_TOUCH_DEVICE;

  /* get access to touch events from input device */
  touchDev = open( touchDevName, O_RDONLY );
  if ( touchDev < 0 )
  {
    EwPrint( "Error: failed to open touch input device %s.\n", touchDevName );
    return 0;
  }

  /* clear all current touch state variables */
  for ( i = 0; i < NO_OF_FINGERS; i++ )
  {
    TouchX[ i ]  = 0;
    TouchY[ i ]  = 0;
    TouchId[ i ] = -1;
  }

  /* loop forever until main application is finished */
  while ( !ShutDown )
  {
    /* read touch event from input device */
    rd = read( touchDev, events, sizeof( struct input_event ) * 64 );

    /* check for right size - otherwise terminate */
    if ( rd < (int) sizeof( struct input_event ) )
      break;

    for ( i = 0; i < rd / sizeof( struct input_event ); i++ )
    {
      unsigned int type;
      unsigned int code;
      long         value;

      type  = events[ i ].type;
      code  = events[ i ].code;
      value = events[ i ].value;

      /* store current read parameter if report event occurs or if slot changes */
      if (( type == SYN_MT_REPORT ) || ( type == SYN_REPORT ) || (( type == EV_ABS ) && ( code == ABS_MT_SLOT )))
      {
        if (( touchSlot >= 0 ) && ( touchSlot < NO_OF_FINGERS ))
        {
          TouchX[ touchSlot ]  = touchX;
          TouchY[ touchSlot ]  = touchY;
          TouchId[ touchSlot ] = touchId;
          // EwPrint( "Touch Event slot %d, id %d, x %d, y %d\n", touchSlot, touchId, touchX, touchY );
        }
      }

      /* read next touch parameters */
      if ( type == EV_ABS )
      {
        if ( code == ABS_MT_POSITION_X )
          touchX = value;
        else if ( code == ABS_MT_POSITION_Y )
          touchY = value;
        else if ( code == ABS_MT_TRACKING_ID )
          touchId = value;
        else if ( code == ABS_MT_SLOT )
        {
          touchSlot = value;

          /* read back all current values when slot has changed */
          if (( touchSlot >= 0 ) && ( touchSlot < NO_OF_FINGERS ))
          {
            touchX  = TouchX[ touchSlot ];
            touchY  = TouchY[ touchSlot ];
            touchId = TouchId[ touchSlot ];
          }
        }
      }
    }
  }

  /* finally, close the input device */
  close( touchDev );

  return 0;
}


/*******************************************************************************
* FUNCTION:
*   EwBspTouchInit
*
* DESCRIPTION:
*   Initalizes the touch driver interface.
*
* ARGUMENTS:
*   aGuiWidth - Width of the GUI application in pixel.
*   aGuiHeight - Height of the GUI application in pixel.
*   aTouchWidth - Width of the window or framebuffer (touchable area) in pixel.
*   aTouchHeight - Height of the window or framebuffer (touchable area) in pixel.
*
* RETURN VALUE:
*   None
*
*******************************************************************************/
void EwBspTouchInit( int aGuiWidth, int aGuiHeight, int aTouchWidth, int aTouchHeight )
{
  pthread_t      touchEventThread;
  pthread_attr_t threadAttr;

  GuiSizeWidth    = aGuiWidth;
  GuiSizeHeight   = aGuiHeight;
  TouchAreaWidth  = aTouchWidth;
  TouchAreaHeight = aTouchHeight;
  ShutDown = 0;

  /* clear all touch state variables */
  memset( TouchData, 0, sizeof( TouchData ));

  /* create thread for touch events */
  pthread_attr_init( &threadAttr );
  pthread_attr_setdetachstate( &threadAttr, PTHREAD_CREATE_JOINABLE );
  pthread_create( &touchEventThread, &threadAttr, TouchEventThread, NULL );
  pthread_attr_destroy( &threadAttr );
  usleep( 100 );
}


/*******************************************************************************
* FUNCTION:
*   EwBspTouchDone
*
* DESCRIPTION:
*   Terminates the touch driver.
*
* ARGUMENTS:
*   None
*
* RETURN VALUE:
*   None
*
*******************************************************************************/
void EwBspTouchDone( void )
{
  ShutDown = 1;
  usleep( 100 );
}


/*******************************************************************************
* FUNCTION:
*   EwBspTouchGetEvents
*
* DESCRIPTION:
*   The function EwBspTouchGetEvents reads the current touch positions from the
*   touch driver and returns the current touch position and touch status of the
*   different fingers. The returned number of touch events indicates the number
*   of XTouchEvent that contain position and status information.
*   The orientation of the touch positions is adjusted to match GUI coordinates.
*   If the hardware supports only single touch, the finger number is always 0.
*
* ARGUMENTS:
*   aTouchEvent - Pointer to return array of XTouchEvent.
*
* RETURN VALUE:
*   Returns the number of detected touch events, otherwise 0.
*
*******************************************************************************/
int EwBspTouchGetEvents( XTouchEvent** aTouchEvent )
{
  int           x, y;
  int           t;
  int           f;
  unsigned long ticks;
  int           noOfEvents = 0;
  int           finger;
  char          identified[ NO_OF_FINGERS ];
  XTouchData*   touch;

  /* all fingers have the state unidentified */
  memset( identified, 0, sizeof( identified ));

  /* get current time in ms */
  ticks = EwGetTicks();

  /* iterate through all potential touch events from the touch event thread */
  for ( t = 0; t < NO_OF_FINGERS; t++ )
  {
    /* check for valid coordinates */
    if (( TouchId[ t ] < 0 ) ||
        ( TouchX[ t ] <= 0 ) || ( TouchX[ t ] > TouchAreaWidth ) ||
        ( TouchY[ t ] <= 0 ) || ( TouchY[ t ] > TouchAreaHeight ))
      continue;

    #if ( EW_ROTATION == 90 )

      x = TouchY[ t ] * GuiSizeWidth / TouchAreaHeight;
      y = ( TouchAreaWidth - TouchX[ t ] ) * GuiSizeHeight / TouchAreaWidth;

    #elif ( EW_ROTATION == 270 )

      x = ( TouchAreaHeight - TouchY[ t ] ) * GuiSizeWidth / TouchAreaHeight;
      y = TouchX[ t ] * GuiSizeHeight / TouchAreaWidth;

    #elif ( EW_ROTATION == 180 )

      x = ( TouchAreaWidth - TouchX[ t ] ) * GuiSizeWidth / TouchAreaWidth;
      y = ( TouchAreaHeight - TouchY[ t ] ) * GuiSizeHeight / TouchAreaHeight;

    #else

      x = TouchX[ t ] * GuiSizeWidth / TouchAreaWidth;
      y = TouchY[ t ] * GuiSizeHeight / TouchAreaHeight;

    #endif

    /* iterate through all fingers to find a finger that matches with the provided touch event */
    for ( finger = -1, f = 0; f < NO_OF_FINGERS; f++ )
    {
      touch = &TouchData[ f ];

      /* check if the finger is already active */
      if (( touch->State != EW_BSP_TOUCH_IDLE ) && ( touch->TouchId == TouchId[ t ]))
      {
        finger = f;
        break;
      }

      /* check if the finger was used within the recent time span and if the touch position is in the vicinity */
      if (( touch->State == EW_BSP_TOUCH_IDLE ) && ( ticks < touch->Ticks + DELTA_TIME )
        && ( x > touch->XPos - DELTA_TOUCH ) && ( x < touch->XPos + DELTA_TOUCH )
        && ( y > touch->YPos - DELTA_TOUCH ) && ( y < touch->YPos + DELTA_TOUCH ))
        finger = f;

      /* otherwise take the first free finger */
      if (( touch->State == EW_BSP_TOUCH_IDLE ) && ( finger == -1 ))
        finger = f;
    }

    /* determine the state within a touch cycle and assign the touch parameter to the found finger */
    if ( finger >= 0 )
    {
      touch = &TouchData[ finger ];
      identified[ finger ] = 1;

      /* check for start of touch cycle */
      if ( touch->State == EW_BSP_TOUCH_IDLE )
        touch->State = EW_BSP_TOUCH_DOWN;
      else
      {
        /* check if the finger has moved */
        if (( touch->XPos != x ) || ( touch->YPos != y ))
          touch->State = EW_BSP_TOUCH_MOVE;
        else
          touch->State = EW_BSP_TOUCH_HOLD;
      }

      /* store current touch parameter */
      touch->XPos    = x;
      touch->YPos    = y;
      touch->TouchId = TouchId[ t ];
      touch->Ticks   = ticks;
    }
  }

  /* prepare sequence of touch events suitable for Embedded Wizard GUI application */
  for ( f = 0; f < NO_OF_FINGERS; f++ )
  {
    touch = &TouchData[ f ];

    /* begin of a touch cycle */
    if ( identified[ f ] && ( touch->State == EW_BSP_TOUCH_DOWN ))
      TouchEvent[ noOfEvents ].State = EW_BSP_TOUCH_DOWN;

    /* move within a touch cycle */
    else if ( identified[ f ] && ( touch->State == EW_BSP_TOUCH_MOVE ))
      TouchEvent[ noOfEvents ].State = EW_BSP_TOUCH_MOVE;

    /* end of a touch cycle */
    else if ( !identified[ f ] && ( touch->State != EW_BSP_TOUCH_IDLE ))
    {
      TouchEvent[ noOfEvents ].State = EW_BSP_TOUCH_UP;
      touch->State = EW_BSP_TOUCH_IDLE;
    }
    else
      continue;

    TouchEvent[ noOfEvents ].XPos   = touch->XPos;
    TouchEvent[ noOfEvents ].YPos   = touch->YPos;
    TouchEvent[ noOfEvents ].Finger = f;

    // EwPrint( "Touch event for finger %d with state %d ( %4d, %4d )\n", f, TouchEvent[ noOfEvents ].State, TouchEvent[ noOfEvents ].XPos, TouchEvent[ noOfEvents ].YPos );

    noOfEvents++;
  }

  /* return the prepared touch events and the number of prepared touch events */
  if ( aTouchEvent )
    *aTouchEvent = TouchEvent;

  return noOfEvents;
}


/* msy */
