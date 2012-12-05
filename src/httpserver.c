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
        HTTPSERVER_PROP_PORT,
};

static void httpserver_class_init (HTTPServerClass *httpserverclass);
static void httpserver_init (HTTPServer *httpserver);
static GObject *httpserver_constructor (GType type, guint n_construct_properties, GObjectConstructParam *construct_properties);
static void httpserver_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec);
static void httpserver_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec);
static gint accept_socket (HTTPServer *http_server);
static gpointer listen_thread (gpointer data);
static gint compare_func (gconstpointer a, gconstpointer b);
static gpointer idle_thread (gpointer data);
static gpointer block_thread (gpointer data);
static void thread_pool_func (gpointer data, gpointer user_data);
static gint read_request (RequestData *request_data);

static void
httpserver_class_init (HTTPServerClass *httpserverclass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS(httpserverclass);
        GParamSpec *param;

        g_object_class->constructor = httpserver_constructor;
        g_object_class->set_property = httpserver_set_property;
        g_object_class->get_property = httpserver_get_property;

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

static gint
compare_func (gconstpointer a, gconstpointer b)
{
        GstClockTime aa = *(GstClockTime *)a;
        GstClockTime bb = *(GstClockTime *)b;

        //GST_ERROR ("%llu-%llu %d", aa, bb, aa-bb);
        
        return (aa < bb ? -1 : (aa > bb ? 1 : 0));
}

static void
httpserver_init (HTTPServer *http_server)
{
        gint i;
        RequestData *request_data;

        http_server->listen_thread = NULL;
        http_server->thread_pool = NULL;
        http_server->request_data_queue = g_queue_new ();
        for (i=0; i<kMaxRequests; i++) {
                request_data = (RequestData *)g_malloc (sizeof (RequestData));
                request_data->events_mutex = g_mutex_new ();
                http_server->request_data_pointers[i] = request_data;
                g_queue_push_head (http_server->request_data_queue, &http_server->request_data_pointers[i]);
        }

        http_server->idle_queue_mutex = g_mutex_new (); //FIXME: release, deprecated api in new version of glib.
        http_server->idle_queue_cond = g_cond_new ();
        http_server->idle_queue = g_tree_new ((GCompareFunc)compare_func);

        http_server->block_queue_mutex = g_mutex_new ();
        http_server->block_queue_cond = g_cond_new ();
        http_server->block_queue = g_queue_new ();

        http_server->total_click = 0;
        http_server->encoder_click = 0;
        http_server->system_clock = gst_system_clock_obtain ();
        g_object_set (http_server->system_clock, "clock-type", GST_CLOCK_TYPE_REALTIME, NULL);
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
        gint count, read_pos = request_data->request_length;
        gchar *buf = &(request_data->raw_request[0]);

        for (;;) {
                count = read (request_data->sock, buf + read_pos, kRequestBufferSize - read_pos);
                if (count == -1) {
                        if (errno != EAGAIN) {
                                GST_ERROR ("read error %s", g_strerror (errno));
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
        request_data->request_length = read_pos;

        buf[read_pos] = '\0'; /* string */
        return read_pos;
}

static gint
parse_request (RequestData *request_data)
{
        gchar *buf = request_data->raw_request;
        gchar *uri = &(request_data->uri[0]);
        gchar *parameters = &(request_data->parameters[0]);
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
        while (*buf != ' ' && *buf != '?' && i++ < 255) { /* max length of uri is 255*/
                *uri = *buf;
                buf++;
                uri++;
        }
        if (i <= 255) {
                *uri = '\0';
        } else { /* Bad request, uri too long */
                return 3;
        }

        i = 0;
        if (*buf == '?') { /* have parameters */
                buf++;
                while (*buf != ' ' && i++ < 1024) {
                        *parameters = *buf;
                        buf++;
                        parameters++;
                }
        }
        if (i <= 1024) {
                *parameters = '\0';
        } else { /* Bad request, parameters too long */
                return 3;
        }

        while (*buf == ' ') { /* skip space */
                buf++;
        }

        if (strncmp (buf, "HTTP/1.1", 8) == 0) { /* http version must be 1.1 */
                request_data->version = HTTP_1_1; 
        } else if (strncmp (buf, "HTTP/1.0", 8) == 0) {
                request_data->version = HTTP_1_0;
        }else { /* Bad request, must be http 1.1 */
                return 4;
        }

        buf += 8;

        /* TODO : parse headers */
        return 0;
}

static gint
setNonblocking(int fd)
{
        int flags;
        if (-1 ==(flags = fcntl(fd, F_GETFL, 0)))
                flags = 0;
        return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static gint
accept_socket (HTTPServer *http_server)
{
        struct epoll_event ee;
        gint accepted_sock, ret;
        struct sockaddr in_addr;
        socklen_t in_len;
        RequestData **request_data_pointer;
        RequestData *request_data;

        for (;;) { /* repeat accept until -1 returned */
                accepted_sock = accept (http_server->listen_sock, &in_addr, &in_len);
                if (accepted_sock == -1) {
                        if (( errno == EAGAIN) || (errno == EWOULDBLOCK)) { /* We have processed all incoming connections. */
                                break;
                        } else {
                                GST_ERROR ("accept error  %s", g_strerror (errno));
                                break;
                        }
                }
                if (g_queue_get_length (http_server->request_data_queue) == 0) {
                        GST_ERROR ("event queue empty");
                        close (accepted_sock);
                } else {
                        GST_INFO ("new request arrived, accepted_sock %d", accepted_sock);
                        http_server->total_click += 1;
                        int on = 1;
                        setsockopt (accepted_sock, SOL_TCP, TCP_CORK, &on, sizeof(on));
                        setNonblocking (accepted_sock);
                        request_data_pointer = g_queue_pop_tail (http_server->request_data_queue);
                        if (request_data_pointer == NULL) {
                                GST_ERROR ("No NONE request, refuse this request.");
                                close (accepted_sock);
                        } else {
                                request_data = *request_data_pointer;
                                request_data->client_addr = in_addr;
                                request_data->sock = accepted_sock;
                                request_data->birth_time = gst_clock_get_time (http_server->system_clock);
                                request_data->status = HTTP_CONNECTED;
                                request_data->request_length = 0;
                                ee.events = EPOLLIN | EPOLLOUT | EPOLLET;
                                ee.data.ptr = request_data_pointer;
                                ret = epoll_ctl (http_server->epollfd, EPOLL_CTL_ADD, accepted_sock, &ee);
                                if (ret == -1) {
                                        GST_ERROR ("epoll_ctl add error  %s", g_strerror (errno));
                                        close (accepted_sock);
                                        request_data->status = HTTP_NONE;
                                        g_queue_push_head (http_server->request_data_queue, request_data_pointer);
                                        return;
                                }
                        }
                }
        }
}

static gpointer
listen_thread (gpointer data)
{
        HTTPServer *http_server = (HTTPServer *)data;
        struct addrinfo hints;
        struct addrinfo *result, *rp;
        gint ret, listen_sock;
        gchar *port = g_strdup_printf ("%d", http_server->listen_port);
        struct epoll_event event, event_list[kMaxRequests];
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
                GST_ERROR ("epoll_create error %s", g_strerror (errno));
                return NULL;
        }

        event.data.ptr = NULL;
        event.events = EPOLLIN | EPOLLOUT | EPOLLET;
        ret = epoll_ctl (http_server->epollfd, EPOLL_CTL_ADD, listen_sock, &event);
        if (ret == -1) {
                GST_ERROR ("epoll_ctl add epollfd error %s", g_strerror (errno));
                return NULL;
        }

        for (;;) {
                int n, i;

                n = epoll_wait (http_server->epollfd, event_list, kMaxRequests, -1);
                if (n == -1) {
                        GST_ERROR ("epoll_wait error %s", g_strerror (errno));
                }
                for (i = 0; i < n; i++) { /* push to thread pool queue */
                        if (event_list[i].data.ptr == NULL) {
                                accept_socket (http_server);
                        } else {
                                RequestData *request_data = *(RequestData **)(event_list[i].data.ptr);
                                g_mutex_lock (request_data->events_mutex);
                                request_data->events |= event_list[i].events;
                                g_mutex_unlock (request_data->events_mutex);
                                if (event_list[i].events & EPOLLIN) {
                                        GST_DEBUG ("event on sock %d events EPOLLIN", request_data->sock);
                                        if (request_data->status == HTTP_CONNECTED) {
                                                g_thread_pool_push (http_server->thread_pool, event_list[i].data.ptr, &e);
                                                if (e != NULL) { // FIXME
                                                        GST_ERROR ("Thread pool push error %s", e->message);
                                                        g_error_free (e);
                                                }
                                        } 
                                } 
                                if (event_list[i].events & EPOLLOUT) {
                                        GST_DEBUG ("event on sock %d events EPOLLOUT", request_data->sock);
                                        if (request_data->status == HTTP_BLOCK) {
                                                g_mutex_lock (http_server->block_queue_mutex);
                                                g_cond_signal (http_server->block_queue_cond);
                                                g_mutex_unlock (http_server->block_queue_mutex);        
                                        }
                                }
                                if (event_list[i].events & EPOLLERR) {
                                        GST_DEBUG ("event on sock %d events EPOLLERR", request_data->sock);
                                        if (request_data->status == HTTP_BLOCK) {
                                                g_mutex_lock (http_server->block_queue_mutex);
                                                g_cond_signal (http_server->block_queue_cond);
                                                g_mutex_unlock (http_server->block_queue_mutex);        
                                        }
                                } 
                                if (event_list[i].events & EPOLLHUP) {
                                        GST_DEBUG ("event on sock %d events EPOLLHUP", request_data->sock);
                                        if (request_data->status == HTTP_BLOCK) {
                                                g_mutex_lock (http_server->block_queue_mutex);
                                                g_cond_signal (http_server->block_queue_cond);
                                                g_mutex_unlock (http_server->block_queue_mutex);        
                                        }
                                }
                                if (event_list[i].events & EPOLLRDBAND) {
                                        GST_DEBUG ("event on sock %d events EPOLLRDBAND", request_data->sock);
                                }
                                if (event_list[i].events & EPOLLRDNORM) { 
                                        GST_DEBUG ("event on sock %d events EPOLLRDNORM", request_data->sock);
                                }
                                if (event_list[i].events & EPOLLWRNORM) {
                                        GST_DEBUG ("event on sock %d events EPOLLWRNORM", request_data->sock);
                                }
                                if (event_list[i].events & EPOLLWRBAND) {
                                        GST_DEBUG ("event on sock %d events EPOLLWRBAND", request_data->sock);
                                }
                                if (event_list[i].events & EPOLLRDHUP) {
                                        GST_DEBUG ("event on sock %d events EPOLLRDHUP", request_data->sock);
                                }
                        }
                }
        }
}

typedef struct _ForeachFuncData {
        GSList **wakeup_list; /* point to list of wakeuped */
        HTTPServer *http_server;
        GTimeVal wakeup_time;
} ForeachFuncData;

static gboolean
gtree_foreach_func (gpointer key, gpointer value, gpointer data)
{
        ForeachFuncData *func_data = data;
        HTTPServer *http_server = func_data->http_server;
        GSList **wakeup_list = func_data->wakeup_list;
        GstClockTime current_time;
        
        current_time = gst_clock_get_time (http_server->system_clock);
        if (current_time > *(GstClockTime *)key) {
                *wakeup_list = g_slist_append (*wakeup_list, value);
                return FALSE;
        } else {
                func_data->wakeup_time.tv_sec = (*(GstClockTime *)key) / 1000000000;
                func_data->wakeup_time.tv_usec = ((*(GstClockTime *)key) % 1000000000) / 1000;
                return TRUE;
        }
}

static void
gslist_foreach_func (gpointer data, gpointer user_data)
{
        RequestData **request_data_pointer = data;
        RequestData *request_data = *request_data_pointer;
        HTTPServer *http_server = user_data;
        GError *e = NULL;

        g_tree_remove (http_server->idle_queue, &(request_data->wakeup_time));
        request_data->status = HTTP_CONTINUE;
        g_thread_pool_push (http_server->thread_pool, request_data_pointer, &e);
        if (e != NULL) { // FIXME
                GST_ERROR ("Thread pool push error %s", e->message);
                g_error_free (e);
        }
}

static gpointer
idle_thread (gpointer data)
{
        HTTPServer *http_server = (HTTPServer *)data;
        ForeachFuncData func_data;
        GSList *wakeup_list = NULL;

        func_data.http_server = http_server;
        func_data.wakeup_list = &wakeup_list;
        for (;;) {
                g_mutex_lock (http_server->idle_queue_mutex);
                while (g_tree_nnodes (http_server->idle_queue) == 0) {
                        g_cond_wait (http_server->idle_queue_cond, http_server->idle_queue_mutex);
                }
                func_data.wakeup_time.tv_sec = 0;
                g_tree_foreach (http_server->idle_queue, gtree_foreach_func, &func_data);
                if (wakeup_list != NULL) {
                        g_slist_foreach (wakeup_list, gslist_foreach_func, http_server);
                        g_slist_free (wakeup_list);
                        wakeup_list = NULL;
                }
                if (func_data.wakeup_time.tv_sec != 0) {
                        /* more than one idle request in the idle queue, wait until. */
                        g_cond_timed_wait (http_server->idle_queue_cond, http_server->idle_queue_mutex, &(func_data.wakeup_time));
                }
                g_mutex_unlock (http_server->idle_queue_mutex);
        }
}

static void
block_queue_foreach_func (gpointer data, gpointer user_data)
{
        RequestData **request_data_pointer = data;
        RequestData *request_data = *request_data_pointer;
        HTTPServer *http_server = user_data;
        GError *e = NULL;

        if (request_data->events & (EPOLLOUT | EPOLLHUP | EPOLLERR)) {
                /* EPOLL event, popup request from block queue and push to thread pool */
                g_queue_remove (http_server->block_queue, request_data_pointer);
                g_thread_pool_push (http_server->thread_pool, request_data_pointer, &e);
                if (e != NULL) { // FIXME
                        GST_ERROR ("Thread pool push error %s", e->message);
                        g_error_free (e);
                }
        }
}

static gpointer
block_thread (gpointer data)
{
        HTTPServer *http_server = (HTTPServer *)data;
        GTimeVal timeout_time;

        for (;;) {
                g_get_current_time (&timeout_time);
                timeout_time.tv_sec += 5;
                g_mutex_lock (http_server->block_queue_mutex);
                g_cond_timed_wait (http_server->block_queue_cond, http_server->block_queue_mutex, &timeout_time);
                g_queue_foreach (http_server->block_queue, block_queue_foreach_func, http_server);
                g_mutex_unlock (http_server->block_queue_mutex);
        }
}

static void
thread_pool_func (gpointer data, gpointer user_data)
{
        HTTPServer *http_server = (HTTPServer *)user_data;
        RequestData **request_data_pointer = data;
        RequestData *request_data = *request_data_pointer;
        gint ret;
        GstClockTime cb_ret;
        
        g_mutex_lock (request_data->events_mutex);
        if (request_data->events & EPOLLHUP) {
                request_data->status = HTTP_FINISH;
                request_data->events = 0;
        } else if ((request_data->events & EPOLLOUT) && (request_data->status == HTTP_BLOCK)) {
                request_data->status = HTTP_CONTINUE;
                request_data->events = 0;
        }
        g_mutex_unlock (request_data->events_mutex);

        if (request_data->status == HTTP_CONNECTED) {
                ret = read_request (request_data);
                if (ret <= 0) {
                        GST_ERROR ("no data");
                        close (request_data->sock);
                        g_queue_push_head (http_server->request_data_queue, request_data_pointer);
                        return;
                } 

                ret = parse_request (request_data);
                if (ret == 0) {
                        /* parse complete, call back user function */
                        request_data->status = HTTP_REQUEST;
                        cb_ret = http_server->user_callback (request_data, http_server->user_data);
                        if (cb_ret > 0) {
                                GST_DEBUG ("insert idle queue end");
                                http_server->encoder_click += 1;
                                request_data->wakeup_time = cb_ret;
                                g_mutex_lock (http_server->idle_queue_mutex);
                                while (g_tree_lookup (http_server->idle_queue, &(request_data->wakeup_time)) != NULL) {
                                        /* avoid time conflict */
                                        request_data->wakeup_time++;
                                }
                                request_data->status = HTTP_IDLE;
                                g_tree_insert (http_server->idle_queue, &(request_data->wakeup_time), request_data_pointer);
                                g_cond_signal (http_server->idle_queue_cond);
                                g_mutex_unlock (http_server->idle_queue_mutex);
                        } else { //FIXME
                                request_data->status = HTTP_NONE;
                                close (request_data->sock);
                                g_queue_push_head (http_server->request_data_queue, request_data_pointer);
                        }
                } else if (ret == 1) {
                        /* need read more data */
                        return;
                } else {
                        /* Bad Request */
                        GST_ERROR ("Bad request, return is %d", ret);
                        gchar *buf = g_strdup_printf (http_400, ENCODER_NAME, ENCODER_VERSION);
                        write (request_data->sock, buf, strlen (buf));
                        g_free (buf);
                        request_data->status = HTTP_NONE;
                        close (request_data->sock);
                        g_queue_push_head (http_server->request_data_queue, request_data_pointer);
                }
        } else if (request_data->status == HTTP_CONTINUE) {
                cb_ret = http_server->user_callback (request_data, http_server->user_data);
                if (cb_ret == GST_CLOCK_TIME_NONE) {
                        g_mutex_lock (http_server->block_queue_mutex);
                        request_data->status = HTTP_BLOCK;
                        g_queue_push_head (http_server->block_queue, request_data_pointer);
                        g_mutex_unlock (http_server->block_queue_mutex);
                } else if (cb_ret > 0) {
                        int iiii=0;
                        request_data->wakeup_time = cb_ret;
                        g_mutex_lock (http_server->idle_queue_mutex);
                        if (request_data->status != HTTP_CONTINUE) {
                                GST_ERROR ("insert a un continue request to idle queue !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
                        }
                        if (g_tree_nnodes (http_server->idle_queue) > 0) {
                                while (g_tree_lookup (http_server->idle_queue, &(request_data->wakeup_time)) != NULL) {
                                        /* avoid time conflict */
                                        GST_DEBUG ("look up, tree node number %d find %lld", g_tree_nnodes (http_server->idle_queue), request_data->wakeup_time);
                                        request_data->wakeup_time++;
                                        if (iiii++==10) exit(0);
                                }
                        }
                        request_data->status = HTTP_IDLE;
                        g_tree_insert (http_server->idle_queue, &(request_data->wakeup_time), request_data_pointer);
                        g_cond_signal (http_server->idle_queue_cond);
                        g_mutex_unlock (http_server->idle_queue_mutex);
                } else {//FIXME
                        GST_ERROR ("return value is 0");
                        g_mutex_lock (http_server->idle_queue_mutex);
                        g_tree_remove (http_server->idle_queue, &(request_data->wakeup_time));
                        g_mutex_unlock (http_server->idle_queue_mutex);
                        request_data->status = HTTP_NONE;
                        close (request_data->sock);
                        g_queue_push_head (http_server->request_data_queue, request_data_pointer);
                }
        } else if (request_data->status == HTTP_FINISH) { // FIXME: how about if have continue request in idle queue??
                GST_INFO ("request finish %d", request_data->sock);
                cb_ret = http_server->user_callback (request_data, http_server->user_data);
                if (cb_ret == 0) {
                        g_mutex_lock (http_server->idle_queue_mutex);
                        g_tree_remove (http_server->idle_queue, &(request_data->wakeup_time));
                        g_mutex_unlock (http_server->idle_queue_mutex);
                        request_data->status = HTTP_NONE;
                        close (request_data->sock);
                        g_queue_push_head (http_server->request_data_queue, request_data_pointer);
                }
        }
}

gint
httpserver_start (HTTPServer *http_server, http_callback_t user_callback, gpointer user_data)
{
        GError *e = NULL;
        RequestData *request_data;

        http_server->thread_pool = g_thread_pool_new (thread_pool_func, http_server, kMaxThreads, TRUE, &e);
        if (e != NULL) {
                GST_ERROR ("Create thread pool error %s", e->message);
                g_error_free (e);
                return -1;
        }

        http_server->user_callback = user_callback;
        http_server->user_data = user_data;

        http_server->listen_thread = g_thread_create (listen_thread, http_server, TRUE, &e);
        if (e != NULL) {
                GST_ERROR ("Create httpserver thread error %s", e->message);
                g_error_free (e);
                return -1;
        }

        http_server->idle_thread = g_thread_create (idle_thread, http_server, TRUE, &e);
        if (e != NULL) {
                GST_ERROR ("Create idle thread error %s", e->message);
                g_error_free (e);
                return -1;
        }

        http_server->block_thread = g_thread_create (block_thread, http_server, TRUE, &e);
        if (e != NULL) {
                GST_ERROR ("Create block thread error %s", e->message);
                g_error_free (e);
                return -1;
        }

        return 0;
}

gint
httpserver_report_request_data (HTTPServer *http_server)
{
        gint i, count;
        RequestData *request_data;

        count = 0;
        for (i = 0; i < kMaxRequests; i++) {
                request_data = http_server->request_data_pointers[i];
                if (request_data->status != HTTP_NONE) {
                        GST_INFO ("%d : status %d uri %s", i, request_data->status, request_data->uri);
                } else {
                        count += 1;
                }
        }
        GST_INFO ("status None %d, total click %llu, encoder click %llu",
                  count,
                  http_server->total_click,
                  http_server->encoder_click);
}
