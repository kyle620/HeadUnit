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
*   This file contains basic defines and useful configuration macros.
*
*******************************************************************************/

#ifndef EWDEF_H
#define EWDEF_H

/* defines for different framebuffer color formats */
#define EW_FRAME_BUFFER_COLOR_FORMAT_Index8   1
#define EW_FRAME_BUFFER_COLOR_FORMAT_LumA44   2
#define EW_FRAME_BUFFER_COLOR_FORMAT_RGB565   3
#define EW_FRAME_BUFFER_COLOR_FORMAT_RGB888   4
#define EW_FRAME_BUFFER_COLOR_FORMAT_RGBA4444 5
#define EW_FRAME_BUFFER_COLOR_FORMAT_RGBA8888 6

#define EW_STRINGIZE( aArg )      EW_STRINGIZE_ARG( aArg )
#define EW_STRINGIZE_ARG( aArg )  #aArg

#endif

/* mli, msy */
