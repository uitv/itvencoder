/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include "channel.h"

GST_DEBUG_CATEGORY_EXTERN (ITVENCODER);
#define GST_CAT_DEFAULT ITVENCODER

enum {
        CHANNEL_PROP_0,
        CHANNEL_PROP_CONFIGURE,
        CHANNEL_PROP_NAME,
};

enum {
        SOURCE_PROP_0,
        SOURCE_PROP_NAME,
        SOURCE_PROP_STATE
};

enum {
        ENCODER_PROP_0,
        ENCODER_PROP_NAME,
        ENCODER_PROP_STATE
};

static void source_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec);
static void source_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec);
static void encoder_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec);
static void encoder_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec);
static void channel_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec);
static void channel_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec);

/* Source class */
static void
source_class_init (SourceClass *sourceclass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS(sourceclass);
        GParamSpec *param;

        g_object_class->set_property = source_set_property;
        g_object_class->get_property = source_get_property;

        param = g_param_spec_string (
                "name",
                "name",
                "name of source",
                NULL,
                G_PARAM_WRITABLE | G_PARAM_READABLE
        );
        g_object_class_install_property (g_object_class, SOURCE_PROP_NAME, param);

        param = g_param_spec_int (
                "state",
                "statef",
                "state",
                GST_STATE_VOID_PENDING,
                GST_STATE_PLAYING,
                GST_STATE_VOID_PENDING,
                G_PARAM_WRITABLE | G_PARAM_READABLE
        );
        g_object_class_install_property (g_object_class, SOURCE_PROP_STATE, param);
}

static void
source_init (Source *source)
{
        source->streams = g_array_new (FALSE, FALSE, sizeof(gpointer)); //TODO: free!
}

GType
source_get_type (void)
{
        static GType type = 0;

        if (type) return type;
        static const GTypeInfo info = {
                sizeof (SourceClass), // class size
                NULL, // base class initializer
                NULL, // base class finalizer
                (GClassInitFunc)source_class_init, // class init
                NULL, // class finalize
                NULL, // class data
                sizeof (Source), //instance size
                0, // n_preallocs
                (GInstanceInitFunc)source_init, //instance_init
                NULL // value_table
        };
        type = g_type_register_static (G_TYPE_OBJECT, "Source", &info, 0);

        return type;
}

static void
source_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
        g_return_if_fail(IS_SOURCE(obj));

        switch(prop_id) {
        case SOURCE_PROP_NAME:
                SOURCE(obj)->name = (gchar *)g_value_dup_string (value); //TODO: should release dup string config_file_path?
                break;
        case SOURCE_PROP_STATE:
                SOURCE(obj)->state= g_value_get_int (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
                break;
        }
}

static void
source_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
        Source *source = SOURCE(obj);

        switch(prop_id) {
        case SOURCE_PROP_NAME:
                g_value_set_string (value, source->name);
                break;
        case SOURCE_PROP_STATE:
                g_value_set_int (value, source->state);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
                break;
        }
}

/* Encoder class */
static void
encoder_class_init (EncoderClass *encoderclass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS(encoderclass);
        GParamSpec *param;

        g_object_class->set_property = encoder_set_property;
        g_object_class->get_property = encoder_get_property;

        param = g_param_spec_string (
                "name",
                "name",
                "name of encoder",
                NULL,
                G_PARAM_WRITABLE | G_PARAM_READABLE
        );
        g_object_class_install_property (g_object_class, ENCODER_PROP_NAME, param);

        param = g_param_spec_int (
                "state",
                "statef",
                "state",
                GST_STATE_VOID_PENDING,
                GST_STATE_PLAYING,
                GST_STATE_VOID_PENDING,
                G_PARAM_WRITABLE | G_PARAM_READABLE
        );
        g_object_class_install_property (g_object_class, ENCODER_PROP_STATE, param);
}

static void
encoder_init (Encoder *encoder)
{
        encoder->streams = g_array_new (FALSE, FALSE, sizeof(gpointer)); //TODO: free!
}

GType
encoder_get_type (void)
{
        static GType type = 0;

        if (type) return type;
        static const GTypeInfo info = {
                sizeof (EncoderClass),
                NULL, // base class initializer
                NULL, // base class finalizer
                (GClassInitFunc)encoder_class_init,
                NULL,
                NULL,
                sizeof (Encoder),
                0,
                (GInstanceInitFunc)encoder_init,
                NULL
        };
        type = g_type_register_static (G_TYPE_OBJECT, "Encoder", &info, 0);

        return type;
}

static void
encoder_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
        g_return_if_fail(IS_ENCODER(obj));

        switch(prop_id) {
        case ENCODER_PROP_NAME:
                ENCODER(obj)->name = (gchar *)g_value_dup_string (value); //TODO: should release dup string config_file_path?
                break;
        case ENCODER_PROP_STATE:
                ENCODER(obj)->state= g_value_get_int (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
                break;
        }
}

static void
encoder_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
        Encoder *encoder = ENCODER(obj);

        switch(prop_id) {
        case ENCODER_PROP_NAME:
                g_value_set_string (value, encoder->name);
                break;
        case ENCODER_PROP_STATE:
                g_value_set_int (value, encoder->state);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
                break;
        }
}

/* channel class */
static void
channel_class_init (ChannelClass *channelclass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS(channelclass);
        GParamSpec *param;

        g_object_class->set_property = channel_set_property;
        g_object_class->get_property = channel_get_property;

        param = g_param_spec_string (
                "name",
                "name",
                "name of channel",
                NULL,
                G_PARAM_WRITABLE | G_PARAM_READABLE
        );
        g_object_class_install_property (g_object_class, CHANNEL_PROP_NAME, param);

        param = g_param_spec_pointer (
                "configure",
                "Configure",
                NULL,
                G_PARAM_WRITABLE | G_PARAM_READABLE
        );
        g_object_class_install_property (g_object_class, CHANNEL_PROP_CONFIGURE, param);
}

static void
channel_init (Channel *channel)
{
        channel->system_clock = gst_system_clock_obtain ();
        g_object_set (channel->system_clock, "clock-type", GST_CLOCK_TYPE_REALTIME, NULL);
        channel->source = source_new (0, NULL); // TODO free!
        channel->encoder_array = g_array_new (FALSE, FALSE, sizeof(gpointer)); //TODO: free!
        channel->worker_pid = 0;
        channel->age = 0;
}

static void
set_enable_property (Channel *channel)
{
        gchar *enable;

        enable = (gchar *)gst_structure_get_string (channel->configure, "enable");
        if (g_strcmp0 (enable, "no") == 0) {
                channel->enable = FALSE;
        } else if (g_strcmp0 (enable, "yes") == 0) {
                channel->enable = TRUE;
        }
}

static void
channel_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
        g_return_if_fail(IS_CHANNEL(obj));

        switch(prop_id) {
        case CHANNEL_PROP_NAME:
                CHANNEL(obj)->name = (gchar *)g_value_dup_string (value); //TODO: should release dup string config_file_path?
                break;
        case CHANNEL_PROP_CONFIGURE:
                CHANNEL(obj)->configure = (GstStructure *)g_value_get_pointer (value); //TODO: should release dup string config_file_path?
                set_enable_property (CHANNEL(obj));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
                break;
        }
}

static void
channel_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
        Channel *channel = CHANNEL(obj);

        switch(prop_id) {
        case CHANNEL_PROP_NAME:
                g_value_set_string (value, channel->name);
                break;
        case CHANNEL_PROP_CONFIGURE:
                g_value_set_pointer (value, channel->configure);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
                break;
        }
}

GType
channel_get_type (void)
{
        static GType type = 0;

        if (type) return type;
        static const GTypeInfo info = {
                sizeof (ChannelClass),
                NULL, // base class initializer
                NULL, // base class finalizer
                (GClassInitFunc)channel_class_init,
                NULL,
                NULL,
                sizeof (Channel),
                0,
                (GInstanceInitFunc)channel_init,
                NULL
        };
        type = g_type_register_static (G_TYPE_OBJECT, "Channel", &info, 0);

        return type;
}

static void
print_one_tag (const GstTagList * list, const gchar * tag, gpointer user_data)
{
        int i, num;

        num = gst_tag_list_get_tag_size (list, tag);

        for (i = 0; i < num; ++i) {
                const GValue *val;

                /* Note: when looking for specific tags, use the g_tag_list_get_xyz() API,
                * we only use the GValue approach here because it is more generic */
                val = gst_tag_list_get_value_index (list, tag, i);
                if (G_VALUE_HOLDS_STRING (val)) {
                        GST_INFO ("%20s : %s", tag, g_value_get_string (val));
                } else if (G_VALUE_HOLDS_UINT (val)) {
                        GST_INFO ("%20s : %u", tag, g_value_get_uint (val));
                } else if (G_VALUE_HOLDS_DOUBLE (val)) {
                        GST_INFO ("%20s : %g", tag, g_value_get_double (val));
                } else if (G_VALUE_HOLDS_BOOLEAN (val)) {
                        GST_INFO ("%20s : %s", tag, (g_value_get_boolean (val)) ? "true" : "false");
                } else if (GST_VALUE_HOLDS_BUFFER (val)) {
                        GST_INFO ("%20s : buffer of size %u", tag, GST_BUFFER_SIZE (gst_value_get_buffer (val)));
                } else if (GST_VALUE_HOLDS_DATE (val)) {
                        GST_INFO ("%20s : date (year=%u,...)", tag, g_date_get_year (gst_value_get_date (val)));
                } else {
                        GST_INFO ("%20s : tag of type '%s'", tag, G_VALUE_TYPE_NAME (val));
                }
        }
}

static gboolean
bus_callback (GstBus *bus, GstMessage *msg, gpointer user_data)
{
        gchar *debug;
        GError *error;
        GstState old, new, pending;
        GstStreamStatusType type;
        GstClock *clock;
        GstTagList *tags;
        GObject *object = user_data;
        GValue state = { 0, }, name = { 0, };

        g_value_init (&name, G_TYPE_STRING);
        g_object_get_property (object, "name", &name);

        switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_EOS:
                GST_INFO ("End of stream\n");
                break;
        case GST_MESSAGE_TAG:
                GST_INFO ("TAG");
                gst_message_parse_tag (msg, &tags);
                gst_tag_list_foreach (tags, print_one_tag, NULL);
                break;
        case GST_MESSAGE_ERROR: 
                gst_message_parse_error (msg, &error, &debug);
                g_free (debug);
                GST_WARNING ("%s error: %s", g_value_get_string (&name), error->message);
                g_error_free (error);
                break;
        case GST_MESSAGE_STATE_CHANGED:
                g_value_init (&state, G_TYPE_INT);
                gst_message_parse_state_changed (msg, &old, &new, &pending);
                GST_INFO ("pipeline %s element %s state from %s to %s", g_value_get_string (&name), GST_MESSAGE_SRC_NAME (msg), gst_element_state_get_name (old), gst_element_state_get_name (new));
                if (g_strcmp0 (g_value_get_string (&name), GST_MESSAGE_SRC_NAME (msg)) == 0) {
                        GST_INFO ("pipeline %s state change to %s", g_value_get_string (&name), gst_element_state_get_name (new));
                        g_value_set_int (&state, new);
                        g_object_set_property (object, "state", &state);
                        g_value_unset (&state);
                }
                break;
        case GST_MESSAGE_STREAM_STATUS:
                gst_message_parse_stream_status (msg, &type, NULL);
                GST_INFO ("stream status %d", type);
                break;
        case GST_MESSAGE_NEW_CLOCK:
                gst_message_parse_new_clock (msg, &clock);
                GST_INFO ("New source clock %s", GST_OBJECT_NAME (clock));
                break;
        case GST_MESSAGE_ASYNC_DONE:
                GST_INFO ("source %s message: %s", g_value_get_string (&name), GST_MESSAGE_TYPE_NAME (msg));
                //gst_bin_recalculate_latency (GST_BIN (encoder->pipeline));
                break;
        default:
                GST_INFO ("%s message: %s", g_value_get_string (&name), GST_MESSAGE_TYPE_NAME (msg));
        }

        g_value_unset (&name);
        return TRUE;
}

static gchar**
get_property_names (gchar *param)
{
        gchar *p1, *p2, **pp, **pp1;
        GRegex *regex;

        /* strip space at begin */
        regex = g_regex_new ("[^ ]* *(.*)", 0, 0, NULL);
        p1 = g_regex_replace (regex, param, -1, 0, "\\1", 0, NULL);
        GST_INFO ("param: %s, property: %s", param, p1);
        g_regex_unref (regex);
        /* strip space beside = */
        regex = g_regex_new ("( *= *)", 0, 0, NULL);
        p2 = g_regex_replace (regex, p1, -1, 0, "=", 0, NULL);
        g_free (p1);
        g_regex_unref (regex);
        GST_INFO ("param: %s, property: %s", param, p2);
        /* strip redundant space */
        regex = g_regex_new ("( +)", 0, 0, NULL);
        p1 = g_regex_replace (regex, p2, -1, 0, " ", 0, NULL);
        g_free (p2);
        g_regex_unref (regex);
        GST_INFO ("param: %s, property: %s\n", param, p1);
        pp = g_strsplit_set (p1, " ", 0);
        g_free (p1);

        pp1 = pp;
        while (*pp1 != NULL) {
                if (g_strrstr (*pp1, "=") == NULL) {
                        GST_ERROR ("Configure error: %s", *pp1);
                        g_strfreev (pp);
                        return NULL;
                }
                regex = g_regex_new ("([^=]*)=.*", 0, 0, NULL);
                p1 = g_regex_replace (regex, *pp1, -1, 0, "\\1", 0, NULL);
                g_free (*pp1);
                g_regex_unref (regex);
                *pp1 = p1;
                pp1++;
        }

        return pp;
}

/*
 * return new allocated property value or NULL.
 */
static gchar*
get_property_value (gchar *param, gchar *property)
{
        gchar *r, *v;
        GRegex *regex;

        r = g_strdup_printf (".* *%s *= *([^ ]*).*", property);
        regex = g_regex_new (r, 0, 0, NULL);
        v = g_regex_replace (regex, param, -1, 0, "\\1", 0, NULL);
        g_regex_unref (regex);
        g_free (r);
        if (g_strcmp0 (param, v) == 0) {
                g_free (v);
                v = NULL;
        }

        return v;
}

static gboolean
set_element_property (GstElement *element, gchar* name, gchar* value)
{
        GParamSpec *param_spec;
        GValue v = { 0, };

        param_spec = g_object_class_find_property (G_OBJECT_GET_CLASS (element), name);
        if (param_spec == NULL) {
                GST_ERROR ("Can't find property name: %s", name);
                return FALSE;
        }

        g_value_init (&v, param_spec->value_type);
        gst_value_deserialize (&v, value);
        g_object_set_property (G_OBJECT (element), param_spec->name, &v);

        return TRUE;
}

/**
 * create_element
 * @configure: configure object.
 * @param: like this: /server/httpstreaming
 *
 * create element, based on GstStructure type element info.
 *
 * Returns: the new created element or NULL.
 */
GstElement *
create_element (GstStructure *pipeline, gchar *param)
{
        GstElement *element;
        GValue *value;
        gint n, i;
        gchar *factory, *name, *p, **pp, **pp1;
        GstStructure *structure, *property;
        GRegex *regex;

        /* extract factory */
        regex = g_regex_new ("([^ ]*).*", 0, 0, NULL);
        p = g_regex_replace (regex, param, -1, 0, "\\1", 0, NULL);
        g_regex_unref (regex);
        regex = g_regex_new (".*/(.*)", 0, 0, NULL);
        factory = g_regex_replace (regex, p, -1, 0, "\\1", 0, NULL);
        g_regex_unref (regex);
        g_free (p);
        element = gst_element_factory_make (factory, NULL);

        /* extract element configure. */
        regex = g_regex_new (" .*", 0, 0, NULL);
        p = g_regex_replace (regex, param, -1, 0, "", 0, NULL);
        g_regex_unref (regex);
        //g_print ("create element, param: %s, fatory: %s name: %s conf path: %s\n", param, factory, name, p);
        value = (GValue *)gst_structure_get_value (pipeline, "elements");
        structure = (GstStructure *)gst_value_get_structure (value);
        value = (GValue *)gst_structure_get_value (structure, p);
        g_free (p);
        g_free (factory);

        if (value != NULL) {
                /* set propertys in element property configure. */
                structure = (GstStructure *)gst_value_get_structure (value);
                if (gst_structure_has_field (structure, "property")) {
                        value = (GValue *)gst_structure_get_value (structure, "property");
                        if (!GST_VALUE_HOLDS_STRUCTURE (value)) {
                                GST_ERROR ("elements property should be structure.\n");
                                gst_object_unref (GST_OBJECT (element));
                                return NULL;
                        }
                        property = (GstStructure *)gst_value_get_structure (value);
                        n = gst_structure_n_fields (property);
                        for (i = 0; i < n; i++) {
                                name = (gchar *)gst_structure_nth_field_name (property, i);
                                p = (gchar *)gst_structure_get_string (property, name);
                                if (!set_element_property (element, name, p)) {
                                        GST_ERROR ("Set property error %s=%s", name, p);
                                        return NULL;
                                }
                                GST_INFO ("Set property: %s = %s.", name, p);
                        }
                }
        }

        /* set element propertys configured in bin definition. */
        pp = get_property_names (param);
        if (pp == NULL) {
                gst_object_unref (element);
                g_strfreev (pp);
                return NULL;
        }
        pp1 = pp;
        while (*pp1 != NULL) {
                p = get_property_value (param, *pp1);
                if (p == NULL) {
                        GST_ERROR ("Configure error: %s=%s", *pp1, p);
                        gst_object_unref (element);
                        g_strfreev (pp);
                        return NULL;
                }
                if (!set_element_property (element, *pp1, p)) {
                        GST_ERROR ("Set property error: %s=%s", *pp1, p);
                        return NULL;
                }
                g_free (p);
                pp1++;
        }
        g_strfreev (pp);

        return element;
}

static gchar *
get_caps (GstStructure *configure, gchar *name)
{
        GstStructure *structure;
        GValue *value;
        gchar *p;

        p = NULL;
        value = (GValue *)gst_structure_get_value (configure, "elements");
	structure = (GstStructure *)gst_value_get_structure (value);
        value = (GValue *)gst_structure_get_value (structure, name);
        if (value == NULL) {
                return NULL;
        }
        structure = (GstStructure *)gst_value_get_structure (value);
        if (gst_structure_has_field (structure, "caps")) {
                p = (gchar *)gst_structure_get_string (structure, "caps");
                GST_INFO ("%s caps found: %s", name, p);
        }

        return p;
}

static void
pad_added_callback (GstElement *src, GstPad *pad, gpointer data)
{
        gchar *src_pad_name;
        Bin *bin = data;
        GstCaps *caps;
        GSList *elements, *links;
        GstElement *element, *pipeline;
        Link *link;
        gchar *p;

        src_pad_name = gst_pad_get_name (pad);
        if (g_strcmp0 (src_pad_name, bin->previous->src_pad_name) != 0) {
                GST_INFO ("new added pad name: %s, delayed src pad name %s, not match.", src_pad_name, bin->previous->src_pad_name);
                return;
        }

        pipeline = (GstElement *)gst_element_get_parent (src);

        links = bin->links;
        while (links != NULL) {
                link = links->data;
                GST_INFO ("Link %s -> %s", link->src_name, link->sink_name);
                p = get_caps (bin->configure, link->src_name);
                if (p != NULL) {
                        caps = gst_caps_from_string (p);
                        gst_element_link_filtered (link->src, link->sink, caps);
                        gst_caps_unref (caps);
                } else {
                        gst_element_link (link->src, link->sink);
                }
                links = links->next;
        }

        caps = gst_pad_get_caps (pad);
        GST_INFO ("caps: %s", gst_caps_to_string (caps));
        if (gst_element_link_pads_filtered (src, bin->previous->src_pad_name, bin->previous->sink, NULL, caps)) {
                GST_INFO ("new added pad name: %s, delayed src pad name %s. ok!", src_pad_name, bin->previous->src_pad_name);
                g_signal_handler_disconnect (src, bin->signal_id);
        } else {
                GST_WARNING ("new added pad name: %s, delayed src pad name %s. failure!", src_pad_name, bin->previous->src_pad_name);
        }

        g_free (src_pad_name);
}

static void
free_bin (Bin *bin)
{
}

/*
 * muser. or demuxer.video_0024 style return TRUE, else return FALSE.
 */
static gboolean
is_pad (gchar *element)
{
        gchar *p;

        p = element;
        while (*p != '\0') {
                if (*p == '.') {
                        return TRUE;
                }
                if (*p == ' ') {
                        return FALSE;
                }
                p++;
        }

        return FALSE;
}

/*
 * is_element_selected
 *
 * is element optional and be selected, or it's not optional.
 *
 * Returns: TRUE if selected, otherwise FALSE.
 */
static gboolean
is_element_selected (GstStructure *pipeline, gchar *element)
{
        GValue *value;
        GstStructure *structure;
        gchar *p, **pp;

        /* value of /elements/name/option. */
        value = (GValue *)gst_structure_get_value (pipeline, "elements");
        structure = (GstStructure *)gst_value_get_structure (value);
        pp = g_strsplit (element, " ", 0);
        value = (GValue *)gst_structure_get_value (structure, pp[0]);
        g_strfreev (pp);
        if (value == NULL) {
                return TRUE;
        }
        structure = (GstStructure *)gst_value_get_structure (value);
        value = (GValue *)gst_structure_get_value (structure, "option");
        if (value == NULL) {
                return TRUE;
        }
        p = (gchar *)g_value_get_string (value);
        if (g_strcmp0 (p, "yes") == 0) {
                return TRUE;
        } else {
                return FALSE;
        }
}

/*
 * is_bin_selected
 *
 * is bin optional and be selected, or it's not optional.
 *
 * Returns: TRUE if selected, otherwise FALSE.
 */
static gboolean
is_bin_selected (GstStructure *pipeline, gchar *bin)
{
        GValue *value;
        GstStructure *structure;
        gchar *p;

        /* value of bins/name/option */
        value = (GValue *)gst_structure_get_value (pipeline, "bins");
        structure = (GstStructure *)gst_value_get_structure (value);
        value = (GValue *)gst_structure_get_value (structure, bin);
        structure = (GstStructure *)gst_value_get_structure (value);
        value = (GValue *)gst_structure_get_value (structure, "option");
        if (value == NULL) {
                return TRUE;
        }
        p = (gchar *)g_value_get_string (value);
        if (g_strcmp0 (p, "yes") == 0) {
                return TRUE;
        } else {
                return FALSE;
        }
}

/*
 * get_bin_definition
 *
 * Returns:the definition of the bin. 
 */
static gchar*
get_bin_definition (GstStructure *pipeline, gchar *bin)
{
        GValue *value;
        GstStructure *structure;
        gchar *p;

        /* value of bins/name/option */
        value = (GValue *)gst_structure_get_value (pipeline, "bins");
        structure = (GstStructure *)gst_value_get_structure (value);
        value = (GValue *)gst_structure_get_value (structure, bin);
        structure = (GstStructure *)gst_value_get_structure (value);
        value = (GValue *)gst_structure_get_value (structure, "definition");
        p = (gchar *)g_value_get_string (value);

        return p;
}

static GstElement*
pickup_element (GSList *list, gchar *name)
{
        GSList *elements, *bins;
        Bin *bin;
        GstElement *element;

        bins = list;
        while (bins != NULL) {
                bin = bins->data;
                elements = bin->elements;
                while (elements != NULL) {
                        element = elements->data;
                        if (g_strcmp0 (gst_element_get_name (element), name) == 0) {
                                return element;
                        }
                        elements = elements->next;
                }
                bins = bins->next;
        }

        return NULL;
}

/**
 * get_bins
 * @configure: Configure object.
 * @param: like this: /server/httpstreaming
 *
 * Returns: the pipeline bins.
 */
GSList *
get_bins (GstStructure *structure)
{
        GValue *value;
        GstElement *element, *src;
        gchar *name, *p, *p1, **pp, **pp1, *src_name, *src_pad_name;
        gint i, n;
        Bin *bin;
        Link *link;
        GSList *list;
        GstStructure *bins;

        list = NULL;
        /* bin */
        value = (GValue *)gst_structure_get_value (structure, "bins");
        bins = (GstStructure *)gst_value_get_structure (value);
        n = gst_structure_n_fields (bins);
        for (i = 0; i < n; i++) {
                name = (gchar *)gst_structure_nth_field_name (bins, i);
                if (!is_bin_selected (structure, name)) {
                        g_print ("skip bin %s\n", name);
                        continue;
                }
                bin = g_slice_new (Bin);
                bin->name = name;
                bin->links = NULL;
                bin->elements = NULL;
                bin->previous = NULL;
                bin->configure = structure;
                p = get_bin_definition (structure, name);
                //g_print ("p: %s\n", p);
                pp = pp1 = g_strsplit (p, "!", 0);
                src = NULL;
                src_name = NULL;
                src_pad_name = NULL;
                while (*pp != NULL) {
                        p1 = g_strdup (*pp);
                        p1 = g_strstrip (p1);
                        if (is_pad (p1)) {
                                if (src == NULL) {
                                        /* should be a sometimes pad */
                                        src_name = g_strndup (p1, g_strrstr (p1, ".") - p1);
                                        src_pad_name = g_strndup (g_strrstr (p1, ".") + 1, strlen (p1) - strlen (src_name) -1);
                                } else {
                                        /* should be a request pad */
                                        link = g_slice_new (Link);
                                        link->src = src;
                                        link->src_name = src_name;
                                        link->src_pad_name = src_pad_name;
                                        link->sink = NULL;
                                        link->sink_name = g_strndup (p1, g_strrstr (p1, ".") - p1);
                                        link->sink_pad_name = g_strndup (g_strrstr (p1, ".") + 1, strlen (p1) - strlen (link->sink_name) -1);
                                        bin->links = g_slist_append (bin->links, link);
                                }
                        } else if (is_element_selected (structure, p1)) {
                                /* plugin name, create a element. */
                                element = create_element (structure, p1);
                                if (element != NULL) {
                                        if (src_name != NULL) {
                                                link = g_slice_new (Link);
                                                link->src = src;
                                                link->src_name = src_name;
                                                link->src_pad_name = src_pad_name;
                                                link->sink = element;
                                                link->sink_name = p1;
                                                link->sink_pad_name = NULL;
                                                if (src_pad_name == NULL) {
                                                        bin->links = g_slist_append (bin->links, link);
                                                } else {
                                                        bin->previous = link;
                                                }
                                        } else {
                                                bin->first = element;
                                        }
                                        bin->elements = g_slist_append (bin->elements, element);
                                        src = element;
                                        src_name = p1;
                                        GST_INFO ("element_name: %s", src_name);
                                        src_pad_name = NULL;
                                } else {
                                        GST_ERROR ("error create element %s", *pp);
                                        g_free (p);
                                        g_strfreev (pp1);
                                        return NULL; //FIXME release pipeline
                                }
                        } else {
                                g_free (p1);
                        }
                        pp++;
                }
                bin->last = element;
                list = g_slist_append (list, bin);
                g_strfreev (pp1);
        }

        return list;
}

static SourceStream*
source_get_stream (Source *source, gchar *name)
{
        SourceStream *stream;
        gint i;

        for (i = 0; i < source->streams->len; i++) {
                stream = g_array_index (source->streams, gpointer, i);
                if (g_strcmp0 (stream->name, name) == 0) {
                        break;
                }
        }

        return stream;
}

static GstFlowReturn
source_appsink_callback (GstAppSink *elt, gpointer user_data)
{
        GstBuffer *buffer;
        SourceStream *stream = (SourceStream *)user_data;
        EncoderStream *encoder;
        gint i;
        GstCaps *caps;
        gchar *str;

        buffer = gst_app_sink_pull_buffer (GST_APP_SINK (elt));
        *(stream->last_heartbeat) = gst_clock_get_time (stream->system_clock);
        if (stream->current_position == -1) {
                /* set stream type */
                caps = GST_BUFFER_CAPS (buffer);
                str = gst_caps_to_string (caps);
                if (g_str_has_prefix (str, "video")) {
                        *(stream->type) = ST_VIDEO;
                } else if (g_str_has_prefix (str, "audio")) {
                        *(stream->type) = ST_AUDIO;
                }
                g_free (str);
        }
        stream->current_position = (stream->current_position + 1) % SOURCE_RING_SIZE;

        /* output running status */
        GST_DEBUG ("%s current position %d, buffer duration: %d", stream->name, stream->current_position, GST_BUFFER_DURATION(buffer));
        for (i = 0; i < stream->encoders->len; i++) {
                encoder = g_array_index (stream->encoders, gpointer, i);
                if (stream->current_position == encoder->current_position) {
                        GST_WARNING ("encoder %s stream %s can not catch up output.", encoder->name, stream->name);
                }
        }

        /* out a buffer */
        if (stream->ring[stream->current_position] != NULL) {
                gst_buffer_unref (stream->ring[stream->current_position]);
        }
        stream->ring[stream->current_position] = buffer;
        *(stream->current_timestamp) = GST_BUFFER_TIMESTAMP (buffer);
}

/**
 * create_source_pipeline
 * @configure: Configure object.
 * @param: like this: /server/httpstreaming
 *
 * Returns: the cteated pipeline or NULL.
 */
static GstElement *
create_source_pipeline (Source *source)
{
        GValue *value;
        GstStructure *structure;
        GstElement *pipeline, *element;
        Bin *bin;
        Link *link;
        GSList *bins, *links, *elements;
        GstAppSinkCallbacks appsink_callbacks = {
                NULL,
                NULL,
                source_appsink_callback,
                NULL
        };
        GstElementFactory *element_factory;
        GType type;
        SourceStream *stream;
        gchar *p;
        GstCaps *caps;
        GstBus *bus;

        pipeline = gst_pipeline_new (NULL);

        bins = source->bins;
        while (bins != NULL) {
                bin = bins->data;

                /* add element to pipeline */
                elements = bin->elements;
                while (elements != NULL) {
                        element = elements->data;
                        gst_bin_add (GST_BIN (pipeline), element);
                        elements = g_slist_next (elements);
                }

                if (bin->previous == NULL) {
                        /* links element */
                        links = bin->links;
                        while (links != NULL) {
                                link = links->data;
                                GST_INFO ("link %s -> %s", link->src_name, link->sink_name);
                                p = get_caps (source->configure, link->src_name);
                                if (p != NULL) {
                                        caps = gst_caps_from_string (p);
                                        gst_element_link_filtered (link->src, link->sink, caps);
                                        gst_caps_unref (caps);
                                } else {
                                        gst_element_link (link->src, link->sink);
                                }
                                links = g_slist_next (links);
                        }
                } else {
                        /* delayed sometimes pad link. */
                        element = pickup_element (source->bins, bin->previous->src_name);
                        bin->signal_id = g_signal_connect_data (element, "pad-added", G_CALLBACK (pad_added_callback), bin, (GClosureNotify)free_bin, (GConnectFlags) 0);
                        GST_INFO ("Delayed link %s -> %s", bin->previous->src_name, bin->name);
                }
                        
                /* new stream, set appsink output callback. */
                element = bin->last;
                element_factory = gst_element_get_factory (element);
                type = gst_element_factory_get_element_type (element_factory);
                if (g_strcmp0 ("GstAppSink", g_type_name (type)) == 0) {
                        stream = source_get_stream (source, bin->name);
                        gst_app_sink_set_callbacks (GST_APP_SINK (element), &appsink_callbacks, stream, NULL);
                        GST_INFO ("Set callbacks for bin %s -> %s", bin->name);
                }

                bins = g_slist_next (bins);
        }

        bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
        gst_bus_add_watch (bus, bus_callback, source);
        g_object_unref (bus);

        return pipeline;
}

/*
 * complete_request_element
 *
 * request element link.
 * 
 */
static gboolean
complete_request_element (GSList *bins)
{
        GSList *l1, *l2, *l3, *l4;
        Bin *bin, *bin2;
        Link *link;
        GstElement *element;

        l1 = bins;
        while (l1 != NULL) {
                bin = l1->data;
                l2 = bin->links;
                while (l2 != NULL) {
                        link = l2->data;
                        if (link->sink == NULL) {
                                GST_INFO ("Request element link: %s -> %s", link->src_name, link->sink_name);
                                l3 = bins;
                                while (l3 != NULL) {
                                        bin2 = l3->data;
                                        l4 = bin2->elements;
                                        while (l4 != NULL) {
                                                element = l4->data;
                                                l4 = g_slist_next (l4);
                                                if (g_strcmp0 (link->sink_name, gst_element_get_name (element)) == 0) {
                                                        link->sink = element;
                                                }
                                        }
                                        l3 = g_slist_next (l3);
                                }
                        }
                        l2 = g_slist_next (l2);
                }
                l1 = g_slist_next (l1);
        }
}

static EncoderStream*
encoder_get_stream (Encoder *encoder, gchar *name)
{
        EncoderStream *stream;
        gint i;

        for (i = 0; i < encoder->streams->len; i++) {
                stream = g_array_index (encoder->streams, gpointer, i);
                if (g_strcmp0 (stream->name, name) == 0) {
                        break;
                }
        }

        return stream;
}

/*
 * memory between tail and head is free.
 */
static gint
cache_free (Encoder *encoder)
{
        if (*(encoder->head_addr) > *(encoder->tail_addr)) {
                return *(encoder->head_addr) - *(encoder->tail_addr);
        } else {
                return (*(encoder->head_addr) - encoder->cache_addr) + (encoder->cache_end_addr - *(encoder->tail_addr));
        }
}

/*
 * move head_addr to next gop.
 */
static void
move_head (Encoder *encoder)
{
        gint gop_size, n;

        /* head gop size. */
        if (*(encoder->head_addr) + 12 < encoder->cache_end_addr) {
                memcpy (&gop_size, *(encoder->head_addr) + 8, 4);
        } else {
                n = encoder->cache_end_addr - *(encoder->head_addr) - 8;
                memcpy (&gop_size, *(encoder->head_addr), n);
                memcpy (&gop_size + n, encoder->cache_addr, 4 - n);
        }

        /* move head. */
        if (*(encoder->head_addr) + gop_size < encoder->cache_end_addr) {
                *(encoder->head_addr) += gop_size;
        } else {
                *(encoder->head_addr) =  *(encoder->head_addr) + gop_size - *(encoder->cache_end_addr) + *(encoder->cache_addr);
        }
}

/*
 * move last random access point address.
 */
static void
move_last_rap (Encoder *encoder, GstBuffer *buffer)
{
        gchar buf[12];
        gint32 size, n;

        /* calculate and write gop size. */
        if (*(encoder->tail_addr) >= *(encoder->last_rap_addr)) {
                size = *(encoder->tail_addr) - *(encoder->last_rap_addr);
        } else {
                size = encoder->cache_end_addr - *(encoder->last_rap_addr) + *(encoder->tail_addr) - encoder->cache_addr;
        }
        if (*(encoder->last_rap_addr) + 12 <= encoder->cache_end_addr) {
                memcpy (*(encoder->last_rap_addr) + 8, &size, 4);
        } else {
                n = encoder->cache_end_addr - *(encoder->last_rap_addr) - 8;
                memcpy (*(encoder->last_rap_addr), &size, n);
                memcpy (encoder->cache_addr, &size + n, 4 - n);
        }

        /* new gop timestamp, 4bytes reservation for gop size. */
        if (*(encoder->tail_addr) < encoder->cache_end_addr) {
                *(encoder->last_rap_addr) = *(encoder->tail_addr);
        } else {
                *(encoder->last_rap_addr) = encoder->cache_addr;
        }
        memcpy (buf, &(GST_BUFFER_TIMESTAMP (buffer)), 8);
        size = 0;
        memcpy (buf + 8, &size, 4);
        if (*(encoder->tail_addr) + 12 < encoder->cache_end_addr) {
                memcpy (*(encoder->tail_addr), buf, 12);
                *(encoder->tail_addr) += 12;
        } else {
                n = encoder->cache_end_addr - *(encoder->tail_addr);
                memcpy (*(encoder->tail_addr), buf, n);
                memcpy (encoder->cache_addr, buf + n, 12 - n);
                *(encoder->tail_addr) = encoder->cache_addr + 12 - n;
        }
}

static void
copy_buffer (Encoder *encoder, GstBuffer *buffer)
{
        gint size;

        if (*(encoder->tail_addr) + GST_BUFFER_SIZE (buffer) < encoder->cache_end_addr) {
                memcpy (*(encoder->tail_addr), GST_BUFFER_DATA (buffer), GST_BUFFER_SIZE (buffer));
                *(encoder->tail_addr) = *(encoder->tail_addr) + GST_BUFFER_SIZE (buffer);
        } else {
                size = encoder->cache_end_addr - *(encoder->tail_addr);
                memcpy (*(encoder->tail_addr), GST_BUFFER_DATA (buffer), size);
                memcpy (encoder->cache_addr, GST_BUFFER_DATA (buffer) + size, GST_BUFFER_SIZE (buffer) - size);
                *(encoder->tail_addr) = encoder->cache_addr + GST_BUFFER_SIZE (buffer) - size;
        }
}

static GstFlowReturn
encoder_appsink_callback (GstAppSink * elt, gpointer user_data)
{
        GstBuffer *buffer;
        Encoder *encoder = (Encoder *)user_data;
        gint i;

        buffer = gst_app_sink_pull_buffer (GST_APP_SINK (elt));
        GST_DEBUG ("%s current position %d, buffer duration: %d", encoder->name, encoder->output_position, GST_BUFFER_DURATION(buffer));
        i = encoder->output_position + 1;
        i = i % ENCODER_RING_SIZE;
        encoder->output_position = i;
        if (encoder->output_ring[i] != NULL) {
                gst_buffer_unref (encoder->output_ring[i]);
        }
        encoder->output_ring[i] = buffer;
        (*(encoder->total_count))++;
        
        /* update head_addr, free enough memory for current buffer. */
        while (cache_free (encoder) < GST_BUFFER_SIZE (buffer) + 12) { /* timestamp + gop size = 12 */
                move_head (encoder);
        }

        if (!GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT)) {
                /* 
                 * random access point found.
                 * write previous gop size to 4 bytes reservation,
                 * write current gop timestamp,
                 * reserve 4 bytes for size of current gop,
                 */
                move_last_rap (encoder, buffer);
        }

        if (*(encoder->head_addr) == *(encoder->tail_addr)) {
                GST_WARNING ("Discard buffer before first IDR.");
                return;
        }

        /*
         * copy buffer to cache.
         * update tail_addr and last_rap_addr
         */
        copy_buffer (encoder, buffer);
}

static void
encoder_appsrc_need_data_callback (GstAppSrc *src, guint length, gpointer user_data)
{
        EncoderStream *stream = (EncoderStream *)user_data;
        gint current_position;
        GstCaps *caps;

        current_position = (stream->current_position + 1) % SOURCE_RING_SIZE;
        for (;;) {
                *(stream->last_heartbeat) = gst_clock_get_time (stream->system_clock);
                /* insure next buffer isn't current buffer */
                if (current_position == stream->source->current_position ||
                        stream->source->current_position == -1) { /*FIXME: condition variable*/
                        GST_DEBUG ("waiting %s source ready", stream->name);
                        g_usleep (50000); /* wiating 50ms */
                        continue;
                }

                /* first buffer, set caps. */
                if (stream->current_position == -1) {
                        caps = GST_BUFFER_CAPS (stream->source->ring[0]);
                        gst_app_src_set_caps (src, caps);
                        GST_INFO ("set stream %s caps: %s", stream->name, gst_caps_to_string (caps));
                }

                GST_DEBUG ("%s encoder position %d; timestamp %" GST_TIME_FORMAT " source position %d",
                        stream->name,   
                        stream->current_position,
                        GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (stream->source->ring[stream->current_position])),
                        stream->source->current_position);

                if (gst_app_src_push_buffer (src, gst_buffer_ref (stream->source->ring[current_position])) != GST_FLOW_OK) {
                        GST_ERROR ("%s, gst_app_src_push_buffer failure.", stream->name);
                }
                *(stream->current_timestamp) = GST_BUFFER_TIMESTAMP (stream->source->ring[current_position]);
                break;
        }
        stream->current_position = current_position;
}

/**
 * create_encoder_pipeline
 * @configure: Configure object.
 * @param: like this: /server/httpstreaming
 *
 */
static void
create_encoder_pipeline (Encoder *encoder) //FIXME return failure
{
        GValue *value;
        GstElement *pipeline, *element;
        Bin *bin;
        Link *link;
        GSList *bins, *links, *elements;
        GstElementFactory *element_factory;
        GType type;
        EncoderStream *stream;
        GstAppSrcCallbacks callbacks = {
                encoder_appsrc_need_data_callback,
                NULL,
                NULL
        };
        GstAppSinkCallbacks encoder_appsink_callbacks = {
                NULL,
                NULL,
                encoder_appsink_callback,
                NULL
        };
        gchar *p;
        GstCaps *caps;
        GstBus *bus;
 
        pipeline = gst_pipeline_new (NULL);

        /* add element to pipeline first. */
        bins = encoder->bins;
        while (bins != NULL) {
                bin = bins->data;
                elements = bin->elements;
                while (elements != NULL) {
                        element = elements->data;
                        if (!gst_bin_add (GST_BIN (pipeline), element)) {
                                GST_ERROR ("add element %s to bin %s error.", gst_element_get_name (element), bin->name);
                        }
                        elements = g_slist_next (elements);
                }
                bins = g_slist_next (bins);
        }

        /* then links element. */
        bins = encoder->bins;
        while (bins != NULL) {
                bin = bins->data;
                element = bin->first;
                element_factory = gst_element_get_factory (element);
                type = gst_element_factory_get_element_type (element_factory);
                if (g_strcmp0 ("GstAppSrc", g_type_name (type)) == 0) {
                        GST_INFO ("Encoder appsrc found.");
                        stream = encoder_get_stream (encoder, bin->name);
                        gst_app_src_set_callbacks (GST_APP_SRC (element), &callbacks, stream, NULL);
                }
                element = bin->last;
                element_factory = gst_element_get_factory (element);
                type = gst_element_factory_get_element_type (element_factory);
                if (g_strcmp0 ("GstAppSink", g_type_name (type)) == 0) {
                        GST_INFO ("Encoder appsink found.");
                        gst_app_sink_set_callbacks (GST_APP_SINK(element), &encoder_appsink_callbacks, encoder, NULL);
                }
                links = bin->links;
                while (links != NULL) {
                        link = links->data;
                        GST_INFO ("link element: %s -> %s", link->src_name, link->sink_name);
                        p = get_caps (encoder->configure, link->src_name);
                        if (p != NULL) {
                                caps = gst_caps_from_string (p);
                                gst_element_link_filtered (link->src, link->sink, caps);
                                gst_caps_unref (caps);
                        } else {
                                gst_element_link (link->src, link->sink);
                        }
                        links = g_slist_next (links);
                }
                bins = g_slist_next (bins);
        }

        bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
        gst_bus_add_watch (bus, bus_callback, encoder);
        g_object_unref (bus);

        encoder->pipeline = pipeline;
}

static gint
channel_source_extract_streams (Source *source)
{
        GRegex *regex;
        GMatchInfo *match_info;
        SourceStream *stream;
        GstStructure *structure, *bins, *bin;
        GValue *value;
        gint i, n;
        gchar *name, *definition, *option;

        structure = source->configure;
        value = (GValue *)gst_structure_get_value (structure, "bins");
        bins = (GstStructure *)gst_value_get_structure (value);
        n = gst_structure_n_fields (bins);
        for (i = 0; i < n; i++) {
                name = (gchar *)gst_structure_nth_field_name (bins, i);
                value = (GValue *)gst_structure_get_value (bins, name);
                bin = (GstStructure *)gst_value_get_structure (value);
                option = (gchar *)gst_structure_get_string (bin, "option");
                if ((option != NULL) && (g_strcmp0 (option, "no") == 0)) {
                        GST_WARNING ("Source %s's %s option set to no.", source->name, name);
                        continue;
                }
                definition = (gchar *)gst_structure_get_string (bin, "definition");
                regex = g_regex_new ("! *appsink[^!]*$", G_REGEX_OPTIMIZE, 0, NULL);
                g_regex_match (regex, definition, 0, &match_info);
                g_regex_unref (regex);
                if (g_match_info_matches (match_info)) {
                        stream = (SourceStream *)g_malloc (sizeof (SourceStream));
                        stream->name = name;
                        GST_INFO ("stream found %s: %s", name, definition);
                        g_array_append_val (source->streams, stream);
                }
        }
}

static gint
channel_encoder_extract_streams (Encoder *encoder)
{
        GstStructure *structure, *bins, *bin;
        GValue *value;
        GRegex *regex;
        GMatchInfo *match_info;
        EncoderStream *stream;
        gint i, n;
        gchar *name, *definition, *option;

        structure = encoder->configure;
        value = (GValue *)gst_structure_get_value (structure, "bins");
        bins = (GstStructure *)gst_value_get_structure (value);
        n = gst_structure_n_fields (bins);
        for (i = 0; i < n; i++) {
                name = (gchar *)gst_structure_nth_field_name (bins, i);
                value = (GValue *)gst_structure_get_value (bins, name);
                bin = (GstStructure *)gst_value_get_structure (value);
                option = (gchar *)gst_structure_get_string (bin, "option");
                if ((option != NULL) && (g_strcmp0 (option, "no") == 0)) {
                        GST_WARNING ("Encoder %s's option set to no.", name);
                        continue;
                }
                definition = (gchar *)gst_structure_get_string (bin, "definition");
                regex = g_regex_new ("appsrc name=(?<stream>[^ ]*)", G_REGEX_OPTIMIZE, 0, NULL);
                g_regex_match (regex, definition, 0, &match_info);
                g_regex_unref (regex);
                if (g_match_info_matches (match_info)) {
                        stream = (EncoderStream *)g_malloc (sizeof (EncoderStream));
                        stream->name = g_match_info_fetch_named (match_info, "stream");
                        GST_INFO ("stream found %s", stream->name);
                        g_array_append_val (encoder->streams, stream);
                } else if (g_str_has_prefix (definition, "appsrc")) {
                        GST_ERROR ("appsrc name property must be set");
                        return 1;
                }
        }

        return 0;
}

static guint
channel_source_initialize (Channel *channel, GstStructure *configure)
{
        gint i, j;
        SourceStream *stream;
        Source *source;

        source = channel->source;
        source->configure = configure;
        source->name = (gchar *)gst_structure_get_name (configure);
        source->channel = channel;
        channel_source_extract_streams (source);

        for (i = 0; i < channel->source->streams->len; i++) {
                stream = g_array_index (channel->source->streams, gpointer, i);
                stream->current_position = -1;
                stream->system_clock = channel->system_clock;
                stream->encoders = g_array_new (FALSE, FALSE, sizeof(gpointer)); //TODO: free!
                for (j = 0; j < SOURCE_RING_SIZE; j++) {
                        stream->ring[j] = NULL;
                }
                stream->type = &(channel->output->source.streams[i].type);
                stream->last_heartbeat = &(channel->output->source.streams[i].last_heartbeat);
                stream->current_timestamp = &(channel->output->source.streams[i].current_timestamp);
                g_strlcpy (channel->output->source.streams[i].name, stream->name, STREAM_NAME_LEN);
        }

        source->bins = get_bins (configure);
        if (source->bins == NULL) {
                return 1;
        }
        source->pipeline = create_source_pipeline (source);

        return 0;
}

static guint
channel_encoder_initialize (Channel *channel, GstStructure *configure)
{
        gint i, j, k, n;
        gchar *name;
        GValue *value;
        GstStructure *structure;
        Encoder *encoder;
        EncoderStream *stream;
        SourceStream *source;

        n = gst_structure_n_fields (configure);
        for (i = 0; i < n; i++) {
                name = (gchar *)gst_structure_nth_field_name (configure, i);
                GST_INFO ("encoder found: %s", name);
                value = (GValue *)gst_structure_get_value (configure, name);
                structure = (GstStructure *)gst_value_get_structure (value);
                encoder = encoder_new (0, NULL); //TODO free!
                encoder->channel = channel;
                encoder->id = i;
                encoder->name = name;
                g_strlcpy (channel->output->encoders[i].name, name, STREAM_NAME_LEN);
                encoder->cache_addr = channel->output->encoders[i].cache_addr;
                encoder->cache_end_addr = channel->output->encoders[i].cache_end_addr;
                encoder->cache_size = &(channel->output->encoders[i].cache_size);
                encoder->total_count = &(channel->output->encoders[i].total_count);
                encoder->head_addr = &(channel->output->encoders[i].head_addr);
                encoder->tail_addr = &(channel->output->encoders[i].tail_addr);
                encoder->last_rap_addr = &(channel->output->encoders[i].last_rap_addr);
                encoder->configure = structure;

                if (channel_encoder_extract_streams (encoder) != 0) {
                        return 1;
                }

                for (j = 0; j < encoder->streams->len; j++) {
                        stream = g_array_index (encoder->streams, gpointer, j);
                        stream->last_heartbeat = &(channel->output->encoders[i].streams[j].last_heartbeat);
                        stream->current_timestamp = &(channel->output->encoders[i].streams[j].current_timestamp);
                        g_strlcpy (channel->output->encoders[i].streams[j].name, stream->name, STREAM_NAME_LEN);
                        for (k = 0; k < channel->source->streams->len; k++) {
                                source = g_array_index (channel->source->streams, gpointer, k);
                                if (g_strcmp0 (source->name, stream->name) == 0) {
                                        GST_INFO ("source: %s encoder: %s", source->name, stream->name);
                                        stream->source = source;
                                        stream->current_position = -1;
                                        stream->system_clock = encoder->channel->system_clock;
                                        g_array_append_val (source->encoders, stream);
                                        break;
                                }
                        }
                        if (stream->source == NULL) {
                                GST_ERROR ("cant find channel %s source %s.", channel->name, stream->name);
                                return -1;
                        }
                }

                for (j = 0; j < ENCODER_RING_SIZE; j++) {
                        encoder->output_ring[j] = NULL;
                }

                encoder->bins = get_bins (structure);
                if (encoder->bins == NULL) {
                        return 1;
                }
                complete_request_element (encoder->bins);
                create_encoder_pipeline (encoder);

                g_array_append_val (channel->encoder_array, encoder);
        }

        return 0;
}

/*
 * parse configure.
 */
gint
channel_configure_parse (Channel *channel)
{
        GstStructure *configure = channel->configure;
        gint i, j, n, k, escount;
        GstStructure *structure, *encoder, *bins, *bin;
        GValue *value;
        gchar *name, *option, *definition;
        GRegex *regex;
        GMatchInfo *match_info;

        /* calculate corespond output size of the configure. */
        channel->shm_size = sizeof (ChannelOutput);
        value = (GValue *)gst_structure_get_value (configure, "source");
        structure = (GstStructure *)gst_value_get_structure (value);
        value = (GValue *)gst_structure_get_value (structure, "bins");
        structure = (GstStructure *)gst_value_get_structure (value);
        n = gst_structure_n_fields (structure);
        channel->sscount = 0;
        for (i = 0; i < n; i++) {
                name = (gchar *)gst_structure_nth_field_name (structure, i);
                value = (GValue *)gst_structure_get_value (structure, name);
                bin = (GstStructure *)gst_value_get_structure (value);
                option = (gchar *)gst_structure_get_string (bin, "option");
                if ((option != NULL) && (g_strcmp0 (option, "no") == 0)) {
                        GST_WARNING ("%s option set to no.", name);
                        continue;
                }
                definition = (gchar *)gst_structure_get_string (bin, "definition");
                regex = g_regex_new ("! *appsink[^!]*$", G_REGEX_OPTIMIZE, 0, NULL);
                g_regex_match (regex, definition, 0, &match_info);
                g_regex_unref (regex);
                if (g_match_info_matches (match_info)) {
                        channel->sscount += 1;
                        channel->shm_size += sizeof (struct _SourceStreamState);
                }
        }

        value = (GValue *)gst_structure_get_value (configure, "encoders");
        structure = (GstStructure *)gst_value_get_structure (value);
        n = gst_structure_n_fields (structure);
        channel->escountlist = g_array_new (FALSE, FALSE, sizeof (gint));
        for (i = 0; i < n; i++) {
                name = (gchar *)gst_structure_nth_field_name (structure, i);
                value = (GValue *)gst_structure_get_value (structure, name);
                encoder = (GstStructure *)gst_value_get_structure (value);
                channel->shm_size += sizeof (struct _EncoderOutput);
                value = (GValue *)gst_structure_get_value (encoder, "bins");
                bins = (GstStructure *)gst_value_get_structure (value);
                k = gst_structure_n_fields (bins);
                escount = 0;
                for (j = 0; j < k; j++) {
                        name = (gchar *)gst_structure_nth_field_name (bins, j);
                        value = (GValue *)gst_structure_get_value (bins, name);
                        bin = (GstStructure *)gst_value_get_structure (value);
                        option = (gchar *)gst_structure_get_string (bin, "option");
                        if ((option != NULL) && (g_strcmp0 (option, "no") == 0)) {
                                GST_WARNING ("%s option set to no.", name);
                                continue;
                        }
                        definition = (gchar *)gst_structure_get_string (bin, "definition");
                        regex = g_regex_new ("appsrc name=(?<stream>[^ ]*)", G_REGEX_OPTIMIZE, 0, NULL);
                        g_regex_match (regex, definition, 0, &match_info);
                        g_regex_unref (regex);
                        if (g_match_info_matches (match_info)) {
                                GST_ERROR ("encoder stream");
                                channel->shm_size += sizeof (struct _EncoderStreamState);
                                escount += 1;
                        }
                }
                g_array_append_val (channel->escountlist, escount);
        }
        GST_INFO ("Channel sscount %d, encoder count %d, out put size is : %d", channel->sscount, channel->escountlist->len, channel->shm_size);

        return 0;
}

/*
 * Interface to (share) memory for producing.
 */
gint
channel_output_init (Channel *channel, gboolean daemon)
{
        GstStructure *configure = channel->configure;
        gint i, fd;
        ChannelOutput *output;
        gchar *name, *p;

        if (daemon) {
                /* daemon, use share memory */
                name = (gchar *)gst_structure_get_name (configure);
                fd = shm_open (name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
                ftruncate (fd, channel->shm_size);
                p = mmap (NULL, channel->shm_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        } else {
                p = g_malloc (channel->shm_size);
        }
        output = (ChannelOutput *)p;
        p += sizeof (ChannelOutput);
        output->source.sync_error_times = 0;
        output->source.stream_count = channel->sscount;
        output->source.streams = (struct _SourceStreamState *)p;
        for (i = 0; i < channel->sscount; i++) {
                output->source.streams[i].type = ST_UNKNOWN;
        }
        p += channel->sscount * sizeof (struct _SourceStreamState);
        output->encoder_count = channel->escountlist->len;
        output->encoders = (struct _EncoderOutput *)p;
        p += channel->escountlist->len * sizeof (struct _EncoderOutput);
        for (i = 0; i < channel->escountlist->len; i++) {
                output->encoders[i].stream_count = g_array_index (channel->escountlist, gint, i);
                output->encoders[i].streams = (struct _EncoderStreamState *)p;
                p += output->encoders[i].stream_count * sizeof (struct _EncoderStreamState);
                if (daemon) {
                        /* daemon, use share memory. */
                        name = g_strdup_printf ("%s.%d", (gchar *)gst_structure_get_name (configure), i);
                        fd = shm_open (name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
                        ftruncate (fd, 64 * 1024 * 1024);
                        output->encoders[i].cache_addr = mmap (NULL, 64 * 1024 * 1024, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
                        g_free (name);
                } else {
                        output->encoders[i].cache_addr = g_malloc (64 * 1024 * 1024);
                }
                output->encoders[i].cache_size = 64 * 1024 * 1024;
                output->encoders[i].head_addr = output->encoders[i].cache_addr;
                output->encoders[i].tail_addr = output->encoders[i].cache_addr;
                output->encoders[i].last_rap_addr = output->encoders[i].cache_addr;
                output->encoders[i].cache_end_addr = output->encoders[i].cache_addr + output->encoders[i].cache_size;
                output->encoders[i].total_count = 0;
        }

        GST_LOG ("output : %llu, p: %llu", output, p);
        channel->output = output;

        return 0;
}

static void
child_watch_cb (GPid pid, gint status, Channel *channel)
{
        /* Close pid */
        g_spawn_close_pid (pid);
}

gint
channel_setup (Channel *channel, gboolean daemon)
{
        GValue *value;
        GstStructure *structure;

        if (channel_configure_parse (channel) != 0) {
                GST_ERROR ("Configure parse error.");
                return 1;
        }

        if (channel_output_init (channel, daemon) != 0) {
                GST_ERROR ("Output init error.");
                return 2;
        }

        /* initialize source */
        value = (GValue *)gst_structure_get_value (channel->configure, "source");
        structure = (GstStructure *)gst_value_get_structure (value);
        if (channel_source_initialize (channel, structure) != 0) {
                GST_ERROR ("Initialize channel source error.");
                return 3;
        }

        /* initialize encoders */
        value = (GValue *)gst_structure_get_value (channel->configure, "encoders");
        structure = (GstStructure *)gst_value_get_structure (value);
        if (channel_encoder_initialize (channel, structure) != 0) {
                GST_ERROR ("Initialize channel encoder error.");
                return 4;
        }

        return 0;
}


/*
 * channel_start
 *
 * @channel: channel to be initialize.
 * @structure: configure data.
 *
 * Returns: TRUE on success, FALSE on failure.
 *
 */
gboolean
channel_start (Channel *channel, gboolean daemon)
{
        GError *e = NULL;
        gchar *argv[4], path[512];
        GPid pid;
        Encoder *encoder;
        gint i;

        if (!channel->enable) {
                GST_WARNING ("Can't start a channel %s with enable set to no.", channel->name);
                return TRUE;
        }

        if (daemon) {
                memset (path, '\0', sizeof (path));
                readlink ("/proc/self/exe", path, sizeof (path));
                argv[0] = path;
                argv[1] = "-n";
                argv[2] = "0";
                argv[3] = NULL;
                if (!g_spawn_async (NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid, NULL)) {
                }
                channel->worker_pid = pid;
                g_child_watch_add (pid, (GChildWatchFunc)child_watch_cb, channel);
                return TRUE;
        } else {
                /* set pipelines as PLAYING state */
                gst_element_set_state (channel->source->pipeline, GST_STATE_PLAYING);
                channel->source->state = GST_STATE_PLAYING;
                for (i = 0; i < channel->encoder_array->len; i++) {
                        encoder = g_array_index (channel->encoder_array, gpointer, i);
                        gst_element_set_state (encoder->pipeline, GST_STATE_PLAYING);
                        encoder->state = GST_STATE_PLAYING;
                }
                channel->output->state = GST_STATE_PLAYING;
                return TRUE;
        }
}

void
channel_stop (Channel *channel, gint sig)
{
        GST_ERROR ("stop %d", channel->worker_pid);
        channel->output->state = GST_STATE_NULL;
        if (channel->worker_pid != 0) {
                kill (channel->worker_pid, sig);
                channel->worker_pid = 0;
        } else {
                exit (0);
        }
}

Encoder *
channel_get_encoder (gchar *uri, GArray *channels)
{
        GRegex *regex = NULL;
        GMatchInfo *match_info = NULL;
        gchar *c, *e;
        Channel *channel;
        Encoder *encoder = NULL;

        regex = g_regex_new ("^/channel/(?<channel>[0-9]+)/encoder/(?<encoder>[0-9]+).*", G_REGEX_OPTIMIZE, 0, NULL);
        g_regex_match (regex, uri, 0, &match_info);
        if (g_match_info_matches (match_info)) {
                c = g_match_info_fetch_named (match_info, "channel");
                if (atoi (c) < channels->len) {
                        channel = g_array_index (channels, gpointer, atoi (c));
                        e = g_match_info_fetch_named (match_info, "encoder");
                        if (atoi (e) < channel->encoder_array->len) {
                                GST_INFO ("http get request, channel is %s, encoder is %s", c, e);
                                encoder = g_array_index (channel->encoder_array, gpointer, atoi (e));
                        } else {
                                GST_ERROR ("Out range encoder %s, len is %d.", e, channel->encoder_array->len);
                        }
                        g_free (e);
                } else {
                        GST_ERROR ("Out range channel %s.", c);
                }
                g_free (c);
        }

        if (match_info != NULL)
                g_match_info_free (match_info);
        if (regex != NULL)
                g_regex_unref (regex);

        return encoder;
}

