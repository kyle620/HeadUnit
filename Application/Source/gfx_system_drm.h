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
*   generated UI application and the graphics subsystem.
*
*   This template is responsible to access the grafics subsystem within an
*   embedded Linux environment. The display is accessed by using EGL via the
*   graphics subsystem (e.g. Wayland, DRM, X11 or fbdev).
*
*   Important: This file is intended to be used as a template. Please adapt the
*   implementation and declarations according your particular hardware.
*
*******************************************************************************/

#ifndef GFX_SYSTEM_H
#define GFX_SYSTEM_H


#ifdef __cplusplus
  extern "C"
  {
#endif


/*******************************************************************************
* FUNCTION:
*   GfxSystemInit
*
* DESCRIPTION:
*   The function GfxSystemInit is responsible to get an access to the
*   graphics subsystem. In case that the underlying graphics subsystem contains
*   a window manager, the given size is used to create a suitable window.
*   Otherwise, the given size is ignored, because the size of the framebuffer
*   cannot be changed.
*
* ARGUMENTS:
*   aWidth   - Desired width of the GUI application in pixel.
*   aHeight  - Desired height of the GUI application in pixel.
*
* RETURN VALUE:
*   Returns 1 if successful, 0 otherwise.
*
*******************************************************************************/
int GfxSystemInit
(
  int                         aWidth,
  int                         aHeight
);


/*******************************************************************************
* FUNCTION:
*   GfxSystemDone
*
* DESCRIPTION:
*   The function GfxSystemDone is responsible to close the access to the
*   graphics subsystem.
*
* ARGUMENTS:
*   None
*
* RETURN VALUE:
*   None
*
*******************************************************************************/
void GfxSystemDone( void );


/*******************************************************************************
* FUNCTION:
*   GfxSystemProcess
*
* DESCRIPTION:
*   The function GfxSystemProcess drives the graphics subsystem or window
*   manager in order to process all pending events.
*
* ARGUMENTS:
*   None
*
* RETURN VALUE:
*   1, if further processing is needed, 0 otherwise.
*
*******************************************************************************/
int GfxSystemProcess( void );


/*******************************************************************************
* FUNCTION:
*   DrmEglInit
*
* DESCRIPTION:
*   The function DrmEglInit initializes the display hardware via EGL and
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
int DrmEglInit
(
  void**                      aDisplay,
  void**                      aSurface,
  int*                        aFrameBuffer,
  int*                        aWidth,
  int*                        aHeight
);


/*******************************************************************************
* FUNCTION:
*   DrmEglDone
*
* DESCRIPTION:
*   The function DrmEglDone deinitializes EGL.
*
* ARGUMENTS:
*   aEglDisplay - EGL display to deinitialize.
*   aEglSurface - EGL surface to deinitialize.
*
* RETURN VALUE:
*   None
*
*******************************************************************************/
void DrmEglDone
(
  void*                       aDisplay,
  void*                       aSurface
);


/*******************************************************************************
* FUNCTION:
*   DrmEglSwapBuffers
*
* DESCRIPTION:
*   The function DrmEglSwapBuffers is called from the completion callback
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
void DrmEglSwapBuffers
(
  void*                       aDisplay,
  void*                       aSurface
);


#ifdef __cplusplus
  }
#endif

#endif /* GFX_SYSTEM_H */


/* msy */
