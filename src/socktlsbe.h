#ifndef _H_socktlsbe

#pragma once

#define _H_socktlsbe

/* The interface that each TLS backend implements. socktls.c owns the
   buffers, the tlsio router and the CLIPS functions. It reaches the library
   itself only through the calls below. */

#include <sys/socket.h>

/* The progress of a handshake. */
#define TLS_HANDSHAKE_DONE   1
#define TLS_HANDSHAKE_AGAIN  0
#define TLS_HANDSHAKE_FAIL (-1)
/* A DTLS server answered a ClientHello with a HelloVerifyRequest. It now
   waits for the client to send the cookie back. Nothing is wrong and nothing
   is incomplete, because the association does not exist yet. This constant is
   different from AGAIN because AGAIN shows that one handshake is in progress.
   This constant shows that no handshake started. */
#define TLS_HANDSHAKE_LISTEN_AGAIN 2

/* Read and write give a positive byte count, or one of these constants. They
   never give zero. As a result, no caller can mistake a correct close for a
   short read. */
#define TLS_RESULT_EOF   (-1L)
#define TLS_RESULT_AGAIN (-2L)
#define TLS_RESULT_FAIL  (-3L)

/* The minimum protocol version. These constants have names and not the
   numbers of a library. As a result, the constants of the two families stay
   in the backends. */
#define TLS_VERSION_1_2 2
#define TLS_VERSION_1_3 3
/* DTLS 1.2. It comes after DTLS 1.0, and there is no DTLS 1.1. The version
   numbers agree with the TLS numbers, and TLS 1.1 has no datagram equivalent.
   This file does not offer DTLS 1.0, because DTLS 1.0 has the weaknesses of
   the TLS 1.1 that it comes from. */
#define TLS_VERSION_DTLS_1_2 4

   bool                           TLSBackendStartup(void);
   const char                    *TLSBackendName(void);
   const char                    *TLSBackendVersion(void);

/* Tells if this backend does DTLS in the given direction. There are two
   questions because one backend gives a different answer for each direction.
   BoringSSL has no DTLSv1_listen. It can be a DTLS client, but it cannot run
   the cookie exchange that a server needs. A server without that exchange
   replies to datagrams from addresses that it did not check. s2n has no DTLS
   in either direction. */
   bool                           TLSBackendSupportsDTLS(bool);

/* The second argument asks for a datagram context. The argument is here and
   not at the creation of a session, because the libraries select the
   transport at this point. OpenSSL selects DTLS_client_method at SSL_CTX_new,
   and mbedTLS sets the transport in the configuration. The function gives
   NULL if this backend has no DTLS. */
   void                          *TLSBackendNewContext(Environment *,bool,bool);
   void                           TLSBackendFreeContext(Environment *,void *);
   bool                           TLSBackendLoadVerifyLocations(Environment *,void *,const char *,const char *);
   bool                           TLSBackendLoadSystemTrust(void *);
   bool                           TLSBackendUseCertificateFile(Environment *,void *,const char *);
   bool                           TLSBackendUsePrivateKeyFile(Environment *,void *,const char *);
   bool                           TLSBackendSetVerify(void *,bool);
   bool                           TLSBackendSetMinVersion(void *,int);

   void                          *TLSBackendNewSession(Environment *,void *,int,bool,const char *);
   void                           TLSBackendFreeSession(Environment *,void *);
   int                            TLSBackendHandshake(void *);
   long                           TLSBackendRead(void *,void *,size_t);
   long                           TLSBackendWrite(void *,const void *,size_t);
   int                            TLSBackendPending(void *);
   bool                           TLSBackendShutdown(void *);

/*==========================================================================*/
/* DATAGRAM SESSIONS                                                        */
/*                                                                          */
/* Read and write need no functions of their own. One call is one record,   */
/* and the byte counts and the TLS_RESULT_* constants above already give    */
/* that. A datagram session needs three items that a stream never asks      */
/* about: the identity of the peer before there is an association, the time */
/* to send a flight again, and the quantity of data in one record.          */
/*==========================================================================*/

/* Runs the stateless cookie exchange until a ClientHello arrives with a
   cookie that agrees with the address that sent it. The function then gives
   that address, and the caller can connect the socket to it. The function
   gives the handshake constants: DONE when an association is ready for the
   handshake, LISTEN_AGAIN when a HelloVerifyRequest went out, and AGAIN when
   the socket would block. */
   int                            TLSBackendDTLSListen(void *,struct sockaddr_storage *,socklen_t *);

/* Gives a session a descriptor that the caller connected to the peer. The
   caller uses this function after DTLSListen finds a peer. A library that
   keeps its own record of the transport must be told that its socket now
   sends to one address only. If a server keeps its listen socket free, the
   library must also be told that the session moved to a different socket. */
   bool                           TLSBackendSetSocket(void *,int);

/* Tells if the code can discard an incomplete cookie exchange and start it
   again on a new session, or if it must continue on the first session.

   The protocol permits a new session. The exchange has no state, because the
   cookie is an HMAC of the peer address and not stored data. But the
   libraries do not agree, and they differ in opposite directions. Because of
   this, the code asks the question and does not assume the answer:

     - wolfSSL makes its cookie from state inside the session. A new session
       answers the returned cookie of the client with a new HelloVerifyRequest
       and does not accept the cookie. The two ends then send their first
       flights again and again.

     - LibreSSL runs the exchange through SSL_accept on the same object. A
       session that already processed one ClientHello does not survive the
       handshake that comes after a subsequent successful listen.

   OpenSSL operates correctly with either method. */
   bool                           TLSBackendDTLSRestartable(void);

/* The number of milliseconds until the code must send the last flight again.
   The value is -1 if there is no flight to send again. */
   long                           TLSBackendDTLSTimeout(void *);

/* Sends the last flight again. It gives the handshake constants. */
   int                            TLSBackendDTLSHandleTimeout(void *);

/* The link MTU that a datagram session starts with. Without this value a
   library asks the socket for the path MTU. On loopback the answer is 65535,
   and the records are then too large for a real network. A program that you
   test only on loopback would divide its first record on the first real
   network. One small fixed value operates in the same manner everywhere, and
   dtls-set-mtu changes it.

   This value is here and not in each backend, because it makes a record the
   same size for each library. This is correct only while the backends use the
   same number. */
#define TLS_DEFAULT_LINK_MTU 1500

/* Sets the link MTU. The library calculates its record size from the MTU. */
   bool                           TLSBackendSetMTU(void *,int);

/* The number of bytes of application data in one record at the current MTU.
   The code makes the two buffers from this value. A datagram that goes into a
   buffer that is smaller than the record keeps only the part that fits. The
   code discards the remainder and gives no indication of the loss. */
   int                            TLSBackendMaxPayload(void *);

   const char                    *TLSBackendCipher(void *);
   const char                    *TLSBackendProtocol(void *);
   bool                           TLSBackendVerifyOK(void *);
   bool                           TLSBackendVerifyDescription(void *,char *,size_t);
   bool                           TLSBackendPeerSubject(void *,char *,size_t);

/* Writes to STDERR the data that the backend holds about the most recent
   failure. The name of the operation comes first. */
   void                           TLSBackendReportError(Environment *,const char *);

/* Tells if the given name is an address and not a host name.

   An address has only digits and dots, or it has a colon. The test is not
   exact, and that is on purpose. It only selects one of two comparisons. A
   TRUE answer for "1.2.3.4.5" causes no problem, because the address
   comparison refuses a value that it cannot read. To refuse a wildcard for a
   value that is not a name is the safe direction.

   The backends share this function and do not copy it, because of what it
   selects. A wildcard that matches an address lets a certificate with
   DNS:*.0.0.1 be correct for 127.0.0.1 and for each other address with that
   end. This is one rule to keep correct, and not one rule for each
   backend. */
   bool                           TLSLooksLikeAddress(const char *);

/* Keeps a copy of a path in *slot and frees the data that was there. An
   empty path is not a failure. A caller that gives a file and no directory
   gives a correct value for the directory.

   This is for the backends that cannot read their settings back from the
   library. Those backends must apply the settings again, and they must keep
   the settings for that. */
   bool                           TLSRememberPath(Environment *,char **,const char *);

/* Frees the path in *slot and puts NULL there. A slot that is already NULL is
   not an error.

   This function exists because the memory manager of CLIPS takes the size of
   the block at the free and not only the address. The size of a path is not a
   value that a backend keeps, and the backends that keep paths would each
   calculate it in the same manner. One function does that calculation one
   time.

   That size must be correct, and not only close. For a block of this size, rm
   puts the memory on a free list that it selects by the size, and a later gm2
   for that size gives the block back with no check of its own. A size that is
   too small at the free becomes a block that is too short at the next
   allocation. */
   void                           TLSForgetPath(Environment *,char **);

/* Writes one failure. The parameter detail is the description from the
   library, or NULL if the library gave no description. For NULL the function
   writes the name of the backend, and a FALSE always has some cause.

   The TLSBackendReportError of each backend ends here. The backends differ
   only in how they change a code into text. */
   void                           TLSReportBackendError(Environment *,const char *,const char *);

#define TLS_DEFINE_LAST_ERROR() \
   static int LastError = 0; \
   static void RememberError(int code) \
     { if (code != 0) LastError = code; }

#define TLS_DEFINE_NO_DTLS_SESSION_STUBS() \
   int TLSBackendDTLSListen(void *sess,struct sockaddr_storage *peer,socklen_t *peerLen) \
     { return TLS_HANDSHAKE_FAIL; } \
   bool TLSBackendSetSocket(void *sess,int fd) \
     { return false; } \
   long TLSBackendDTLSTimeout(void *sess) \
     { return -1; } \
   int TLSBackendDTLSHandleTimeout(void *sess) \
     { return TLS_HANDSHAKE_FAIL; } \
   bool TLSBackendSetMTU(void *sess,int mtu) \
     { return false; } \
   int TLSBackendMaxPayload(void *sess) \
     { return 0; }

#endif /* _H_socktlsbe */
