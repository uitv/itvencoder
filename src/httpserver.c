/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <gst/gst.h>
#include <string.h>
#include <errno.h>
#include "httpserver.h"

GST_DEBUG_CATEGORY_EXTERN (ITVENCODER);
#define GST_CAT_DEFAULT ITVENCODER

enum {
        HTTPSERVER_PROP_0,
        HTTPSERVER_PROP_ITVENCODER,
        HTTPSERVER_PROP_PORT,
};

static void httpserver_class_init (HTTPServerClass *httpserverclass);
static void httpserver_init (HTTPServer *httpserver);
static GObject *httpserver_constructor (GType type, guint n_construct_properties, GObjectConstructParam *construct_properties);
static void httpserver_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec);
static void httpserver_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec);
static void *request_dispatcher (enum mg_event event, struct mg_connection *conn);
static gint http_get_encoder (struct mg_connection *conn, ITVEncoder *itvencoder);
static gpointer httpserver_listen_thread (gpointer data);
static void thread_pool_func (gpointer data, gpointer user_data);
static gint read_request (RequestData *request_data);

static gint
http_get_encoder (struct mg_connection *conn, ITVEncoder *itvencoder)
{
        gchar *uri;
        GRegex *regex = NULL;
        GMatchInfo *match_info = NULL;
        gchar *c, *e;
        Channel *channel;
        EncoderPipeline *encoder;
        gint ret=-1, socket;

        uri = mg_get_request_info (conn)->uri;
        regex = g_regex_new ("^/channel/(?<channel>[0-9]+)/encoder/(?<encoder>[0-9]+)$", G_REGEX_OPTIMIZE, 0, NULL);
        g_regex_match (regex, uri, 0, &match_info);
        if (g_match_info_matches (match_info)) {
                c = g_match_info_fetch_named (match_info, "channel");
                if (atoi (c) < itvencoder->channel_array->len) {
                        channel = g_array_index (itvencoder->channel_array, gpointer, atoi (c));
                        e = g_match_info_fetch_named (match_info, "encoder");
                        if (atoi (e) < channel->encoder_pipeline_array->len) {
                                GST_DEBUG ("http get request, channel is %s, encoder is %s", c, e);
                                encoder = g_array_index (channel->encoder_pipeline_array, gpointer, atoi (e));
                                socket = mg_get_socket (conn);
                                encoder->httprequest_socket_list = g_slist_append (encoder->httprequest_socket_list, GINT_TO_POINTER (socket));
                                ret = 0;
                        } 
                        g_free (e);
                } 
                g_free (c);
        }

        if (match_info != NULL)
                g_match_info_free (match_info);
        if (regex != NULL)
                g_regex_unref (regex);
        return ret;
}

static void *request_dispatcher (enum mg_event event, struct mg_connection *conn)
{
        const struct mg_request_info *request_info = mg_get_request_info(conn);
        ITVEncoder *itvencoder = (ITVEncoder *)mg_get_user_data (conn);
        Channel *channel;
        gchar *message_404 = "No such encoder channel.";

        channel = g_array_index (itvencoder->channel_array, gpointer, 1);
        gchar *s = channel->decoder_pipeline->pipeline_string;
        switch (event) {
        case MG_NEW_REQUEST:
                switch (request_info->uri[1]) {
                case 'c': /* uri is /channel..., maybe request for encoder streaming */
                        if (http_get_encoder (conn, itvencoder) != 0) {
                                mg_printf (conn,
                                        "HTTP/1.1 404 Not Found\r\n"
                                        "Server: %s-%s\r\n"
                                        "Content-Type: text/html\r\n"
                                        "Connection: close\r\n"
                                        "Content-Length: %d\r\n\r\n"
                                        "%s",
                                        ENCODER_NAME,
                                        ENCODER_VERSION,
                                        strlen (message_404),
                                        message_404
                                );
                        } else {
                                mg_printf(conn,
                                        "HTTP/1.1 200 OK\r\n"
                                        "Content-Type: video/mpeg\r\n"
                                        "Server: %s-%s\r\n"
                                        "Transfer-Encoding: chunked\r\n"
                                        "\r\n",
                                        ENCODER_NAME,
                                        ENCODER_VERSION
                                );
                        }
                        return "";
                }
                mg_printf(conn,
                          "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/plain\r\n"
                          "Server: %s-%s\r\n"
                          "Content-Length: %d\r\n"
                          "\r\n"
                          "%s\n%s",
                          ENCODER_NAME,
                          ENCODER_VERSION,
                          strlen (request_info->uri) + strlen (s) + 1,
                          request_info->uri,
                          s);
                return "";
        default:
                GST_INFO ("event is %d", event);
                return NULL;
        }
}

static void
httpserver_class_init (HTTPServerClass *httpserverclass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS(httpserverclass);
        GParamSpec *param;

        g_object_class->constructor = httpserver_constructor;
        g_object_class->set_property = httpserver_set_property;
        g_object_class->get_property = httpserver_get_property;

        param = g_param_spec_object (
                "itvencoder",
                "itvencoderf",
                "itvencoder context",
                TYPE_ITVENCODER,
                G_PARAM_WRITABLE | G_PARAM_READABLE
        );
        g_object_class_install_property (g_object_class, HTTPSERVER_PROP_ITVENCODER, param);

        param = g_param_spec_int (
                "port",
                "portf",
                "server port",
                1000,
                65535,
                20129,
                G_PARAM_WRITABLE | G_PARAM_READABLE
        );
        g_object_class_install_property (g_object_class, HTTPSERVER_PROP_PORT, param);
}

static void
httpserver_init (HTTPServer *httpserver)
{
        gint i;

        httpserver->server_thread = NULL;
        httpserver->thread_pool = NULL;
        httpserver->request_data_queue = g_queue_new ();
        for (i=0; i<kMaxRequests; i++) {
                g_queue_push_head (httpserver->request_data_queue, (RequestData *)g_malloc (sizeof (RequestData)));
        }
}

static GObject *
httpserver_constructor (GType type, guint n_construct_properties, GObjectConstructParam *construct_properties)
{
        GObject *obj;
        GObjectClass *parent_class = g_type_class_peek(G_TYPE_OBJECT);

        GST_LOG ("httpserver constructor");

        obj = parent_class->constructor(type, n_construct_properties, construct_properties);

        return obj;
}

static void
httpserver_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
        g_return_if_fail (IS_HTTPSERVER (obj));

        GST_LOG ("httpserver set property");

        switch(prop_id) {
        case HTTPSERVER_PROP_ITVENCODER:
                HTTPSERVER(obj)->itvencoder = g_value_get_object (value); 
                break;
        case HTTPSERVER_PROP_PORT:
                HTTPSERVER(obj)->listen_port = g_value_get_int (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
                break;
        }
}

static void
httpserver_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
        HTTPServer *httpserver = HTTPSERVER (obj);

        GST_LOG ("httpserver get property");

        switch(prop_id) {
        case HTTPSERVER_PROP_ITVENCODER:
                g_value_set_object (value, httpserver->itvencoder);
                break;
        case HTTPSERVER_PROP_PORT:
                g_value_set_int (value, httpserver->listen_port);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
                break;
        }
}

GType
httpserver_get_type (void)
{
        static GType type = 0;

        GST_LOG ("httpserver get type");

        if (type) return type;
        static const GTypeInfo info = {
                sizeof (HTTPServerClass),
                NULL, // base class initializer
                NULL, // base class finalizer
                (GClassInitFunc) httpserver_class_init,
                NULL,
                NULL,
                sizeof (HTTPServer),
                0,
                (GInstanceInitFunc) httpserver_init,
                NULL
        };
        type = g_type_register_static (G_TYPE_OBJECT, "HTTPServer", &info, 0);

        return type;
}

static gint
read_request (RequestData *request_data)
{
        gint count, read_pos = 0;
        gchar *buf = &(request_data->raw_request[0]);

        while (1) {
                count = read (request_data->sock, buf + read_pos, kRequestBufferSize - read_pos);
                if (count == -1) {
                        if (errno != EAGAIN) {
                                GST_ERROR ("read error number %d", errno);
                                return -1;
                        } else { /* errno == EAGAIN means read complete */
                                GST_INFO ("read complete\n");
                                break;
                        }
                } else if (count == 0) { /* closed by client */
                        GST_WARNING ("client closed\n");
                        return -2;
                } else if (count > 0) {
                        read_pos += count;
                        if (read_pos == kRequestBufferSize) {
                                GST_ERROR ("rquest size too large");
                                return -3;
                        }
                }
        }

        buf[read_pos] = '\0'; /* string */
        return read_pos;
}

static gint
parse_request (RequestData *request_data)
{
        gchar *buf = request_data->raw_request;
        gchar *uri = &(request_data->uri[0]);
        gint i;

        if ((strstr (buf, "\n\n") == NULL) && (strstr(buf, "\r\n\r\n") == NULL)) {
                return 1;
        }

        if (strncmp (buf, "GET", 3) == 0) {
                request_data->method = HTTP_GET;
        } else if (strncmp (buf, "PUT", 3) == 0) {
                request_data->method = HTTP_PUT;
        } else {
                return 2; /* Bad request */
        }

        buf += 3;
        while (*buf == ' ') { /* skip space */
                buf++;
        }

        i = 0;
        while (*buf != ' ' && i++ < 255) { /* max length of uri is 255*/
                *uri = *buf;
                buf++;
                uri++;
        }
        if (i <= 255) {
                *uri = '\0';
        } else { /* Bad request, uri too long */
                return 3;
        }

        while (*buf == ' ') { /* skip space */
                buf++;
        }

        if (strncmp (buf, "HTTP/1.1", 8) == 0) { /* http version must be 1.1 */
                request_data->version = HTTP_1_1; 
        } else { /* Bad request, must be http 1.1 */
                return 4;
        }

        buf += 8;

        /* TODO : parse headers */
        return 0;
}

static int
setNonblocking(int fd)
{
        int flags;
        if (-1 ==(flags = fcntl(fd, F_GETFL, 0)))
                flags = 0;
        return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static gpointer
httpserver_listen_thread (gpointer data)
{
        HTTPServer *http_server = (HTTPServer *)data;
        struct addrinfo hints;
        struct addrinfo *result, *rp;
        gint ret, listen_sock;
        gchar *port = g_strdup_printf ("%d", http_server->listen_port);
        struct epoll_event event, events[kMaxRequests];
        GError *e = NULL;

        memset (&hints, 0, sizeof (struct addrinfo));
        hints.ai_family = AF_UNSPEC; /* Return IPv4 and IPv6 choices */
        hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */
        hints.ai_flags = AI_PASSIVE; /* All interfaces */
        ret = getaddrinfo (NULL, port, &hints, &result);
        if (ret != 0) {
                GST_ERROR ("getaddrinfo: %s\n", gai_strerror(ret));
                return NULL; // FIXME
        }

        GST_INFO ("start http server on port is %s", port);
        for (rp = result; rp != NULL; rp = rp->ai_next) {
                listen_sock = socket (rp->ai_family, rp->ai_socktype, rp->ai_protocol);
                int opt = 1;
                setsockopt (listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
                if (listen_sock == -1)
                        continue;
                ret = bind (listen_sock, rp->ai_addr, rp->ai_addrlen);
                if (ret == 0) {
                        /* We managed to bind successfully! */
                        GST_INFO ("listen socket %d", listen_sock);
                        break;
                }
                close (listen_sock);
        }

        if (rp == NULL) {
                GST_ERROR ("Could not bind %s\n", port);
                return NULL;
        }

        freeaddrinfo (result);
        g_free (port);

        ret = listen (listen_sock, SOMAXCONN);
        if (ret == -1) {
                GST_ERROR ("listen error");
                return NULL;
        }

        setNonblocking (listen_sock);
        http_server->listen_sock = listen_sock;
        http_server->epollfd = epoll_create1(0);
        if (http_server->epollfd == -1) {
                GST_ERROR ("epoll_create error %d", errno);
                return NULL;
        }

        event.data.ptr = NULL;
        event.events = EPOLLIN | EPOLLET;
        ret = epoll_ctl (http_server->epollfd, EPOLL_CTL_ADD, listen_sock, &event);
        if (ret == -1) {
                GST_ERROR ("epoll_ctl %d", errno);
                return NULL;
        }

        while (1) {
                int n, i;

                n = epoll_wait (http_server->epollfd, events, kMaxRequests, -1);
                if (n == -1) {
                        GST_ERROR ("epoll_wait %d", errno);
                }
                for (i = 0; i < n; i++) { /* push to thread pool queue */
                        g_thread_pool_push (http_server->thread_pool, &events[i], &e);
                        if (e != NULL) { // FIXME
                                GST_ERROR ("Thread pool push error %s", e->message);
                                g_error_free (e);
                                //return NULL;
                        }
                }
        }
}

static void
thread_pool_func (gpointer data, gpointer user_data)
{
        struct epoll_event *e = (struct epoll_event *)data;
        HTTPServer *http_server = (HTTPServer *)user_data;
        struct sockaddr in_addr;
        socklen_t in_len;
        gint accepted_sock;
        RequestData *request_data;
        gint ret;
        
        if ((e->events & EPOLLERR) || (e->events & EPOLLHUP)) { //FIXME
                /* An error has occured on this fd, or the socket is not ready for reading(why were we notified then?) */
                GST_ERROR ("epoll error %d" /*fd %d"*/, errno/*, e->data.fd*/);
                //close (e.data.fd); 
                return;
        }

        if (e->data.ptr == NULL) { /* new connection */
                while (1) { /* repeat accept until -1 returned */
                        accepted_sock = accept (http_server->listen_sock, &in_addr, &in_len);
                        if (accepted_sock == -1) {
                                if (( errno == EAGAIN) || (errno == EWOULDBLOCK)) { /* We have processed all incoming connections. */
                                        break;
                                } else {
                                        GST_ERROR ("accept errorno %d", errno);
                                        break;
                                }
                        }
                        if (g_queue_get_length (http_server->request_data_queue) == 0) {
                                GST_WARNING ("event queue empty");
                                close (accepted_sock);
                        } else {
                                GST_INFO ("new request arrived, accepted_sock %d", accepted_sock);
                                int on = 1;
                                setsockopt (accepted_sock, SOL_TCP, TCP_CORK, &on, sizeof(on));
                                setNonblocking (accepted_sock);
                                request_data = g_queue_pop_tail (http_server->request_data_queue);
                                request_data->client_addr = in_addr;
                                request_data->sock = accepted_sock;
                                g_get_current_time (&(request_data->birth_time));
                                request_data->status = HTTP_CONNECTED;
                                e->data.ptr = request_data;
                                e->events = EPOLLIN | EPOLLET;
                                ret = epoll_ctl (http_server->epollfd, EPOLL_CTL_ADD, accepted_sock, e);
                                if (ret == -1) {
                                        GST_ERROR ("epoll_ctl %d", errno);
                                        g_queue_push_head (http_server->request_data_queue, e->data.ptr);
                                        return;
                                }
                        }
                }
        } else { /* request */
                request_data = e->data.ptr;
                ret = read_request (request_data);
                if (ret <= 0) {
                        GST_ERROR ("no data");
                        close (request_data->sock);
                        g_queue_push_head (http_server->request_data_queue, request_data);
                        return;
                }

                ret = parse_request (request_data);
                if (ret == 0) { /* parse complete, call back user function */
                        GST_INFO ("Request uri %s", request_data->uri);
                        if (http_server->user_callback != NULL) {
                                http_server->user_callback (request_data, http_server->user_data);
                        } else {
                                GST_ERROR ("Missing user call back");
                                write (request_data->sock, header_404, sizeof(header_404));
                                close (request_data->sock);
                                g_queue_push_head (http_server->request_data_queue, request_data);
                        }
                } else { /* Bad Request */
                        GST_WARNING ("Bad request, return is %d", ret);
                }
        }
}

gint
httpserver_start (HTTPServer *http_server, GFunc user_callback, gpointer user_data)
{
        const char *options[] = {"listening_ports", "8000", "enable_keep_alive", "yes", NULL};
        GError *e = NULL;
        RequestData *request_data;

        http_server->ctx = mg_start (&request_dispatcher, http_server->itvencoder, options);

        http_server->thread_pool = g_thread_pool_new (thread_pool_func, http_server, kMaxThreads, TRUE, &e);
        if (e != NULL) {
                GST_ERROR ("Create thread pool error %s", e->message);
                g_error_free (e);
                return -1;
        }

        http_server->user_callback = user_callback;
        http_server->user_data = user_data;

        http_server->server_thread = g_thread_create (httpserver_listen_thread, http_server, TRUE, &e);
        if (e != NULL) {
                GST_ERROR ("Create httpserver thread error %s", e->message);
                g_error_free (e);
                return -1;
        }

        return 0;
}
