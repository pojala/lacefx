/*
 *  LXNetworkUtils.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 25.2.2010.
 *  Copyright 2010 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXNetworkUtils.h"
#include <math.h>
#include <unistd.h>


int LXSocketRecvWithTimeout(LXSocket sock, uint8_t *buf, size_t bufSize, size_t *outBytesRead, int flags, double timeout)
{
    if ( !_lxsocket_is_valid(sock)) return EPIPE;

    fd_set fdset;  // set of file descriptors
    FD_ZERO(&fdset);
    FD_SET(sock, &fdset);
     
    struct timeval tv;  // timeout
    tv.tv_sec = floor(timeout);
    tv.tv_usec = (timeout - floor(timeout)) * (1000*1000);
    
    int n = select(sock+1, &fdset, NULL, NULL, &tv);
    
    if (n == 0) {
        if (outBytesRead) *outBytesRead = 0;
        return 0;
    } else if (n < 0) {
        return _lxsocket_last_error();
    } else {
        int n = recv(sock, buf, bufSize, 0);
        
        if (n == 0) {
            return EPIPE;
        } else if (n < 0) {
            return _lxsocket_last_error();
        } else {
            if (outBytesRead) *outBytesRead = n;
            return 0;
        }
    }
}


double LXGetNetworkTimeFromHost(const char *hostname)
{
    int	maxlen = 256;
    unsigned long buf[maxlen];
    memset(buf, 0, maxlen); 
    
    unsigned char msg[48] = { 010, 0, 0, 0, 0, 0, 0, 0, 0 };  // message sent to NTP server

    // new style socket discovery
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    
    const char *portstr = "123";  // NTP is port 123
    struct addrinfo *addrinfo = NULL;
    if (0 != getaddrinfo(hostname, portstr, &hints, &addrinfo) || !addrinfo) {
        fprintf(stderr, "%s: unknown host %s\n", __func__, hostname);
        return 0.0;
    }
    
    LXSocket sock = _lxsocket_create_with_addrinfo(addrinfo);
    if ( !_lxsocket_is_valid(sock)) {
        fprintf(stderr, "%s: could not open socket\n", __func__);
        return 0.0;
    }    
    _lxsocket_connect_with_addrinfo(sock, addrinfo);
    
    freeaddrinfo(addrinfo);
    addrinfo = NULL;
    
    send(sock, msg, sizeof(msg), 0);
    
    size_t n = 0;
    int err;
    if (0 != (err = LXSocketRecvWithTimeout(sock, (uint8_t *)buf, sizeof(buf), &n, 0, 3.0))) {
        fprintf(stderr, "%s: recv failed with error %i\n", __func__, err);
        return 0.0;
    }
    if (n < 48) {
        fprintf(stderr, "%s: received too little data: %i\n", __func__, (int)n);
        return 0.0;
    }
    
    _lxsocket_close(sock);
    
    // we received 12 big-endian long words
    uint32_t tmit = ntohl(buf[10]);
    uint32_t tmit_fraction = ntohl(buf[11]);
    
    // convert to Unix epoch.
    // NTP is number of seconds since 00:00 UT on 1900.01.01.
    // Unix time is num.seconds since 00:00 UT on 1970.01.01.
    tmit -= 2208988800U;	
    
    double dtime = tmit + ((double)tmit_fraction / UINT32_MAX);    
    return dtime;
}
