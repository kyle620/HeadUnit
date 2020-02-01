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
*   This file implements the main.c module for running Embedded Wizard
*   generated GUI applications on a dedicated target with or without the usage
*   of an operating system.
*
*   Important: Please be aware that every Embedded Wizard GUI application
*   requires the execution in a single GUI task!
*   If you are working with an operating system and your software is using
*   several threads/tasks, please take care to access your GUI application
*   only within the context of your GUI thread/task. Use operating system
*   services to exchange data or events between the GUI thread/task and other
*   worker threads/tasks.
*
*   For more information concerning the integration of an Embedded Wizard
*   generated GUI application into your main application, please see
*   https://doc.embedded-wizard.de/main-loop
*
*******************************************************************************/

#include "ewmain.h"
#include "ewrte.h"
#include "ew_bsp_console.h"

#include "gfx_system_drm.h"

/*******************************************************************************
* FUNCTION:
*   main
*
* DESCRIPTION:
*   The main function for running Embedded Wizard generated GUI applications on
*   a dedicated target using the Linux operating system.
*
* ARGUMENTS:
*   None
*
* RETURN VALUE:
*   Zero if successful.
*
*******************************************************************************/
int main( void )
{
  /* initialize console interface for debug messages */
  EwBspConsoleInit();

  /* initialize the graphics subsystem */
  GfxSystemInit( FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT );

  /* initialize Embedded Wizard application */
  if ( EwInit() == 0 )
    return 1;

  EwPrintSystemInfo();

  /* process the graphics subsystem and the Embedded Wizard main loop */
  while( GfxSystemProcess() && EwProcess())
    ;

  /* de-initialize Embedded Wizard application */
  EwDone();

  /* de-initialize graphics subsystem */
  GfxSystemDone();

  /* restore console */
  EwBspConsoleDone();

  return 0;
}


/* msy, mli */
