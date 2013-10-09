/*
 *  LXNetworkUtils.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 20.2.2010.
 *  Copyright 2010 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXNETWORKUTILS_H_
#define _LXNETWORKUTILS_H_

#import "LXBasicTypes.h"
#include <errno.h>

/*
 This file provides a minimal wrapper for Unix/Win32 socket API differences.
 It includes the platform-specific socket API headers, defines "LXSocket",
 and provides macros to account for naming/interface differences in certain calls.
 
 There are also some utility functions provided.
 */



#ifdef __APPLE__
 #define HAVE_SO_NOSIGPIPE_FLAG 1
#endif


#if !defined(LXPLATFORM_WIN)    // ---- POSIX ----

// --- includes ---
#include <sys/types.h>
#include <sys/socket.h>   // for AF_INET, PF_INET, SOCK_STREAM, SOL_SOCKET, SO_REUSEADDR
#include <netinet/in.h>   // for IPPROTO_TCP, sockaddr_in
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <net/if.h>

// --- types ---
typedef int LXSocket;

#define _lxsocket_is_valid(sock_)  ((sock_) > 0)

// --- calls ---
#define _lxsocket_close(sock_)                    close(sock_)

#define _lxsocket_set_nonblocking(sock_, aflag_)  do { \
                                                    int flags = fcntl(sock_, F_GETFL); \
                                                    fcntl(sock_, F_SETFL, (aflag_) ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK)); \
                                                  } while (0)

// --- errors ---
#define _lxsocket_last_error()  (errno)

#define LXSOCKET_EWOULDBLOCK    EWOULDBLOCK


#else                        // ---- WINDOWS ----

// --- includes (Win32) ---
#if defined(_WIN32_WINNT) && (_WIN32_WINNT < 0x0501)
 #undef _WIN32_WINNT
#endif
#if !defined(_WIN32_WINNT)
 #define _WIN32_WINNT 0x0501  // require WinXP or greater (this needs to go before any Win32 headers are included)
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#if defined(__MSVC__)
 #include <wspiapi.h>    // for getaddrinfo()
#endif

// --- types (Win32) ---
typedef SOCKET LXSocket;

#define _lxsocket_is_valid(sock_)  ((sock_) != INVALID_SOCKET)

// --- calls (Win32) ---
#define _lxsocket_close(sock_)                  do {  shutdown(sock_, SD_BOTH);  closesocket(sock_);  } while (0)

#define _lxsocket_set_nonblocking(sock_, aflag_)  do { \
                                                    unsigned long flags = (aflag_) ? 1 : 0; \
                                                    ioctlsocket(sock_, FIONBIO, &flags); \
                                                  } while (0)

// --- errors (Win32) ---
#define _lxsocket_last_error()  WSAGetLastError()

#define LXSOCKET_EWOULDBLOCK    WSAEWOULDBLOCK

#endif // PLATFORM_WIN



// --- utility functions ---


// -- calls that don't need platform-specific implementation --
#define _lxsocket_create_tcp()                          socket(AF_INET, SOCK_STREAM, 0)
#define _lxsocket_create_udp()                          socket(AF_INET, SOCK_DGRAM, 0)
#define _lxsocket_create_with_addrinfo(ai_)             socket(ai_->ai_family, ai_->ai_socktype, ai_->ai_protocol)
#define _lxsocket_connect_with_addrinfo(sock_, ai_)     connect(sock_, ai_->ai_addr, ai_->ai_addrlen)


#ifdef __cplusplus
extern "C" {
#endif

// returns 0 if success or timeout; a standard socket error value otherwise.
// if the connection is dropped (i.e. recv() returns zero), this call will return EPIPE.
// outBytesRead is set to zero on timeout.
LXEXPORT int LXSocketRecvWithTimeout(LXSocket sock, uint8_t *buf, size_t bufSize, size_t *outBytesRead, int flags, double timeout);

// gets time from an NTP server, in Unix epoch.
// function has a timeout of about 3 seconds. will return zero on all errors.
LXEXPORT double LXGetNetworkTimeFromHost(const char *hostname);

#ifdef __cplusplus
}
#endif


#endif //_LXNETWORKUTILS_H_
