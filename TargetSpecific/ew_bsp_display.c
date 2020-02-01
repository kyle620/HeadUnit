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
*   This template is responsible to initialize the display hardware of the board
*   and to provide the necessary access to update the display content.
*
*   Important: This file is intended to be used as a template. Please adapt the
*   implementation and declarations according your particular hardware.
*   This implementation is prepared for using an embedded Linux system.
*   The display is accessed by using EGL. The color format of the framebuffer
*   has to correspond to the color format of the Graphics Engine.
*
*******************************************************************************/

#include "ewrte.h"
#include "ewgfx.h"

#include "ew_bsp_display.h"

#include "gfx_system_drm.h"


/*******************************************************************************
* FUNCTION:
*   EwBspDisplayInit
*
* DESCRIPTION:
*   The function EwBspDisplayInit initializes the display hardware via EGL and
*   returns the display parameter.
*
* ARGUMENTS:
*   aDisplay - Pointer to return EGL display.
*   aSurface - Pointer to return EGL surface.
*   aFrameBuffer - Pointer to return the framebuffer.
*   aWidth   - Pointer to return the width of the framebuffer in pixel.
*   aHeight  - Pointer to return the height of the framebuffer in pixel.
*
* RETURN VALUE:
*   Returns 1 if successful, 0 otherwise.
*
*******************************************************************************/
int EwBspDisplayInit( void** aDisplay, void** aSurface, int* aFrameBuffer,
  int* aWidth, int* aHeight )
{
  return DrmEglInit( aDisplay, aSurface, aFrameBuffer, aWidth, aHeight );
}


/*******************************************************************************
* FUNCTION:
*   EwBspDisplayDone
*
* DESCRIPTION:
*   The function EwBspDisplayDone deinitializes EGL.
*
* ARGUMENTS:
*   aEglDisplay - EGL display to deinitialize.
*   aEglSurface - EGL surface to deinitialize.
*
* RETURN VALUE:
*   None
*
*******************************************************************************/
void EwBspDisplayDone( void* aDisplay, void* aSurface )
{

  DrmEglDone( aDisplay, aSurface );
}


/*******************************************************************************
* FUNCTION:
*   EwBspDisplaySwapBuffers
*
* DESCRIPTION:
*   The function EwBspDisplaySwapBuffers is called from the completion callback
*   of the viewport. The function ensures that the screen content is shown by
*   swapping the EGL buffers.
*
* ARGUMENTS:
*   aEglDisplay - EGL display.
*   aEglSurface - EGL surface.
*
* RETURN VALUE:
*   None
*
*******************************************************************************/
void EwBspDisplaySwapBuffers( void* aDisplay, void* aSurface )
{
  DrmEglSwapBuffers( aDisplay, aSurface );
}


/* mli, msy */
