#ifndef _H_socketrtr

#pragma once

#define _H_socketrtr

#include <stdio.h>

#define SOCKET_ROUTER_DATA USER_ENVIRONMENT_DATA + 1

struct socketRouter
  {
   const char *logicalName;
   FILE *stream;
   struct socketRouter *next;
   int domain;
   int type;
   int fd;
   void *tls;
   /* This flag is true after the code reads from the stream or writes to it.
      A handshake cannot operate on a socket that already moved bytes, because
      the data in the stdio buffer would then cross the start of the
      session. */
   bool ioStarted;
   /* The bytes that the socket did not accept. The code keeps them until a
      flush can send them. A write to a socket with a full send buffer is a
      short write: part of the data went out and the socket refused the
      remainder. That remainder goes here, and the code does not discard it.
      As a result, flush-connection sends the remainder of the same data after
      the peer reads enough data to make space. pending[0..pendingLen) holds
      the data to send, oldest byte first. */
   unsigned char *pending;
   size_t pendingLen;
   size_t pendingCap;
   /* The maximum number of bytes that this socket keeps. A value of 0 is no
      limit, and 0 is the default. With no limit, the code discards no accepted
      data, and that is true for a write of any size. A limit makes the memory
      the maximum instead. A server with peers that stop to read needs that
      choice. set-retained-limit selects between the two. */
   size_t retainedLimit;
  };

struct socketRouterData
  {
   struct socketRouter *ListOfSocketRouters;
  };

struct connectionRouter
  {
   FILE *stream;
   struct connectionRouter *next;
  };

#define SocketRouterData(theEnv) ((struct socketRouterData *) GetEnvironmentData(theEnv,SOCKET_ROUTER_DATA))

   void                           InitializeSocketRouter(Environment *);
   void                           CreateSocketFunction(Environment *,UDFContext *,UDFValue *);
   void                           BindSocketFunction(Environment *,UDFContext *,UDFValue *);
   void                           ListenFunction(Environment *,UDFContext *,UDFValue *);
   void                           AcceptFunction(Environment *,UDFContext *,UDFValue *);
   void                           GetTimeoutFunction(Environment *,UDFContext *,UDFValue *);
   void                           SetTimeoutFunction(Environment *,UDFContext *,UDFValue *);
   void                           SetRetainedLimitFunction(Environment *,UDFContext *,UDFValue *);
   void                           GetRetainedLimitFunction(Environment *,UDFContext *,UDFValue *);
   void                           GetRetainedBytesFunction(Environment *,UDFContext *,UDFValue *);
   void                           ConnectFunction(Environment *,UDFContext *,UDFValue *);
   void                           PollFunction(Environment *,UDFContext *,UDFValue *);
   void                           ShutdownConnectionFunction(Environment *,UDFContext *,UDFValue *);
   void                           FlushConnectionFunction(Environment *,UDFContext *,UDFValue *);
   void                           EmptyConnectionFunction(Environment *,UDFContext *,UDFValue *);
   void                           CloseConnectionFunction(Environment *,UDFContext *,UDFValue *);
   void                           GetsockoptFunction(Environment *,UDFContext *,UDFValue *);
   void                           SetsockoptFunction(Environment *,UDFContext *,UDFValue *);
   void                           GetSocketLogicalNameFunction(Environment *,UDFContext *,UDFValue *);
   void                           FcntlAddStatusFlagsFunction(Environment *, UDFContext *, UDFValue *);
   void                           FcntlRemoveStatusFlagsFunction(Environment *, UDFContext *, UDFValue *);
   void                           SetFullyBufferedFunction(Environment *, UDFContext *, UDFValue *);
   void                           SetNotBufferedFunction(Environment *, UDFContext *, UDFValue *);
   void                           SetLineBufferedFunction(Environment *, UDFContext *, UDFValue *);
   void                           ResolveDomainNameFunction(Environment *, UDFContext *, UDFValue *);
   struct socketRouter            *LogicalNameToSocketRouter(Environment *,const char *);
   struct socketRouter            *FileDescriptorToSocketRouter(Environment *,int);
   struct socketRouter            *GetSocketRouterFromArgument(Environment *,UDFContext *,UDFValue *);
   int                             GenCloseSocket(Environment *,int);

   bool                           FindSocket(Environment *,const char *,void *);
   void                           CloseAllSockets(Environment *);
   void                           RecvfromFunction(Environment *, UDFContext *, UDFValue *);
   void                           SendtoFunction(Environment *, UDFContext *, UDFValue *);

#endif /* _H_socketrtr */
