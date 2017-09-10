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

#if !defined(USE_X11) && !defined(USE_WAYLAND)

#include "EGLDriver.hpp"

#include <stdio.h>

#ifdef MESA_KMS
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <boost/assert.hpp>

#include <limits>
#include <memory>
#include <set>

#include "OS/FileUtil.hpp"
#include "OS/Path.hpp"
#include "Util/AllocatedArray.hxx"
#include "Util/Macros.hpp"
#include "Util/NumberParser.hpp"
#include "Util/StringFormat.hpp"

struct drm_fb {
  struct gbm_bo *bo;
  uint32_t fb_id;
  int dri_fd;
};

#define DRI_DEV_PATH "/dev/dri"
#define DRI_CARD_DEVNAME_PREFIX "card"
#endif

void
EGLDriver::CreateDisplayAndWindow(PixelSize new_size, bool full_screen,
                                  bool resizable)
{
  assert(Status::INITIALISED == current_status);

#ifdef MESA_KMS

  std::set<unsigned> dri_card_numbers;
  const char* dri_device_env = getenv("DRI_DEVICE");
  if ((nullptr == dri_device_env) || (*dri_device_env == '\0')) {
    class Visitor : public File::Visitor {
      std::set<unsigned> &dri_card_numbers;

    public:
      explicit Visitor(std::set<unsigned> &_dri_card_numbers)
        : dri_card_numbers(_dri_card_numbers) {}

      void Visit(Path path, Path filename) override {
        unsigned no = ParseUnsigned(filename.c_str() + 4);
        if (std::numeric_limits<unsigned>::max() != no)
          dri_card_numbers.insert(ParseUnsigned(filename.c_str() + 4));
      }
    } visitor(dri_card_numbers);

    Directory::VisitSpecificFiles(Path(DRI_DEV_PATH),
                                  DRI_CARD_DEVNAME_PREFIX "*",
                                  visitor, false, File::TYPE_CHARACTER_DEV);

    char dri_path_buffer[64];
    bool successful = false;
    for (unsigned dri_card_number : dri_card_numbers) {
      StringFormat(dri_path_buffer, ARRAY_SIZE(dri_path_buffer),
                   DRI_DEV_PATH "/" DRI_CARD_DEVNAME_PREFIX "%u",
                   dri_card_number);
      printf("Trying to initalise DRI interface %s\n", dri_path_buffer);
      if (CreateDRM(dri_path_buffer)) {
        successful = true;
        break;
      } else
        DestroyDRM();
    }
    if (!successful) {
      fprintf(stderr, "No usable DRI interface found\n");
      exit(EXIT_FAILURE);
    }
  } else {
    if (!CreateDRM(dri_device_env))
      exit(EXIT_FAILURE);
  }

#elif defined(USE_VIDEOCORE)

  vc_display = vc_dispmanx_display_open(0);
  vc_update = vc_dispmanx_update_start(0);

  VC_RECT_T dst_rect;
  dst_rect.x = dst_rect.y = 0;
  dst_rect.width = new_size.cx;
  dst_rect.height = new_size.cy;

  VC_RECT_T src_rect = dst_rect;
  src_rect.x = src_rect.y = 0;
  src_rect.width = new_size.cx << 16;
  src_rect.height = new_size.cy << 16;

  vc_element = vc_dispmanx_element_add(vc_update, vc_display,
                                       0, &dst_rect, 0, &src_rect,
                                       DISPMANX_PROTECTION_NONE,
                                       0, 0,
                                       DISPMANX_NO_ROTATE);
  vc_dispmanx_update_submit_sync(vc_update);

  vc_window.element = vc_element;
  vc_window.width = new_size.cx;
  vc_window.height = new_size.cy;

#elif defined(HAVE_MALI)

  mali_native_window.width = new_size.cx;
  mali_native_window.height = new_size.cy;

#endif

  current_status = Status::CREATED_DISPLAY_AND_WINDOW;
}

#ifdef USE_VIDEOCORE

void
EGLDriver::InitVideoCore() noexcept
{
  bcm_host_init();
}

#elif defined(MESA_KMS)

bool
EGLDriver::CreateDRM(const char *dri_device) noexcept
{
  dri_fd = open(dri_device, O_RDWR);
  if (dri_fd == -1) {
    fprintf(stderr, "Could not open DRI device %s: %s\n", dri_device,
            strerror(errno));
    return false;
  }

  if (0 != ioctl(dri_fd, DRM_IOCTL_SET_MASTER, 0)) {
    perror("Could not become DRI device master");
    return false;
  }
  is_dri_master = true;

  drm_mode_card_res res = {};
  std::unique_ptr<uint32_t[]> fb_ids;
  std::unique_ptr<uint32_t[]> crtc_ids;
  std::unique_ptr<uint32_t[]> connector_ids;
  std::unique_ptr<uint32_t[]> encoder_ids;
  bool get_res_success =
      (0 == ioctl(dri_fd, DRM_IOCTL_MODE_GETRESOURCES, &res));
  if (get_res_success) {
    fb_ids = std::unique_ptr<uint32_t[]>(new uint32_t[res.count_fbs]);
    crtc_ids = std::unique_ptr<uint32_t[]>(new uint32_t[res.count_crtcs]);
    connector_ids =
        std::unique_ptr<uint32_t[]>(new uint32_t[res.count_connectors]);
    encoder_ids = std::unique_ptr<uint32_t[]>(new uint32_t[res.count_encoders]);
    res.fb_id_ptr = reinterpret_cast<uintptr_t>(fb_ids.get());
    res.crtc_id_ptr = reinterpret_cast<uintptr_t>(crtc_ids.get());
    res.connector_id_ptr = reinterpret_cast<uintptr_t>(connector_ids.get());
    res.encoder_id_ptr = reinterpret_cast<uintptr_t>(encoder_ids.get());
    get_res_success = (0 == ioctl(dri_fd, DRM_IOCTL_MODE_GETRESOURCES, &res));
  }
  if (!get_res_success) {
    perror("Could not get DRM resources");
    return false;
  }

  assert(0 == drm_connector_id);
  assert(0 == drm_orig_crtc.crtc_id);
  for (uint32_t i = 0;
       (i < res.count_connectors) && (0 == drm_orig_crtc.crtc_id);
       ++i) {
    drm_mode_get_connector conn = {};
    conn.connector_id = connector_ids[i];
    if ((0 == ioctl(dri_fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn)) &&
        (0 != conn.connection) && (0 != conn.encoder_id)) {
      drm_mode_get_encoder enc = {};
      enc.encoder_id = conn.encoder_id;
      if ((0 == ioctl(dri_fd, DRM_IOCTL_MODE_GETENCODER, &enc)) &&
          (0 != enc.crtc_id)) {
        drm_mode_crtc crtc = {};
        crtc.crtc_id = enc.crtc_id;
        if ((0 == ioctl(dri_fd, DRM_IOCTL_MODE_GETCRTC, &crtc)) &&
            (crtc.mode.hdisplay > 0) && (crtc.mode.vdisplay > 0)) {
          drm_connector_id = connector_ids[i];
          drm_orig_crtc = crtc;
        }
      }
    }
  }

  if (0 == drm_orig_crtc.crtc_id) {
    fprintf(stderr, "No usable DRM connector / encoder / CRTC found\n");
    return false;
  }

  gbm_native_display = gbm_create_device(dri_fd);
  if (gbm_native_display == nullptr) {
    fprintf(stderr, "Could not create GBM device\n");
    return false;
  }

  gbm_native_window = gbm_surface_create(
      gbm_native_display,
      drm_orig_crtc.mode.hdisplay, drm_orig_crtc.mode.vdisplay,
      GBM_FORMAT_XRGB8888,
      GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
  if (gbm_native_window == nullptr) {
    fprintf(stderr, "Could not create GBM surface\n");
    return false;
  }

  return true;
}

void
EGLDriver::DestroyDRM() noexcept
{
  if (0 != drm_orig_crtc.crtc_id) {
    drm_orig_crtc.set_connectors_ptr =
        reinterpret_cast<uintptr_t>(&drm_connector_id);
    drm_orig_crtc.count_connectors = 1;
    BOOST_VERIFY(0 == ioctl(dri_fd, DRM_IOCTL_MODE_SETCRTC, &drm_orig_crtc));
    drm_orig_crtc.crtc_id = 0;
  }

  if (nullptr != gbm_native_window) {
    gbm_surface_destroy(gbm_native_window);
    gbm_native_window = nullptr;
  }

  if (nullptr != gbm_native_display) {
    gbm_device_destroy(gbm_native_display);
    gbm_native_display = nullptr;
  }

  if (-1 != dri_fd) {
    if (is_dri_master) {
      BOOST_VERIFY(0 == ioctl(dri_fd, DRM_IOCTL_DROP_MASTER, 0));
      is_dri_master = false;
    }
    close(dri_fd);
    dri_fd = -1;
  }
}

void EGLDriver::SpecificFlipDRM() noexcept
{
  gbm_bo *new_bo = gbm_surface_lock_front_buffer(gbm_native_window);

  drm_fb *fb = (drm_fb*) gbm_bo_get_user_data(new_bo);
  if (!fb) {
    fb = new drm_fb;
    fb->bo = new_bo;
    fb->dri_fd = dri_fd;

    drm_mode_fb_cmd add_fb_cmd;
    add_fb_cmd.width = drm_orig_crtc.mode.hdisplay;
    add_fb_cmd.height = drm_orig_crtc.mode.vdisplay;
    add_fb_cmd.pitch = 4 * drm_orig_crtc.mode.hdisplay;
    add_fb_cmd.bpp = 32;
    add_fb_cmd.depth = 24;
    add_fb_cmd.handle = gbm_bo_get_handle(new_bo).u32;
    if (0 == ioctl(dri_fd, DRM_IOCTL_MODE_ADDFB, &add_fb_cmd))
      fb->fb_id = add_fb_cmd.fb_id;
    else {
      perror("DRM mode add FB failed");
      exit(EXIT_FAILURE);
    }
  }

  gbm_bo_set_user_data(new_bo,
                       fb,
                       [](struct gbm_bo *bo, void *data) {
    struct drm_fb *fb = (struct drm_fb*) data;
    if (fb->fb_id) {
      BOOST_VERIFY(0 == ioctl(fb->dri_fd, DRM_IOCTL_MODE_RMFB, &fb->fb_id));
    }

    delete fb;
  });

  if (nullptr == current_bo) {
    drm_mode_crtc fb_crtc = drm_orig_crtc;
    fb_crtc.fb_id = fb->fb_id;
    fb_crtc.set_connectors_ptr = reinterpret_cast<uintptr_t>(&drm_connector_id);
    fb_crtc.count_connectors = 1;
    if (0 != ioctl(dri_fd, DRM_IOCTL_MODE_SETCRTC, &fb_crtc)) {
      perror("DRM mode set CRTC failed");
      exit(EXIT_FAILURE);
    }
    assert(drm_orig_crtc.crtc_id == fb_crtc.crtc_id);
  }

  drm_mode_crtc_page_flip flip_cmd;
  flip_cmd.crtc_id = drm_orig_crtc.crtc_id;
  flip_cmd.fb_id = fb->fb_id;
  flip_cmd.flags = DRM_MODE_PAGE_FLIP_EVENT;
  flip_cmd.reserved = 0;
  flip_cmd.user_data = 0;
  if (0 != ioctl(dri_fd, DRM_IOCTL_MODE_PAGE_FLIP, &flip_cmd)) {
    perror("DRM mode page flip failed");
    exit(EXIT_FAILURE);
  }

  for (bool flip_finished = false; !flip_finished;) {
    char ev_buffer[64];
    int read_ret = read(dri_fd, &ev_buffer, sizeof(ev_buffer));
    if (-1 == read_ret) {
      perror("Could not read DRI events");
      exit(EXIT_FAILURE);
    }
    assert(0 != read_ret);
    for (int i = 0; i < read_ret;) {
      drm_event ev;
      memcpy(&ev, ev_buffer + i, sizeof(ev));
      if (DRM_EVENT_FLIP_COMPLETE == ev.type) {
        flip_finished = true;
      }
      i += ev.length;
    }
  }

  if (nullptr != current_bo)
    gbm_surface_release_buffer(gbm_native_window, current_bo);

  current_bo = new_bo;
}

#endif

#endif
