#pragma once
#ifndef _COMMON_ERROR_CODE_H_
#define _COMMON_ERROR_CODE_H_

typedef int YondErrCode;

const YondErrCode YOND_ERR_OK = 0; // No error
const YondErrCode YOND_ERR_SOCKET_CREATE = 2000; // Error creating socket
const YondErrCode YOND_ERR_SOCKET_BIND = 2001; // Error binding socket
const YondErrCode YOND_ERR_SOCKET_LISTEN = 2002; // Error listening on socket
const YondErrCode YOND_ERR_SOCKET_ACCEPT = 2003; // Error accepting connection
const YondErrCode YOND_ERR_SOCKET_SEND = 2004; // Error sending data
const YondErrCode YOND_ERR_SOCKET_RECV = 2005; // Error receiving data

const YondErrCode YOND_ERR_EPOLL_CREATE = 2006; // Error creating epoll instance
const YondErrCode YOND_ERR_EPOLL_CTL = 2007; // Error adding or modifying epoll event
const YondErrCode YOND_ERR_EPOLL_WAIT = 2008; // Error waiting for epoll events

#endif // !_COMMON_ERROR_CODE_H_

