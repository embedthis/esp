/**
    gen.c.tst - General tests for HTTP

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "http.h"

    //  MOB - how to manage
int timeout = 10 * 1000 * 1000;
/************************************ Code ************************************/

static void initHttp()
{
    MprSocket   *sp;

    sp = mprCreateSocket(NULL);
    ttrue(sp);

    /*
        Test if we have network connectivity. If not, then skip further tests.
     */
    if (mprConnectSocket(sp, "www.example.com", 80, 0) < 0) {
        tskip("no internet connection");
        exit(0);
    }
    mprCloseSocket(sp, 0);
}


static void createHttp()
{
    Http        *http;

    http = httpCreate(HTTP_SERVER_SIDE);
    ttrue(http != 0);
    httpDestroy(http);
}


static void basicHttpGet()
{
    Http        *http;
    HttpConn    *conn;
    MprOff      length;
    int         rc, status;

    http = httpCreate(HTTP_CLIENT_SIDE);
    ttrue(http != 0);
    if (tget("TM_DEBUG", 0)) {
        httpStartTracing("stdout:4");
    }
    conn = httpCreateConn(NULL, 0);
    rc = httpConnect(conn, "GET", "http://www.example.com/index.html", NULL);
    ttrue(rc >= 0);
    if (rc >= 0) {
        httpWait(conn, HTTP_STATE_COMPLETE, timeout);
        status = httpGetStatus(conn);
        ttrue(status == 200 || status == 302);
        if (status != 200 && status != 302) {
            mprLog("http test", 0, "HTTP response status %d", status);
        }
        ttrue(httpGetError(conn) != 0);
        length = httpGetContentLength(conn);
        ttrue(length != 0);
    }
    httpDestroy(http);
}


#if UNUSED && ME_COM_SSL && (ME_COM_MATRIXSSL || ME_COM_OPENSSL)
static void secureHttpGet()
{
    Http        *http;
    HttpConn    *conn;
    int         rc, status;

    http = httpCreate(HTTP_CLIENT_SIDE);
    ttrue(http != 0);
    conn = httpCreateConn(NULL, 0);
    ttrue(conn != 0);

    rc = httpConnect(conn, "GET", "https://www.example.com/", NULL);
    ttrue(rc >= 0);
    if (rc >= 0) {
        httpFinalize(conn);
        httpWait(conn, HTTP_STATE_COMPLETE, timeout);
        status = httpGetStatus(conn);
        ttrue(status == 200 || status == 301 || status == 302);
        if (status != 200 && status != 301 && status != 302) {
            mprLog("http test", 0, "HTTP response status %d", status);
        }
    }
    httpDestroy(http);
}
#endif


static void stealSocket()
{
    Http        *http;
    HttpConn    *conn;
    MprSocket   *sp, *prior;
    Socket      fd;
    int         rc, priorState;

    http = httpCreate(HTTP_CLIENT_SIDE);
    ttrue(http != 0);

    /*
        Test httpStealSocket
     */
    conn = httpCreateConn(NULL, 0);
    ttrue(conn != 0);
    rc = httpConnect(conn, "GET", "http://www.example.com/", NULL);
    ttrue(rc >= 0);
    if (rc >= 0) {
        ttrue(conn->sock != 0);
        ttrue(conn->sock->fd != INVALID_SOCKET);
        prior = conn->sock;
        sp = httpStealSocket(conn);
        ttrue(sp != conn->sock);
        ttrue(prior == conn->sock);

        mprNop(prior);

        ttrue(conn->state == HTTP_STATE_COMPLETE);
        ttrue(sp->fd != INVALID_SOCKET);
        ttrue(conn->sock->fd == INVALID_SOCKET);
        mprCloseSocket(sp, 0);
    }


    /*
        Test httpStealSocketHandle
     */
    conn = httpCreateConn(NULL, 0);
    ttrue(conn != 0);
    rc = httpConnect(conn, "GET", "http://www.example.com/", NULL);
    ttrue(rc >= 0);
    if (rc >= 0) {
        ttrue(conn->sock != 0);
        ttrue(conn->sock->fd != INVALID_SOCKET);
        priorState = conn->state;
        fd = httpStealSocketHandle(conn);
        ttrue(conn->state == priorState);
        ttrue(fd != INVALID_SOCKET);
        ttrue(conn->sock->fd == INVALID_SOCKET);
        closesocket(fd);
    }
    httpDestroy(http);
}


int main(int argc, char **argv)
{
    mprCreate(argc, argv, 0);
    mprSetModuleSearchPath(BIN);
    mprVerifySslPeer(NULL, 0);

    if (tget("TM_DEBUG", 0)) {
        mprSetDebugMode(1);
        mprStartLogging("stdout:4", 0);
    }
    initHttp();
    createHttp();
    basicHttpGet();
#if UNUSED && ME_COM_SSL && (ME_COM_MATRIXSSL || ME_COM_OPENSSL)
    secureHttpGet();
#endif
    stealSocket();
    return 0;
};

/*
    @copy   default
    
    Copyright (c) Embedthis Software. All Rights Reserved.
    Copyright (c) Michael O'Brien. All Rights Reserved.
    
    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */

