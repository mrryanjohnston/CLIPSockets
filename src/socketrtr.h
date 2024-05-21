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
   FILE                          *FindSptr(Environment *,const char *);
   void                           CreateSocketFunction(Environment *,UDFContext *,UDFValue *);
   void                           BindSocketFunction(Environment *,UDFContext *,UDFValue *);
   void                           ListenFunction(Environment *,UDFContext *,UDFValue *);
   void                           AcceptFunction(Environment *,UDFContext *,UDFValue *);
   void                           GetTimeoutFunction(Environment *,UDFContext *,UDFValue *);
   void                           SetTimeoutFunction(Environment *,UDFContext *,UDFValue *);
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
   struct socketRouter            *LogicalNameToSocketRouter(Environment *,const char *);
   struct socketRouter            *FileDescriptorToSocketRouter(Environment *,int);

   bool                           FindSocket(Environment *,const char *,void *);
   void                           CloseAllSockets(Environment *);

#endif /* _H_socketrtr */
