/*
    http.h -- Header for the Embedthis Http Library.

    The http program is a client to issue HTTP requests. It is also a test platform for loading and testing web servers.
    Do NOT use this program as a sample for creating a simple http client.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

#ifndef _h_HTTP
#define _h_HTTP 1

/********************************* Includes ***********************************/

#include    "mpr.h"

/****************************** Forward Declarations **************************/

#ifdef __cplusplus
extern "C" {
#endif

#if !DOXYGEN
struct Http;
struct HttpAuth;
struct HttpEndpoint;
struct HttpHost;
struct HttpLimits;
struct HttpNet;
struct HttpPacket;
struct HttpQueue;
struct HttpRoute;
struct HttpRx;
struct HttpSession;
struct HttpStage;
struct HttpStream;
struct HttpTrace;
struct HttpTx;
struct HttpUri;
struct HttpUser;
struct HttpWebSocket;
#endif

/********************************** Tunables **********************************/

#ifndef ME_HTTP_HTTP2
    #define ME_HTTP_HTTP2           1
#endif
#ifndef ME_HTTP_WEB_SOCKETS
    #define ME_HTTP_WEB_SOCKETS     1
#endif

/*
    Unlimited limit value
 */
#define HTTP_UNLIMITED MAXINT64

#if ME_TUNE_SIZE
    #ifndef ME_PACKET_SIZE
        #define ME_PACKET_SIZE      (8 * 1024)      /**< Default packet size for pipeline packets */
    #endif
    #ifndef ME_CHUNK_SIZE
        #define ME_CHUNK_SIZE       (8 * 1024)      /**< Maximum chunk size for transfer chunk encoding */
    #endif
#elif ME_TUNE_SPEED
    #ifndef ME_PACKET_SIZE
        #define ME_PACKET_SIZE      (32 * 1024)
    #endif
    #ifndef ME_CHUNK_SIZE
        #define ME_CHUNK_SIZE       (32 * 1024)
    #endif
#else
    #ifndef ME_PACKET_SIZE
        #define ME_PACKET_SIZE      (16 * 1024)
    #endif
    #ifndef ME_CHUNK_SIZE
        #define ME_CHUNK_SIZE       (16 * 1024)
    #endif
#endif
#ifndef ME_SANITY_PACKET
    #define ME_SANITY_PACKET        (128 * 1024)
#endif
#ifndef ME_HTTP_DEFAULT_METHODS
    #define ME_HTTP_DEFAULT_METHODS "GET,POST"           /**< Default methods for routes */
#endif
#ifndef ME_HTTP_PORT
    #define ME_HTTP_PORT            80
#endif
#ifndef ME_HTTP_SOFTWARE
    #define ME_HTTP_SOFTWARE        "Embedthis-http"     /**< Default Http protocol name used in Http Server header */
#endif
#ifndef ME_HTTP_BAN_PERIOD
    #define ME_HTTP_BAN_PERIOD      (5 * 60 * 1000)      /**< Default ban IP period */
#endif
#ifndef ME_HTTP_DELAY_PERIOD
    #define ME_HTTP_DELAY_PERIOD    (5 * 60 * 1000)      /**< Default delay IP period */
#endif
#ifndef ME_HTTP_MONITOR_PERIOD
    #define ME_HTTP_MONITOR_PERIOD  (15 * 1000)          /**< Monitor prune period */
#endif
#ifndef ME_HTTP_REMEDY_TIMEOUT
    #define ME_HTTP_REMEDY_TIMEOUT  (60 * 1000)          /**< Default remedy command timeout */
#endif
#ifndef ME_HTTP_DELAY
    #define ME_HTTP_DELAY           (2000)               /**< 2 second delay per request - while delay enforced */
#endif
#ifndef ME_MAX_URI
    #define ME_MAX_URI              512                  /**< Reasonable URI size */
#endif
#ifndef ME_MAX_IOVEC
    #define ME_MAX_IOVEC            16                   /**< Number of fragments in a single socket write */
#endif
#ifndef ME_MAX_CLIENTS_HASH
    #define ME_MAX_CLIENTS_HASH     131                  /**< Hash table for client IP addresses */
#endif
#ifndef  ME_MAX_CACHE_ITEM
    #define ME_MAX_CACHE_ITEM       (256 * 1024)         /**< Maximum cachable item size */
#endif
#ifndef ME_MAX_CHUNK
    #define ME_MAX_CHUNK            (8 * 1024)           /**< Maximum chunk size for transfer chunk encoding */
#endif
#ifndef ME_MAX_CLIENTS
    #define ME_MAX_CLIENTS          32                   /**< Maximum unique client IP addresses */
#endif
#ifndef ME_MAX_CONNECTIONS
    #define ME_MAX_CONNECTIONS      50                   /**< Maximum concurrent client endpoints */
#endif
#ifndef ME_MAX_HPACK_SIZE
    #define ME_MAX_HPACK_SIZE       4096                 /**< Maximum size of the hpack table */
#endif
#ifndef ME_MAX_STREAMS
    #define ME_MAX_STREAMS          20                    /**< Default maximum concurrent streams per network */
#endif
#ifndef ME_MAX_HEADERS
    #define ME_MAX_HEADERS          8192                 /**< Maximum size of the headers (8K) */
#endif
#ifndef ME_MAX_KEEP_ALIVE
    #define ME_MAX_KEEP_ALIVE       400                  /**< Maximum requests per network */
#endif
#ifndef ME_MAX_NUM_HEADERS
    #define ME_MAX_NUM_HEADERS      64                   /**< Maximum number of header lines */
#endif
#ifndef ME_QUEUE_MAX_FACTOR
    #define ME_QUEUE_MAX_FACTOR     4                    /**< Queue max set to packetSize * factor */
#endif
#ifndef ME_MAX_PROCESSES
    #define ME_MAX_PROCESSES        10                   /**< Maximum concurrent processes */
#endif
#ifndef ME_MAX_RX_BODY
    #define ME_MAX_RX_BODY          (512 * 1024)         /**< Maximum incoming body size (512K) */
#endif
#ifndef ME_MAX_RX_FORM
    #define ME_MAX_RX_FORM          (512 * 1024)         /**< Maximum incoming form size (512K) */
#endif
#ifndef ME_MAX_REQUESTS_PER_CLIENT
    #define ME_MAX_REQUESTS_PER_CLIENT 20               /**< Maximum concurrent requests per client */
#endif
#ifndef ME_MAX_REWRITE
    #define ME_MAX_REWRITE          20                   /**< Maximum URI rewrites */
#endif
#ifndef ME_MAX_ROUTE_MATCHES
    #define ME_MAX_ROUTE_MATCHES    32                   /**< Maximum number of submatches in routes */
#endif
#ifndef ME_MAX_ROUTE_MAP_HASH
    #define ME_MAX_ROUTE_MAP_HASH   17                   /**< Size of the route mapping hash */
#endif
#ifndef ME_MAX_SESSIONS
    #define ME_MAX_SESSIONS         100                  /**< Maximum concurrent sessions */
#endif
#ifndef ME_MAX_SESSION_HASH
    #define ME_MAX_SESSION_HASH     31                   /**< Hash table for session data */
#endif
#ifndef ME_MAX_TX_BODY
    #define ME_MAX_TX_BODY          HTTP_UNLIMITED       /**< Maximum buffer for response data */
#endif
#ifndef ME_MAX_UPLOAD
    #define ME_MAX_UPLOAD           HTTP_UNLIMITED       /**< Maximum file upload size */
#endif
#ifndef ME_MAX_WSS_FRAME
    #define ME_MAX_WSS_FRAME        (4 * 1024)           /**< Default max WebSockets message frame size */
#endif
#ifndef ME_MAX_WSS_PACKET
    #define ME_MAX_WSS_PACKET       (8 * 1024)           /**< Default size to provide to application in one packet */
#endif
#ifndef ME_MAX_WSS_SOCKETS
    #define ME_MAX_WSS_SOCKETS      25                   /**< Default max WebSockets */
#endif
#ifndef ME_MAX_WSS_MESSAGE
    #define ME_MAX_WSS_MESSAGE      (2147483647)         /**< Default max WebSockets message size (2GB) */
#endif
#ifndef ME_MAX_CACHE_DURATION
    #define ME_MAX_CACHE_DURATION   (86400 * 1000)       /**< Default cache lifespan to 1 day */
#endif
#ifndef ME_MAX_INACTIVITY_DURATION
    #define ME_MAX_INACTIVITY_DURATION (30  * 1000)     /**< Default keep alive between requests timeout (30 sec) */
#endif
#ifndef ME_MAX_PARSE_DURATION
    #define ME_MAX_PARSE_DURATION   (5  * 1000)          /**< Default request parse header timeout (5 sec) */
#endif
#ifndef ME_MAX_REQUEST_DURATION
    #define ME_MAX_REQUEST_DURATION (5 * 60 * 1000)     /**< Default request timeout (5 minutes) */
#endif
#ifndef ME_MAX_SESSION_DURATION
    #define ME_MAX_SESSION_DURATION (5 * 60 * 1000)     /**< Default session inactivity timeout (5 mins) */
#endif
#ifndef ME_MAX_PING_DURATION
    #define ME_MAX_PING_DURATION    (30 * 1000)          /**< WSS ping defeat Keep-Alive timeouts (30 sec) */
#endif
#ifndef ME_XSRF_COOKIE
    #define ME_XSRF_COOKIE          "XSRF-TOKEN"          /**< CSRF token cookie name */
#endif
#ifndef ME_XSRF_HEADER
    #define ME_XSRF_HEADER          "X-XSRF-TOKEN"        /**< CSRF token name in Http headers */
#endif
#ifndef ME_XSRF_PARAM
    #define ME_XSRF_PARAM           "-xsrf-"              /**< CSRF parameter in form fields */
#endif

#ifndef ME_HTTP_LOG
    /* Host, "-" username time requeset-line response-status bytes-written local-host */
    #define ME_HTTP_LOG_FORMAT       "%h %l %u %t \"%r\" %>s %b %n"
#endif

#define HTTP_DATE_FORMAT              "%a, %d %b %Y %T GMT"
#define HTTP_MAX_SECRET               16                  /**< Size of secret data for auth */
#define HTTP_SMALL_HASH_SIZE          31                  /* Small hash (less than the alphabet) */
#define HTTP_TIMER_PERIOD             1000                /**< HttpTimer checks ever 1 second */

#define HTTP_PACKET_ALIGN(x)          (((x) + 0x3FF) & ~0x3FF)

/********************************** Defines ***********************************/
/*
    Standard HTTP/1.1 status codes
 */
#define HTTP_CODE_CONTINUE                  100     /**< Continue with request, only partial content transmitted */
#define HTTP_CODE_SWITCHING                 101     /**< Switching protocols */
#define HTTP_CODE_OK                        200     /**< The request completed successfully */
#define HTTP_CODE_CREATED                   201     /**< The request has completed and a new resource was created */
#define HTTP_CODE_ACCEPTED                  202     /**< The request has been accepted and processing is continuing */
#define HTTP_CODE_NOT_AUTHORITATIVE         203     /**< The request has completed but content may be from another source */
#define HTTP_CODE_NO_CONTENT                204     /**< The request has completed and there is no response to send */
#define HTTP_CODE_RESET                     205     /**< The request has completed with no content. Client must reset view */
#define HTTP_CODE_PARTIAL                   206     /**< The request has completed and is returning partial content */
#define HTTP_CODE_MOVED_PERMANENTLY         301     /**< The requested URI has moved permanently to a new location */
#define HTTP_CODE_MOVED_TEMPORARILY         302     /**< The URI has moved temporarily to a new location */
#define HTTP_CODE_SEE_OTHER                 303     /**< The requested URI can be found at another URI location */
#define HTTP_CODE_NOT_MODIFIED              304     /**< The requested resource has changed since the last request */
#define HTTP_CODE_USE_PROXY                 305     /**< The requested resource must be accessed via the location proxy */
#define HTTP_CODE_TEMPORARY_REDIRECT        307     /**< The request should be repeated at another URI location */
#define HTTP_CODE_BAD_REQUEST               400     /**< The request is malformed */
#define HTTP_CODE_UNAUTHORIZED              401     /**< Authentication for the request has failed */
#define HTTP_CODE_PAYMENT_REQUIRED          402     /**< Reserved for future use */
#define HTTP_CODE_FORBIDDEN                 403     /**< The request was legal, but the server refuses to process */
#define HTTP_CODE_NOT_FOUND                 404     /**< The requested resource was not found */
#define HTTP_CODE_BAD_METHOD                405     /**< The request HTTP method was not supported by the resource */
#define HTTP_CODE_NOT_ACCEPTABLE            406     /**< The requested resource cannot generate the required content */
#define HTTP_CODE_REQUEST_TIMEOUT           408     /**< The server timed out waiting for the request to complete */
#define HTTP_CODE_CONFLICT                  409     /**< The request had a conflict in the request headers and URI */
#define HTTP_CODE_GONE                      410     /**< The requested resource is no longer available*/
#define HTTP_CODE_LENGTH_REQUIRED           411     /**< The request did not specify a required content length*/
#define HTTP_CODE_PRECOND_FAILED            412     /**< The server cannot satisfy one of the request preconditions */
#define HTTP_CODE_REQUEST_TOO_LARGE         413     /**< The request is too large for the server to process */
#define HTTP_CODE_REQUEST_URL_TOO_LARGE     414     /**< The request URI is too long for the server to process */
#define HTTP_CODE_UNSUPPORTED_MEDIA_TYPE    415     /**< The request media type is not supported by the server or resource */
#define HTTP_CODE_RANGE_NOT_SATISFIABLE     416     /**< The request content range does not exist for the resource */
#define HTTP_CODE_EXPECTATION_FAILED        417     /**< The server cannot satisfy the Expect header requirements */
#define HTTP_CODE_IM_A_TEAPOT               418     /**< Short and stout error code (RFC 2324) */
#define HTTP_CODE_NO_RESPONSE               444     /**< The connection was closed with no response to the client */
#define HTTP_CODE_INTERNAL_SERVER_ERROR     500     /**< Server processing or configuration error. No response generated */
#define HTTP_CODE_NOT_IMPLEMENTED           501     /**< The server does not recognize the request or method */
#define HTTP_CODE_BAD_GATEWAY               502     /**< The server cannot act as a gateway for the given request */
#define HTTP_CODE_SERVICE_UNAVAILABLE       503     /**< The server is currently unavailable or overloaded */
#define HTTP_CODE_GATEWAY_TIMEOUT           504     /**< The server gateway timed out waiting for the upstream server */
#define HTTP_CODE_BAD_VERSION               505     /**< The server does not support the HTTP protocol version */
#define HTTP_CODE_INSUFFICIENT_STORAGE      507     /**< The server has insufficient storage to complete the request */

/*
    Custom error codes
 */
#define HTTP_CODE_CERT_ERROR                495     /**< The peer provided certificate is unacceptable */

/*
    Proprietary HTTP status codes
 */
#define HTTP_CODE_START_LOCAL_ERRORS        550
#define HTTP_CODE_COMMS_ERROR               550     /**< The server had a communications error responding to the client */
#define HTTP_CODE_BAD_HANDSHAKE             551     /**< The server handsake response is unacceptable */
#define HTTP_CODE_CLIENT_ERROR              552     /**< The server responded, but the response is unacceptable to the client */

/*
    Flags that can be ored into the status code
 */
#define HTTP_CODE_MASK                      0xFFFF
#define HTTP_ABORT                          0x10000 /* Abort the request and network connection */
#define HTTP_CLOSE                          0x20000 /* Close the stream at the completion of the request */

/**
    HttpStream state change notification callback
    @description The notifier callback is invoked for state changes and I/O events. A user notifier function can
        respond to these events with any desired custom code.
        There are four valid event types:
        <ul>
            <li>HTTP_EVENT_STATE. The stream object has changed state. See stream->state.</li>
            <li>HTTP_EVENT_READABLE. The input queue has I/O to read. See stream->readq.
                Use #httpRead to read the data. For WebSockets, use #httpGetPacket.</li>
            <li>HTTP_EVENT_WRITABLE. The output queue is now writable.</li>
            <li>HTTP_EVENT_ERROR. The stream has an error. </li>
        </ul>
    @param stream HttpStream stream object created via #httpCreateStream
    @param event Http state
    @param arg Per-event information
    @ingroup HttpStream
    @stability Stable
 */
typedef void (*HttpNotifier)(struct HttpStream *stream, int event, int arg);

/**
    Set environment vars callback. Invoked per request to permit custom form var definition
    @ingroup HttpStream
    @stability Stable
 */
typedef void (*HttpEnvCallback)(struct HttpStream *stream);

/**
    Listen callback. Invoked after listening on a socket endpoint
    @return "Zero" if the listening endpoint can be opened for service. Otherwise, return a negative MPR error code.
    @ingroup HttpStream
    @stability Stable
 */
typedef int (*HttpListenCallback)(struct HttpEndpoint *endpoint);

/**
    Request completion callback
    @param stream HttpStream object
    @ingroup HttpRx
    @stability Evolving
  */
typedef void (*HttpRequestCallback)(struct HttpStream *stream);

/**
    Timeout callback
    @description The timeout callback for the request inactivity and duration timeouts
    @param stream HttpStream stream object created via #httpCreateStream
    @ingroup HttpStream
    @stability Stable
  */
typedef void (*HttpTimeoutCallback)(struct HttpStream *stream);

/**
    Set the fork callback.
    @param proc Fork callback procedure
    @param arg Argument to supply when the callback is invoked.
    @ingroup HttpStream
    @stability Evolving
 */
PUBLIC void httpSetForkCallback(MprForkCallback proc, void *arg);

/********************************* HttpMonitor ************************************/
/*
    Monitored counters. These are per-client IP unless specified.
 */
#define HTTP_COUNTER_ACTIVE_CLIENTS     0       /**< Active unique client IP addresses */
#define HTTP_COUNTER_ACTIVE_CONNECTIONS 1       /**< Active connections per client */
#define HTTP_COUNTER_ACTIVE_REQUESTS    2       /**< Active requests per client */
#define HTTP_COUNTER_ACTIVE_PROCESSES   3       /**< Total processes for server */
#define HTTP_COUNTER_BAD_REQUEST_ERRORS 4       /**< Bad request format errors */
#define HTTP_COUNTER_ERRORS             5       /**< All errors */
#define HTTP_COUNTER_LIMIT_ERRORS       6       /**< Limit violation errors */
#define HTTP_COUNTER_MEMORY             7       /**< Total application memory for server */
#define HTTP_COUNTER_NETWORK_IO         8       /**< Network I/O */
#define HTTP_COUNTER_NOT_FOUND_ERRORS   9       /**< URI not found errors */
#define HTTP_COUNTER_REQUESTS           10      /**< Request count */
#define HTTP_COUNTER_SSL_ERRORS         11      /**< SSL upgrade errors */
#define HTTP_COUNTER_MAX                12      /**< Max standard counters */

#define HTTP_MONITOR_MIN_PERIOD         (5 * 1000)

/**
    Monitoring counter
    @ingroup HttpMonitor
    @stability Internal
 */
typedef struct HttpCounter {
    uint64      value;                          /**< Current counter value */
} HttpCounter;

/**
    Monitor control structure
    @defgroup HttpMonitor HttpMonitor
    @stability Internal
 */
typedef struct HttpMonitor {
    cchar       *counterName;                   /**< Name of counter to monitor */
    int         counterIndex;                   /**< Counter item index to monitor */
    int         expr;                           /**< Expression. Set to '<' or '>' */
    uint64      limit;                          /**< Comparison limit value */
    MprTicks    period;                         /**< Frequence of comparison */
    MprList     *defenses;                      /**< List of defensive measures */
    MprEvent    *timer;                         /**< Monitor timer */
    struct Http *http;
} HttpMonitor;

/**
    Per-IP address structure
    @ingroup HttpMonitor HttpMonitor
    @stability Internal
 */
typedef struct HttpAddress {
    MprTicks    updated;                        /**< When the address counters were last updated */
    MprTicks    banUntil;                       /**< Ban IP address until this time */
    MprTicks    delayUntil;                     /**< Delay (go-slow) servicing requests until this time  */
    cchar       *banMsg;                        /**< Ban response message */
    int         banStatus;                      /**< Ban response status */
    int         delay;                          /**< Delay per request */
    int         ncounters;                      /**< Number of counters in ncounters */
    int         seqno;                          /**< Uniqueu client sequence number */
    HttpCounter counters[1];                    /**< Counters allocated here */
} HttpAddress;

/**
    Defense remedy callback
    @param args Hash of configuration args for the callback
    @ingroup HttpMonitor
    @stability Evolving
  */
typedef void (*HttpRemedyProc)(MprHash *args);

/**
    Monitor defense configuration
    @ingroup HttpMonitor
    @stability Evolving
 */
typedef struct HttpDefense {
    cchar           *name;                      /**< Defense name */
    cchar           *remedy;                    /**< Remedy name to invoke */
    MprHash         *args;                      /**< Remedy arguments */
    MprHash         *suppress;                  /**< Active defenses to suppress */
    MprTicks        suppressPeriod;             /**< Period to suppress defense */
    int             suppressed;                 /**< Number of remedies suppressed */
} HttpDefense;

/**
    Monitor an event and validate against defined limits and monitored resources
    @description The Http library supports a suite of resource limits that restrict the impact of a request on
        the system. This call validates a processing event for the current request against the server's endpoint limits.
    @param stream HttpStream stream object
    @param counter The counter to adjust.
    @param adj Value to adjust the counter by. May be positive or negative.
    @return Monitor value after applying the adjustment.
    @ingroup HttpMonitor
    @stability Evolving
 */
PUBLIC int64 httpMonitorEvent(struct HttpStream *stream, int counter, int64 adj);

/**
    Monitor a network event and validate against defined limits and monitored resources
    @description The Http library supports a suite of resource limits that restrict the impact of a request on
        the system. This call validates a processing event for the current request against the server's endpoint limits.
    @param net Network object.
    @param counter The counter to adjust.
    @param adj Value to adjust the counter by. May be positive or negative.
    @return Monitor value after applying the adjustment.
    @ingroup HttpMonitor
    @stability Evolving
 */
PUBLIC int64 httpMonitorNetEvent(struct HttpNet *net, int counter, int64 adj);

/**
    Add a monitor
    @param counter Name of counter to monitor. Some of the standard counter names are:
        ActiveClients, ActiveConnections, ActiveRequests, ActiveProcesses, BadRequestErrors, LimitErrors, Memory,
        NotFoundErrors, NetworkIO, Requests, SSLErrors, TotalErrors
    @param expr Expression operator. Select from "<" or ">".
    @param limit Limit value to compare with the counter value.
    @param period Time period over which to determine the counter value.
    @param defenses List of defenses to invoke if the counter exceeds the limit value over the designated period.
    @return Zero if successful, otherwise a negative MPR error code.
    @ingroup HttpMonitor
    @stability Evolving
 */
PUBLIC int httpAddMonitor(cchar *counter, cchar *expr, uint64 limit, MprTicks period, cchar *defenses);

/**
    Add a defense
    @param name Name of defensive policy
    @param remedy Remedy action to invoke. Standard remedies include: ban, cmd, delay, email, http and log.
        This can be null and the remedy can be specified via REMEDY=remedy in the args.
    @param args Arguments to pass to the remedy. These may include ${tokens}.
    @return Zero if successful, otherwise a negative MPR error code.
    @ingroup HttpMonitor
    @stability Evolving
 */
PUBLIC int httpAddDefense(cchar *name, cchar *remedy, cchar *args);

/**
    Add a defense using JSON arguments
    @param name Name of defensive policy
    @param remedy Remedy action to invoke. Standard remedies include: ban, cmd, delay, email, http and log.
        This can be null and the remedy can be specified via REMEDY=remedy in the args.
    @param jargs Arguments to pass to the remedy as a JSON object. These may include ${tokens}.
    @return Zero if successful, otherwise a negative MPR error code.
    @ingroup HttpMonitor
    @stability Evolving
 */
PUBLIC int httpAddDefenseFromJson(cchar *name, cchar *remedy, MprJson *jargs);

/**
    Add a counter to be monitored
    @param name Name of the counter
    @return The counter index in HttpAddress.counters[] to use
    @ingroup HttpMonitor
    @stability Evolving
 */
PUBLIC int httpAddCounter(cchar *name);

/**
    Add a remedy
    @param name Name of the remedy
    @param remedy Remedy callback function
    @return Zero if successful, otherwise a negative MPR error code.
    @ingroup HttpMonitor
    @stability Evolving
 */
PUBLIC int httpAddRemedy(cchar *name, HttpRemedyProc remedy);

/**
    Ban a client IP from service
    @param ip Client IP address to ban
    @param period Period in milliseconds to ban the client
    @param status If non-zero, then return a HTTP response to the client with this HTTP status.
    @param msg If non-null, then return a HTTP response with this message. If both status and msg are zero and null respectively,
        then do not send a response to the client, rather immediately close the network connection.
    @ingroup HttpMonitor
    @stability Evolving
 */
PUBLIC int httpBanClient(cchar *ip, MprTicks period, int status, cchar *msg);

/**
    Print the monitor counters to the error log
    @ingroup HttpMonitor
    @stability Evolving
  */
PUBLIC void httpDumpCounters(void);

/*
    Internal
 */
PUBLIC void httpAddCounters(void);
PUBLIC int httpAddRemedies(void);
PUBLIC MprTicks httpGetTicks(cchar *value);
PUBLIC uint64 httpGetNumber(cchar *value);
PUBLIC int httpGetInt(cchar *value);
PUBLIC void httpPruneMonitors(void);
PUBLIC HttpAddress *httpMonitorAddress(struct HttpNet *net, int counterIndex);

/********************************** HttpTrace *********************************/

#define HTTP_TRACE_MAX_SIZE         (10 * 1024) /**< Default maximum body size to trace */
#define HTTP_TRACE_MIN_LOG_SIZE     (10 * 1024) /**< Minimum log file size */

/*
    Formatter flags
 */
#define HTTP_TRACE_PACKET           0x1         /**< Trace a packet */
#define HTTP_TRACE_HEX              0x2         /**< Format content in hex with side ascii */

/**
    Trace formatter callback
    @param trace Trace object
    @param stream Stream object
    @param event Event to trace
    @param type Type of event to trace
    @param values Formatted comma separated key=value pairs
    @param data Data buffer
    @param len Length of data. May be zero.
    @stability Evolving
    @ingroup HttpTrace
 */
typedef void (*HttpTraceFormatter)(struct HttpTrace *trace, cchar *event, cchar *type, int flags, cchar *data, ssize len, cchar *fmt, va_list args);

/**
    Trace logger callback
    @param trace Trace object
    @param data Data buffer to write.
    @param len Length of data. May be zero.
    @stability Evolving
    @ingroup HttpTrace
 */
typedef void (*HttpTraceLogger)(struct HttpTrace *trace, cchar *data, ssize len);

/**
    Trace management structure
    @stability Evolving
    @defgroup HttpTrace HttpTrace
  */
typedef struct HttpTrace {
    cchar               *format;                        /**< Output format (used by Common Log Format) */
    cchar               *path;                          /**< Trace logger filename */
    cchar               *lastTime;                      /**< Most recent time string */
    MprTime             lastMark;                       /**< When lastTime was last updated */
    MprBuf              *buf;                           /**< Output buffer */
    MprFile             *file;                          /**< Trace logger file object */
    int                 backupCount;                    /**< Trace logger backup count */
    int                 flags;                          /**< Trace control flags (append|anew) */
    MprOff              size;                           /**< Max trace log size */
    ssize               maxContent;                     /**< Maximum content size to trace */
    MprHash             *events;                        /**< Configuration of events */
    HttpTraceFormatter  formatter;                      /**< Trace formatter */
    HttpTraceLogger     logger;                         /**< Trace logger */
    struct HttpTrace    *parent;                        /**< Parent trace */
    MprMutex            *mutex;                         /**< Multithread sync */
} HttpTrace;

/**
    Backup the request trace log if required
    @description If the log file is greater than the maximum configured, or MPR_ANEW was set via httpSetTraceLog,
    then archive the log.
    @param trace HttpTrace object
    @return Zero if successful, otherwise a negative MPR error code.
    @ingroup HttpTrace
    @stability Evolving
 */
PUBLIC int httpBackupTraceLogFile(HttpTrace *trace);

/**
    Common Log trace formatter
    @param trace HttpTrace object
    @param event Event to trace
    @param type Event type
    @param flags Formatting flags (HTTP_TRACE_PACKET and set buf to a HttpPacket)
    @param buf Data buffer to trace.
    @param len Length of the data buf.
    @param fmt Printf style formatted string
    @param args Varargs arguments for fmt
    @ingroup HttpTrace
    @stability Evolving
 */
PUBLIC void httpCommonTraceFormatter(HttpTrace *trace, cchar *event, cchar *type, int flags, cchar *buf, ssize len, cchar *fmt, va_list args);

/**
    Create a trace object.
    @description If parent is defined, inherit default settings from the parent
    @param parent Parent trace object from which to inherit settings
    @ingroup HttpTrace
    @stability Evolving
    @internal
 */
PUBLIC HttpTrace *httpCreateTrace(HttpTrace *parent);

/**
    Detailed log trace formatter
    @param trace HttpTrace object
    @param event Event to trace
    @param type Event type to trace
    @param flags Formatting flags (HTTP_TRACE_PACKET and set buf to a HttpPacket)
    @param buf Data buffer to trace.
    @param len Length of the data buf.
    @param buf Trace data buffer to write
    @param fmt Printf style formatted string
    @param args Varargs arguments for fmt
    @ingroup HttpTrace
    @stability Evolving
 */
PUBLIC void httpDetailTraceFormatter(HttpTrace *trace, cchar *event, cchar *type, int flags, cchar *buf, ssize len, cchar *fmt, va_list args);

/**
    Simple log trace formatter for debugging
    @param trace HttpTrace object
    @param event Event to trace
    @param type Event type to trace
    @param flags Formatting flags (HTTP_TRACE_PACKET and set buf to a HttpPacket)
    @param buf Data buffer to trace.
    @param len Length of the data buf.
    @param buf Trace data buffer to write
    @param fmt Printf style formatted string
    @param args Varargs arguments for fmt
    @ingroup HttpTrace
    @stability Evolving
 */
PUBLIC void httpSimpleTraceFormatter(HttpTrace *trace, cchar *event, cchar *type, int flags, cchar *buf, ssize len, cchar *fmt, va_list args);

/**
    Convenience routine to format trace via the configured formatter
    @description The formatter will invoke the trace logger and actually write the trace mesage
    @param trace HttpTrace object
    @param type Event type to trace
    @param flags Formatting flags (HTTP_TRACE_PACKET and set buf to a HttpPacket)
    @param event Event name to trace
    @param buf Trace data buffer to write
    @param len Length of data buffer
    @param fmt Printf style formatted string
    @param args Varargs arguments for fmt
    @ingroup HttpTrace
    @stability Evolving
 */
PUBLIC void httpFormatTrace(HttpTrace *trace, cchar *event, cchar *type, int flags, cchar *buf, ssize len, cchar *fmt, va_list args);

/*
    Trace LogFile logger
    @description Open the trace log file defined in the HttpTrace object
    @param trace Trace object
    @stability Evolving
    @internal
 */
PUBLIC int httpOpenTraceLogFile(HttpTrace *trace);

/**
    Set the formatter callback to use with a trace object
    @description The trace formatter should
    @param trace Trace object to configure
    @param callback Formatter callback
    @return Prior trace formatter
    @ingroup HttpTrace
    @stability Evolving
 */
PUBLIC HttpTraceFormatter httpSetTraceFormatter(HttpTrace *trace, HttpTraceFormatter callback);

/**
    Set the logging format
    @description This is used by the Common log formatter to define the fields written to the log
    @param trace Trace object
    @param format The format string defaults to: "%h %l %u %t \"%r\" %>s %b %n".
    @ingroup HttpTrace
    @stability Evolving
 */

PUBLIC void httpSetTraceFormat(HttpTrace *trace, cchar *format);

/**
    Set the current tracing verbosity level.
    @description This call defines the maximum trace level of messages that will be
        traced. Trace events have an associated verbosity level at which they will be enabled.
        If the event level is greater than the defined tracing verbosity level, the event is ignored.
    @param level New tracing level. Must be 0-5 inclusive.
    @ingroup HttpTrace
    @stability Evolving.
 */
PUBLIC void httpSetTraceLevel(int level);

/**
    Get the current tracing level
    @return The tracing level 0-5
    @ingroup HttpTrace
    @stability Evolving
 */
PUBLIC int httpGetTraceLevel(void);

/**
    Configure the tracing level for an event type
    @param trace Tracing object
    @param type Event type to modify
    @param level Desired trace level (0-5)
    @ingroup HttpTrace
    @stability Evolving.
    @internal
 */
PUBLIC void httpSetTraceEventLevel(HttpTrace *trace, cchar *type, int level);

/**
    Set the maximum content size to trace
    @description Tracing will be suspended for files that are larger than this size.
    @param trace Tracing object
    @param size Maximum content size to trace
    @ingroup HttpTrace
    @stability Evolving.
 */
PUBLIC void httpSetTraceContentSize(HttpTrace *trace, ssize size);

/**
    Set the trace callback to use with a trace object
    @description The trace logger is responsible for taking formatted messages and writing to the log.
    @param trace Trace object to configure
    @param callback Trace logger callback
    @ingroup HttpTrace
    @stability Evolving
 */
PUBLIC void httpSetTraceLogger(HttpTrace *trace, HttpTraceLogger callback);

/**
    Configure the request trace log
    @param trace HttpTrace object
    @param path Path for request trace log file.
    @param size Maximum size of the log file before archiving
    @param backup Set to true to create a backup of the log file if archiving.
    @param format Log file format
    @param flags Set to MPR_LOG_ANEW to archive the log when the application reboots.
    @return Zero if successful, otherwise a negative MPR error code.
    @ingroup HttpTrace
    @stability Evolving
 */
PUBLIC int httpSetTraceLogFile(HttpTrace *trace, cchar *path, ssize size, int backup, cchar *format, int flags);

/**
    Define the trace formatter by name
    @param trace Tracing object
    @param name Formatter name. Set to "common" for the Common Log format or "detail" for the Appweb detailed trace format.
    @ingroup HttpTrace
    @stability Evolving.
    @internal
 */
PUBLIC void httpSetTraceFormatterName(HttpTrace *trace, cchar *name);

#define httpTracing(net) (net->http->traceLevel > 0)

/**
    Start tracing for the given trace log file when instructed via a command line switch.
    @param traceSpec Set the trace log file name and level. The format is "pathName[:level]".
    The following levels are generally observed:
    <ul>
        <li>0 - Essential messages, fatal errors and critical warnings</li>
        <li>1 - Hard errors</li>
        <li>2 - Configuration setup and soft warnings</li>
        <li>3 - Useful informational messages</li>
        <li>4 - Debug information</li>
        <li>5 - Most verbose levels of messages useful for debugging</li>
    </ul>
    If the traceSpec is null, not tracing is enabled.
    @return Zero if successful, otherwise a negative Mpr error code. See the Appweb log for diagnostics.
    @ingroup HttpTrace
    @stability Evolving
*/
PUBLIC int httpStartTracing(cchar *traceSpec);

#if DOXYGEN
/**
    Trace an event of interest
    @description The Http trace log is for operational request and server messages and should be used in preference to
    the MPR error log which should be used only for configuration and hard system-wide errors.
    @param trace HttpTrace object. Typically used via HttpStream.trace or HttpNet.trace.
    @param event Event name to trace.
    @param type Event type to trace. Events are grouped into types that are traced at the same level.
    The standard set of types and their default trace levels are:
    debug: 0, request:1, error: 2, result:2, context:3, packet:4, detail:5. Users can create custom types.
    The request type is used for the initial http request line. The result type is used for the request status.
    The context type is used for general information including http headers. The form type is used for POST form data.
    The body type is used for request body data.
    \n\n
    Context type events may include a "msg" value field. By convention, these messages should be aggregated by trace
    formatters so that subsequent context events do not overwrite prior msg values.
    \n\n
    Event types are orthogonal to event names.
    @param fmt Printf style format string. String should be comma separated key=value pairs
    @param ... Arguments for fmt
    @return True if the event was traced
    @ingroup HttpTrace
    @stability Evolving
 */
PUBLIC bool httpTrace(HttpTrace *trace, cchar *event, cchar *type, cchar *fmt, ...);
#else
    #define httpTrace(trace, event, type, ...) \
        if (trace && HTTP->traceLevel > 0) { \
            int __tlevel = PTOI(mprLookupKey(trace->events, type)); \
            if (__tlevel >= 0 && __tlevel <= HTTP->traceLevel) { \
                httpLogProc(trace, event, type, 0, __VA_ARGS__); \
            } \
        } else

#endif

PUBLIC bool httpLogProc(HttpTrace *trace, cchar *event, cchar *type, int flags, cchar *fmt, ...) PRINTF_ATTRIBUTE(5,6);

/**
    Trace with data buffer
    @description This is similar to #httpTrace but will also trace the contents of a data buffer.
    If the buffer contains binary data, it will be displayed in hex format. The content will be traced up
    to the maximum size defined via #httpSetTraceLogFile.
    @param trace HttpTrace object. Typically used via HttpStream.trace or HttpNet.trace.
    @param event Event to trace
    @param type Event type to trace
    @param flags Output formatting flags
    @param buf Data buffer to trace
    @param len Size of the data buffer.
    @param fmt Printf style format string. String should be comma separated key=value pairs
    @param ... Arguments for fmt
    @return True if the event was traced
    @ingroup HttpTrace
    @stability Evolving
 */
PUBLIC bool httpTraceData(HttpTrace *trace, cchar *event, cchar *type, int flags, cchar *buf, ssize len, cchar *fmt, ...) PRINTF_ATTRIBUTE(7,8);

/**
    Trace request packet
    @description This is similar to #httpTraceData but accepts a packet as a parameter.
    If the buffer contains binary data, it will be displayed in hex format. The content will be traced up
    to the maximum size defined via #httpSetTraceLogFile.
    @param trace HttpTrace object. Typically used via HttpStream.trace or HttpNet.trace.
    @param event Event to trace
    @param type Event type to trace
    @param flags Output formatting flags
    @param packet Packet to trace.
    @param values Formatted comma separated key=value pairs
    @param ... Arguments for fmt
    @return True if the event was traced
    @ingroup HttpTrace
    @stability Evolving
 */
PUBLIC bool httpTracePacket(HttpTrace *trace, cchar *event, cchar *type, int flags, struct HttpPacket *packet, cchar *values, ...) PRINTF_ATTRIBUTE(6,7);

/**
    Convenience routine to write trace to the trace logger. Should only be used by formatters.
    @param trace HttpTrace object
    @param buf Trace message to write
    @param len Length of trace message
    @ingroup HttpTrace
    @stability Evolving
 */
PUBLIC void httpWriteTrace(HttpTrace *trace, cchar *buf, ssize len);

/**
    Write a message to the trace file logger
    @param trace HttpTrace object
    @param buf Message to write
    @param len Length of message
    @ingroup HttpTrace
    @stability Evolving
 */
PUBLIC void httpWriteTraceLogFile(HttpTrace *trace, cchar *buf, ssize len);

/*
    Internal
 */
PUBLIC cchar *httpMakePrintable(HttpTrace *trace, cchar *buf, bool *hex, ssize *lenp);

/************************************ Http **********************************/
/**
    Http service object
    @description Configuration is not thread safe and must occur at initialization time when the application is
    single threaded. If the configuration is modified when the application is multithreaded, all requests must be
    first be quiesced.
    @defgroup Http Http
    @see Http HttpStream HttpEndpoint gettGetDateString httpCreate httpGetContext httpGetDateString
        httpLookupEndpoint httpLookupStatus httpLooupHost httpSetContext httpSetDefaultClientHost
        httpSetDefaultClientPort httpSetDefaultPort httpSetForkCallback httpSetProxy httpSetSoftware httpConfigure
    @stability Internal
 */
typedef struct Http {
    MprList         *endpoints;             /**< Currently configured listening endpoints */
    MprList         *hosts;                 /**< List of host objects */
    MprList         *networks;              /**< Currently open network connections */
    MprHash         *parsers;               /**< Table config parser callbacks */
    MprHash         *stages;                /**< Possible stages in connection pipelines */
    MprCache        *sessionCache;          /**< Session state cache */
    MprHash         *statusCodes;           /**< Http status codes */

    MprHash         *routeSets;             /**< Http route sets functions */
    MprHash         *routeTargets;          /**< Http route target functions */
    MprHash         *routeConditions;       /**< Http route condition functions */
    MprHash         *routeUpdates;          /**< Http route update functions */

    MprHash         *authTypes;             /**< Available authentication protocol types */
    MprHash         *authStores;            /**< Available password stores */
    MprHash         *dateCache;             /**< Cache of date modified times */

    MprList         *staticHeaders;         /**< HTTP/2 static headers */
    MprList         *counters;              /**< List of counters */
    MprList         *monitors;              /**< List of monitors */
    MprHash         *defenses;              /**< List of Defenses */
    MprHash         *remedies;              /**< List of Defense Remedies */
    MprHash         *addresses;             /**< Monitored per-IP-address counters */

    /*
        Some standard pipeline stages
     */
    struct HttpStage *actionHandler;        /**< Action handler */
    struct HttpStage *cacheFilter;          /**< Cache filter */
    struct HttpStage *cacheHandler;         /**< Cache filter */
    struct HttpStage *chunkFilter;          /**< Chunked transfer encoding filter */
    struct HttpStage *httpFilter;           /**< Http filter */
    struct HttpStage *cgiHandler;           /**< CGI handler */
    struct HttpStage *cgiConnector;         /**< CGI connector */
    struct HttpStage *clientHandler;        /**< Client-side handler (dummy) */
    struct HttpStage *dirHandler;           /**< Directory listing handler */
    struct HttpStage *egiHandler;           /**< Embedded Gateway Interface (EGI) handler */
#if DEPRECATED || 1
    struct HttpStage *ejsHandler;           /**< Ejscript Web Framework handler */
#endif
    struct HttpStage *espHandler;           /**< ESP Web Framework handler */
    struct HttpStage *fileHandler;          /**< Static file handler */
    struct HttpStage *netConnector;         /**< Default network connector */
    struct HttpStage *passHandler;          /**< Pass through handler */
    struct HttpStage *phpHandler;           /**< PHP through handler */
    struct HttpStage *rangeFilter;          /**< Ranged requests filter */
    struct HttpStage *tailFilter;           /**< Tail filter */
    struct HttpStage *uploadFilter;         /**< Upload filter */

#if ME_HTTP_WEB_SOCKETS
    struct HttpStage *webSocketFilter;      /**< WebSocket filter */
#endif
    struct HttpStage *http1Filter;          /**< Http/1 filter */
#if ME_HTTP_HTTP2
    struct HttpStage *http2Filter;          /**< Http/2 filter */
#endif
    struct HttpLimits *clientLimits;        /**< Client resource limits */
    struct HttpLimits *serverLimits;        /**< Server resource limits */
    struct HttpRoute *clientRoute;          /**< Default route for clients */

    MprEvent        *timer;                 /**< Admin service timer */
    MprEvent        *timestamp;             /**< Timestamp timer */
    MprTime         booted;                 /**< Time the server started */
    MprTicks        now;                    /**< Current time in ticks */
    MprMutex        *mutex;                 /**< Multithread sync */
    HttpTrace       *trace;                 /**< Default tracing configuration */

    char            *software;              /**< Software name and version */
    void            *forkData;

    int             monitorsStarted;        /**< Monitors are running */
    MprTicks        monitorPeriod;          /**< Minimum monitor period */

    int             nextAuth;               /**< Auth object version vector */
    int             activeProcesses;        /**< Count of active external processes */
    uint64          totalConnections;       /**< Total connections accepted */
    uint64          totalRequests;          /**< Total requests served */

    int             flags;                  /**< Open flags */
    void            *context;               /**< Embedding context */
    MprTicks        currentTime;            /**< When currentDate was last calculated (ticks) */
    char            *currentDate;           /**< Date string for HTTP response headers */
    char            *secret;                /**< Random bytes for authentication */

    char            *defaultClientHost;     /**< Default ip address */
    int             defaultClientPort;      /**< Default port */
    char            *proxyHost;             /**< Proxy ip address */
    int             proxyPort;              /**< Proxy port */

    cchar           *group;                 /**< O/S application group name */
    cchar           *localPlatform;         /**< Local (dev) platform os-arch-profile (lower case) */
    cchar           *platform;              /**< Target platform os-arch-profile (lower case) */
    cchar           *platformDir;           /**< Path to platform directory containing binaries */
    cchar           *user;                  /**< O/S application user name */
    int             uid;                    /**< User Id */
    int             gid;                    /**< Group Id */
    int             userChanged;            /**< User name changed */
    int             groupChanged;           /**< Group name changed */
    int             staticLink;             /**< Target platform is using a static linking */
    int             traceLevel;             /**< Current request trace level */
    int             startLevel;             /**< Start endpoint trace level */

    /*
        Callbacks
     */
    HttpEnvCallback     envCallback;        /**< SetEnv callback */
    MprForkCallback     forkCallback;       /**< Callback in child after fork() */
    HttpListenCallback  listenCallback;     /**< Invoked when creating listeners */
    HttpRequestCallback requestCallback;    /**< Request completion callback */

} Http;

#if DOXYGEN
    /**
        Return the MPR control instance.
        @description Return the MPR singleton control object.
        @return Returns the MPR control object.
        @ingroup Mpr
        @stability Stable.
     */
    PUBLIC Http *HTTP;
#elif ME_WIN_LIKE
    PUBLIC Http *httpGetHttp(void);
    #define HTTP httpGetHttp()
#else
    PUBLIC_DATA Http *HTTP;
#endif

/**
    Callback procedure for HttpConfigure
    @param arg User definable data. May be managed or unmanaged.
    @ingroup Http
    @stability Evolving
 */
typedef void (*HttpConfigureProc)(void *arg);

/**
    Apply the changed group ID.
    @description Apply configuration changes and actually change the group id
    @return Zero if successful, otherwise a negative Mpr error code. See the Appweb log for diagnostics.
    @ingroup Http
    @stability Evolving
 */
PUBLIC int httpApplyChangedGroup(void);

/**
    Apply the changed user ID
    @description Apply configuration changes and actually change the user id
    @ingroup Http
    @stability Evolving
 */
PUBLIC int httpApplyChangedUser(void);

/**
    Apply the changed user and group ID.
    @description Apply configuration changes and actually change the user and group id
    @return Zero if successful, otherwise a negative Mpr error code. See the Appweb log for diagnostics.
    @ingroup Http
    @stability Evolving
 */
PUBLIC int httpApplyUserGroup(void);

/*
    Flags for httpCreate
 */
#define HTTP_CLIENT_SIDE    0x1             /**< Initialize the client-side support */
#define HTTP_SERVER_SIDE    0x2             /**< Initialize the server-side support */

/**
    Alter the configuration by first quiescing all Http activity. This waits until there are no open connections
    and then invokes the configuration callback while blocking further connections. When the callback completes,
    connections are resumed with the new configuration.
    This callback is required because configuration of the Http engine must be done when single-threaded.
    @param proc Function of the type HttpConfigureProc.
    @param arg Reference argument to pass to the callback proc. Can be a managed or an unmanaged reference.
    @param timeout Timeout in milliseconds to wait. Set to -1 to use the default server inactivity timeout. Set to zero
        to wait forever.
    @ingroup Http
    @stability Evolving
  */
PUBLIC bool httpConfigure(HttpConfigureProc proc, void *arg, MprTicks timeout);

/**
    Create a Http service object
    @description Create a http service object. One http service object should be created per application.
    @param flags Set to zero to initialize bo Initialize the client-side support only.
    @return The http service object.
    @ingroup Http
    @stability Stable
 */
PUBLIC Http *httpCreate(int flags);

/**
    Destroy the Http service.
    @description This routine is invoked as the final stage in shutting down the http service.
        It stops the request timeout timer and releases all http memory.
    @ingroup Http
    @stability Internal
 */
PUBLIC void httpDestroy(void);

/**
    Get the http context object
    @return The http context object defined via httpSetContext
    @ingroup Http
    @stability Stable
 */
PUBLIC void *httpGetContext(void);

/**
    Get the time as an ISO date string
    @param sbuf Optional path buffer. If supplied, the modified time of the path is used. If NULL, then the current
        time is used.
    @return RFC822 formatted date string.
    @ingroup Http
    @stability Stable
 */
PUBLIC char *httpGetDateString(MprPath *sbuf);

/**
    Get the user group
    @description Get the user and group ID for the process
    @ingroup Http
    @stability Internal
 */
PUBLIC void httpGetUserGroup(void);

/**
    Initialize the Http configuration parser
    @return Zero if successful, otherwise a negative Mpr error code. See the Appweb log for diagnostics.
    @ingroup Http
    @stability Evolving
 */
PUBLIC int httpInitParser(void);

/**
    Lookup a Http status code
    @description Lookup the code and return the corresponding text message briefly expaining the status.
    @param status Http status code
    @return Text message corresponding to the status code
    @ingroup Http
    @stability Stable
 */
PUBLIC cchar *httpLookupStatus(int status);

/**
    Lookup a host by name
    @param name The name of the host to find
    @return The corresponding host object
    @ingroup Http
    @stability Stable
 */
PUBLIC struct HttpHost *httpLookupHost(cchar *name);

/**
    Lookup a listening endpoint
    @param ip Listening IP address to look for
    @param port Listening port number
    @return HttpEndpoint object
    @ingroup Http
    @stability Stable
 */
PUBLIC struct HttpEndpoint *httpLookupEndpoint(cchar *ip, int port);

/**
    Parse a platform string
    @param platform The platform string. Must be of the form: os-arch-profile
    @param os Parsed O/S portion
    @param arch Parsed architecture portion
    @param profile Parsed profile portion
    @return Zero if successful, otherwise a negative Mpr error code.
    @ingroup Http
    @stability Evolving
 */
PUBLIC int httpParsePlatform(cchar *platform, cchar **os, cchar **arch, cchar **profile);

/**
    Set the http context object
    @param context New context object
    @ingroup Http
    @stability Stable
 */
PUBLIC void httpSetContext(void *context);

/**
    Define a default client host
    @description Define a default host to use for client connections if the URI does not specify a host
    @param host Host or IP address
    @ingroup Http
    @stability Stable
 */
PUBLIC void httpSetDefaultClientHost(cchar *host);

/**
    Define a default client port
    @description Define a default port to use for client connections if the URI does not define a port
    @param port Integer port number
    @ingroup Http
    @stability Stable
 */
PUBLIC void httpSetDefaultClientPort(int port);

/**
    Set the group account
    @description Define the group account name under which to run the process
    @param group Group name. Must be defined in the system group database.
    @return Zero if successful, otherwise a negative Mpr error code.
    @ingroup Http
    @stability Evolving
 */
PUBLIC int httpSetGroupAccount(cchar *group);

/**
    Set platform description
    @description Some web frameworks need to recompile sources before serving requests (ESP).
        These need access to the http libraries to link with.
    @param platform Platform string of the form: OS-ARCH-PROFILE.
    @return Zero if the platform string parses, otherwise a negative Mpr error code.
    @ingroup Http
    @stability Evolving
 */
PUBLIC int httpSetPlatform(cchar *platform);

/**
    Set platform directory location
    @description Set the platform directory location which contains libraries and headers for the application.
    @param platform Path to the platform directory.
    @return Zero if successful, otherwise a negative Mpr error code.
    @ingroup Http
    @stability Evolving
 */
PUBLIC int httpSetPlatformDir(cchar *platform);

/**
    Define a Http proxy host to use for all client connect requests.
    @description Define a http proxy host to communicate via when accessing the net.
    @param host Proxy host name or IP address
    @param port Proxy host port number.
    @ingroup Http
    @stability Stable
 */
PUBLIC void httpSetProxy(cchar *host, int port);

/**
    Set the software description
    @param description String describing the Http software. By default, this is set to HTTP_NAME.
    @ingroup Http
    @stability Stable
 */
PUBLIC void httpSetSoftware(cchar *description);

/**
    Set the user account
    @description Define the user account name under which to run the process
    @param user User name. Must be defined in the system password database.
    @return Zero if successful, otherwise a negative Mpr error code.
    @ingroup Http
    @stability Evolving
 */
PUBLIC int httpSetUserAccount(cchar *user);

/* Internal APIs */
PUBLIC void httpAddStream(struct HttpNet *net, struct HttpStream *stream);
PUBLIC void httpRemoveStream(struct HttpNet *net, struct HttpStream *stream);
PUBLIC void httpAddNet(struct HttpNet *net);
PUBLIC void httpRemoveNet(struct HttpNet *net);
PUBLIC struct HttpEndpoint *httpGetFirstEndpoint(void);
PUBLIC void httpAddEndpoint(struct HttpEndpoint *endpoint);
PUBLIC void httpRemoveEndpoint(struct HttpEndpoint *endpoint);
PUBLIC void httpAddHost(struct HttpHost *host);
PUBLIC void httpRemoveHost(struct HttpHost *host);
PUBLIC void httpDefineRouteBuiltins(void);
PUBLIC void httpSetInfoLevel(int level);
PUBLIC void httpStopNetworks(void *data);

/*********************************** HttpStats ********************************/
/**
    HttpStats
    @defgroup HttpStats HttpStats
    @stability Internal
 */
typedef struct HttpStats {
    uint64  ram;                        /**< System total RAM */
    uint64  mem;                        /**< Current application memory (includes code + data + heap) */
    uint64  memRedline;                 /**< Memory heap warnHeap limit */
    uint64  memMax;                     /**< Memory heap maximum permitted */
    uint64  memSessions;                /**< Memory used for sessions */

    uint64  heap;                       /**< Current application heap memory */
    uint64  heapPeak;                   /**< Peak heap memory usage */
    uint64  heapUsed;                   /**< Current heap memory in use */
    uint64  heapFree;                   /**< Current heap memory available */
    uint    heapRegions;                /**< Count of heap memory regions */

    int     workersBusy;                /**< Current busy worker threads */
    int     workersIdle;                /**< Current idle worker threads */
    int     workersYielded;             /**< Number of busy workers that are yielded for GC */
    int     workersMax;                 /**< Maximum number of workers in the thread pool */

    int     activeClients;              /**< Current active client IPs */
    int     activeConnections;          /**< Current active connections */
    int     activeProcesses;            /**< Current active processes */
    int     activeRequests;             /**< Current active requests */
    int     activeSessions;             /**< Current active sessions */

    uint64  totalSweeps;                /**< Total GC sweeps */
    uint64  totalRequests;              /**< Total requests served */
    uint64  totalConnections;           /**< Total connections accepted */
    uint64  cpuUsage;                   /**< Total process CPU usage in ticks */
    int     cpuCores;
} HttpStats;

#define HTTP_STATS_MEMORY   0x1
#define HTTP_STATS_ALL      0x1

/**
    Get an Http performance report
    @param flags reserved
    @return String containing the report
    @ingroup HttpStats
    @stability Internal
 */
PUBLIC char *httpStatsReport(int flags);

/**
    Get the Http performance statistics
    @param sp Reference to a HttpStats structure
    @ingroup HttpStats
    @stability Internal
 */
PUBLIC void httpGetStats(HttpStats *sp);

/************************************* Limits *********************************/
/**
    Http limits
    @defgroup HttpLimits HttpLimits
    @see HttpLimits httpInitLimits httpCreateLimits httpEaseLimits
    @stability Internal
 */
typedef struct HttpLimits {
    int      cacheItemSize;             /**< Maximum size of a cachable item */
    ssize    chunkSize;                 /**< Maximum chunk size for transfer encoding */
    int      clientMax;                 /**< Maximum number of unique clients IP addresses */
    int      connectionsMax;            /**< Maximum number of simultaneous client connections */
    int      headerMax;                 /**< Maximum number of header lines */
    int      headerSize;                /**< Maximum size of the total header */
    MprTicks inactivityTimeout;         /**< Timeout for keep-alive and idle requests (msec) */
    int      keepAliveMax;              /**< Maximum number of Keep-Alive requests to perform per socket */
    int      packetSize;                /**< Maximum packet size for queues and stages */
    int      processMax;                /**< Maximum number of processes (CGI) */
    int      requestMax;                /**< Maximum number of simultaneous concurrent requests */
    MprTicks requestTimeout;            /**< Time a request can take (msec) */
    MprTicks requestParseTimeout;       /**< Time a request can take to parse the request headers (msec) */
    int      requestsPerClientMax;      /**< Maximum number of requests per client IP */
    MprOff   rxBodySize;                /**< Maximum size of receive body data */
    MprOff   rxFormSize;                /**< Maximum size of form data */
    int      sessionMax;                /**< Maximum number of sessions */
    MprTicks sessionTimeout;            /**< Time a session can persist (msec) */
    MprOff   txBodySize;                /**< Maximum size of transmission body content */
    MprOff   uploadSize;                /**< Maximum size of an uploaded file */
    int      uriSize;                   /**< Maximum size of a uri */

#if ME_HTTP_WEB_SOCKETS || DOXYGEN
    int      webSocketsFrameSize;       /**< Maximum size of sent WebSocket frames. Incoming frames have no limit
                                             except message size.  */
    int      webSocketsMax;             /**< Maximum number of WebSockets */
    int      webSocketsMessageSize;     /**< Maximum total size of a WebSocket message including all frames */
    int      webSocketsPacketSize;      /**< Maximum size of a WebSocket packet exchanged with the user callback */
    MprTicks webSocketsPing;            /**< Time between pings */
#endif
#if ME_HTTP_HTTP2 || DOXYGEN
    int      frameSize;                 /**< HTTP/2 maximum frame size */
    int      hpackMax;                  /**< HTTP/2 maximum size of the hpack header table */
    int      streamsMax;                /**< HTTP/2 maximum number of streams per connection (both peer and self initiated) */
    int      txStreamsMax;                 /**< HTTP/2 maximum number of streams the peer will permit per connection */
    int      window;                    /**< HTTP/2 Initial rx window size (size willing to receive) */
#endif
} HttpLimits;

/**
    Initialize a limits object with default values
    @param limits Limits object to modify
    @param serverSide Set to "true" for server side limits. Set to "false" for client side default limits
    @ingroup HttpLimits
    @stability Stable
 */
PUBLIC void httpInitLimits(HttpLimits *limits, bool serverSide);

/**
    Create a new limits object
    @description Create and initialize a new limits object with default values
    @param serverSide Set to "true" for server side limits. Set to "false" for client side default limits
    @return The allocated limits object
    @ingroup HttpLimits
    @stability Stable
 */
PUBLIC HttpLimits *httpCreateLimits(int serverSide);

/**
    Ease the limits
    @description This increases the receive body size, transmission body size and upload size to the maximum
        sizes supported by the system. Client side limits are eased by default.
    @param limits Limits object. This can be either HttpHost.limits HttpStream.limits or HttpEndpoint.limits
    @ingroup HttpLimits
    @stability Stable
 */
PUBLIC void httpEaseLimits(HttpLimits *limits);

/************************************* URI Services ***************************/
/**
    URI management
    @description The HTTP provides routines for formatting and parsing URIs. Routines are also provided
        to escape dangerous characters for URIs as well as HTML content and shell commands.
    @see HttpStream httpCloneUri httpCompleteUri httpCreateUri httpCreateUriFromParts httpFormatUri httpGetRelativeUri
        httpJoinUri httpJoinUriPath httpLookupMimeType httpMakeUriLocal httpNormalizeUriPath httpResolveUri
        httpUriToString
    @defgroup HttpUri HttpUri
    @stability Internal
 */
typedef struct HttpUri {
    cchar       *scheme;                /**< URI scheme (http|https|...) */
    cchar       *host;                  /**< Host name */
    cchar       *path;                  /**< Uri path (without scheme, host, query or fragements) */
    cchar       *ext;                   /**< Document extension */
    cchar       *reference;             /**< Reference fragment within the specified resource */
    cchar       *query;                 /**< Query string */
    int         port;                   /**< Port number */
    int         secure;                 /**< Using https */
    int         webSockets;             /**< Using WebSockets */
    int         valid;                  /**< Uri was successfully created */
} HttpUri;

#define HTTP_COMPLETE_URI       0x1     /**< Complete all missing URI fields. Set from "http://localhost/" */
#define HTTP_COMPLETE_URI_PATH  0x2     /**< Complete missing URI path. Set to "/" */

/**
    Clone a URI
    @description This call copies the base URI and optionally completes missing fields in the URI
    @param base Base URI to copy
    @param flags Set to HTTP_COMPLETE_URI to add missing components. ie. Add scheme, host and port if not supplied.
    @return A new URI object
    @ingroup HttpUri
    @stability Stable
 */
PUBLIC HttpUri *httpCloneUri(HttpUri *base, int flags);

/**
    Complete the given URI
    @description Complete the URI supplying missing URI components from the other URI. This modifies the supplied URI and
        does not allocate or create a new URI.
    @param uri URI to complete
    @param other Other URI to supply the missing components
    @return The supplied URI.
    @ingroup HttpUri
    @stability Stable
  */
PUBLIC HttpUri *httpCompleteUri(HttpUri *uri, HttpUri *other);

/**
    Create and initialize a URI.
    @description Parse a uri and return a tokenized HttpUri structure.
    @param uri Uri string to parse
    @param flags Set to HTTP_COMPLETE_URI to add missing components. ie. Add scheme, host and port if not supplied.
    @return A newly allocated HttpUri structure.
    @ingroup HttpUri
    @stability Stable
 */
PUBLIC HttpUri *httpCreateUri(cchar *uri, int flags);

/**
    Create a URI from parts
    @description This call constructs a URI from the given parts. Various URI parts can be omitted by setting to null.
        The URI path is the only mandatory parameter.
    @param scheme The URI scheme. This is typically "http" or "https".
    @param host The URI host name portion. This can be a textual host and domain name or it can be an IP address.
    @param port The URI port number. Set to zero to accept the default value for the selected scheme.
    @param path The URI path to the requested document.
    @param reference URI reference with an HTML document. This is the URI component after the "#" in the URI path.
    @param query URI query component. This is the URI component after the "?" in the URI.
    @param flags Set to HTTP_COMPLETE_URI to add missing components. ie. Add scheme, host and port if not supplied.
    @return A new URI
    @ingroup HttpUri
    @stability Stable
 */
PUBLIC HttpUri *httpCreateUriFromParts(cchar *scheme, cchar *host, int port, cchar *path, cchar *reference,
        cchar *query, int flags);

/**
    Format a URI
    @description Format a URI string using the input components.
    @param scheme Protocol string for the uri. Example: "http"
    @param host Host or IP address
    @param port TCP/IP port number
    @param path URL path
    @param ref URL reference fragment
    @param query Additiona query parameters.
    @param flags Set to HTTP_COMPLETE_URI to add missing components. ie. Add scheme, host and port if not supplied.
    @return A newly allocated uri string
    @ingroup HttpUri
    @stability Stable
 */
PUBLIC char *httpFormatUri(cchar *scheme, cchar *host, int port, cchar *path, cchar *ref, cchar *query, int flags);

/**
    Join URIs
    @param base Base URI to being with
    @param argc Count of URIs in others
    @param others Array of URIs to join to the base
    @return The resulting, joined URI
    @ingroup HttpUri
    @stability Stable
 */
PUBLIC HttpUri *httpJoinUri(HttpUri *base, int argc, HttpUri **others);

/**
    Join a URI path
    @param result URI that will be modified with a joined path
    @param base URI supplying the base path
    @param other Other URI whose path is joined to the base
    @return The result URI
    @ingroup HttpUri
    @stability Stable
 */
PUBLIC HttpUri *httpJoinUriPath(HttpUri *result, HttpUri *base, HttpUri *other);

/**
    Get the mime type for an extension.
    This call will return the mime type from a limited internal set of mime types for the given path or extension.
    @param ext Path or extension to examine
    @returns Mime type. This is a static string.
    @ingroup HttpUri
    @stability Stable
 */
PUBLIC cchar *httpLookupMimeType(cchar *ext);

/**
    Normalize a URI
    @description Validate and canonicalize a URI. This invokes httpNormalizeUriPath to normalize the URI path.
        This removes redundant ./ and ../ segments including leading ../ segments. It does not make the URI absolute.
    @param uri URI object to normalize
    @return The supplied uri so it can be used in chaining. Returns null if the URI cannot be normalized.
    @ingroup HttpUri
    @stability Stable
 */
PUBLIC HttpUri *httpNormalizeUri(HttpUri *uri);

/**
    Normalize a URI
    @description Validate and canonicalize a URI path. This removes redundant "./" and "../dir"
        sequences including leading "../" segments.
    @param uri Uri path string to normalize. This is the URI path portion without scheme, host and port components.
    @return A new validated uri string. Returns null if the URI cannot be normalized.
    @ingroup HttpUri
    @stability Stable
 */
PUBLIC char *httpNormalizeUriPath(cchar *uri);

/**
    Get a relative URI from the base to the target
    @description This creates a URI relative from the base to the target. This may contain ".." segments. This API is
        designed to create relative URIs for use in a browser web page.
        \n\n
        If the target is null, an absolute URI, or if a relative URI from the base cannot be constructed, then
        the target will be returned. If clone is true, then a clone of the target will be returned.
    @param base The base URI considered to be the current URI. Think of this as the current directory.
    @param target The destination URI for which a relative URI will be crafted to reach.
    @param clone If true, the target URI will be cloned if the target is an absolute URI or if a relative URI
        cannot be constructed.
    @ingroup HttpUri
    @stability Stable
  */
PUBLIC HttpUri *httpGetRelativeUri(HttpUri *base, HttpUri *target, int clone);

/**
    Make a URI local
    @description This routine removes the scheme, host and port portions of a URI
    @param uri URI to modify
    @return The given URI.
    @ingroup HttpUri
    @stability Stable
 */
PUBLIC HttpUri *httpMakeUriLocal(HttpUri *uri);

/**
    Resolve URIs relative to a base
    @param [in] stream HttpStream stream object
    @param base Base URI to begin with
    @param target URI to resolve relative to the base
    @ingroup HttpUri
    @stability Stable
  */
PUBLIC HttpUri *httpResolveUri(struct HttpStream *stream, HttpUri *base, HttpUri *target);

/**
    Create a URI link
    @description Create a URI link based on a given target relative to the current request.
        This API expands embedded tokens based on the current request and route state. The target URI parameter
        may contain partial or complete URI information. The missing parts are supplied using the current request
        and route tables.
    @param [in] stream HttpStream stream object
    @param target The URI target. The target parameter can be a URI string or JSON style set of options.
        The target will have any embedded "{tokens}" expanded by using token values from the request parameters.
        If the target has an absolute URI path, that path is used directly after tokenization. If the target begins with
        "~", that character will be replaced with the route prefix. This is a very convenient way to create application
        top-level relative links.
        \n\n
        If the target is a string that begins with "{AT}" it will be interpreted as a service/action pair of the
        form "{AT}Service/action". If the "service/" portion is absent, the current service is used. If
        the action component is missing, the "list" action is used. A bare "{AT}" refers to the "list" action
        of the current service.
        \n\n
        If the target starts with "{" it is interpreted as being a JSON style set of options that describe the link.
        If the target is a relative URI path, it is appended to the current request URI path.
        \n\n
        If the is a JSON style of options, it can specify the URI components: scheme, host, port, path, reference and
        query. If these component properties are supplied, these will be combined to create a URI.
        \n\n
        If the target specifies either a service/action or a JSON set of options, The URI will be created according
        to the route URI template. The template may be explicitly specified
        via a "route" target property. Otherwise, if an "action" property is specified, the route of the same
        name will be used. If these don't result in a usable route, the "default" route will be used.
        \n\n
        These are the properties supported in a JSON style "{ ... }" target:
        <ul>
            <li>scheme String URI scheme portion</li>
            <li>host String URI host portion</li>
            <li>port Number URI port number</li>
            <li>path String URI path portion</li>
            <li>reference String URI path reference. Does not include "#"</li>
            <li>query String URI query parameters. Does not include "?"</li>
            <li>service String Service name if using a Service-based route. This can also be specified via
                the action option.</li>
            <li>action String Action to invoke. This can be a URI string or a Service action of the form
                {AT}Service/action.</li>
            <li>route String Route name to use for the URI template</li>
        </ul>
    @return A normalized Uri string.
    @ingroup HttpUri
    @stability Evolving
    @remarks Examples:<pre>
    httpLink(stream, "http://example.com/index.html");
    httpLink(stream, "/path/to/index.html");
    httpLink(stream, "../images/splash.png");
    httpLink(stream, "~/static/images/splash.png");
    httpLink(stream, "${app}/static/images/splash.png");
    httpLink(stream, "@service/checkout");
    httpLink(stream, "@service/")               //  Service = Service, action = index
    httpLink(stream, "@init")                   //  Current service, action = init
    httpLink(stream, "@")                       //  Current service, action = index
    httpLink(stream, "{ action: '@post/create' }");
    httpLink(stream, "{ action: 'checkout' }");
    httpLink(stream, "{ action: 'logout', service: 'admin' }");
    httpLink(stream, "{ action: 'admin/logout'");
    httpLink(stream, "{ product: 'candy', quantity: '10', template: '/cart/${product}/${quantity}' }");
    httpLink(stream, "{ route: '~/STAR/edit', action: 'checkout', id: '99' }");
    httpLink(stream, "{ template: '~/static/images/${theme}/background.jpg', theme: 'blue' }");
</pre>
 */
PUBLIC char *httpLink(struct HttpStream *stream, cchar *target);

/**
    Create an absolute link that includes scheme and host
    @param stream HttpStream stream object
    @param target
    @param target The URI target. See #httpLink for details of the target parameter.
    @return A normalized Uri string.
    @ingroup HttpUri
    @stability Prototype
 */
PUBLIC char *httpLinkAbs(struct HttpStream *stream, cchar *target);

/**
    Extended URI link creation.
    @description Extended httpLink with custom options. This routine extends the #httpLink API with an options hash
        of token values.
    @param [in] stream HttpStream stream object
    @param target The URI target. See #httpLink for details.
    @param options Hash of option values for embedded tokens. This hash is blended with the route variables.
    @return A normalized Uri string.
    @ingroup HttpUri
    @stability Evolving
 */
PUBLIC char *httpLinkEx(struct HttpStream *stream, cchar *target, MprHash *options);

/*
    Create a URI link and return a URI object.
    @param [in] stream HttpStream stream object
    @param target The URI target. See #httpLink for details.
    @param options Hash of option values for embedded tokens. This hash is blended with the route variables.
    @return A normalized Uri string.
    @ingroup HttpUri
    @stability Evolving
 */
PUBLIC HttpUri *httpLinkUri(struct HttpStream *stream, cchar *target, MprHash *options);

/**
    Convert a Uri to a string.
    @description Convert the given Uri to a string, optionally completing missing parts such as the host, port and path.
    @param uri A Uri object created via httpCreateUri
    @param flags Set to HTTP_COMPLETE_URI to add missing components. ie. Add scheme, host and port if not supplied.
    @return A newly allocated uri string.
    @ingroup HttpUri
    @stability Stable
 */
PUBLIC char *httpUriToString(HttpUri *uri, int flags);

/**
    Validate a URI path as expected in a HTTP request line
    @description This expects a URI beginning with "/" and containing only valid URI characters.
    The URI is decoded, and normalized removing "../" and "." segments.
    The URI must begin with a "/" both before and after decoding and normalization.
    @param uri URI to validate.
    @return A validated, normalized URI path
    @stability Evolving
    @ingroup HttpUri
 */
PUBLIC char *httpValidateUriPath(cchar *uri);

/**
    Test if a URI is using only valid characters
    Note this does not test if the URI is fully legal. Some components of the URI have restricted character sets
    that this routine does not test. This tests if the URI has only characters valid to use in a URI before decoding.
    i.e. It will permit %NN encodings. The set of valid characters is:
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~:/?#[]@!$&'()*+,;=%"
    @param uri Uri to test
    @return True if the URI string is comprised of legal URI characters.
    @ingroup HttpUri
  */
PUBLIC bool httpValidUriChars(cchar *uri);

/************************************* Range **********************************/
/**
    Content range structure
    @pre
        Range:  0,  49  First 50 bytes
        Range: -1, -50  Last 50 bytes
        Range:  1,  -1  Skip first byte then select content to the end
    @defgroup HttpRange HttpRange
    @see HttpRange
    @stability Internal
 */
typedef struct HttpRange {
    MprOff              start;                  /**< Start of range */
    MprOff              end;                    /**< End byte of range + 1 */
    MprOff              len;                    /**< Redundant range length */
    struct HttpRange    *next;                  /**< Next range */
} HttpRange;

/************************************* Packet *********************************/
/*
    Packet flags
 */
#define HTTP_PACKET_HEADER      0x1               /**< Packet contains HTTP headers */
#define HTTP_PACKET_RANGE       0x2               /**< Packet is a range boundary packet */
#define HTTP_PACKET_DATA        0x4               /**< Packet contains actual content data */
#define HTTP_PACKET_END         0x8               /**< End of stream packet */
#define HTTP_PACKET_SOLO        0x10              /**< Don't join this packet */

/**
    Callback procedure to fill a packet with data
    @param q Queue owning the packet
    @param packet The packet to fill
    @param off Offset in the packet to fill with data
    @param size Size of packet from the offset to fill.
    @return The number of bytes copied into the packet.
    @ingroup HttpPacket
    @stability Stable
 */
typedef ssize (*HttpFillProc)(struct HttpQueue *q, struct HttpPacket *packet, MprOff pos, ssize size);

/**
    Packet object.
    @description The request/response pipeline sends data and control information in HttpPacket objects. The output
        stream typically consists of a HEADER packet followed by zero or more data packets and terminated by an END
        packet. If the request has input data, the input stream consists of one or more data packets followed by
        an END packet.
        \n\n
        Packets contain data and optional prefix or suffix headers. Packets can be split, joined, filled, or emptied.
        The pipeline stages will fill or transform packet data as required.
    @defgroup HttpPacket HttpPacket
    @see HttpFillProc HttpPacket HttpQueue httpAdjustPacketEnd httpAdjustPacketStart httpClonePacket
        httpCreateDataPacket httpCreateEndPacket httpCreateEntityPacket httpCreateHeaderPacket httpCreatePacket
        httpGetPacket httpGetPacketLength httpIsLastPacket httpJoinPacket
        httpPutBackPacket httpPutForService httpPutPacket httpPutPacketToNext httpSplitPacket
    @stability Internal
 */
typedef struct HttpPacket {
    uint            flags: 7;               /**< Packet flags */
    uint            last: 1;                /**< Last packet in a message - used by web sockets */
    uint            type: 24;               /**< Packet type extension */
    struct HttpPacket *next;                /**< Next packet in chain */
    MprBuf          *content;               /**< Chunk content */
    MprBuf          *prefix;                /**< Prefix message to be emitted before the content */
    MprOff          esize;                  /**< Data size in entity (file) */
    MprOff          epos;                   /**< Data position in entity (file) */
    HttpFillProc    fill;                   /**< Callback to fill packet with data */
    struct HttpStream *stream;              /**< Reference to owning stream */
    void            *data;                  /**< Managed data reference */
} HttpPacket;

/**
    Adjust the packet starting position.
    @description This adjusts the packet content by the given size. The packet position is incremented by start and the
    packet length (size) is decremented. If the packet describes entity data, the given size amount to the Packet.epos and
    decrements the Packet.esize fields. If the packet has actual data buffered in Packet.content, the content buffer
    start is incremeneted by the size amount.
    @param packet Packet to modify
    @param size Size to add to the packet current position.
    @ingroup HttpPacket
    @stability Stable
 */
PUBLIC void httpAdjustPacketStart(HttpPacket *packet, MprOff size);

/**
    Adjust the packet end position.
    @description This adjusts the packet content by the given size. The packet length (size) is decremented by the requested
    amount. If the packet describes entity data, the Packet.esize field is reduced by the requested size amount. If the
    packet has actual data buffered in Packet.content, the content buffer end position is reduced by
    by the size amount.
    @param packet Packet to modify
    @param size Size to adjust packet end position.
    @ingroup HttpPacket
    @stability Stable
 */
PUBLIC void httpAdjustPacketEnd(HttpPacket *packet, MprOff size);

/**
    Clone a packet
    @param orig Original packet to clone
    @return A new packet equivalent to the original
    @ingroup HttpPacket
    @stability Stable
 */
PUBLIC HttpPacket *httpClonePacket(HttpPacket *orig);

/**
    Create a data packet
    @description Create a packet and set the HTTP_PACKET_DATA flag
        Data packets convey data through the response pipeline.
    @param size Size of the package data storage.
    @return HttpPacket object.
    @ingroup HttpPacket
    @stability Stable
 */
PUBLIC HttpPacket *httpCreateDataPacket(ssize size);

/**
    Create an end-of-stream packet
    @description Create an end-of-stream packet and set the HTTP_PACKET_END flag. The end pack signifies the
        end of data. It is used on both incoming and outgoing streams through the request/response pipeline.
    @return HttpPacket object.
    @ingroup HttpPacket
    @stability Stable
 */
PUBLIC HttpPacket *httpCreateEndPacket(void);

/**
    Create an entity data packet
    @description Create an entity packet and set the HTTP_PACKET_DATA flag.
        Entity packets describe the resource (entity) to send to the client and provide a #HttpFillProc procedure
        used to fill packets with data from the entity.
    @param pos Position within the entity for packet data
    @param size Size of the entity data
    @param fill HttpFillProc callback to supply the entity data.
    @return HttpPacket object.
    @ingroup HttpPacket
    @stability Stable
 */
PUBLIC HttpPacket *httpCreateEntityPacket(MprOff pos, MprOff size, HttpFillProc fill);

/**
    Create a response header packet
    @description Create a response header packet and set the HTTP_PACKET_HEADER flag.
        A header packet is used by the pipeline to hold the response headers.
    @return HttpPacket object.
    @ingroup HttpPacket
    @stability Stable
 */
PUBLIC HttpPacket *httpCreateHeaderPacket(void);

/**
    Create a data packet
    @description Create a packet of the required size.
    @param size Size of the package data storage.
    @return HttpPacket object.
    @ingroup HttpPacket
    @stability Stable
 */
PUBLIC HttpPacket *httpCreatePacket(ssize size);

/**
    Get the next packet from a queue
    @description Get the next packet. This will remove the packet from the queue and adjust the queue counts
        accordingly. If the queue is full and upstream queues are blocked, they will be enabled.
    @param q Queue reference
    @return The packet removed from the queue.
    @ingroup HttpQueue
    @stability Stable
 */
PUBLIC HttpPacket *httpGetPacket(struct HttpQueue *q);

#if DOXYGEN
/**
    Get the packet data contents.
    @description Get the packet content reference. This is an MprBuf object.
    @param packet Packet to examine.
    @return MprBuf reference or zero if there are not contents.
    @ingroup HttpPacket
    @stability Stable
 */
PUBLIC ssize httpGetPacketContents(HttpPacket *packet);
#else
    #define httpGetPacketContents(p) ((p && p->content) ? p->content : 0)
#endif

#if DOXYGEN
/**
    Get the length of the packet data contents.
    @description Get the content length of a packet. This does not include the prefix or virtual data length -- just
    the pure buffered data contents.
    @param packet Packet to examine.
    @return Count of bytes contained by the packet.
    @ingroup HttpPacket
    @stability Stable
 */
PUBLIC ssize httpGetPacketLength(HttpPacket *packet);
#else
    #define httpGetPacketLength(p) ((p && p->content) ? mprGetBufLength(p->content) : 0)
#endif

/**
    Get the start of the packet data contents.
    @param packet Packet to examine.
    @return A reference to the start of the packet contents.
    @ingroup HttpPacket
    @stability Stable
 */
PUBLIC char *httpGetPacketStart(HttpPacket *packet);

/**
    Get the packet data contents as a string.
    @description Get the packet content reference. The packet contents will be null terminated.
    @param packet Packet to examine.
    @return A reference to the start of the packet contents.
    @ingroup HttpPacket
    @stability Stable
 */
PUBLIC char *httpGetPacketString(HttpPacket *packet);

/**
    Test if the packet is the last in a logical message.
    @description Useful for WebSockets to test if the packet is the last frame in a message
    @param packet Packet to examine
    @return True if the packet is the last in a message.
    @ingroup HttpPacket
    @stability Stable
 */
PUBLIC bool httpIsLastPacket(HttpPacket *packet);

/**
    Join two packets
    @description Join the contents of one packet to another by copying the data from the \a other packet into
        the first packet.
    @param packet Destination packet
    @param other Other packet to copy data from.
    @return "Zero" if successful, otherwise a negative Mpr error code
    @ingroup HttpPacket
    @stability Stable
 */
PUBLIC int httpJoinPacket(HttpPacket *packet, HttpPacket *other);

/**
    Split a data packet
    @description Split a data packet at the specified offset. Packets may need to be split so that downstream
        stages can digest their contents. If a packet is too large for the queue maximum size, it should be split.
        When the packet is split, a new packet is created containing the data after the offset. Any suffix headers
        are moved to the new packet.
        NOTE: when splitting packets, the HttpPacket.content reference may be modified.
    @param packet Packet to split
    @param offset Location in the original packet at which to split. This is an offset relative to the current
        'start' read position not the beginning of the packet content buffer.
    @return New HttpPacket object containing the data after the offset. No need to free, unless you have a very long
        running request. Otherwise the packet memory will be released automatically when the request completes.
    @ingroup HttpPacket
    @stability Stable
 */
PUBLIC HttpPacket *httpSplitPacket(HttpPacket *packet, ssize offset);

/*
    Internal
 */
#define httpGetPacketEntityLength(p) (p->content ? mprGetBufLength(p->content) : packet->esize)

/************************************* Queue *********************************/
/*
    Queue directions
 */
#define HTTP_QUEUE_TX             0         /**< Send (transmit to client) queue */
#define HTTP_QUEUE_RX             1         /**< Receive (read from client) queue */
#define HTTP_MAX_QUEUE            2         /**< Number of queue types */

/*
   Queue flags
 */
#define HTTP_QUEUE_OPEN_TRIED     0x1       /**< Queue's open routine has been called */
#define HTTP_QUEUE_OPENED         0x2       /**< Queue's open routine has been called */
#define HTTP_QUEUE_SUSPENDED      0x4       /**< Queue's service routine is suspended due to flow control */
#define HTTP_QUEUE_ALL            0x8       /**< Queue has all the data there is and will be */
#define HTTP_QUEUE_SERVICED       0x10      /**< Queue has been serviced at least once */
#define HTTP_QUEUE_EOF            0x20      /**< Queue at end of data */
#define HTTP_QUEUE_STARTED        0x40      /**< Handler stage start routine called */
#define HTTP_QUEUE_READY          0x80      /**< Handler stage ready routine called */
#define HTTP_QUEUE_RESERVICE      0x100     /**< Queue requires reservicing */
#define HTTP_QUEUE_OUTGOING       0x200     /**< Queue is for outgoing traffic */
#define HTTP_QUEUE_REQUEST        0x400     /**< Queue is specific for this request */

/*
    Queue callback prototypes
 */
typedef int  (*HttpQueueOpen)(struct HttpQueue *q);
typedef void (*HttpQueueClose)(struct HttpQueue *q);
typedef void (*HttpQueueStart)(struct HttpQueue *q);
typedef void (*HttpQueueData)(struct HttpQueue *q, HttpPacket *packet);
typedef void (*HttpQueueService)(struct HttpQueue *q);

/**
    Queue object
    @description The request pipeline consists of a full-duplex pipeline of stages. Each stage has two queues,
        one for outgoing data and one for incoming. A HttpQueue object manages the data flow for a request stage
        and has the ability to queue and process data, manage flow control, and schedule packets for service.
        \n\n
        Queue's provide open, close, put, and service methods. These methods manage and respond to incoming packets.
        A queue can respond immediately to an incoming packet by processing or dispatching a packet in its put() method.
        Alternatively, the queue can defer processing by queueing the packet on it's service queue and then waiting for
        it's service() method to be invoked.
        \n\n
        If a queue does not define a put() method, the default put() method will
        be used which queues data onto the service queue. The default incoming put() method joins incoming packets
        into a single packet on the service queue.
        \n\n
        Data flows downstream from one queue to the next queue linked via the nextQ field.
    @defgroup HttpQueue HttpQueue
    @see HttpStream HttpPacket HttpQueue httpDiscardQueueData httpFlushQueue httpGetQueueRoom
        httpIsEof httpIsPacketTooBig httpIsQueueEmpty httpIsQueueSuspended httpJoinPacketForService httpJoinPackets
        httpPutBackPacket httpPutForService httpPutPacket httpPutPacketToNext httpRemoveQueue httpResizePacket
        httpResumeQueue httpScheduleQueue httpSetQueueLimits httpSuspendQueue
        httpWillQueueAcceptPacket httpWillNextQueueAcceptSize httpWrite httpWriteBlock httpWriteBody httpWriteString
    @stability Internal
 */
typedef struct HttpQueue {
    /* Ordered for debugging */
    cchar               *name;                  /**< Queue name for debugging */
    ssize               count;                  /**< Bytes in queue (Does not include virt packet data) */
    int                 flags;                  /**< Queue flags */
    struct HttpQueue    *nextQ;                 /**< Downstream queue for next stage */
    struct HttpQueue    *prevQ;                 /**< Upstream queue for prior stage */
    HttpPacket          *first;                 /**< First packet in queue (singly linked) */
    HttpPacket          *last;                  /**< Last packet in queue (tail pointer) */

    struct HttpStream   *stream;                /**< Stream owning this queue may be null */
    struct HttpNet      *net;                   /**< Network connection owning this queue */
    struct HttpStage    *stage;                 /**< Stage owning this queue */
    HttpQueueOpen       open;                   /**< Open the queue */
    HttpQueueClose      close;                  /**< Close the queue */
    HttpQueueStart      start;                  /**< Start the queue */
    HttpQueueData       put;                    /**< Callback to receive a packet */
    HttpQueueService    service;                /**< Service the queue */
    struct HttpQueue    *scheduleNext;          /**< Next linkage when queue is on the service queue */
    struct HttpQueue    *schedulePrev;          /**< Previous linkage when queue is on the service queue */
    struct HttpQueue    *pair;                  /**< Queue for the same stage in the opposite direction */
    void                *queueData;             /**< Stage instance data - must be a managed reference */
    void                *staticData;            /**< Stage instance data - must be an unmanaged reference */

    ssize               window;                 /**< HTTP/2 flow control window size */
    ssize               max;                    /**< Advisory maxiumum queue size */
    ssize               low;                    /**< Low water mark for flow control */
    ssize               packetSize;             /**< Maximum acceptable packet size */
    int                 servicing;              /**< Currently being serviced */
    int                 direction;              /**< Flow direction */

    /*
        Connector instance data
     */
    MprIOVec            iovec[ME_MAX_IOVEC];
    int                 ioIndex;                /**< Next index into iovec */
    int                 ioFile;                 /**< Sending a file */
    MprOff              ioCount;                /**< Count of bytes in iovec including file I/O */
    MprOff              ioPos;                  /**< Position in file */
} HttpQueue;

/**
    Discard all data from the queue
    @description Discard data from the queue. If removePackets (not yet implemented) is "true", then remove the packets.
        Oherwise, just discard the data and preserve the packets.
    @param q Queue reference
    @param removePackets If "true", the data packets will be removed from the queue.
    @ingroup HttpQueue
    @stability Stable
 */
PUBLIC void httpDiscardQueueData(HttpQueue *q, bool removePackets);

/**
    Flush queue data
    @description This initiates writing buffered data (flushes) by scheduling the queue and servicing the queues.
    \n\n
    If blocking mode is selected, all queues will be immediately serviced and the call may block while output drains.
    If non-blocking, the queues will be serviced but the call will not block nor yield.
    In blocking mode, this routine may invoke mprYield before it blocks to consent for the garbage collector to trun. Callers must
    ensure they have retained all required temporary memory before invoking this routine.
    \n\n
    This routine when used with HTTP_BLOCK should never be used in filters, connectors or by handlers outside their
    open, close, ready, start and writable callbacks.
    @param q Queue to flush
    @param flags If set to HTTP_BLOCK, this call will block until the data has drained through the network connector.
    @return "True" if there is room for more data in the queue after flushing.
    @ingroup HttpQueue
    @stability Stable
 */
PUBLIC bool httpFlushQueue(HttpQueue *q, int flags);

/**
    Get the room in the queue
    @description Get the amount of data the queue can accept before being full.
    @param q Queue reference
    @return A count of bytes that can be written to the queue
    @ingroup HttpQueue
    @stability Stable
 */
PUBLIC ssize httpGetQueueRoom(HttpQueue *q);

/**
    Test if the connection has received all incoming content
    @description This tests if the connection is at an "End of File condition.
    @param stream HttpStream object created via #httpCreateStream
    @return "True" if all Receive content has been received
    @ingroup HttpQueue
    @stability Stable
 */
PUBLIC bool httpIsEof(struct HttpStream *stream);

/**
    Test if a packet is too big
    @description Test if a packet is too big to fit downstream. If the packet content exceeds the downstream queue's
        maximum or exceeds the downstream queue's requested packet size -- then this routine will return "true".
    @param q Queue reference
    @param packet Packet to test
    @return "True" if the packet is too big for the downstream queue
    @ingroup HttpQueue
    @stability Stable
 */
PUBLIC bool httpIsPacketTooBig(struct HttpQueue *q, HttpPacket *packet);

/**
    Determine if the queue is empty
    @description Determine if the queue has no packets queued. This does not test if the queue has no data content.
    @param q Queue reference
    @return "True" if there are no packets queued.
    @ingroup HttpQueue
    @stability Stable
 */
PUBLIC bool httpIsQueueEmpty(HttpQueue *q);

/**
    Test if a queue is suspended.
    @param q Queue reference
    @return true if the queue is suspended.
    @ingroup HttpQueue
    @stability Stable
 */
PUBLIC bool httpIsQueueSuspended(HttpQueue *q);

/**
    Join packets together
    @description This call joins data packets on the given queue into a single packet. The given size specifies the
    maximum size of data to be joined. The maximum size may also limited by the downstream queue maximum packet size.
    @param q Queue to examine
    @param size The maximum-sized packet that will be created by joining queue packets is the minimum of the given size
        and the downstream queues maximum packet size. Note: this routine will not split packets and so the
        maximum is advisory only.
    @ingroup HttpQueue
    @stability Stable
 */
PUBLIC void httpJoinPackets(HttpQueue *q, ssize size);

/**
    Join a packet onto the service queue
    @description Add a packet to the service queue. If the queue already has data, then this packet
        will be joined (aggregated) into the existing packet. If serviceQ is true, the queue will be scheduled
        for service.
    @param q Queue reference
    @param packet Packet to join to the queue
    @param serviceQ If true, schedule the queue for service
    @ingroup HttpQueue
    @stability Stable
 */
PUBLIC void httpJoinPacketForService(struct HttpQueue *q, HttpPacket *packet, bool serviceQ);

/**
    Put a packet back onto a queue
    @description Put the packet back onto the front of the queue. The queue's put() method is not called.
        This is typically used by the queue's service routine when a packet cannot complete processing.
    @param q Queue reference
    @param packet Packet to put back
    @ingroup HttpQueue
    @stability Stable
 */
PUBLIC void httpPutBackPacket(struct HttpQueue *q, HttpPacket *packet);

/*
    Convenience flags for httpPutForService in the serviceQ argument
 */
#define HTTP_DELAY_SERVICE      0           /**< Delay servicing the queue */
#define HTTP_SCHEDULE_QUEUE     1           /**< Schedule the queue for service */

/**
    Put a packet into the service queue for deferred processing.
    @description Add a packet to the service queue. If serviceQ is true, the queue will be scheduled for service.
    @param q Queue reference
    @param packet Packet to join to the queue
    @param serviceQ If true, schedule the queue for service
    @ingroup HttpQueue
    @stability Stable
 */
PUBLIC void httpPutForService(struct HttpQueue *q, HttpPacket *packet, bool serviceQ);

/**
    Put a packet to the queue.
    @description The packet is passed to the queue by invoking its put() callback.
        Note the receiving queue may immediately process the packet or it may choose to defer processing by putting to
        its service queue.  @param q Queue reference
    \n\n
    Note: the garbage collector may run while calling httpSendBlock to reclaim unused packets. It is essential that all
        required memory be retained by a relevant manager calling mprMark as required.

    @param packet Packet to put
    @ingroup HttpQueue
    @stability Stable
 */
PUBLIC void httpPutPacket(struct HttpQueue *q, HttpPacket *packet);

/**
    Put a packet to the next queue downstream.
    @description Put a packet onto the next downstream queue by calling the downstream queue's put() method.
        Note the receiving queue may immediately process the packet or it may choose to defer processing by putting to
        its service queue.  @param q Queue reference
    \n\n
    Note: the garbage collector may run while calling httpSendBlock to reclaim unused packets. It is essential that all
        required memory be retained by a relevant manager calling mprMark as required.

    @param q Queue reference. The packet will not be queued on this queue, but rather on the queue downstream.
    @param packet Packet to put
    @ingroup HttpQueue
    @stability Stable
 */
PUBLIC void httpPutPacketToNext(struct HttpQueue *q, HttpPacket *packet);

/**
    Remove a queue
    @description Remove a queue from the request/response pipeline. This will remove a queue so that it does
        not participate in the pipeline, effectively removing the processing stage from the pipeline. This is
        useful to remove unwanted filters and to speed up pipeline processing
    @param q Queue reference
    @ingroup HttpQueue
    @stability Stable
 */
PUBLIC void httpRemoveQueue(HttpQueue *q);

/**
    Resize a packet
    @description Resize a packet, if required, so that it fits in the downstream queue. This may split the packet
        if it is too big to fit in the downstream queue. If it is split, the tail portion is put back on the queue.
    @param q Queue reference. The q->nextQ will be examined to see if the packet will fit.
    @param packet Packet to put
    @param size If size is > 0, then also ensure the packet is not larger than this size.
    @return Zero if the packet is not resized. Otherwise return the tail packet that was put back onto the queue.
    @ingroup HttpQueue
    @stability Stable
 */
PUBLIC HttpPacket *httpResizePacket(struct HttpQueue *q, HttpPacket *packet, ssize size);

/**
    Resume a queue
    @description Resume a queue for service and schedule it to run. This will cause the service routine
        to run as soon as possible. This is normally called automatically called by the pipeline when downstream
        congestion has cleared.
    @param q Queue reference
    @ingroup HttpQueue
    @stability Stable
 */
PUBLIC void httpResumeQueue(HttpQueue *q);

/**
    Schedule a queue
    @description Schedule a queue by adding it to the schedule queue. Queues are serviced FIFO.
    @param q Queue reference
    @ingroup HttpQueue
    @stability Stable
 */
PUBLIC void httpScheduleQueue(HttpQueue *q);

#if UNUSED
/**
    Service a queue
    @description Service a queue by invoking its service() routine.
    @param q Queue reference
    @ingroup HttpQueue
    @stability Stable
 */
PUBLIC void httpServiceQueue(HttpQueue *q);
#endif

/**
    Set a queue's max packetSize and flow control low, max and window thresholds
    @description If size parameters are set to -1, default values from the limits are used.
    @param q Queue reference
    @param limits Default limits to use if other arguments are not provided.
    @param packetSize The default maximum packet size.
    @param low The low water mark. Typically set to packet size by default.
    @param max The high water mark. Set by default to packetSize * 4.
    @param window HTTP/2 flow control window size. Must be at least HTTP_DEFAULT_WINDOW_SIZE.
    @ingroup HttpQueue
    @stability Evolving
 */
PUBLIC void httpSetQueueLimits(HttpQueue *q, HttpLimits *limits, ssize packetSize, ssize low, ssize max, ssize window);

/**
    Suspend a queue.
    @description Suspended a queue so that it will not be scheduled for service. The pipeline will
    will automatically call httpResumeQueue when the downstream queues are less congested.
    @param q Queue reference
    @ingroup HttpQueue
    @stability Stable
 */
PUBLIC void httpSuspendQueue(HttpQueue *q);

#if ME_DEBUG
/**
    Verify a queue
    @param q Queue reference
    @return "True" if the queue verifies
    @internal
 */
PUBLIC bool httpVerifyQueue(HttpQueue *q);
#define VERIFY_QUEUE(q) httpVerifyQueue(q)
#else
#define VERIFY_QUEUE(q)
#endif

/**
    Determine if the downstream queue will accept this packet.
    @description Test if the downstream queue will accept a packet. The packet will be resized, if required, in an
        attempt to get the downstream queue to accept it. If the downstream queue is full, disable this queue
        and mark the downstream queue as full, and service it immediately to try to relieve the congestion.
    @param q Queue reference
    @param packet Packet to put
    @return "True" if the downstream queue will accept the packet. Use #httpPutPacketToNext to send the
        packet downstream
    @ingroup HttpQueue
    @stability Stable
 */
PUBLIC bool httpWillNextQueueAcceptPacket(HttpQueue *q, HttpPacket *packet);

/**
    Test if the next queue is full
    @description Tests if the next queue count is over the queue maximum
    @param q Queue reference
    @return "True" if the next q->count > q->max
    @ingroup HttpQueue
    @stability Stable
 */
PUBLIC bool httpNextQueueFull(HttpQueue *q);

/**
    Determine if the given queue will accept this packet.
    @description Test if the queue will accept a packet. The packet will be resized, if split is true, in an
        attempt to get the downstream queue to accept it.
    @param q Queue reference
    @param nextQ Next (downstream) queue reference
    @param packet Packet to put
    @return "True" if the queue will accept the packet.
    @ingroup HttpQueue
    @stability Stable
 */
PUBLIC bool httpWillQueueAcceptPacket(HttpQueue *q, HttpQueue *nextQ, HttpPacket *packet);

/**
    Determine if the downstream queue will accept a certain amount of data.
    @description Test if the downstream queue will accept data of a given size.
    @param q Queue reference
    @param size Size of data to test for
    @return "True" if the downstream queue will accept the given sized data.
    @ingroup HttpQueue
    @stability Stable
 */
PUBLIC bool httpWillNextQueueAcceptSize(HttpQueue *q, ssize size);

/**
    Write a formatted string
    @description Write a formatted string of data into packets onto the end of the queue. Data packets will be created
        as required to store the write data. This call always accepts all the data and will buffer as required.
        This call may block waiting for the downstream queue to drain if it is or becomes full.
        Data written after #httpFinalizeOutput or #httpError is called will be ignored.
        \n\n
        Handlers may only call httpWrite in their open, close, ready, start and writable callbacks as these are the only
        callbacks permitted to block. If a handler
        needs to write in other callbacks, it should use #httpWriteBlock and use the HTTP_NON_BLOCK or HTTP_BUFFER flags.
        \n\n
        Filters and connectors must never call httpWrite as it may block.
    @param q Queue reference
    @param fmt Printf style formatted string
    @param ... Arguments for fmt
    @return A count of the bytes actually written
    @ingroup HttpQueue
    @stability Stable
 */
PUBLIC ssize httpWrite(HttpQueue *q, cchar *fmt, ...) PRINTF_ATTRIBUTE(2,3);

/*
    Set HTTP_BLOCK to 0x1 so that legacy calls to httpFlushQueue that supplied a boolean block value will function correctly
 */
#define HTTP_BLOCK      0x1    /**< Flag for httpSendBlock and httpWriteBlock to indicate blocking operation */
#define HTTP_NON_BLOCK  0x2    /**< Flag for httpSendBlock and httpWriteBlock to indicate non-blocking operation */
#define HTTP_BUFFER     0x4    /**< Flag for httpSendBlock and httpWriteBlock to always absorb the data without blocking */

/**
    Write a block of data to the queue
    @description Write a block of data onto the end of the queue. This will queue the data and may initiaite writing
        to the connection if the queue is full. Data will be appended to last packet in the queue if there is room.
        Otherwise, data packets will be created as required to store the write data.
        \n\n
        This call operates in buffering mode by default unless either the HTTP_BLOCK OR HTTP_NON_BLOCK flag is specified.
        When blocking, the call will either accept and write all the data or it will fail, it will never return "short"
        with a partial write.
        \n\n
        In blocking mode (HTTP_BLOCK), it block for up to the inactivity timeout specified in the
        stream->limits->inactivityTimeout value. In blocking mode, this routine may invoke mprYield before blocking to
        consent for the garbage collector to run. Callers must ensure they have retained all required temporary memory
        before invoking this routine.
        \n\n
        In non-blocking mode (HTTP_NON_BLOCK), the call may return having written fewer bytes than requested.
        \n\n
        In buffering mode (HTTP_BUFFER), the data is always absorbed without blocking and queue size limits are ignored.
        In buffering mode, this routine may invoke mprYield if required to consent for the garbage collector to run.
        Callers must ensure they have retained all required temporary memory before invoking this routine.
        \n\n
        Data written after calling #httpFinalize, #httpFinalizeOutput or #httpError will be discarded.
    @param q Queue reference
    @param buf Buffer containing the write data
    @param size of the data in buf
    @param flags Set to HTTP_BLOCK for blocking operation or HTTP_NON_BLOCK for non-blocking. Set to HTTP_BUFFER to
        buffer the data if required and never block. Set to zero will default to HTTP_BUFFER.
    @return The size value if successful or a negative MPR error code.
    @ingroup HttpQueue
    @stability Evolving
 */
PUBLIC ssize httpWriteBlock(HttpQueue *q, cchar *buf, ssize size, int flags);

/**
    Write a string of data to the queue
    @description Write a string of data into packets onto the end of the queue. Data packets will be created
        as required to store the write data. This call may block waiting for the downstream queue to drain if it is
        or becomes full.
        Data written after #httpFinalizeOutput or #httpError is called will be ignored.
    @param q Queue reference
    @param s String containing the data to write
    @return A count of the bytes actually written
    @ingroup HttpQueue
    @stability Stable
 */
PUBLIC ssize httpWriteString(HttpQueue *q, cchar *s);

/**
    Write a safe string of data to the queue
    @description This will escape any HTML sequences before writing the string into packets onto the end of the queue.
        Data packets will be created as required to store the write data. This call may block waiting for the
        downstream queue to drain if it is or becomes full.
        Data written after #httpFinalizeOutput or #httpError is called will be ignored.
    @param q Queue reference
    @param s String containing the data to write
    @return A count of the bytes actually written
    @ingroup HttpQueue
    @stability Stable
 */
PUBLIC ssize httpWriteString(HttpQueue *q, cchar *s);

/* Internal */

PUBLIC HttpQueue *httpAppendQueue(HttpQueue *q, HttpQueue *prev);
PUBLIC void httpAssignQueueCallbacks(HttpQueue *q, struct HttpStage *stage, int dir);
PUBLIC HttpQueue *httpCreateQueue(struct HttpNet *net, struct HttpStream *stream, struct HttpStage *stage, int dir, HttpQueue *prev);
PUBLIC HttpQueue *httpCreateQueueHead(struct HttpNet *net, struct HttpStream *stream, cchar *name, int dir);
PUBLIC HttpQueue *httpFindPreviousQueue(HttpQueue *q);
PUBLIC HttpQueue *httpGetNextQueueForService(HttpQueue *q);
PUBLIC void httpInitSchedulerQueue(HttpQueue *q);
PUBLIC void httpMarkQueueHead(HttpQueue *q);
PUBLIC void httpOpenQueues(struct HttpStream *stream);
PUBLIC void httpPairQueues(HttpQueue *q1, HttpQueue *q2);
PUBLIC void httpRemovePacket(HttpQueue *q, HttpPacket *prev, HttpPacket *packet);
PUBLIC cchar *httpTraceHeaders(HttpQueue *q, MprHash *headers);
PUBLIC void httpTraceQueues(struct HttpStream *stream);

/******************************** Pipeline Stages *****************************/
/*
    Stage Flags
 */
#define HTTP_STAGE_CONNECTOR      0x1000            /**< Stage is a connector  */
#define HTTP_STAGE_HANDLER        0x2000            /**< Stage is a handler  */
#define HTTP_STAGE_FILTER         0x4000            /**< Stage is a filter  */
#define HTTP_STAGE_MODULE         0x8000            /**< Stage is a filter  */
#define HTTP_STAGE_AUTO_DIR       0x10000           /**< Want auto directory redirection */
#define HTTP_STAGE_UNLOADED       0x20000           /**< Stage module library has been unloaded */
#define HTTP_STAGE_RX             0x40000           /**< Stage to be used in the Rx direction */
#define HTTP_STAGE_TX             0x80000           /**< Stage to be used in the Tx direction */
#define HTTP_STAGE_INTERNAL       0x100000          /**< Internal stage - hidden */

typedef int (*HttpParse)(cchar *key, char *value, void *state);

/**
    Pipeline Stages
    @description The request pipeline consists of a full-duplex pipeline of stages.
        Stages are used to process client HTTP requests in a modular fashion. Each stage either creates, filters or
        consumes data packets. The HttpStage structure describes the stage capabilities and callbacks.
        Each stage has two queues, one for outgoing data and one for incoming data.
        \n\n
        Stages provide callback methods for parsing configuration, matching requests, open/close, run and the
        acceptance and service of incoming and outgoing data.
    Configuration is not thread safe and must occur at initialization time when the application is single threaded.
    If the configuration is modified when the application is multithreaded, all requests must be first be quiesced.
    @defgroup HttpStage HttpStage
    @see HttpStream HttpQueue HttpStage httpCloneStage httpCreateStreamector httpCreateFilter httpCreateHandler
        httpCreateStage httpDefaultOutgoingServiceStage httpGetStageData httpHandleOptionsTrace httpLookupStage
        httpLookupStageData httpSetStageData
    @stability Internal
 */
typedef struct HttpStage {
    char            *name;                  /**< Stage name */
    char            *path;                  /**< Backing module path (from LoadModule) */
    int             flags;                  /**< Stage flags */
    void            *stageData;             /**< Private stage data */
    MprModule       *module;                /**< Backing module */
    MprHash         *extensions;            /**< Matching extensions for this filter */

    /*  These callbacks apply to all stages */

    /**
        Match a request
        @description This routine is invoked to see if the stage wishes to handle the request. For handlers,
            the match callback is invoked when selecting the appropriate route for the request. For filters,
            the callback is invoked subsequently when constructing the request pipeline.
            If a filter declines to handle a request, the filter will be removed from the pipeline for the
            specified direction. The direction argument should be ignored for handlers.
            Handlers and filters must not actually handle the request in the match callback and must not call httpError.
            Errors can be reported via mprError. Handlers can defer error reporting until their start callback.
        @param stream HttpStream stream object
        @param route Route object
        @param dir Queue direction. Set to HTTP_QUEUE_TX or HTTP_QUEUE_RX. Always set to HTTP_QUEUE_TX for handlers.
        @return HTTP_ROUTE_OK if the request is acceptable. Return HTTP_ROUTE_REROUTE if the request has been rewritten.
            Return HTTP_ROUTE_REJECT it the request is not acceptable.
        @ingroup HttpStage
        @stability Evolving
      */
    int (*match)(struct HttpStream *stream, struct HttpRoute *route, int dir);

    /**
        Rewrite a request after matching.
        @description This callback will be invoked for handlers after matching and selecting the handler.
        @param stream HttpStream stream object
        @return Zero for success. Otherwise a negative MPR error code.
        @ingroup HttpStage
        @stability Evolving
     */
    int (*rewrite)(struct HttpStream *stream);

    /**
        Open the stage
        @description Open the stage for this request instance. A handler may service the request in the open routine
            and may call #httpError if required.
            Handlers may block or yield in this callback.
        @param q Queue instance object
        @return Zero for success. Otherwise a negative MPR error code.
        @ingroup HttpStage
        @stability Evolving
     */
    int (*open)(HttpQueue *q);

    /**
        Close the stage
        @description Close the stage and cleanup any request resources.
        Handlers may block or yield in this callback.
        @param q Queue instance object
        @ingroup HttpStage
        @stability Evolving
     */
    void (*close)(HttpQueue *q);

    /**
        Process outgoing data.
        @description Accept a packet as outgoing data. Not used by handlers as handler generate packets internally.
            Filters will use this entry point to accept outgoing packets.
            Filters can choose to immediately process or forward the packet, or they can queue the packet on their
            queue and schedule their outgoingService callback for batch processing of all queued packets. This is
            a common pattern where the outgoing routine is not used and packets are automatically queued and the
            outgoingService callback is used to process data. Filters should not block or yield in this callback.
        @param q Queue instance object
        @param packet Packet of data
        @ingroup HttpStage
        @stability Evolving
     */
    void (*outgoing)(HttpQueue *q, HttpPacket *packet);

    /**
        Service the outgoing data queue
        @description This callback should service packets on the queue and process or forward as appropriate.
        A service routine should check downstream queues by calling #httpWillNextQueueAcceptPacket before forwarding
        packets to ensure they do not overfow downstream queues. Stages should not block or yield in this callback.
        @param q Queue instance object
        @ingroup HttpStage
        @stability Evolving
     */
    void (*outgoingService)(HttpQueue *q);

    /**
        Process incoming data.
        @description Accept an incoming packet of data.
            Filters and handlers recieve packets via their incoming callback. They can choose to immediately process or
            forward the packet, or they can queue the packet on their queue and schedule their incomingService callback
            for batch processing of all queued packets. This is a common pattern where the incoming routine is not
            used and packets are automatically queued and the incomingService callback is used to process.
            Not used by connectors.  Stages should not block or yield in this callback.
        @param q Queue instance object
        @param packet Packet of data
        @ingroup HttpStage
        @stability Evolving
     */
    void (*incoming)(HttpQueue *q, HttpPacket *packet);

    /**
        Service the incoming data queue
        @description This callback should service packets on the queue and process or forward as appropriate.
        A service routine should check upstream queues by calling #httpWillNextQueueAcceptPacket before forwarding
        packets to ensure they do not overfow upstream queues.  Handlers may not block or yield in this callback.
        @param q Queue instance object
        @ingroup HttpStage
        @stability Evolving
     */
    void (*incomingService)(HttpQueue *q);

    /*  These callbacks apply only to handlers */

    /**
        Start the handler
        @description The start callback is primarily responsible for starting the request processing.
        Depending on the request Content Type, the request will be started at different times.
        Form requests with a Content-Type of "application/x-www-form-urlencoded", will be started after fully
        receiving all input data. Other requests will be started immediately after the request headers have been
        parsed and before receiving input data. This enables such requests to stream large quantities of input
        data without buffering. The start callback should test the HTTP method in stream->rx->method and only
        respond to supported HTTP methods. It should call httpError for unsupported methods. The start callback
        will not be called if the request already has an error. Handlers may block or yield in this callback.
        @param q Queue instance object
        @ingroup HttpStage
        @stability Evolving
     */
    void (*start)(HttpQueue *q);

    /**
        The request is now fully ready.
        @description This callback will be invoked when all incoming data has been received.
            The ready callback will not be called if the request already has an error.
            If a handler finishes processing the request, it should call #httpFinalizeOutput in the ready routine.
        Handlers may block or yield in this callback.
        @param q Queue instance object
        @ingroup HttpStage
        @stability Evolving
     */
    void (*ready)(HttpQueue *q);

    /**
        The outgoing pipeline is writable and can accept more response data.
        @description This callback will be invoked after all incoming data has been receeived and whenever the outgoing
        pipeline can absorb more output data (writable). As such, it may be called multiple times and can be effectively
        used for non-blocking generation of a response.
        The writable callback will not be invoked if the request output has been finalized or if an error has occurred.
        Handlers may block or yield in this callback.
        @param q Queue instance object
        @ingroup HttpStage
        @stability Evolving
     */
    void (*writable)(HttpQueue *q);

} HttpStage;

/**
    Create a clone of an existing state. This is used when creating filters configured to match certain extensions.
    @param stage Stage object to clone
    @return A new stage object
    @ingroup HttpStage
    @stability Stable
*/
PUBLIC HttpStage *httpCloneStage(HttpStage *stage);

/**
    Create a connector stage
    @description Create a new connector. Connectors are the final stage for outgoing data. Their job is to transmit
        outgoing data to the client.
    @param name Name of connector stage
    @param module Optional module object for loadable stages
    @return A new stage object
    @ingroup HttpStage
    @stability Stable
 */
PUBLIC HttpStage *httpCreateStreamector(cchar *name, MprModule *module);

/**
    Create a filter stage
    @description Create a new filter. Filters transform data generated by handlers and before connectors transmit to
        the client. Filters can apply transformations to incoming, outgoing or bi-directional data.
    @param name Name of connector stage
    @param module Optional module object for loadable stages
    @return A new stage object
    @ingroup HttpStage
    @stability Stable
 */
PUBLIC HttpStage *httpCreateFilter(cchar *name, MprModule *module);

/**
    Create a request handler stage
    @description Create a new handler. Handlers generate outgoing data and are the final stage for incoming data.
        Their job is to process requests and send outgoing data downstream toward the client consumer.
        There is ever only one handler for a request.
    @param name Name of connector stage
    @param module Optional module object for loadable stages
    @return A new stage object
    @ingroup HttpStage
    @stability Stable
 */
PUBLIC HttpStage *httpCreateHandler(cchar *name, MprModule *module);

/**
    Create a connector stage
    @description Create a new stage.
    @param name Name of connector stage
    @param flags Stage flags
    @param module Optional module object for loadable stages
    @return A new stage object
    @ingroup HttpStage
    @stability Stable
 */
PUBLIC HttpStage *httpCreateStage(cchar *name, int flags, MprModule *module);

/**
    Lookup a stage by name
    @param name Name of stage to locate
    @return Stage or NULL if not found
    @ingroup HttpStage
    @stability Stable
*/
PUBLIC struct HttpStage *httpLookupStage(cchar *name);

/**
    Default incoming put callback.
    @description Adds packet to the service queue
    @param q Current queue
    @param packet Packet containg data
    @ingroup HttpStage
    @stability Stable
*/
PUBLIC void httpDefaultIncoming(HttpQueue *q, HttpPacket *packet);

/**
    Default outgoing data handling
    @description This routine provides default handling of outgoing data for stages. It simply sends all packets
        downstream.
    @param q Queue object
    @ingroup HttpStage
    @stability Stable
 */
PUBLIC void httpDefaultOutgoingServiceStage(HttpQueue *q);

/**
    Get stage data
    @description Stages can store extra configuration information indexed by key. This is used by handlers, filters,
        connectors and and handlers.
    @param stream HttpStream stream object
    @param key Key index into the stage data
    @return A reference to the stage data. Otherwise return null if the route data for the given key was not found.
    @ingroup HttpRx
    @stability Stable
 */
PUBLIC cvoid *httpGetStageData(struct HttpStream *stream, cchar *key);

/**
    Handle a Http Options method request
    @description Convenience routine to respond to an OPTIONS request.
    @param stream HttpStream object created via #httpCreateStream
    @ingroup HttpStage
    @stability Stable
 */
PUBLIC void httpHandleOptions(struct HttpStream *stream);

/**
    Lookup stage data
    @description This looks up the stage by name and returns the private stage data.
    @param name Name of the stage concerned
    @return Reference to the stage data block.
    @ingroup HttpStage
    @stability Stable
 */
PUBLIC void *httpLookupStageData(cchar *name);

/**
    Set stage data
    @description Stages can store extra configuration information indexed by key. This is used by handlers, filters,
        connectors and applications.
    @param stream HttpStream stream object
    @param key Key index into the stage data
    @param data Reference to custom data allocated via mprAlloc.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpSetStageData(struct HttpStream *stream, cchar *key, cvoid *data);

/* Internal APIs */
PUBLIC void httpAddStage(HttpStage *stage);
PUBLIC ssize httpFilterChunkData(HttpQueue *q, HttpPacket *packet);
PUBLIC int httpOpenActionHandler(void);
PUBLIC int httpOpenChunkFilter(void);
PUBLIC int httpOpenCacheHandler(void);
PUBLIC int httpOpenDirHandler(void);
PUBLIC int httpOpenFileHandler(void);
PUBLIC int httpOpenPassHandler(void);
PUBLIC int httpOpenRangeFilter(void);
PUBLIC int httpOpenNetConnector(void);
PUBLIC int httpOpenUploadFilter(void);
PUBLIC int httpOpenWebSockFilter(void);
PUBLIC int httpSendOpen(HttpQueue *q);
PUBLIC void httpSendOutgoingService(HttpQueue *q);
PUBLIC int httpHandleDirectory(struct HttpStream *stream);
PUBLIC int httpOpenHttp1Filter(void);
PUBLIC int httpOpenHttp2Filter(void);
PUBLIC int httpOpenTailFilter(void);

/********************************** Http2 **************************************/
#if ME_HTTP_HTTP2 || DOXYGEN
/*
    HTTP/2 Frame format

     Byte 0          Byte 1          Byte 2          Byte 3
     0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
    +------------------------------------------------+--------------+
    |                Length (24)                     |    Type (8)  |
    +------------------------------------------------+--------------+
    |     Flags (8)  |R|           Stream Identifier (31)           |
    +------------------------------------------------+--------------+
    |    Stream Cont |    Payload Data ....                         |
    +------------------------------------------------+--------------+
 */

/*
    HTTP/2 Frame sizes
 */
#define HTTP2_FRAME_OVERHEAD        9                       /**< Minimum HTTP/2 frame size */
#define HTTP2_SETTINGS_SIZE         6                       /**< Size of settings frame data */
#define HTTP2_WINDOW_SIZE           4                       /**< Size of windows frame data */
#define HTTP2_RESET_SIZE            4                       /**< Size of rest frame data */
#define HTTP2_GOAWAY_SIZE           8                       /**< Size of goaway frame data */

/*
    HTTP/2 parameters
 */
#define HTTP2_MAX_FRAME_SIZE        ((1 << 24) - 1)         /**< Maximum frame size by spec */
#define HTTP2_MAX_STREAM            ((1U << 31) - 1)        /**< Maximum stream number by spec */
#define HTTP2_MAX_WINDOW            ((1U << 31) - 1)        /**< Maximum window size by spec */

/*
    Do not change these defaults. They are defined by the spec.
 */
#define HTTP2_MIN_WINDOW            65535                   /**< Initial default window size by spec */
#define HTTP2_MIN_FRAME_SIZE        (16 * 1024)             /**< Default and minimum frame size - modified by config */
#define HTTP2_DEFAULT_WEIGHT        16                      /**< Unused */

/*
    Misc flags and constants
 */
#define HTTP2_PREFACE               "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"
#define HTTP2_PREFACE_SIZE          24
#define HTTP2_ENCODE_RAW            0
#define HTTP2_ENCODE_HUFF           0x80
#define HTTP2_HEADER_TABLE_SIZE     512
#define HTTP2_TABLE_SIZE            4096
#define HTTP2_HEADER_OVERHEAD       32                      /**< HPACK table overhead by spec */
#define HTTP_STREAM_MASK            0x7fffffff

/*
    HTTP/2 Frame types
*/
#define HTTP2_DATA_FRAME            0x0
#define HTTP2_HEADERS_FRAME         0x1
#define HTTP2_PRIORITY_FRAME        0x2
#define HTTP2_RESET_FRAME           0x3
#define HTTP2_SETTINGS_FRAME        0x4
#define HTTP2_PUSH_FRAME            0x5
#define HTTP2_PING_FRAME            0x6
#define HTTP2_GOAWAY_FRAME          0x7
#define HTTP2_WINDOW_FRAME          0x8
#define HTTP2_CONT_FRAME            0x9
#define HTTP2_MAX_FRAME             0xA

/*
    HTTP/2 frame flags
 */
#define HTTP2_ACK_FLAG              0x1
#define HTTP2_END_STREAM_FLAG       0x1
#define HTTP2_END_HEADERS_FLAG      0x4
#define HTTP2_PADDED_FLAG           0x8
#define HTTP2_PRIORITY_FLAG         0x20

/*
    Settings fields
 */
#define HTTP2_HEADER_TABLE_SIZE_SETTING    0x1
#define HTTP2_ENABLE_PUSH_SETTING          0x2
#define HTTP2_MAX_STREAMS_SETTING          0x3
#define HTTP2_INIT_WINDOW_SIZE_SETTING     0x4
#define HTTP2_MAX_FRAME_SIZE_SETTING       0x5
#define HTTP2_MAX_HEADER_SIZE_SETTING      0x6

/*
    HTTP2 error codes
 */
#define HTTP2_NO_ERROR              0x0
#define HTTP2_INTERNAL_ERROR        0x2
#define HTTP2_PROTOCOL_ERROR        0x1
#define HTTP2_FLOW_CONTROL_ERROR    0x3
#define HTTP2_SETTINGS_TIMEOUT      0x4
#define HTTP2_STREAM_CLOSED         0x5
#define HTTP2_FRAME_SIZE_ERROR      0x6
#define HTTP2_REFUSED_STREAM        0x7
#define HTTP2_CANCEL                0x8
#define HTTP2_COMP_ERROR            0x9
#define HTTP2_CONNECT_ERROR         0xa
#define HTTP2_ENHANCE_YOUR_CALM     0xb
#define HTTP2_INADEQUATE_SECURITY   0xc
#define HTTP2_HTTP_1_1_REQUIRED     0xd

/*
    Hpack static string indexes
 */
#define HTTP2_METHOD_GET        2
#define HTTP2_METHOD_POST       3
#define HTTP2_PATH_ROOT         4
#define HTTP2_PATH_INDEX        4
#define HTTP2_STATUS_200        8
#define HTTP2_STATUS_204        9
#define HTTP2_STATUS_206        10
#define HTTP2_STATUS_304        11
#define HTTP2_STATUS_400        12
#define HTTP2_STATUS_404        13
#define HTTP2_STATUS_500        14

typedef struct HttpFrame {
    struct HttpStream   *stream;
    int                 type;               /**< Frame type */
    int                 flags;              /**< Flags */
    int                 streamID;           /**< Current stream ID */
} HttpFrame;

/**
    HTTP HPACK header table
 */
typedef struct HttpHeaderTable {
    MprList         *list;                  /**< Header list */
    ssize           size;
    ssize           max;
} HttpHeaderTable;

/*
    Internal
 */
PUBLIC void httpCreatePackedHeaders(void);
PUBLIC int httpLookupPackedHeader(HttpHeaderTable *headers, cchar *key, cchar *value, bool *withValue);
PUBLIC MprKeyValue *httpGetPackedHeader(HttpHeaderTable *headers, int index);
PUBLIC int httpAddPackedHeader(HttpHeaderTable *headers, cchar *key, cchar *value);
PUBLIC int httpSetPackedHeadersMax(HttpHeaderTable *headers, int size);
PUBLIC ssize httpHuffEncode(cchar *src, ssize len, char *dst, uint lower);
PUBLIC cchar *httpHuffDecode(uchar *src, int len);
#endif /* ME_HTTP_HTTP2 */

/********************************* Network *************************************/
/**
    I/O callback for network connections
    @param stream HttpNet object created via #httpCreateStream
    @param event Event object describing the I/O event
    @ingroup HttpNet
    @stability Evolving
  */
typedef void (*HttpIOCallback)(struct HttpNet *net, MprEvent *event);

/*
    HTTP protocol variants
 */
#define HTTP_1_0        0
#define HTTP_1_1        1
#define HTTP_2          2

#define HTTP_NET_ASYNC  0x1

/**
    Control object for the network connection. A network connection may multiplex many HttpStream objects that represent
    logical streams over the connection.
    @defgroup HttpNet HttpNet
    @see HttpNet httpCreateNet httpDestroyNet httpIOEvent httpNetError httpServiceQueues httpSetIOCallback httpSetNetContext httpStealSocket httpStealSocketHandle httpEnableNetEvents httpNetTimeout httpGetProtocol httpGetAsync httpSetAsync httpConnectNet
    @stability Internal
 */
typedef struct HttpNet {
    Http            *http;                  /**< Http service object  */
    HttpLimits      *limits;                /**< Service limits */
    MprSocket       *sock;                  /**< Underlying socket handle */
    MprList         *streams;               /**< List of streams */
    struct HttpEndpoint
                    *endpoint;              /**< Endpoint object (if set - indicates server-side) */

    int             port;                   /**< Remote port */
    char            *ip;                    /**< Remote client IP address */
    cchar           *errorMsg;              /**< Error message for the last request (if any) */

    HttpTrace       *trace;                 /**< Tracing configuration */
    HttpIOCallback  ioCallback;             /**< I/O event callback */
    HttpAddress     *address;               /**< Per-client IP address reference */

    HttpQueue       *holdq;                 /**< GC hold queue while scheduling */
    HttpQueue       *inputq;                /**< Queue of packets received from the network (http-rx) */
    HttpQueue       *outputq;               /**< Queue of packets to write to the network (http-tx) */
    HttpQueue       *serviceq;              /**< List of queues that require service */
    HttpQueue       *socketq;               /**< Queue of packets to write to the output socket (last queue) */

#if ME_HTTP_HTTP2 || DOXYGEN
    HttpHeaderTable *rxHeaders;             /**< Cache of HPACK rx headers */
    HttpHeaderTable *txHeaders;             /**< Cache of HPACK tx headers */
    HttpFrame       *frame;                 /**< Current frame being parsed */
#endif

    MprDispatcher   *dispatcher;            /**< Event dispatcher */
    MprDispatcher   *newDispatcher;         /**< New dispatcher if using a worker thread */
    MprDispatcher   *oldDispatcher;         /**< Original dispatcher if using a worker thread */
    HttpNotifier    notifier;               /**< Default notifier to use for streams - copied from endpoint */

    MprEvent        *timeoutEvent;          /**< Connection or request timeout event */
    MprEvent        *workerEvent;           /**< Event for running connection via a worker thread (used by ejs) */
    MprTicks        lastActivity;           /**< Last activity on the connection */
    MprOff          bytesWritten;           /**< Total bytes written */

    void            *context;               /**< Embedding context (EjsRequest) */
    void            *data;                  /**< Custom data */

#if UNUSED || 1
    void            *ejs;                   /**< Embedding VM */
    void            *pool;                  /**< Pool of VMs */
#endif

    int             delay;                  /**< Delay servicing requests due to defense strategy */
    int             nextStreamID;           /**< Next stream ID */
    int             lastStreamID;           /**< Last stream ID */
    int             ownStreams;             /**< Number of peer created streams */
    int             seqno;                  /**< Unique network sequence number */
    int             session;                /**< Currently parsing frame for this session */
    int             timeout;                /**< Network timeout indication */
    int             totalRequests;          /**< Total number of requests serviced */

    bool            async: 1;               /**< Network is in async mode (non-blocking) */
    bool            borrowed: 1;            /**< Socket has been borrowed */
    bool            destroyed: 1;           /**< Net object has been destroyed */
    bool            eof: 1;                 /**< Socket has been closed */
    bool            error: 1;               /**< Hard network error - cannot continue */
    uint            eventMask: 3;           /**< Last IO event mask */
    bool            goaway: 1;              /**< Closing network connection (sent or received a goAway frame) */
    bool            init: 1;                /**< Settings frame has been sent and network is ready to use */
    uint            protocol: 2;            /**< HTTP protocol: 0 for HTTP/1.0, 1 for HTTP/1.1 or 2+ */
    bool            receivedGoaway: 1;      /**< Received goaway frame */
    bool            ownDispatcher: 1;       /**< Using own the dispatcher and should destroy when closing connection */
    bool            push: 1;                /**< Receiver will accept push */
    bool            secure: 1;              /**< Using https */
    bool            skipTrace: 1;           /**< Omit trace from now on */
    bool            worker: 1;              /**< Use worker */
    bool            writeBlocked: 1;        /**< Transmission writing is blocked */
} HttpNet;

/**
    Borrow a network connection
    @description Borrow the network from Http. This effectively gains an exclusive loan of the network so that it
    cannot be destroyed while the loan is active. After the loan is complete, you must call return the network
    by calling #httpReturnNet. Otherwise the network will not be freed and memory will leak.
    \n\n
    The httpBorrowNet routine is used to stabilize a network while interacting with some outside service.
    Without this routine, the network could be destroyed while waiting. Many things can happen while waiting.
    For example: the client could disconnect or the network connection could timeout. These events will still be
    serviced while the network is borrowed, but the network object will not be destroyed.
    \n\n
    While borrowed, you must not access the network object using foreign / non-MPR threads. If you need to do this,
    use #mprCreateEvent to schedule an event to run on the networks's event dispatcher.
    This is essential to serialize access to the network object.
    \n\n
    Before returning from the event callback, you must call #httpReturnNet to end the exclusive loan.
    This restores normal processing of the connection and enables any required I/O events.
    \n\n
    @param net HttpNet object created via #httpCreateNet
    @ingroup HttpNet
    @stability Prototype
 */
PUBLIC void httpBorrowNet(HttpNet *net);

/**
    Connect the network to a remote peer.
    @param net HttpNet Network object created via #httpCreateNet
    @param ip Remote IP address to connect to
    @param port TCP/IP port to connect to
    @param ssl MprSsl object that defines the SSL context
    @return Zero if successful, otherwise a negative MPR error code.
    @ingroup HttpNet
    @stability Prototype
  */
PUBLIC int httpConnectNet(HttpNet *net, cchar *ip, int port, MprSsl *ssl);

/**
    Create a network object.
    @description The network object defines the underlying network connection over which HttpStream connection streams will
    be multiplexed.
    @param dispatcher Event MprDispatcher object to serialize events for the network.
    @param endpoint Server-side HttpEndpoint object. Set to NULL for client-side.
    @param protocol HTTP protocol to use by default. Set to 1 for HTTP/1 and 2 for HTTP/2.
    @param flags Set to HTTP_NET_ASYNC if you wish to use async I/O. Otherwise set to zero.
    @returns A new network object
    @ingroup HttpNet
    @stability Evolving
*/
PUBLIC HttpNet *httpCreateNet(MprDispatcher *dispatcher, struct HttpEndpoint *endpoint, int protocol, int flags);

/**
    Destroy the network object
    @description This call closes the network socket, destroys the connection dispatcher, disconnects the HttpStream
        objects and removes the network from the HttpHost list of networks. All active stream connections
        (HttpStream) will also be destroyed. Thereafter, the garbage collector can reclaim all memory. It may be called by
        client connections at any time from a top-level event running on the connection's dispatcher. Server-side code
        should not explicitly destroy the connection as it will be done automatically via httpIOEvent.
    @param net HttpNet object created via #httpCreateNet
    @ingroup HttpNet
    @stability Internal
 */
PUBLIC void httpDestroyNet(HttpNet *net);

/**
    Enable network events
    @description Network events are automatically disabled upon receipt of an I/O event on a network connection. This
        permits a network to process the I/O without fear of interruption by another I/O event. At the completion
        of processing of the I/O request, the network should be re-enabled via httpEnableNetEvents. This call is
        made for requests in #httpIOEvent. Client-side networks may need to enable network events if they are
        running in async mode and encounter a blocking condition.
    @param net HttpNet Network object created via #httpCreateNet
    @ingroup HttpNet
    @stability Evolving
 */
PUBLIC void httpEnableNetEvents(HttpNet *net);

/**
    Get the Http protocol variant for this network connection
    @param net HttpNet Network object created via #httpCreateNet
    @return HTTP/1.0, HTTP/1.1 or HTTP/2.
    @ingroup HttpNet
    @stability Evolving
 */
PUBLIC cchar *httpGetProtocol(HttpNet *net);

/**
    Get the async mode value for the network
    @param net HttpNet object created via #httpCreateNet
    @return True if the Network is in async mode
    @ingroup HttpNet
    @stability Evolving
 */
PUBLIC bool httpGetAsync(HttpNet *net);

/**
    Respond to a HTTP I/O event
    @description This routine responds to I/O events. If any readable data is present, it allocates a standard sized
        packet and reads data into this packet and passes to the input queue pipeline.
    @param net HttpNet object created via #httpCreateNet
    @param event Event structure
    @ingroup HttpNet
    @stability Internal
 */
PUBLIC void httpIOEvent(struct HttpNet *net, MprEvent *event);

/**
    Test if the network is a client-side network
    @param net HttpNet Network object created via #httpCreateNet
    @return true if the network is client-side
    @ingroup HttpNet
    @stability Prototype
  */
#define httpIsClient(net) (net && !net->endpoint)

/**
    Test if the network is a server-side network
    @param net HttpNet Network object created via #httpCreateNet
    @return true if the network is server-side
    @ingroup HttpNet
    @stability Prototype
  */
#define httpIsServer(net) (net && net->endpoint)

/**
    Error handling for the network.
    @description The httpNetError call is used to flag the current network as failed. If httpNetError is called multiple
        times, those calls are ignored and only the first call to httpNetError has effect.
        This call will close all network streams and discard all data in output pipeline queues.
    @param net HttpNet object created via #httpCreateNet
    @param fmt Printf style formatted string
    @param ... Arguments for fmt
    @ingroup HttpNet
    @stability Evolving
 */
PUBLIC void httpNetError(HttpNet *net, cchar *fmt, ...);

/**
    Schedule a network connection timeout event on a network
    @description This call schedules a timeout event to run serialized on the network's dispatcher. When run, it will
        cancel all current requests on the network, disconnect the socket and issue an error to the error log.
        This call is normally invoked by the httpTimer which runs regularly to check for timed out requests.
    @param net HttpNet Network object created via #httpCreateNet
    @ingroup HttpNet
    @stability Internal
  */
PUBLIC void httpNetTimeout(HttpNet *net);

/**
    Return a borrowed a network connection
    @description Returns a borrowed network object back to the Http engine. This ends the exclusive loan of the
        network so that the current request can be completed. It also enables I/O events based on the
        current state of the network.
    \n\n
    While the network is borrowed, you must not access the network using foreign / non-MPR threads.
    Use #mprCreateEvent to schedule an event to run on the network's event dispatcher. This is
    essential to serialize access to the network object.
    \n\n
    You should only call this routine (once) after calling #httpBorrowNet.
    \n\n
    @param net HttpNet object created via #httpCreateNet
    @ingroup HttpNet
    @stability Prototype
 */
PUBLIC void httpReturnNet(HttpNet *net);

/**
    Service pipeline queues to flow data.
    @description This routine should not be called by handlers, filters or user applications. It should only be called
        by the http pipeline and support routines.
    @param net HttpNet object created via #httpCreateNet
    @param flags Set to HTTP_BLOCK to yield for GC if due
    @return True if work was done servicing queues.
    @ingroup HttpNet
    @stability Evolving
 */
PUBLIC bool httpServiceQueues(HttpNet *net, int flags);

/**
    Define an I/O callback for network connections
    @description The I/O callback is invoked when I/O events are detected on the network. The default I/O callback
        is #httpIOEvent.
    @param net HttpNet object created via #httpCreateNet
    @param fn Callback function to invoke
    @ingroup HttpNet
    @stability Evolving
  */
PUBLIC void httpSetIOCallback(struct HttpNet *net, HttpIOCallback fn);

/**
    Test if the network queues need service
    @param net HttpNet object created via #httpCreateNet
    @return True if there are queues that require servicing
    @ingroup HttpNet
    @stability Evolving
  */
PUBLIC bool httpQueuesNeedService(HttpNet *net);

/**
    Set the async mode value for the network
    @param net HttpNet object created via #httpCreateNet
    @param async Set to 1 to enable async mode
    @return True if the network is in async mode
    @ingroup HttpNet
    @stability Evolving
 */
PUBLIC void httpSetAsync(HttpNet *net, bool async);

/**
    Set the network context object
    @param net HttpNet object created via #httpCreateNet
    @param context New context object. Must be a managed memory reference.
    @ingroup HttpNet
    @stability Evolving
 */
PUBLIC void httpSetNetContext(HttpNet *net, void *context);

/**
    Set the Http protocol variant for this network connection
    @description Set the Http protocol variant to use.
    @param net HttpNet Network object created via #httpCreateNet
    @param protocol  Integer representing the protocol variant. Valid values are: 0 for HTTP/1.0, 1 for HTTP/1.1 and 2 for HTTP/2.
    @ingroup HttpNet
    @stability Evolving
 */
PUBLIC void httpSetNetProtocol(HttpNet *net, int protocol);

/**
    Steal a socket from a network
    @description Steal the MprSocket object from a network so the caller can assume total responsibility for the socket.
    This routine returns a clone of the networks's socket object with the socket O/S handle. The handle is removed from the
    networks's socket object. The network retains ownership of the original MprSocket object -- sans the socket handle.
    This is done to preserve the HttpNetwork.sock object but remove the socket handle from its management.
    \n\n
    Note: The current request is aborted and queue data is discarded.
    After calling, the normal Appweb request and inactivity timeouts will not apply to the returned socket object.
    It is the callers responsibility to call mprCloseSocket on the returned MprSocket when ready.
    \n\n
    An alternative to this routine is #httpBorrowNet which temporarily loans the network socket and secures it from destruction.
    @param net HttpNet object created via #httpCreateNet
    @return A clone of the network's MprSocket object with the socket handle.
    @ingroup HttpNet
    @stability Evolving
 */
PUBLIC MprSocket *httpStealSocket(HttpNet *net);

/**
    Steal the O/S socket handle from the network socket object.
    @description This removes the O/S socket handle from active management by the network. After calling,
    normal request and inactivity timeouts will apply to the network, but will not disturb the underlying
    actual socket handle.  It is the callers responsibility to call close() on the socket handle when ready.
    @param net HttpNet object created via #httpCreateNet
    @return The O/S Socket handle.
    @ingroup HttpNet
    @stability Evolving
 */
PUBLIC Socket httpStealSocketHandle(HttpNet *net);

/*
    Internal
 */
PUBLIC void httpGetUriAddress(HttpUri *uri, cchar **ip, int *port);
PUBLIC void httpSetNetTimeout(HttpNet *net, MprTicks inactivityTimeout);
PUBLIC void httpSendGoAway(struct HttpNet *net, int status, cchar *fmt, ...);
PUBLIC void httpBindSocket(HttpNet *net, MprSocket *sock);
PUBLIC void httpNetClosed(HttpNet *net);
PUBLIC void httpUsePrimary(HttpNet *net);
PUBLIC void httpUseWorker(HttpNet *net, MprDispatcher *dispatcher, MprEvent *event);
PUBLIC void httpSetupWaitHandler(HttpNet *net, int eventMask);

/********************************** HttpStream *********************************/
/**
    Notifier events
 */
#define HTTP_EVENT_STATE            1       /**< The request is changing state */
#define HTTP_EVENT_READABLE         2       /**< The request has data available for reading */
#define HTTP_EVENT_WRITABLE         3       /**< The request is now writable (post / put data) */
#define HTTP_EVENT_ERROR            4       /**< The request has an error */
#define HTTP_EVENT_DESTROY          5       /**< The HttpStream object is being closed and destroyed */

/*
    Application level events
 */
#define HTTP_EVENT_APP_CLOSE        6       /**< The request is now closed */

/*
    Internal hidden events. Not exposed by the Http notifier.
 */
#define HTTP_EVENT_APP_OPEN         7       /* The request is now open */
#define HTTP_EVENT_MAX              8       /**< Maximum event plus one */

/*
    Stream states
    It is critical that the states be ordered and the values be contiguous. The httpSetState relies on this.
    The http notifier is called when transitioning to these states.
 */
#define HTTP_STATE_BEGIN            1       /**< Ready for a new request */
#define HTTP_STATE_CONNECTED        2       /**< Connection received or made */
#define HTTP_STATE_FIRST            3       /**< First request line has been parsed */
#define HTTP_STATE_PARSED           4       /**< Headers have been parsed, handler can start */
#define HTTP_STATE_CONTENT          5       /**< Reading posted content */
#define HTTP_STATE_READY            6       /**< Handler ready - all body data received  */
#define HTTP_STATE_RUNNING          7       /**< Handler running */
#define HTTP_STATE_FINALIZED        8       /**< Input received, request processed and response transmitted */
#define HTTP_STATE_COMPLETE         9       /**< Request complete */


/**
    Callback to fill headers
    @description If defined, the headers callback will run before the standard response headers are generated. This gives an
    opportunity to pre-populate the response headers.
    @param arg Argument provided to httpSetHeadersCallback when the callback was established.
    @ingroup HttpStream
    @stability Evolving
 */
typedef int (*HttpHeadersCallback)(void *arg);

/**
    Define a headers callback
    @description The headers callback will run before the standard response headers are generated. This gives an
        opportunity to pre-populate the response headers.
    @param stream HttpStream object created via #httpCreateStream
    @param fn Callback function to invoke
    @param arg Argument to provide when invoking the headers callback
    @ingroup HttpStream
    @stability Evolving
 */
PUBLIC void httpSetHeadersCallback(struct HttpStream *stream, HttpHeadersCallback fn, void *arg);

/**
    Http request stream
    @description The HttpStream object represents a logical request stream to a peer.
        A stream object is created for each request transaction. A stream object uses the underlying HttpNet network object. If using HTTP/2 there may be multiple HttpStream objects multiplexed over a single HttpNet object.
        If using HTTP/1, a single HttpStream object may service many Http requests due to HTTP/1.1 keep-alive.
        \n\n
        In prior versions before HTTP/2, this was called the HttpConn (connection) object. For compatibility, a macro define will redefine
        HttpConn references to HttpStream.
        \n\n
        Each connection has a request timeout and inactivity timeout. These can be set via #httpSetTimeout.
        The set of APIs that block and yield to the garbage collector are:
        <ul>
        <li>httpFlushQueue(, HTTP_BLOCK)</li>
        <li>httpWriteBlock(, HTTP_BLOCK)</li>
        <li>httpSendBlock(, HTTP_BLOCK)</li>
        <li>httpRead() when in sync mode</li>
        <li>httpReadBlock(, HTTP_BLOCK)</li>
        <li>httpWait()</li>
        <li>httpWriteUploadData</li>
        </ul>
        When these APIs block and yield, the garbage collector may reclaim allocated memory that does not have a
        managed reference. Read Appweb memory allocation at https://embedthis.com/appweb/doc/ref/memory.html.
        \n\n
        Some of the HttpStream fields are replicated from the HttpNet object for API compatibility.

    @defgroup HttpStream HttpStream
    @see HttpStream HttpEnvCallback HttpGetPassword HttpListenCallback HttpNotifier HttpQueue HttpRedirectCallback
        HttpRx HttpStage HttpTx HtttpListenCallback httpCallEvent httpFinalizeConnector httpStreamTimeout
        httpCreateStream httpCreateRxPipeline httpCreateTxPipeline httpDestroyStream httpClosePipeline httpDiscardData
        httpDisconnect httpEnableUpload httpError httpGetChunkSize httpGetStreamContext
        httpGetStreamHost httpGetError httpGetExt httpGetKeepAliveCount httpGetWriteQueueCount httpMatchHost httpMemoryError
        httpAfterEvent httpResetClientConn httpResetCredentials httpRouteRequest httpRunHandlerReady httpService
        httpSetChunkSize httpSetStreamContext httpSetStreamHost httpSetStreamNotifier httpSetCredentials
        httpSetFileHandler httpSetKeepAliveCount httpSetNetProtocol httpSetRetries httpSetState
        httpSetTimeout httpSetTimestamp httpStartPipeline
    @stability Internal
 */
typedef struct HttpStream {
    HttpNet         *net;
    int             state;                  /**< Stream state */
    struct HttpRx   *rx;                    /**< Rx object for HTTP/1 */
    struct HttpTx   *tx;                    /**< Tx object for HTTP/1 */

    HttpQueue       *rxHead;                /**< Receive queue head */
    HttpQueue       *txHead;                /**< Transmit queue head */
    HttpQueue       *inputq;                /**< Start of the read pipeline (tailFilter-rx) */
    HttpQueue       *outputq;               /**< End of the write pipeline (tailFilter-tx) */
    HttpQueue       *readq;                 /**< Application queue to old incoming data for reading (qhead) */
    HttpQueue       *writeq;                /**< Application queue to write outgoing data (handler) */

    MprSocket       *sock;                  /**< Underlying socket handle */
    HttpLimits      *limits;                /**< Service limits. Alias to HttpRoute.limits for this request */
    Http            *http;                  /**< Http service object  */
    MprDispatcher   *dispatcher;            /**< Event dispatcher */
    HttpNotifier    notifier;               /**< Http state change notification callback */

    struct HttpEndpoint *endpoint;          /**< Endpoint object (if set - indicates server-side) */
    struct HttpHost *host;                  /**< Host object (if relevant) */

    MprTicks        started;                /**< When the request started (ticks) */
    MprTicks        lastActivity;           /**< Last activity on the connection */
    MprEvent        *timeoutEvent;          /**< Connection or request timeout event */
    HttpTrace       *trace;                 /**< Tracing configuration */
    uint64          startMark;              /**< High resolution tick time of request */

    char            *boundary;              /**< File upload boundary */
    void            *context;               /**< Embedding context (EjsRequest) */
    void            *data;                  /**< Custom data for request - must be a managed reference */
    void            *ejs;                   /**< Embedding VM */
    cchar           *errorMsg;              /**< Error message for the last request (if any) */
    void            *grid;                  /**< Current request database grid for MVC apps */
    char            *ip;                    /**< Remote client IP address */
    void            *mark;                  /**< Reference for GC marking */
    void            *pool;                  /**< Pool of VMs */
    char            *protocols;             /**< Supported WebSocket protocols (clients) */
    void            *reqData;               /**< Extended request data for use by web frameworks */
    void            *record;                /**< Current request database record for MVC apps */
    void            *staticData;            /**< Custom data for request - must be an unmanaged reference */

    int             activeRequest;          /**< Actively servicing a request */
    int             keepAliveCount;         /**< Count of remaining Keep-Alive requests for this connection */
    int             port;                   /**< Remote port */
    int             streamID;               /**< Http/2 stream */
    int             timeout;                /**< Timeout indication */

    bool            authRequested: 1;       /**< Authorization requested based on user credentials */
    bool            complete: 1;            /**< Request is complete */
    bool            destroyed: 1;           /**< Stream has been destroyed */
    bool            disconnect;             /**< Must disconnect/reset the connection - can not continue */
    bool            encoded: 1;             /**< True if the password is MD5(username:realm:password) */
    bool            error;                  /**< An error has occurred and the request cannot be completed */
    bool            errorDoc: 1;            /**< Processing an error document */
    bool            followRedirects: 1;     /**< Follow redirects for client requests */
    bool            peerCreated: 1;         /**< Stream created by peer */
    bool            ownDispatcher: 1;       /**< Own the dispatcher and should destroy when closing connection */
    bool            secure: 1;              /**< Using https */
    bool            seenHeader:1;           /**< Already seen at least one header packet in the output queue */
    bool            streamReset: 1;         /**< Stream reset (http2) */
    bool            suppressTrace: 1;       /**< Do not trace this connection */
    bool            upgraded: 1;            /**< Request protocol upgraded */

    /*
        Authentication
     */
    char            *authType;              /**< Type of authentication: set to basic, digest, post or a custom name */
    void            *authData;              /**< Authorization state data */
    cchar           *username;              /**< Supplied user name */
    cchar           *password;              /**< Password for client requests (only) */
    struct HttpUser *user;                  /**< Authorized User record for access checking */

    HttpTimeoutCallback timeoutCallback;    /**< Request and inactivity timeout callback */
    HttpIOCallback  ioCallback;             /**< I/O event callback */
    HttpHeadersCallback headersCallback;    /**< Callback to fill headers */
    void            *headersCallbackArg;    /**< Arg to fillHeaders */
} HttpStream;

/*
    LEGACY redefines
 */
#define HttpConn HttpStream
#define httpCreateConn httpCreateStream
#define httpDestroyConn httpDestoryStream
#define httpDisconnectConn httpDisconnectStream
#define httpResetClientConn httpResetClientStream
#define httpPrepClientConn httpPrepClientStream

#define httpGetConnContext httpGetStreamContext
#define httpGetConnEventMask httpGetStreamEventMask
#define httpGetConnHost httpGetStreamHost
#define httpSetConnContext httpSetStreamContext
#define httpSetConnHost httpSetStreamHost
#define httpSetConnData httpSetStreamData
#define httpSetConnNotifier httpSetStreamNotifier
#define httpSetConnUser httpSetStreamUser

#define httpClientConn(stream) httpClientStream(stream)
#define httpServerConn(stream) httpServerStream(stream)
#define httpDisconnect(stream) httpDisconnectStream(stream)

/**
    Destroy the request pipeline.
    @description This is called at the conclusion of a request.
    @param stream HttpStream object created via #httpCreateStream
    @ingroup HttpStream
    @stability Internal
 */
PUBLIC void httpClosePipeline(HttpStream *stream);

#define HTTP_REQUEST_TIMEOUT        1
#define HTTP_INACTIVITY_TIMEOUT     2
#define HTTP_PARSE_TIMEOUT          3

/**
    Create a stream object.
    @description Most interactions with the Http library are via a stream object. It is used for server-side
        communications when responding to client requests and it is used to initiate outbound client requests.
        In HTTP/2 networks, connections are multiplexed onto HttpNet objects.
    @param net Network object owning the connection. Can be NULL and a HttpNetwork object will be transparently
        created and defined as HttpStream.net.
    @param peerCreated Set to true if the connection is being created in response to an incoming peer request.
    @returns A new stream object
    @ingroup HttpStream
    @stability Internal
*/
PUBLIC HttpStream *httpCreateStream(HttpNet *net, bool peerCreated);

/**
    Create the receive request pipeline
    @param stream HttpStream object created via #httpCreateStream
    @param route Route object controlling how the pipeline is configured for the request
    @ingroup HttpStream
    @stability Internal
 */
PUBLIC void httpCreateRxPipeline(HttpStream *stream, struct HttpRoute *route);

/**
    Create the transmit request pipeline
    @param stream HttpStream object created via #httpCreateStream
    @param route Route object controlling how the pipeline is configured for the request
    @ingroup HttpStream
    @stability Internal
 */
PUBLIC void httpCreateTxPipeline(HttpStream *stream, struct HttpRoute *route);

/**
    Destroy the stream object
    @description This call closes the connection socket, destroys the connection dispatcher, disconnects the HttpTx and
        HttpRx property objects and removes the connection from the HttpHost list of connections. Thereafter, the
        garbage collector can reclaim all memory. It may be called by client connections at any time from a
        top-level event running on the connection's dispatcher. Server-side code should not need to explicitly
        destroy the connection as it will be done automatically via httpIOEvent. This routine should not be called
        deep within the stack as it will zero the HttpStream.http property to signify the connection is destroyed.
    @param stream HttpStream object created via #httpCreateStream
    @ingroup HttpStream
    @stability Internal
 */
PUBLIC void httpDestroyStream(HttpStream *stream);

/**
    Discard buffered transmit pipeline data
    @param stream HttpStream object created via #httpCreateStream
    @param dir Queue direction. Either HTTP_QUEUE_TX or HTTP_QUEUE_RX.
    @ingroup HttpStream
    @stability Stable
 */
PUBLIC void httpDiscardData(HttpStream *stream, int dir);

/**
    Disconnect the connection's socket
    @description This call will close the socket and signal a connection error by setting connError.
        Subsequent use of the connection socket will not be possible. It will also set HttpRx.eof and will finalize
        the request. Used internally when a connection times out and for abortive errors.
        This should not be generally used. Rather, #httpDestroyStream and #httpError should be used in preference.
    @param stream HttpStream stream object created via #httpCreateStream
    @ingroup HttpStream
    @stability Internal
  */
PUBLIC void httpDisconnectStream(HttpStream *stream);

/**
    Enable Multipart-Mime File Upload for this request. This will define a "Content-Type: multipart/form-data..."
    header and will create a mime content boundary for use to delimit the various upload content files and fields.
    @param stream HttpStream stream object
    @ingroup HttpStream
    @stability Stable
 */
PUBLIC void httpEnableUpload(HttpStream *stream);

/**
    Error handling for the connection.
    @description The httpError call is used to flag the current request as failed. If httpError is called multiple
        times, those calls are ignored and only the first call to httpError has effect.
        This call will discard all data in the output pipeline queues. If some data has already been written to the
        client the connection will be aborted so the client can get some indication that an error has occurred after the
        headers have been transmitted.
    @param stream HttpStream stream object created via #httpCreateStream
    @param status Http status code. The status code can be ored with the flags HTTP_ABORT to immediately abort
        the connection or HTTP_CLOSE to close the connection at the completion of the request.
    @param fmt Printf style formatted string
    @param ... Arguments for fmt
    @ingroup HttpStream
    @stability Stable
 */
PUBLIC void httpError(HttpStream *stream, int status, cchar *fmt, ...) PRINTF_ATTRIBUTE(3,4);

/**
    Emit an error message for limit violations
    @param stream HttpStream stream object created via #httpCreateStream
    @param status Http status code. The status code can be ored with the flags HTTP_ABORT to immediately abort the
        connection or HTTP_CLOSE to close the connection at the completion of the request.
    @param fmt Printf style formatted string
    @ingroup HttpStream
    @stability Evolving
 */
PUBLIC void httpLimitError(HttpStream *stream, int status, cchar *fmt, ...) PRINTF_ATTRIBUTE(3,4);

/**
    Emit an error message for a badly formatted request
    @param stream HttpStream stream object created via #httpCreateStream
    @param status Http status code. The status code can be ored with the flags HTTP_ABORT to immediately abort
        the connection or HTTP_CLOSE to close the connection at the completion of the request.
    @param fmt Printf style formatted string
    @ingroup HttpStream
    @stability Evolving
 */
PUBLIC void httpBadRequestError(HttpStream *stream, int status, cchar *fmt, ...) PRINTF_ATTRIBUTE(3,4);

/**
    Get the preferred chunked size for transfer chunk encoding.
    @param stream HttpStream stream object created via #httpCreateStream
    @return Chunk size. Returns "zero" if not yet defined.
    @ingroup HttpStream
    @stability Stable
 */
PUBLIC ssize httpGetChunkSize(HttpStream *stream);

/**
    Get the connection context object
    @param stream HttpStream object created via #httpCreateStream
    @return The connection context object defined via httpSetStreamContext
    @ingroup HttpStream
    @stability Stable
 */
PUBLIC void *httpGetStreamContext(HttpStream *stream);

/**
    Get an IO event mask for events of interest to the connection
    @param stream HttpStream object created via #httpCreateStream
    @return Mask of MPR_READABLE and MPR_WRITABLE events.
    @ingroup HttpStream
    @stability Evolving
 */
PUBLIC int httpGetStreamEventMask(HttpStream *stream);

/**
    Get the connection host object
    @param stream HttpStream object created via #httpCreateStream
    @return The connection host object defined via httpSetstreamHost
    @ingroup HttpStream
    @stability Stable
 */
PUBLIC void *httpGetStreamHost(HttpStream *stream);

/**
    Get the error message associated with the last request.
    @description Error messages may be generated for internal or client side errors.
    @param stream HttpStream stream object created via #httpCreateStream
    @return A error string. The caller must not free this reference.
    @ingroup HttpStream
    @stability Stable
 */
PUBLIC cchar *httpGetError(HttpStream *stream);

/**
    Get a URI extension
    @description If the URI has no extension and the response content filename (HttpTx.filename) has been calculated,
        it will be tested for an extension.
    @param stream HttpStream stream object created via #httpCreateStream
    @return The URI extension without the leading period.
    @ingroup HttpStream
    @stability Stable
  */
PUBLIC char *httpGetExt(HttpStream *stream);

/**
    Get the count of Keep-Alive requests that will be used for this stream object.
    @description Http Keep-Alive means that the TCP/IP connection is preserved accross multiple requests. This
        typically means much higher performance and better response. Http Keep-Alive is enabled by default
        for Http/1.1 (the default). Disable Keep-Alive when talking to old, broken HTTP servers.
    @param stream HttpStream stream object created via #httpCreateStream
    @return The maximum count of Keep-Alive requests.
    @ingroup HttpStream
    @stability Stable
 */
PUBLIC int httpGetKeepAliveCount(HttpStream *stream);

/**
    Get the count of bytes buffered on the write queue.
    @param stream HttpStream stream object created via #httpCreateStream
    @return The number of bytes buffered.
    @ingroup HttpStream
    @stability Stable
 */
PUBLIC ssize httpGetWriteQueueCount(HttpStream *stream);

#if DOXYGEN
/**
    Test if the connection is a server-side connection
    @param stream HttpStream stream object created via #httpCreateStream
    @return true if the connection is server-side
    @ingroup HttpStream
    @stability Stable
  */
PUBLIC bool httpServerStream(HttpStream *stream);

/**
    Test if the connection is a client-side connection
    @param stream HttpStream stream object created via #httpCreateStream
    @return true if the connection is client-side
    @ingroup HttpStream
    @stability Stable
  */
PUBLIC bool httpClientStream(HttpStream *stream);
#else
#define httpServerStream(stream) (stream && stream->net && stream->net->endpoint)
#define httpClientStream(stream) (stream && stream->net && !stream->net->endpoint)
#endif

/**
    Match the HttpHost object that should serve this request
    @description This selects the appropriate host object for this request. If no suitable host can be found, #httpError
        will be called and stream->error will be set.
    @param net Network object created via #httpCreateNet
    @param hostname Host name to select.
    @return Host object to serve the request. Also sets stream->host.
    @ingroup HttpStream
    @stability Internal
  */
PUBLIC struct HttpHost *httpMatchHost(HttpNet *net, cchar *hostname);

/**
    Match the HttpHost object that should serve this request
    @description This selects the appropriate SSL configuration for the request.
    @param sp Socket object
    @param hostname Host name to select.
    @return SSL configuration object.
    @ingroup HttpStream
    @stability Internal
  */
PUBLIC MprSsl *httpMatchSsl(MprSocket *sp, cchar *hostname);

/**
    Signal a memory allocation error in a response to the peer
    @param stream HttpStream stream object created via #httpCreateStream
    @ingroup HttpStream
    @stability Stable
 */
PUBLIC void httpMemoryError(HttpStream *stream);

/**
    Inform notifiers of a connection event or state change.
    @description This is an internal API and should not be called by handler or user code.
    @param stream HttpStream object created via #httpCreateStream
    @param event Event to issue
    @param arg Argument to event
    @ingroup HttpStream
    @stability Stable
 */
PUBLIC void httpNotify(HttpStream *stream, int event, int arg);

#define HTTP_NOTIFY(stream, event, arg) \
    if (1) { \
        if (stream->notifier) { \
            httpNotify(stream, event, arg); \
        } \
    } else

/**
    Do setup after an I/O event to receive future events.
    @param stream HttpStream object created via #httpCreateStream
    @ingroup HttpStream
    @stability Internal
 */
PUBLIC void httpAfterEvent(HttpStream *stream);

/**
    Prepare a client connection for a new request.
    @param stream HttpStream object created via #httpCreateStream
    @param keepHeaders If true, keep the headers already defined on the stream object
    @ingroup HttpStream
    @stability Internal
 */
PUBLIC void httpResetClientStream(HttpStream *stream, bool keepHeaders);
#define httpPrepClientStream(stream, keepHeaders) httpResetClientStream(stream, keepHeaders)

/**
    Run the handler ready callback.
    @description This will be called when all incoming data for the request has been fully received.
    @param stream HttpStream object created via #httpCreateStream
    @ingroup HttpStream
    @stability Internal
 */
PUBLIC void httpReadyHandler(HttpStream *stream);

/**
    Test if a directory listing should be rendered for the request.
    @param stream Stream object
    @return True if a directory listing is configured to be rendered for this request.
    @ingroup HttpStream
    @stability Internal
    @internal
 */
PUBLIC bool httpShouldRenderDirListing(HttpStream *stream);

/**
    Test if a request has exceeded its timeout limits
    @description This tests the request against the HttpLimits.requestTimeout and HttpLimits.inactivityTimeout limits.
    It uses the HttpStream.started and HttpStream.lastActivity time markers.
    @param stream HttpStream object created via #httpCreateStream
    @param timeout Overriding timeout in milliseconds. If timeout is zero, override default limits and wait forever.
        If timeout is < 0, use default connection inactivity and duration timeouts. If timeout is > 0, then use this
        timeout as an additional timeout.
    @ingroup HttpStream
    @stability Stable
 */
PUBLIC bool httpRequestExpired(HttpStream *stream, MprTicks timeout);

/**
    Reset the current security credentials
    @description Remove any existing security credentials.
    @param stream HttpStream stream object created via #httpCreateStream
    @ingroup HttpStream
    @stability Stable
 */
PUBLIC void httpResetCredentials(HttpStream *stream);

/**
    Route the request and select that matching route and handle to process the request.
    @param stream HttpStream stream object created via #httpCreateStream
    @ingroup HttpStream
    @stability Internal
  */
PUBLIC void httpRouteRequest(HttpStream *stream);

/**
    Schedule a connection timeout event on a connection
    @description This call schedules an event to run serialized on the connection dispatcher. When run, it will
        cancels the current request, disconnects the socket and issues an error to the error log.
        This call is normally invoked by the httpTimer which runs regularly to check for timed out requests.
    @param stream HttpStream stream object created via #httpCreateStream
    @ingroup HttpStream
    @stability Internal
  */
PUBLIC void httpStreamTimeout(HttpStream *stream);

/**
    Set the chunk size for transfer chunked encoding. When set, a "Transfer-Encoding: Chunked" header will
    be added to the request, and all write data will be broken into chunks of the requested size.
    @param stream HttpStream stream object created via #httpCreateStream
    @param size Requested chunk size.
    @ingroup HttpStream
    @stability Stable
 */
PUBLIC void httpSetChunkSize(HttpStream *stream, ssize size);

/**
    Set the connection context object
    @param stream HttpStream object created via #httpCreateStream
    @param context New context object. Must be a managed memory reference.
    @ingroup HttpStream
    @stability Stable
 */
PUBLIC void httpSetStreamContext(HttpStream *stream, void *context);

/**
    Set the connection data field
    @description The HttpStream.data field is a managed reference that applications can use to retain their
        own per connection state. It will be marked for GC retention by Http.
        See also HttpStream.reqData and HttpQueue.stageData;
    @param stream HttpStream object created via #httpCreateStream
    @param data Data object to associate with the connection. Must be a managed memory reference.
    @stability Stable
 */
PUBLIC void httpSetStreamData(HttpStream *stream, void *data);

/**
    Set the connection host object
    @param stream HttpStream object created via #httpCreateStream
    @param host New context host
    @ingroup HttpStream
    @stability Stable
 */
PUBLIC void httpSetStreamHost(HttpStream *stream, void *host);

/**
    Define a notifier callback for this connection.
    @description The notifier callback will be invoked for state changes and I/O events as Http requests are processed.
    The notifier is invoked when transitioning to these states. i.e. the activities for these states may not yet be complete.
    The supported events are:
    <ul>
    <li>HTTP_EVENT_STATE &mdash; The request is changing state. Valid states are:
        HTTP_STATE_BEGIN, HTTP_STATE_CONNECTED, HTTP_STATE_FIRST, HTTP_STATE_CONTENT, HTTP_STATE_READY,
        HTTP_STATE_RUNNING, HTTP_STATE_FINALIZED and HTTP_STATE_COMPLETE. A request will always visit all states and the
        notifier will be invoked for each and every state. This is true even if the request has no content, the
        HTTP_STATE_CONTENT will still be visited.</li>
    <li>HTTP_EVENT_READABLE &mdash; There is data available to read</li>
    <li>HTTP_EVENT_WRITABLE &mdash; The outgoing pipeline can absorb more data. The WRITABLE event is issued when the
        outgoing pipeline is empties and can absorb more data.</li>
    <li>HTTP_EVENT_ERROR &mdash; The request has encountered an error</li>
    <li>HTTP_EVENT_DESTROY &mdash; The HttpStream object is about to be destoyed</li>
    <li>HTTP_EVENT_APP_OPEN &mdash; The application layer is now open</li>
    <li>HTTP_EVENT_APP_CLOSE &mdash; The application layer is now closed</li>
    </ul>
    @param stream HttpStream stream object created via #httpCreateStream
    @param notifier Notifier function.
    @ingroup HttpStream
    @stability Stable
 */
PUBLIC void httpSetStreamNotifier(HttpStream *stream, HttpNotifier notifier);

/**
    Set the logged in user associated with the connection
    @param stream HttpStream stream object created via #httpCreateStream
    @param user User object
    @ingroup HttpStream
    @stability Stable
 */
PUBLIC void httpSetStreamUser(HttpStream *stream, struct HttpUser *user);

/**
    Set the Http credentials
    @description Define a user and password to use with Http authentication for sites that require it. This will
        be used for the next client connection.
    @param stream HttpStream stream object created via #httpCreateStream
    @param user String user
    @param password Decrypted password string
    @param authType Authentication type. Set to basic or digest. Defaults to nothing.
    @ingroup HttpStream
    @stability Stable
 */
PUBLIC void httpSetCredentials(HttpStream *stream, cchar *user, cchar *password, cchar *authType);

/**
    Set the "fileHandler" to process the request
    @description This is used by handlers to relay file requests to the file handler. Should be called from the other
        handlers start entry point.
    @param stream HttpStream stream object created via #httpCreateStream
    @param path Optional filename to serve. If null, use HttpTx.filename.
    @ingroup HttpStream
    @stability Evolving
 */
PUBLIC void httpSetFileHandler(HttpStream *stream, cchar *path);

/**
    Control Http Keep-Alive for the connection.
    @description Http Keep-Alive means that the TCP/IP connection is preserved accross multiple requests. This
        typically means much higher performance and better response. Http Keep-Alive is enabled by default
        for Http/1.1 (the default). Disable Keep-Alive when talking to old, broken HTTP servers.
    @param stream HttpStream stream object created via #httpCreateStream
    @param count Count of Keep-Alive transactions to use before closing the connection. Set to zero to disable keep-alive.
    @ingroup HttpStream
    @stability Stable
 */
PUBLIC void httpSetKeepAliveCount(HttpStream *stream, int count);

/**
    Set the connection state and invoke notifiers.
    @description The connection states are, in order : HTTP_STATE_BEGIN HTTP_STATE_CONNECTED HTTP_STATE_FIRST
    HTTP_STATE_PARSED HTTP_STATE_CONTENT HTTP_STATE_READY HTTP_STATE_RUNNING HTTP_STATE_FINALIZED HTTP_STATE_COMPLETE.
    When httpSetState advances the state it will invoke any registered #HttpNotifier. If the state is set to a state beyond
        the next intermediate state, the HttpNotifier will be invoked for all intervening states.
        This is true even if the request has no content, the HTTP_STATE_CONTENT will still be visited..
    @param stream HttpStream object created via #httpCreateStream
    @param state New state to enter
    @ingroup HttpStream
    @stability Internal
 */
PUBLIC void httpSetState(HttpStream *stream, int state);

/**
    Set the Http inactivity timeout
    @description Define an inactivity timeout after which the Http connection will be closed.
    @param stream HttpStream object created via #httpCreateStream
    @param requestTimeout Request timeout in msec. This is the total time for the request. Set to -1 to preserve the
        existing value.
    @param inactivityTimeout Inactivity timeout in msec. This is maximum connection idle time. Set to -1 to preserve the
        existing value.
    @ingroup HttpStream
    @stability Stable
 */
PUBLIC void httpSetTimeout(HttpStream *stream, MprTicks requestTimeout, MprTicks inactivityTimeout);

/**
    Define a timestamp in the MPR log file.
    @description This routine initiates the writing of a timestamp in the MPR log file
    @param period Time in milliseconds between timestamps
    @ingroup HttpStream
    @stability Stable
 */
PUBLIC void httpSetTimestamp(MprTicks period);

/**
    Start the pipeline. This starts the request handler.
    @param stream HttpStream object created via #httpCreateStream
    @ingroup HttpStream
    @stability Internal
 */
PUBLIC void httpStartPipeline(HttpStream *stream);

PUBLIC void httpStartHandler(HttpStream *stream);

/**
    Create the pipeline.
    @description Create the processing pipeline.
    @param stream HttpStream object created via #httpCreateStream
    @ingroup HttpStream
    @stability Internal
 */
PUBLIC void httpCreatePipeline(HttpStream *stream);

/**
    Verify the server handshake
    @param stream HttpStream stream object created via #httpCreateStream
    @return True if the handshake is valid
    @ingroup HttpStream
    @stability Evolving
 */
PUBLIC bool httpVerifyWebSocketsHandshake(HttpStream *stream);


/* Internal APIs */
PUBLIC void httpParseMethod(HttpStream *stream);
PUBLIC void httpResetServerStream(HttpStream *stream);
PUBLIC HttpLimits *httpSetUniqueStreamLimits(HttpStream *stream);
PUBLIC void httpInitChunking(HttpStream *stream);

/********************************** HttpAuthStore *********************************/
/**
    AuthStore callback Verify the user credentials
    @param stream HttpStream stream object
    @param username Users login name
    @param password Actual user password
    @return True if the user credentials can validate
    @ingroup HttpAuth
    @stability Evolving
 */
typedef bool (*HttpVerifyUser)(HttpStream *stream, cchar *username, cchar *password);

/**
    Password backend store. Support stores are: system, file
    @ingroup HttpAuth
    @stability Evolving
    @see HttpAskLogin HttpParseAuth httpSetAuthStoreVerify HttpVerifyUser
    httpCreateAuthStore httpSetAuthStore httpsetAuthStoreSessions
 */
typedef struct HttpAuthStore {
    char            *name;                  /**< Authentication password store name: 'system', 'file' */
    int             noSession;              /**< Do not create a session after login */
    HttpVerifyUser  verifyUser;             /**< Default user verification routine */
} HttpAuthStore;

/**
    Add an authorization store for password validation. The pre-supplied types are "config" and "system".
    @description This creates an AuthType object with the defined name and callbacks.
    @param name Unique authorization type name
    @param verifyUser Callback to verify the username and password contained in the HttpStream object passed to the callback.
    @return Auth store if successful, otherwise zero.
    @ingroup HttpAuth
    @stability Stable
 */
PUBLIC HttpAuthStore *httpCreateAuthStore(cchar *name, HttpVerifyUser verifyUser);

/**
    Control whether sessions and session cookies are created for user logins
    @description By default, a session and response cookie are created when a user is authenticated via #httpLogin.
    This boosts performance because subsequent requests can supply the cookie and bypass authentication for each
    subseqent request. This API permits the default behavior to be suppressed and thus no cookie or session will be created.
    @param store AuthStore object created via #httpCreateAuthStore.
    @param noSession Set to true to suppress creation of sessions or cookies.
    @ingroup HttpAuth
    @stability Evolving
 */
PUBLIC void httpSetAuthStoreSessions(HttpAuthStore *store, bool noSession);

/**
    Set the global verify callback for an authentication store
    @description The verification callback is invoked to verify user credentials when authentication is required.
    The callback has the signature: typedef bool (*HttpVerifyUser)(HttpStream *stream, cchar *username, cchar *password);
    @param store AuthStore object allocated by #httpCreateAuthStore.
    @param verifyUser Verification callback
    @ingroup HttpAuth
    @stability Evolving
    @see httpSetAuthVerify
  */
PUBLIC void httpSetAuthStoreVerify(HttpAuthStore *store, HttpVerifyUser verifyUser);

/********************************** HttpAuth *********************************/
/*
    Authorization flags for HttpAuth.flags
 */
#define HTTP_ALLOW_DENY         0x1         /**< Run allow checks before deny checks */
#define HTTP_DENY_ALLOW         0x2         /**< Run deny checks before allow checks */
#define HTTP_AUTH_NO_SESSION    0x4         /**< Do not create a session when authenticated */

#define HTTP_BLOW_ROUNDS        16          /***< Cipher rounds for blowfish encryption */
#define HTTP_BLOW_SALT          16          /***< Bytes of salt for blowfish encryption */

/**
    AuthType callback to generate a response requesting the user login
    This should call httpError if such a response cannot be generated.
    @param stream HttpStream stream object
    @ingroup HttpAuth
    @stability Evolving
 */
typedef void (*HttpAskLogin)(HttpStream *stream);

/**
    AuthType callback to parse the HTTP 'Authorize' (client) and 'www-authenticate' (server) headers
    @description This callback must extract the username and password. The username is set on HttpStream.username.
    The password is returned by this call.
    @param stream HttpStream stream object
    @return The password if successful, otherwise NULL.
    @ingroup HttpAuth
    @stability Evolving
 */
typedef int (*HttpParseAuth)(HttpStream *stream, cchar **username, cchar **password);

/**
    AuthType callback to set the necessary HTTP authorization headers for a client request
    @param stream HttpStream stream object
    @return True if the authorization headers can be set.
    @ingroup HttpAuth
    @stability Evolving
 */
typedef bool (*HttpSetAuth)(HttpStream *stream, cchar *username, cchar *password);

/*
    Flags for AuthType
 */
#define HTTP_AUTH_TYPE_CONDITION     0x1    /**< Use auth condition */

/**
    Authentication Protocol. Supported protocols  are: basic, digest, form.
    @ingroup HttpAuth
    @stability Internal
 */
typedef struct HttpAuthType {
    char                *name;              /**< Authentication protocol name: 'basic', 'digest', 'form' */
    HttpAskLogin        askLogin;           /**< Callback to generate a client login response */
    HttpParseAuth       parseAuth;          /**< Callback to parse request auth details */
    HttpSetAuth         setAuth;            /**< Callback to set the HTTP response authentication headers */
    int                 flags;
} HttpAuthType;

/**
    Authorization
    @description HttpAuth is the foundation authorization object and is used by HttpRoute.
    It stores the authorization configuration information required to determine if a client request should be permitted
    access to a given resource.
    @defgroup HttpAuth HttpAuth
    @see HttpAskLogin HttpAuth HttpAuthType HttpGetCredentials HttpRole HttpSetAuth HttpVerifyUser HttpUser
        HttpVerifyUser httpAddAuthType httpAddRole httpAddUser httpCanUser httpAuthenticate
        httpComputeAllUserAbilities httpComputeUserAbilities httpCreateRole httpCreateAuth httpAdduser
        httpIsAuthenticated httpLogin httpRemoveRole httpRemoveUser httpSetAuthAllow httpSetAuthAnyValidUser
        httpSetAuthUsername httpSetAuthDeny httpSetAuthOrder httpSetAuthPermittedUsers httpSetAuthLogin httpSetAuthQop
        httpSetAuthRealm httpSetAuthRequiredAbilities httpSetAuthType
    @stability Internal
 */
typedef struct HttpAuth {
    struct HttpAuth *parent;                /**< Parent auth */
    char            *cipher;                /**< Encryption cipher */
    char            *realm;                 /**< Realm of access */
    int             flags;                  /**< Authorization flags */
    MprHash         *allow;                 /**< Clients to allow */
    MprHash         *deny;                  /**< Clients to deny */
    MprHash         *userCache;             /**< Cache of authenticated users */
    MprHash         *roles;                 /**< Hash of roles */
    MprHash         *abilities;             /**< Set of required abilities (all are required) */
    MprHash         *permittedUsers;        /**< Set of valid users */
    char            *loginPage;             /**< Web page for user login for 'form' type */
    char            *loggedInPage;          /**< Target URI after logging in */
    char            *loggedOutPage;         /**< Target URI after logging out */
    char            *username;              /**< Automatic login username. Password not required if defined */
    char            *qop;                   /**< Quality of service */
    HttpAuthType    *type;                  /**< Authorization protocol type (basic|digest|form|custom)*/
    HttpAuthStore   *store;                 /**< Authorization password backend (system|file|custom)*/
    HttpVerifyUser  verifyUser;             /**< Password verification */
} HttpAuth;

/**
    Create an authentication object
    @return An empty authentiction object
    @ingroup HttpAuth
    @stability Stable
    @internal
 */
PUBLIC HttpAuth *httpCreateAuth(void);

/**
    Create an authorization protocol type. The pre-supplied types are 'basic', 'digest' and 'form'.
    @description This creates an AuthType with the defined name and callbacks. The basic and digest types are
        supported by most browsers. The form type is implemented via web form requests over HTTP.
    @param name Unique authorization type name
    @param askLogin Callback to generate a client login response
    @param parse Callback to parse the HTTP authentication headers
    @param setAuth Callback to set the HTTP response authentication headers
    @return Zero if successful, otherwise a negative MPR error code
    @ingroup HttpAuth
    @stability Stable
 */
PUBLIC int httpCreateAuthType(cchar *name, HttpAskLogin askLogin, HttpParseAuth parse, HttpSetAuth setAuth);

/**
    Control whether a session and session cookie will be created for user logins for this authentication route
    @description By default, a session and response cookie are created when a user is authenticated via #httpLogin.
    This boosts performance because subsequent requests can supply the cookie and bypass authentication for each
    subseqent request. This API permits the default behavior to be suppressed and thus no cookie or session will be created.
    @param auth Auth object created via #httpCreateAuth.
    @param noSession Set to true to suppress creation of sessions or cookies.
    @ingroup HttpAuth
    @stability Evolving
 */
PUBLIC void httpSetAuthSession(HttpAuth *auth, bool noSession);

/********************************* Users and Roles ***************************/

/**
    User Authorization. A user has a name, password and a set of roles. These roles define a set of abilities.
    @see HttpAuth
    @ingroup HttpAuth
    @stability Internal
 */
typedef struct HttpUser {
    char            *name;                  /**< User name */
    char            *password;              /**< User password for "internal" auth store - (actually the password hash */
    char            *roles;                 /**< Original list of roles */
    MprHash         *abilities;             /**< User abilities */
} HttpUser;

/**
    Authorization Roles. Roles are named sets of abilities.
    @see HttpAuth
    @ingroup HttpAuth
    @stability Internal
 */
typedef struct  HttpRole {
    char            *name;                  /**< Role name */
    MprHash         *abilities;             /**< Role's abilities */
} HttpRole;

/**
    Add a role. If the role already exists, the role is updated.
    @description This creates the role with given abilities. Ability words can also be other roles.
    @param auth Auth object allocated by #httpCreateAuth.
    @param role Role name to add
    @param abilities Space separated list of abilities.
    @return Allocated role object.
    @ingroup HttpAuth
    @stability Stable
 */
PUBLIC HttpRole *httpAddRole(HttpAuth *auth, cchar *role, cchar *abilities);

/**
    Add a user. If the user already exists, the user is updated.
    @description This creates the user and adds the user to the authentication database.
    @param auth Auth object allocated by #httpCreateAuth.
    @param user User name to add
    @param password User password. The password should not be encrypted. The backend will encrypt as required.
    @param abilities Space separated list of abilities.
    @return The User object allocated or NULL for an error.
    @ingroup HttpAuth
    @stability Stable
 */
PUBLIC HttpUser *httpAddUser(HttpAuth *auth, cchar *user, cchar *password, cchar *abilities);

/**
    Lookup a role by name
    @param auth HttpAuth object. Stored in HttpStream.rx.route.auth
    @param name Role name
    @return Role object
    @ingroup HttpAuth
    @stability Evolving
  */
PUBLIC HttpRole *httpLookupRole(HttpAuth *auth, cchar *name);

/**
    Lookup a user by username
    @description This looks up a user in the internal user store.
    This is only used i
    @param auth HttpAuth object. Stored in HttpStream.rx.route.auth
    @param name Username
    @return User object
    @ingroup HttpAuth
    @stability Evolving
  */
PUBLIC HttpUser *httpLookupUser(HttpAuth *auth, cchar *name);

/**
    Remove a role
    @param auth Auth object allocated by #httpCreateAuth.
    @param role Role name to remove
    @return Zero if successful, otherwise a negative MPR error code
    @ingroup HttpAuth
    @stability Stable
    @internal
 */
PUBLIC int httpRemoveRole(HttpAuth *auth, cchar *role);

/**
    Remove a user
    @param auth Auth object allocated by #httpCreateAuth.
    @param user User name to remove
    @return Zero if successful, otherwise a negative MPR error code
    @ingroup HttpAuth
    @stability Stable
    @internal
 */
PUBLIC int httpRemoveUser(HttpAuth *auth, cchar *user);

/**
    Compute all the user abilities for a route using the given auth
    @param auth Auth object allocated by #httpCreateAuth
    @ingroup HttpAuth
    @stability Stable
 */
PUBLIC void httpComputeAllUserAbilities(HttpAuth *auth);

/**
    Compute the user abilities for a given user in a route using the given auth
    @param auth Auth object allocated by #httpCreateAuth
    @param user User object
    @ingroup HttpAuth
    @stability Stable
 */
PUBLIC void httpComputeUserAbilities(HttpAuth *auth, HttpUser *user);

/*
    Internal
 */
PUBLIC char *httpRolesToAbilities(HttpAuth *auth, cchar *roles, cchar *separator);

/********************************* Login *************************************/
/**
    Authenticate a user based on session data
    @description This authenticates a user by testing the user supplied session cookie against login credentials
    stored in the server-side session store. The httpAuthenticate call is not automatically performed by the
    request pipeline. Web Frameworks should call this if required.
    @param stream HttpStream stream object created via #httpCreateStream object.
    @return True if the user is authenticated.
 */
PUBLIC bool httpAuthenticate(HttpStream *stream);

/**
    Test if a user has the required abilities
    @param stream HttpStream stream object created via #httpCreateStream object.
    @param abilities Comma separated list of abilities to test for. If null, then use the required abilities defined
        for the current request route.
    @return True if the user has all the required abilities
    @ingroup HttpAuth
    @stability Stable
 */
PUBLIC bool httpCanUser(HttpStream *stream, cchar *abilities);

/**
    Test if the user is authenticated
    @param stream HttpStream stream object
    @return True if the username and password have been authenticated.
    @ingroup HttpAuth
    @stability Stable
 */
PUBLIC bool httpIsAuthenticated(HttpStream *stream);

/**
    Log the user in.
    @description This will verify the supplied username and password. If the user is successfully logged in,
    the user identity will be stored in session state for fast authentication on subsequent requests.
    Note: this does not verify any user abilities.
    @param stream HttpStream stream object
    @param username User name to authenticate
    @param password Password for the user
    @return True if the username and password have been authenticated.
    @ingroup HttpAuth
    @stability Stable
 */
PUBLIC bool httpLogin(HttpStream *stream, cchar *username, cchar *password);

/**
    Logout the user.
    @param stream HttpStream stream object
    @ingroup HttpAuth
    @stability Evolving
 */
PUBLIC void httpLogout(HttpStream *stream);


/***************************** Auth Route ************************************/
/**
    Allow access by a client IP IP address
    @param auth Authorization object allocated by #httpCreateAuth.
    @param ip Client IP address to allow.
    @ingroup HttpAuth
    @stability Stable
 */
PUBLIC void httpSetAuthAllow(HttpAuth *auth, cchar *ip);

/**
    Allow access by any valid user
    @description This configures the basic or digest authentication for the authorization object
    @param auth Authorization object allocated by #httpCreateAuth.
    @ingroup HttpAuth
    @stability Stable
 */
PUBLIC void httpSetAuthAnyValidUser(HttpAuth *auth);

/**
    Deny access by a client IP address
    @param auth Authorization object allocated by #httpCreateAuth.
    @param ip Client IP address to deny. This must be an IP address string.
    @ingroup HttpAuth
    @stability Stable
 */
PUBLIC void httpSetAuthDeny(HttpAuth *auth, cchar *ip);

/**
    Define login service URLs for use with "form" authentication.
    @description This defines the login form URL and login/out service URLs.
        Set arguments to null if they are not required because the application is implementing its own redirection
        management during login. This API should not be used for web frameworks like ESP or PHP that define their own
        login/out services.
    @param route Route from which to inherit when creating a route for the login pages and services.
    @param loginPage Web page URI for the user to enter username and password.
    @param loginService URI to use for the internal login service. To use your own login URI, set to this the empty string.
    @param logoutService URI to use to log the user out. To use your won logout URI, set this to the empty string.
    @param loggedInPage The client is redirected to this URI once logged in. Use a "referrer:" prefix to the URI to
        redirect the user to the referring URI before the loginPage. If the referrer cannot be determined, the base
        URI is utilized.
    @param loggedOutPage The client is redirected to this URI once logged in. Use a "referrer:" prefix to the URI to
        redirect the user to the referring URI before the loginPage. If the referrer cannot be determined, the base
        URI is utilized.
    @ingroup HttpAuth
    @stability Evolving
 */
PUBLIC void httpSetAuthFormDetails(struct HttpRoute *route, cchar *loginPage, cchar *loginService, cchar *logoutService,
    cchar *loggedInPage, cchar *loggedOutPage);

/**
    Define the login page for use with authentication
    @param auth Authorization object allocated by #httpCreateAuth.
    @param uri URI for the login page. Can use "https:///page" to specify the SSL protocol with the current domain.
    @ingroup HttpAuth
    @stability Evolving
 */
PUBLIC void httpSetAuthLogin(HttpAuth *auth, cchar *uri);

/**
    Set the auth allow/deny order
    @param auth Auth object allocated by #httpCreateAuth.
    @param order Set to HTTP_ALLOW_DENY to run allow checks before deny checks. Set to HTTP_DENY_ALLOW to run deny
        checks before allow.
    @ingroup HttpAuth
    @stability Stable
 */
PUBLIC void httpSetAuthOrder(HttpAuth *auth, int order);

/**
    Define the set of permitted users
    @param auth Auth object allocated by #httpCreateAuth.
    @param users Space separated list of acceptable users.
    @ingroup HttpAuth
    @stability Stable
 */
PUBLIC void httpSetAuthPermittedUsers(HttpAuth *auth, cchar *users);

/**
    Set the required quality of service for digest authentication
    @description This configures the basic or digest authentication for the auth object
    @param auth Auth object allocated by #httpCreateAuth.
    @param qop Quality of service description.
    @ingroup HttpAuth
    @stability Stable
 */
PUBLIC void httpSetAuthQop(HttpAuth *auth, cchar *qop);

/**
    Set the required realm for basic or digest authentication
    @description This configures the authentication realm. The realm is displayed to the user in the browser login
        dialog box.
    @param auth Auth object allocated by #httpCreateAuth.
    @param realm Authentication realm
    @ingroup HttpAuth
    @stability Stable
 */
PUBLIC void httpSetAuthRealm(HttpAuth *auth, cchar *realm);

/**
    Set the required abilities for access
    @param auth Auth object allocated by #httpCreateAuth.
    @param abilities Space separated list of the required abilities. May supply roles in the abilities string.
    @ingroup HttpAuth
    @stability Stable
 */
PUBLIC void httpSetAuthRequiredAbilities(HttpAuth *auth, cchar *abilities);

/**
    Set the authentication password store to use
    @param auth Auth object allocated by #httpCreateAuth.
    @param store Password store to use. Select from: "app", "config" or "system"
    @ingroup HttpAuth
    @stability Stable
 */
PUBLIC int httpSetAuthStore(HttpAuth *auth, cchar *store);

/**
    Set the authentication protocol type to use
    @param auth Auth object allocated by #httpCreateAuth.
    @param proto Protocol name to use. Select from: 'basic', 'digest', 'form' or 'none'. Set to NULL or 'none' to disable
        authentication.
    @param details Extra protocol details.
    @ingroup HttpAuth
    @stability Stable
 */
PUBLIC int httpSetAuthType(HttpAuth *auth, cchar *proto, cchar *details);

/**
    Set an automatic login username
    @description If defined, no password is required and the user will be automatically logged in as this username.
    @param auth Auth object allocated by #httpCreateAuth.
    @param username Username to automatically login with
    @ingroup HttpAuth
    @stability Stable
 */
PUBLIC void httpSetAuthUsername(HttpAuth *auth, cchar *username);

/**
    Set the verify callback for an authentication object that is part of a route.
    @param auth Auth object allocated by #httpCreateAuth.
    @param verifyUser Verification callback
    @ingroup HttpAuth
    @stability Evolving
    @see httpSetAuthStoreVerify
  */
PUBLIC void httpSetAuthVerify(HttpAuth *auth, HttpVerifyUser verifyUser);

/*
    Internal
 */
PUBLIC void httpBasicLogin(HttpStream *stream);
PUBLIC int httpBasicParse(HttpStream *stream, cchar **username, cchar **password);
PUBLIC bool httpBasicSetHeaders(HttpStream *stream, cchar *username, cchar *password);
PUBLIC void httpComputeRoleAbilities(HttpAuth *auth, MprHash *abilities, cchar *role);
PUBLIC HttpAuth *httpCreateInheritedAuth(HttpAuth *parent);
PUBLIC void httpDigestLogin(HttpStream *stream);
PUBLIC int httpDigestParse(HttpStream *stream, cchar **username, cchar **password);
PUBLIC bool httpDigestSetHeaders(HttpStream *stream, cchar *username, cchar *password);
PUBLIC bool httpGetCredentials(HttpStream *stream, cchar **username, cchar **password);
PUBLIC void httpInitAuth(void);
PUBLIC bool httpInternalVerifyUser(HttpStream *stream, cchar *username, cchar *password);
PUBLIC HttpAuthType *httpLookupAuthType(cchar *type);
PUBLIC bool httpPamVerifyUser(HttpStream *stream, cchar *username, cchar *password);

/********************************** HttpLang  ********************************/

#define HTTP_LANG_BEFORE        0x1         /**< Insert suffix before extension */
#define HTTP_LANG_AFTER         0x2         /**< Insert suffix after extension */

/**
    Language definition record for routes
    @ingroup HttpRoute
    @stability Internal
  */
typedef struct HttpLang {
    char        *path;                      /**< Document directory for the language */
    char        *suffix;                    /**< Suffix to add to filenames */
    int         flags;                      /**< Control suffix position */
} HttpLang;

/********************************** HttpCache  *********************************/

#define HTTP_CACHE_CLIENT           0x1     /**< Cache on the client side */
#define HTTP_CACHE_SERVER           0x2     /**< Cache on the server side */
#define HTTP_CACHE_MANUAL           0x4     /**< Cache manually. User must call httpWriteCache */
#define HTTP_CACHE_RESET            0x8     /**< Don't inherit cache config from outer routes */
#define HTTP_CACHE_UNIQUE           0x10    /**< Uniquely cache request with different params */
#define HTTP_CACHE_HAS_PARAMS       0x20    /**< Cache definition has params */
#define HTTP_CACHE_STATIC           0x40    /**< Cache extensions: css, gif, ico, jpg, js, html, pdf, ttf, txt, xml, woff */

/**
    Cache Control
    @description Configuration is not thread safe and must occur at initialization time when the application is
        single threaded.
    If the configuration is modified when the application is multithreaded, all requests must be first be quiesced.
    @defgroup HttpCache HttpCache
    @see HttpCache httpAddCache httpUpdateCache httpWriteCache
    @stability Internal
*/
typedef struct HttpCache {
    MprHash     *extensions;                /**< Extensions to cache */
    MprHash     *methods;                   /**< Methods to cache */
    MprHash     *types;                     /**< MimeTypes to cache */
    MprHash     *uris;                      /**< URIs to cache */
    MprTicks    clientLifespan;             /**< Lifespan for client cached content */
    MprTicks    serverLifespan;             /**< Lifespan for server cached content */
    int         flags;                      /**< Cache control flags */
} HttpCache;

/**
    Add caching for response content
    @description This call configures caching for request responses. Caching may be used for any HTTP method,
    though typically it is most useful for state-less GET requests. Output data may be uniquely cached for requests
    with different request parameters (query, post, and route parameters).
    \n\n
    When server-side caching is requested and manual-mode is not enabled, the request response will be automatically
    cached. Subsequent client requests will revalidate the cached content with the server. If the server-side cached
    content has not expired, a HTTP Not-Modified (304) response will be sent and the client will use its client-side
    cached content.  This results in a very fast transaction with the client as no response data is sent.
    Server-side caching will cache both the response headers and content.
    \n\n
    If manual server-side caching is requested, the response will be automatically cached, but subsequent requests will
    require the handler to explicitly send cached content by calling #httpWriteCached.
    \n\n
    If client-side caching is requested, a 'Cache-Control' Http header will be sent to the client with the caching
    'max-age' set to the lifespan argument value (converted to seconds). This causes the client to serve client-cached
    content and to not contact the server at all until the max-age expires.
    Alternatively, you can use #httpSetHeader to explicitly set a 'Cache-Control' header. For your reference, here are
    some keywords that can be used in the Cache-Control Http header.
    \n\n
        'max-age' Maximum time in seconds the resource is considered fresh.
        's-maxage' Maximum time in seconds the resource is considered fresh from a shared cache.
        'public' marks authenticated responses as cacheable.
        'private' shared caches may not store the response.
        'no-cache' cache must re-submit request for validation before using cached copy.
        'no-store' response may not be stored in a cache.
        'must-revalidate' forces clients to revalidate the request with the server.
        'proxy-revalidate' similar to must-revalidate except only for proxy caches.
    \n\n
    Use client-side caching for static content that will rarely change or for content for which using 'reload' in
    the browser is an adequate solution to force a refresh. Use manual server-side caching for situations where you need to
    explicitly control when and how cached data is returned to the client. For most other situations, use server-side
    caching.
    @param route HttpRoute object
    @param methods List of methods for which caching should be enabled. Set to a comma or space separated list
        of method names. Method names can be any case. Set to null or '*' for all methods. Example:
        'GET, POST'.
    @param uris Set of URIs to cache.
        If the URI is set to '*' all URIs for that action are uniquely cached. If the request has POST data,
        the URI may include such post data in a sorted query format. E.g. {uri: /buy?item=scarf&quantity=1}.
    @param extensions List of document extensions for which caching should be enabled. Set to a comma or space
        separated list of extensions. Extensions should not have a period prefix. Set to null, '' or '*' for all extensions.
        Example: 'html, css, js'. The URI may include request parameters in sorted www-urlencoded format. For example:
        /example.esp?hobby=sailing&name=john.
    @param types List of document mime types for which caching should be enabled. Set to a comma or space
        separated list of types. The mime types are those that correspond to the document extension and NOT the
        content type defined by the handler serving the document. Set to null or '*' for all types.
        Example: image/gif, application/x-php.
    @param clientLifespan Lifespan of client cache items in milliseconds. If not set to positive integer,
        the lifespan will default to the route lifespan.
    @param serverLifespan Lifespan of server cache items in milliseconds. If not set to positive integer,
        the lifespan will default to the route lifespan.
    @param flags Cache control flags. Select HTTP_CACHE_MANUAL to enable manual mode. In manual mode, cached content
        will not be automatically sent. Use #httpWriteCached in the request handler to write previously cached content.
        \n\n
        Select HTTP_CACHE_CLIENT to enable client-side caching. In this mode a 'Cache-Control' Http header will be
        sent to the client with the caching 'max-age'. WARNING: the client will not send any request for this URI
        until the max-age timeout has expired.
        \n\n
        Select HTTP_CACHE_RESET to first reset existing caching configuration for this route.
        \n\n
        Select HTTP_CACHE_SERVER to define the server-side caching mode.
        \n\n
        Select HTTP_CACHE_UNIQUE to uniquely cache requests with different request parameters.
    @return A count of the bytes actually written
    @ingroup HttpCache
    @stability Evolving
 */
PUBLIC void httpAddCache(struct HttpRoute *route, cchar *methods, cchar *uris, cchar *extensions, cchar *types,
        MprTicks clientLifespan, MprTicks serverLifespan, int flags);

/**
    Update the cached content for a URI
    @param stream HttpStream stream object
    @param uri The request URI for which to update the cache. The URI may
        contain the request parameters in sorted www-urlencoded format.
        The URI should include any route prefix.
    @param data Data to cache for the URI. If you wish to cache response headers, include those at the start of the
    data followed by an additional new line.
    @param lifespan Lifespan in milliseconds for the cached content
    @ingroup HttpCache
    @stability Evolving
  */
PUBLIC ssize httpUpdateCache(HttpStream *stream, cchar *uri, cchar *data, MprTicks lifespan);

/**
    Write the cached content for a URI to the client
    @description This call explicitly writes cached content to the client. It is useful when the caching is
        configured in manual mode via the HTTP_CACHE_MANUAL flag to #httpAddCache.
    @param stream HttpStream stream object
    @ingroup HttpCache
    @stability Evolving
  */
PUBLIC ssize httpWriteCached(HttpStream *stream);

/******************************** Action Handler *************************************/
/**
    Action handler callback signature
    @description The Action Handler provides a simple mechanism to bind 'C' callback functions with URIs.
    @param stream HttpStream stream object created via #httpCreateStream
    @defgroup HttpStream HttpStream
    @stability Stable
 */
typedef void (*HttpAction)(HttpStream *stream);

/**
    Define a function procedure to invoke when the specified URI is requested.
    @description This creates the role with given abilities. Ability words can also be other roles.
    @param uri URI to bind with. When this URI is requested, the callback will be invoked if the procHandler is
        configured for the request route.
    @param fun Callback function procedure
    @ingroup HttpAction
    @stability Stable
 */
PUBLIC void httpDefineAction(cchar *uri, HttpAction fun);

/********************************** Streaming **********************************/
/**
    Determine if input body content should be streamed or buffered for requests with content of a given mime type
    @description The mime type and URI are used to match the request.
    @param host Host to modify
    @param mime Mime type to configure
    @param uri URI prefix to match with.
    @return True if input should be streamed. False if it should be buffered.
    @ingroup HttpHost
    @stability Evolving
    @internal
 */
PUBLIC bool httpGetStreaming(struct HttpHost *host, cchar *mime, cchar *uri);

/**
    Control if input body content should be streamed or buffered for requests with content of a given mime type
    @param host Host to modify
    @param mime Mime type to configure
    @param uri URI prefix to match.
    @param streaming Set to true to enable streaming for this mime type.
    @ingroup HttpHost
    @stability Evolving
    @internal
 */
PUBLIC void httpSetStreaming(struct HttpHost *host, cchar *mime, cchar *uri, bool streaming);

/********************************** HttpRoute  *********************************/
/*
    Misc route API flags
 */
#define HTTP_ROUTE_NOT                  0x1         /**< Negate the route pattern test result */
#define HTTP_ROUTE_FREE                 0x2         /**< Free Route.mdata back to malloc when route is freed */
#define HTTP_ROUTE_FREE_PATTERN         0x4         /**< Free Route.patternCompiled back to malloc when route is freed */
#define HTTP_ROUTE_RAW                  0x8         /**< Don't html encode the write data */
#define HTTP_ROUTE_STARTED              0x10        /**< Route initialized */
#define HTTP_ROUTE_XSRF                 0x20        /**< Generate XSRF tokens */
#define HTTP_ROUTE_CORS                 0x40        /**< Cross-Origin resource sharing */
#define HTTP_ROUTE_STEALTH              0x80        /**< Stealth mode */
#define HTTP_ROUTE_SHOW_ERRORS          0x100       /**< Show errors to the client */
#define HTTP_ROUTE_VISIBLE_SESSION      0x200       /**< Create a session cookie visible to client Javascript */
#define HTTP_ROUTE_PRESERVE_FRAMES      0x400       /**< Preserve WebSocket frame boundaries */
#define HTTP_ROUTE_HIDDEN               0x800       /**< Hide this route in route tables. */
#define HTTP_ROUTE_ENV_ESCAPE           0x1000      /**< Escape env vars */
#define HTTP_ROUTE_DOTNET_DIGEST_FIX    0x2000      /**< .NET digest auth omits query in MD5 */
#define HTTP_ROUTE_REDIRECT             0x4000      /**< Redirect secureCondition */
#define HTTP_ROUTE_STRICT_TLS           0x8000      /**< Emit Strict-Transport-Security header */
#define HTTP_ROUTE_HOSTED               0x10000     /**< Route being hosted (appweb) */
#define HTTP_ROUTE_NO_LISTEN            0x20000     /**< Not listening on endpoints */
#define HTTP_ROUTE_PERSIST_COOKIE       0x40000     /**< Persist session cookie to disk */
#define HTTP_ROUTE_OWN_LISTEN           0x80000     /**< Override listening endpoints */
#define HTTP_ROUTE_UTILITY              0x100000    /**< Route hosted by a utility */

/**
    Route Control
    @description Configuration is not thread safe and must occur at initialization time when the application is
        single threaded.
    If the configuration is modified when the application is multithreaded, all requests must be first be quiesced.
    @defgroup HttpRoute HttpRoute
    @see HttpRoute httpAddRouteCondition httpAddRouteErrorDocument
        httpAddRouteFilter httpAddRouteHandler httpAddRouteHeader httpAddRouteLanguageDir httpAddRouteLanguageSuffix
        httpAddRouteLoad httpAddRouteQuery httpAddRouteUpdate httpClearRouteStages httpCreateAliasRoute
        httpCreateDefaultRoute httpCreateInheritedRoute httpCreateRoute httpDefineRoute
        httpDefineRouteCondition httpDefineRouteTarget httpDefineRouteUpdate httpFinalizeRoute httpGetRouteData
        httpGetRouteDocuments httpLookupRouteErrorDocument httpMakePath httpResetRoutePipeline
        httpSetRouteAuth httpSetRouteAutoDelete httpSetRouteConnector httpSetRouteData
        httpSetRouteDefaultLanguage httpSetRouteDocuments httpSetRouteFlags httpSetRouteHandler httpSetRouteHost
        httpSetRouteIndex httpSetRouteMethods httpSetRouteVar httpSetRoutePattern
        httpSetRoutePrefix httpSetRouteScript httpSetRouteSource httpSetRouteTarget httpSetRouteWorkers httpTemplate
        httpTokenize httpTokenizev httpLink httpLinkEx
    @stability Internal
 */
typedef struct HttpRoute {
    /* Ordered for debugging */
    struct HttpRoute *parent;               /**< Parent route */
    char            *pattern;               /**< Original matching URI pattern for the route (includes prefix) */
    char            *startSegment;          /**< First starting literal segment of pattern */
    char            *startWith;             /**< Starting literal portion of pattern */
    char            *optimizedPattern;      /**< Processed pattern (excludes prefix) */
    char            *prefix;                /**< Application scriptName prefix. Set to '' for '/'. Always set */
    char            *tplate;                /**< URI template for forming links based on this route (includes prefix) */
    char            *targetRule;            /**< Target rule */
    char            *target;                /**< Route target details */

    cchar           *documents;             /**< Documents directory */
    cchar           *home;                  /**< Home directory for configuration files */
    char            *envPrefix;             /**< Environment strings prefix */
    MprList         *indexes;               /**< Directory index documents */
    HttpStage       *handler;               /**< Fixed handler */

    int             nextGroup;              /**< Next route with a different startWith */
    int             responseStatus;         /**< Response status code */
    ssize           prefixLen;              /**< Prefix length */
    ssize           startWithLen;           /**< Length of startWith */
    ssize           startSegmentLen;        /**< Prefix length */

    MprJson         *config;                /**< Configuration file content */
    cchar           *mode;                  /**< Application run profile mode (debug|release) */

    cchar           *database;              /**< Name of database for route */
    cchar           *responseFormat;        /**< Client response format */
    cchar           *clientConfig;          /**< Configuration to send to the client */

    bool            debug: 1;               /**< Application running in debug mode */
    bool            error: 1;               /**< Parse or runtime error */
    bool            ignoreEncodingErrors: 1;/**< Ignore UTF8 encoding errors */
    bool            json: 1;                /**< Response format is json */

    MprList         *caching;               /**< Items to cache */
    MprTicks        lifespan;               /**< Default lifespan for all cache items in route */
    HttpAuth        *auth;                  /**< Per route block authentication */
    Http            *http;                  /**< Http service object (copy of appweb->http) */
    struct HttpHost *host;                  /**< Owning host */
    int             flags;                  /**< Route flags */

    char            *defaultLanguage;       /**< Default language */
    MprHash         *extensions;            /**< Hash of handlers by extensions */
    MprList         *handlers;              /**< List of handlers for this route */
    HttpStage       *connector;             /**< Network connector to use */
    MprHash         *map;                   /**< Map of alternate extensions (gzip|minified) */
    MprHash         *data;                  /**< Hash of extra data configuration */
    MprHash         *vars;                  /**< Route variables. Used to expand Path ${token} refrerences */
    MprHash         *languages;             /**< Languages supported */
    MprList         *inputStages;           /**< Input stages */
    MprList         *outputStages;          /**< Output stages */
    MprHash         *errorDocuments;        /**< Set of error documents to use on errors */
    void            *context;               /**< Hosting context (Appweb == EjsPool) */
    void            *eroute;                /**< Extended route information for handler (only) */
    int             autoDelete;             /**< Automatically delete uploaded files */
    int             renameUploads;          /**< Rename uploaded files */

    HttpLimits      *limits;                /**< Host resource limits */
    MprHash         *mimeTypes;             /**< Hash table of mime types (key is extension) */

    HttpTrace       *trace;                 /**< Per-route tracing configuration */

    cchar           *cookie;                /**< Cookie name for session data */
    cchar           *corsOrigin;            /**< CORS permissible client origins */
    cchar           *corsHeaders;           /**< Headers to add for Access-Control-Expose-Headers */
    cchar           *corsMethods;           /**< Methods to add for Access-Control-Allow-Methods */
    bool            corsCredentials;        /**< Whether to emit an Access-Control-Allow-Credentials */
    int             corsAge;                /**< Age in seconds of the pre-flight authorization */

#if DEPRECATED || 1
    /*
        Used by Ejscript
     */
    char            *script;                /**< Startup script for handlers serving this route */
    char            *scriptPath;            /**< Startup script path for handlers serving this route */
    int             workers;                /**< Number of workers to use for this route */
#endif

    MprHash         *methods;               /**< Matching HTTP methods */
    MprList         *params;                /**< Matching param field data */
    MprList         *requestHeaders;        /**< Required request header values */
    MprList         *conditions;            /**< Route conditions */
    MprList         *updates;               /**< Route and request updates */

    void            *patternCompiled;       /**< Compiled pattern regular expression (not alloced) */
    cchar           *source;                /**< Final source for route target */
    cchar           *sourceName;            /**< Source name for route target */
    MprList         *tokens;                /**< Tokens in pattern, {name} */
    MprList         *headers;               /**< Response header values */

    struct MprSsl   *ssl;                   /**< SSL configuration */
    char            *webSocketsProtocol;    /**< WebSockets sub-protocol */
    MprTicks        webSocketsPingPeriod;   /**< Time between pings (msec) */

} HttpRoute;


/**
    Route operation record
    @stability Internal
 */
typedef struct HttpRouteOp {
    char            *name;                  /**< Name of route operation */
    char            *details;               /**< General route operation details */
    char            *var;                   /**< Var to set */
    char            *value;                 /**< Value to assign to var */
    void            *mdata;                 /**< pcre_ data (unmanaged) */
    int             flags;                  /**< Route flags to control freeing mdata */
} HttpRouteOp;

/*
    Route matching return codes
 */
#define HTTP_ROUTE_OK           0           /**< The route matches the request */
#define HTTP_ROUTE_REJECT       1           /**< The route does not match the request */
#define HTTP_ROUTE_REROUTE      2           /**< Request has been modified and must be re-routed */
#define HTTP_ROUTE_OMIT_FILTER  1           /**< Omit filter. Same code as HTTP_ROUTE_REJECT for handlers */

/**
    Http JSON configuration parse callback
    @param route Current route
    @param key Configuration file property key
    @param child Key value as a JSON object
    @ingroup HttpRoute
    @stability Evolving
  */
typedef void (*HttpParseCallback)(struct HttpRoute *route, cchar *key, MprJson *child);

/*
    Emit a parse error message
    @description This aborts processing further configuration.
    @param route Current route
    @fmt Printf style format string
    @ingroup HttpRoute
    @stability Evolving
 */
PUBLIC void httpParseError(HttpRoute *route, cchar *fmt, ...);

/*
    Emit a parse warning message
    @param route Current route
    @fmt Printf style format string
    @ingroup HttpRoute
    @stability Evolving
 */
PUBLIC void httpParseWarn(HttpRoute *route, cchar *fmt, ...);

/**
    General route procedure. Used by targets, conditions and updates.
    @return Zero for success. Otherwise a negative MPR error code.
 */
typedef int (HttpRouteProc)(HttpStream *stream, HttpRoute *route, HttpRouteOp *item);

/**
    RouteSet callback
    @param route Parent route for new routes
    @param name Name of route set to add
    @ingroup HttpRoute
    @stability Evolving
  */
typedef void (*HttpRouteSetProc)(HttpRoute *route, cchar *name);

/**
    Add a configuration file callback for a property key
    @param key Configuration file property key
    @param callback Callback function of type #HttpParseCallback
    @return Returns prior callback function. This should be invoked from the new callback to implemented multiple
        callbacks per key.
    @ingroup HttpRoute
    @stability Evolving
  */
PUBLIC HttpParseCallback httpAddConfig(cchar *key, HttpParseCallback callback);

/**
    Define a route set callback
    @param name Name of the route set
    @param fn Callback function
    @ingroup HttpRoute
    @stability Evolving
  */
PUBLIC HttpRouteSetProc httpDefineRouteSet(cchar *name, HttpRouteSetProc fn);

/**
    Add a route set
    @description This will add a set of routes. It will add a home route and optional routes depending on the route set.
    <table>
        <tr><td>Name</td><td>Method</td><td>Pattern</td><td>Target</td></tr>
        <tr><td>home</td><td>GET,POST,PUT</td><td>^/$</td><td>index.esp</td></tr>
    </table>
    @param route Parent route from which to inherit configuration.
    @param set Route set name to select.
    @ingroup HttpRoute
    @stability Evolving
 */
PUBLIC void httpAddRouteSet(HttpRoute *route, cchar *set);

/**
    Add routes for a resource
    @description This routing adds a set of RESTful routes for a resource. It will add the following routes:
    <table>
        <tr><td>Name</td><td>Method</td><td>Pattern</td><td>Action</td></tr>
        <tr><td>create</td><td>POST</td><td>/NAME(/)*$</td><td>create</td></tr>
        <tr><td>edit</td><td>GET</td><td>/NAME/edit$</td><td>edit</td></tr>
        <tr><td>get</td><td>GET</td><td>/NAME$</td><td>get</td></tr>
        <tr><td>init</td><td>GET</td><td>/NAME/init$</td><td>init</td></tr>
        <tr><td>update</td><td>PUT</td><td>/NAME$</td><td>update</td></tr>
        <tr><td>remove</td><td>DELETE</td><td>/NAME$</td><td>remove</td></tr>
        <tr><td>default</td><td>*</td><td>/NAME/{action}$</td><td>cmd-${action}</td></tr>
    </tr>
    </table>
    @param parent Parent route from which to inherit configuration.
    @param resource Resource name. This should be a lower case, single word, alphabetic resource name.
    @ingroup HttpRoute
    @stability Evolving
 */
PUBLIC void httpAddResource(HttpRoute *parent, cchar *resource);

/**
    Add routes for a permanent resource
    @description This routing adds a set of RESTful routes for a resource. It will add the following routes:
    <table>
        <tr><td>Name</td><td>Method</td><td>Pattern</td><td>Action</td></tr>
        <tr><td>get</td><td>GET</td><td>/NAME$</td><td>get</td></tr>
        <tr><td>update</td><td>PUT</td><td>/NAME$</td><td>update</td></tr>
        <tr><td>default</td><td>*</td><td>/NAME/{action}$</td><td>cmd-${action}</td></tr>
    </tr>
    </table>
    @param parent Parent route from which to inherit configuration.
    @param resource Resource name. This should be a lower case, single word, alphabetic resource name.
    @ingroup HttpRoute
    @stability Evolving
 */
PUBLIC void httpAddPermResource(HttpRoute *parent, cchar *resource);

/**
    Add routes for a group of resources
    @description This routing adds a set of RESTful routes for a resource group. It will add the following routes:
    <table>
        <tr><td>Name</td><td>Method</td><td>Pattern</td><td>Action</td></tr>
        <tr><td>create</td><td>POST</td><td>/NAME(/)*$</td><td>create</td></tr>
        <tr><td>edit</td><td>GET</td><td>/NAME/{id=[0-9]+}/edit$</td><td>edit</td></tr>
        <tr><td>get</td><td>GET</td><td>/NAME/{id=[0-9]+}$</td><td>get</td></tr>
        <tr><td>init</td><td>GET</td><td>/NAME/init$</td><td>init</td></tr>
        <tr><td>list</td><td>GET</td><td>/NAME(/)*$</td><td>list</td></tr>
        <tr><td>remove</td><td>DELETE</td><td>/NAME/{id=[0-9]+}$</td><td>remove</td></tr>
        <tr><td>update</td><td>PUT</td><td>/NAME/{id=[0-9]+}$</td><td>update</td></tr>
        <tr><td>action</td><td>POST</td><td>/NAME/{action}/{id=[0-9]+}$</td><td>${action}</td></tr>
        <tr><td>default</td><td>*</td><td>/NAME/{action}$</td><td>cmd-${action}</td></tr>
    </tr>
    </table>
    @param parent Parent route from which to inherit configuration.
    @param resource Resource name. This should be a lower case, single word, alphabetic resource name.
    @ingroup HttpRoute
    @stability Evolving
 */
PUBLIC void httpAddResourceGroup(HttpRoute *parent, cchar *resource);

/**
    Add a route condition
    @description A route condition is run after matching the route pattern. For a route to be accepted, all conditions
        must match. Route conditions are built-in rules that can be applied to routes.
    @param route Route to modify
    @param name Condition rule to add. Supported conditions are: "auth", "missing", "directory", "exists", and "match".
        The "auth" rule is used internally to implement basic and digest authentication.
        \n\n
        The "missing" rule tests if the target filename is missing. The "missing" rule takes no arguments.
        \n\n
        The "directory" rule tests if the condition argument is a directory. The form of the "directory" rule is:
            "directory pathString". For example: "directory /stuff/${request:pathInfo}.txt"
        \n\n
        The "exists" rule tests if the condition argument is present in the file system. The form of the "exists" rule is:
            "exists pathString". For example: "exists ${request.filename}.gz",
        \n\n
        The match directory tests a regular expression pattern against the rest of the condition arguments. The form of
        the match rule is: "match RegExp string". For example: "match https ${request.scheme}".
    @param details Condition parameters.
        See #httpSetRouteTarget for a list of the token values that can be included in the condition rule details.
    @param flags Set to HTTP_ROUTE_NOT to negate the condition test
    @return "Zero" if successful, otherwise a negative MPR error code.
    @ingroup HttpRoute
    @stability Evolving
 */
PUBLIC int httpAddRouteCondition(HttpRoute *route, cchar *name, cchar *details, int flags);

/**
    Add an error document
    @description This defines an error document to be used when the requested document cannot be found.
        This definition is used by some handlers for error processing.
    @param route Route to modify
    @param status The HTTP status code to use with the error document.
    @param uri URL describing the error document
    @ingroup HttpRoute
    @stability Evolving
 */
PUBLIC void httpAddRouteErrorDocument(HttpRoute *route, int status, cchar *uri);

/**
    Add a route filter
    @description This configures the route pipeline by adding processing filters for a request.
        must match. Route conditions are built-in rules that can be applied to routes.
    @param route Route to modify
    @param name Filter name to add
    @param extensions Request extensions for which the filter will be run. A request extension may come from the URI
        if present or from the corresponding filename.
    @param direction Set to HTTP_STAGE_TX for transmit direction and HTTP_STAGE_RX for receive data flow.
    @return "Zero" if successful, otherwise a negative MPR error code.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC int httpAddRouteFilter(HttpRoute *route, cchar *name, cchar *extensions, int direction);

/**
    Add a route handler
    @description This configures the route pipeline by adding the given handler.
        Must only be called at initialization time for the route.
    @param route Route to modify
    @param name Filter name to add
    @param extensions Request extensions for which the handler will be selected. A request extension may come from the URI
        if present or from the corresponding filename.
    @return Zero if successful, otherwise a negative MPR error code.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC int httpAddRouteHandler(HttpRoute *route, cchar *name, cchar *extensions);

/**
    Set the route index document
    @description Set the name of the index document to serve. Index documents may be served when the request corresponds
        to a directory on the file system.
    @param route Route to modify
    @param path Path name to the index document. If the path is a relative path, it may be joined to the route
        directory to create an absolute path.
    @return A reference to the route data. Otherwise return null if the route data for the given key was not found.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpAddRouteIndex(HttpRoute *route, cchar *path);

/**
    Add a route language directory
    @description This configures the route pipeline by adding the given language content directory.
        When creating filenames for matching requests, the language directory is prepended to the request filename.
    @param route Route to modify
    @param language Language symbolic name. For example: "en" for english.
    @param path File system directory to contain content for matching requests.
    @return Zero if successful, otherwise a negative MPR error code.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC int httpAddRouteLanguageDir(HttpRoute *route, cchar *language, cchar *path);

/**
    Add a route language suffix
    @description This configures the route pipeline by adding the given language for request processing.
        The language definition includes a suffix which will be added to the request filename.
    @param route Route to modify
    @param language Language symbolic name. For example: "en" for english.
    @param suffix Extension suffix to add when creating filenames for the request. For example: "fr" to add to "index.html"
        could produce: "index.fr.html".
    @param flags Set to HTTP_LANG_BEFORE to insert the suffix before the filename extension. Set to HTTP_LANG_AFTER to
        append after the extension. For example: HTTP_LANG_AFTER would produce "index.html.fr".
    @return "Zero" if successful, otherwise a negative MPR error code.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC int httpAddRouteLanguageSuffix(HttpRoute *route, cchar *language, cchar *suffix, int flags);

/**
    Add a route mapping
    @description Route mappings will map the request filename by changing the default extension to the mapped extension.
        This is used primarily to select compressed content.
    @param route Route to modify
    @param extensions Comma separated list of extensions to map. For example: "css,html,js,less,txt,xml"
        Set to "*" or the empty string to match all extensions.
    @param mappings List of new file extensions to consider. This may include a "${1}" token to replace the
        previous extension. The extensions are searched in order and the first matching extensions for which there is
        an existing file will be selected. For example: "${1}.gz, min.${1}.gz, min.${1}".
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpAddRouteMapping(HttpRoute *route, cchar *extensions, cchar *mappings);

/**
    Add HTTP methods for the route
    @description This defines additional HTTP methods for requests to match this route
    @param route Route to modify
    @param methods Set to a comma or space separated list of methods. Can also set to "All" or "*" for all possible
        methods. Typical methods include: "DELETE, GET, OPTIONS, POST, PUT, TRACE". Must be upper case.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpAddRouteMethods(HttpRoute *route, cchar *methods);

/**
    Add a route param check
    @description This configures the route to match a request only if the specified param field matches a specific value.
    @param route Route to modify
    @param field Param field to interrogate
    @param value Header value that will match
    @param flags Set to HTTP_ROUTE_NOT to negate the query test
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpAddRouteParam(HttpRoute *route, cchar *field, cchar *value, int flags);

/**
    Add a request header check
    @description This configures the route to match a request only if the specified header field matches a specific value.
    @param route Route to modify
    @param header Header field to interrogate
    @param value Header value that will match
    @param flags Set to HTTP_ROUTE_NOT to negate the header test
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpAddRouteRequestHeaderCheck(HttpRoute *route, cchar *header, cchar *value, int flags);

/*
    Commands for httpAddRouteResponseHeader
 */
#define HTTP_ROUTE_ADD_HEADER       1
#define HTTP_ROUTE_APPEND_HEADER    2
#define HTTP_ROUTE_REMOVE_HEADER    3
#define HTTP_ROUTE_SET_HEADER       4

/**
    Add a response header
    @description This modifies the response header set
    @param route Route to modify
    @param cmd Set to HTTP_ROUTE_HEADER_ADD to add a header if it is not already present in the response header set.
        Set to HTTP_ROUTE_HEADER_REMOVE to remove a header. Set to HTTP_ROUTE_HEADER_SET to define a header and overwrite any
        prior values. Set to HTTP_ROUTE_HEADER_APPEND to append to an existing header value.
    @param header Header field to interrogate
    @param value Header value that will match
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpAddRouteResponseHeader(HttpRoute *route, int cmd, cchar *header, cchar *value);

/**
    Add a route update rule
    @description This configures the route pipeline by adding processing update rules for a request.
        Updates are built-in rules that can be applied to routes.
    @param route Route to modify
    @param name Update rule to add. Supported update rules include: "cmd", "field" and "lang".
        \n\n
        The "cmd" rule is used to run external commands. For example: "cmd touch /tmp/filename".
        \n\n
        The "param" rule is used to set values in the request param fields. For example: "param priority high".
        \n\n
        The "lang" update rule is used internally to implement the various language options.
        See #httpSetRouteTarget for a list of the token values that can be included in the condition rule details.
    @param details Update rule parameters.
    @param flags Reserved.
    @return "Zero" if successful, otherwise a negative MPR error code.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC int httpAddRouteUpdate(HttpRoute *route, cchar *name, cchar *details, int flags);

/**
    Add a route using the WebSockets filter
    @param route Parent route from which to inherit configuration.
    @param action Name of the action to invoke on the route
    @return The new route object.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC HttpRoute *httpAddWebSocketsRoute(HttpRoute *route, cchar *action);


/**
    Clear the pipeline stages for the route
    @description This resets the configured pipeline stages for the route.
    @param route Route to modify
    @param direction Set to HTTP_STAGE_TX for transmit direction and HTTP_STAGE_RX for receive data flow.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpClearRouteStages(HttpRoute *route, int direction);

/**
    Create a route suitable for use as an alias
    @description The parent supplies the owning host for the route. A route is not added to its owning host until it
        is finalized by calling #httpFinalizeRoute
    @param parent Parent route to inherit from
    @param pattern Pattern to match URIs
    @param path File system directory containing documents for this route
    @param status Http redirect status for matching requests. Set to zero if not using redirects
    @return Allocated HttpRoute object
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC HttpRoute *httpCreateAliasRoute(HttpRoute *parent, cchar *pattern, cchar *path, int status);

/**
    Create a configured route
    @description This creates a route and configures the request pipeline with range, chunk and upload filters.
    @param host HttpHost object owning the route
    @param serverSide Set to "true" if this is a server side route. Set to "false" for client side.
    @return Allocated HttpRoute object
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC HttpRoute *httpCreateConfiguredRoute(struct HttpHost *host, int serverSide);

/**
    Create a default route for a host
    @description When the route is fully configured, it should be finalized which will add it to its owning host.
    @param host HttpHost object owning the route
    @return Allocated HttpRoute object
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC HttpRoute *httpCreateDefaultRoute(struct HttpHost *host);

/**
    Create a route inherited from a parent route
    @description When the route is fully configured, it should be finalized which will add it to its owning host.
    @param route Parent route from which to inherit
    @return Allocated HttpRoute object
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC HttpRoute *httpCreateInheritedRoute(HttpRoute *route);

/**
    Create a route for use with the Action Handler
    @description This call creates a route inheriting from a parent route. The new route is configured for use with the
        actionHandler and the given callback procedure.
    @param parent Parent route from which to inherit
    @param pattern Pattern to match URIs
    @param action Action to invoke
    @return Newly created route
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC HttpRoute *httpCreateActionRoute(HttpRoute *parent, cchar *pattern, HttpAction action);

/**
    Create a route for a host
    @description This call creates a bare route without inheriting from a parent route.
    When the route is fully configured, it should be finalized which will add it to its owning host.
    @param host HttpHost object owning the route
    @return Allocated HttpRoute object
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC HttpRoute *httpCreateRoute(struct HttpHost *host);

/**
    Define a route
    @description This creates a route and then configures it using the given parameters. The route is finalized and
        added to the parent host.
    @param parent Parent route from which to inherit configuration.
    @param methods Http methods for which this route is active
    @param pattern Matching URI pattern for which this route will qualify
    @param target Route target string expression. This is used by handlers to determine the physical or virtual resource
        to serve.
    @param source Source file pattern containing the resource to activate or serve.
    @return Created route.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC HttpRoute *httpDefineRoute(HttpRoute *parent, cchar *methods, cchar *pattern, cchar *target, cchar *source);

/**
    Define a RESTful route
    @description This creates a restful route and then configures it using the given parameters. The route is finalized and
        added to the parent host.
    @param parent Parent route from which to inherit configuration.
    @param methods Http methods for which this route is active
    @param pattern Matching URI pattern for which this route will qualify
    @param target Route target string expression. This is used by handlers to determine the physical or virtual resource
        to serve.
    @param resource Resource basename to use when constructing a source file name.
    @return Created route.
    @ingroup HttpRoute
    @stability Evolving
 */
PUBLIC HttpRoute *httpAddRestfulRoute(HttpRoute *parent, cchar *methods, cchar *pattern, cchar * target, cchar *resource);

/**
    Define a route condition rule
    @description This creates a new condition rule.
    @param name Condition name
    @param proc Condition function to process the condition during route matching.
    @ingroup HttpRoute
    @stability Evolving
 */
PUBLIC void httpDefineRouteCondition(cchar *name, HttpRouteProc *proc);

/**
    Define a route target rule
    @description This creates a new target rule.
    @param name Target name
    @param proc Target function to process the target during route matching.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpDefineRouteTarget(cchar *name, HttpRouteProc *proc);

/**
    Define a route update rule
    @description This creates a new update rule.
    @param name Update name
    @param proc Update function to process the update during route matching.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpDefineRouteUpdate(cchar *name, HttpRouteProc *proc);

/**
    Expand route variables in a string
    @param route Route to modify
    @param str String to expand
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC cchar *httpExpandRouteVars(HttpRoute *route, cchar *str);

/**
    Finalize a route
    @description A route must be finalized to add it to its owning hosts list of routes.
    @param route Route to modify
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpFinalizeRoute(HttpRoute *route);

/**
    Get a route directory variable
    @description This looks up the value of the directory
    @param route Route to modify
    @param name Lower case name of the directory. This should not include the '_DIR' suffix.
    @return Directory path
    @ingroup HttpRoute
    @stability Evolving
 */
PUBLIC cchar *httpGetDir(HttpRoute *route, cchar *name);

/**
    Parse a boolean token
    @param tok Token to parse
    @return True if tok is set to "yes", "on", "true" or "1"
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC bool httpGetBoolToken(cchar *tok);

/**
    Get extra route data
    @description Routes can store extra configuration information indexed by key. This is used by handlers, filters,
        connectors and updates to store additional information on a per-route basis.
    @param route Route to modify
    @param key Unique string key to identify the data.
    @return A reference to the route data. Otherwise return null if the route data for the given key was not found.
    @see httpGetRouteData
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void *httpGetRouteData(HttpRoute *route, cchar *key);

/**
    Get the route documents directory
    @description Routes can define a default directory for documents to serve. This value may be used by
        target rules to calculate the response filename.
    @param route Route to modify
    @return The route documents directory pathname.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC cchar *httpGetRouteDocuments(HttpRoute *route);

/**
    Get the route home directory
    @description Routes can define a home directory for configuration files.
    @param route Route to modify
    @return The route home directory pathname.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC cchar *httpGetRouteHome(HttpRoute *route);

/**
    Get the route method list
    @param route Route to examine
    @return The list of support methods. Return NULL if not method list is defined.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC cchar *httpGetRouteMethods(HttpRoute *route);

/**
    Get a URL path to the top of the route from the current request (rx->pathInfo)
    @param stream Current stream object
    @return A relative URL path to the top of the route. This URL does not contain a trailing "/"
    @ingroup HttpRoute
    @stability Evolving
 */
PUBLIC cchar *httpGetRouteTop(HttpStream *stream);

/**
    Get a path token variable
    @param route Route to get
    @param key Token key value
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC cchar *httpGetRouteVar(HttpRoute *route, cchar *key);

/**
    Graduate the limits from the parent route.
    @description This creates a unique limit structure for the route if it is currently inheriting its parents limits.
    @param route Route to modify
    @param limits Limits to use if graduating.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC HttpLimits *httpGraduateLimits(HttpRoute *route, HttpLimits *limits);

/**
    Hide the route from route tables.
    The route is still active, just not displayed in route tables. This is used to hide
    parent routes that are used just for inheritance for child routes.
    @param route Route to hide
    @param on Set to true to hide the route
    @stability Evolving
 */
PUBLIC void httpHideRoute(HttpRoute *route, bool on);

/**
    Initialize and prepare to load configuration files.
    @ingroup HttpRoute
    @stability Evolving
 */
PUBLIC void httpInitConfig(HttpRoute *route);

/**
    Load a JSON configuration file
    @description This loads the JSON configuration file.
    @param route Parent route to configure
    @param path Filename of the JSON configuration file. If this is a relative path, it will be resolved relative
        to the routes home directory.
    @return 'Zero' if successful, otherwise a negative MPR error code.
    @ingroup HttpRoute
    @stability Evolving
 */
PUBLIC int httpLoadConfig(HttpRoute *route, cchar *path);

/**
    Lookup an error document by HTTP status code
    @description This looks up error documents configured via #httpAddRouteErrorDocument
    @param route Route to modify
    @param status HTTP status code integer
    @return URI associated with the error document for the requested status.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC cchar *httpLookupRouteErrorDocument(HttpRoute *route, int status);

/**
    Make a filename path
    @description This makes a filename by expanding the tokens "${token}" and then normalizing the path. Relative paths
        are resolved relative to the optional dir parameter.
        The supported tokens are:
        <ul>
            <li>DOCUMENTS_DIR - for the default directory containing documents to serve</li>
            <li>HOME_DIR - for the directory containing the web server configuration files</li>
            <li>BIN_DIR - for the shared library directory. E.g. /usr/local/lib/appweb/bin </li>
            <li>OS - for the operating system name. E.g. LINUX, MACOSX, VXWORKS, or WIN</li>
            <li>PRODUCT - for the product name</li>
            <li>VERSION - for the product version. E.g. 4.0.2</li>
        </ul>
        Additional tokens can be defined via #httpSetRouteVar.
    @param route Route to modify
    @param dir Directory to use as a base directory for relative paths.
    @param path Path name to examine
    @return A resolved absolute path name.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC char *httpMakePath(HttpRoute *route, cchar *dir, cchar *path);

/**
    Map a content filename
    @description Test a filename for alternative extension mappings. This is used to server compressed or minified
        content intead of "vanilla" files.
    @param stream HttpStream stream object
    @param filename Base filename.
    @ingroup HttpRoute
    @stability Internal
 */
PUBLIC cchar *httpMapContent(HttpStream *stream, cchar *filename);

/**
    Map the request URI to a filename in physical storage for a handler.
    @description This routine is invoked by handlers to map the request URI to a filename and should be called by handlers
    that serve physical documents. The request URI is resolved relative to the route documents directory.
    If a route language directory is defined, that directory is prefixed to the filename after the route documents directory.
    \n\n
    If route maps have been defined, the filename may be mapped to a preferred compressed or minified filename to serve.
    \n\n
    After computing the filename, this routine calls #httpSetFilename to set the HttpTx.filename, ext, etag and fileInfo
    fields. If a filename has already been defined by a prior call to httpMapFile or #httpSetFilename, this routine will
    do nothing.  To reset a prior filename, use #httpSetFilename with a null argument.
    @param stream HttpStream stream object
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpMapFile(HttpStream *stream);

/**
    Parse all the properties under the given key
    @param route Parent route to configure
    @param key Json property key
    @param prop Json property value
    @ingroup HttpRoute
    @stability Evolving
 */
PUBLIC void httpParseAll(HttpRoute *route, cchar *key, MprJson *prop);

/**
    Remove HTTP methods for the route
    @description This removes supported HTTP methods from this route
    @param route Route to modify
    @param methods Set to a comma or space separated list of methods.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpRemoveRouteMethods(HttpRoute *route, cchar *methods);

/**
    Reset all defined indexes
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpResetRouteIndexes(HttpRoute *route);

/**
    Reset the route pipeline
    @description This completely resets the pipeline and discards inherited pipeline configuration. This resets the
        error documents, expiry cache values, extensions, handlers, input and output stage configuration.
    @param route Route to modify
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpResetRoutePipeline(HttpRoute *route);

/**
    Define a route directory path variable
    @description This creates an upper case route variable with a _DIR suffix for the given name.
    @param route Route to modify
    @param name Name of the directory to define
    @param value Directory path value.
    @ingroup HttpRoute
    @stability Evolving
 */
PUBLIC void httpSetDir(HttpRoute *route, cchar *name, cchar *value);

/**
    Set the route authentication
    @description This defines the authentication configuration for basic and digest authentication for the route.
    @param route Route to modify
    @param auth Authentication object
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpSetRouteAuth(HttpRoute *route, HttpAuth *auth);

/**
    Control file upload auto delete
    @description This controls whether files are auto-deleted after the handler runs to service a request.
    @param route Route to modify
    @param on Set to true to enable auto-delete. Auto-delete is enabled by default.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpSetRouteAutoDelete(HttpRoute *route, bool on);

/**
    Define whether updating a request may compile from source
    @param route Route to modify
    @param on Set to true to enable
    @ingroup HttpRoute
    @stability Prototype
 */
PUBLIC void httpSetRouteCompile(HttpRoute *route, bool on);

/**
    Set the connector to use for a route
    @param route Route to modify
    @param name Connector name to use for this route
    @return "Zero" if successful, otherwise a negative MPR error code.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC int httpSetRouteConnector(HttpRoute *route, cchar *name);

/**
    Set route data
    @description Routes can store extra configuration information indexed by key. This is used by handlers, filters,
        connectors and updates to store additional information on a per-route basis.
    @param route Route to modify
    @param key Unique string to identify the data
    @param data Data object. This must be allocated via mprAlloc.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpSetRouteData(HttpRoute *route, cchar *key, void *data);
PUBLIC void httpSetRouteModuleData(HttpRoute *route, cchar *key, void *data);

/**
    Set the default language for the route
    @description This call defines the default language to serve if the client does not provide an Accept HTTP header
        with language preference instructions.
    @param route Route to modify
    @param language Language symbolic name. For example: "en" for english.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpSetRouteDefaultLanguage(HttpRoute *route, cchar *language);

/**
    Set the route directory
    @description Routes can define a default directory for documents to serve. This value may be used by
        target rules to calculate the response filename.
    @param route Route to modify
    @param path Directory path name for the route content
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpSetRouteDocuments(HttpRoute *route, cchar *path);

/**
    Define a prefix string for environment variables
    @description When mapping URI query parameters and form variables to environment variables, it is
    important to prevent important system variables like SHELL, PATH and IFS being overwritten or
    corrupted. Defining a unique prefix for such parameters ensures they have their own namespace.
    @param route Route to modify
    @param prefix Prefix to use in front of environment variables for URI and form parameters.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpSetRouteEnvPrefix(HttpRoute *route, cchar *prefix);

/**
    Define whether shell special characters are escaped in environment variables
    @description If using shell scripts as CGI programs, it is useful to escape all special shell characters
    to make scripting easier. This will escape (with \) the following characters:
    &;`'\"|*?~<>^()[]{}$\\\n and also on windows \\r%
    @param route Route to modify
    @param on Set to true to enable escaping shell special characters.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpSetRouteEnvEscape(HttpRoute *route, bool on);

/**
    Update the route flags
    @description Low level routine to manipulate the route flags
    @param route Route to modify
    @param flags Flags mask
    @ingroup HttpRoute
    @stability Stable
    @internal
 */
PUBLIC void httpSetRouteFlags(HttpRoute *route, int flags);

/**
    Set the handler to use for a route
    @description This defines the stage handler to use in the request pipline for requests matching this route.
        Note that you can also use httpAddRouteHandler which configures a set of handlers that will match by extension.
    @param route Route to modify
    @param name Handler name to define
    @return "Zero" if successful, otherwise a negative MPR error code.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC int httpSetRouteHandler(HttpRoute *route, cchar *name);

/**
    Set the route directory for configuration files
    @description Routes can define a default directory for configuration files.
    @param route Route to modify
    @param home Directory path name for configuration files
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpSetRouteHome(HttpRoute *route, cchar *home);

/*
    Define the owning host for a route.
    @description WARNING: this should not be called by users.
    @param route Route to modify
    @param host HttpHost object
    @ingroup HttpRoute
    @stability Internal
    @internal
 */
PUBLIC void httpSetRouteHost(HttpRoute *route, struct HttpHost *host);

/**
    Set the route to ignore UTF encoding errors for WebSocket connections
    @param route Route to modify
    @param on Set to true to ignore encoding errors
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpSetRouteIgnoreEncodingErrors(HttpRoute *route, bool on);

/**
    Define the methods for the route
    @description This defines the set of valid HTTP methods for requests to match this route
    @param route Route to modify
    @param methods Set to a comma or space separated list of methods. Can also set to "All" or "*" for all possible
        methods.  Typical methods include: "DELETE, GET, OPTIONS, POST, PUT, TRACE".
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpSetRouteMethods(HttpRoute *route, cchar *methods);

/**
    Set the route session cookie
    @param route Route to modify
    @param cookie Session cookie name
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpSetRouteCookie(HttpRoute *route, cchar *cookie);

/**
    Persist the cookie to disk
    @description By default, browser session cookies are created so they are discarded when the browser exits.
    If persistent cookies are created, they live despite browser restarts
    @param route Route to modify
    @param enable Set to true to enable
    @ingroup HttpRoute
    @stability Evolving
 */
PUBLIC void httpSetRouteCookiePersist(HttpRoute *route, int enable);

/**
    Set the route pattern
    @description This call defines the route regular expression pattern that is used to match against the request URI.
        The route pattern is an enhanced JavaScript-compatibile regular expression. It is enhanced by optionally
        embedding braced tokens "{name}" in the pattern. During request URI matching, these tokens are extracted and
        defined in the request params and are available to the request. The normal regular expression repeat syntax
        also uses "{}". To use the traditional (uncommon) repeat syntax, back quote with "\\".
        Sub-expressions and token expressions are also available in various rules as numbered tokens "$1". For example:
        the pattern "/app/(.*)(\.html)$" will permit a file target "$1.${request.Language=fr}.$2".
    @param route Route to modify
    @param pattern Route regular expression pattern
    @param flags Set to HTTP_ROUTE_NOT to negate the pattern match result
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpSetRoutePattern(HttpRoute *route, cchar *pattern, int flags);

/**
    Set the route prefix
    @description Routes may have a prefix which will be stripped from the request URI if the request matches.
        The prefix is made available as the "${request:prefix}" token and also as the ScriptName via some handlers.
    @param route Route to modify
    @param prefix URI prefix to define for the route.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpSetRoutePrefix(HttpRoute *route, cchar *prefix);

/**
    Set the route to preserve WebSocket frames boundaries
    @description When enabled, the WebSocketFilter will not merge or fragment frames.
    @param route Route to modify
    @param on Set to true perserve frame boundaries
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpSetRoutePreserveFrames(HttpRoute *route, bool on);

/**
    Control the renaming of uploaded filenames
    @param route Route to modify
    @param enable Set to true to enable renaming to the client specified filename. Renaming is disabled by default.
    @ingroup HttpRoute
    @stability Evolving
 */
PUBLIC void httpSetRouteRenameUploads(HttpRoute *route, bool enable);

/**
    Set the script to service the route.
    @description This is used by handlers to add a per-route script for processing.
        Either a literal script or a path to a script filename can be provided.
    @param route Route to modify
    @param script Literal script to execute.
    @param scriptPath Pathname to the script file to execute
    @ingroup HttpRoute
    @stability Stable
    @internal
 */
PUBLIC void httpSetRouteScript(HttpRoute *route, cchar *script, cchar *scriptPath);

/**
    Make session cookies that are visible to javascript.
    @description If not visible, cookies will be created with httponly. This helps reduce the XSS risk as
    Javascripts cannot read the session cookie.
    @param route Route to modify
    @param visible Set to true to create session cookies that are visible to Javascript.
    @ingroup HttpRoute
    @stability Evolving
  */
PUBLIC void httpSetRouteSessionVisibility(HttpRoute *route, bool visible);

/**
    Define whether to show errors to the client
    @param route Route to modify
    @param on Set to true to show errors to the client.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpSetRouteShowErrors(HttpRoute *route, bool on);

/**
    Set the source code module for the route
    @description Some handlers can dynamically load web applications and services to serve requests.
    @param route Route to modify
    @param source Source path or description
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpSetRouteSource(HttpRoute *route, cchar *source);

/**
    Set stealth mode for the route
    @description Stealth mode tries to emit as little information as possible.
    @param route Route to modify
    @param on Set to True to enable stealth mode
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpSetRouteStealth(HttpRoute *route, bool on);

/**
    Set a route target
    @description This configures the route pipeline by defining a route target. The route target is interpreted by
        the selected route handler to process the request.
        Route targets can contain symbolic tokens that are expanded at run-time with their corresponding values. There are
        three classes of tokens:
        <ul>
            <li>System and Route varibles - such as DOCUMENTS_DIR, HOME_DIR, BIN_DIR, PRODUCT, OS, VERSION.</li>
            <li>Route URI tokens - these are the braced tokens in the route pattern.</li>
            <li>Request fields - these are request state and property values.</li>
        </ul>
        System and URI tokens are of the form: "${token}" where "token" is the name of the variable or URI token.
        Request fields are of the form: "${family:name=defaultValue}" where the family defines a set of values.
        If the named field is not present, an optional default value "=defaultValue" will be used instead.
        These supported request field families are:
        <ul>
            <li>header - for request HTTP header values</li>
            <li>param - for request params</li>
            <li>query - for request query field values</li>
            <li>request - for request details</li>
            <li>Any URI pattern  token</li>
        </ul>
        For example: "run ${header:User-Agent}" to select the client's browser string passed in the HTTP headers.
        For example: "run ${field:name}" to select the client's browser string passed in the HTTP headers.
        For example: "run ${name}.html" where {name} was a token in the route pattern.
        For example: "run ${name}.html" where {name} was a token in the route pattern.
        The supported request key names are:
        <ul>
            <li>clientAddress - The client IP address</li>
            <li>clientPort - The client port number</li>
            <li>error - Any request or connection error message</li>
            <li>ext - The request extension</li>
            <li>extraPath - The request extra path after the script extension</li>
            <li>filename - The mapped request filename in physical storage</li>
            <li>language - The selected language for the request</li>
            <li>languageDir - The langauge directory</li>
            <li>host - The host name owning the route for the request</li>
            <li>method - The request HTTP method</li>
            <li>originalUri - The original, pre-decoded URI</li>
            <li>pathInfo - The path portion of the URI after the host and port information</li>
            <li>prefix - The route prefix</li>
            <li>query - The request query information</li>
            <li>reference - The request reference fragment. This is the URI portion after "#"</li>
            <li>scheme - The request protocol scheme. E.g. "http"</li>
            <li>scriptName - The request script or application name</li>
            <li>serverAddress - The server IP address</li>
            <li>serverPort - The server port number</li>
            <li>uri - The full request URI. May be modified by routes, handlers and filters</li>
        </ul>
        Also see #httpMakePath for additional tokens (DOCUMENTS_DIR, HOME_DIR, BIN_DIR, PRODUCT, OS, VERSION).
    @param route Route to modify
    @param name Target rule to add. Supported update rules include:
        "close", "redirect", "run" and "write".
        \n\n
        The "close" rule is used to do abortive closes for the request. This is useful for ward off known security attackers.
        For example: "close immediate". The "close" rule takes no addition parameters.
        \n\n
        The "redirect" rule is used to redirect the request to a new resource. For example: "redirect 302 /tryAgain.html".
        The "redirect" takes the form: "redirect status URI". The status code is used as the HTTP response
        code. The URI can be a fully qualified URI beginning with "http" or it can be a relative URI.
        \n\n
        The "run" target is used to run the configured handler to respond to the request.
        For example: "file ${DOCUMENTS}/${request.uri}.gz".
        \n\n
        The "write" rule is used to write literal data back to the client. For example: "write 200 Hello World\r\n".
        The "write" rule takes the form: "write [-r] status message". Write data is by default HTML encoded to help
        eliminate XSS security exposures. The "-r" option selects "raw" output and bypasses the HTML encoding of the
        write data string.
        \n\n
        WARNING: Take great care when using raw writes with tokens. Write data is not HTML encoded and echoing back to
        raw data to the client can cause XSS and other security issues.
        The status field defines the HTTP status code to use in the response.
    @param details Update rule parameters.
    @return "Zero" if successful, otherwise a negative MPR error code.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC int httpSetRouteTarget(HttpRoute *route, cchar *name, cchar *details);

/**
    Set the route template
    @description Set the route URI template uses when constructing URIs via httpLink.
    @param route Route to modify
    @param tplate URI template to use. Templates may contain embedded tokens "{token}" where the token names correspond
        to the token names in the route pattern.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpSetRouteTemplate(HttpRoute *route, cchar *tplate);

/**
    Define a route variable
    @description This defines a route variable that will be used by #httpMakePath and route conditions,
        updates, headers, fields and targets to expand tokenized expressions "${token}".
    @param route Route to modify
    @param token Name of the token to define
    @param value Value of the token
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpSetRouteVar(HttpRoute *route, cchar *token, cchar *value);

/**
    Define whether updating a cached request is required
    @param route Route to modify
    @param on Set to true to enable
    @ingroup HttpRoute
    @stability Evolving
 */
PUBLIC void httpSetRouteUpdate(HttpRoute *route, bool on);

/**
    Set the default upload directory for file uploads
    @param route Route to modify
    @param dir Directory path
    @ingroup HttpRoute
    @stability Evolving
 */
PUBLIC void httpSetRouteUploadDir(HttpRoute *route, cchar *dir);

/**
    Define the maximum number of workers for a route
    @param route Route to modify
    @param workers Maximum number of workers for this route
    @ingroup HttpRoute
    @stability Stable
    @internal
 */
PUBLIC void httpSetRouteWorkers(HttpRoute *route, int workers);

/**
    Control whether an XSRF token will be emitted during a user login sequence.
    @description The XSRF token is emitted in the HTTP response headers and may be used to match with a
        session XSRF token to mitigate XSS security threats.
    @param route Route to modify
    @param enable Set to true to emit and XSRF header token
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpSetRouteXsrf(HttpRoute *route, bool enable);

/**
    Expand a template string using given options
    @description This expands a string with embedded tokens of the form "${token}" using values from the given options.
    This routine also understands the leading aliases: "~" for the route prefix.
    @param stream HttpStream stream object created via #httpCreateStream
    @param tplate Template string to process
    @param options Hash of option values for embedded tokens.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC char *httpTemplate(HttpStream *stream, cchar *tplate, MprHash *options);

/**
    Tokenize a string based on route data
    @description This is a utility routine to parse a string into tokens given a format specifier.
    Mandatory tokens can be specified with "%" format specifier. Optional tokens are specified with "?" format.
    Supported tokens:
    <ul>
    <li>%B - Boolean. Parses: on/off, true/false, yes/no.</li>
    <li>%N - Number. Parses numbers in base 10.</li>
    <li>%S - String. Removes quotes.</li>
    <li>%P - Path string. Removes quotes and expands ${PathVars}. Resolved relative to host->dir (Home).</li>
    <li>%W - Parse words into a list</li>
    <li>%! - Optional negate. Set value to HTTP_ROUTE_NOT present, otherwise zero.</li>
    </ul>
    Values wrapped in quotes will have the outermost quotes trimmed.
    @param route Route to modify
    @param str String to expand
    @param fmt Format string specifier
    @return True if the string can be successfully parsed.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC bool httpTokenize(HttpRoute *route, cchar *str, cchar *fmt, ...);

/**
    Tokenize a string based on route data
    @description This is a utility routine to parse a string into tokens given a format specifier.
    This call is similar to #httpTokenize but uses a va_list argument.
    @param route Route to modify
    @param str String to expand
    @param fmt Format string specifier
    @param args Varargs argument list
    @return True if the string can be successfully parsed.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC bool httpTokenizev(HttpRoute *route, cchar *str, cchar *fmt, va_list args);

/*
    Internal
 */
PUBLIC int httpStartRoute(HttpRoute *route);
PUBLIC void httpStopRoute(HttpRoute *route);
PUBLIC char *httpExpandVars(HttpStream *stream, cchar *str);

/*********************************** Session ***************************************/

#define HTTP_SESSION_COOKIE     "-http-session-"    /**< Session cookie name */
#define HTTP_SESSION_USERNAME   "__USERNAME__"      /**< Username variable */
#define HTTP_SESSION_IP         "__IP__"            /**< Connection IP address - prevents session hijack */

/**
    Session state object
    @defgroup HttpSession HttpSession
    @see httpAllocSession httpCreateSession httpDestroySession httpGetSession httpGetSessionObj
        httpRemoveSessionVar httpGetSessionID httpSetSessionObj httpSetSessionVar
    @stability Internal
 */
typedef struct HttpSession {
    char            *id;                        /**< Session ID key */
    MprCache        *cache;                     /**< Cache store reference */
    MprTicks        lifespan;                   /**< Session inactivity timeout (msecs) */
    MprHash         *data;                      /**< Intermediate session data before writing to cache */
    int             dirty;                      /**< Session updated and needs saving */
    int             seqno;                      /**< Unique sequence number */
} HttpSession;

/**
    Allocate a new session state object.
    @param stream Http stream object
    @param id Unique session state ID
    @param lifespan Session lifespan in ticks
    @return A session state object
    @ingroup HttpSession
    @stability Internal
 */
PUBLIC HttpSession *httpAllocSession(HttpStream *stream, cchar *id, MprTicks lifespan);

/**
    Create a session object.
    @description This call creates a session object. If one already exists, it is destroyed and a fresh session
        is created. Use httpGetSession to retrieve an existing session object.
    @param stream Http stream object
    @return A session state object
    @ingroup HttpSession
    @stability Internal
 */
PUBLIC HttpSession *httpCreateSession(HttpStream *stream);

/**
    Destroy a session state object.
    This destroys a session. It will emit an expired cookie to force the client to remove the old session cookie.
    @description
    @param stream Http stream object.
    @ingroup HttpSession
    @stability Internal
 */
PUBLIC void httpDestroySession(HttpStream *stream);

/**
    Get a session state object.
    This will optionally create a session if one does not already exist. It will not re-create a session that exists.
    @description
    @param stream Http stream object
    @param create Set to "true" to create a session state object if one does not already exist for this client
    @return A session state object
    @ingroup HttpSession
    @stability Stable
 */
PUBLIC HttpSession *httpGetSession(HttpStream *stream, int create);

/**
    Get the session ID.
    @description
    @param stream Http stream object
    @return The session ID string
    @ingroup HttpSession
    @stability Stable
 */
PUBLIC cchar *httpGetSessionID(HttpStream *stream);

/**
    Get an object from the session state store.
    @description Retrieve an object from the session state store by deserializing all properties.
    @param stream Http stream object
    @param key Session state key
    @ingroup HttpSession
    @stability Stable
 */
PUBLIC MprHash *httpGetSessionObj(HttpStream *stream, cchar *key);

/**
    Get a session state variable.
    @param stream Http stream object
    @param name Variable name to get
    @param defaultValue If the variable does not exist, return the defaultValue.
    @return The variable value or defaultValue if it does not exist.
    @ingroup HttpSession
    @stability Stable
 */
PUBLIC cchar *httpGetSessionVar(HttpStream *stream, cchar *name, cchar *defaultValue);

/**
    Lookup a session ID
    @param id Session ID to lookup.
    @return True if the ID is associated with a session
    @ingroup HttpSession
    @stability Stable
 */
PUBLIC bool httpLookupSessionID(cchar *id);

/**
    Remove a session state variable
    @param stream Http stream object
    @param name Variable name to remove
    @return Zero if successful, otherwise a negative MPR error code.
    @ingroup HttpSession
    @stability Stable
 */
PUBLIC int httpRemoveSessionVar(HttpStream *stream, cchar *name);

/**
    Set a linked managed memory reference for a session.
    @description When the session expires, the linked memory will be eligible for garbage collection.
    This routine is useful to attach objects or memory to a session and have them be released together.
    @param stream Http stream object
    @param link Managed memory reference. May be NULL.
    @ingroup MprCache
    @stability Evolving
 */
PUBLIC int httpSetCacheLink(HttpStream *stream, void *link);

/**
    Set a notification callback to be invoked for session notification events.
    WARNING: the callback may happen on any thread. Use careful locking to synchronize access to data. Take care
            not to block the thread issuing the callback.
    @param notifyProc MprCacheProc notification callback. Invoked for events of interest on cache items.
        The event is set to MPR_CACHE_NOTIFY_REMOVE when items are removed from the cache.  Invoked as:
        \n\n
        (*MprCacheProc)(MprCache *cache, cchar *key, cchar *data, int event);
    @ingroup HttpSession
    @stability Evolving
  */
PUBLIC void httpSetSessionNotify(MprCacheProc notifyProc);

/**
    Set an object into the session state store.
    @description Store an object in the session state store by serializing all properties.
    @param stream Http stream object
    @param key Session state key
    @param value Object to serialize. This must be an MprHash object.
    @ingroup HttpSession
    @stability Stable
 */
PUBLIC int httpSetSessionObj(HttpStream *stream, cchar *key, MprHash *value);

/**
    Set a session variable.
    @description
    @param stream Http stream object
    @param name Variable name to set
    @param value String variable value to use. This must point to a valid null terminated string.
    @return A session state object
    @ingroup HttpSession
    @stability Stable
 */
PUBLIC int httpSetSessionVar(HttpStream *stream, cchar *name, cchar *value);

/**
    Write the session state to persistent data storage
    @description This is called internally by the ESP handler at the completion of any processing.
    @param stream Http stream object
    @stability Evolving
    @ingroup HttpSession
    @internal
 */
PUBLIC int httpWriteSession(HttpStream *stream);

/**
    Check a security token.
    @description Check the request security token against the security token defined in the session state.
    @param stream Http stream object
    @return True if the security token matches the session held token.
    @ingroup HttpSession
    @stability Stable
 */
PUBLIC bool httpCheckSecurityToken(HttpStream *stream);

/**
    Get a unique security token.
    @description This will get an existing security token or create a new token if one does not exist.
        If recreate is true, the security token will be recreated.
        Use #httpAddSecurityToken to add the token to the response headers.
    @param stream HttpStream stream object
    @param recreate Set to true to recreate the security token.
    @return The security token string
    @ingroup HttpSession
    @stability Stable
*/
PUBLIC cchar *httpGetSecurityToken(HttpStream *stream, bool recreate);

/**
    Add the security token to the response.
    @description To minimize form replay attacks, a security token may be required for POST requests on a route.
    This call will set a security token in the response as a response header and as a response cookie.
    Client-side Javascript must then send this token as a request header in subsquent POST requests.
    To configure a route to require security tokens, use #httpSetRouteXsrf.
    @param stream Http stream object
    @param recreate Set to true to recreate the security token.
    @ingroup HttpSession
    @stability Stable
*/
PUBLIC int httpAddSecurityToken(HttpStream *stream, bool recreate);

/********************************** HttpUploadFile *********************************/
/**
    Upload File
    @description Each uploaded file has an HttpUploadedFile entry. This is managed by the upload handler.
    @stability Stable
    @defgroup HttpUploadFile HttpUploadFile
    @see httpAddUploadFile httpRemoveAllUploadedFiles httpRemoveUploadFile
    @stability Internal
 */
typedef struct HttpUploadFile {
    cchar           *name;                  /**< Form field name */
    cchar           *filename;              /**< Local (temp) name of the file */
    cchar           *clientFilename;        /**< Client side name of the file */
    cchar           *contentType;           /**< Content type */
    ssize           size;                   /**< Uploaded file size */
} HttpUploadFile;

/********************************** HttpRx *********************************/
/*
    Rx flags
 */
#define HTTP_DELETE             0x1         /**< DELETE method  */
#define HTTP_GET                0x2         /**< GET method  */
#define HTTP_HEAD               0x4         /**< HEAD method  */
#define HTTP_OPTIONS            0x8         /**< OPTIONS method  */
#define HTTP_POST               0x10        /**< Post method */
#define HTTP_PUT                0x20        /**< PUT method  */
#define HTTP_TRACE              0x40        /**< TRACE method  */
#define HTTP_CREATE_ENV         0x80        /**< Must create env for this request */
#define HTTP_IF_MODIFIED        0x100       /**< If-[un]modified-since supplied */
#define HTTP_CHUNKED            0x200       /**< Content is chunk encoded */
#define HTTP_ADDED_QUERY_PARAMS 0x400       /**< Query added to params */
#define HTTP_ADDED_BODY_PARAMS  0x800       /**< Body data added to params */
#define HTTP_EXPECT_CONTINUE    0x1000      /**< Client expects an HTTP 100 Continue response */

/*
    Incoming chunk encoding states
 */
#define HTTP_CHUNK_UNCHUNKED  0             /**< Data is not transfer-chunk encoded */
#define HTTP_CHUNK_START      1             /**< Start of a new chunk */
#define HTTP_CHUNK_DATA       2             /**< Start of chunk data */
#define HTTP_CHUNK_EOF        3             /**< End of last chunk */

/**
    Http Rx
    @description Most of the APIs in the rx group still take a HttpStream object as their first parameter. This is
        to make the API easier to remember - APIs take a stream object rather than a rx or tx object.
    @defgroup HttpRx HttpRx
    @see HttpStream HttpRx HttpTx httpAddBodyVars httpAddParamsFromBuf httpContentNotModified
        httpCreateCGIParams httpGetContentLength httpGetCookies httpGetParam httpGetParams httpGetHeader
        httpGetHeaderHash httpGetHeaders httpGetIntParam httpGetLanguage httpGetQueryString httpGetReadCount httpGetStatus
        httpGetStatusMessage httpMatchParam httpRead httpReadString httpSetParam httpSetIntParam httpSetUri
        httpTestParam httpTrimExtraPath
    @stability Internal
 */
typedef struct HttpRx {
    /* Ordered for debugging */
    cchar           *method;                /**< Request method */
    cchar           *uri;                   /**< Current URI (not decoded, may be rewritten) */
    cchar           *pathInfo;              /**< Path information after the scriptName (Decoded and normalized) */
    cchar           *scriptName;            /**< ScriptName portion of the uri (Decoded). May be empty or start with "/" */
    cchar           *extraPath;             /**< Extra path information (CGI|PHP) */
    MprOff          bytesUploaded;          /**< Length of uploaded content by user */
    MprOff          bytesRead;              /**< Length of content read by user (includes bytesUloaded) */
    MprOff          length;                 /**< Content length header value (ENV: CONTENT_LENGTH) */
    MprOff          remainingContent;       /**< Remaining content data to read (in next chunk if chunked) */

    HttpStream      *stream;                /**< HttpStream object */
    HttpRoute       *route;                 /**< Route for request */
    HttpSession     *session;               /**< Session for request */
    cchar           *traceId;               /**< Request trace id */
    int             seqno;                  /**< Unique request sequence number */

    MprList         *etags;                 /**< Document etag to uniquely identify the document version */
    MprList         *files;                 /**< List of uploaded files (HttpUploadFile objects) */
    HttpPacket      *headerPacket;          /**< HTTP headers */
    MprHash         *headers;               /**< Header variables */
    MprList         *inputPipeline;         /**< Input processing */
    HttpUri         *parsedUri;             /**< Parsed request uri */
    MprHash         *requestData;           /**< General request data storage. Set via #httpSetStageData */
    MprTime         since;                  /**< If-Modified date */

    int             chunkState;             /**< Chunk encoding state */
    int             flags;                  /**< Rx modifiers */

    bool            authenticateProbed: 1;  /**< Request has been authenticated */
    bool            authenticated: 1;       /**< Request has been authenticated */
    bool            autoDelete: 1;          /**< Automatically delete uploaded files */
    bool            eof: 1;                 /**< All read data has been received (eof) */
    bool            form: 1;                /**< Using mime-type application/x-www-form-urlencoded */
    bool            ifModified: 1;          /**< If-Modified processing requested */
    bool            ifMatch: 1;             /**< If-Match processing requested */
    bool            json: 1;                /**< Using a JSON body */
    bool            needInputPipeline: 1;   /**< Input pipeline required to process received data */
    bool            ownParams: 1;           /**< Do own parameter handling */
    bool            renameUploads: 1;       /**< Rename uploaded files to the client specified filename */
    bool            sessionProbed: 1;       /**< Session has been resolved */
    bool            streaming: 1;           /**< Stream incoming content. Forms typically buffer and dont stream */
    bool            upload: 1;              /**< Request is using file upload */

    /*
        Incoming response line if a client request
     */
    int             status;                 /**< HTTP response status */
    cchar           *statusMessage;         /**< HTTP Response status message */

    /*
        Header values
     */
    cchar           *accept;                /**< Accept header */
    cchar           *acceptCharset;         /**< Accept-Charset header */
    cchar           *acceptEncoding;        /**< Accept-Encoding header */
    cchar           *acceptLanguage;        /**< Accept-Language header */
    cchar           *authDetails;           /**< Header details: authorization|www-authenticate provided by peer */
    cchar           *authType;              /**< Type of authentication: set to basic, digest, post or a custom name */
    cchar           *cookie;                /**< Cookie header - may contain many cookies */
    cchar           *connection;            /**< Connection header */
    cchar           *contentLength;         /**< Content length string value */
    cchar           *hostHeader;            /**< Client supplied host name header */
    cchar           *mimeType;              /**< Mime type of the request payload (ENV: CONTENT_TYPE) */
    cchar           *originalMethod;        /**< Original method from the client */
    cchar           *origin;                /**< Origin header (not used) */
    cchar           *originalUri;           /**< Original URI passed by the client */
    cchar           *paramString;           /**< Cached param data as a string */
    cchar           *pragma;                /**< Pragma header */
    cchar           *passwordDigest;        /**< User password digest for authentication */
    cchar           *redirect;              /**< Redirect route header */
    cchar           *referrer;              /**< Refering URL */
    cchar           *securityToken;         /**< Security form token */
    cchar           *upgrade;               /**< Protocol upgrade header */
    cchar           *userAgent;             /**< User-Agent header */

    HttpLang        *lang;                  /**< Selected language */
    MprJson         *params;                /**< Request params (Query and post data variables) */
    MprHash         *svars;                 /**< Server variables */
    HttpRange       *inputRange;            /**< Specified range for rx (post) data */

    struct HttpWebSocket *webSocket;        /**< WebSocket state */

    /*
        Routing info
     */
    char            *target;                /**< Route target */
    int             matches[ME_MAX_ROUTE_MATCHES * 2];
    int             matchCount;
} HttpRx;

/**
    Add parameters from the request query string.
    @description This adds query data to the request params
    @param stream HttpStream stream object
    @ingroup HttpRx
    @stability Internal
    @internal
 */
PUBLIC void httpAddQueryParams(HttpStream *stream);

/**
    Add parameters from the request body content.
    @description This adds query data to the request params
    @param stream HttpStream stream object
    @return Zero if successful, otherwise a negative MPR error code.
    @ingroup HttpRx
    @stability Internal
    @internal
 */
PUBLIC int httpAddBodyParams(HttpStream *stream);

/**
    Add parameters from a JSON body.
    @description This adds query data and posted body data to the request params
    @param stream HttpStream stream object
    @ingroup HttpRx
    @stability Internal
    @internal
 */
PUBLIC void httpAddJsonParams(HttpStream *stream);

/**
    Test if the content has not been modified
    @description This call tests if the file content to be served has been modified since the client last
        requested this resource. The client must provide an Etag and Since or If-Modified headers.
    @param stream HttpStream stream object
    @return True if the content is current and has not been modified.
    @ingroup HttpRx
    @stability Stable
 */
PUBLIC bool httpContentNotModified(HttpStream *stream);

/**
    Create CGI parameters
    @description This call creates request params corresponding to the standard CGI/1.1 environment variables.
    This is used by the CGI and PHP handlers. It may also be useful to handlers that wish to expose CGI style
    environment variables
    through the form vars interface.
    @param stream HttpStream stream object
    @ingroup HttpRx
    @stability Stable
 */
PUBLIC void httpCreateCGIParams(HttpStream *stream);

/**
    Get the receive body content length
    @description Get the length of the receive body content (if any). This is used in servers to get the length of posted
        data and in clients to get the response body length.
    @param stream HttpStream stream object created via #httpCreateStream
    @return A count of the response content data in bytes.
    @ingroup HttpRx
    @stability Stable
 */
PUBLIC MprOff httpGetContentLength(HttpStream *stream);

/**
    Get a request cookie
    @description Get a request cookie by name
    @param stream HttpStream stream object created via #httpCreateStream
    @param name Name of cookie retrieve
    @return Return the cookie value. Return null if the cookie is not defined.
    @ingroup HttpRx
    @stability Stable
 */
PUBLIC cchar *httpGetCookie(HttpStream *stream, cchar *name);

/**
    Get the request cookies
    @description Get the cookies defined in the current requeset
    @param stream HttpStream stream object created via #httpCreateStream
    @return Return a string containing the cookies sent in the Http header of the last request.
        Return null if there are not cookies defined.
    @ingroup HttpRx
    @stability Stable
 */
PUBLIC cchar *httpGetCookies(HttpStream *stream);

/**
    Get a request param
    @description Get the value of a named request param. Form variables are define via www-urlencoded query or post
        data contained in the request.
    @param stream HttpStream stream object
    @param var Name of the request param to retrieve
    @param defaultValue Default value to return if the variable is not defined. Can be null.
    @return String containing a reference to the the request param's value. Caller should not mutate this value.
        Returns defaultValue if not defined.
    @ingroup HttpRx
    @stability Stable
 */
PUBLIC cchar *httpGetParam(HttpStream *stream, cchar *var, cchar *defaultValue);

/**
    Get the request params table
    @description This call gets the form var table for the current request.
        Query data and www-url encoded form data is entered into the table after decoding.
        Use #mprLookupKey to retrieve data from the table.
    @param stream HttpStream stream object
    @return MprJson JSON object instance containing the form vars
    @ingroup HttpRx
    @stability Stable
 */
PUBLIC MprJson *httpGetParams(HttpStream *stream);

/**
    Get the request params table as a string
    @description This call gets the request params encoded as a string. The params are always in the same order
        regardless of the form parameter order. Request parameters include query parameters, form data and routing
        parameters.
    @param stream HttpStream stream object
    @return A string representation in www-urlencoded format.
    @ingroup HttpRx
    @stability Stable
 */
PUBLIC cchar *httpGetParamsString(HttpStream *stream);

/**
    Get an rx http header.
    @description Get a http request header value for a given header key.
    @param stream HttpStream stream object created via #httpCreateStream
    @param key Name of the header to retrieve.
    @return Value associated with the header key or null if the key did not exist in the request.
    @ingroup HttpRx
    @stability Stable
 */
PUBLIC cchar *httpGetHeader(HttpStream *stream, cchar *key);

/**
    Get the hash table of rx Http headers
    @description Get the internal hash table of rx headers
    @param stream HttpStream stream object created via #httpCreateStream
    @return Hash table. See MprHash for how to access the hash table.
    @ingroup HttpRx
    @stability Stable
 */
PUBLIC MprHash *httpGetHeaderHash(HttpStream *stream);

/**
    Get all the request http headers.
    @description Get all the rx headers. The returned string formats all the headers in the form:
        key: value\\nkey2: value2\\n...
    @param stream HttpStream stream object created via #httpCreateStream
    @return String containing all the headers. The caller must free this returned string.
    @ingroup HttpRx
    @stability Stable
 */
PUBLIC char *httpGetHeaders(HttpStream *stream);

/**
    Get a header string from the given hash.
    @description This returns a set of "key: value" lines in HTTP header format.
    @param hash Hash table to examine
    @ingroup HttpRx
    @stability Internal
    @internal
 */
PUBLIC char *httpGetHeadersFromHash(MprHash *hash);

/**
    Get a form variable as an integer
    @description Get the value of a named form variable as an integer. Form variables are define via
        www-urlencoded query or post data contained in the request.
    @param stream HttpStream stream object
    @param var Name of the form variable to retrieve
    @param defaultValue Default value to return if the variable is not defined. Can be null.
    @return Integer containing the form variable's value
    @ingroup HttpRx
    @stability Stable
 */
PUBLIC int httpGetIntParam(HttpStream *stream, cchar *var, int defaultValue);

/**
    Get the language to use for the request
    @description This call tests if the file content to be served has been modified since the client last
        requested this resource. The client must provide an Etag and Since or If-Modified headers.
    @param stream HttpStream stream object
    @param spoken Hash table of HttpLang records. This is typically route->languages.
    @param defaultLang Default language to use if none specified in the request Accept-Language header.
    @return A HttpLang reference, or null if no language requested or no language found in the spoken table.
    @ingroup HttpRx
    @stability Stable
 */
PUBLIC HttpLang *httpGetLanguage(HttpStream *stream, MprHash *spoken, cchar *defaultLang);

/**
    Get a path extension
    @param path File pathname to examine
    @return The path extension sans "."
    @ingroup HttpRx
    @stability Stable
  */
PUBLIC char *httpGetPathExt(cchar *path);

/**
    Get the request query string
    @description Get query string sent with the current request.
    @param stream HttpStream stream object
    @return String containing a reference to the request query string. Caller should not mutate this value.
    @ingroup HttpRx
    @stability Stable
 */
PUBLIC cchar *httpGetQueryString(HttpStream *stream);

/**
    Get the number of bytes that can be read from the read queue
    @param stream HttpStream stream object
    @return The number of bytes available in the read queue for the connection
    @ingroup HttpRx
    @stability Stable
 */
PUBLIC ssize httpGetReadCount(HttpStream *stream);

/**
    Get the response status
    @param stream HttpStream stream object created via #httpCreateStream
    @return An integer Http response code. Typically 200 is success.
    @ingroup HttpRx
    @stability Stable
 */
PUBLIC int httpGetStatus(HttpStream *stream);

/**
    Get the Http response status message. The Http status message is supplied on the first line of the Http response.
    @param stream HttpStream stream object created via #httpCreateStream
    @returns A Http status message.
    @ingroup HttpRx
    @stability Stable
 */
PUBLIC cchar *httpGetStatusMessage(HttpStream *stream);

/**
    Match a form variable with an expected value
    @description Compare a form variable and return true if it exists and its value matches.
    @param stream HttpStream stream object
    @param var Name of the form variable
    @param expected Expected value to match with
    @return True if the value matches
    @ingroup HttpRx
    @stability Stable
 */
PUBLIC bool httpMatchParam(HttpStream *stream, cchar *var, cchar *expected);

/**
    Read rx body data.
    @description This routine will read body data from the connection read queue (HttpStream.readq) which is at the head
    of the response pipeline.
    \n\n
    This call will block depending on whether the connection is in async or sync mode. Sync mode is
    the default for client connections and async for server connections.
    \n\n
    If in sync mode, this call may block to wait for data. If in async mode, the call will not block and will
    return with whatever data is available.
    \n\n
    In sync mode, this routine may invoke mprYield before blocking to consent for the garbage collector to run. Callers must
    ensure they have retained all required temporary memory before invoking this routine.
    \n\n
    This call will block for at most the timeout specified by the connection inactivity timeout defined in
    HttpStream.limits.inactivityTimeout. Use #httpSetTimeout to change the timeout value.
    \n\n
    Server applications often prefer to access packets directly from the connection readq which offers a higher performance
    interface.

    @param stream HttpStream stream object created via #httpCreateStream
    @param buffer Buffer to receive read data
    @param size Size of buffer.
    @return The number of bytes read. Returns zero for not data. EOF can be detected by testing #httpIsEof.
    @ingroup HttpRx
    @stability Stable
 */
PUBLIC ssize httpRead(HttpStream *stream, char *buffer, ssize size);

/**
    Read a block of rx body data.
    @description This routine will read body data and provide control over blocking and call duration.
    \n\n
    If in blocking mode (the default for client connections), this call may block to wait for data. If in non-blocking
    mode (the default for server connections), the call will not block and will return with whatever data is available.
    The blocking mode is set via the flags parameter.
    \n\n
    In blocking mode, this routine may invoke mprYield before blocking to consent for the garbage collector to run.
    Callers must ensure they have retained all required temporary memory before invoking this routine.
    \n\n
    This call will block for at most the timeout specified by the connection inactivity timeout defined in
    HttpStream.limits.inactivityTimeout. Use #httpSetTimeout to change the timeout value.
    \n\n
    Server applications should not call httpReadBlock in blocking mode as it will consume a valuable thread.
    Rather, server apps should perform non-blocking reads or access packets directly from the connection readq
   which offers a higher performance interface.

    @param stream HttpStream stream object created via #httpCreateStream
    @param buffer Buffer to receive read data
    @param size Size of buffer.
    @param timeout Timeout in milliseconds to wait. Set to -1 to use the default inactivity timeout. Set to zero
        to wait forever.
    @param flags Set to HTTP_BLOCK to wait for data before returning. Set to HTTP_NON_BLOCK to read what is
        available and return without blocking. If set to zero, it will default to HTTP_BLOCK for sync connections
        and HTTP_NON_BLOCK for async connections.
    @return The number of bytes read. Returns zero for not data. EOF can be detected by testing #httpIsEof.
    @ingroup HttpRx
    @stability Evolving
 */
PUBLIC ssize httpReadBlock(HttpStream *stream, char *buffer, ssize size, MprTicks timeout, int flags);

/**
    Get the receive body input
    @description This will return all the body input. The request must have received all input (rx->eof == 1) and
        must not be streaming (rx->streaming).
    @param stream HttpStream stream object created via #httpCreateStream
    @return A string containing the body input.
    @stability Evolving
 */
PUBLIC cchar *httpGetBodyInput(HttpStream *stream);

/**
    Read response data as a string. This will read all rx body and return a string that the caller should free.
    This will block and should not be used in async mode.
    @param stream HttpStream stream object created via #httpCreateStream
    @returns A string containing the rx body. Caller should free.
    @ingroup HttpRx
    @stability Stable
 */
PUBLIC char *httpReadString(HttpStream *stream);

/**
    Remove a request param
    @description Remove the value of a named request param.
    @param stream HttpStream stream object
    @param var Name of the request param to retrieve
    @ingroup HttpRx
    @stability Stable
 */
PUBLIC void httpRemoveParam(HttpStream *stream, cchar *var);

/**
    Set the HttpRx eof condition
    @description This routine should be called rather than setting HttpRx.eof manually. This is because it will advance
        the HttpStream state to HTTP_STATE_FINALIZED if the request and connector have been finalized.
    @param stream HttpStream stream object
    @ingroup HttpRx
    @stability Stable
  */
PUBLIC void httpSetEof(HttpStream *stream);

/**
    Set a request param value
    @description Set the value of a named request param to a string value. Form variables are define via
        www-urlencoded query or post data contained in the request.
    @param stream HttpStream stream object
    @param var Name of the request param to retrieve
    @param value Default value to return if the variable is not defined. Can be null.
    @ingroup HttpRx
    @stability Stable
 */
PUBLIC void httpSetParam(HttpStream *stream, cchar *var, cchar *value);

/**
    Set an integer request param value
    @description Set the value of a named request param to an integer value. Form variables are define via
        www-urlencoded query or post data contained in the request.
    @param stream HttpStream stream object
    @param var Name of the request param to retrieve
    @param value Default value to return if the variable is not defined. Can be null.
    @ingroup HttpRx
    @stability Stable
 */
PUBLIC void httpSetIntParam(HttpStream *stream, cchar *var, int value);

/**
    Set a new HTTP method for processing
    @description This modifies the request method to alter request processing. The original method is preserved in
        the HttpRx.originalMethod field. This is only useful to do before request routing has matched a route.
    @param stream HttpStream stream object
    @param method New method to use.
    @ingroup HttpRx
    @stability Stable
 */
PUBLIC void httpSetMethod(HttpStream *stream, cchar *method);

/**
    Define a request completion callback
    @description This callback is invoked when the request is completed.
    @param callback The callback is invoked with the signature: void callback(HttpStream *stream).
    @ingroup HttpRx
    @stability Evolving
 */
PUBLIC void httpSetRequestCallback(HttpRequestCallback callback);

/**
    Set a new URI for processing
    @description This modifies the request URI to alter request processing. The original URI is preserved in
        the HttpRx.originalUri field. This is only useful to do before request routing has matched a route.
    @param stream HttpStream stream object
    @param uri New URI to use. The URI can be fully qualified starting with a scheme ("http") or it can be
        a partial/relative URI. Missing portions of the URI will be completed with equivalent portions from the
        current URI. For example: if the current request URI was http://example.com:7777/index.html, then
        a call to httpSetUri(stream, "/new.html", 0)  will set the request URI to http://example.com:7777/new.html.
        The request script name will be reset and the pathInfo will be set to the path portion of the URI.
    @return "Zero" if successful, otherwise a negative MPR error code.
    @ingroup HttpRx
    @stability Stable
 */
PUBLIC int httpSetUri(HttpStream *stream, cchar *uri);

/**
    Test if a request param is defined
    @param stream HttpStream stream object
    @param var Name of the request param to retrieve
    @return True if the request param is defined
    @ingroup HttpRx
    @stability Stable
 */
PUBLIC int httpTestParam(HttpStream *stream, cchar *var);

/**
    Trim extra path from the URI
    @description This call trims extra path information after the uri extension. This is used by CGI and PHP.
    The strategy is to heuristically find the script name in the uri. This is assumed to be the original uri
    up to and including first path component containing a "." Any path information after that is regarded as
    extra path.  WARNING: Extra path is an old, unreliable, CGI specific technique. Do not use directories
    with embedded periods.
    @param stream HttpStream stream object
    @ingroup HttpRx
    @stability Stable
 */
PUBLIC void httpTrimExtraPath(HttpStream *stream);

/**
    Process Http requests and responses
    @description the httpProcess function drives the HTTP request and response state machine. Once a request is received from the peer and the HTTP headers have been parsed into HttpStream and HttpTx, the httpProcess() function should be called to drive the request pipeline to process the request.
    \n\n
    The HTTP state machine drives the HttpStream.state through the states from
    HTTP_STATE_BEGIN, HTTP_STATE_CONNECTED, HTTP_STATE_FIRST, HTTP_STATE_PARSED, HTTP_STATE_CONTENT, HTTP_STATE_READY, HTTP_STATE_RUNNING, HTTP_STATE_FINALIZED to HTTP_STATE_COMPLETE. For each state, the notifier defined by #httpSetStreamNotifier will be invoked.
    The state should be in HTTP_STATE_PARSED via #httpProcessHeaders before calling this routine.
    \n\n
    httpProcess is invoked by the HTTP/1 and HTTP/2 filters after they have decoded input packets and whenever the network socket becomes newly writable and can absorb more output data.
    @param q HttpQueue queue object
    @ingroup HttpRx
    @stability Evolving
 */
PUBLIC void httpProcess(HttpQueue *q);

/**
    Process Http headers
    @description the httpProcessHeaders function drives the HTTP request and response state machine. Once a request is received from the peer and the HTTP headers have been parsed into HttpStream and HttpTx, the httpProcessHeaders() function should be called to drive parse the headers fields.
    \n\n
    The HTTP state machine should be in the HTTP_STATE_FIRST state. This routine will advance the state to HTTP_STATE_PARSED.
    \n\n
    httpProcessHeaders is invoked by the HTTP/1 and HTTP/2 filters after they have decoded input packets and whenever the network socket becomes newly writable and can absorb more output data.
    @param q HttpQueue queue object
    @ingroup HttpRx
    @stability Prototype
 */
PUBLIC bool httpProcessHeaders(HttpQueue *q);


/* Internal */
PUBLIC void httpCloseRx(struct HttpStream *stream);
PUBLIC HttpRange *httpCreateRange(HttpStream *stream, MprOff start, MprOff end);
PUBLIC HttpRx *httpCreateRx(HttpStream *stream);
PUBLIC void httpDestroyRx(HttpRx *rx);
PUBLIC bool httpMatchEtag(HttpStream *stream, char *requestedEtag);
PUBLIC bool httpMatchModified(HttpStream *stream, MprTime time);
PUBLIC bool httpProcessCompletion(HttpStream *stream);
PUBLIC void httpProcessWriteEvent(HttpStream *stream);

/********************************** HttpTx *********************************/
/*
    Tx flags
 */
#define HTTP_TX_NO_BODY             0x1     /**< No transmission body, only send headers */
#define HTTP_TX_HEADERS_CREATED     0x2     /**< Response headers have been created */
#define HTTP_TX_USE_OWN_HEADERS     0x8     /**< Skip adding default headers */
#define HTTP_TX_NO_CHECK            0x10    /**< Do not check if the filename is inside the route documents directory */
#define HTTP_TX_NO_LENGTH           0x20    /**< Do not emit a content length (used for TRACE) */
#define HTTP_TX_NO_MAP              0x40    /**< Do not map the filename to compressed or minified alternatives */
#define HTTP_TX_PIPELINE            0x80    /**< Created Tx pipeline */
#define HTTP_TX_HAS_FILTERS         0x100   /**< Has output filters */

/**
    Http Tx
    @description The tx object controls the transmission of data. This may be client requests or responses to
        client requests. Most of the APIs in the Response group still take a HttpStream object as their first parameter.
        This is to make the API easier to remember - APIs take a stream object rather than a rx or
        transmission object.
    @defgroup HttpTx HttpTx
    @see HttpStream HttpRx HttpTx httpAddHeader httpAddHeaderString httpAppendHeader httpAppendHeaderString httpFinalize
    httpConnect httpCreateTx httpDestroyTx httpFinalize httpFlush httpFollowRedirects httpFormatBody httpFormatError
    httpFormatErrorV httpFormatResponse httpFormatResponseBody httpFormatResponsev httpGetQueueData
    httpIsChunked httpIsComplete httpIsOutputFinalized httpNeedRetry httpOmitBody httpRedirect httpRemoveHeader
    httpSetContentLength httpSetContentType httpSetCookie httpSetHeader httpSetHeaderString
    httpSetResponded httpSetStatus httpWait httpWriteHeaders httpWriteUploadData
    @stability Internal
 */
typedef struct HttpTx {
    /* Ordered for debugging */
    HttpUri         *parsedUri;             /**< Client request uri */
    cchar           *filename;              /**< Name of a real file being served (typically pathInfo mapped) */
    int             status;                 /**< HTTP response status */

    bool            endHeaders:1;           /**< Processed all header packets */
    bool            endData:1;              /**< Processed the last data packet */
    bool            finalized:1;            /**< Request response generated and handler processing is complete */
    bool            finalizedConnector:1;   /**< Connector has finished sending the response */
    bool            finalizedInput:1;       /**< Handler has finished processing all input */
    bool            finalizedOutput:1;      /**< Handler or surrogate has finished writing output */
    bool            needChunking:1;         /**< Use chunk encoding */
    bool            pendingFinalize:1;      /**< Call httpFinalize again once the Tx pipeline is created */
    bool            responded:1;            /**< The handler has started to respond. Some output has been initiated. */
    bool            started:1;              /**< Handler has been started */
    uint            flags:16;               /**< Response flags */

    MprOff          bytesWritten;           /**< Bytes written including headers */
    ssize           chunkSize;              /**< Chunk size to use when using transfer encoding. Zero for unchunked. */
    struct HttpStream *stream;              /**< Current HttpStream object */
    MprList         *outputPipeline;        /**< Output processing */
    HttpStage       *connector;             /**< Network connector to send / receive socket data */
    MprHash         *cookies;               /**< Browser cookies */
    MprHash         *headers;               /**< Transmission headers */
    HttpCache       *cache;                 /**< Cache control entry (only set if this request is being cached) */
    MprBuf          *cacheBuffer;           /**< Response caching buffer */
    ssize           cacheBufferLength;      /**< Current size of the cache buffer data */
    cchar           *cachedContent;         /**< Retrieved cached response to send */
    MprOff          entityLength;           /**< Original content length before range subsetting */
    cchar           *errorDocument;         /**< Error document to render */
    cchar           *ext;                   /**< Filename extension */
    char            *etag;                  /**< Unique identifier tag */
    HttpStage       *handler;               /**< Final handler serving the request */
    MprOff          length;                 /**< Transmission content length */
    char            *method;                /**< Client request method GET, HEAD, POST, DELETE, OPTIONS, PUT, TRACE */
    cchar           *mimeType;              /**< Mime type of the request payload (ENV: CONTENT_TYPE) */

    /*
        Range fields
     */
    HttpRange       *outputRanges;          /**< Data ranges for tx data */
    HttpRange       *currentRange;          /**< Current range being fullfilled */
    char            *rangeBoundary;         /**< Inter-range boundary */
    MprOff          rangePos;               /**< Current range I/O position in response data */

    cchar           *altBody;               /**< Alternate transmission for errors */
    int             traceMethods;           /**< Handler methods supported */

    /* File information for file-based handlers */
    MprFile         *file;                  /**< File to be served */
    MprPath         fileInfo;               /**< File information if there is a real file to serve */
    ssize           headerSize;             /**< Size of the header written */

    char            *webSockKey;            /**< Sec-WebSocket-Key header */
} HttpTx;

/**
    Add a header to the transmission using a format string.
    @description Add a header if it does not already exits.
    @param stream HttpStream stream object created via #httpCreateStream
    @param key Http response header key
    @param fmt Printf style formatted string to use as the header key value
    @param ... Arguments for fmt
    @return "Zero" if successful, otherwise a negative MPR error code. Returns MPR_ERR_ALREADY_EXISTS if the header already
        exists.
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC void httpAddHeader(HttpStream *stream, cchar *key, cchar *fmt, ...) PRINTF_ATTRIBUTE(3,4);

/**
    Add a header to the transmission
    @description Add a header if it does not already exits.
    @param stream HttpStream stream object created via #httpCreateStream
    @param key Http response header key
    @param value Value to set for the header
    @return Zero if successful, otherwise a negative MPR error code. Returns MPR_ERR_ALREADY_EXISTS if the header already
        exists.
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC void httpAddHeaderString(HttpStream *stream, cchar *key, cchar *value);

/**
    Append a transmission header
    @description Set the header if it does not already exists. Append with a ", " separator if the header already exists.
    @param stream HttpStream stream object created via #httpCreateStream
    @param key Http response header key
    @param fmt Printf style formatted string to use as the header key value
    @param ... Arguments for fmt
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC void httpAppendHeader(HttpStream *stream, cchar *key, cchar *fmt, ...) PRINTF_ATTRIBUTE(3,4);

/**
    Append a transmission header string
    @description Set the header if it does not already exists. Append with a ", " separator if the header already exists.
    @param stream HttpStream stream object created via #httpCreateStream
    @param key Http response header key
    @param value Value to set for the header
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC void httpAppendHeaderString(HttpStream *stream, cchar *key, cchar *value);

/**
    Connect to a server and issue Http client request.
    @description Start a new Http request on the http object and return. This routine does not block.
        After starting the request, you can use #httpWait to wait for the request to achieve a certain state or to complete.
    @param stream HttpStream stream object created via #httpCreateStream
    @param method Http method to use. Valid methods include: "GET", "POST", "PUT", "DELETE", "OPTIONS" and "TRACE"
    @param uri URI to fetch
    @param ssl SSL configuration to use if a secure connection.
    @return "Zero" if the request was successfully sent to the server. Otherwise a negative MPR error code is returned.
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC int httpConnect(HttpStream *stream, cchar *method, cchar *uri, struct MprSsl *ssl);

/**
    Create the tx object. This is used internally by the http library.
    @param stream HttpStream stream object created via #httpCreateStream
    @param headers Optional headers to use for the transmission
    @returns A tx object
    @ingroup HttpTx
    @stability Internal
 */
PUBLIC HttpTx *httpCreateTx(HttpStream *stream, MprHash *headers);

/**
    Destroy the tx object
    @description This is called when the garbage collector frees a connection. It should not be called manually.
    @param tx Tx object
    @ingroup HttpTx
    @stability Internal
 */
PUBLIC void httpDestroyTx(HttpTx *tx);

/**
    Indicate the request is finalized.
    @description Calling this routine indicates that the handler has fully finished processing the request including
        processing all input, generating a full response and any other required processing. This call will invoke
        #httpFinalizeOutput and then set the request finalized flag. If the request is already finalized, this call
        does nothing. A handler MUST call httpFinalize when it has completed processing a request.
        As background: there are three finalize concepts: HttpTx.finalizedOutput means the handler has generated all
        the response output but it may not yet be fully transmited through the pipeline and to the network by the
        connector. HttpTx.finalizedConnector means the connector has sent all the output to the network.  HttpTx.finalized
        means the application has fully processed the request including reading all the input data it wishes to read
        and has generated all the output that will be generated. A fully finalized request has both HttpTx.finalized
        and HttpTx.finalizedConnector true.
    @param stream HttpStream stream object
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC void httpFinalize(HttpStream *stream);

/**
    Finalize connector output sending the response.
    @description This should only be called by a connector.
    @param stream HttpStream object created via #httpCreateStream
    @ingroup HttpTx
    @stability Internal
    @internal
 */
PUBLIC void httpFinalizeConnector(HttpStream *stream);

/**
    Finalize transmission of the http response
    @description This routine should be called by applications and handlers to signify the end of the body content being sent with the request or response body. This call will force the transmission of buffered content to the peer. HttpFinalizeOutput will set the HttpTx.finalizedOutput flag and write a final chunk trailer if using chunked transfers. If the output is already finalized, this call does nothing. Note that after finalization, incoming content may continue to be processed. i.e. httpFinalizeOutput can be called before all incoming data has been received. Use httpFinalizeInput to signify that processing all input is complete.
    \n\n
    The difference between #httpFinalize and #httpFinalizeOutput is that #httpFinalize implies that all request processing is complete including both input and output. Whereas #httpFinalizeOutput implies that the output is generated. Note that while the output may be fully generated, it may not be fully transmitted by the pipeline and connector. When the output is fully transmitted, the connector will call #httpFinalizeConnector.
    @param stream HttpStream Queue object.
    @ingroup HttpTx
    @stability Evolving
 */
PUBLIC void httpFinalizeOutput(HttpStream *stream);

/**
    Finalize receiption of the http content
    @description This routine should be called by clients and Handlers to signify that all processing of the input is complete. the request or response body.
    @param stream HttpStream Queue object.
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC void httpFinalizeInput(HttpStream *stream);

/**
    Flush transmit data.
    @description This call initiates writing buffered data an will not block.
    If you need to wait until all the data has been written to the socket, use #httpFlushAll.
    Handlers may only call this routine in their open, close, ready, start and writable callbacks.
    @param stream HttpStream stream object created via #httpCreateStream
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC void httpFlush(HttpStream *stream);

/**
    Flush transmit data and wait for all the data to be written to the socket.
    @description This call initiates writing buffered data.
    If in sync mode this call may block until the output queues drain.
    In sync mode, this may invoke mprYield before blocking to consent for the garbage collector to run. Callers must
    ensure they have retained all required temporary memory before invoking this routine.
    Filters and connectors should not call this routine as it may block. Use #httpFlush in filters or connectors.
    Handlers may only call this routine in their open, close, ready, start and writable callbacks.
    See #httpFlush if you do need to wait for all the data to be written to the socket.
    @param stream HttpStream stream object created via #httpCreateStream
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC void httpFlushAll(HttpStream *stream);

/**
    Follow redirctions
    @description Enabling follow redirects enables the Http service to transparently follow 301 and 302 redirections
        and fetch the redirected URI.
    @param stream HttpStream stream object created via #httpCreateStream
    @param follow Set to true to enable transparent redirections
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC void httpFollowRedirects(HttpStream *stream, bool follow);

/**
    Format an error transmission
    @description Format an error message to use instead of data generated by the request processing pipeline.
        This is typically used to send errors and redirections. The message is also sent to the error log.
    @param stream HttpStream stream object created via #httpCreateStream
    @param status Http response status code
    @param fmt Printf style formatted string. This string may contain HTML tags and is not HTML encoded before
        sending to the user. NOTE: Do not send user input back to the client using this method. Otherwise you open
        large security holes.
    @param ... Arguments for fmt
    @return A count of the number of bytes in the transmission body.
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC void httpFormatError(HttpStream *stream, int status, cchar *fmt, ...) PRINTF_ATTRIBUTE(3,4);

/**
    Format an alternate response
    @description Format a response to use instead of data generated by the request processing pipeline.
        This is used for alternate responses that are not errors.
    @param stream HttpStream stream object created via #httpCreateStream
    @param fmt Printf style formatted string. This string may contain HTML tags and is not HTML encoded before
        sending to the user. NOTE: Do not send user input back to the client using this method. Otherwise you open
        large security holes.
    @param ... Arguments for fmt
    @return A count of the number of bytes in the transmission body.
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC ssize httpFormatResponse(HttpStream *stream, cchar *fmt, ...) PRINTF_ATTRIBUTE(2,3);

/**
    Format an alternate response
    @description Format a response to use instead of data generated by the request processing pipeline.
        This is similar to #httpFormatResponse.
    @param stream HttpStream stream object created via #httpCreateStream
    @param fmt Printf style formatted string. This string may contain HTML tags and is not HTML encoded before
        sending to the user. NOTE: Do not send user input back to the client using this method. Otherwise you open
        large security holes.
    @param args Varargs style list of arguments
    @return A count of the number of bytes in the transmission body.
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC ssize httpFormatResponsev(HttpStream *stream, cchar *fmt, va_list args);

/**
    Format a response body.
    @description Format a transmission body to use instead of data generated by the request processing pipeline.
        The body will be created in HTML or in plain text depending on the value of the request Accept header.
        This call is used for alternate responses that are not errors.
    @param stream HttpStream stream object created via #httpCreateStream
    @param title Title string to format into the HTML transmission body.
    @param fmt Printf style formatted string. This string may contain HTML tags and is not HTML encoded before
        sending to the user. NOTE: Do not send user input back to the client using this method. Otherwise you open
        large security holes.
    @param ... Arguments for fmt
    @return A count of the number of bytes in the transmission body.
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC ssize httpFormatResponseBody(HttpStream *stream, cchar *title, cchar *fmt, ...) PRINTF_ATTRIBUTE(3,4);

/**
    Get a tx http header.
    @description Get a http response header value for a given header key.
    @param stream HttpStream stream object created via #httpCreateStream
    @param key Name of the header to retrieve.
    @return Value associated with the header key or null if the key did not exist in the response.
    @ingroup HttpTx
    @stability Prototype
 */
PUBLIC cchar *httpGetTxHeader(HttpStream *stream, cchar *key);

/**
    Get the queue data for the connection
    @param stream HttpStream stream object created via #httpCreateStream
    @return the private queue data object
 */
PUBLIC void *httpGetQueueData(HttpStream *stream);

/**
    Return whether transfer chunked encoding will be used on this request
    @param stream HttpStream stream object created via #httpCreateStream
    @returns true if chunk encoding will be used
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC int httpIsChunked(HttpStream *stream);

/**
    Test if request has been finalized
    @description This call tests if #httpFinalize has been called.
    @param stream HttpStream stream object
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC int httpIsFinalized(HttpStream *stream);

/**
    Test if request response has been fully generated.
    @description This call tests if all transmit data has been generated and finalized. Handlers call #httpFinalizeOutput
        to signify the end of transmit data.
    @param stream HttpStream stream object
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC int httpIsOutputFinalized(HttpStream *stream);

/**
    Determine if the transmission needs a transparent retry to implement authentication or redirection. This is used
    by client requests. If authentication is required, a request must first be tried once to receive some authentication
    key information that must be resubmitted to gain access.
    @param stream HttpStream stream object created via #httpCreateStream
    @param url Reference to a string to receive a redirection URL. Set to NULL if not redirection is required.
    @return true if the request needs to be retried.
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC bool httpNeedRetry(HttpStream *stream, cchar **url);

/**
    Tell the tx to omit sending any body
    @param stream HttpStream stream object created via #httpCreateStream
 */
PUBLIC void httpOmitBody(HttpStream *stream);

/**
    Redirect the client
    @description Redirect the client to a new uri.
    @param stream HttpStream stream object created via #httpCreateStream
    @param status Http status code to send with the response
    @param uri New uri for the client
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC void httpRedirect(HttpStream *stream, int status, cchar *uri);

/**
    Remove a header from the transmission
    @description Remove a header if present.
    @param stream HttpStream stream object created via #httpCreateStream
    @param key Http response header key
    @return "Zero" if successful, otherwise a negative MPR error code.
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC int httpRemoveHeader(HttpStream *stream, cchar *key);

/**
    Issue a http request
    @param method HTTP method to use
    @param uri URI to request
    @param data Optional data to send with request. Set to null for GET requests.
    @param protocol HTTP protocol to use. Set to 1 for HTTP/1.1 and 2 for HTTP/2.
    @param err Output parameter to receive any error messages.
    @return HttpStream object. Use #httpGetStatus to read status and #httpReadString to read the response data.
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC HttpStream *httpRequest(cchar *method, cchar *uri, cchar *data, int protocol, char **err);

/**
    Define a content length header in the transmission. This will define a "Content-Length: NNN" request header and
        set Tx.length.
    @param stream HttpStream stream object created via #httpCreateStream
    @param length Numeric value for the content length header.
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC void httpSetContentLength(HttpStream *stream, MprOff length);

/**
    Set the transmission (response) content mime type
    @description Set the mime type Http header in the transmission
    @param stream HttpStream stream object created via #httpCreateStream
    @param mimeType Mime type string
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC void httpSetContentType(HttpStream *stream, cchar *mimeType);

/*
    Flags for httpSetCookie
 */
#define HTTP_COOKIE_SECURE      0x1         /**< Flag for Set-Cookie for SSL only */
#define HTTP_COOKIE_HTTP        0x2         /**< Flag for Set-Cookie httponly. Not visible to Javascript */
#define HTTP_COOKIE_SAME_LAX    0x4         /**< Flag for Set-Cookie SameSite=Lax */
#define HTTP_COOKIE_SAME_STRICT 0x8         /**< Flag for Set-Cookie SameSite=Strict */

/**
    Set a transmission cookie
    @description Define a cookie to send in the transmission Http header
    @param stream HttpStream stream object created via #httpCreateStream
    @param name Cookie name
    @param value Cookie value
    @param path URI path to which the cookie applies
    @param domain Domain in which the cookie applies. Must have 2-3 dots. If null, a domain is created using the
        current request host header. If set to the empty string, the domain field is omitted.
        If the domain is a numerical IP address or localhost, the domain will not be included as the browsers do not
        support this pattern consistently.
        Note that hostname port numbers are ignored by browsers and so web sites with the same domain name but different
        port numbers may have conflicting cookies. This is according to the Cookie RFC standard.
    @param lifespan Duration for the cookie to persist in msec. Set to zero to create a session cookie that is meant to
        be automatically removed when the user exits their browser. However, beware, Chrome subverts this and will persist
        session cookies if "Continue where you left off" is enabled in Chrome preferences.
    @param flags Cookie options mask. The following options are supported:
        @li HTTP_COOKIE_SECURE      - Set the 'Secure' attribute on the cookie.
        @li HTTP_COOKIE_HTTP        - Set the 'HttpOnly' attribute on the cookie.
        @li HTTP_COOKIE_SAME_LAX    - Set the 'SameSite=Lax' attribute on the cookie.
        @li HTTP_COOKIE_SAME_STRICT - Set the 'SameSite=Strict' attribute on the cookie.
        See RFC 6265 for details about the 'Secure' and 'HttpOnly' cookie attributes.
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC void httpSetCookie(HttpStream *stream, cchar *name, cchar *value, cchar *path, cchar *domain, MprTicks lifespan,
    int flags);

/**
    Remove a cookie from the client (browser)
    This will emit a Set-Cookie response header with the value set to "" and a one second lifespan.
    @param stream HttpStream stream object created via #httpCreateStream
    @param name Name of the cookie created with httpSetCookie
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC void httpRemoveCookie(HttpStream *stream, cchar *name);

#if UNUSED
/**
    Define the length of the transmission content. When static content is used for the transmission body, defining
    the entity length permits the request pipeline to know when all the data has been sent.
    @param stream HttpStream stream object created via #httpCreateStream
    @param len Transmission body length in bytes
    @ingroup HttpTx
    @stability Prototype
 */
PUBLIC void httpSetEntityLength(HttpStream *stream, MprOff len);
#endif

/**
    Set the filename to serve for a request
    @description This routine defines a non-default response document filename.
       The filename may be virtual and not correspond to a physical file. It also may be a file outside the documents root
       directory. If it is not a file under the route documents directory, set the flags parameter to HTTP_TX_NO_CHECK.
       Otherwise, the filename will be checked to ensure it is inside the route documents directory.
       \n\n
       Typically a handler will call #httpMapFile to perform default request URI to filename mapping and should not need
       to call httpSetFilename unless a file outside the route documents directory is required to be served.
       \n\n
       This routine will set the HttpTx filename, ext, etag and fileInfo fields.
       \n\n
        Note: the response header mime type will be set based on the request URI. To override, use #httpSetContentType
    @param stream HttpStream stream object
    @param filename Tx filename to define. Set to NULL to reset the filename.
    @param flags Flags word. Or together the desired flags. Include to HTTP_TX_NO_CHECK to bypass checking if the
        filename resides inside the route documents directory.
    @return True if the filename exists and is readable.
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC bool httpSetFilename(HttpStream *stream, cchar *filename, int flags);

/**
    Set the handler for this request
    Use this request from the Handler rewrite callback to change the selected handler to process a request.
    Most useful to set the Tx.filename and pass to the fileHandler.
    @param stream HttpStream stream object created via #httpCreateStream
    @param handler Handler to set
    @stability Stable
 */
PUBLIC void httpSetHandler(HttpStream *stream, HttpStage *handler);

/**
    Set a transmission header
    @description Set a Http header to send with the request. If the header already exists, it its value is overwritten.
    @param stream HttpStream stream object created via #httpCreateStream
    @param key Http response header key
    @param fmt Printf style formatted string to use as the header key value
    @param ... Arguments for fmt
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC void httpSetHeader(HttpStream *stream, cchar *key, cchar *fmt, ...) PRINTF_ATTRIBUTE(3,4);

/**
    Set a simple key/value transmission header
    @description Set a Http header to send with the request. If the header already exists, it its value is overwritten.
    @param stream HttpStream stream object created via #httpCreateStream
    @param key Http response header key
    @param value String value for the key
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC void httpSetHeaderString(HttpStream *stream, cchar *key, cchar *value);

/**
    Set a Http response status.
    @description Set the Http response status for the request. This defaults to 200 (OK).
    @param stream HttpStream stream object created via #httpCreateStream
    @param status Http status code.
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC void httpSetStatus(HttpStream *stream, int status);

/**
    Set the responded flag for the request
    @description This call sets the requests responded status. Once the HTTP response status code has been defined,
        HTTP response headers or any output has been generated, the request is regarded as having "responded" in-part to the client.
        This means that any errors cannot revise the HTTP response status and may need to prematurely abort the request to signify
        to the clien that the request has failed.
    @param stream HttpStream stream object
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC void httpSetResponded(HttpStream *stream);

/**
    Wait for the client connection to achieve the requested state.
    @description This call blocks until the connection reaches the desired state. It creates a wait handler and
        services events while waiting. This is useful for blocking client requests, and should never be used on
        server-side connections.
        \n\n
        It is often required to call mprStartDispatcher on the connection dispatcher after calling $httpCreateStream.
        This ensures that all foreground activity on the connection is serialized with respect to work done in response
        to I/O events while waiting in httpWait.
        \n\n
        This routine may invoke mprYield before it sleeps to consent for the garbage collector to turn. Callers must
        ensure they have retained all required temporary memory before invoking this routine.
    @param stream HttpStream stream object created via #httpCreateStream
    @param state HTTP_STATE_XXX to wait for.
    @param timeout Timeout in milliseconds to wait. Set to -1 to use the default connection timeouts. Set to zero
        to wait forever.
    @return "Zero" if successful. Otherwise return a negative MPR error code. Specific returns include:
        MPR_ERR_TIMEOUT and MPR_ERR_BAD_STATE.
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC int httpWait(HttpStream *stream, int state, MprTicks timeout);

/**
    Create a HTTP header packet
    @description Write the Http transmission headers into the given packet. This should only be called by connectors
        just prior to sending output to the client. It should be delayed as long as possible if the content length is
        not yet known to give the pipeline a chance to determine the transmission length. This way, a non-chunked
        transmission can be sent with a content-length header. This is the fastest HTTP transmission.
    @param q Queue owning the packet
    @param packet Packet into which to place the headers
    @ingroup HttpTx
    @stability Stable
 */
PUBLIC HttpPacket *httpCreateHeaders(HttpQueue *q, HttpPacket *packet);
#define httpWriteHeaders(q, packet) httpCreateHeaders

/**
    Write Http upload body data
    @description Write files and form fields as request body data. This will use transfer chunk encoding. This routine
        will block until all the buffer is written.
        This routine may invoke mprYield before it blocks to consent for the garbage collector to turn. Callers must
        ensure they have retained all required temporary memory before invoking this routine.
    @param stream Http stream object created via #httpCreateStream
    @param fileData List of string file names to upload
    @param formData List of strings containing "key=value" pairs. The form data should be already www-urlencoded.
    @return Number of bytes successfully written.
    @ingroup HttpStream
    @stability Stable
 */
PUBLIC ssize httpWriteUploadData(HttpStream *stream, MprList *formData, MprList *fileData);

/*
    Internal
 */
PUBLIC void httpPrepareHeaders(HttpStream *stream);
PUBLIC void httpCreateHeaders1(HttpQueue *q, HttpPacket *packet);
PUBLIC void httpCreateHeaders2(HttpQueue *q, HttpPacket *packet);

/********************************* HttpEndpoint ***********************************/
/*
    Endpoint flags
 */
#define HTTP_NEW_DISPATCHER     0x1         /**< New dispatcher for each connection */

/**
    Listening endpoints. Endpoints may have multiple virtual named hosts.
    @defgroup HttpEndpoint HttpEndpoint
    @see HttpEndpoint httpAcceptNet httpAddHostToEndpoint httpCreateConfiguredEndpoint httpCreateEndpoint
        httpDestroyEndpoint httpGetEndpointContext httpIsEndpointAsync
        httpLookupHostOnEndpoint httpSecureEndpoint httpSecureEndpointByName httpSetEndpointAddress
        httpSetEndpointAsync httpSetEndpointContext httpSetEndpointNotifier
        httpStartEndpoint httpStopEndpoint
    @stability Internal
 */
typedef struct HttpEndpoint {
    Http            *http;                  /**< Http service object */
    MprList         *hosts;                 /**< List of host objects */
    char            *ip;                    /**< Listen IP address. May be null if listening on all interfaces. */
    int             port;                   /**< Listen port */
    int             async;                  /**< Listening is in async mode (non-blocking) */
    int             flags;                  /**< Endpoint control flags */
    void            *context;               /**< Embedding context */
    HttpLimits      *limits;                /**< Alias for first host, default route resource limits */
    MprSocket       *sock;                  /**< Listening socket */
    MprDispatcher   *dispatcher;            /**< Event dispatcher */
    HttpNotifier    notifier;               /**< Default connection notifier callback */
    MprSsl          *ssl;                   /**< SSL configurations to use */
    MprMutex        *mutex;                 /**< Multithread sync */
} HttpEndpoint;

/**
    Accept a new connection.
    Accept a new client connection on a new socket. If multithreaded, this will come in on a worker thread
        dedicated to this connection. This is called from the listen wait handler.
    @param endpoint The endpoint on which the server was listening
    @param event Mpr event object
    @return A HttpNet object representing the new network.
    @ingroup HttpEndpoint
    @stability Internal
    @internal
 */
PUBLIC HttpNet *httpAccept(HttpEndpoint *endpoint, MprEvent *event);

/**
    Add a host to an endpoint
    @description Add the host to the endpoint's list  of hosts. A listening endpoint may have multiple
        virutal hosts.
    @param endpoint Endpoint to which the host will be added.
    @param host HttpHost object to add.
    @return "Zero" if the host can be added.
    @ingroup HttpEndpoint
    @stability Internal
 */
PUBLIC void httpAddHostToEndpoint(HttpEndpoint *endpoint, struct HttpHost *host);

/**
    Create and configure a new endpoint.
    @description Convenience function to create and configure a new endpoint without using a config file.
        If no host is supplied, a default host and route are created.
    @param host Optional HttpHost object.
    @param home Home directory for configuration files for the endpoint
    @param documents Directory containing the
    @param ip IP address to use for the endpoint. Set to null to listen on all interfaces.
    @param port Listening port number to use for the endpoint
    @return A configured HttpEndpoint object instance
    @ingroup HttpEndpoint
    @stability Internal
*/
PUBLIC HttpEndpoint *httpCreateConfiguredEndpoint(struct HttpHost *host, cchar *home, cchar *documents, cchar *ip, int port);

/**
    Create an endpoint  object.
    @description Creates a listening endpoint on the given IP:PORT. Use httpStartEndpoint to begin listening for client
        connections.
    @param ip IP address on which to listen
    @param port IP port number
    @param dispatcher Dispatcher to use. Can be null.
    @ingroup HttpEndpoint
    @stability Stable
 */
PUBLIC HttpEndpoint *httpCreateEndpoint(cchar *ip, int port, MprDispatcher *dispatcher);

/**
    Destroy the endpoint
    @description This destroys the endpoint created by #httpCreateEndpoint. Calling this routine should not
        normally be necessary as the garbage collector will invoke as required.
    @param endpoint HttpEndpoint object returned from #httpCreateEndpoint.
    @ingroup HttpEndpoint
    @stability Stable
 */
PUBLIC void httpDestroyEndpoint(HttpEndpoint *endpoint);

/**
    Get the endpoint context object
    @param endpoint HttpEndpoint object created via #httpCreateEndpoint
    @return The endpoint context object defined via httpSetEndpointContext
    @ingroup HttpEndpoint
    @stability Stable
 */
PUBLIC void *httpGetEndpointContext(HttpEndpoint *endpoint);

/**
    Get if the endpoint is running in asynchronous mode
    @param endpoint HttpEndpoint object created via #httpCreateEndpoint
    @return True if the endpoint is in async mode
    @ingroup HttpEndpoint
    @stability Stable
 */
PUBLIC int httpIsEndpointAsync(HttpEndpoint *endpoint);

/**
    Lookup a host name
    @description Lookup a host by name in the set of defined hosts for this endpoint.
    @param endpoint HttpEndpoint object created via #httpCreateEndpoint
    @param name Host name to search for
    @return An HttpHost object instance or null if the host cannot be found.
    @ingroup HttpEndpoint
    @stability Stable
 */
PUBLIC struct HttpHost *httpLookupHostOnEndpoint(HttpEndpoint *endpoint, cchar *name);

/**
    Secure an endpoint
    @description Define the SSL parameters for an endpoint. This must be done before starting listening on
        the endpoint via #httpStartEndpoint.
    @param endpoint HttpEndpoint object created via #httpCreateEndpoint
    @param ssl MprSsl object
    @returns "Zero" if successful, otherwise a negative MPR error code.
    @ingroup HttpEndpoint
    @stability Stable
 */
PUBLIC int httpSecureEndpoint(HttpEndpoint *endpoint, struct MprSsl *ssl);

/**
    Secure an endpoint by name
    @description Define the SSL parameters for an endpoint that is selected by name. This must be done before
    starting listening on the endpoint via #httpStartEndpoint.
    @param name Endpoint name. The endpoint name is comprised of the IP and port. For example: "127.0.0.1:7777"
    @param ssl MprSsl object
    @returns Zero if successful, otherwise a negative MPR error code.
    @ingroup HttpEndpoint
    @stability Stable
 */
PUBLIC int httpSecureEndpointByName(cchar *name, struct MprSsl *ssl);

/**
    Set the endpoint IP address
    @description This call defines the endpoint's IP address and port number. If the endpoint has already been
        started, this will stop and restart the endpoint. Current requests will not be disturbed.
        This is useful to modify the endpoints address when using dynamically assigned IP addresses.
    @param endpoint HttpEndpoint object created via #httpCreateEndpoint
    @param ip IP address to use for the endpoint. Set to null to listen on all interfaces.
    @param port Listening port number to use for the endpoint
    @returns "Zero" if successful, otherwise a negative MPR error code.
    @ingroup HttpEndpoint
    @stability Stable
 */
PUBLIC int httpSetEndpointAddress(HttpEndpoint *endpoint, cchar *ip, int port);

/**
    Control if the endpoint is running in asynchronous mode
    @param endpoint HttpEndpoint object created via #httpCreateEndpoint
    @param enable Set to 1 to enable async mode.
    @ingroup HttpEndpoint
    @stability Stable
 */
PUBLIC void httpSetEndpointAsync(HttpEndpoint *endpoint, int enable);

/**
    Set the endpoint context object
    @param endpoint HttpEndpoint object created via #httpCreateEndpoint
    @param context New context object
    @ingroup HttpEndpoint
    @stability Stable
 */
PUBLIC void httpSetEndpointContext(HttpEndpoint *endpoint, void *context);

/**
    Define a notifier callback for this endpoint.
    @description The notifier callback will be invoked as Http requests are processed.
    @param endpoint HttpEndpoint object created via #httpCreateEndpoint
    @param fn Notifier function.
    @ingroup HttpEndpoint
    @stability Stable
 */
PUBLIC void httpSetEndpointNotifier(HttpEndpoint *endpoint, HttpNotifier fn);

/**
    Start listening for client connections on an endpoint.
    @description Opens the endpoint socket and starts listening for connections.
    @param endpoint HttpEndpoint object created via #httpCreateEndpoint
    @returns "Zero" if successful, otherwise a negative MPR error code.
    @ingroup HttpEndpoint
    @stability Stable
 */
PUBLIC int httpStartEndpoint(HttpEndpoint *endpoint);

/**
    Start listening for client connections on all endpoints
    @description Opens all endpoints and starts listening for connections.
    @returns "Zero" if successful, otherwise a negative MPR error code.
    @ingroup HttpEndpoint
    @stability Evolving
 */
PUBLIC int httpStartEndpoints(void);

/**
    Stop listening for client connections on all endpoints
    @description Closes all endpoints and stops listening for connections. Does not impact running requests.
    @returns "Zero" if successful, otherwise a negative MPR error code.
    @ingroup HttpEndpoint
 */
PUBLIC void httpStopEndpoints(void);

/**
    Stop the server listening for client connections.
    @description Closes the socket endpoint. This preserves connections accepted via the listening endpoint.
    @param endpoint HttpEndpoint object created via #httpCreateEndpoint
    @ingroup HttpEndpoint
    @stability Stable
 */
PUBLIC void httpStopEndpoint(HttpEndpoint *endpoint);

/********************************** HttpHost ***************************************/
/*
    Flags
 */
#define HTTP_HOST_NO_TRACE      0x10        /**< Host flag to disable the of TRACE HTTP method */
#define HTTP_HOST_WILD_STARTS   0x20        /**< Host name starts with pattern */
#define HTTP_HOST_WILD_CONTAINS 0x40        /**< Host name contains the host name */
#define HTTP_HOST_WILD_REGEXP   0x80        /**< Host name is a regular expression */
#define HTTP_HOST_ATTACHED      0x100       /**< Host name attached to an endpoint */

/**
    Host Object
    @description A Host object represents a logical host. Several logical hosts may share a single HttpEndpoint.
    @defgroup HttpHost HttpHost
    @see HttpHost httpAddRoute httpCloneHost httpCreateHost httpResetRoutes httpSetHostHome
        httpSetHostName httpSetHostProtocol
    @stability Internal
*/
typedef struct HttpHost {
    /*
        NOTE: A host may be associated with multiple listening endpoints.
     */
    cchar           *name;                  /**< Full host name with port */
    cchar           *hostname;              /**< Host name portion only */
    HttpUri         *canonical;             /**< Canonical host name (optional canonial public name for redirections) */
    struct HttpHost *parent;                /**< Parent host to inherit aliases, dirs, routes */
    MprCache        *responseCache;         /**< Response content caching store */
    MprList         *routes;                /**< List of Route defintions */
    HttpRoute       *defaultRoute;          /**< Default route for the host */
    HttpEndpoint    *defaultEndpoint;       /**< Default endpoint for host */
    HttpEndpoint    *secureEndpoint;        /**< Secure endpoint for host */
    MprHash         *streams;               /**< Hash of mime-types to stream record */
    void            *nameCompiled;          /**< Compiled name regular expression (not alloced) */
    int             flags;                  /**< Host flags */
} HttpHost;

/**
    Add a route to a host
    @description Add the route to the host list of routes. During request route matching, routes are processed
    in order, so it is important to define routes in the order in which you wish to match them.
    @param host HttpHost object
    @param route Route to add
    @return "Zero" if the route can be added.
    @ingroup HttpHost
    @stability Stable
 */
PUBLIC int httpAddRoute(HttpHost *host, HttpRoute *route);

/**
    Clone a host
    @description The parent host is cloned and a new host returned. The new host inherites the parent's configuration.
    @param parent Parent HttpHost object to clone
    @return The new HttpHost object.
    @ingroup HttpHost
    @stability Stable
 */
PUBLIC HttpHost *httpCloneHost(HttpHost *parent);

/**
    Create a host
    @description Create a new host object. The host is added to the Http service's list of hosts.
    @return The new HttpHost object.
    @ingroup HttpHost
    @stability Stable
 */
PUBLIC HttpHost *httpCreateHost(void);

/**
    Create the default host
    @description Create and define a default host. The host is added to the Http service's list of hosts.
    A default route is created for the host
    @return The new HttpHost object.
    @ingroup HttpHost
    @stability Evolving
 */
PUBLIC HttpHost *httpCreateDefaultHost(void);

/**
    Get the default host defined via httpSetDefaultHost
    @return The defaul thost object
    @ingroup HttpHost
    @stability Stable
 */
PUBLIC HttpHost *httpGetDefaultHost(void);

/**
    Get the default route for a host
    @param host Host object
    @return The default route for the host
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC HttpRoute *httpGetDefaultRoute(HttpHost *host);

/**
    Return the default route for a host
    @description The host has a default route which holds default configuration. Typically the default route
        is not directly used when routing URIs. Rather other routes inherit from the default route and are used to
        respond to client requests.
    @param host Host to examine.
    @return Default route object
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC HttpRoute *httpGetHostDefaultRoute(HttpHost *host);

/**
    Show the current route table to the error log.
    @description This emits the currently defined route table for a host to the route table. If the "full" argument is true,
        a more-complete, multi-line output format will be used. Othewise, a one-line, abbreviated route description will
        be output.
    @param host Host to examine.
    @param full Set to true for a "fuller" output route description.
    @ingroup HttpRoute
    @stability Stable
 */
PUBLIC void httpLogRoutes(HttpHost *host, bool full);

/**
    Lookup a route by pattern
    @param host HttpHost object owning the route table
    @param pattern Route pattern to find. If null or empty, look for "/"
    @ingroup HttpRoute
    @stability Stable
  */
PUBLIC HttpRoute *httpLookupRoute(HttpHost *host, cchar *pattern);

/**
    Reset the list of routes for the host
    @param host HttpHost object
    @ingroup HttpHost
    @stability Stable
 */
PUBLIC void httpResetRoutes(HttpHost *host);

/**
    Set the host canonical name
    @description The host canonical name is the public perferred name to use for the server. This is
    used when redirecting client requests for directories.
    @param host HttpHost object
    @param name Host canonical name to use
    @return Zero if successful. May return a negative MPR error code if the name is a regular expression and cannot
        be compiled.
    @ingroup HttpHost
    @stability Stable
 */
PUBLIC int httpSetHostCanonicalName(HttpHost *host, cchar *name);

/**
    Set the default host for all servers.
    @param host Host to define as the default host
    @ingroup HttpHost
    @stability Stable
 */
PUBLIC void httpSetDefaultHost(HttpHost *host);

/**
    Set the default endpoint for a host
    @description The host may have a default endpoint that is used when doing redirections to http.
    @param host Host to examine.
    @param endpoint Secure endpoint to use as the default
    @ingroup HttpHost
    @stability Stable
 */
PUBLIC void httpSetHostDefaultEndpoint(HttpHost *host, HttpEndpoint *endpoint);

/**
    Set the default route for a host
    @description The host has a default route which holds default configuration. Typically the default route
        is not directly used when routing URIs. Rather other routes inherit from the default route and are used to
        respond to client requests.
    @param host Host to examine.
    @param route Route to define as the default
    @ingroup HttpHost
    @stability Stable
 */
PUBLIC void httpSetHostDefaultRoute(HttpHost *host, HttpRoute *route);

/**
    Set the host name
    @description The host name is used when matching client requests to virtual hosts using the Http request Host header.
        If the host name starts with "*", it will match names that contain the name.
        If the host name ends with "*", it will match names that start with the name.
        If the host name begins and ends with a "/", the name is assumed to be a regular expression. Regular expressions
        may match multiple host names by using the "|" character to separate names.
    @param host HttpHost object
    @param name Host name to use
    @return Zero if successful. May return a negative MPR error code if the name is a regular expression and cannot
        be compiled.
    @ingroup HttpHost
    @stability Stable
 */
PUBLIC int httpSetHostName(HttpHost *host, cchar *name);

/**
    Set the default secure endpoint for a host
    @description The host may have a default secure endpoint that is used when doing redirections to https.
    @param host Host to examine.
    @param endpoint Secure endpoint to use as the default
    @ingroup HttpHost
    @stability Stable
 */
PUBLIC void httpSetHostSecureEndpoint(HttpHost *host, HttpEndpoint *endpoint);

/**
    Set the server root for a host
    @description The server root is used as the default directory to locate configuration files for the host
    @param host HttpHost object
    @param root Directory path for the host server root
    @ingroup HttpHost
    @stability Stable
 */
PUBLIC void httpSetHostRoot(HttpHost *host, cchar *root);

/**
    Set the host HTTP protocol version
    @description Set the host protocol version to either HTTP/1.0 or HTTP/1.1
    @param host HttpHost object
    @param protocol Set to either HTTP/1.0 or HTTP/1.1
    @ingroup HttpHost
    @stability Stable
 */
PUBLIC void httpSetHostProtocol(HttpHost *host, cchar *protocol);

/*
    Internal
 */
PUBLIC int httpStartHost(HttpHost *host);
PUBLIC void httpStopHost(HttpHost *host);

/********************************* Web Sockets *************************************/
/**
    WebSocket Service to implement the WebSockets RFC 6455 specification for client and server communications.
    @description WebSockets is a technology providing interactive communication between a server and client. Normal HTML
    connections follow a request / response paradigm and do not easily support asynchronous communications or unsolicited
    data pushed from the server to the client. WebSockets solves this by supporting bi-directional, full-duplex
    communications over persistent connections. A WebSocket connection is established over a standard HTTP connection and is
    then upgraded without impacting the original connection. This means it will work with existing networking infrastructure
    including firewalls and proxies.
    @defgroup HttpWebSocket HttpWebSocket
    @see httpGetWebSocketCloseReason httpGetWebSocketData httpGetWebSocketMessageLength httpGetWebSocketProtocol
        httpGetWebSocketState httpGetWriteQueueCount httpIsLastPacket httpSend httpSendBlock httpSendClose
        httpSetWebSocketPreserveFrames httpSetWebSocketData httpSetWebSocketProtocols httpWebSocketOrderlyClosed
    @stability Internal
 */
typedef struct HttpWebSocket {
    int             state;                  /**< State */
    int             frameState;             /**< Message frame state */
    int             closing;                /**< Started closing sequnce */
    int             closeStatus;            /**< Close status provided by peer */
    int             currentMessageType;     /**< Current incoming messsage type */
    int             maskOffset;             /**< Offset in dataMask */
    int             more;                   /**< More data to send in a message */
    int             preserveFrames;         /**< Do not join frames */
    int             partialUTF;             /**< Last frame had a partial UTF codepoint */
    int             rxSeq;                  /**< Incoming packet number */
    int             txSeq;                  /**< Outgoing packet number */
    ssize           frameLength;            /**< Length of the current frame */
    ssize           messageLength;          /**< Length of the current message */
    HttpPacket      *currentFrame;          /**< Message frame being currently read */
    HttpPacket      *currentMessage;        /**< Current incoming messsage so far */
    HttpPacket      *tailMessage;           /**< Subsequent message frames */
    MprEvent        *pingEvent;             /**< Ping timer event */
    char            *subProtocol;           /**< Application level sub-protocol */
    cchar           *errorMsg;              /**< Error message for last I/O */
    cchar           *closeReason;           /**< Reason for closure */
    void            *data;                  /**< Custom data for applications (marked) */
    uchar           dataMask[4];            /**< Mask for data */
} HttpWebSocket;

#define WS_MAGIC        "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WS_MAX_CONTROL  125                 /**< Maximum bytes in control message */
#define WS_VERSION      13                  /**< Current WebSocket specification version */

/*
    httpSendBlock message types
 */
#define WS_MSG_CONT     0x0                 /**< Continuation of WebSocket message */
#define WS_MSG_TEXT     0x1                 /**< httpSendBlock type for text messages */
#define WS_MSG_BINARY   0x2                 /**< httpSendBlock type for binary messages */
#define WS_MSG_CONTROL  0x8                 /**< Start of control messages */
#define WS_MSG_CLOSE    0x8                 /**< httpSendBlock type for close message */
#define WS_MSG_PING     0x9                 /**< httpSendBlock type for ping messages */
#define WS_MSG_PONG     0xA                 /**< httpSendBlock type for pong messages */
#define WS_MSG_MAX      0xB                 /**< Max message type for httpSendBlock */

/*
    Close message status codes
    0-999       Unused
    1000-1999   Reserved for spec
    2000-2999   Reserved for extensions
    3000-3999   Library use
    4000-4999   Application use
 */
#define WS_STATUS_OK                   1000     /**< Normal closure */
#define WS_STATUS_GOING_AWAY           1001     /**< Endpoint is going away. Server down or browser navigating away */
#define WS_STATUS_PROTOCOL_ERROR       1002     /**< WebSockets protocol error */
#define WS_STATUS_UNSUPPORTED_TYPE     1003     /**< Unsupported message data type */
#define WS_STATUS_FRAME_TOO_LARGE      1004     /**< Reserved. Message frame is too large */
#define WS_STATUS_NO_STATUS            1005     /**< No status was received from the peer in closing */
#define WS_STATUS_COMMS_ERROR          1006     /**< TCP/IP communications error  */
#define WS_STATUS_INVALID_UTF8         1007     /**< Text message has invalid UTF-8 */
#define WS_STATUS_POLICY_VIOLATION     1008     /**< Application level policy violation */
#define WS_STATUS_MESSAGE_TOO_LARGE    1009     /**< Message is too large */
#define WS_STATUS_MISSING_EXTENSION    1010     /**< Unsupported WebSockets extension */
#define WS_STATUS_INTERNAL_ERROR       1011     /**< Server terminating due to an internal error */
#define WS_STATUS_TLS_ERROR            1015     /**< TLS handshake error */
#define WS_STATUS_MAX                  5000     /**< Maximum error status (less one) */

/*
    WebSocket states (rx->webSockState)
 */
#define WS_STATE_CONNECTING     0               /**< WebSocket connection is being established */
#define WS_STATE_OPEN           1               /**< WebSocket handsake is complete and ready for communications */
#define WS_STATE_CLOSING        2               /**< WebSocket is closing */
#define WS_STATE_CLOSED         3               /**< WebSocket is closed */

/**
    Get the close reason supplied by the peer.
    @description The peer may supply a UTF8 messages reason for the closure.
    @param stream HttpStream stream object created via #httpCreateStream
    @return The UTF8 reason string supplied by the peer when closing the WebSocket.
    @ingroup HttpWebSocket
    @stability Evolving
 */
PUBLIC cchar *httpGetWebSocketCloseReason(HttpStream *stream);

/**
    Get the WebSocket private data
    @description Get the private data defined with #httpSetWebSocketData
    @param stream HttpStream stream object created via #httpCreateStream
    @return The private data reference
    @ingroup HttpWebSocket
    @stability Evolving
 */
PUBLIC void *httpGetWebSocketData(HttpStream *stream);

/**
    Get the message length for the current message
    @description The message length will be updated as the message frames are received. The message length is
        only complete when the last frame has been received. See #httpIsLastPacket
    @param stream HttpStream stream object created via #httpCreateStream
    @return The size of the message.
    @ingroup HttpWebSocket
    @stability Evolving
 */
PUBLIC ssize httpGetWebSocketMessageLength(HttpStream *stream);

/**
    Get the selected WebSocket protocol selected by the server
    @param stream HttpStream stream object created via #httpCreateStream
    @return The WebSocket protocol string
    @ingroup HttpWebSocket
    @stability Evolving
 */
PUBLIC char *httpGetWebSocketProtocol(HttpStream *stream);

/**
    Get the WebSocket state
    @return The WebSocket state. Will be WS_STATE_CONNECTING, WS_STATE_OPEN, WS_STATE_CLOSING or WS_STATE_CLOSED.
    @ingroup HttpWebSocket
    @stability Evolving
 */
PUBLIC ssize httpGetWebSocketState(HttpStream *stream);

/**
    Send a UTF-8 text message to the WebSocket peer
    @description This call invokes httpSend with a type of WS_MSG_TEXT and flags of HTTP_BUFFER.
        The message must be valid UTF8 as the peer will reject invalid UTF8 messages.
    @param stream HttpStream stream object created via #httpCreateStream
    @param fmt Printf style formatted string
    @param ... Arguments for the format
    @return Number of bytes written
    @ingroup HttpWebSocket
    @stability Evolving
 */
PUBLIC ssize httpSend(HttpStream *stream, cchar *fmt, ...) PRINTF_ATTRIBUTE(2,3);

/**
    Flag for #httpSendBlock to indicate there are more frames for this message
 */
#define HTTP_MORE   0x1000

/**
    Send a message of a given type to the WebSocket peer
    @description This is the lower-level message send routine. It permits control of message types and message framing.
    \n\n
    This routine can operate in a blocking, non-blocking or buffered mode. Blocking mode is specified via the HTTP_BLOCK
    flag. When blocking, the call will wait until it has written all the data. The call will either accept and write all
    the data or it will fail, it will never return "short" with a partial write. If in blocking mode, the call may block
    for up to the inactivity timeout specified in the stream->limits->inactivityTimeout value.
    \n\n
    Non-blocking mode is specified via the HTTP_NON_BLOCK flag. In this mode, the call will consume that amount of data
    that will fit within the outgoing WebSocket queues. Consequently, it may return "short" with a partial write. If this
    occurs the next call to httpSendBlock should set the message type to WS_MSG_CONT to indicate a continued message.
    This is required by the WebSockets specification.
    \n\n
    Buffered mode is the default and may be explicitly specified via the HTTP_BUFFER flag. In buffered mode, the entire
    message will be accepted and will be buffered if required.
    \n\n
    This API may split the message into frames such that no frame is larger than the limit stream->limits->webSocketsFrameSize.
    However, if the HTTP_MORE flag is specified to indicate there is more data to complete this entire message, the data
    provided to this call will not be split into frames and will not be aggregated with previous or subsequent messages.
    i.e. frame boundaries will be presserved and sent as-is to the peer.
    \n\n
    In blocking mode, this routine may invoke mprYield before blocking to consent for the garbage collector to run. Callers
    must ensure they have retained all required temporary memory before invoking this routine.

    @param stream HttpStream stream object created via #httpCreateStream
    @param type Web socket message type. Choose from WS_MSG_TEXT, WS_MSG_BINARY or WS_MSG_PING.
        Use httpSendClose to send a close message. Do not send a WS_MSG_PONG message as it is generated internally
        by the Web Sockets module. If using HTTP_NON_BLOCK and the call returns having written only a portion of the data,
        you must set the type to WS_MSG_CONT for the
    @param msg Message data buffer to send
    @param len Length of msg
    @param flags Include the flag HTTP_BLOCK for blocking operation or HTTP_NON_BLOCK for non-blocking. Set to HTTP_BUFFER to
        buffer the data if required and never block. Set to zero will default to HTTP_BUFFER.
        Include the flag HTTP_MORE to indicate there is more data to come to complete this message. This will set
        frame continuation bit. Setting HTTP_MORE preserve the frame boundaries. i.e. it will ensure the data written is
        not split into frames or aggregated with other data.
    @return Number of data message bytes written. Should equal len if successful, otherwise returns a negative
        MPR error code.
    @ingroup HttpWebSocket
    @stability Evolving
 */
PUBLIC ssize httpSendBlock(HttpStream *stream, int type, cchar *msg, ssize len, int flags);

/**
    Send a close message to the WebSocket peer
    @description This call invokes httpSendBlock with a type of WS_MSG_CLOSE and flags of HTTP_BUFFER.
        The status and reason are encoded in the message. The reason is an optional UTF8 closure reason message.
    @param stream HttpStream stream object created via #httpCreateStream
    @param status Web socket status
    @param reason Optional UTF8 reason text message. The reason must be less than 124 bytes in length.
    @return Number of data message bytes written. Should equal len if successful, otherwise returns a negative
        MPR error code.
    @ingroup HttpWebSocket
    @stability Evolving
 */
PUBLIC ssize httpSendClose(HttpStream *stream, int status, cchar *reason);

/**
    Set the WebSocket private data
    @description Set private data to be retained by the garbage collector
    @param stream HttpStream stream object created via #httpCreateStream
    @param data Managed data reference.
    @ingroup HttpWebSocket
    @stability Evolving
 */
PUBLIC void httpSetWebSocketData(HttpStream *stream, void *data);

/**
    Preserve frames for incoming messages
    @description This routine enables user control of message framing.
        When preserving frames, sent message boundaries will be preserved and  will not be split into frames or
        aggregated with other message frames. Received messages will similarly have their frame boundaries preserved
        and will be stored one frame per HttpPacket.
        Note: enabling this option may prevent full validation of UTF8 text messages if UTF8 codepoints span frame boundaries.
    @param stream HttpStream stream object created via #httpCreateStream
    @param on Set to true to preserve frames
    @return True if the WebSocket was orderly closed.
    @ingroup HttpWebSocket
    @stability Evolving
*/
PUBLIC void httpSetWebSocketPreserveFrames(HttpStream *stream, bool on);

/**
    Set a list of application-level protocols supported by the client
    @param stream HttpStream stream object created via #httpCreateStream
    @param protocols Comma separated list of application-level protocols
    @ingroup HttpWebSocket
    @stability Evolving
 */
PUBLIC void httpSetWebSocketProtocols(HttpStream *stream, cchar *protocols);

/**
    Upgrade a client HTTP connection connection to use WebSockets
    @description This requests an upgrade to use WebSockets. Note this is the upgrade request and the
        confirmation handshake response must still be received and validated. The connection must be upgraded
        before sending any data to the server.
    @param stream HttpStream stream object created via #httpCreateStream
    @return Return Zero if the connection upgrade can be requested.
    @stability Evolving
    @ingroup HttpWebSocket
    @internal
 */
PUBLIC int httpUpgradeWebSocket(HttpStream *stream);

/**
    Test if WebSocket connection was orderly closed by sending an acknowledged close message
    @param stream HttpStream stream object created via #httpCreateStream
    @return True if the WebSocket was orderly closed.
    @ingroup HttpWebSocket
    @stability Evolving
 */
PUBLIC bool httpWebSocketOrderlyClosed(HttpStream *stream);

/************************************ Dir  *****************************************/
/**
    Directory object for the DirHandler
    @defgroup HttpDir HttpDir
    @stability Internal
 */

typedef struct HttpDir {
#if DIR_DIRECTIVES
    MprList         *dirList;
    cchar           *defaultIcon;
    MprList         *extList;
    MprList         *ignoreList;
#endif
    bool            enabled;
    int             fancyIndexing;
    bool            foldersFirst;
    cchar           *pattern;
    char            *sortField;
    int             sortOrder;              /* 1 == ascending, -1 descending */
} HttpDir;

/**
    Get the HttpDir object for a route
    @ingroup HttpDir
    @stability Evolving
    @internal
 */
PUBLIC HttpDir *httpGetDirObj(HttpRoute *route);

/************************************ Misc *****************************************/
/**
    Add an option to the options table
    @param options Option table returned from httpGetOptions
    @param field Field key name
    @param value Value to use for the field
    @ingroup Http
    @stability Evolving
 */
PUBLIC void httpAddOption(MprHash *options, cchar *field, cchar *value);

/**
    Add an option to the options table.
    @description If the field already exists, the added value is inserted prior to the existing value.
    @param options Option table returned from httpGetOptions
    @param field Field key name
    @param value Value to use for the field
    @ingroup Http
    @stability Evolving
*/
PUBLIC void httpInsertOption(MprHash *options, cchar *field, cchar *value);

/**
    Extract a field value from an option string.
    @param options Option string of the form: "field='value' field='value'..."
    @param field Field key name
    @param defaultValue Value to use if "field" is not found in options
    @return Option value.
    @ingroup Http
    @stability Evolving
 */
PUBLIC void *httpGetOption(MprHash *options, cchar *field, cchar *defaultValue);

/**
    Get an option value that is itself an object (hash)
    @description This returns an option value that is an instance of MprHash. When deserializing a JSON option string which
    contains multiple levels, this routine can be used to extract lower option container values.
    @param options Options object to examine.
    @param field Property to return.
    @return An MprHash instance for the given field. This will contain option sub-properties.
    @ingroup Http
    @stability Evolving
 */
PUBLIC MprHash *httpGetOptionHash(MprHash *options, cchar *field);

/**
    Convert an options string into an options table
    @param options Option string of the form: "{field:'value', field:'value'}"
        This is a sub-set of the JSON syntax. Arrays are not supported.
    @return Options table
    @ingroup Http
    @stability Evolving
 */
PUBLIC MprHash *httpGetOptions(cchar *options);

/**
    Test a field value from an option string.
    @param options Option string of the form: "field='value' field='value'..."
    @param field Field key name
    @param value Test if the field is set to this value
    @param useDefault If true and "field" is not found in options, return true
    @return Allocated value string.
    @ingroup Http
    @stability Evolving
 */
PUBLIC bool httpOption(MprHash *options, cchar *field, cchar *value, int useDefault);

/**
    Remove an option
    @description Remove a property from an options hash
    @param options Options table returned from httpGetOptions
    @param field Property field to remove
    @ingroup Http
    @stability Evolving
 */
PUBLIC void httpRemoveOption(MprHash *options, cchar *field);

/**
    Set an option
    @description Set a property in an options hash
    @param options Options table returned from httpGetOptions
    @param field Property field to set
    @param value Property value to use
    @ingroup Http
    @stability Evolving
 */
PUBLIC void httpSetOption(MprHash *options, cchar *field, cchar *value);

#ifdef __cplusplus
} /* extern C */
#endif

#endif /* _h_HTTP */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.
 */
