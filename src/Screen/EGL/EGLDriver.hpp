/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2017 The XCSoar Project
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

#ifndef XCSOAR_SCREEN_EGL_DRIVER_HPP
#define XCSOAR_SCREEN_EGL_DRIVER_HPP

#include "System.hpp"

#if !defined(USE_X11) && !defined(USE_WAYLAND)

#include "Screen/Point.hpp"

#include <assert.h>

#ifdef USE_VIDEOCORE
#include <bcm_host.h>
#endif

#ifdef MESA_KMS
#include <drm_mode.h>
#endif

class EGLDriver final {
public:
  enum Status {
    UNINITIALISED,
    INITIALISED,
    CREATED_DISPLAY_AND_WINDOW,
    DESTROYED,
  };

private:
  Status current_status = Status::UNINITIALISED;

#ifdef USE_VIDEOCORE
  /* for Raspberry Pi */
  DISPMANX_DISPLAY_HANDLE_T vc_display;
  DISPMANX_UPDATE_HANDLE_T vc_update;
  DISPMANX_ELEMENT_HANDLE_T vc_element;
  EGL_DISPMANX_WINDOW_T vc_window;

  void InitVideoCore() noexcept;
#elif defined(HAVE_MALI)
  struct mali_native_window mali_native_window;
#elif defined(MESA_KMS)
  struct gbm_device *gbm_native_display;
  struct gbm_surface *gbm_native_window;

  int dri_fd = -1;

  struct gbm_bo *current_bo = nullptr;

  bool is_dri_master = false;
  uint32_t drm_connector_id;
  drm_mode_crtc drm_orig_crtc;

  bool CreateDRM(const char *dri_device) noexcept;
  void DestroyDRM() noexcept;
  void SpecificFlipDRM() noexcept;
#endif

public:
  EGLDriver() = default;
  EGLDriver(const EGLDriver&) = delete;
  EGLDriver &operator=(const EGLDriver&) = delete;

  ~EGLDriver() {
    assert(Status::DESTROYED == current_status);
  }

  void Init() noexcept {
#ifdef USE_VIDEOCORE
    InitVideoCore();
#endif
    current_status = Status::INITIALISED;
  }

  void CreateDisplayAndWindow(PixelSize new_size, bool full_screen,
                              bool resizable);
  void DestroyDisplayAndWindow() noexcept {
    assert(Status::CREATED_DISPLAY_AND_WINDOW == current_status);
#ifdef MESA_KMS
    DestroyDRM();
#endif
    current_status = Status::DESTROYED;
  }

  void SpecificFlip() noexcept {
#ifdef MESA_KMS
    SpecificFlipDRM();
#endif
  }

  EGLNativeDisplayType GetNativeDisplay() {
    assert(Status::CREATED_DISPLAY_AND_WINDOW == current_status);
#ifdef MESA_KMS
    return gbm_native_display;
#else
    return EGL_DEFAULT_DISPLAY;
#endif
  }

  EGLNativeWindowType GetNativeWindow() {
    assert(Status::CREATED_DISPLAY_AND_WINDOW == current_status);
#ifdef USE_VIDEOCORE
    return &vc_window;
#elif defined(HAVE_MALI)
    return &mali_native_window;
#elif defined(MESA_KMS)
    return gbm_native_window;
#endif
  }
};

extern EGLDriver *global_egl_driver;

#endif

#endif
