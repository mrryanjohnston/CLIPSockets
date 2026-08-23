#ifndef _H_socktls

#pragma once

#define _H_socktls

/* TLS for the socket I/O routers, as the remainder of CLIPS sees it. Include
   this file after clips.h. It uses Environment but does not declare it, and
   socketrtr.h does the same.

   This file has no data about one specific TLS library. The code reaches the
   library through socktlsbe.h. Only socktls.c and the backend files include
   socktlsbe.h. As a result, this header does not put the headers of a library
   into each file that asks if a socket is encrypted.

   If USE_TLS is not defined, this header still compiles. Each entry point
   below then becomes a constant, and callers never need an #ifdef. */

#ifdef USE_TLS

#define SocketIsTLS(sptr) ((sptr)->tls != NULL)

   void                           TLSInitialize(Environment *);

/* The socket router calls these functions for the sockets that it finds are
   encrypted. */
   bool                           TLSFlushSession(Environment *,struct socketRouter *);
   bool                           TLSEmptySession(Environment *,struct socketRouter *);
   void                           TLSCloseSession(Environment *,struct socketRouter *);
   bool                           TLSSetSessionBuffered(Environment *,struct socketRouter *,int);
   int                            TLSSessionPending(Environment *,struct socketRouter *);
   size_t                         TLSSessionRetained(Environment *,struct socketRouter *);

   void                           TLSCreateContextFunction(Environment *,UDFContext *,UDFValue *);
   void                           TLSFreeContextFunction(Environment *,UDFContext *,UDFValue *);
   void                           TLSContextLoadVerifyLocationsFunction(Environment *,UDFContext *,UDFValue *);
   void                           TLSContextSetDefaultVerifyPathsFunction(Environment *,UDFContext *,UDFValue *);
   void                           TLSContextUseCertificateFileFunction(Environment *,UDFContext *,UDFValue *);
   void                           TLSContextUsePrivateKeyFileFunction(Environment *,UDFContext *,UDFValue *);
   void                           TLSContextSetVerifyFunction(Environment *,UDFContext *,UDFValue *);
   void                           TLSContextSetMinProtoVersionFunction(Environment *,UDFContext *,UDFValue *);
   void                           TLSConnectFunction(Environment *,UDFContext *,UDFValue *);
   void                           TLSAcceptFunction(Environment *,UDFContext *,UDFValue *);
   void                           TLSHandshakeFunction(Environment *,UDFContext *,UDFValue *);
   void                           TLSShutdownFunction(Environment *,UDFContext *,UDFValue *);
   void                           TLSPendingFunction(Environment *,UDFContext *,UDFValue *);
   void                           TLSCipherFunction(Environment *,UDFContext *,UDFValue *);
   void                           TLSVersionFunction(Environment *,UDFContext *,UDFValue *);
   void                           TLSVerifyResultFunction(Environment *,UDFContext *,UDFValue *);
   void                           TLSPeerSubjectFunction(Environment *,UDFContext *,UDFValue *);
   void                           TLSBackendFunction(Environment *,UDFContext *,UDFValue *);
   void                           TLSBackendVersionFunction(Environment *,UDFContext *,UDFValue *);

/* DTLS. A datagram session replies to printout and readline through the same
   router as a stream session. A plaintext datagram socket already did this,
   and a handshake must not remove the capability. As a result, the functions
   here are only the ones that have no stream equivalent. They make an
   association where there is no accept(), they move complete records, and
   they control retransmission. */
   void                           DTLSConnectFunction(Environment *,UDFContext *,UDFValue *);
   void                           DTLSAcceptFunction(Environment *,UDFContext *,UDFValue *);
   void                           DTLSSendFunction(Environment *,UDFContext *,UDFValue *);
   void                           DTLSRecvFunction(Environment *,UDFContext *,UDFValue *);
   void                           DTLSTimeoutFunction(Environment *,UDFContext *,UDFValue *);
   void                           DTLSHandleTimeoutFunction(Environment *,UDFContext *,UDFValue *);
   void                           DTLSSetMTUFunction(Environment *,UDFContext *,UDFValue *);
   void                           TLSSupportsDTLSFunction(Environment *,UDFContext *,UDFValue *);

#else /* ! USE_TLS */

/* If the build does not include TLS, there are no sessions. The entry points
   that the socket router calls then become constants. Each call site keeps
   its branch and the compiler removes that branch. As a result, no call site
   needs an #ifdef. */

#define SocketIsTLS(sptr)                    ((void) (sptr), false)
#define TLSInitialize(theEnv)                ((void) (theEnv))
#define TLSFlushSession(theEnv,sptr)         (false)
#define TLSEmptySession(theEnv,sptr)         (false)
#define TLSCloseSession(theEnv,sptr)         ((void) 0)
#define TLSSetSessionBuffered(theEnv,sptr,m) (false)
#define TLSSessionPending(theEnv,sptr)       (0)
#define TLSSessionRetained(theEnv,sptr)      ((size_t) 0)

#endif /* USE_TLS */

#endif /* _H_socktls */
