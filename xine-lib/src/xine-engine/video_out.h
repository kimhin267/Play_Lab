/*
 * Copyright (C) 2000-2004 the xine project
 *
 * This file is part of xine, a free video player.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 * xine version of video_out.h
 *
 * vo_frame    : frame containing yuv data and timing info,
 *               transferred between video_decoder and video_output
 *
 * vo_driver   : lowlevel, platform-specific video output code
 *
 * vo_port     : generic frame_handling code, uses
 *               a vo_driver for output
 */

#ifndef HAVE_VIDEO_OUT_H
#define HAVE_VIDEO_OUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>

#ifdef XINE_COMPILE
#  include "xine.h"
#  include "buffer.h"
#else
#  include <xine.h>
#  include <xine/buffer.h>
#endif


typedef struct vo_frame_s vo_frame_t;
typedef struct vo_driver_s vo_driver_t;
typedef struct video_driver_class_s video_driver_class_t;
typedef struct vo_overlay_s vo_overlay_t;
typedef struct video_overlay_manager_s video_overlay_manager_t;

/* public part, video drivers may add private fields
 *
 * Remember that adding new functions to this structure requires
 * adaption of the post plugin decoration layer. Be sure to look into
 * src/xine-engine/post.[ch].
 */
struct vo_frame_s {
  /*
   * member functions
   */

  /* Duplicate picture data and acceleration specific data of a frame. */
  /* if the image format isn't already known by Xine. Currently this is needed */
  /* For all image formats except XINE_IMGFMT_YV12 and XINE_IMGFMT_YUY2 */
  void (*proc_duplicate_frame_data) (vo_frame_t *vo_img, vo_frame_t *src);

  /* tell video driver to copy/convert the whole of this frame, may be NULL */
  /* at least one of proc_frame() and proc_slice() MUST set the variable proc_called to 1 */
  void (*proc_frame) (vo_frame_t *vo_img);

  /* tell video driver to copy/convert a slice of this frame, may be NULL */
  /* at least one of proc_frame() and proc_slice() MUST set the variable proc_called to 1 */
  void (*proc_slice) (vo_frame_t *vo_img, uint8_t **src);

  /* tell video driver that the decoder starts a new field */
  void (*field) (vo_frame_t *vo_img, int which_field);

  /* append this frame to the display queue,
     returns number of frames to skip if decoder is late */
  /* when the frame does not originate from a stream, it is legal to pass an anonymous stream */
  int (*draw) (vo_frame_t *vo_img, xine_stream_t *stream);

  /* lock frame as reference, must be paired with free.
   * most decoders/drivers do not need to call this function since
   * newly allocated frames are already locked once.
   */
  void (*lock) (vo_frame_t *vo_img);

  /* this frame is no longer used by the decoder, video driver, etc */
  void (*free) (vo_frame_t *vo_img);

  /* free memory/resources for this frame */
  void (*dispose) (vo_frame_t *vo_img);

  /*
   * public variables to decoders and vo drivers
   * changing anything here will require recompiling them both
   */
  int64_t                    pts;           /* presentation time stamp (1/90000 sec)        */
  int64_t                    vpts;          /* virtual pts, generated by metronom           */
  int                        bad_frame;     /* e.g. frame skipped or based on skipped frame */
  int                        duration;      /* frame length in time, in 1/90000 sec         */

  /* yv12 (planar)       base[0]: y,       base[1]: u,  base[2]: v  */
  /* yuy2 (interleaved)  base[0]: yuyv..., base[1]: --, base[2]: -- */
  uint8_t                   *base[3];
  int                        pitches[3];

  /* info that can be used for interlaced output (e.g. tv-out)      */
  int                        top_field_first;
  int                        repeat_first_field;
  /* note: progressive_frame is set wrong on many mpeg2 streams. for
   * that reason, this flag should be interpreted as a "hint".
   */
  int                        progressive_frame;
  int                        picture_coding_type;

  /* cropping to be done */
  int                        crop_left, crop_right, crop_top, crop_bottom;

  /* extra info coming from input or demuxers */
  extra_info_t              *extra_info;

  /* additional information to be able to duplicate frames:         */
  int                        width, height;
  double                     ratio;         /* aspect ratio  */
  int                        format;        /* IMGFMT_YV12 or IMGFMT_YUY2                     */

  int                        drawn;         /* used by decoder, frame has already been drawn */
  int                        flags;         /* remember the frame flags */
  int                        proc_called;   /* track use of proc_*() methods */

  /* Used to carry private data for accelerated plugins.*/
  void                      *accel_data;

  /* "backward" references to where this frame originates from */
  xine_video_port_t         *port;
  vo_driver_t               *driver;
  xine_stream_t             *stream;

  /* displacement for overlays */
  int                       overlay_offset_x, overlay_offset_y;

  /*
   * that part is used only by video_out.c for frame management
   * obs: changing anything here will require recompiling vo drivers
   */
  struct vo_frame_s         *next;
  int                        lock_counter;
  pthread_mutex_t            mutex; /* protect access to lock_count */

  int                        id; /* debugging - track this frame */
  int                        is_first;
};


/*
 * Remember that adding new functions to this structure requires
 * adaption of the post plugin decoration layer. Be sure to look into
 * src/xine-engine/post.[ch].
 */
struct xine_video_port_s {

  uint32_t (*get_capabilities) (xine_video_port_t *self); /* for constants see below */

  /* open display driver for video output */
  /* when you are not a full-blown stream, but still need to open the port
   * (e.g. you are a post plugin) it is legal to pass an anonymous stream */
  void (*open) (xine_video_port_t *self, xine_stream_t *stream);

  /*
   * get_frame - allocate an image buffer from display driver
   *
   * params : width      == width of video to display.
   *          height     == height of video to display.
   *          ratio      == aspect ration information
   *          format     == FOURCC descriptor of image format
   *          flags      == field/prediction flags
   */
  vo_frame_t* (*get_frame) (xine_video_port_t *self, uint32_t width,
			    uint32_t height, double ratio,
			    int format, int flags);

  /* retrieves the last displayed frame (useful for taking snapshots) */
  vo_frame_t* (*get_last_frame) (xine_video_port_t *self);

  /* overlay stuff */
  void (*enable_ovl) (xine_video_port_t *self, int ovl_enable);

  /* get overlay manager */
  video_overlay_manager_t* (*get_overlay_manager) (xine_video_port_t *self);

  /* flush video_out fifo */
  void (*flush) (xine_video_port_t *self);

  /* Get/Set video property
   *
   * See VO_PROP_* bellow
   */
  int (*get_property) (xine_video_port_t *self, int property);
  int (*set_property) (xine_video_port_t *self, int property, int value);

  /* return true if port is opened for this stream, stream can be anonymous */
  int (*status) (xine_video_port_t *self, xine_stream_t *stream,
                 int *width, int *height, int64_t *img_duration);

  /* video driver is no longer used by decoder => close */
  /* when you are not a full-blown stream, but still need to close the port
   * (e.g. you are a post plugin) it is legal to pass an anonymous stream */
  void (*close) (xine_video_port_t *self, xine_stream_t *stream);

  /* called on xine exit */
  void (*exit) (xine_video_port_t *self);

  /* the driver in use */
  vo_driver_t *driver;

};

/* constants for the get/set property functions */
#define VO_PROP_INTERLACED            0
#define VO_PROP_ASPECT_RATIO          1
#define VO_PROP_HUE                   2
#define VO_PROP_SATURATION            3
#define VO_PROP_CONTRAST              4
#define VO_PROP_BRIGHTNESS            5
#define VO_PROP_COLORKEY              6
#define VO_PROP_AUTOPAINT_COLORKEY    7
#define VO_PROP_ZOOM_X                8
#define VO_PROP_PAN_SCAN              9
#define VO_PROP_TVMODE		      10
#define VO_PROP_MAX_NUM_FRAMES        11
#define VO_PROP_GAMMA		      12
#define VO_PROP_ZOOM_Y                13
#define VO_PROP_DISCARD_FRAMES        14 /* not used by drivers */
#define VO_PROP_WINDOW_WIDTH          15 /* read-only */
#define VO_PROP_WINDOW_HEIGHT         16 /* read-only */
#define VO_PROP_BUFS_IN_FIFO          17 /* read-only */
#define VO_PROP_NUM_STREAMS           18 /* read-only */
#define VO_PROP_OUTPUT_WIDTH          19 /* read-only */
#define VO_PROP_OUTPUT_HEIGHT         20 /* read-only */
#define VO_PROP_OUTPUT_XOFFSET        21 /* read-only */
#define VO_PROP_OUTPUT_YOFFSET        22 /* read-only */
#define VO_NUM_PROPERTIES             23

/* number of colors in the overlay palette. Currently limited to 256
   at most, because some alphablend functions use an 8-bit index into
   the palette. This should probably be classified as a bug. */
#define OVL_PALETTE_SIZE 256

#define OVL_MAX_OPACITY  0x0f

/* number of recent frames to keep in memory
   these frames are needed by some deinterlace algorithms
   FIXME: we need a method to flush the recent frames (new stream)
*/
#define VO_NUM_RECENT_FRAMES     2

/* get_frame flags */
#define VO_TOP_FIELD         1
#define VO_BOTTOM_FIELD      2
#define VO_BOTH_FIELDS       (VO_TOP_FIELD | VO_BOTTOM_FIELD)
#define VO_PAN_SCAN_FLAG     4
#define VO_INTERLACED_FLAG   8
#define VO_NEW_SEQUENCE_FLAG 16 /* set after MPEG2 Sequence Header Code (used by XvMC) */

/* video driver capabilities */
#define VO_CAP_YV12                   0x00000001 /* driver can handle YUV 4:2:0 pictures */
#define VO_CAP_YUY2                   0x00000002 /* driver can handle YUY2 pictures */
#define VO_CAP_XVMC_MOCOMP            0x00000004 /* driver can use XvMC motion compensation */
#define VO_CAP_XVMC_IDCT              0x00000008 /* driver can use XvMC idct acceleration   */
#define VO_CAP_UNSCALED_OVERLAY       0x00000010 /* driver can blend overlay at output resolution */
#define VO_CAP_CROP                   0x00000020 /* driver can crop */
#define VO_CAP_XXMC                   0x00000040 /* driver can use extended XvMC */


/*
 * vo_driver_s contains the functions every display driver
 * has to implement. The vo_new_port function (see below)
 * should then be used to construct a vo_port using this
 * driver. Some of the function pointers will be copied
 * directly into xine_video_port_s, others will be called
 * from generic vo functions.
 */

#define VIDEO_OUT_DRIVER_IFACE_VERSION  21

struct vo_driver_s {

  uint32_t (*get_capabilities) (vo_driver_t *self); /* for constants see above */

  /*
   * allocate an vo_frame_t struct,
   * the driver must supply the copy, field and dispose functions
   */
  vo_frame_t* (*alloc_frame) (vo_driver_t *self);

  /*
   * check if the given image fullfills the format specified
   * (re-)allocate memory if necessary
   */
  void (*update_frame_format) (vo_driver_t *self, vo_frame_t *img,
			       uint32_t width, uint32_t height,
			       double ratio, int format, int flags);

  /* display a given frame */
  void (*display_frame) (vo_driver_t *self, vo_frame_t *vo_img);

  /* overlay_begin and overlay_end are used by drivers suporting
   * persistent overlays. they can be optimized to update only when
   * overlay image has changed.
   *
   * sequence of operation (pseudo-code):
   *   overlay_begin(this,img,true_if_something_changed_since_last_blend );
   *   while(visible_overlays)
   *     overlay_blend(this,img,overlay[i]);
   *   overlay_end(this,img);
   *
   * any function pointer from this group may be set to NULL.
   */
  void (*overlay_begin) (vo_driver_t *self, vo_frame_t *vo_img, int changed);
  void (*overlay_blend) (vo_driver_t *self, vo_frame_t *vo_img, vo_overlay_t *overlay);
  void (*overlay_end)   (vo_driver_t *self, vo_frame_t *vo_img);

  /*
   * these can be used by the gui directly:
   */
  int (*get_property) (vo_driver_t *self, int property);
  int (*set_property) (vo_driver_t *self,
		       int property, int value);
  void (*get_property_min_max) (vo_driver_t *self,
				int property, int *min, int *max);

  /*
   * general purpose communication channel between gui and driver
   *
   * this should be used to propagate events, display data, window sizes
   * etc. to the driver
   */
  int (*gui_data_exchange) (vo_driver_t *self, int data_type,
			    void *data);

  /* check if a redraw is needed (due to resize)
   * this is only used for still frames, normal video playback
   * must call that inside display_frame() function.
   */
  int (*redraw_needed) (vo_driver_t *self);

  /*
   * free all resources, close driver
   */
  void (*dispose) (vo_driver_t *self);

  void *node; /* needed by plugin_loader */
};

struct video_driver_class_s {

  /*
   * open a new instance of this plugin class
   */
  vo_driver_t* (*open_plugin) (video_driver_class_t *self, const void *visual);

  /*
   * return short, human readable identifier for this plugin class
   */
  char* (*get_identifier) (video_driver_class_t *self);

  /*
   * return human readable (verbose = 1 line) description for
   * this plugin class
   */
  char* (*get_description) (video_driver_class_t *self);

  /*
   * free all class-related resources
   */
  void (*dispose) (video_driver_class_t *self);
};


typedef struct rle_elem_s {
  uint16_t len;
  uint16_t color;
} rle_elem_t;

struct vo_overlay_s {

  rle_elem_t       *rle;           /* rle code buffer                  */
  int               data_size;     /* useful for deciding realloc      */
  int               num_rle;       /* number of active rle codes       */
  int               x;             /* x start of subpicture area       */
  int               y;             /* y start of subpicture area       */
  int               width;         /* width of subpicture area         */
  int               height;        /* height of subpicture area        */

  uint32_t          color[OVL_PALETTE_SIZE];  /* color lookup table     */
  uint8_t           trans[OVL_PALETTE_SIZE];  /* mixer key table        */
  int               rgb_clut;      /* true if clut was converted to rgb */

  /* define a highlight area with different colors */
  int               hili_top;
  int               hili_bottom;
  int               hili_left;
  int               hili_right;
  uint32_t          hili_color[OVL_PALETTE_SIZE];
  uint8_t           hili_trans[OVL_PALETTE_SIZE];
  int               hili_rgb_clut; /* true if clut was converted to rgb */

  int               unscaled;      /* true if it should be blended unscaled */
};


/* API to video_overlay manager
 *
 * Remember that adding new functions to this structure requires
 * adaption of the post plugin decoration layer. Be sure to look into
 * src/xine-engine/post.[ch].
 */
struct video_overlay_manager_s {
  void (*init) (video_overlay_manager_t *this_gen);

  void (*dispose) (video_overlay_manager_t *this_gen);

  int32_t (*get_handle) (video_overlay_manager_t *this_gen, int object_type );

  void (*free_handle) (video_overlay_manager_t *this_gen, int32_t handle);

  int32_t (*add_event) (video_overlay_manager_t *this_gen, void *event);

  void (*flush_events) (video_overlay_manager_t *this_gen );

  int (*redraw_needed) (video_overlay_manager_t *this_gen, int64_t vpts );

  void (*multiple_overlay_blend) (video_overlay_manager_t *this_gen, int64_t vpts,
                                  vo_driver_t *output, vo_frame_t *vo_img, int enabled);
};

/*
 * build a video_out_port from
 * a given video driver
 */
xine_video_port_t *_x_vo_new_port (xine_t *xine, vo_driver_t *driver, int grabonly) XINE_MALLOC XINE_PROTECTED;

#ifdef __cplusplus
}
#endif

#endif
