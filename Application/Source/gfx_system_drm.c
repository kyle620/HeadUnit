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

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <errno.h>

#include "ewrte.h"
#include "gfx_system_drm.h"

#define DEFAULT_DRM_DEVICE "/dev/dri/card1"

static drmModeModeInfo*    DrmMode = NULL;
static int                 DrmFd;
static uint32_t            DrmCrtcId;
static drmModeConnector*   DrmConnector;
static struct gbm_device*  GbmDevice;
static struct gbm_surface* GbmSurface;
static struct gbm_bo*      GbmBuffer;


/*******************************************************************************
 * private functions
 *******************************************************************************/
/*
 * callback function, called after frame buffer flip is done
 */
static void DrmFbFlipCallback( int fd, unsigned int frame, unsigned int sec,
  unsigned int usec, void* data )
{
  /* suppress 'unused parameter' warnings */
  (void)fd, (void)frame, (void)sec, (void)usec;

  int* waiting_for_flip = data;
  *waiting_for_flip = 0;
}


/*
 * callback function to destroy a drm frame buffer
 */
static void DrmFbDestroyCallback( struct gbm_bo* aGbmBuffer, void* data )
{
  int drm_fd     = gbm_device_get_fd( gbm_bo_get_device( aGbmBuffer ));
  uint32_t fb_id = (uint32_t)data;

  if ( fb_id )
    drmModeRmFB( drm_fd, fb_id );
}


/*
 * helper function to get a drm frame buffer
 */
static uint32_t DrmGetFb( struct gbm_bo* aGbmBuffer )
{
  int drm_fd = gbm_device_get_fd( gbm_bo_get_device( aGbmBuffer ));
  uint32_t fb_id = (uint32_t) gbm_bo_get_user_data( aGbmBuffer );
  uint32_t width, height, format, strides[4] = {0}, handles[4] = {0}, offsets[4] = {0};
  int ret = -1;

  if ( fb_id )
    return fb_id;

  width  = gbm_bo_get_width( aGbmBuffer );
  height = gbm_bo_get_height( aGbmBuffer );
  format = gbm_bo_get_format( aGbmBuffer );

  memcpy( handles, (uint32_t [4]){ gbm_bo_get_handle( aGbmBuffer ).u32, 0, 0, 0 }, 16 );
  memcpy( strides, (uint32_t [4]){ gbm_bo_get_stride( aGbmBuffer ), 0, 0, 0 }, 16 );
  memset( offsets, 0, 16 );

  ret = drmModeAddFB2( drm_fd, width, height, format, handles, strides, offsets, &fb_id, 0 );

  if ( ret )
    return 0;

  gbm_bo_set_user_data( aGbmBuffer, (void*)fb_id, DrmFbDestroyCallback );

  return fb_id;
}

/*
 * helper function to get the config index for the given visual_id
 */
static int match_config_to_visual( EGLDisplay egl_display, EGLint visual_id,
  EGLConfig* configs, int count )
{
  int i;

  for ( i = 0; i < count; ++i )
  {
    EGLint id;

    if ( ! eglGetConfigAttrib( egl_display, configs[ i ], EGL_NATIVE_VISUAL_ID, &id ))
      continue;

    if ( id == visual_id )
      return i;
  }

  return -1;
}


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
int GfxSystemInit( int aWidth, int aHeight )
{
  drmModeRes*       resources;
  drmModeConnector* connector;
  drmModeEncoder*   encoder;
  uint32_t          drmCrtcId = 0;
  unsigned int      i;
  char*             drmDevName = NULL;

  if (( drmDevName = getenv( "EW_DRMDEVICE" )) == NULL )
    drmDevName = DEFAULT_DRM_DEVICE;

  DrmFd = open( drmDevName, O_RDWR );
  if ( DrmFd < 0 )
  {
    EwPrint( "GfxSystemInit: Cannot open '%s'!\n", drmDevName );
    return 0;
  }

  /* retrieve resources */
  resources = drmModeGetResources( DrmFd );
  if ( !resources )
    return 0;

  /* iterate all connectors and use the first connected one */
  for ( i = 0; i < resources->count_connectors; i++ )
  {
    /* get information for each connector */
    connector = drmModeGetConnector( DrmFd, resources->connectors[ i ] );
    if ( !connector )
      continue;

    if ( connector->connection != DRM_MODE_CONNECTED )
    {
      EwPrint( "GfxSystemInit: DRM connector not connected\n" );
      drmModeFreeConnector( connector );
      continue;
    }

    /* find preferred mode or the highest resolution mode: */
    for ( i = 0; i < connector->count_modes; i++ )
    {
      drmModeModeInfo*  mode = &connector->modes[ i ];
      if ( mode->type & DRM_MODE_TYPE_PREFERRED )
      {
        DrmMode = mode;
        break;
      }
    }

    if ( !DrmMode )
    {
      drmModeFreeConnector( connector );
      continue;
    }

    /* find crtc_id via encoder */
    for ( i = 0; i < resources->count_encoders; i++ )
    {
      encoder = drmModeGetEncoder( DrmFd, resources->encoders[i] );
      if ( encoder->encoder_id == connector->encoder_id )
      {
        drmCrtcId = encoder->crtc_id;
        break;
      }

      drmModeFreeEncoder( encoder );
    }

    if ( connector && DrmMode && drmCrtcId )
    {
      DrmConnector   = connector;
      DrmCrtcId      = drmCrtcId;
    }
    else if ( connector )
    {
      drmModeFreeConnector( connector );
    }
  }
  drmModeFreeResources( resources );

  /*
  EwPrint( "%u x %u, %u x %u mm, connector_type: %u, connector_type_id: %u\n",
    DrmMode->hdisplay, DrmMode->vdisplay, DrmConnector->mmWidth, DrmConnector->mmHeight,
    DrmConnector->connector_type, DrmConnector->connector_type_id);
  */

  return ( DrmMode != 0 );
}


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
void GfxSystemDone( void )
{
  if ( DrmConnector )
    drmModeFreeConnector( DrmConnector );

  if ( DrmFd )
    close ( DrmFd );
}


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
int GfxSystemProcess( void )
{
  return 1;
}


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
int DrmEglInit( void** aDisplay, void** aSurface, int* aFrameBuffer,
  int* aWidth, int* aHeight )
{
  EGLDisplay        eglDisplay        = 0;
  EGLConfig         eglConfig         = 0;
  EGLSurface        eglSurface        = 0;
  EGLContext        eglContext        = 0;
  EGLint            count = 0;
  EGLint            matched = 0;
  EGLConfig*        configs;
  int               config_index      = -1;
  uint32_t          fb_id;
  int               errorCode;
  PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = 0;
  const EGLint      contextAttribs[]  = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
  const char*       eglClientExtensions;
  const EGLint      configAttribs[] =
  {
    EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_RED_SIZE,        1,
    EGL_GREEN_SIZE,      1,
    EGL_BLUE_SIZE,       1,
    EGL_ALPHA_SIZE,      0,
    EGL_SAMPLES,         0,
    EGL_NONE
  };

  if ( !DrmMode )
    return 0;

  /* access to EGL is done via GBM (graphics buffer management) */
  GbmDevice = gbm_create_device( DrmFd );
  if ( !GbmDevice )
    return 0;

  GbmSurface = gbm_surface_create( GbmDevice, DrmMode->hdisplay, DrmMode->vdisplay,
    GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING );
  if ( !GbmSurface )
    return 0;

  /* get egl extension */
  eglClientExtensions = eglQueryString( EGL_NO_DISPLAY, EGL_EXTENSIONS );
  if ( strstr( eglClientExtensions, "EGL_EXT_platform_base" ))
    eglGetPlatformDisplayEXT = (void*)eglGetProcAddress( "eglGetPlatformDisplayEXT" );

  if ( eglGetPlatformDisplayEXT )
    eglDisplay = eglGetPlatformDisplayEXT( EGL_PLATFORM_GBM_KHR, GbmDevice, NULL );
  else
    eglDisplay = eglGetDisplay((void*)GbmDevice );

  eglInitialize( eglDisplay, 0, 0 );
  eglBindAPI( EGL_OPENGL_ES_API );

  if (!eglGetConfigs(eglDisplay, NULL, 0, &count) || count < 1) {
    EwPrint("No EGL configs to choose from.\n");
    return 0;
  }

  configs = malloc(count * sizeof *configs);
  if (!configs)
    return 0;

  if (!eglChooseConfig(eglDisplay, configAttribs, configs,
            count, &matched) || !matched) {
    EwPrint("No EGL configs with appropriate attributes.\n");
    goto out;
  }

  config_index = match_config_to_visual(
    eglDisplay, GBM_FORMAT_XRGB8888, configs, matched);

  if (config_index != -1)
    eglConfig = configs[config_index];

out:
  free(configs);
  if (config_index == -1)
    return 0;

  eglSurface = eglCreateWindowSurface( eglDisplay, eglConfig, (EGLNativeWindowType)GbmSurface, 0 );
  eglContext = eglCreateContext( eglDisplay, eglConfig, EGL_NO_CONTEXT, contextAttribs );

  #if EW_PERFORM_FULLSCREEN_UPDATE == 0
    eglSurfaceAttrib( eglDisplay, eglSurface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED );
  #else
    eglSurfaceAttrib( eglDisplay, eglSurface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_DESTROYED );
  #endif

  eglMakeCurrent( eglDisplay, eglSurface, eglSurface, eglContext );

  /* return EGL display and surface */
  if ( aDisplay )
    *aDisplay = eglDisplay;
  if ( aSurface )
    *aSurface = eglSurface;

  /* get the framebuffer and its size in pixel */
  if ( aFrameBuffer )
    glGetIntegerv( GL_FRAMEBUFFER_BINDING, aFrameBuffer );
  if ( aWidth )
    eglQuerySurface( eglDisplay, eglSurface, EGL_WIDTH, aWidth );
  if ( aHeight )
    eglQuerySurface( eglDisplay, eglSurface, EGL_HEIGHT, aHeight );

  eglSwapBuffers( eglDisplay, eglSurface );
  GbmBuffer = gbm_surface_lock_front_buffer( GbmSurface );
  fb_id = DrmGetFb( GbmBuffer );
  if ( !fb_id )
    return 0;

  /* set mode */
  errorCode = drmModeSetCrtc( DrmFd, DrmCrtcId, fb_id, 0, 0, &DrmConnector->connector_id, 1, DrmMode );
  if ( errorCode )
    return 0;

  return 1;
}


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
void DrmEglDone( void* aDisplay, void* aSurface )
{
  /* shutdown the EGL / OpenGL ES 2.0 sub-system */
  eglMakeCurrent( (EGLDisplay)aDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT ) ;
  eglTerminate( (EGLDisplay)aDisplay );

  /* access to EGL was done via GBM (graphics buffer management) */
  if ( GbmSurface )
  {
    if ( GbmBuffer )
      gbm_surface_release_buffer( GbmSurface, GbmBuffer );

    gbm_surface_destroy( GbmSurface );
  }

  if ( GbmDevice )
    gbm_device_destroy( GbmDevice );
}


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
void DrmEglSwapBuffers( void* aDisplay, void* aSurface )
{
  struct gbm_bo*  next_bo;
  int             waiting_for_flip = 1;
  int             ret;
  fd_set          fds;
  uint32_t        fb_id;
  drmEventContext evctx =
  {
    .version = 2,
    .page_flip_handler = DrmFbFlipCallback
  };

  /* perform the swap if there was something drawn on the screen */
  eglSwapBuffers( (EGLDisplay)aDisplay, (EGLSurface)aSurface );

  next_bo = gbm_surface_lock_front_buffer( GbmSurface );
  fb_id = DrmGetFb( next_bo );
  if ( !fb_id )
    return;

  ret = drmModePageFlip( DrmFd, DrmCrtcId, fb_id,
      DRM_MODE_PAGE_FLIP_EVENT, &waiting_for_flip );
  if ( ret )
    return;

  while ( waiting_for_flip )
  {
    FD_ZERO( &fds );
    FD_SET( 0, &fds );
    FD_SET( DrmFd, &fds );

    ret = select( DrmFd + 1, &fds, NULL, NULL, NULL );
    if ( ret <= 0 )
      break;
    else if ( FD_ISSET( DrmFd, &fds ) )
      drmHandleEvent( DrmFd, &evctx );
  }

  /* release last buffer to render on again */
  if ( GbmBuffer )
    gbm_surface_release_buffer( GbmSurface, GbmBuffer );
  GbmBuffer = next_bo;
}


/* mli, msy */
