/*
  Copyright
*/

#include <unistd.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include "mongoose.h"

char *pipeline = "videotestsrc is-live=true ! queue ! "
                 "x264enc byte-stream=true pass=17 threads=2 bitrate=300 bframes=3 vbv-buf-capacity=3000 ! "
                 "mpegtsmux name=muxer ! queue ! "
                 "appsink name=testsink "
                 "audiotestsrc is-live=true ! queue ! "
                 "faac outputformat=1 profile=1 ! queue ! "
                 "muxer.";

static GstFlowReturn
on_buffer (GstAppSink * elt, gpointer user_data)
{
        guint size;
        GstBuffer *buffer;
        int *sockets;

        /* get the buffer from appsink */
        buffer = gst_app_sink_pull_buffer (GST_APP_SINK (elt));
        sockets = (int *)user_data;
        size = GST_BUFFER_SIZE (buffer);
        write(*sockets, "bc\r\n", 4);
        write(*sockets, GST_BUFFER_DATA(buffer), size);
        write(*sockets, "\r\n", 2);
        printf("port is: %d, buffer size is: %d\n", *sockets, size);
        /* we don't need the appsink buffer anymore */
        gst_buffer_unref (buffer);
}

static void
on_eos (GstAppSink * sink, gpointer user_data)
{
}

static void *callback(enum mg_event event, struct mg_connection *conn)
{
        const struct mg_request_info *request_info = mg_get_request_info(conn);
        int *sockets_addr = mg_get_user_data(conn);
        int socket = (int)mg_get_socket(conn);

        if (event == MG_NEW_REQUEST) {
                mg_printf(conn,
                          "HTTP/1.1 200 OK\r\n"
                          "Content-Type: video/mpegts\r\n"
                          "Server: iencoder/0.0.1\r\n"
                          "Transfer-Encoding: chunked\r\n"
                          "\r\n");
                *sockets_addr = socket;
                return "";
        } else {
                return NULL;
        }
}

int main(int argc, char *argv[])
{
        GstElement *pipe, *testsink=NULL;
        guint major, minor, micro, nano;
        GMainLoop *loop;

        int sockets=0;
        struct mg_context *ctx;
        const char *options[] = {"listening_ports", "8000", "enable_keep_alive", "yes", NULL};

        ctx = mg_start(&callback, &sockets, options);
 
        gst_init(&argc, &argv);
        gst_version(&major, &minor, &micro, &nano);
        printf("Gstreamer %d.%d.%d\n", major, minor, micro);

        pipe = gst_parse_launch(pipeline, NULL);
        testsink = gst_bin_get_by_name (GST_BIN (pipe), "testsink");
        GstAppSinkCallbacks callbacks = {on_eos, NULL, on_buffer, NULL};
        gst_app_sink_set_callbacks(GST_APP_SINK(testsink), &callbacks, &sockets, NULL);
        gst_object_unref (testsink);

        gst_element_set_state(pipe, GST_STATE_PLAYING);
        loop = g_main_loop_new(NULL, FALSE);
        g_main_loop_run(loop);

        return 0;
}
