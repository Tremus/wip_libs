#ifndef XHL_REQUEST_H
#define XHL_REQUEST_H

/*
===============================================================================
MIT No Attribution

Copyright 2025 Tré Dudman

Permission is hereby granted, free of charge, to any person obtaining a copy of this
software and associated documentation files (the "Software"), to deal in the Software
without restriction, including without limitation the rights to use, copy, modify,
merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
===============================================================================

NOTE: The Windows implementation is a big refactor of the free library found here.
https://github.com/RandyGaul/cute_headers/blob/71928ed90d7b9745bcc3320ab438cf4a0a021eaa/cute_tls.h

I used the code from mid 2024. At some point I fixed at least 2 bugs from that library but unfortunately I forgot what
they were. The nice thing to do would have been to create a github issue and help fix Randys lib, but I was in too much
of a hurry at that time. Hopefully Randy has already found & fixed the problems, or someone else reported it...
From memory, the Windows bugs were:
1. Contacting some hosts (amazon.com IIRC) sometimes returned partial respones. The bug was some encrypted data in some
buffer still needed to be processed, and the bug was some conditon somewhere in a loop. I forgot where...
2. A user of one of my products to fail to establish a connection. The probem was `socket(AF_INET, SOCK_STREAM, 0)`
needed to be `socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)`. I haven't been able to reproduce this error on my machine, but
it appeared to fix this users problems.
With these settings I haven't had any more Windows problems in almost a year.
I don't recall ever having a user report a bug on OSX.

COMPILING:
Add "#define XHL_REQUEST_IMPL" before including in **only one** of your source files

LINKING:
Windows: -lSecur32 -lWs2_32
macOS: -framework Security

SUPPORT:
Windows: 8.1+
macOS: 10.2+
*/

// Return non zero to cancel request/response
// Return zero to continue
// If both 'data' and 'size' are non zero, you may copy 'size' bytes from 'data' into your own response buffer
typedef int (*xreq_callback_t)(
    void*       user,
    const void* data, // May be NULL
    unsigned    size  // May be zero
);

void xrequest(
    const char*     hostname, // eg. www.google.com
    int             port,     // eg. 80, 443, 3000, 8000, 8080
    const char*     req,      // eg. "GET / HTTP/1.1\r\nHost: www.google.com\r\nConnection: close\r\n\r\n"
    unsigned        reqlen,   // eg. strlen(req)
    void*           user,     // ptr passed to callback
    xreq_callback_t cb);      // Callback for receiving HTTP response buffer

#ifndef XREQ_LOGERROR
#define XREQ_LOGERROR(fmt, ...) fprintf(stderr, fmt "\n", __VA_ARGS__)
#endif // XREQ_LOGERROR

#ifndef XREQ_ASSERT
#include <assert.h>
#define XREQ_ASSERT(cond) assert(cond)
#endif

#ifdef XHL_REQUEST_IMPL

#ifdef __APPLE__
#include <Carbon/Carbon.h>
#include <Security/SecureTransport.h>
#include <netdb.h>

#define ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))

#ifdef NDEBUG
#define dbg_printf(arg)
#else
#define dbg_printf(arg) printf arg
char* sslGetProtocolVersionString(SSLProtocol prot)
{
    static char noProt[20];

    switch (prot)
    {
    case kSSLProtocolUnknown:
        return "kSSLProtocolUnknown";
    case kSSLProtocol2:
        return "kSSLProtocol2";
    case kSSLProtocol3:
        return "kSSLProtocol3";
    case kSSLProtocol3Only:
        return "kSSLProtocol3Only";
    case kTLSProtocol1:
        return "kTLSProtocol1";
    case kTLSProtocol11:
        return "kTLSProtocol11";
    case kTLSProtocol12:
        return "kTLSProtocol12";
    case kTLSProtocol1Only:
        return "kTLSProtocol1Only";
    default:
        snprintf(noProt, sizeof(noProt), "Unknown (%d)", (unsigned)prot);
        return noProt;
    }
}

char* sslGetSSLErrString(OSStatus err)
{
    static char noErrStr[20];

    switch (err)
    {
    case noErr:
        return "noErr";
    case memFullErr:
        return "memFullErr";
    case paramErr:
        return "paramErr";
    case unimpErr:
        return "unimpErr";
    case errSecUserCanceled:
        return "errSecUserCanceled";
    case errSSLProtocol:
        return "errSSLProtocol";
    case errSSLNegotiation:
        return "errSSLNegotiation";
    case errSSLFatalAlert:
        return "errSSLFatalAlert";
    case errSSLWouldBlock:
        return "errSSLWouldBlock";
    case ioErr:
        return "ioErr";
    case errSSLSessionNotFound:
        return "errSSLSessionNotFound";
    case errSSLClosedGraceful:
        return "errSSLClosedGraceful";
    case errSSLClosedAbort:
        return "errSSLClosedAbort";
    case errSSLXCertChainInvalid:
        return "errSSLXCertChainInvalid";
    case errSSLBadCert:
        return "errSSLBadCert";
    case errSSLCrypto:
        return "errSSLCrypto";
    case errSSLInternal:
        return "errSSLInternal";
    case errSSLModuleAttach:
        return "errSSLModuleAttach";
    case errSSLUnknownRootCert:
        return "errSSLUnknownRootCert";
    case errSSLNoRootCert:
        return "errSSLNoRootCert";
    case errSSLCertExpired:
        return "errSSLCertExpired";
    case errSSLCertNotYetValid:
        return "errSSLCertNotYetValid";
    case badReqErr:
        return "badReqErr";
    case errSSLClosedNoNotify:
        return "errSSLClosedNoNotify";
    case errSSLBufferOverflow:
        return "errSSLBufferOverflow";
    case errSSLBadCipherSuite:
        return "errSSLBadCipherSuite";
    default:
        snprintf(noErrStr, sizeof(noErrStr), "Unknown (%d)", (unsigned)err);
        return noErrStr;
    }
}

char* sslGetCipherSuiteString(SSLCipherSuite cs)
{
    static char noSuite[40];

    switch (cs)
    {
    case SSL_NULL_WITH_NULL_NULL:
        return "SSL_NULL_WITH_NULL_NULL";
    case SSL_NO_SUCH_CIPHERSUITE:
        return "SSL_NO_SUCH_CIPHERSUITE";

    case 0xC02B:
        return "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256";
    case 0xC02C:
        return "TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384";
    case 0xC02D:
        return "TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256";
    case 0xC02E:
        return "TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384";
    case 0xC02F:
        return "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256";
    case 0xC030:
        return "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384";
    case 0xC031:
        return "TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256";
    case 0xC032:
        return "TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384";

    default:
        snprintf(noSuite, sizeof(noSuite), "Unknown (%d)", (unsigned)cs);
        return noSuite;
    }
}

#endif // NDEBUG

static const SSLCipherSuite suitesAESGCM[] = {
    TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, // 0xC02B
    TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384, // 0xC02C
    TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256,  // 0xC02D
    TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384,  // 0xC02E
    TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,   // 0xC02F
    TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,   // 0xC030
    TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256,    // 0xC031
    TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384,    // 0xC032
};

// R/W. Called out from SSL.
OSStatus SocketRead(
    SSLConnectionRef connection,
    void*            data, // owned by  caller, data , RETURNED
    size_t*          dataLength)    // IN/OUT
{
    size_t   bytesToGo = *dataLength;
    size_t   initLen   = bytesToGo;
    UInt8*   currData  = (UInt8*)data;
    int      sock      = (int)(uint64_t)connection;
    OSStatus rtn       = noErr;
    size_t   bytesRead;
    ssize_t  rrtn;

    *dataLength = 0;

    while (bytesToGo != 0)
    {
        bytesRead = 0;
        rrtn      = read(sock, currData, bytesToGo);

        if (rrtn > 0)
        {
            bytesRead = rrtn;
            rtn       = errSSLWouldBlock;
        }
        else if (rrtn == 0)
        {
            bytesRead = bytesToGo;
            rtn       = errSSLClosedGraceful;
        }
        else // rrtn < 0
        {
            /* this is guesswork... */
            int theErr = errno;
            if ((rrtn == 0) && (theErr == 0))
            {
                /* try fix for iSync */
                rtn = errSSLClosedGraceful;
                // rtn = errSSLClosedAbort;
            }
            else /* do the switch */
            {
                switch (theErr)
                {
                case ENOENT:
                    /* connection closed */
                    rtn       = errSSLClosedGraceful;
                    bytesRead = bytesToGo;
                    break;
                case ECONNRESET:
                    rtn = errSSLClosedAbort;
                    break;
                case 0: /* ??? */
                    rtn = errSSLWouldBlock;
                    break;
                case ERANGE:
                default:
                    rtn = ioErr;
                    break;
                }
            }
            break;
        }
        bytesToGo -= bytesRead;
        currData  += bytesRead;
    }
    *dataLength = initLen - bytesToGo;

    return rtn;
}

OSStatus SocketWrite(SSLConnectionRef connection, const void* data, size_t* dataLength) /* IN/OUT */
{
    OSStatus ortn = noErr;

    size_t bytesSent = 0;
    size_t dataLen   = *dataLength;
    int    sock      = (int)(uint64_t)connection;

    ssize_t length = 0;
    do
    {
        length = write(sock, (char*)data + bytesSent, dataLen - bytesSent);
    }
    while ((length > 0) && ((bytesSent += length) < dataLen));

    if (length <= 0)
    {
        if (errno == EAGAIN)
            ortn = errSSLWouldBlock;
        else
            ortn = ioErr;
    }

    *dataLength = bytesSent;
    return ortn;
}

void xrequest(const char* hostname, int port, const char* req, unsigned reqlen, void* user_ptr, xreq_callback_t cb)
{
    OSStatus           ortn       = noErr;
    int                sock       = 0;
    int                open       = 0;
    struct sockaddr_in addr       = {0};
    struct hostent*    ent        = NULL;
    SSLContextRef      ctx        = NULL;
    SSLProtocol        negVersion = kSSLProtocolUnknown;
    SSLCipherSuite     negCipher  = SSL_NULL_WITH_NULL_NULL;

    // Apparently this is retained, but when I free it, the program errors.
    // If I don't, the program runs fine...?
    SecTrustRef trust = NULL; // peer certs macOS 10.6-10.15

    /* first make sure requested server is there */
    ent = gethostbyname(hostname);
    if (!ent)
    {
        dbg_printf(("gethostbyname failed\n"));
        ortn = ioErr;
    }
    else
    {
        memcpy(&addr.sin_addr, ent->h_addr_list[0], sizeof(addr.sin_addr));

        sock          = socket(AF_INET, SOCK_STREAM, 0);
        addr.sin_port = htons((unsigned short)port);

        addr.sin_family = AF_INET;
        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0)
        {
            dbg_printf(("connect returned error\n"));
            ortn = ioErr;
        }
    }
    if (ortn)
    {
        dbg_printf(("Failed to make connection with server. Status %d; aborting\n", ortn));
        goto cleanup;
    }
    dbg_printf(("connected to server; starting SecureTransport...\n"));

    /*
     * Set up a SecureTransport session.
     * First the standard calls.
     */
    ortn = SSLNewContext(false, &ctx);
    if (ortn)
    {
        dbg_printf(("*** SSLNewContext: %s", sslGetSSLErrString(ortn)));
        goto cleanup;
    }
    ortn = SSLSetIOFuncs(ctx, SocketRead, SocketWrite);
    if (ortn)
    {
        dbg_printf(("*** SSLSetIOFuncs: %s", sslGetSSLErrString(ortn)));
        goto cleanup;
    }
    ortn = SSLSetProtocolVersion(ctx, kTLSProtocol12);
    if (ortn)
    {
        dbg_printf(("*** SSLSetProtocolVersion: %s", sslGetSSLErrString(ortn)));
        goto cleanup;
    }
    ortn = SSLSetConnection(ctx, (SSLConnectionRef)(size_t)sock);
    if (ortn)
    {
        dbg_printf(("*** SSLSetConnection: %s", sslGetSSLErrString(ortn)));
        goto cleanup;
    }

    /* if this isn't set, it isn't checked by APpleX509TP */
    ortn = SSLSetPeerDomainName(ctx, hostname, strlen(hostname) + 1);
    if (ortn)
    {
        dbg_printf(("*** SSLSetPeerDomainName: %s", sslGetSSLErrString(ortn)));
        goto cleanup;
    }

    ortn = SSLSetEnabledCiphers(ctx, suitesAESGCM, ARRLEN(suitesAESGCM));
    if (ortn)
    {
        dbg_printf(("*** SSLSetEnabledCiphers: %s", sslGetSSLErrString(ortn)));
        goto cleanup;
    }

    /*** end options ***/

    dbg_printf(("starting SSL handshake...\n"));

    do
    {
        if (cb(user_ptr, 0, 0))
        {
            ortn = errSecUserCanceled;
            goto cleanup;
        }

        ortn = SSLHandshake(ctx);
    }
    while (ortn == errSSLWouldBlock);

    if (ortn == noErr)
    {
        open = 1;
    }
    else
    {
        dbg_printf(("***Error SSLHandshake: %s\n", sslGetSSLErrString(ortn)));
        goto cleanup;
    }

    /* this works even if handshake failed due to cert chain invalid */
    trust = NULL;
    ortn  = SSLCopyPeerTrust(ctx, &trust);
    if (ortn)
    {
        dbg_printf(("***Error obtaining peer certs: %s\n", sslGetSSLErrString(ortn)));
        goto cleanup;
    }

    ortn = SSLGetNegotiatedCipher(ctx, &negCipher);
    if (ortn)
    {
        dbg_printf(("***Error SSLGetNegotiatedCipher: %s\n", sslGetSSLErrString(ortn)));
        goto cleanup;
    }
    ortn = SSLGetNegotiatedProtocolVersion(ctx, &negVersion);
    if (ortn)
    {
        dbg_printf(("***Error SSLGetNegotiatedProtocolVersion: %s\n", sslGetSSLErrString(ortn)));
        goto cleanup;
    }

    if (cb(user_ptr, 0, 0))
    {
        ortn = errSecUserCanceled;
        goto cleanup;
    }

    dbg_printf(("SSL handshake complete; Sending request message...\n"));
    {
        size_t nprocessed;
        ortn = SSLWrite(ctx, req, reqlen, &nprocessed);
    }
    if (ortn)
    {
        dbg_printf(("***Error SSLWrite: %s\n", sslGetSSLErrString(ortn)));
        goto cleanup;
    }

    if (cb(user_ptr, 0, 0))
    {
        ortn = errSecUserCanceled;
        goto cleanup;
    }

    dbg_printf(("SSL write complete complete; Receiving response...\n"));
    {
        uint8  resbuf[4096]; // decrypted response
        size_t resbuflen;
        size_t avail;
        while (ortn == noErr)
        {
            resbuflen = 0;
            avail     = 0;

            ortn = SSLGetBufferedReadSize(ctx, &avail);
            if (ortn)
            {
                dbg_printf(("***Error SSLGetBufferedReadSize: %s\n", sslGetSSLErrString(ortn)));
                break;
            }
            ortn = SSLRead(ctx, resbuf, sizeof(resbuf), &resbuflen);

            // we expect blocking, not an error
            if (ortn == errSSLWouldBlock)
                ortn = noErr;

            // 'Successful' SSLRead calls returns abort errors on some domains.
            // I'm unsure if this is related to bad code used here, or bad code within macOS.
            if (ortn == errSSLClosedAbort && (errno == ENOENT || errno == ERANGE))
                ortn = errSSLClosedGraceful;

            if (resbuflen > 0)
            {
                int cancel = cb(user_ptr, resbuf, resbuflen);
                if (cancel)
                    ortn = errSecUserCanceled;
            }
        }
    }
    /* connection closed by server or by error */
    dbg_printf(("\nFinished SSLRead with OSStatus %s, errno: %d\n", sslGetSSLErrString(ortn), errno));

    /* convert normal "shutdown" into zero err rtn */
    if (ortn == errSSLClosedGraceful)
        ortn = noErr;

cleanup:
#ifndef NDEBUG
    if (ortn == errSecUserCanceled)
        ortn = noErr;
    XREQ_ASSERT(ortn == noErr);
#endif
    if (ortn != noErr)
        XREQ_LOGERROR("[TLS] WARNING: Finished with code %d", ortn);

    if (open)
        SSLClose(ctx);
    if (sock)
        close(sock);
    if (ctx)
        SSLDisposeContext(ctx);

    dbg_printf(("\n"));
    dbg_printf(("   Attempted  SSL version : %s\n", sslGetProtocolVersionString(kTLSProtocol12)));
    dbg_printf(("   Result                 : %s\n", sslGetSSLErrString(ortn)));
    dbg_printf(("   Negotiated SSL version : %s\n", sslGetProtocolVersionString(negVersion)));
    dbg_printf(("   Negotiated CipherSuite : %s\n", sslGetCipherSuiteString(negCipher)));

    if (trust != NULL)
    {
        CFIndex           numCerts;
        CFIndex           i;
        SecCertificateRef certData;
        numCerts = SecTrustGetCertificateCount(trust);
        // dbg_printf(("   Number of server certs : %ld\n", numCerts));
        for (i = 0; i < numCerts; i++)
        {
            certData = SecTrustGetCertificateAtIndex(trust, i);
            CFRelease(certData);
        }
        // CFRelease(trust);
    }
}
#endif // __APPLE__

#ifdef _WIN32
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE
#endif
#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <WS2tcpip.h>
#include <WinSock2.h>
#include <Windows.h>

#ifndef SECURITY_WIN32
#define SECURITY_WIN32
#endif

#include <schannel.h>
#include <security.h>
// #include <shlwapi.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define TLS_PACKET_QUEUE_MAX_ENTRIES 64
#define TLS_1_KB                     1024
#define TLS_MAX_RECORD_SIZE          (16 * TLS_1_KB)                  // TLS defines records to be up to 16kb.
#define TLS_MAX_PACKET_SIZE          (TLS_MAX_RECORD_SIZE + TLS_1_KB) // Some extra rooms for records split over two packets.

typedef enum TLS_State
{
    TLS_STATE_INVALID_TOKEN                     = -9, // Bad or unsupported cert format.
    TLS_STATE_BAD_CERTIFICATE                   = -8, // Bad or unsupported cert format.
    TLS_STATE_SERVER_ASKED_FOR_CLIENT_CERTS     = -7, // Not supported.
    TLS_STATE_CERTIFICATE_EXPIRED               = -6,
    TLS_STATE_BAD_HOSTNAME                      = -5,
    TLS_STATE_CANNOT_VERIFY_CA_CHAIN            = -4,
    TLS_STATE_NO_MATCHING_ENCRYPTION_ALGORITHMS = -3,
    TLS_STATE_INVALID_SOCKET                    = -2,
    TLS_STATE_UNKNOWN_ERROR                     = -1,
    TLS_STATE_DISCONNECTED                      = 0,
    TLS_STATE_PENDING                           = 1, // Handshake in progress.
    TLS_STATE_CONNECTED                         = 2,
} TLS_State;

#ifdef NDEBUG
#define dbg_fprintf(arg)
#else
#define dbg_fprintf(arg) fprintf arg
#endif

#ifndef NDEBUG
const char* tls_state_string(TLS_State state)
{
    switch (state)
    {
    case TLS_STATE_INVALID_TOKEN:
        return "TLS_STATE_INVALID_TOKEN";
    case TLS_STATE_BAD_CERTIFICATE:
        return "TLS_STATE_BAD_CERTIFICATE";
    case TLS_STATE_SERVER_ASKED_FOR_CLIENT_CERTS:
        return "TLS_STATE_SERVER_ASKED_FOR_CLIENT_CERTS";
    case TLS_STATE_CERTIFICATE_EXPIRED:
        return "TLS_STATE_CERTIFICATE_EXPIRED";
    case TLS_STATE_BAD_HOSTNAME:
        return "TLS_STATE_BAD_HOSTNAME";
    case TLS_STATE_CANNOT_VERIFY_CA_CHAIN:
        return "TLS_STATE_CANNOT_VERIFY_CA_CHAIN";
    case TLS_STATE_NO_MATCHING_ENCRYPTION_ALGORITHMS:
        return "TLS_STATE_NO_MATCHING_ENCRYPTION_ALGORITHMS";
    case TLS_STATE_INVALID_SOCKET:
        return "TLS_STATE_INVALID_SOCKET";
    case TLS_STATE_UNKNOWN_ERROR:
        return "TLS_STATE_UNKNOWN_ERROR";
    case TLS_STATE_DISCONNECTED:
        return "TLS_STATE_DISCONNECTED";
    case TLS_STATE_PENDING:
        return "TLS_STATE_PENDING";
    case TLS_STATE_CONNECTED:
        return "TLS_STATE_CONNECTED";
    }
    return NULL;
}
#endif // NDEBUG

typedef struct TLS_Context
{
    TLS_State state; // Current state of the connection. Negative values are errors.

    ADDRINFOW* AddrInfo;
    SOCKET     sock;
    CredHandle handle;
    CtxtHandle context;

    SecPkgContext_StreamSizes sizes;

    int  received; // Byte count in incoming buffer (ciphertext).
    char incoming[TLS_MAX_PACKET_SIZE];
} TLS_Context;

// Called in a poll-style manner on Windows.
static void tls_recv(TLS_Context* ctx)
{
    fd_set         sockets_to_check;
    struct timeval timeout;

    FD_ZERO(&sockets_to_check);
    FD_SET(ctx->sock, &sockets_to_check);

    timeout.tv_sec  = 0;
    timeout.tv_usec = 0;
    while (select((int)(ctx->sock + 1), &sockets_to_check, NULL, NULL, &timeout) == 1)
    {
        int buflen = (int)sizeof(ctx->incoming) - ctx->received;
        XREQ_ASSERT(buflen > 0);
        int r = recv(ctx->sock, ctx->incoming + ctx->received, buflen, 0);
        if (r == 0)
        {
            // Server disconnected the socket.
            ctx->state = TLS_STATE_DISCONNECTED;
            break;
        }
        else if (r == SOCKET_ERROR)
        {
            // Socket related error.
            ctx->state = TLS_STATE_INVALID_SOCKET;
            break;
        }
        else
        {
            ctx->received += r;
        }

        if (ctx->received == sizeof(ctx->incoming))
        {
            // dbg_fprintf((stderr, "\nbuffer full\n"));
            break;
        }
    }
}

int tls_connect(TLS_Context* ctx, const char* hostname, int port, void* userptr, xreq_callback_t cb)
{
    int   err = 0;
    int   NumChars;
    WCHAR HostName[64];
    NumChars = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        hostname,
        -1,
        HostName,
        sizeof(HostName) / sizeof(HostName[0]));
    if (NumChars == 0)
    {
        XREQ_LOGERROR("[TLS] Error - Failed converting hostname to UTF16");
    }

    // Initialize winsock.
    {
        // https://learn.microsoft.com/en-us/windows/win32/api/winsock/ns-winsock-wsadata
        WSADATA wsadata;
        // https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-wsastartup
        err = WSAStartup(MAKEWORD(2, 2), &wsadata);
        XREQ_ASSERT(err == 0);
        if (err)
        {
            XREQ_LOGERROR("[TLS] Error - WSAStartup returned code %d", err);
            return err;
        }
    }

    // Perform DNS lookup.
    {
        WCHAR     ServiceName[64];
        ADDRINFOW hints;

        swprintf_s(ServiceName, sizeof(ServiceName), L"%d", port);

        ZeroMemory(&hints, sizeof(hints));
        hints.ai_family   = AF_INET;
        hints.ai_flags    = AI_PASSIVE;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        // https://learn.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-getaddrinfow
        err = GetAddrInfoW(HostName, ServiceName, &hints, &ctx->AddrInfo);
        XREQ_ASSERT(err == 0);
        if (err)
        {
            XREQ_LOGERROR("[TLS] Error - getaddrinfo returned: %d", err);
            return err;
        }
    }

    if (cb(userptr, 0, 0))
        return 1;

    // Create a TCP IPv4 socket.
    // https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-socket
    ctx->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ctx->sock == (SOCKET)-1)
    {
        int error = WSAGetLastError();
        XREQ_ASSERT(ctx->sock != -1);
        XREQ_LOGERROR("[TLS] Error - socket returned: %d", error);
        err = 1;
        return err;
    }

    // Set non-blocking IO.
    {
        // https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-ioctlsocket
        u_long non_blocking = 0;
        err                 = ioctlsocket(ctx->sock, (long)FIONBIO, &non_blocking);
        XREQ_ASSERT(err == 0);
        if (err)
        {
            int error = WSAGetLastError();
            XREQ_LOGERROR("[TLS] Error - ioctlsocket returned: %d", error);
            return err;
        }
    }

    // Startup the TCP connection.
    // https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-connect
    err = connect(ctx->sock, ctx->AddrInfo->ai_addr, (int)ctx->AddrInfo->ai_addrlen);
    XREQ_ASSERT(err == 0);
    if (err == SOCKET_ERROR)
    {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK && error != WSAEINPROGRESS)
        {
            err = 1;
            XREQ_LOGERROR("[TLS] Error - connect returned: %d", error);
            ctx->state = ctx->state = TLS_STATE_INVALID_SOCKET;
            return err;
        }
    }

    if (cb(userptr, 0, 0))
        return 1;

    ctx->state = TLS_STATE_PENDING;

    // Initialize a credentials handle for Secure Channel.
    // This is needed for InitializeSecurityContextA.
    SCHANNEL_CRED cred = {0};
    cred.dwVersion     = SCHANNEL_CRED_VERSION;
    cred.dwFlags       = SCH_USE_STRONG_CRYPTO     // Disable deprecated or otherwise weak algorithms (on as default).
                   | SCH_CRED_AUTO_CRED_VALIDATION // Automatically validate server cert (on as default), as opposed
                                                   // to manual verify.
                   | SCH_CRED_NO_DEFAULT_CREDS;    // Client certs are not supported.
    cred.grbitEnabledProtocols = SP_PROT_TLS1_2;   // Specifically pick only TLS 1.2.

    err = AcquireCredentialsHandleW(0, UNISP_NAME_W, SECPKG_CRED_OUTBOUND, 0, &cred, 0, 0, &ctx->handle, 0);
    XREQ_ASSERT(err == 0);
    if (err != SEC_E_OK)
    {
        XREQ_LOGERROR("[TLS] Error - AcquireCredentialsHandleA: %d\n", err);
        return err;
    }

    // Wait for TCP to connect.
    while (1)
    {
        fd_set         sockets_to_check;
        struct timeval timeout;

        FD_ZERO(&sockets_to_check);
        FD_SET(ctx->sock, &sockets_to_check);
        timeout.tv_sec  = 0;
        timeout.tv_usec = 0;
        int numsockets  = select((int)(ctx->sock + 1), NULL, &sockets_to_check, NULL, &timeout);
        if (numsockets == 1)
        {
            int       opt = -1;
            socklen_t len = sizeof(opt);
            if (getsockopt(ctx->sock, SOL_SOCKET, SO_ERROR, (char*)(&opt), &len) >= 0 && opt == 0)
                break;
        }
    }

    bool first_call = true;

    while (ctx->state == TLS_STATE_PENDING)
    {
        if (cb(userptr, 0, 0))
            return 1;

        // TLS handshake algorithm.
        // 1. Call InitializeSecurityContext.
        //    The first call creates a security context.
        //    Subsequent calls update the security context.
        // 2. Check InitializeSecurityContext's return value.
        //    SEC_E_OK                     - Handshake completed, TLS tunnel ready to go.
        //    SEC_I_INCOMPLETE_CREDENTIALS - The server asked for client certs (not supported).
        //    SEC_I_CONTINUE_NEEDED        - Success, keep calling InitializeSecurityContext (and send).
        //    SEC_E_INCOMPLETE_MESSAGE     - Success, continue reading data from the server (recv).
        // 3. Otherwise an error may have been encountered. Set an error state and return.
        // 4. Read data from the server (recv).

        // 1. Call InitializeSecurityContext.
        if (first_call || ctx->received)
        {
            SecBuffer inbuffers[2]  = {0};
            inbuffers[0].BufferType = SECBUFFER_TOKEN;
            inbuffers[0].pvBuffer   = ctx->incoming;
            inbuffers[0].cbBuffer   = ctx->received;
            inbuffers[1].BufferType = SECBUFFER_EMPTY;

            SecBuffer outbuffers[1]  = {0};
            outbuffers[0].BufferType = SECBUFFER_TOKEN;

            SecBufferDesc indesc  = {SECBUFFER_VERSION, ARRAYSIZE(inbuffers), inbuffers};
            SecBufferDesc outdesc = {SECBUFFER_VERSION, ARRAYSIZE(outbuffers), outbuffers};

            DWORD flags = ISC_REQ_USE_SUPPLIED_CREDS | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_CONFIDENTIALITY |
                          ISC_REQ_REPLAY_DETECT | ISC_REQ_SEQUENCE_DETECT | ISC_REQ_STREAM;

            // https://learn.microsoft.com/en-us/windows/win32/api/sspi/nf-sspi-initializesecuritycontextw
            SECURITY_STATUS sec = InitializeSecurityContextW(
                &ctx->handle,
                first_call ? NULL : &ctx->context,
                first_call ? HostName : NULL,
                flags,
                0,
                0,
                first_call ? NULL : &indesc,
                0,
                first_call ? &ctx->context : NULL,
                &outdesc,
                &flags,
                NULL);

            // After the first call we are supposed to re-use the same context.
            first_call = false;

            // Fetch incoming data.
            if (inbuffers[1].BufferType == SECBUFFER_EXTRA)
            {
                memmove(ctx->incoming, ctx->incoming + (ctx->received - inbuffers[1].cbBuffer), inbuffers[1].cbBuffer);
                ctx->received = inbuffers[1].cbBuffer;
            }
            else if (inbuffers[1].BufferType != SECBUFFER_MISSING)
            {
                ctx->received = 0;
            }

            // 2. Check InitializeSecurityContext's return value.
            if (sec == SEC_E_OK)
            {
                // Successfully completed handshake. TLS tunnel is now operational.
                QueryContextAttributesW(&ctx->context, SECPKG_ATTR_STREAM_SIZES, &ctx->sizes);
                ctx->state = TLS_STATE_CONNECTED;
                break;
            }
            else if (sec == SEC_I_CONTINUE_NEEDED)
            {
                // Continue sending data to the server.
                char* buffer = (char*)outbuffers[0].pvBuffer;
                int   size   = outbuffers[0].cbBuffer;
                while (size != 0)
                {
                    int d = send(ctx->sock, buffer, size, 0);
                    if (d <= 0)
                    {
                        break;
                    }
                    size   -= d;
                    buffer += d;
                }
                FreeContextBuffer(outbuffers[0].pvBuffer);
                if (size != 0)
                {
                    // Somehow failed to send() data to server.
                    ctx->state = TLS_STATE_UNKNOWN_ERROR;
                    break;
                }
            }
            else if (sec != SEC_E_INCOMPLETE_MESSAGE)
            {
                if (sec == SEC_E_CERT_EXPIRED)
                    ctx->state = TLS_STATE_CERTIFICATE_EXPIRED;
                else if (sec == SEC_E_WRONG_PRINCIPAL)
                    ctx->state = TLS_STATE_BAD_HOSTNAME;
                else if (sec == SEC_E_UNTRUSTED_ROOT)
                    ctx->state = TLS_STATE_CANNOT_VERIFY_CA_CHAIN;
                else if (sec == SEC_E_ILLEGAL_MESSAGE || sec == SEC_E_ALGORITHM_MISMATCH)
                    ctx->state = TLS_STATE_NO_MATCHING_ENCRYPTION_ALGORITHMS;
                else if (sec == SEC_I_INCOMPLETE_CREDENTIALS)
                    ctx->state = TLS_STATE_SERVER_ASKED_FOR_CLIENT_CERTS; // Client certs are not supported.
                else if (sec == SEC_E_INVALID_TOKEN)
                    ctx->state = TLS_STATE_INVALID_TOKEN;
                else
                    ctx->state = TLS_STATE_UNKNOWN_ERROR;
                break;
            }
            else
            {
                XREQ_ASSERT(sec == SEC_E_INCOMPLETE_MESSAGE);
                // Need to read more bytes.
            }
        }

        if (ctx->received == sizeof(ctx->incoming))
        {
            // Server is sending too much data instead of proper handshake?
            ctx->state = TLS_STATE_UNKNOWN_ERROR;
            break;
        }

        // 4. Read data from the server (recv).
        XREQ_ASSERT(ctx->state == TLS_STATE_PENDING);
        tls_recv(ctx);
    }

    return err;
}

void xrequest(const char* hostname, int port, const char* req, unsigned reqlen, void* user_ptr, xreq_callback_t cb)
{
    TLS_Context* ctx;
    int          err = 0;
    ctx              = calloc(1, sizeof(*ctx));
    if (tls_connect(ctx, hostname, port, user_ptr, cb))
        goto disconnect;

    XREQ_ASSERT(ctx->state == TLS_STATE_CONNECTED);

    if (ctx->state != TLS_STATE_CONNECTED)
    {
        dbg_fprintf((stderr, "Error connecting to to %s with code %s.\n", hostname, tls_state_string(ctx->state)));
        goto disconnect;
    }

    dbg_fprintf((stderr, "Connected to %s!\n", hostname));
    dbg_fprintf((stderr, "Sending HTTP request\n"));

    // Send request.
    {
        const char* req_read_ptr  = req;
        int         req_remaining = reqlen;

        while (req_remaining != 0 && err == 0)
        {
            int use = req_remaining;
            if (use > ctx->sizes.cbMaximumMessage)
                use = ctx->sizes.cbMaximumMessage;

            char wbuffer[TLS_MAX_PACKET_SIZE];
            XREQ_ASSERT(ctx->sizes.cbHeader + ctx->sizes.cbMaximumMessage + ctx->sizes.cbTrailer <= sizeof(wbuffer));

            SecBuffer buffers[3];
            buffers[0].BufferType = SECBUFFER_STREAM_HEADER;
            buffers[0].pvBuffer   = wbuffer;
            buffers[0].cbBuffer   = ctx->sizes.cbHeader;
            buffers[1].BufferType = SECBUFFER_DATA;
            buffers[1].pvBuffer   = wbuffer + ctx->sizes.cbHeader;
            buffers[1].cbBuffer   = (unsigned long)use;
            buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
            buffers[2].pvBuffer   = wbuffer + ctx->sizes.cbHeader + use;
            buffers[2].cbBuffer   = ctx->sizes.cbTrailer;

            memcpy(buffers[1].pvBuffer, req_read_ptr, (size_t)use);

            SecBufferDesc   desc = {SECBUFFER_VERSION, ARRAYSIZE(buffers), buffers};
            SECURITY_STATUS sec  = EncryptMessage(&ctx->context, 0, &desc, 0);
            if (sec != SEC_E_OK)
            {
                // This should not happen, but just in case check it.
                ctx->state = TLS_STATE_UNKNOWN_ERROR;
                err        = -1;
                break;
            }

            int total = (int)(buffers[0].cbBuffer + buffers[1].cbBuffer + buffers[2].cbBuffer);
            int sent  = 0;
            while (sent != total)
            {
                int d = send(ctx->sock, wbuffer + sent, total - sent, 0);
                if (d <= 0)
                {
                    int error = WSAGetLastError();
                    if (error != WSAEWOULDBLOCK && error != WSAEINPROGRESS)
                    {
                        // Error sending data to socket, or server disconnected.
                        ctx->state = TLS_STATE_UNKNOWN_ERROR;
                        err        = -1;
                        break;
                    }
                }
                sent += d;
            }

            if (err == 0)
            {
                req_read_ptr  += use;
                req_remaining -= use;
            }
        }

        if (err != 0)
        {
            dbg_fprintf((stderr, "Failed to send request.\n"));
            goto disconnect;
        }
    }

    dbg_fprintf((stderr, "Receiving HTTP response\n"));
    // Write the full HTTP response to file.
    while (ctx->state == TLS_STATE_CONNECTED && err == 0)
    {
        // Read ciphertext data data from the TCP socket.
        tls_recv(ctx);

        // Buffer may be full
        while (ctx->received && err == 0)
        {
            int       size       = 0;
            SecBuffer buffers[4] = {0};
            XREQ_ASSERT(ctx->sizes.cBuffers == ARRAYSIZE(buffers));

            buffers[0].BufferType = SECBUFFER_DATA;
            buffers[0].pvBuffer   = ctx->incoming;
            buffers[0].cbBuffer   = ctx->received;
            buffers[1].BufferType = SECBUFFER_EMPTY;
            buffers[2].BufferType = SECBUFFER_EMPTY;
            buffers[3].BufferType = SECBUFFER_EMPTY;

            SecBufferDesc desc = {SECBUFFER_VERSION, ARRAYSIZE(buffers), buffers};

            ULONG           type = 0;
            SECURITY_STATUS sec  = DecryptMessage(&ctx->context, &desc, 0, &type);

            XREQ_ASSERT(sec == SEC_E_OK || sec == SEC_E_INCOMPLETE_MESSAGE || sec == SEC_I_CONTEXT_EXPIRED);
            if (sec == SEC_E_OK)
            {
                // Successfully decrypted some data.
                XREQ_ASSERT(buffers[0].BufferType == SECBUFFER_STREAM_HEADER);
                XREQ_ASSERT(buffers[1].BufferType == SECBUFFER_DATA);
                XREQ_ASSERT(buffers[2].BufferType == SECBUFFER_STREAM_TRAILER);

                char* decrypted = (char*)buffers[1].pvBuffer;
                size            = buffers[1].cbBuffer;

                // Byte count used from incoming buffer to decrypt current packet.
                int used = ctx->received - (buffers[3].BufferType == SECBUFFER_EXTRA ? buffers[3].cbBuffer : 0);

                // Consume decrypted buffer
                if (decrypted)
                {
                    err = cb(user_ptr, decrypted, size);
                    if (err)
                    {
                        dbg_fprintf((stderr, "Cancelled\n"));
                        break;
                    }

                    // Remove ciphertext from incoming buffer so next time it starts from beginning.
                    memmove(ctx->incoming, ctx->incoming + used, ctx->received - used);
                    ctx->received -= used;
                }
            }
            else if (sec == SEC_I_CONTEXT_EXPIRED)
            {
                // Server closed TLS connection (but socket is still open).
                // XREQ_LOGERROR("[TLS] WARNING: Server closed the TLS connection. Remaining: %u\n", ctx->received);
                ctx->state    = TLS_STATE_DISCONNECTED;
                ctx->received = 0;
            }
            else if (sec == SEC_I_RENEGOTIATE)
            {
                // Server wants to renegotiate TLS connection, not implemented here.
                XREQ_LOGERROR("[TLS] WARNING: DecryptMessage returned %s", "SEC_I_RENEGOTIATE");

                ctx->state    = TLS_STATE_UNKNOWN_ERROR;
                ctx->received = 0;
            }
            else if (sec != SEC_E_INCOMPLETE_MESSAGE)
            {
                XREQ_LOGERROR("[TLS] WARNING: DecryptMessage returned unkown code %ld", sec);
                ctx->state    = TLS_STATE_UNKNOWN_ERROR;
                ctx->received = 0;
            }
            else
            {
                XREQ_LOGERROR("[TLS] WARNING: DecryptMessage returned %s", "SEC_E_INCOMPLETE_MESSAGE");
                XREQ_ASSERT(sec == SEC_E_INCOMPLETE_MESSAGE);
                // More data needs to be read.
            }

            if (size == 0)
                break;
        }
    }

    // After the server disconnects, there may be lots of remaining data to process
    XREQ_ASSERT(ctx->received == 0);
    dbg_fprintf((stderr, "State %s\n", tls_state_string(ctx->state)));

disconnect:
    if (ctx->state >= 0)
    {
        DWORD type = SCHANNEL_SHUTDOWN;

        SecBuffer inbuffers[1];
        inbuffers[0].BufferType = SECBUFFER_TOKEN;
        inbuffers[0].pvBuffer   = &type;
        inbuffers[0].cbBuffer   = sizeof(type);

        SecBufferDesc indesc = {SECBUFFER_VERSION, ARRAYSIZE(inbuffers), inbuffers};
        ApplyControlToken(&ctx->context, &indesc);

        SecBuffer outbuffers[1];
        outbuffers[0].BufferType = SECBUFFER_TOKEN;

        SecBufferDesc outdesc = {SECBUFFER_VERSION, ARRAYSIZE(outbuffers), outbuffers};
        DWORD         flags   = ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_CONFIDENTIALITY | ISC_REQ_REPLAY_DETECT |
                      ISC_REQ_SEQUENCE_DETECT | ISC_REQ_STREAM;

        HRESULT result = InitializeSecurityContextW(
            &ctx->handle,
            &ctx->context,
            NULL,
            flags,
            0,
            0,
            &outdesc,
            0,
            NULL,
            &outdesc,
            &flags,
            NULL);
        if (result == SEC_E_OK)
        {
            char* buffer = (char*)outbuffers[0].pvBuffer;
            int   size   = outbuffers[0].cbBuffer;
            while (size != 0)
            {
                int d   = send(ctx->sock, buffer, size, 0);
                buffer += d;
                size   -= d;
            }
            FreeContextBuffer(outbuffers[0].pvBuffer);
        }
        shutdown(ctx->sock, SD_BOTH);
    }
    if (ctx->AddrInfo)
        FreeAddrInfoW(ctx->AddrInfo);
    if (ctx->sock != 0 && ctx->sock != (SOCKET)-1ull)
        closesocket(ctx->sock);
    if (ctx->context.dwLower || ctx->context.dwUpper)
        DeleteSecurityContext(&ctx->context);
    if (ctx->handle.dwLower || ctx->handle.dwUpper)
        FreeCredentialsHandle(&ctx->handle);

    free(ctx);
}
#endif // _WIN32

#endif // XHL_REQUEST_IMPL
#endif // XHL_REQUEST_H