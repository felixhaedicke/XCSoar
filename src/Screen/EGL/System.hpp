/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2016 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#ifndef XCSOAR_SCREEN_EGL_SYSTEM_HPP
#define XCSOAR_SCREEN_EGL_SYSTEM_HPP

#ifdef USE_WAYLAND
#include <wayland-egl.h>
#endif

#ifdef USE_X11
/* kludges to work around namespace collisions with X11 headers */
#define Font X11Font
#define Window X11Window
#define Display X11Display
#endif

#ifdef USE_CONSOLE

#ifdef DYNAMIC_EGL_PLATFORM_DRIVER
/* For ARM (currently not including AARCH64 builds), we support EGL platforms
 * which use a proprietary plaform API instead of Mesa's GBM. This includes
 * Mali and Raspbery Pi's VideoCore legacy driver.
 * On these platforms, required userspace libraries for preparing the EGL
 * are loaded dynamically using dlopen().
 * For all other target platforms, there is currently only the Mesa GBM based
 * driver, which is linked in a traditional way.
*/

/* This allows us to use the Mesa EGL headers for all console based EGL
 * implmentations, even for non-Mesa drivers. */
#define MESA_EGL_NO_X11_HEADERS

#else
#include <gbm.h>
#endif

#endif

#include <EGL/egl.h>

#ifdef USE_X11
#undef Font
#undef Window
#undef Display

#ifdef Expose
#undef Expose
#endif

#ifdef NoValue
#undef NoValue
#endif

#ifdef None
#undef None
#endif
#endif

#endif
