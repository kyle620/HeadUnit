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
*   This file implements a generic framework for running Embedded Wizard
*   generated GUI applications on a dedicated target with or without the usage
*   of an operating system.
*   The module ewmain contains three major functions that are used within
*   every Embedded Wizard GUI application: EwInit() - EwProcess() - EwDone().
*   These functions represent the lifecycle of the entire GUI application.
*
*   EwInit() is responsible to initialize the system, to configure the display,
*   to get access to the desired input devices (like keyboard or touch),
*   to initialize the Embedded Wizard Runtime Environment / Graphics Engine,
*   to create an instance of the application class and to initialize all
*   needed peripheral components.
*
*   EwProcess() implements one cycle of the main loop. This function has to be
*   called in an (endless) loop and contains typically the following steps:
*   1. Processing data from your device driver(s)
*   2. Processing key events
*   3. Processing cursor or touch screen events
*   4. Processing timers
*   5. Processing signals
*   6. Updating the screen
*   7. Triggering the garbage collection
*
*   EwDone() is responsible to shutdown the application and to release all
*   used resources.
*
*   Important: Please be aware that every Embedded Wizard GUI application
*   requires the execution in a single GUI task!
*   If you are working with an operating system and your software is using
*   several threads/tasks, please take care to access your GUI application
*   only within the context of your GUI thread/task. Use operating system
*   services to exchange data or events between the GUI thread/task and other
*   worker threads/tasks.
*
*   In order to keep this framework independent from the particular GUI
*   application, the application class and the screen size are taken from the
*   generated code. In this manner, it is not necessary to modify this file
*   when creating new GUI applications. Just set the attributes 'ScreenSize'
*   and 'ApplicationClass' of the profile in the Embedded Wizard IDE.
*
*   For more information concerning the integration of an Embedded Wizard
*   generated GUI application into your main application, please see
*   https://doc.embedded-wizard.de/main-loop
*
*******************************************************************************/

#include <unistd.h>

#include "ewconfig.h"
#include "ewmain.h"
#include "Core.h"
#include "Graphics.h"

#include "ew_bsp_display.h"
#include "ew_bsp_touch.h"
#include "ew_bsp_console.h"

#include "DeviceDriver.h"


/* memory pool */
#ifdef EW_MEMORY_POOL_SECTION
  EW_MEMORY_POOL_SECTION static unsigned long
    EwMemory[ EW_MEMORY_POOL_SIZE / sizeof( unsigned long )];
  #define EW_MEMORY_POOL_ADDR EwMemory
#endif

/* optional second memory pool */
#ifdef EW_EXTRA_POOL_SECTION
  EW_EXTRA_POOL_SECTION static unsigned long
    EwExtraMemory[ EW_EXTRA_POOL_SIZE / sizeof( unsigned long )];
  #define EW_EXTRA_POOL_ADDR EwExtraMemory
#endif

#define CHECK_HANDLE( handle ) \
  if ( !handle )               \
  {                            \
    EwPrint( "[failed]\n" );   \
    return 0;                  \
  }                            \
  else                         \
    EwPrint( "[OK]\n" );

/* helper functions used within this module */
static void EwUpdate( XViewport* aViewport, CoreRoot aApplication );
static void ViewportProc( XViewport* aViewport, unsigned long aHandle,
  void* aDisplay1, void* aDisplay2, void* aDisplay3, XRect aArea );
static XEnum EwGetKeyCommand( void );


static void*      EglDisplay  = 0;
static void*      EglSurface  = 0;
static int32_t    Framebuffer = 0xFFFFFFFF;
static int32_t    Width       = -1;
static int32_t    Height      = -1;
static CoreRoot   RootObject;
static XViewport* Viewport;


/*******************************************************************************
* FUNCTION:
*   EwInit
*
* DESCRIPTION:
*   EwInit() is responsible to initialize the system, to configure the display,
*   to get access to the desired input devices (like keyboard or touch),
*   to initialize the Embedded Wizard Runtime Environment / Graphics Engine,
*   to create an instance of the application class and to initialize all
*   needed peripheral components.
*
* ARGUMENTS:
*   None
*
* RETURN VALUE:
*   1 if successful, 0 otherwise.
*
*******************************************************************************/
int EwInit( void )
{
  /* initialize display */
  EwPrint( "Initialize Display...                        " );
  CHECK_HANDLE( EwBspDisplayInit( &EglDisplay, &EglSurface, &Framebuffer, &Width, &Height ));

  /* initialize touchscreen */
  EwPrint( "Initialize Touch Driver...                   " );
  EwBspTouchInit( EwScreenSize.X, EwScreenSize.Y, Width, Height );
  EwPrint( "[OK]\n" );

  #if EW_MEMORY_POOL_SIZE > 0
    /* initialize heap manager */
    EwPrint( "Initialize Memory Manager...                 " );
    EwInitHeap( 0 );
    EwAddHeapMemoryPool( (void*)EW_MEMORY_POOL_ADDR, EW_MEMORY_POOL_SIZE );

    #if EW_EXTRA_POOL_SIZE > 0
      EwAddHeapMemoryPool( (void*)EW_EXTRA_POOL_ADDR, EW_EXTRA_POOL_SIZE );
    #endif

    EwPrint( "[OK]\n" );
  #endif

  /* initialize the Graphics Engine and Runtime Environment */
  EwPrint( "Initialize Graphics Engine...                " );
  CHECK_HANDLE( EwInitGraphicsEngine( 0 ));

  /* create the applications root object ... */
  EwPrint( "Create Embedded Wizard Root Object...        " );
  RootObject = (CoreRoot)EwNewObjectIndirect( EwApplicationClass, 0 );
  CHECK_HANDLE( RootObject );

  EwLockObject( RootObject );
  CoreRoot__Initialize( RootObject, EwScreenSize );

  /* create Embedded Wizard viewport object to provide uniform access to the framebuffer */
  EwPrint( "Create Embedded Wizard Viewport...           " );
  Viewport = EwInitViewport( EwScreenSize, EwNewRect( 0, 0, Width, Height ),
    EW_ROTATION, 255, &Framebuffer, EglDisplay, EglSurface, ViewportProc );
  CHECK_HANDLE( Viewport );

  /* initialize your device driver(s) that provide data for your GUI */
  DeviceDriver_Initialize();

  EwPrint( "Starting Embedded Wizard main loop - press <p> to shutdown application...\n" );

  return 1;
}


/*******************************************************************************
* FUNCTION:
*   EwDone
*
* DESCRIPTION:
*   EwDone() is responsible to shutdown the application and to release all
*   used resources.
*
* ARGUMENTS:
*   None
*
* RETURN VALUE:
*   None.
*
*******************************************************************************/
void EwDone( void )
{
  /* deinitialize your device driver(s) */
  DeviceDriver_Deinitialize();

  /* destroy the applications root object and release unused resources and memory */
  EwPrint( "Shutting down Application...                 " );
  EwDoneViewport( Viewport );
  EwUnlockObject( RootObject );
  EwReclaimMemory();
  EwPrint( "[OK]\n" );

  /* deinitialize the Graphics Engine */
  EwPrint( "Deinitialize Graphics Engine...              " );
  EwDoneGraphicsEngine();
  EwPrint( "[OK]\n" );

  #if EW_MEMORY_POOL_SIZE > 0
    /* deinitialize heap manager */
    EwDoneHeap();
  #endif

  EwPrint( "Deinitialize Touch Driver...                 " );
  EwBspTouchDone();
  EwPrint( "[OK]\n" );

  /* deinitialize display */
  EwBspDisplayDone( EglDisplay, EglSurface );
}


/*******************************************************************************
* FUNCTION:
*   EwProcess
*
* DESCRIPTION:
*   EwProcess() implements one cycle of the main loop. This function has to be
*   called in an (endless) loop and contains typically the following steps:
*   1. Processing data from your device driver(s)
*   2. Processing key events
*   3. Processing cursor or touch screen events
*   4. Processing timers
*   5. Processing signals
*   6. Updating the screen
*   7. Triggering the garbage collection
*   For more information concerning the integration of an Embedded Wizard
*   generated GUI application into your main application, please see
*   https://doc.embedded-wizard.de/main-loop
*
* ARGUMENTS:
*   None.
*
* RETURN VALUE:
*   1, if further processing is needed, 0 otherwise.
*
*******************************************************************************/
int EwProcess( void )
{
  int          timers  = 0;
  int          signals = 0;
  int          events  = 0;
  int          devices = 0;
  XEnum        cmd     = CoreKeyCodeNoKey;
  int          noOfTouch;
  XTouchEvent* touchEvent;
  int          touch;
  int          finger;
  XPoint       touchPos;

  /* process data of your device driver(s) and update the GUI
     application by setting properties or by triggering events */
  devices = DeviceDriver_ProcessData();

  /* receive keyboard inputs */
  cmd = EwGetKeyCommand();

  if ( cmd != CoreKeyCodeNoKey )
  {
    if ( cmd == CoreKeyCodePower )
      return 0;

    /* feed the application with a 'press' and 'release' event */
    events |= CoreRoot__DriveKeyboardHitting( RootObject, cmd, 0, 1 );
    events |= CoreRoot__DriveKeyboardHitting( RootObject, cmd, 0, 0 );
  }

  /* receive (multi-) touch inputs and provide it to the application */
  noOfTouch = EwBspTouchGetEvents( &touchEvent );

  if ( noOfTouch > 0 )
  {
    for ( touch = 0; touch < noOfTouch; touch++ )
    {
      /* get data out of the touch event */
      finger     = touchEvent[ touch ].Finger;
      touchPos.X = touchEvent[ touch ].XPos;
      touchPos.Y = touchEvent[ touch ].YPos;

      /* begin of touch cycle */
      if ( touchEvent[ touch ].State == EW_BSP_TOUCH_DOWN )
        events |= CoreRoot__DriveMultiTouchHitting(  RootObject, 1, finger, touchPos );

      /* movement during touch cycle */
      else if ( touchEvent[ touch ].State == EW_BSP_TOUCH_MOVE )
        events |= CoreRoot__DriveMultiTouchMovement( RootObject, finger, touchPos );

      /* end of touch cycle */
      else if ( touchEvent[ touch ].State == EW_BSP_TOUCH_UP )
        events |= CoreRoot__DriveMultiTouchHitting(  RootObject, 0, finger, touchPos );
    }
  }

  /* process expired timers */
  timers = EwProcessTimers();

  /* process the pending signals */
  signals = EwProcessSignals();

  /* refresh the screen, if something has changed and draw its content */
  if ( devices || timers || signals || events )
  {
    if ( CoreRoot__DoesNeedUpdate( RootObject ))
      EwUpdate( Viewport, RootObject );

    /* just for debugging purposes: check the memory structure */
    EwVerifyHeap();

    /* after each processed message start the garbage collection */
    EwReclaimMemory();

    /* print current memory statistic to console interface */
    #ifdef EW_PRINT_MEMORY_USAGE
      EwPrintProfilerStatistic( 0 );
    #endif

    /* evaluate memory pools and print report */
    #ifdef EW_DUMP_HEAP
      EwDumpHeap( 0 );
    #endif
  }
  else
  {
    /* otherwise sleep/suspend the UI application... */
    usleep( 100 );
  }

  return 1;
}


/*******************************************************************************
* FUNCTION:
*   EwUpdate
*
* DESCRIPTION:
*   The function EwUpdate performs the screen update of the dirty area.
*
* ARGUMENTS:
*   aViewPort    - Viewport used for the screen update.
*   aApplication - Root object used for the screen update.
*
* RETURN VALUE:
*   None
*
*******************************************************************************/
static void EwUpdate( XViewport* aViewport, CoreRoot aApplication )
{
  XBitmap*       bitmap     = EwBeginUpdate( aViewport );
  GraphicsCanvas canvas     = EwNewObject( GraphicsCanvas, 0 );
  XRect          updateRect = {{ 0, 0 }, { 0, 0 }};

  /* let's redraw the dirty area of the screen. Cover the returned bitmap
     objects within a canvas, so Mosaic can draw to it. */
  if ( bitmap && canvas )
  {
    GraphicsCanvas__AttachBitmap( canvas, (XUInt32)bitmap );
    updateRect = CoreRoot__UpdateGE20( aApplication, canvas );
    GraphicsCanvas__DetachBitmap( canvas );
  }

  /* complete the update */
  if ( bitmap )
    EwEndUpdate( aViewport, updateRect );
}


/* Completion callback for the viewport. If EwEndUpdate() is called, the
   callback ensures, that the screen content is flipped */
static void ViewportProc( XViewport* aViewport, unsigned long aHandle,
  void* aDisplay1, void* aDisplay2, void* aDisplay3, XRect aArea )
{
  /* Perform the swap if there was something drawn on the screen */
  if (( aArea.Point2.X > aArea.Point1.X ) && ( aArea.Point2.Y > aArea.Point1.Y ))
    EwBspDisplaySwapBuffers( aDisplay2, aDisplay3 );
}


/*******************************************************************************
* FUNCTION:
*   EwGetKeyCommand
*
* DESCRIPTION:
*   The function EwGetKeyCommand reads the next key code from the console and
*   translates it into an Embedded Wizard key code. The mapping between the key
*   code from the console and the resulting Embedded Wizard key code can be
*   adapted to the needs of your application.
*
* ARGUMENTS:
*   None
*
* RETURN VALUE:
*   Returns the next EmWi key code or CoreKeyCodeNoKey if no key code available.
*
*******************************************************************************/
static XEnum EwGetKeyCommand( void )
{
  #if EW_USE_TERMINAL_INPUT == 1
    switch ( EwBspConsoleGetCharacter())
    {
      case 0x1b :
        switch ( EwBspConsoleGetCharacter())
        {
          case 0x00 : return CoreKeyCodeExit;
          case 0x5b :
            switch ( EwBspConsoleGetCharacter())
            {
              case 0x41 : return CoreKeyCodeUp;
              case 0x42 : return CoreKeyCodeDown;
              case 0x43 : return CoreKeyCodeRight;
              case 0x44 : return CoreKeyCodeLeft;
            }
          break;
        }
      break;

      case 0x0A : return CoreKeyCodeOk;
      case 'm'  : return CoreKeyCodeMenu;
      case 'p'  : return CoreKeyCodePower;
    }
  #endif
  return CoreKeyCodeNoKey;
}


/*******************************************************************************
* FUNCTION:
*   EwPrintSystemInfo
*
* DESCRIPTION:
*   This function prints system and configuration information - very helpful in
*   case of any support issues.
*
* ARGUMENTS:
*   None
*
* RETURN VALUE:
*   None
*
*******************************************************************************/
void EwPrintSystemInfo( void )
{
  EwPrint( "---------------------------------------------\n" );
  EwPrint( "Target system                                %s      \n", PLATFORM_STRING );
  EwPrint( "Color format                                 %s      \n", EW_FRAME_BUFFER_COLOR_FORMAT_STRING );
  #if EW_MEMORY_POOL_SIZE > 0
  EwPrint( "MemoryPool address                           0x%08X  \n", EW_MEMORY_POOL_ADDR );
  EwPrint( "MemoryPool size                              %u bytes\n", EW_MEMORY_POOL_SIZE );
  #endif
  #if EW_EXTRA_POOL_SIZE > 0
  EwPrint( "ExtraPool address                            0x%08X  \n", EW_EXTRA_POOL_ADDR );
  EwPrint( "ExtraPool size                               %u bytes\n", EW_EXTRA_POOL_SIZE );
  #endif
  EwPrint( "Framebuffer size                             %u x %u \n", Width, Height );
  EwPrint( "EwScreeenSize                                %d x %d \n", EwScreenSize.X, EwScreenSize.Y );
  EwPrint( "Graphics accelerator                         %s      \n", GRAPHICS_ACCELERATOR_STRING );
  EwPrint( "Vector graphics support                      %s      \n", VECTOR_GRAPHICS_SUPPORT_STRING );
  EwPrint( "Warp function support                        %s      \n", WARP_FUNCTION_SUPPORT_STRING );
  EwPrint( "Index8 bitmap resource format                %s      \n", INDEX8_SURFACE_SUPPORT_STRING );
  EwPrint( "RGB565 bitmap resource format                %s      \n", RGB565_SURFACE_SUPPORT_STRING );
  EwPrint( "Bidirectional text support                   %s      \n", BIDI_TEXT_SUPPORT_STRING );
  EwPrint( "Operating system                             %s      \n", OPERATING_SYSTEM_STRING );
  EwPrint( "Toolchain                                    %s      \n", TOOLCHAIN_STRING );
  #ifdef COMPILER_VERSION_STRING
  EwPrint( "C-Compiler version                           %s      \n", COMPILER_VERSION_STRING );
  #endif
  EwPrint( "Build date and time                          %s, %s  \n", __DATE__, __TIME__ );
  EwPrint( "Runtime Environment (RTE) version            %u.%02u \n", EW_RTE_VERSION >> 16, EW_RTE_VERSION & 0xFF );
  EwPrint( "Graphics Engine (GFX) version                %u.%02u \n", EW_GFX_VERSION >> 16, EW_GFX_VERSION & 0xFF );
  EwPrint( "Max surface cache size                       %u bytes\n", EW_MAX_SURFACE_CACHE_SIZE );
  EwPrint( "Glyph cache size                             %u x %u \n", EW_MAX_GLYPH_SURFACE_WIDTH, EW_MAX_GLYPH_SURFACE_HEIGHT );
  EwPrint( "Max issue tasks                              %u      \n", EW_MAX_ISSUE_TASKS );
  EwPrint( "Surface rotation                             %u      \n", EW_ROTATION );
  EwPrint( "---------------------------------------------\n" );
}


/* msy, mli */
