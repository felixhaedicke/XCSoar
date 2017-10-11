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

#ifdef DYNAMIC_EGL_PLATFORM_DRIVER

// === BEGIN type definitions for the VideoCore EGL platform
using DISPMANX_DISPLAY_HANDLE_T = uint32_t;
using DISPMANX_UPDATE_HANDLE_T = uint32_t;
using DISPMANX_ELEMENT_HANDLE_T = uint32_t;

struct EGL_DISPMANX_WINDOW_T {
   DISPMANX_ELEMENT_HANDLE_T element;
   int width, height;
};
// === END type definitions for the VideoCore EGL platform

// === BEGIN type definitions for the MALI EGL platform
struct mali_native_window {
	unsigned short width;
	unsigned short height;
};
// === END type definitions for the MALI EGL platform

#endif

#include <drm_mode.h>

class EGLDriver final {
public:
  enum class Platform {
    GBM,
    VIDEOCORE,
    MALI,
  };

  enum Status {
    UNINITIALISED,
    INITIALISED,
    CREATED_DISPLAY_AND_WINDOW,
    DESTROYED,
  };

private:
  Status current_status = Status::UNINITIALISED;

#ifdef DYNAMIC_EGL_PLATFORM_DRIVER
  Platform platform;

  // === BEGIN private members for the VideoCore EGL platform
  DISPMANX_DISPLAY_HANDLE_T vc_display;
  DISPMANX_UPDATE_HANDLE_T vc_update;
  DISPMANX_ELEMENT_HANDLE_T vc_element;
  EGL_DISPMANX_WINDOW_T vc_window;
  // === END private members for the VideoCore EGL platform

  // === BEGIN private members for the MALI EGL platform
  struct mali_native_window mali_native_window;
  // === END private members for the MALI EGL platform
#else
  static constexpr Platform platform = Platform::GBM;
#endif

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

public:
  EGLDriver() = default;
  EGLDriver(const EGLDriver&) = delete;
  EGLDriver &operator=(const EGLDriver&) = delete;

  ~EGLDriver() {
    assert(Status::DESTROYED == current_status);
  }

#ifdef DYNAMIC_EGL_PLATFORM_DRIVER
  void Init() noexcept;
#else
  void Init() noexcept {
    current_status = Status::INITIALISED;
  }
#endif

  void CreateDisplayAndWindow(PixelSize new_size, bool full_screen,
                              bool resizable);
  void DestroyDisplayAndWindow() noexcept {
    assert(Status::CREATED_DISPLAY_AND_WINDOW == current_status);
    if (Platform::GBM == platform)
      DestroyDRM();
    current_status = Status::DESTROYED;
  }

  void SpecificFlip() noexcept {
    if (Platform::GBM == platform)
      SpecificFlipDRM();
  }

  EGLNativeDisplayType GetNativeDisplay() {
    assert(Status::CREATED_DISPLAY_AND_WINDOW == current_status);
    if (Platform::GBM != platform)
      return EGL_DEFAULT_DISPLAY;
    else
      return gbm_native_display;
  }

  EGLNativeWindowType GetNativeWindow() {
    assert(Status::CREATED_DISPLAY_AND_WINDOW == current_status);
#ifdef DYNAMIC_EGL_PLATFORM_DRIVER
    switch (platform) {
    case Platform::GBM:
      static_assert(sizeof(EGLNativeWindowType) == sizeof(gbm_native_window));
      return reinterpret_cast<EGLNativeWindowType>(gbm_native_window);

    case Platform::VIDEOCORE:
      static_assert(sizeof(EGLNativeWindowType) == sizeof(&vc_window));
      return reinterpret_cast<EGLNativeWindowType>(&vc_window);

    case Platform::MALI:
      static_assert(sizeof(EGLNativeWindowType) == sizeof(&mali_native_window));
      return reinterpret_cast<EGLNativeWindowType>(&mali_native_window);
    }
#else
    return gbm_native_window;
#endif
  }

#ifdef DYNAMIC_EGL_PLATFORM_DRIVER
  Platform GetPlatform() const {
    assert((Status::INITIALISED == current_status) ||
        (Status::CREATED_DISPLAY_AND_WINDOW == current_status));
    return platform;
  }
#endif

  PixelSize GetDisplaySize();
};

extern EGLDriver *global_egl_driver;

#endif

#endif
