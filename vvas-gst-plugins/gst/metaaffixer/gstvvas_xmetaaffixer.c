/*
 * Copyright (C) 2020 - 2021 Xilinx, Inc.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software
 * is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
 * KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO
 * EVENT SHALL XILINX BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
 * OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. Except as contained in this notice, the name of the Xilinx shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization from Xilinx.
 */

/**
 * SECTION:element-vvas_xmetaaffixer
 *
 * FIXME:Describe vvas_xmetaaffixer here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! vvas_xmetaaffixer ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <gst/gst.h>

#include "gstvvas_xmetaaffixer.h"

GST_DEBUG_CATEGORY_STATIC (gst_vvas_xmetaaffixer_debug);
#define GST_CAT_DEFAULT gst_vvas_xmetaaffixer_debug

/* Enable this flag, ENABLE_TEST_CODE,  if you want t create and attach Meta Data in caes 
 * master buffer doesn't have meta data. This is just for testing entire
 * flow inside plugin.
 */
#define ENABLE_TEST_CODE 0

GQuark _scale_quark;

enum
{
  PROP_0,
  PROP_SYNC,
  PROP_TIMEOUT,
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */

static GstStaticPadTemplate master_sink_factory =
GST_STATIC_PAD_TEMPLATE ("sink_master",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (GST_VIDEO_FORMATS_ALL))
    );

static GstStaticPadTemplate slave_sink_factory =
GST_STATIC_PAD_TEMPLATE ("sink_slave_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (GST_VIDEO_FORMATS_ALL))
    );

/* src pad templates */
static GstStaticPadTemplate master_src_factory =
GST_STATIC_PAD_TEMPLATE ("src_master",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (GST_VIDEO_FORMATS_ALL))
    );

static GstStaticPadTemplate slave_src_factory =
GST_STATIC_PAD_TEMPLATE ("src_slave_%u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (GST_VIDEO_FORMATS_ALL))
    );


#define gst_vvas_xmetaaffixer_parent_class parent_class
G_DEFINE_TYPE (GstVvas_XMetaAffixer, gst_vvas_xmetaaffixer, GST_TYPE_ELEMENT);

G_DEFINE_TYPE (GstVvas_XMetaAffixerPad, gst_vvas_xmetaaffixer_pad,
    GST_TYPE_PAD);

static GstPad *gst_vvas_xmetaaffixer_request_new_pad (GstElement * element,
    GstPadTemplate * sink_templ, const gchar * name, const GstCaps * caps);

static void
gst_vvas_xmetaaffixer_release_pad (GstElement * element, GstPad * pad);

static GstStateChangeReturn
gst_vvas_xmetaaffixer_change_state (GstElement * element,
    GstStateChange transition);

static void gst_vvas_xmetaaffixer_finalize (GObject * obj);
static GstFlowReturn
gst_vvas_xmetaaffixer_collected (GstCollectPads * pads, gpointer user_data);
GstFlowReturn vvas_xmetaaffixer_combined_return (GstVvas_XMetaAffixer * self);

static void
gst_vvas_xmetaaffixer_pad_class_init (GstVvas_XMetaAffixerPadClass * klass)
{
}

static void
gst_vvas_xmetaaffixer_pad_init (GstVvas_XMetaAffixerPad * pad)
{
  pad->collect = NULL;
  pad->srcpad = NULL;
  pad->width = 0;
  pad->height = 0;
  pad->duration = GST_CLOCK_TIME_NONE;
  pad->curr_pts = GST_CLOCK_TIME_NONE;
  pad->eos_received = FALSE;
}

static void gst_vvas_xmetaaffixer_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_vvas_xmetaaffixer_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_vvas_xmetaaffixer_sink_event (GstCollectPads * pads,
    GstCollectData * cdata, GstEvent * event, GstVvas_XMetaAffixer * self);

/* GObject vmethod implementations */

/* initialize the vvas_xmetaaffixer's class */
static void
gst_vvas_xmetaaffixer_class_init (GstVvas_XMetaAffixerClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_vvas_xmetaaffixer_set_property;
  gobject_class->get_property = gst_vvas_xmetaaffixer_get_property;
  gobject_class->finalize = gst_vvas_xmetaaffixer_finalize;

  gst_element_class_set_details_simple (gstelement_class,
      "Xilinx VVAS Metaaffixer Plugin",
      "Filter/Effect/Video",
      "Scale Meta data as per the resolution",
      "Xilinx Inc <www.xilinx.com>");

  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_vvas_xmetaaffixer_request_new_pad);
  gstelement_class->release_pad =
      GST_DEBUG_FUNCPTR (gst_vvas_xmetaaffixer_release_pad);
  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_vvas_xmetaaffixer_change_state);

  /* SINK pad template */
  gst_element_class_add_static_pad_template (gstelement_class,
      &master_sink_factory);
  gst_element_class_add_static_pad_template (gstelement_class,
      &slave_sink_factory);

  /* SRC pad template */
  gst_element_class_add_static_pad_template (gstelement_class,
      &master_src_factory);
  gst_element_class_add_static_pad_template (gstelement_class,
      &slave_src_factory);

  g_object_class_install_property (gobject_class, PROP_SYNC,
      g_param_spec_boolean ("sync", "Sync",
          "Sync buffers on slave pads with buffers on master pad", TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TIMEOUT,
      g_param_spec_int64 ("timeout", "Timeout for collect pads",
          "Timeout in millisec \
          (-1) Disable timeout \
          Increase the timeout if debug logs are enabled", -1, G_MAXINT64, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  _scale_quark = gst_video_meta_transform_scale_get_quark ();
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_vvas_xmetaaffixer_init (GstVvas_XMetaAffixer * self)
{
  int i;

  for (i = 0; i < MAX_SLAVE_SOURCES; i++)
    self->sink_slave[i] = NULL;

  self->sink_master = NULL;
  self->num_slaves = 0;
  self->collect = gst_collect_pads_new ();
  self->sync = TRUE;
  self->flowcombiner = gst_flow_combiner_new ();
  self->prev_m_end_ts = GST_CLOCK_TIME_NONE;
  self->prev_meta_buf = NULL;
  self->timeout_thread = NULL;
  self->retry_timeout = 2000;
  self->timeout_issued = FALSE;
  self->start_thread = FALSE;
  self->stop_thread = FALSE;
  g_cond_init (&self->timeout_cond);
  g_mutex_init (&self->timeout_lock);
  g_mutex_init (&self->collected_lock);

  gst_collect_pads_set_function (self->collect, gst_vvas_xmetaaffixer_collected,
      self);

  gst_collect_pads_set_event_function (self->collect,
      (GstCollectPadsEventFunction) gst_vvas_xmetaaffixer_sink_event, self);
}

static void
gst_vvas_xmetaaffixer_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstVvas_XMetaAffixer *self = GST_VVAS_XMETAAFFIXER (object);

  switch (prop_id) {
    case PROP_SYNC:
      self->sync = g_value_get_boolean (value);
      break;
    case PROP_TIMEOUT:
      self->retry_timeout = g_value_get_int64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_vvas_xmetaaffixer_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstVvas_XMetaAffixer *self = GST_VVAS_XMETAAFFIXER (object);

  switch (prop_id) {
    case PROP_SYNC:
      g_value_set_boolean (value, self->sync);
      break;
    case PROP_TIMEOUT:
      g_value_set_int64 (value, self->retry_timeout);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_vvas_xmetaaffixer_sink_event (GstCollectPads * pads,
    GstCollectData * cdata, GstEvent * event, GstVvas_XMetaAffixer * self)
{
  GstVvas_XMetaAffixerPad *pad = GST_VVAS_XMETAAFFIXER_PAD (cdata->pad);
  gboolean discard = FALSE;
  GstSegment segment;

  GST_DEBUG_OBJECT (pad, "Pad %s Got %s event: %" GST_PTR_FORMAT,
      GST_PAD_NAME (pad), GST_EVENT_TYPE_NAME (event), event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:{
      GstCaps *caps;
      gint fps_n;
      gint fps_d;

      gst_event_parse_caps (event, &caps);

      GST_INFO_OBJECT (pad, "Received caps %" GST_PTR_FORMAT, caps);

      if (!gst_video_info_from_caps (&pad->vinfo, caps)) {
        GST_ERROR_OBJECT (pad, "Failed to parse caps");
        return FALSE;
      }

      /* handling dynamic resolution change */
      if (pad->width != GST_VIDEO_INFO_WIDTH (&pad->vinfo) ||
          pad->height != GST_VIDEO_INFO_HEIGHT (&pad->vinfo)) {
        GST_DEBUG_OBJECT (pad, "MetaAffixer pad %s input resolution"
            " changed:Old: w=%d h=%d",
            GST_PAD_NAME (pad), pad->width, pad->height);
      }

      pad->width = GST_VIDEO_INFO_WIDTH (&pad->vinfo);
      pad->height = GST_VIDEO_INFO_HEIGHT (&pad->vinfo);

      fps_d = GST_VIDEO_INFO_FPS_D (&pad->vinfo);
      fps_n = GST_VIDEO_INFO_FPS_N (&pad->vinfo);

      /* duration in nano sec. */
      pad->duration = (fps_d * 1000000000) / fps_n;

      pad->curr_pts = 0;

      GST_INFO_OBJECT (pad, "MetaAffixer pad %s resolution w=%d h=%d",
          GST_PAD_NAME (pad), pad->width, pad->height);

      GST_INFO_OBJECT (pad, "MetaAffixer pad %s fps_n=%d fps_d=%d",
          GST_PAD_NAME (pad), fps_n, fps_d);

      GST_INFO_OBJECT (pad, "Duration pad %" GST_TIME_FORMAT,
          GST_TIME_ARGS (pad->duration));

      GST_DEBUG_OBJECT (pad, "Input Format No:%d   Name %s",
          GST_VIDEO_INFO_FORMAT (&pad->vinfo),
          GST_VIDEO_INFO_NAME (&pad->vinfo));

      /* set the same caps on the src pad */
      gst_pad_set_caps (pad->srcpad, caps);

      break;
    }
    case GST_EVENT_EOS:
      GST_DEBUG_OBJECT (pad, "received eos");
      pad->eos_received = TRUE;
      break;
    case GST_EVENT_SEGMENT:
      gst_event_copy_segment (event, &segment);

      if (!gst_pad_push_event (pad->srcpad, gst_event_new_segment(&segment))) {
        GST_WARNING_OBJECT (pad, "Pushing segment event to src pad failed");
      } else {
        GST_DEBUG_OBJECT (pad, "Pushing segment event to src pad is successful");
      }
      break;
    
    case GST_EVENT_STREAM_START:
      if (!gst_pad_push_event (pad->srcpad, event)) {
        GST_WARNING_OBJECT (pad, "Pushing stream start event to src pad failed");
        return FALSE;
      } else {
        GST_DEBUG_OBJECT (pad, "Pushing stream start event to src pad is successful");
        return TRUE;
      }
      break;

    default:
      break;
  }

  return gst_collect_pads_event_default (pads, cdata, event, discard);
}

static void
gst_vvas_xmetaaffixer_finalize (GObject * obj)
{
  GstVvas_XMetaAffixer *self = GST_VVAS_XMETAAFFIXER (obj);
  g_cond_clear (&self->timeout_cond);
  g_mutex_clear (&self->timeout_lock);
  g_mutex_clear (&self->collected_lock);
  gst_flow_combiner_free (self->flowcombiner);
  if (self->prev_meta_buf)
    gst_buffer_unref (self->prev_meta_buf);
  gst_object_unref (self->collect);
}

static GstIterator *
gst_vvas_xmetaaffixer_iterate_internal_links (GstPad * pad, GstObject * parent)
{
  GstVvas_XMetaAffixer *self = GST_VVAS_XMETAAFFIXER (parent);
  GValue val = { 0, };
  GstIterator *it = NULL;
  GstPad *other_pad;
  gboolean bFound = FALSE;

  GST_LOG_OBJECT (self, "Internal links for pad %s", GST_PAD_NAME (pad));

  GST_OBJECT_LOCK (parent);

  if (GST_PAD_IS_SINK (pad)) {
    GstVvas_XMetaAffixerPad *sink_pad = (GstVvas_XMetaAffixerPad *) pad;
    other_pad = sink_pad->srcpad;
  } else {
    if (self->sink_master && self->sink_master->srcpad == pad) {
      other_pad = GST_PAD (self->sink_master);
      bFound = TRUE;
    } else {
      int i;
      for (i = 0; i < MAX_SLAVE_SOURCES; i++) {
        if (self->sink_slave[i] && self->sink_slave[i]->srcpad == pad) {
          other_pad = GST_PAD (self->sink_slave[i]);
          bFound = TRUE;
          break;
        }
      }
    }

    if (bFound == FALSE) {
      GST_ERROR_OBJECT (self, "wrong pad, %s, in iterate internal link",
          GST_PAD_NAME (pad));
      GST_OBJECT_UNLOCK (parent);
      return NULL;
    }
  }

  g_value_init (&val, GST_TYPE_PAD);
  g_value_set_object (&val, other_pad);
  it = gst_iterator_new_single (GST_TYPE_PAD, &val);
  g_value_unset (&val);
  GST_OBJECT_UNLOCK (parent);

  return it;
}

static GstPad *
gst_vvas_xmetaaffixer_request_new_pad (GstElement * element,
    GstPadTemplate * sink_templ, const gchar * name, const GstCaps * caps)
{
  GstVvas_XMetaAffixer *self = GST_VVAS_XMETAAFFIXER (element);
  GstVvas_XMetaAffixerClass *klass = GST_VVAS_XMETAAFFIXER_GET_CLASS (self);
  const gchar *sink_tmpl_name = GST_PAD_TEMPLATE_NAME_TEMPLATE (sink_templ);
  GstVvas_XMetaAffixerPad *sink_pad = NULL;
  GstPad *src_pad = NULL;
  GstVvas_XMetaAffixerCollectData *collect_data = NULL;
  GstPadTemplate *src_templ;
  gboolean bret = TRUE;
  unsigned int slave_index = 0xFFFFFFFF;

  GST_DEBUG_OBJECT (self, "Requesting pad with name %s", name);

  if (!g_strcmp0 (sink_tmpl_name, "sink_slave_%u")) {
    /* Get the index of the slave */
    if (name && sscanf (name, "sink_slave_%u", &slave_index) == 1) {
      if (slave_index >= MAX_SLAVE_SOURCES) {
        GST_ERROR_OBJECT (self, "Max allowed sources, %d, already attached",
            MAX_SLAVE_SOURCES);
        return NULL;
      } else if (slave_index > self->num_slaves) {
        GST_ERROR_OBJECT (self, "Pad index must be incremental"
            "should be %d, but received %d", self->num_slaves, slave_index);
        return NULL;
      }
    } else {
      GST_ERROR_OBJECT (self, "Requested Sink pad name, %s, format is not "
          "correct", name);
      return NULL;
    }

    sink_pad = self->sink_slave[slave_index];
  } else if (!g_strcmp0 (sink_tmpl_name, "sink_master")) {
    sink_pad = self->sink_master;
  } else {
    GST_ERROR_OBJECT (self, "sink pad template name %s, not matching "
        "with any sink pad template", sink_tmpl_name);
    goto error;
  }

  /* Check if pad already exists */
  if (sink_pad) {
    GST_ERROR_OBJECT (self, "pad %s already exist", GST_PAD_NAME (sink_pad));
    goto error;
  }

  /* requested pad with name doesn't exists. Create new pad */

  sink_pad =
      g_object_new (GST_TYPE_VVAS_XMETAAFFIXER_PAD, "name", name, "direction",
      sink_templ->direction, "template", sink_templ, NULL);

  collect_data = (GstVvas_XMetaAffixerCollectData *)
      gst_collect_pads_add_pad (self->collect,
      GST_PAD (sink_pad), sizeof (GstVvas_XMetaAffixerCollectData), NULL, TRUE);

  collect_data->sinkpad = sink_pad;
  sink_pad->collect = collect_data;

  bret = gst_element_add_pad (element, GST_PAD (sink_pad));

  if (!bret) {
    GST_ERROR_OBJECT (self, "Failed to add sink pad %s", name);
    goto error;
  }

  gst_pad_set_iterate_internal_links_function (GST_PAD (sink_pad),
      gst_vvas_xmetaaffixer_iterate_internal_links);

  GST_PAD_SET_PROXY_CAPS (GST_PAD (sink_pad));
  GST_PAD_SET_PROXY_ALLOCATION (GST_PAD (sink_pad));
  GST_PAD_SET_PROXY_SCHEDULING (GST_PAD (sink_pad));

  /* create source pad corresponding to sinkpad */

  if (!g_strcmp0 (sink_tmpl_name, "sink_slave_%u")) {
    char src_slave_name[25];
    self->sink_slave[slave_index] = sink_pad;
    src_templ = gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass),
        "src_slave_%u");

    if (src_templ == NULL) {
      GST_ERROR_OBJECT (self, "For sink slave %s, src template is NULL",
          GST_PAD_NAME (sink_pad));
      goto error;
    }

    sprintf (src_slave_name, "src_slave_%u", slave_index);

    src_pad = gst_pad_new_from_template (src_templ, src_slave_name);

    if (src_pad) {
      GST_INFO_OBJECT (self, "Created %s pad", GST_PAD_NAME (src_pad));
    } else {
      GST_ERROR_OBJECT (self, "Unable to create %s pad",
          GST_PAD_NAME (sink_pad));
      goto error;
    }
    self->num_slaves++;
  } else if (!g_strcmp0 (sink_tmpl_name, "sink_master")) {
    self->sink_master = sink_pad;

    src_templ = gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass),
        "src_master");
    if (src_templ == NULL) {
      GST_ERROR_OBJECT (self, "For %s, src template is NULL",
          GST_PAD_NAME (sink_pad));
      goto error;
    }

    src_pad = gst_pad_new_from_template (src_templ, "src_master");

    if (src_pad) {
      GST_INFO_OBJECT (self, "Created %s Pad", GST_PAD_NAME (src_pad));
    } else {
      GST_ERROR_OBJECT (self, "Unable to create %s pad for master sink",
          GST_PAD_NAME (sink_pad));
      goto error;
    }
  }

  gst_element_add_pad ((GstElement *) self, src_pad);
  gst_flow_combiner_add_pad (self->flowcombiner, src_pad);

  sink_pad->srcpad = src_pad;
  sink_pad->fret = GST_FLOW_OK;
  sink_pad->stream_state = VVAS_XMETAAFFIXER_STATE_FIRST_BUFFER;

  gst_pad_set_iterate_internal_links_function (src_pad,
      gst_vvas_xmetaaffixer_iterate_internal_links);

  return GST_PAD (sink_pad);

error:
  if (collect_data)
    free (collect_data);
  if (sink_pad)
    g_free (sink_pad);

  return NULL;
}

static void
gst_vvas_xmetaaffixer_release_pad (GstElement * element, GstPad * pad)
{
  GstVvas_XMetaAffixer *self = GST_VVAS_XMETAAFFIXER (element);
  GstVvas_XMetaAffixerPad *sink_pad = GST_VVAS_XMETAAFFIXER_PAD (pad);

  GST_INFO_OBJECT (self, "  %" GST_PTR_FORMAT, pad);

  gst_collect_pads_remove_pad (self->collect, pad);

  if (sink_pad && sink_pad->srcpad) {
    gst_element_remove_pad (element, sink_pad->srcpad);
    gst_flow_combiner_remove_pad (self->flowcombiner, sink_pad->srcpad);
  }

  gst_element_remove_pad (element, pad);
}

#if ENABLE_TEST_CODE
GstMeta *
create_dummy_infermeta (GstBuffer *buffer, GstVideoInfo *vinfo)
{
  BoundingBox bbox;
  GstInferencePrediction *predict;
  GstInferenceMeta *infer_meta;

  infer_meta = (GstInferenceMeta *) gst_buffer_add_meta (buffer, gst_inference_meta_get_info (), NULL);
  infer_meta->prediction = gst_inference_prediction_new ();
  infer_meta->prediction->bbox.width = GST_VIDEO_INFO_WIDTH (vinfo);
  infer_meta->prediction->bbox.height = GST_VIDEO_INFO_HEIGHT (vinfo);

  bbox.x = 10;
  bbox.y = 5;
  bbox.width = 100;
  bbox.height = 50;

  predict = gst_inference_prediction_new_full (&bbox);
  gst_inference_prediction_append (infer_meta->prediction, predict);

  bbox.x = 2;
  bbox.y = 8;
  bbox.width = 200;
  bbox.height = 400;

  predict = gst_inference_prediction_new_full (&bbox);
  gst_inference_prediction_append (infer_meta->prediction, predict);

  GST_WARNING ("attaching dummy infermeta");

  return (GstMeta *) infer_meta;
}
#endif

GstFlowReturn
vvas_xmetaaffixer_combined_return (GstVvas_XMetaAffixer * self)
{
  GstFlowReturn fret = GST_FLOW_OK;
  int slave_idx;

  for (slave_idx = 0; slave_idx < self->num_slaves; slave_idx++) {
    GstVvas_XMetaAffixerPad *sink_slave = self->sink_slave[slave_idx];
    gst_flow_combiner_update_pad_flow (self->flowcombiner, sink_slave->srcpad,
        sink_slave->fret);
  }

  fret =
      gst_flow_combiner_update_pad_flow (self->flowcombiner,
      self->sink_master->srcpad, self->sink_master->fret);
  GST_LOG_OBJECT (self, "combined to return %s", gst_flow_get_name (fret));
  return fret;
}

static GstFlowReturn
vvas_xmetaaffixer_get_min_end_ts (GstVvas_XMetaAffixer * self,
    GstCollectPads * pads, GstClockTime * min_end_ts)
{
  GstBuffer *buffer = NULL;
  GstClockTime cur_start_ts = GST_CLOCK_TIME_NONE;
  GstClockTime cur_end_ts = GST_CLOCK_TIME_NONE;
  GstClockTime cur_dur = GST_CLOCK_TIME_NONE;
  guint slave_idx;

  /* find minimum end ts of buffer at each slave sink pad */
  for (slave_idx = 0; slave_idx < self->num_slaves; slave_idx++) {
    GstVvas_XMetaAffixerPad *sink_slave = self->sink_slave[slave_idx];

    buffer =
        gst_collect_pads_peek (pads, (GstCollectData *) sink_slave->collect);
    if (!buffer) {
      if (GST_COLLECT_PADS_STATE_IS_SET ((GstCollectData *) sink_slave->collect,
              GST_COLLECT_PADS_STATE_EOS)) {
        if (!sink_slave->sent_eos) {
          GST_INFO_OBJECT (self, "pushing eos on pad %s",
              GST_PAD_NAME (sink_slave->srcpad));

          if (!gst_pad_push_event (sink_slave->srcpad, gst_event_new_eos ()))
            GST_ERROR_OBJECT (self, "failed to send eos event on pad %s",
                GST_PAD_NAME (sink_slave->srcpad));

          sink_slave->sent_eos = TRUE;
        }
        sink_slave->fret = GST_FLOW_EOS;
        continue;
      } else {
        g_mutex_lock (&self->timeout_lock);
        if (self->timeout_issued) {
          g_mutex_unlock (&self->timeout_lock);
          continue;
        }
        g_mutex_unlock (&self->timeout_lock);
        GST_LOG_OBJECT (self, "buffer is not availale at slave pad index %d",
            slave_idx);
        sink_slave->fret = GST_FLOW_OK;
        goto exit;
      }
    }

    cur_start_ts = GST_BUFFER_PTS (buffer);
    cur_dur = GST_BUFFER_DURATION (buffer);
    gst_buffer_unref (buffer);

    if (!GST_CLOCK_TIME_IS_VALID (cur_dur)
        && GST_CLOCK_TIME_IS_VALID (sink_slave->duration)) {
      GST_LOG_OBJECT (self,
          "slave index %d does not have a valid timestamp, calculated duration from framerate %"
          GST_TIME_FORMAT, slave_idx, GST_TIME_ARGS (sink_slave->duration));
      cur_dur = sink_slave->duration;
    }

    if (!GST_CLOCK_TIME_IS_VALID (cur_start_ts)
        || !GST_CLOCK_TIME_IS_VALID (cur_dur)) {
      GST_WARNING_OBJECT (self,
          "does not have valid frame start timestamp and/or duration to do sync");
      /* even one invalid timestamp can stop sync mode */
      self->sync = FALSE;
    } else {
      /* have valid start_ts and duration */
      cur_end_ts = cur_start_ts + cur_dur;

      if (cur_end_ts < *min_end_ts) {
        /*update min end ts */
        *min_end_ts = cur_end_ts;
      }
    }
  }

  /* get min end ts based on master buffer */
  GST_COLLECT_PADS_STREAM_LOCK (pads);
  buffer =
      gst_collect_pads_peek (pads,
      (GstCollectData *) self->sink_master->collect);
  GST_COLLECT_PADS_STREAM_UNLOCK (pads);

  if (!buffer) {
    if (GST_COLLECT_PADS_STATE_IS_SET ((GstCollectData *) self->
            sink_master->collect, GST_COLLECT_PADS_STATE_EOS)) {
      if (!self->sink_master->sent_eos) {
        GST_INFO_OBJECT (self, "pushing eos on pad %s",
            GST_PAD_NAME (self->sink_master->srcpad));

        if (!gst_pad_push_event (self->sink_master->srcpad,
                gst_event_new_eos ()))
          GST_ERROR_OBJECT (self, "failed to send eos event on pad %s",
              GST_PAD_NAME (self->sink_master->srcpad));
        self->sink_master->sent_eos = TRUE;
      }
      self->sink_master->fret = GST_FLOW_EOS;
      goto exit;
    } else {
      g_mutex_lock (&self->timeout_lock);
      if (!buffer && self->timeout_issued) {
        g_mutex_unlock (&self->timeout_lock);
        goto exit;
      }
      g_mutex_unlock (&self->timeout_lock);
      GST_LOG_OBJECT (self, "buffer is not availale at master pad");
      self->sink_master->fret = GST_FLOW_OK;
      goto exit;
    }
  }

  cur_start_ts = GST_BUFFER_PTS (buffer);
  cur_dur = GST_BUFFER_DURATION (buffer);
  gst_buffer_unref (buffer);

  if (!GST_CLOCK_TIME_IS_VALID (cur_dur)
      && GST_CLOCK_TIME_IS_VALID (self->sink_master->duration)) {
    GST_LOG_OBJECT (self,
        "sink master pad does not have a valid timestamp, calculated duration"
        " from framerate %" GST_TIME_FORMAT,
        GST_TIME_ARGS (self->sink_master->duration));
    cur_dur = self->sink_master->duration;
  }

  if (!GST_CLOCK_TIME_IS_VALID (cur_start_ts)
      || !GST_CLOCK_TIME_IS_VALID (cur_dur) || !cur_dur || !self->num_slaves) {
    GST_LOG_OBJECT (self,
        "does not have valid frame start timestamp and/or duration or no slaves"
        " going to run in no sync mode");
    /* even one invalid timestamp can stop sync mode */
    self->sync = FALSE;
  } else {
    /* have valid start_ts and duration */
    cur_end_ts = cur_start_ts + cur_dur;

    if (cur_end_ts < *min_end_ts) {
      /*update min end ts */
      *min_end_ts = cur_end_ts;
    }
  }

exit:
  GST_DEBUG_OBJECT (self, "found minimum end ts %" GST_TIME_FORMAT,
      GST_TIME_ARGS (*min_end_ts));

  return vvas_xmetaaffixer_combined_return (self);
}

static GstFlowReturn
vvas_xmetaaffixer_process (GstVvas_XMetaAffixer * self, GstCollectPads * pads,
    GstClockTime min_end_ts)
{
  GstBuffer *mbuffer = NULL;
  guint slave_idx;
  GstMeta *infer_meta = NULL;
  gboolean pick_prev_meta = FALSE;
  GstClockTime m_cur_start_ts = GST_CLOCK_TIME_NONE;
  GstClockTime m_cur_end_ts = GST_CLOCK_TIME_NONE;
  GstClockTime m_cur_dur = GST_CLOCK_TIME_NONE;
#if ENABLE_TEST_CODE
  GstBuffer *tmp_buffer = NULL;
#endif

  GST_COLLECT_PADS_STREAM_LOCK (pads);
  mbuffer =
      gst_collect_pads_peek (pads,
      (GstCollectData *) self->sink_master->collect);
  GST_COLLECT_PADS_STREAM_UNLOCK (pads);

  if (!mbuffer) {
    if (GST_COLLECT_PADS_STATE_IS_SET ((GstCollectData *) self->
            sink_master->collect, GST_COLLECT_PADS_STATE_EOS)) {
      if (!self->sink_master->sent_eos) {
        GST_INFO_OBJECT (self, "pushing eos on pad %s",
            GST_PAD_NAME (self->sink_master->srcpad));

        if (!gst_pad_push_event (self->sink_master->srcpad,
                gst_event_new_eos ()))
          GST_ERROR_OBJECT (self, "failed to send eos event on pad %s",
              GST_PAD_NAME (self->sink_master->srcpad));
        self->sink_master->sent_eos = TRUE;
      }
      self->sink_master->fret = GST_FLOW_EOS;
    } else {
      g_mutex_lock (&self->timeout_lock);
      if (!mbuffer && self->timeout_issued) {
        g_mutex_unlock (&self->timeout_lock);
        goto slave;
      }
      g_mutex_unlock (&self->timeout_lock);
      GST_LOG_OBJECT (self, "buffer is not availale at master pad");
      self->sink_master->fret = GST_FLOW_OK;
      goto exit;
    }
  } else {
    infer_meta = gst_buffer_get_meta (mbuffer, gst_inference_meta_api_get_type ());
#if ENABLE_TEST_CODE
    /* Enable this code if your master buffer does not have meta data and
     * you want to test the plugin. */

    if (infer_meta == NULL) {
      tmp_buffer = gst_buffer_new ();
      if (tmp_buffer == NULL) {
        printf ("Temp buffer not Allocated");
        return GST_FLOW_ERROR;
      }
      infer_meta = create_dummy_infermeta (tmp_buffer, &self->sink_master->vinfo);
      GST_ERROR ("infer meta %p", infer_meta);
    }
#endif
    GST_LOG_OBJECT (self, "infermeta %p received from master buffer %p",
        infer_meta, mbuffer);
  }

slave:
  for (slave_idx = 0; slave_idx < self->num_slaves; slave_idx++) {
    GstVvas_XMetaAffixerPad *sink_slave = self->sink_slave[slave_idx];
    GstBuffer *writable_buffer = NULL;
    GstBuffer *sbuffer = NULL;
    GstClockTime s_cur_start_ts = GST_CLOCK_TIME_NONE;
    GstClockTime s_cur_end_ts = GST_CLOCK_TIME_NONE;
    GstClockTime s_cur_dur = GST_CLOCK_TIME_NONE;

    GST_COLLECT_PADS_STREAM_LOCK (pads);
    sbuffer =
        gst_collect_pads_peek (pads, (GstCollectData *) sink_slave->collect);
    GST_COLLECT_PADS_STREAM_UNLOCK (pads);

    if (!sbuffer) {
      if (GST_COLLECT_PADS_STATE_IS_SET ((GstCollectData *) sink_slave->collect,
              GST_COLLECT_PADS_STATE_EOS)) {
        if (!sink_slave->sent_eos) {
          GST_INFO_OBJECT (self, "pushing eos on pad %s",
              GST_PAD_NAME (sink_slave->srcpad));

          if (!gst_pad_push_event (sink_slave->srcpad, gst_event_new_eos ()))
            GST_ERROR_OBJECT (self, "failed to send eos event on pad %s",
                GST_PAD_NAME (sink_slave->srcpad));
          sink_slave->sent_eos = TRUE;
        }
        sink_slave->fret = GST_FLOW_EOS;
        continue;
      } else {
        g_mutex_lock (&self->timeout_lock);
        if (self->timeout_issued) {
          g_mutex_unlock (&self->timeout_lock);
          continue;
        }
        g_mutex_unlock (&self->timeout_lock);
        GST_LOG_OBJECT (self, "failed to get buffer from collectpad %s",
            GST_PAD_NAME (sink_slave));
        sink_slave->fret = GST_FLOW_OK;
        goto exit;
      }
    }

    s_cur_start_ts = GST_BUFFER_PTS (sbuffer);
    s_cur_dur = GST_BUFFER_DURATION (sbuffer);
    s_cur_end_ts = s_cur_start_ts + s_cur_dur;

    if (self->sync) {
      if (s_cur_end_ts != min_end_ts) {
        if (sink_slave->stream_state == VVAS_XMETAAFFIXER_STATE_PROCESS_BUFFER
            || sink_slave->stream_state ==
            VVAS_XMETAAFFIXER_STATE_DROP_BUFFER) {
          /* don't push slave buffer */
          gst_buffer_unref (sbuffer);
          sink_slave->fret = GST_FLOW_OK;
          continue;
        } else if (sink_slave->stream_state ==
            VVAS_XMETAAFFIXER_STATE_FIRST_BUFFER) {
          /* don't unref if its is first buffer,
           * lets send out to finish state change */
          /* as we have done _peek and sent buffer downstream,
           * unref next popped buffer as it is a duplicate buffer */
          GST_INFO_OBJECT (sink_slave, "received first buffer %p", sbuffer);
          sink_slave->stream_state = VVAS_XMETAAFFIXER_STATE_DROP_BUFFER;
          goto push_slave;
        }
      } else {
        if (sink_slave->stream_state == VVAS_XMETAAFFIXER_STATE_FIRST_BUFFER) {
          sink_slave->stream_state = VVAS_XMETAAFFIXER_STATE_PROCESS_BUFFER;
        } else if (sink_slave->stream_state ==
            VVAS_XMETAAFFIXER_STATE_DROP_BUFFER) {
          sink_slave->stream_state = VVAS_XMETAAFFIXER_STATE_PROCESS_BUFFER;
          /* unref buffer acquired using _peek */
          gst_buffer_unref (sbuffer);

          GST_COLLECT_PADS_STREAM_LOCK (pads);
          sbuffer =
              gst_collect_pads_pop (pads,
              (GstCollectData *) sink_slave->collect);
          GST_COLLECT_PADS_STREAM_UNLOCK (pads);

          GST_INFO_OBJECT (sink_slave,
              "dropping buffer %p which has been pushed already", sbuffer);

          gst_buffer_unref (sbuffer);
          sink_slave->fret = GST_FLOW_OK;
          goto exit;
        }
        gst_buffer_unref (sbuffer);
      }

      if ((s_cur_start_ts + (s_cur_dur >> 1)) <= self->prev_m_end_ts) {
        /* more than 50% of the current frame falls in
         * previous master buffer duration */
        if (self->prev_meta_buf) {
          infer_meta = gst_buffer_get_meta (self->prev_meta_buf, gst_inference_meta_api_get_type ());
          pick_prev_meta = TRUE;
          GST_DEBUG_OBJECT (sink_slave,
              "picking previous master buffer metadata %p", infer_meta);
        }
      } else if (self->sink_master->sent_eos
          && s_cur_start_ts < self->prev_m_end_ts) {
        GST_INFO_OBJECT (sink_slave,
            "master received EOS and cur start ts %" GST_TIME_FORMAT
            " < prev master end ts %" GST_TIME_FORMAT,
            GST_TIME_ARGS (s_cur_start_ts),
            GST_TIME_ARGS (self->prev_m_end_ts));
        if (self->prev_meta_buf) {
          infer_meta = gst_buffer_get_meta (self->prev_meta_buf, gst_inference_meta_api_get_type ());
          pick_prev_meta = TRUE;
          GST_DEBUG_OBJECT (sink_slave,
              "attaching best possible metadata %p as master on EOS", infer_meta);
        }
      }
    } else {
      gst_buffer_unref (sbuffer);
    }

    GST_COLLECT_PADS_STREAM_LOCK (pads);
    sbuffer =
        gst_collect_pads_pop (pads, (GstCollectData *) sink_slave->collect);
    GST_COLLECT_PADS_STREAM_UNLOCK (pads);

  push_slave:
    g_mutex_lock (&self->timeout_lock);
    g_cond_signal (&self->timeout_cond);
    g_mutex_unlock (&self->timeout_lock);
    if (infer_meta) {
      const GstMetaInfo *info;

      GstVideoMetaTransform trans = { &self->sink_master->vinfo,
            &sink_slave->vinfo };

      writable_buffer = gst_buffer_make_writable (sbuffer);

      info = infer_meta->info;

      GST_LOG_OBJECT (sink_slave, "attaching infer metadata %p to buffer %p",
            infer_meta, writable_buffer);

      if (!pick_prev_meta)
        info->transform_func (writable_buffer, infer_meta, mbuffer,
            _scale_quark, &trans);
      else
        info->transform_func (writable_buffer, infer_meta, self->prev_meta_buf,
            _scale_quark, &trans);

    } else {
      writable_buffer = sbuffer;
    }

    GST_LOG_OBJECT (self, "pushing buffer %p on pad %s with pts %"
        GST_TIME_FORMAT " duration %" GST_TIME_FORMAT " and end ts %"
        GST_TIME_FORMAT, writable_buffer, GST_PAD_NAME (sink_slave->srcpad),
        GST_TIME_ARGS (s_cur_start_ts), GST_TIME_ARGS (s_cur_dur),
        GST_TIME_ARGS (s_cur_end_ts));

    sink_slave->fret = gst_pad_push (sink_slave->srcpad, writable_buffer);
    if (sink_slave->fret != GST_FLOW_OK) {
      GST_ERROR_OBJECT (self, "failed to push to pad %s. reason %s",
          GST_PAD_NAME (sink_slave), gst_flow_get_name (sink_slave->fret));
      goto exit;
    }
  }

#if ENABLE_TEST_CODE
  if (tmp_buffer)
    gst_buffer_unref (tmp_buffer);
#endif

  if (mbuffer) {
    m_cur_start_ts = GST_BUFFER_PTS (mbuffer);
    m_cur_dur = GST_BUFFER_DURATION (mbuffer);
    m_cur_end_ts = m_cur_start_ts + m_cur_dur;

    if (self->sync && self->num_slaves) {
      if (m_cur_end_ts != min_end_ts) {
        if (self->sink_master->stream_state ==
            VVAS_XMETAAFFIXER_STATE_PROCESS_BUFFER
            || self->sink_master->stream_state ==
            VVAS_XMETAAFFIXER_STATE_DROP_BUFFER) {
          /* don't push master buffer now */
          self->sink_master->fret = GST_FLOW_OK;
          gst_buffer_unref (mbuffer);
          goto exit;
        } else if (self->sink_master->stream_state ==
            VVAS_XMETAAFFIXER_STATE_FIRST_BUFFER) {
          /* don't unref if its is first buffer,
           * lets send out to finish state change */
          /* as we have done _peek and sent buffer downstream,
           * unref next popped buffer as it is a duplicate buffer */
          self->sink_master->stream_state = VVAS_XMETAAFFIXER_STATE_DROP_BUFFER;
          goto push_master;
        }
      } else {
        if (self->sink_master->stream_state ==
            VVAS_XMETAAFFIXER_STATE_FIRST_BUFFER) {
          self->sink_master->stream_state =
              VVAS_XMETAAFFIXER_STATE_PROCESS_BUFFER;
        } else if (self->sink_master->stream_state ==
            VVAS_XMETAAFFIXER_STATE_DROP_BUFFER) {
          self->sink_master->stream_state =
              VVAS_XMETAAFFIXER_STATE_PROCESS_BUFFER;
          /* unref buffer acquired using _peek */
          gst_buffer_unref (mbuffer);

          GST_COLLECT_PADS_STREAM_LOCK (pads);
          mbuffer =
              gst_collect_pads_pop (pads,
              (GstCollectData *) self->sink_master->collect);
          GST_COLLECT_PADS_STREAM_UNLOCK (pads);

          GST_INFO_OBJECT (self->sink_master,
              "dropping buffer %p which has been pushed already", mbuffer);

          gst_buffer_unref (mbuffer);
          self->sink_master->fret = GST_FLOW_OK;
          goto exit;
        }
        gst_buffer_unref (mbuffer);
      }
    } else {
      gst_buffer_unref (mbuffer);
    }

    GST_COLLECT_PADS_STREAM_LOCK (pads);
    mbuffer =
        gst_collect_pads_pop (pads,
        (GstCollectData *) self->sink_master->collect);
    GST_COLLECT_PADS_STREAM_UNLOCK (pads);

  push_master:
    g_mutex_lock (&self->timeout_lock);
    g_cond_signal (&self->timeout_cond);
    g_mutex_unlock (&self->timeout_lock);
    if (self->sync && self->num_slaves) {
      if (self->prev_meta_buf)
        gst_buffer_unref (self->prev_meta_buf);

      self->prev_meta_buf = gst_buffer_new ();
      gst_buffer_copy_into (self->prev_meta_buf, mbuffer,
          GST_BUFFER_COPY_META, 0, -1);

#if ENABLE_TEST_CODE
      /* Enable this code if your master buffer does not have meta data and
       * you want to test the plugin. */
      infer_meta = gst_buffer_get_meta (self->prev_meta_buf, gst_inference_meta_api_get_type ());

      if (infer_meta == NULL) {
        tmp_buffer = gst_buffer_new ();
        if (tmp_buffer == NULL) {
          printf ("Temp buffer not Allocated");
          return GST_FLOW_ERROR;
        }

        infer_meta = create_dummy_infermeta (tmp_buffer, &self->sink_master->vinfo);
        gst_buffer_copy_into (self->prev_meta_buf, tmp_buffer,
            GST_BUFFER_COPY_META, 0, -1);
        gst_buffer_unref (tmp_buffer);
      }
#endif
    }
    self->prev_m_end_ts = m_cur_end_ts;

    GST_LOG_OBJECT (self, "pushing buffer %p on pad %s with pts %"
        GST_TIME_FORMAT " duration %" GST_TIME_FORMAT " and end ts %"
        GST_TIME_FORMAT, mbuffer, GST_PAD_NAME (self->sink_master->srcpad),
        GST_TIME_ARGS (m_cur_start_ts), GST_TIME_ARGS (m_cur_dur),
        GST_TIME_ARGS (m_cur_end_ts));

    self->sink_master->fret = gst_pad_push (self->sink_master->srcpad, mbuffer);
    if (self->sink_master->fret != GST_FLOW_OK) {
      GST_ERROR_OBJECT (self, "failed to push to pad %s. reason %s",
          GST_PAD_NAME (self->sink_master->srcpad),
          gst_flow_get_name (self->sink_master->fret));
      goto exit;
    }
  }

exit:
  return vvas_xmetaaffixer_combined_return (self);
}

static GstFlowReturn
gst_vvas_xmetaaffixer_collected (GstCollectPads * pads, gpointer user_data)
{
  GstVvas_XMetaAffixer *self = GST_VVAS_XMETAAFFIXER (user_data);
  GstClockTime min_end_ts = GST_CLOCK_TIME_NONE;
  GstFlowReturn fret = GST_FLOW_OK;

  g_mutex_lock (&self->collected_lock);
  if (NULL == self->sink_master) {
    GST_ERROR_OBJECT (self, "Master sink pad not created, can't proceed");
    GST_ELEMENT_ERROR (self, STREAM, FAILED, ("master sink pad not created"),
        (NULL));
    g_mutex_unlock (&self->collected_lock);
    return GST_FLOW_ERROR;
  }

  if (self->sync) {
    /* find min end ts in all sink pads */
    fret = vvas_xmetaaffixer_get_min_end_ts (self, pads, &min_end_ts);
    if (fret != GST_FLOW_EOS && fret != GST_FLOW_OK) {
      GST_ERROR_OBJECT (self, "failed to find min end ts. reason %s",
          gst_flow_get_name (fret));
      g_mutex_unlock (&self->collected_lock);
      goto exit;
    }
  }

  fret = vvas_xmetaaffixer_process (self, pads, min_end_ts);
  g_mutex_unlock (&self->collected_lock);

exit:
  return fret;
}

static gpointer
timeout_func (gpointer data)
{
  GstVvas_XMetaAffixer *self = GST_VVAS_XMETAAFFIXER (data);
  gint64 end_time;
  GstFlowReturn fret;
  gboolean is_locked = FALSE;
  while (1) {
    g_mutex_lock (&self->timeout_lock);
    is_locked = TRUE;
    if (!self->start_thread) {
      end_time = G_MAXINT64;
    }
    else {
      end_time = g_get_monotonic_time () + self->retry_timeout;
    }

    if (!g_cond_wait_until (&self->timeout_cond, &self->timeout_lock, end_time)) {
      GST_INFO_OBJECT (self, "Timeout occured");
      self->timeout_issued = TRUE;
      if (is_locked) {
        g_mutex_unlock (&self->timeout_lock);
        is_locked = FALSE;
      }
      fret = gst_vvas_xmetaaffixer_collected (self->collect, self);
      g_mutex_lock (&self->timeout_lock);
      is_locked = TRUE;
      self->timeout_issued = FALSE;
      if (fret != GST_FLOW_OK) {
        if (is_locked) {
          g_mutex_unlock (&self->timeout_lock);
          is_locked = FALSE;
        }
        break;
      }
    }
    else if (self->stop_thread) {
      if (is_locked) {
        g_mutex_unlock (&self->timeout_lock);
        is_locked = FALSE;
      }
      break;
    }

    if (is_locked) {
      g_mutex_unlock (&self->timeout_lock);
      is_locked = FALSE;
    }
  }
  return NULL;
}

static GstStateChangeReturn
gst_vvas_xmetaaffixer_change_state (GstElement * element,
    GstStateChange transition)
{
  GstStateChangeReturn ret;
  GstVvas_XMetaAffixer *self = GST_VVAS_XMETAAFFIXER (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_collect_pads_start (self->collect);
      if (self->retry_timeout != -1) {
        g_mutex_lock (&self->timeout_lock);
        self->retry_timeout *= G_TIME_SPAN_MILLISECOND;
        g_mutex_unlock (&self->timeout_lock);
        /* start thread to monitor buffer flow */
        self->timeout_thread = g_thread_new ("metaaffixer watchdog", timeout_func, self);
      }
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      if (self->retry_timeout != -1) {
        g_mutex_lock (&self->timeout_lock);
        self->start_thread = TRUE;
        g_cond_signal (&self->timeout_cond);
        g_mutex_unlock (&self->timeout_lock);
      }
      break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      if (self->retry_timeout != -1) {
        g_mutex_lock (&self->timeout_lock);
        self->start_thread = FALSE;
        g_mutex_unlock (&self->timeout_lock);
      }
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      if (self->retry_timeout != -1) {
        g_mutex_lock (&self->timeout_lock);
        self->stop_thread = TRUE;
        g_cond_signal (&self->timeout_cond);
        g_mutex_unlock (&self->timeout_lock);
        if (self->timeout_thread) {
          GST_LOG_OBJECT (self, "waiting for watchdog thread to join");
          g_thread_join (self->timeout_thread);
          self->timeout_thread = NULL;
        }
        self->retry_timeout /= G_TIME_SPAN_MILLISECOND;
        self->timeout_issued = FALSE;
        self->stop_thread = FALSE;
      }
      gst_collect_pads_stop (self->collect);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  return ret;
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
vvas_xmetaaffixer_init (GstPlugin * vvas_xmetaaffixer)
{
  /* debug category for fltering log messages
   * exchange the string 'Template vvas_xmetaaffixer' with your description
   */
  GST_DEBUG_CATEGORY_INIT (gst_vvas_xmetaaffixer_debug, "vvas_xmetaaffixer",
      0, "Template vvas_xmetaaffixer");

  return gst_element_register (vvas_xmetaaffixer, "vvas_xmetaaffixer",
      GST_RANK_NONE, GST_TYPE_VVAS_XMETAAFFIXER);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "vvas_xmetaaffixer"
#endif

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    vvas_xmetaaffixer,
    "Xilinx plugin to Scale and affix Meta data as per resolution",
    vvas_xmetaaffixer_init, "1.0", "MIT/X11",
    "Xilinx VVAS SDK plugin", "http://xilinx.com/")
