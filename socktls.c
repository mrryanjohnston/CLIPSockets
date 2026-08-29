/*******************************************************/
/*      "C" Language Integrated Production System      */
/*                                                     */
/*            CLIPS Version ?.??  08/09/26             */
/*                                                     */
/*                    TLS MODULE                       */
/*******************************************************/

/***********************************************************************/
/* Purpose: This file has the buffers, the tlsio I/O router and the    */
/*   CLIPS functions for encrypted sockets. It uses the TLS library    */
/*   only through socktlsbe.h. Because of this, it does not know       */
/*   which TLS library the build links.                                */
/*                                                                     */
/*   A handshake puts a session on a socket, and the socket is then    */
/*   encrypted. Two routers operate on the one list of sockets, but    */
/*   their query callbacks do not agree about which sockets they own.  */
/*   Because of this, a session on a socket moves that socket from     */
/*   one router to the other. The logical name does not change, and    */
/*   printout and readline continue to operate through the handshake.  */
/*                                                                     */
/* Principal Programmer(s):                                            */
/*      Ryan Johnston                                                  */
/*                                                                     */
/* Revision History:                                                   */
/*                                                                     */
/*      ?.??: Added this file.                                         */
/*                                                                     */
/**********************************************************************/

#define _POSIX_C_SOURCE 200112L

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include "setup.h"

#include "argacces.h"
#include "constant.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#include "memalloc.h"
#include "multifld.h"
#include "prntutil.h"
#include "router.h"
#include "symbol.h"
#include "sysdep.h"

#include "socketrtr.h"
#include "socktls.h"
#include "socktlsbe.h"

#ifdef USE_TLS

#define TLS_DATA (USER_ENVIRONMENT_DATA + 2)

/* A TLS record holds a maximum of about 16k. But these buffers hold CLIPS
   data and not records, and readline reads one character at a time. A size of
   4k keeps a usual request or reply in one write. It also keeps the memory
   cost of each socket small. */
#define TLS_BUFFER_SIZE 4096

/* A datagram session gets its buffer size from the backend. It does this
   because a buffer that is smaller than the record keeps only the part of the
   record that fits and discards the remainder. The session gives no
   indication of the loss. The test tests/integration/udp-router-io.clp shows
   the same loss on a plaintext UDP socket. There the stdio buffer sets the
   point where the data stops. This constant is the maximum buffer size. It
   also limits a backend that reports an incorrect record size. A DTLS record
   holds a maximum of 2^14 bytes of application data. */
#define TLS_DGRAM_BUFFER_MAX 16384

/* The maximum length of a peer subject and of a verification description. */
#define TLS_TEXT_SIZE 512

/* The message to write when a handshake gets the incorrect type of socket.
   The two messages point in opposite directions. Because the socket type
   alone does not identify the correct message, the caller gives the message
   to ValidateForHandshake. */
#define TLS_WRONG_TYPE_STREAM \
   ": only stream sockets can be upgraded; a datagram socket needs DTLS\n"
#define TLS_WRONG_TYPE_DGRAM \
   ": only datagram sockets can carry DTLS; a stream socket wants tls-connect or tls-accept\n"

struct tlsContext
  {
   long long handle;
   void *backendContext;
   /* The number of references to this context. There is one reference for
      the handle that the caller holds, and one reference for each session
      that this context makes. The function tls-free-context removes the first
      reference. The code frees the backend context when the last reference
      goes away.

      Only one of the four libraries is safe without this count. OpenSSL
      counts the references to an SSL_CTX itself, and a session keeps that
      SSL_CTX in memory. GnuTLS, mbedTLS and s2n keep the context in the
      session as a plain pointer, and they copy no data out of it. If you free
      such a context while a connection operates, that connection then reads
      memory that is free. This is possible inside certificate verification if
      the handshake is not complete. To free a context after you make
      connections from it is a usual operation. As a result, this count makes
      that operation safe and does not forbid it. */
   unsigned refCount;
   /* This flag is true if this context asks for verification of the peer.
      The code keeps the flag here and also in the library. It does this
      because the libraries do not agree on the result when they make no
      check. OpenSSL reports X509_V_OK, because no check failed. But
      tls-verify-result must tell a check that passed from a check that did
      not run. A record of the request is the one answer that all backends can
      give.

      The role sets this flag when the code makes the context. Only
      tls-context-set-verify changes the flag, and only that function tells
      the backend. */
   bool verifyRequired;
   /* This flag is true if this context makes DTLS sessions. The code sets
      the flag when it makes the context, because each library selects the
      transport at that point. The code keeps the flag here so that it can
      report a stream context that goes to dtls-connect as an error. Without
      the flag, the error shows only later, in a handshake that cannot
      succeed. */
   bool datagram;
   struct tlsContext *next;
  };

struct tlsSession
  {
   void *backendSession;
   /* The context that made this session. The session holds a reference to
      that context. */
   struct tlsContext *owner;
   /* The read buffer. The code allocates it and does not keep it in this
      structure, because a datagram session gets its size from the record size
      that the backend reports. The buffer is bufSize bytes long and keeps
      that size. A read gives only the data that the backend supplies. As a
      result, the buffer never holds more data than the caller asked for. */
   unsigned char *inBuf;
   size_t inLen;
   size_t inPos;
   /* One character of pushback. This is sufficient for all callers, because
      each UnreadRouter call in CLIPS pushes back only the character that it
      read last. */
   int pushback;
   /* The data that the session must still send to the peer, oldest byte
      first. This buffer can grow, but inBuf cannot. The session keeps a write
      that it cannot send and does not discard it. As a result, the quantity
      of data that the session holds is a function of how quickly the peer
      reads, and not of a size in this file. The field outCap holds the
      allocated size. It is not bufSize, because only this buffer changes
      size. */
   unsigned char *outBuf;
   size_t outLen;
   size_t outCap;
   /* The number of bytes that the code gave to a backend write that could
      not send them at that time. It is 0 if no write is incomplete. The next
      attempt must give the backend the same number of bytes again.
      FlushOutput gives the cause. */
   size_t writeOffered;
   /* The socket that holds this session. The session needs the socket for
      one item of data that the session does not have: the maximum quantity of
      data that the session can keep. The router holds that limit, because
      set-retained-limit gives the same answer for an encrypted socket and for
      a plaintext socket. One copy of the limit prevents two values that do
      not agree.

      This pointer is safe. The session does not continue after the socket
      goes away. The code either puts a session on the socket that it made the
      session for, or it frees the session before it puts the session on a
      socket. The code never moves a session to a different socket. */
   struct socketRouter *owningSocket;
   /* The size of the read buffer. On a stream it is also the fill level at
      which the code sends the write buffer. */
   size_t bufSize;
   /* The quantity of data that the code can put in outBuf before it must
      send the data. On a stream this is bufSize. On a datagram it is the
      quantity of application data in one record at the current MTU. A flush
      is then always a write that the backend can send in one record. This
      field is separate from bufSize, because a caller can change the MTU
      after the session exists. The code makes the buffers large enough for
      the largest record that DTLS permits. As a result, a new MTU never moves
      memory that the caller is in the middle of filling. */
   size_t maxPayload;
   int bufMode;
   bool handshakeDone;
   bool asClient;
   /* This flag is true if this is a DTLS session. The code copies it from
      the context. The flag makes a write one datagram and not part of a
      stream. */
   bool datagram;
   /* This flag is true for a server session that is in the cookie exchange.
      At that time there is no association to give to a handshake. The code
      puts the session on the socket in this condition. A subsequent call can
      then continue the exchange and does not start it again.

      Some libraries keep state for the exchange, although the protocol gives
      the exchange no state. wolfSSL makes its cookie from data in the session.
      A new session there answers the cookie of the client with a new
      HelloVerifyRequest and does not accept the cookie. The two ends then send
      their first flights again and again until the budget is empty. To keep
      the session also removes one allocation from each cycle of a poll loop on
      the backends that permit both designs. */
   bool listening;
   /* The code copies this flag from the context when it makes the session.
      It does this because a caller can change the context later, or free it.
      This connection uses the setting that was in effect when the session
      started. */
   bool verifyRequired;
   /* This flag is true if close_notify went to the peer. The first
      SSL_shutdown sends close_notify and returns. A second SSL_shutdown waits
      for the reply of the peer. On a blocking socket this can wait for a peer
      that never replies. */
   bool shutdownSent;
  };

struct tlsData
  {
   struct tlsContext *ListOfContexts;
   long long NextHandle;
  };

#define TLSData(theEnv) ((struct tlsData *) GetEnvironmentData(theEnv,TLS_DATA))

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static void                    DeallocateTLSData(Environment *);
static bool                    FindTLSSocket(Environment *,const char *,void *);
static void                    WriteTLSSocket(Environment *,const char *,const char *,void *);
static int                     ReadTLSSocket(Environment *,const char *,void *);
static int                     UnreadTLSSocket(Environment *,const char *,int,void *);
static struct socketRouter    *EncryptedRouter(Environment *,const char *);
static bool                    FlushOutput(Environment *,struct tlsSession *);
static size_t                  RetainedLimit(struct tlsSession *);
static bool                    GrowOutput(Environment *,struct tlsSession *);
static bool                    AppendOutput(Environment *,struct tlsSession *,unsigned char);
static void                    WriteBytes(Environment *,struct tlsSession *,const char *,size_t);
static int                     ReadChar(Environment *,struct tlsSession *);
static struct tlsContext      *HandleToContext(Environment *,long long);
static void                    ReleaseContext(Environment *,struct tlsContext *);
static void                    FreeSession(Environment *,struct tlsSession *);
static struct tlsContext      *ContextArgument(Environment *,UDFContext *,UDFValue *,UDFValue *,const char *);
static struct tlsSession      *SessionArgument(Environment *,UDFContext *,UDFValue *,UDFValue *,const char *);
static bool                    ValidateForHandshake(Environment *,struct socketRouter *,int,const char *,const char *);
static struct socketRouter    *HandshakeSocket(Environment *,UDFContext *,UDFValue *,UDFValue *,int,const char *,const char *);
static void                    RefreshPayloadCap(struct tlsSession *);
static struct tlsSession      *DatagramSession(Environment *,UDFContext *,UDFValue *,UDFValue *,const char *);
static bool                    Handshake(Environment *,struct socketRouter *,struct tlsSession *,const char *);
static bool                    RequireDatagramContext(Environment *,struct tlsContext *,UDFValue *,const char *,const char *);
static void                    StartSession(Environment *,struct tlsContext *,struct socketRouter *,UDFValue *,bool,const char *,const char *);
static void                    ClientHandshake(Environment *,UDFContext *,UDFValue *,bool,const char *);
static void                    ContextFileFunction(Environment *,UDFContext *,UDFValue *,bool (*)(Environment *,void *,const char *),const char *);
static void                    SessionTextFunction(Environment *,UDFContext *,UDFValue *,const char *(*)(void *),const char *);

/**********************************************************/
/* TLSInitialize: Starts the TLS library, the context     */
/*   table for the environment, and the tlsio router.     */
/*   InitializeSocketRouter calls it one time.            */
/**********************************************************/
void TLSInitialize(
		Environment *theEnv)
{
	AllocateEnvironmentData(
			theEnv,
			TLS_DATA,
			sizeof(struct tlsData),
			DeallocateTLSData);

	TLSData(theEnv)->ListOfContexts = NULL;
	TLSData(theEnv)->NextHandle = 1;

	if (! TLSBackendStartup())
	{
		WriteString(theEnv,STDERR,"Could not initialize the TLS library\n");
		return;
	}

	// This router has no exit callback. ExitRouter calls the exit callback
	// of each active router, and not only the callback of the router that
	// owns the name. socketio already closes each socket in the list,
	// including the encrypted sockets. An exit callback here would close
	// them a second time.
	AddRouter(theEnv,"tlsio",0,FindTLSSocket,
			WriteTLSSocket,ReadTLSSocket,UnreadTLSSocket,NULL,NULL);
}

/**************************************************************************/
/* DeallocateTLSData: Removes the reference of the list from each context */
/*   that is still open when the environment goes away. Each socket owns  */
/*   its session and frees that session. The socket router closes each    */
/*   socket when it shuts down. As a result, the reference of the list is */
/*   usually the last reference, and this function frees each context.    */
/**************************************************************************/
static void DeallocateTLSData(
		Environment *theEnv)
{
	struct tlsContext *theContext, *nextContext;

	theContext = TLSData(theEnv)->ListOfContexts;

	while (theContext != NULL)
	{
		nextContext = theContext->next;
		ReleaseContext(theEnv,theContext);
		theContext = nextContext;
	}

	TLSData(theEnv)->ListOfContexts = NULL;
}

/**************************************************************************/
/* ReleaseContext: Removes one reference. If that reference was the last  */
/*   one, this function frees the context. The code calls it for the      */
/*   handle when tls-free-context runs, and for each session when the     */
/*   session closes. The sequence of the two calls is not important.      */
/**************************************************************************/
static void ReleaseContext(
		Environment *theEnv,
		struct tlsContext *theContext)
{
	if (theContext == NULL) return;

	if (theContext->refCount > 0) theContext->refCount--;

	if (theContext->refCount > 0) return;

	TLSBackendFreeContext(theEnv,theContext->backendContext);
	rm(theEnv,theContext,sizeof(struct tlsContext));
}

/**************************************************************************/
/* FreeSession: Frees a session and all the memory that it owns. This is  */
/*   one function because the sequence is important and there is more     */
/*   than one caller. A handshake that failed and a connection that       */
/*   closes both come here.                                               */
/*                                                                        */
/*   The code frees the backend session before it removes the context     */
/*   reference, and never after it. The session holds that reference. On  */
/*   three of the four backends, the backend session reads the context    */
/*   until the code frees the backend session.                            */
/**************************************************************************/
static void FreeSession(
		Environment *theEnv,
		struct tlsSession *session)
{
	if (session == NULL) return;

	TLSBackendFreeSession(theEnv,session->backendSession);

	ReleaseContext(theEnv,session->owner);

	if (session->inBuf != NULL)  rm(theEnv,session->inBuf,session->bufSize);
	if (session->outBuf != NULL) rm(theEnv,session->outBuf,session->outCap);

	rm(theEnv,session,sizeof(struct tlsSession));
}

/*==========================*/
/* ROUTER CALLBACKS         */
/*==========================*/

/*************************************************************************/
/* EncryptedRouter: Gives the socket for a logical name, but only if     */
/*   this router owns that socket. Each callback starts here. A socket   */
/*   can stop to be encrypted between the query and the operation, and   */
/*   this check prevents the use of such a socket as an encrypted one.   */
/*************************************************************************/
static struct socketRouter *EncryptedRouter(
		Environment *theEnv,
		const char *logicalName)
{
	struct socketRouter *sptr;

	sptr = LogicalNameToSocketRouter(theEnv,logicalName);

	if ((sptr == NULL) || (sptr->tls == NULL)) return NULL;

	return sptr;
}

/*************************************************************************/
/* FindTLSSocket: The query callback. It claims the sockets that         */
/*   socketio does not claim: the sockets that hold a session, of the    */
/*   two socket types.                                                   */
/*                                                                       */
/*   It includes a datagram socket because a plaintext datagram socket   */
/*   already replies to printout and readline through socketio. The file */
/*   examples/server-udp.bat shows this, and a handshake must not remove */
/*   the capability. The functions dtls-send and dtls-recv are also      */
/*   available for callers that need the record limits. A character      */
/*   stream cannot show those limits.                                    */
/*************************************************************************/
static bool FindTLSSocket(
		Environment *theEnv,
		const char *logicalName,
		void *context)
{
	struct socketRouter *sptr;

	sptr = LogicalNameToSocketRouter(theEnv,logicalName);

	return (sptr != NULL) && (sptr->tls != NULL);
}

/***********************************************************/
/* WriteTLSSocket: The write callback of the tlsio router. */
/***********************************************************/
static void WriteTLSSocket(
		Environment *theEnv,
		const char *logicalName,
		const char *str,
		void *context)
{
	struct socketRouter *sptr;

	sptr = EncryptedRouter(theEnv,logicalName);
	if (sptr == NULL) return;

	WriteBytes(theEnv,(struct tlsSession *) sptr->tls,str,strlen(str));
}

/**********************************************************/
/* ReadTLSSocket: The read callback of the tlsio router.  */
/**********************************************************/
static int ReadTLSSocket(
		Environment *theEnv,
		const char *logicalName,
		void *context)
{
	struct socketRouter *sptr;

	sptr = EncryptedRouter(theEnv,logicalName);
	if (sptr == NULL) return EOF;

	return ReadChar(theEnv,(struct tlsSession *) sptr->tls);
}

/************************************************************/
/* UnreadTLSSocket: The unread callback of the tlsio        */
/* router.                                                  */
/************************************************************/
static int UnreadTLSSocket(
		Environment *theEnv,
		const char *logicalName,
		int ch,
		void *context)
{
	struct socketRouter *sptr;
	struct tlsSession *session;

	sptr = EncryptedRouter(theEnv,logicalName);
	if (sptr == NULL) return EOF;

	session = (struct tlsSession *) sptr->tls;

	// The code refuses a second pushback. stdio does the same, and no caller
	// in CLIPS asks for a second pushback.
	if (session->pushback != -1) return EOF;

	session->pushback = ch;

	return ch;
}

/*==========================*/
/* BUFFERING                */
/*==========================*/

/*************************************************************************/
/* FlushOutput: Gives the write buffer to the backend. The code moves    */
/*   the data that the backend did not send to the start of the buffer.  */
/*   A subsequent attempt then continues from that point. It does not    */
/*   send the data a second time and it does not discard the data.       */
/*************************************************************************/
static bool FlushOutput(
		Environment *theEnv,
		struct tlsSession *session)
{
	size_t sent = 0;
	long n;

	// A datagram write sends all the data or no data. The backend cannot
	// send one half of a record and continue from that point. As a result,
	// the stream code below has no meaning here, because that code continues
	// from the end of a short write. The fill level is already limited to
	// one record, and the data here goes out in one datagram or not at all.
	if (session->datagram)
	{
		if (session->outLen == 0) return true;

		n = TLSBackendWrite(session->backendSession,session->outBuf,session->outLen);

		if (n > 0)
		{
			session->outLen = 0;
			return true;
		}

		if (n == TLS_RESULT_AGAIN) errno = EAGAIN;

		// The code keeps the data and does not discard it. A caller that
		// flushes again after the socket is ready sends the same record, and
		// not a shorter one.
		return false;
	}

	while (sent < session->outLen)
	{
		size_t want = session->outLen - sent;

		// A backend that got a length and could not send it at that time made
		// a record of that size. It is in the middle of the transmission of
		// that record. All of these libraries need the next attempt to give
		// the same length again. mbedTLS loses data if the length changes: it
		// completes the record that it holds, but it reports the new length as
		// written. A longer second attempt then counts bytes that mbedTLS
		// never received.
		//
		// This length is less than the full quantity of data only because the
		// buffer grew. The code wrote more data after the refusal, and that
		// data waits behind the record that the backend holds. It goes out on
		// the next cycle.
		if ((session->writeOffered > 0) && (session->writeOffered < want))
		{ want = session->writeOffered; }

		n = TLSBackendWrite(session->backendSession,
				session->outBuf + sent,
				want);

		if (n > 0)
		{
			// A byte count shows that the backend took the full record.
			// Nothing is incomplete, and the next attempt can use any
			// size.
			session->writeOffered = 0;
			sent += (size_t) n;
			continue;
		}

		memmove(session->outBuf,session->outBuf + sent,session->outLen - sent);
		session->outLen -= sent;

		if (n == TLS_RESULT_AGAIN)
		{
			// The code records this length against the start of the buffer.
			// The memmove above made that position the start of the data
			// that it gave to the backend.
			session->writeOffered = want;

			// This is the same result as a non-blocking write through
			// stdio. Code that already controls that condition also
			// controls this one.
			errno = EAGAIN;
		}
		else
		{ session->writeOffered = 0; }

		return false;
	}

	session->outLen = 0;
	session->writeOffered = 0;

	return true;
}

/*************************************************************************/
/* RetainedLimit: The maximum quantity of data that this session can     */
/*   keep for a peer that does not read. A value of 0 is no limit, and 0 */
/*   is the default.                                                     */
/*                                                                       */
/*   The function reads the limit from the socket and does not copy it   */
/*   into the session. As a result, set-retained-limit does not need to  */
/*   know if the socket is encrypted.                                    */
/*************************************************************************/
static size_t RetainedLimit(
		struct tlsSession *session)
{
	if (session->owningSocket == NULL) return 0;

	return session->owningSocket->retainedLimit;
}

/*************************************************************************/
/* GrowOutput: Makes space for more data that the peer did not accept.   */
/*                                                                       */
/*   The function multiplies the size by two. As a result, the number of */
/*   copies to fill a buffer of any size stays small. The size never     */
/*   goes above the limit, because the code cannot use memory above the  */
/*   limit.                                                              */
/*************************************************************************/
static bool GrowOutput(
		Environment *theEnv,
		struct tlsSession *session)
{
	size_t cap;
	size_t limit;
	unsigned char *bigger;

	cap = (session->outCap == 0) ? TLS_BUFFER_SIZE : session->outCap * 2;

	limit = RetainedLimit(session);
	if ((limit > 0) && (cap > limit)) cap = limit;

	if (cap <= session->outCap) return false;

	bigger = (unsigned char *) gm2(theEnv,cap);

	if (session->outLen > 0)
	{ memcpy(bigger,session->outBuf,session->outLen); }

	if (session->outBuf != NULL)
	{ rm(theEnv,session->outBuf,session->outCap); }

	session->outBuf = bigger;
	session->outCap = cap;

	return true;
}

/*************************************************************************/
/* AppendOutput: Adds one byte to the data that the session must send.   */
/*   It gives false if it cannot keep the byte. This is the limit in     */
/*   operation and it is not an error.                                   */
/*                                                                       */
/*   A new limit that is less than the quantity of data in the buffer    */
/*   does not discard the bytes that the session accepted before. It     */
/*   only stops the session from more bytes. RetainTail applies the same */
/*   rule to a plaintext socket, and the two must agree.                 */
/*************************************************************************/
static bool AppendOutput(
		Environment *theEnv,
		struct tlsSession *session,
		unsigned char ch)
{
	size_t limit;

	limit = RetainedLimit(session);

	if ((limit > 0) && (session->outLen >= limit)) return false;

	if (session->outLen == session->outCap)
	{
		if (! GrowOutput(theEnv,session)) return false;
	}

	session->outBuf[session->outLen++] = ch;

	return true;
}

static void WriteBytes(
		Environment *theEnv,
		struct tlsSession *session,
		const char *data,
		size_t len)
{
	size_t i;
	size_t cap;
	bool blocked = false;

	if (len == 0) return;

	cap = session->datagram ? session->maxPayload : session->bufSize;

	for (i = 0; i < len; i++)
	{
		// When the buffer is full, the code sends the data to make space. It
		// tries this one time in each call. If the peer stopped to read, more
		// attempts would do one write for each byte. The result cannot change
		// in the middle of one printout.
		if ((! blocked) && (session->outLen >= cap))
		{
			if (! FlushOutput(theEnv,session))
			{
				// A datagram is one record. The backend sends all of it or
				// none of it, and there is nothing to add to. The buffer
				// holds the record that could not go out. The code refuses
				// the remainder of this write and does not add that
				// remainder to the record.
				if (session->datagram) return;

				blocked = true;
			}
		}

		// The code keeps the byte and does not discard it. A caller cannot
		// control how quickly the peer reads, and all the data that printout
		// supplied is still available for the subsequent flush. Only a limit
		// that the user sets stops this, and it stops it at this point.
		if (! AppendOutput(theEnv,session,(unsigned char) data[i])) return;

		if ((session->bufMode == _IOLBF) && (data[i] == '\n') && (! blocked))
		{
			if (! FlushOutput(theEnv,session)) blocked = true;
		}
	}

	if ((session->bufMode == _IONBF) && (! blocked))
	{ FlushOutput(theEnv,session); }
}

/*************************************************************************/
/* ReadChar: Gives one character. It takes the character from the        */
/*   pushback field first, then from the read buffer, then from the      */
/*   connection.                                                         */
/*************************************************************************/
static int ReadChar(
		Environment *theEnv,
		struct tlsSession *session)
{
	long n;

	if (session->pushback != -1)
	{
		int ch = session->pushback;
		session->pushback = -1;
		return ch;
	}

	if (session->inPos == session->inLen)
	{
		// The code sends the data in the write buffer before it waits for the
		// peer. A request that stays in the write buffer while the program
		// waits for its reply is a deadlock, and a deadlock is difficult to
		// find.
		if (session->outLen > 0)
		{ FlushOutput(theEnv,session); }

		n = TLSBackendRead(session->backendSession,session->inBuf,session->bufSize);

		if (n <= 0)
		{
			if (n == TLS_RESULT_AGAIN) errno = EAGAIN;
			return EOF;
		}

		session->inLen = (size_t) n;
		session->inPos = 0;
	}

	return session->inBuf[session->inPos++];
}

/*==========================================*/
/* ENTRY POINTS FOR THE SOCKET ROUTER       */
/*==========================================*/

/*******************************************************************/
/* TLSFlushSession: Does flush-connection for an encrypted socket. */
/*******************************************************************/
bool TLSFlushSession(
		Environment *theEnv,
		struct socketRouter *sptr)
{
	return FlushOutput(theEnv,(struct tlsSession *) sptr->tls);
}

/*******************************************************************/
/* TLSEmptySession: Does empty-connection for an encrypted socket. */
/*                                                                 */
/*   One character at a time empties a datagram session as well as */
/*   a stream session. ReadChar fills the buffer from full         */
/*   records. The loop stops when it reads the last record and the */
/*   next read finds no data.                                      */
/*******************************************************************/
bool TLSEmptySession(
		Environment *theEnv,
		struct socketRouter *sptr)
{
	struct tlsSession *session = (struct tlsSession *) sptr->tls;

	while (ReadChar(theEnv,session) != EOF) { /* discard */ }

	return true;
}

/**************************************************************************/
/* TLSCloseSession: Stops the session and frees it. This function does    */
/*   not close the descriptor. The socket router closes the descriptor,   */
/*   as it does for each socket, after this function sends its last data. */
/**************************************************************************/
void TLSCloseSession(
		Environment *theEnv,
		struct socketRouter *sptr)
{
	struct tlsSession *session = (struct tlsSession *) sptr->tls;

	if (session == NULL) return;

	if (session->outLen > 0)
	{ FlushOutput(theEnv,session); }

	// The code does this only if tls-shutdown did not send close_notify
	// before. A second call makes OpenSSL wait for the close_notify of the
	// peer. On a blocking socket the program then waits for a peer that
	// usually closes at the same time.
	if (! session->shutdownSent)
	{ TLSBackendShutdown(session->backendSession); }

	FreeSession(theEnv,session);

	// This assignment gives the socket back to socketio. Code that still
	// has the name then finds a plaintext socket, and not a session that is
	// free.
	sptr->tls = NULL;
}

/***************************************************************************/
/* TLSSetSessionBuffered: Does the set-*-buffered functions. The code uses */
/*   the mode constants of stdio. As a result, the socket router can give  */
/*   its argument to this function without a change.                       */
/***************************************************************************/
bool TLSSetSessionBuffered(
		Environment *theEnv,
		struct socketRouter *sptr,
		int mode)
{
	struct tlsSession *session = (struct tlsSession *) sptr->tls;

	if ((mode != _IOFBF) && (mode != _IOLBF) && (mode != _IONBF))
	{ return false; }

	// A change to a mode with more buffering keeps the data in the buffer.
	// The code would then send that data under the old mode, and the user
	// would think that the change did not take effect.
	if (session->outLen > 0)
	{
		if (! FlushOutput(theEnv,session)) return false;
	}

	session->bufMode = mode;

	return true;
}

/*************************************************************************/
/* TLSSessionRetained: Does get-retained-bytes for an encrypted socket.  */
/*   The write buffer holds the data that the socket did not accept. The */
/*   answer is the quantity of data in that buffer. The plaintext code   */
/*   answers the same question with its own pending count.               */
/*************************************************************************/
size_t TLSSessionRetained(
		Environment *theEnv,
		struct socketRouter *sptr)
{
	struct tlsSession *session = (struct tlsSession *) sptr->tls;

	return session->outLen;
}

int TLSSessionPending(
		Environment *theEnv,
		struct socketRouter *sptr)
{
	struct tlsSession *session = (struct tlsSession *) sptr->tls;
	int pending;

	pending = (int) (session->inLen - session->inPos);

	if (session->pushback != -1) pending++;

	pending += TLSBackendPending(session->backendSession);

	return pending;
}


/*==========================*/
/* ARGUMENT HELPERS         */
/*==========================*/

static struct tlsContext *HandleToContext(
		Environment *theEnv,
		long long handle)
{
	struct tlsContext *theContext;

	for (theContext = TLSData(theEnv)->ListOfContexts;
			theContext != NULL;
			theContext = theContext->next)
	{
		if (theContext->handle == handle) return theContext;
	}

	return NULL;
}

/* Each of these functions reads one argument and gives the item that the
   argument names. If it cannot do this, it writes the cause to STDERR and
   gives NULL. A function that gives NULL also sets returnValue to FALSE.
   FALSE is what each of these CLIPS functions returns for a bad argument. As
   a result, a caller needs two lines and not five. A caller also cannot
   report success for an argument that the code refused. */

static struct tlsContext *ContextArgument(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *theArg,
		UDFValue *returnValue,
		const char *func)
{
	struct tlsContext *theContext;

	UDFNextArgument(context,INTEGER_BIT,theArg);

	theContext = HandleToContext(theEnv,theArg->integerValue->contents);

	if (theContext == NULL)
	{
		WriteString(theEnv,STDERR,func);
		WriteString(theEnv,STDERR,": no TLS context with that handle\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
	}

	return theContext;
}

static struct tlsSession *SessionArgument(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *theArg,
		UDFValue *returnValue,
		const char *func)
{
	struct socketRouter *sptr;

	sptr = GetSocketRouterFromArgument(theEnv,context,theArg);

	if (sptr == NULL)
	{
		WriteString(theEnv,STDERR,func);
		WriteString(theEnv,STDERR,": argument was not recognized as a socket\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return NULL;
	}

	if (sptr->tls == NULL)
	{
		WriteString(theEnv,STDERR,func);
		WriteString(theEnv,STDERR,": socket '");
		WriteString(theEnv,STDERR,sptr->logicalName != NULL ? sptr->logicalName : "(unnamed socket)");
		WriteString(theEnv,STDERR,"' is not encrypted\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return NULL;
	}

	return (struct tlsSession *) sptr->tls;
}

/*************************************************************************/
/* ValidateForHandshake: Tells if a handshake can operate on this        */
/*   socket. This function makes all the checks that a handshake needs,  */
/*   and the code does not find a problem in the middle of a handshake.  */
/*                                                                       */
/*   One function does the two transports, because the checks and their  */
/*   sequence are the same and only the socket type is different. The    */
/*   parameter wantType is SOCK_STREAM or SOCK_DGRAM. The parameter      */
/*   wrongType is the message for the other socket type.                 */
/*                                                                       */
/*   This function is separate from the code that reads the argument.    */
/*   dtls-accept must examine the socket before it makes these checks. A */
/*   socket that already holds an incomplete association is a socket to  */
/*   continue with, and not a socket to refuse.                          */
/*************************************************************************/
static bool ValidateForHandshake(
		Environment *theEnv,
		struct socketRouter *sptr,
		int wantType,
		const char *wrongType,
		const char *func)
{
	if (sptr->tls != NULL)
	{
		WriteString(theEnv,STDERR,func);
		WriteString(theEnv,STDERR,": socket is already encrypted\n");
		return false;
	}

	if (sptr->type != wantType)
	{
		WriteString(theEnv,STDERR,func);
		WriteString(theEnv,STDERR,wrongType);
		return false;
	}

	if (sptr->logicalName == NULL)
	{
		WriteString(theEnv,STDERR,func);
		// A stream socket gets its name from connect or accept. A datagram
		// socket gets its name from bind or connect. The message is
		// different for each transport.
		WriteString(theEnv,STDERR,(wantType == SOCK_STREAM)
				? ": socket has no logical name; are you sure it is connected or accepted?\n"
				: ": socket has no logical name; are you sure it is bound or connected?\n");
		return false;
	}

	// Data that stdio put in a buffer before the handshake causes an error.
	// Written bytes would go out as plaintext before the ClientHello. Bytes
	// that the code read would be plaintext that this session did not get.
	if (sptr->ioStarted)
	{
		WriteString(theEnv,STDERR,func);
		WriteString(theEnv,STDERR,": socket '");
		WriteString(theEnv,STDERR,sptr->logicalName);
		WriteString(theEnv,STDERR,"' has already been read from or written to; a handshake has to come first\n");
		return false;
	}

	return true;
}

/*************************************************************************/
/* HandshakeSocket: Gives the socket that the next argument names, but   */
/*   only if a handshake for the given transport can operate on that     */
/*   socket.                                                             */
/*************************************************************************/
static struct socketRouter *HandshakeSocket(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *theArg,
		UDFValue *returnValue,
		int wantType,
		const char *wrongType,
		const char *func)
{
	struct socketRouter *sptr;

	sptr = GetSocketRouterFromArgument(theEnv,context,theArg);

	if (sptr == NULL)
	{
		WriteString(theEnv,STDERR,func);
		WriteString(theEnv,STDERR,": argument was not recognized as a socket\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return NULL;
	}

	if (! ValidateForHandshake(theEnv,sptr,wantType,wrongType,func))
	{
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return NULL;
	}

	return sptr;
}

/**************************************************************************/
/* RefreshPayloadCap: Asks the backend again how much data one record     */
/*   holds.                                                               */
/*                                                                        */
/*   The answer changes, and the code asks more than one time. Before a   */
/*   handshake there is no cipher and no overhead to subtract, and each   */
/*   backend gives zero or an estimate. After the handshake the value is  */
/*   correct. The code calls this function when it makes a session, when  */
/*   a handshake completes, and when a caller changes the MTU. The end of */
/*   the handshake is the first moment when the value is correct. It is   */
/*   also the last moment before the code can write data.                 */
/*                                                                        */
/*   A stream session has no records. This function does nothing for such */
/*   a session, and callers do not need to know which type they hold.     */
/**************************************************************************/
static void RefreshPayloadCap(
		struct tlsSession *session)
{
	size_t payload;

	if (! session->datagram) return;

	payload = (size_t) TLSBackendMaxPayload(session->backendSession);

	// The backend gave no usable value. The buffer size is the safe value,
	// because it is the largest record that DTLS permits. The backend
	// refuses a write of that size and does not cut the data.
	if ((payload == 0) || (payload > session->bufSize))
	{ payload = session->bufSize; }

	session->maxPayload = payload;
}

/*************************************************************************/
/* DatagramSession: Gives the session on the socket that the next        */
/*   argument names, but only if that session is a DTLS session. All the */
/*   record functions start here. As a result, the code refuses a        */
/*   request for a datagram from a stream session. It does not give part */
/*   of a stream.                                                        */
/*************************************************************************/
static struct tlsSession *DatagramSession(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *theArg,
		UDFValue *returnValue,
		const char *func)
{
	struct tlsSession *session;

	session = SessionArgument(theEnv,context,theArg,returnValue,func);

	if (session == NULL) return NULL;

	if (! session->datagram)
	{
		WriteString(theEnv,STDERR,func);
		WriteString(theEnv,STDERR,": socket carries a TLS session, not a DTLS one; a stream has no records to send or receive\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return NULL;
	}

	return session;
}

/*=========================*/
/* HANDSHAKING             */
/*=========================*/

/*************************************************************************/
/* Handshake: Runs a handshake to its end. If the handshake succeeds,    */
/*   the function puts the session on the socket. It also keeps an       */
/*   incomplete session on the socket. As a result, tls-handshake can    */
/*   continue that handshake on a non-blocking socket.                   */
/*************************************************************************/
static bool Handshake(
		Environment *theEnv,
		struct socketRouter *sptr,
		struct tlsSession *session,
		const char *func)
{
	int rv;

	rv = TLSBackendHandshake(session->backendSession);

	if (rv == TLS_HANDSHAKE_DONE)
	{
		session->handshakeDone = true;
		RefreshPayloadCap(session);
		sptr->tls = session;
		return true;
	}

	if (rv == TLS_HANDSHAKE_AGAIN)
	{
		// The session is on the socket but the handshake is not complete.
		// The socket now replies through tlsio, and a subsequent
		// tls-handshake can find the session.
		errno = EAGAIN;
		sptr->tls = session;
		return false;
	}

	TLSBackendReportError(theEnv,func);
	FreeSession(theEnv,session);

	return false;
}

/*************************************************************/
/* NewSession: Allocates and initializes a session for the   */
/*   given socket. If it cannot do this, it reports the      */
/*   cause and gives NULL.                                   */
/*************************************************************/
static struct tlsSession *NewSession(
		Environment *theEnv,
		struct tlsContext *theContext,
		struct socketRouter *sptr,
		bool asClient,
		const char *hostname,
		const char *func)
{
	struct tlsSession *session;
	void *backendSession;

	backendSession = TLSBackendNewSession(theEnv,theContext->backendContext,sptr->fd,asClient,hostname);

	if (backendSession == NULL)
	{
		TLSBackendReportError(theEnv,func);
		return NULL;
	}

	session = (struct tlsSession *) gm2(theEnv,sizeof(struct tlsSession));
	session->backendSession = backendSession;
	session->owner = theContext;
	session->owningSocket = sptr;
	theContext->refCount++;
	session->inLen = 0;
	session->inPos = 0;
	session->pushback = -1;
	session->outLen = 0;
	session->writeOffered = 0;
	session->handshakeDone = false;
	session->asClient = asClient;
	session->datagram = theContext->datagram;
	session->listening = false;
	session->verifyRequired = theContext->verifyRequired;
	session->shutdownSent = false;
	session->inBuf = NULL;
	session->outBuf = NULL;
	session->outCap = 0;

	if (session->datagram)
	{
		// The size is the largest record that DTLS allows. It is not the size
		// of the record that this session sends. As a result, a new MTU
		// changes only the fill limit and never the buffers. A read asks for
		// the full buffer, because a buffer that is smaller than the record
		// loses the end of the record and gives no indication of the loss.
		session->bufSize = TLS_DGRAM_BUFFER_MAX;

		// One line goes in one datagram. Full buffering would keep a line
		// until the record is full. A protocol that reads lines would then
		// stop, and the peer could do nothing about it.
		session->bufMode = _IOLBF;
	}
	else
	{
		session->bufSize = TLS_BUFFER_SIZE;
		session->bufMode = _IOFBF;
	}

	// The value is zero for a stream and the value of the backend for a
	// datagram. That value is an estimate until there is a cipher whose
	// overhead the backend can subtract. As a result, the code calls
	// RefreshPayloadCap again at the end of the handshake.
	session->maxPayload = 0;
	RefreshPayloadCap(session);

	session->inBuf = (unsigned char *) gm2(theEnv,session->bufSize);

	// The output buffer starts at the same size, but only this buffer
	// changes size. As a result, the code keeps its size in a field that is
	// separate from bufSize. The field bufSize continues to give the size of
	// the read buffer and the fill level at which the code sends a write.
	session->outCap = session->bufSize;
	session->outBuf = (unsigned char *) gm2(theEnv,session->outCap);

	return session;
}

/**************************************************************************/
/* RequireDatagramContext: Tells if this context makes DTLS sessions.     */
/*                                                                        */
/*   A stream context that goes to dtls-connect or dtls-accept is an      */
/*   error, and this function reports it. Without this check, the error   */
/*   shows only later, in a handshake that cannot succeed. The parameter  */
/*   role is the correct value to make the context with. It is the one    */
/*   part of the message that the two callers do not share.               */
/**************************************************************************/
static bool RequireDatagramContext(
		Environment *theEnv,
		struct tlsContext *theContext,
		UDFValue *returnValue,
		const char *func,
		const char *role)
{
	if (theContext->datagram) return true;

	WriteString(theEnv,STDERR,func);
	WriteString(theEnv,STDERR,": that context was made for TLS; create it with ");
	WriteString(theEnv,STDERR,role);
	WriteString(theEnv,STDERR,"\n");
	returnValue->lexemeValue = FalseSymbol(theEnv);

	return false;
}

/**************************************************************************/
/* StartSession: The last steps of each function that starts a handshake. */
/*   It makes the session on a socket that the code checked. It then runs */
/*   the handshake as far as it can go without a block.                   */
/*                                                                        */
/*   The functions share this code because both a session that the code   */
/*   could not make and a handshake that is not complete give FALSE. A    */
/*   caller that writes this code itself can confuse the two conditions.  */
/*   NewSession already reported the cause of its failure, and the FALSE  */
/*   here must be silent. An incomplete handshake is not a failure at     */
/*   all, and tls-handshake continues it.                                 */
/**************************************************************************/
static void StartSession(
		Environment *theEnv,
		struct tlsContext *theContext,
		struct socketRouter *sptr,
		UDFValue *returnValue,
		bool asClient,
		const char *hostname,
		const char *func)
{
	struct tlsSession *session;

	session = NewSession(theEnv,theContext,sptr,asClient,hostname,func);

	if (session == NULL)
	{
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	returnValue->lexemeValue = CreateBoolean(theEnv,Handshake(theEnv,sptr,session,func));
}

/**************************************************************************/
/* ClientHandshake: The client part of a handshake, for the two           */
/*   transports. It does tls-connect and dtls-connect. The two functions  */
/*   take the same three arguments in the same sequence. Only the         */
/*   necessary socket type is different.                                  */
/*                                                                        */
/*   The server parts do not share code in this manner. tls-accept has    */
/*   the same shape without the hostname, and it is short enough to read  */
/*   on its own. dtls-accept must examine the socket before it knows      */
/*   which checks to make. A socket that already holds an incomplete      */
/*   association is a socket to continue with, and not a socket to        */
/*   refuse. As a result, the two servers share less code than the two    */
/*   clients.                                                             */
/**************************************************************************/
static void ClientHandshake(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue,
		bool datagram,
		const char *func)
{
	UDFValue theArg;
	struct tlsContext *theContext;
	struct socketRouter *sptr;
	const char *hostname;

	theContext = ContextArgument(theEnv,context,&theArg,returnValue,func);
	if (theContext == NULL) return;

	if (datagram)
	{
		if (! RequireDatagramContext(theEnv,theContext,returnValue,func,"DTLS_CLIENT"))
		{ return; }
	}

	sptr = HandshakeSocket(theEnv,context,&theArg,returnValue,
			datagram ? SOCK_DGRAM : SOCK_STREAM,
			datagram ? TLS_WRONG_TYPE_DGRAM : TLS_WRONG_TYPE_STREAM,
			func);
	if (sptr == NULL) return;

	// A datagram socket with no peer has no destination for a ClientHello.
	// Without this check, the failure is a write error in the middle of a
	// handshake and not a message about the socket. A stream socket cannot
	// be in this condition, because connect() made it a connection.
	if (datagram)
	{
		struct sockaddr_storage peer;
		socklen_t peerLen = sizeof(peer);

		if (getpeername(sptr->fd,(struct sockaddr *) &peer,&peerLen) != 0)
		{
			WriteString(theEnv,STDERR,func);
			WriteString(theEnv,STDERR,": socket '");
			WriteString(theEnv,STDERR,sptr->logicalName);
			WriteString(theEnv,STDERR,"' is not connected; connect it to the server first\n");
			returnValue->lexemeValue = FalseSymbol(theEnv);
			return;
		}
	}

	UDFNextArgument(context,LEXEME_BITS,&theArg);
	hostname = theArg.lexemeValue->contents;

	if (hostname[0] == '\0')
	{
		WriteString(theEnv,STDERR,func);
		WriteString(theEnv,STDERR,": a hostname is required; it is what the peer certificate is checked against\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	StartSession(theEnv,theContext,sptr,returnValue,true,hostname,func);
}

/*=========================*/
/* CONTEXT FUNCTIONS       */
/*=========================*/

/*****************************************************************/
/* TLSCreateContextFunction: The H/L access function for         */
/*   tls-create-context. It gives an integer handle or FALSE.    */
/*****************************************************************/
void TLSCreateContextFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;
	struct tlsContext *newContext;
	void *backendContext;
	bool asClient;
	bool datagram;

	UDFNextArgument(context,SYMBOL_BIT,&theArg);

	if (strcmp(theArg.lexemeValue->contents,"TLS_CLIENT") == 0)
	{ asClient = true; datagram = false; }
	else if (strcmp(theArg.lexemeValue->contents,"TLS_SERVER") == 0)
	{ asClient = false; datagram = false; }
	else if (strcmp(theArg.lexemeValue->contents,"DTLS_CLIENT") == 0)
	{ asClient = true; datagram = true; }
	else if (strcmp(theArg.lexemeValue->contents,"DTLS_SERVER") == 0)
	{ asClient = false; datagram = true; }
	else
	{
		WriteString(theEnv,STDERR,"tls-create-context: role '");
		WriteString(theEnv,STDERR,theArg.lexemeValue->contents);
		WriteString(theEnv,STDERR,"' not supported; use TLS_CLIENT, TLS_SERVER, DTLS_CLIENT or DTLS_SERVER\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	// The code asks this question before it makes the context. A backend
	// without DTLS then reports the missing function. Without this check,
	// the message is the one that its context function supplies.
	if (datagram && (! TLSBackendSupportsDTLS(! asClient)))
	{
		WriteString(theEnv,STDERR,"tls-create-context: ");
		WriteString(theEnv,STDERR,TLSBackendName());
		WriteString(theEnv,STDERR,asClient ? " does not implement DTLS\n"
		                                   : " does not implement a DTLS server\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	backendContext = TLSBackendNewContext(theEnv,asClient,datagram);

	if (backendContext == NULL)
	{
		TLSBackendReportError(theEnv,"tls-create-context");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	newContext = get_struct(theEnv,tlsContext);
	newContext->handle = TLSData(theEnv)->NextHandle++;
	newContext->backendContext = backendContext;
	// This reference belongs to the handle that the function gives back.
	// Each session adds its own reference when the code makes the session.
	newContext->refCount = 1;
	// This is the same default that each backend sets. A client verifies
	// the peer unless the user stops it. A server has no peer certificate to
	// check unless the user asks for one.
	newContext->verifyRequired = asClient;
	newContext->datagram = datagram;
	newContext->next = TLSData(theEnv)->ListOfContexts;
	TLSData(theEnv)->ListOfContexts = newContext;

	returnValue->integerValue = CreateInteger(theEnv,newContext->handle);
}

/****************************************************************/
/* TLSFreeContextFunction: The H/L access function for          */
/*   tls-free-context.                                          */
/****************************************************************/
void TLSFreeContextFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;
	struct tlsContext *theContext, *prev;
	long long handle;

	UDFNextArgument(context,INTEGER_BIT,&theArg);
	handle = theArg.integerValue->contents;

	for (theContext = TLSData(theEnv)->ListOfContexts, prev = NULL;
			theContext != NULL;
			prev = theContext, theContext = theContext->next)
	{
		if (theContext->handle != handle) continue;

		if (prev == NULL)
		{ TLSData(theEnv)->ListOfContexts = theContext->next; }
		else
		{ prev->next = theContext->next; }

		// The code removes the context from the list, and no caller can find
		// the handle again. It then removes one reference. Sessions from this
		// context hold their own references, and the last reference frees the
		// context. As a result, this function does not stop the connections
		// that already operate.
		ReleaseContext(theEnv,theContext);

		returnValue->lexemeValue = TrueSymbol(theEnv);
		return;
	}

	WriteString(theEnv,STDERR,"tls-free-context: no TLS context with that handle\n");
	returnValue->lexemeValue = FalseSymbol(theEnv);
}

/*********************************************************************/
/* TLSContextLoadVerifyLocationsFunction: The H/L access function    */
/*   for tls-context-load-verify-locations.                          */
/*********************************************************************/
void TLSContextLoadVerifyLocationsFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;
	struct tlsContext *theContext;
	const char *caFile = NULL;
	const char *caPath = NULL;

	theContext = ContextArgument(theEnv,context,&theArg,returnValue,"tls-context-load-verify-locations");
	if (theContext == NULL) return;

	UDFNextArgument(context,LEXEME_BITS,&theArg);
	caFile = theArg.lexemeValue->contents;

	if (UDFHasNextArgument(context))
	{
		UDFNextArgument(context,LEXEME_BITS,&theArg);
		caPath = theArg.lexemeValue->contents;
	}

	if (! TLSBackendLoadVerifyLocations(theEnv,theContext->backendContext,caFile,caPath))
	{
		TLSBackendReportError(theEnv,"tls-context-load-verify-locations");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	returnValue->lexemeValue = TrueSymbol(theEnv);
}

/**********************************************************************/
/* TLSContextSetDefaultVerifyPathsFunction: The H/L access function   */
/*   for tls-context-set-default-verify-paths.                        */
/**********************************************************************/
void TLSContextSetDefaultVerifyPathsFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;
	struct tlsContext *theContext;

	theContext = ContextArgument(theEnv,context,&theArg,returnValue,"tls-context-set-default-verify-paths");
	if (theContext == NULL) return;

	if (! TLSBackendLoadSystemTrust(theContext->backendContext))
	{
		TLSBackendReportError(theEnv,"tls-context-set-default-verify-paths");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	returnValue->lexemeValue = TrueSymbol(theEnv);
}

/*******************************************************************/
/* ContextFileFunction: The code that                              */
/*   tls-context-use-certificate-file and                          */
/*   tls-context-use-private-key-file share. Each takes a context  */
/*   and a path. Each makes one backend call, and that call either */
/*   accepts the file or reports the cause of the refusal. The two */
/*   functions have no other difference.                           */
/*******************************************************************/
static void ContextFileFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue,
		bool (*apply)(Environment *,void *,const char *),
		const char *func)
{
	UDFValue theArg;
	struct tlsContext *theContext;

	theContext = ContextArgument(theEnv,context,&theArg,returnValue,func);
	if (theContext == NULL) return;

	UDFNextArgument(context,LEXEME_BITS,&theArg);

	if (! (*apply)(theEnv,theContext->backendContext,theArg.lexemeValue->contents))
	{
		TLSBackendReportError(theEnv,func);
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	returnValue->lexemeValue = TrueSymbol(theEnv);
}

/*******************************************************************/
/* TLSContextUseCertificateFileFunction: The H/L access function   */
/*   for tls-context-use-certificate-file.                         */
/*******************************************************************/
void TLSContextUseCertificateFileFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	ContextFileFunction(theEnv,context,returnValue,
			TLSBackendUseCertificateFile,"tls-context-use-certificate-file");
}

/******************************************************************/
/* TLSContextUsePrivateKeyFileFunction: The H/L access function   */
/*   for tls-context-use-private-key-file.                        */
/******************************************************************/
void TLSContextUsePrivateKeyFileFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	ContextFileFunction(theEnv,context,returnValue,
			TLSBackendUsePrivateKeyFile,"tls-context-use-private-key-file");
}

/*********************************************************/
/* TLSContextSetVerifyFunction: The H/L access function  */
/*   for tls-context-set-verify.                         */
/*********************************************************/
void TLSContextSetVerifyFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;
	struct tlsContext *theContext;
	bool required;

	theContext = ContextArgument(theEnv,context,&theArg,returnValue,"tls-context-set-verify");
	if (theContext == NULL) return;

	UDFNextArgument(context,SYMBOL_BIT,&theArg);

	if (strcmp(theArg.lexemeValue->contents,"SSL_VERIFY_PEER") == 0)
	{ required = true; }
	else if (strcmp(theArg.lexemeValue->contents,"SSL_VERIFY_NONE") == 0)
	{ required = false; }
	else
	{
		WriteString(theEnv,STDERR,"tls-context-set-verify: mode '");
		WriteString(theEnv,STDERR,theArg.lexemeValue->contents);
		WriteString(theEnv,STDERR,"' not supported; use SSL_VERIFY_PEER or SSL_VERIFY_NONE\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	// The code uses this result and does not discard it. A backend that
	// cannot obey the request must be able to report that condition. A TRUE
	// in all conditions would let a program think that it asked for a peer
	// certificate on a library that never asked for one.
	if (! TLSBackendSetVerify(theContext->backendContext,required))
	{
		TLSBackendReportError(theEnv,"tls-context-set-verify");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	// The code sets this field only after the backend accepts the value. If
	// the backend refuses the value, the two records continue to agree.
	// Without this sequence, this record would show a setting that the
	// library never got.
	theContext->verifyRequired = required;

	returnValue->lexemeValue = TrueSymbol(theEnv);
}

/****************************************************************/
/* TLSContextSetMinProtoVersionFunction: The H/L access         */
/*   function for tls-context-set-min-proto-version.            */
/****************************************************************/
void TLSContextSetMinProtoVersionFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;
	struct tlsContext *theContext;
	int version;

	theContext = ContextArgument(theEnv,context,&theArg,returnValue,"tls-context-set-min-proto-version");
	if (theContext == NULL) return;

	UDFNextArgument(context,SYMBOL_BIT,&theArg);

	// The transport sets which names are correct. A datagram context has no
	// TLS 1.2, and a stream context has no DTLS 1.2. If the code accepted
	// all the names for the two transports, a context could get a value that
	// it cannot obey. The backend would then have to select the correct
	// value itself.
	if (theContext->datagram)
	{
		if (strcmp(theArg.lexemeValue->contents,"DTLS1_2_VERSION") == 0)
		{ version = TLS_VERSION_DTLS_1_2; }
		else
		{
			WriteString(theEnv,STDERR,"tls-context-set-min-proto-version: version '");
			WriteString(theEnv,STDERR,theArg.lexemeValue->contents);
			WriteString(theEnv,STDERR,"' not supported on a DTLS context; use DTLS1_2_VERSION\n");
			returnValue->lexemeValue = FalseSymbol(theEnv);
			return;
		}
	}
	else if (strcmp(theArg.lexemeValue->contents,"TLS1_2_VERSION") == 0)
	{ version = TLS_VERSION_1_2; }
	else if (strcmp(theArg.lexemeValue->contents,"TLS1_3_VERSION") == 0)
	{ version = TLS_VERSION_1_3; }
	else
	{
		WriteString(theEnv,STDERR,"tls-context-set-min-proto-version: version '");
		WriteString(theEnv,STDERR,theArg.lexemeValue->contents);
		WriteString(theEnv,STDERR,"' not supported; use TLS1_2_VERSION or TLS1_3_VERSION\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	if (! TLSBackendSetMinVersion(theContext->backendContext,version))
	{
		TLSBackendReportError(theEnv,"tls-context-set-min-proto-version");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	returnValue->lexemeValue = TrueSymbol(theEnv);
}

/*=========================*/
/* CONNECTION FUNCTIONS    */
/*=========================*/

/*****************************************************************/
/* TLSConnectFunction: The H/L access function for tls-connect.  */
/*   It runs the client part of a handshake on a connected       */
/*   socket. It compares the peer certificate with the hostname. */
/*****************************************************************/
void TLSConnectFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	ClientHandshake(theEnv,context,returnValue,false,"tls-connect");
}

/****************************************************************/
/* TLSAcceptFunction: The H/L access function for tls-accept.   */
/*   It runs the server part of a handshake on an accepted      */
/*   socket.                                                    */
/****************************************************************/
void TLSAcceptFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;
	struct tlsContext *theContext;
	struct socketRouter *sptr;

	theContext = ContextArgument(theEnv,context,&theArg,returnValue,"tls-accept");
	if (theContext == NULL) return;

	sptr = HandshakeSocket(theEnv,context,&theArg,returnValue,
			SOCK_STREAM,TLS_WRONG_TYPE_STREAM,"tls-accept");
	if (sptr == NULL) return;

	StartSession(theEnv,theContext,sptr,returnValue,false,NULL,"tls-accept");
}

/**************************************************************************/
/* TLSHandshakeFunction: The H/L access function for tls-handshake. It    */
/*   continues a handshake that could not complete on a non-blocking      */
/*   socket.                                                              */
/**************************************************************************/
void TLSHandshakeFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;
	struct socketRouter *sptr;
	struct tlsSession *session;
	int rv;

	sptr = GetSocketRouterFromArgument(theEnv,context,&theArg);

	if ((sptr == NULL) || (sptr->tls == NULL))
	{
		WriteString(theEnv,STDERR,"tls-handshake: socket is not encrypted; start with tls-connect or tls-accept\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	session = (struct tlsSession *) sptr->tls;

	if (session->handshakeDone)
	{
		returnValue->lexemeValue = TrueSymbol(theEnv);
		return;
	}

	rv = TLSBackendHandshake(session->backendSession);

	if (rv == TLS_HANDSHAKE_DONE)
	{
		session->handshakeDone = true;
		RefreshPayloadCap(session);
		returnValue->lexemeValue = TrueSymbol(theEnv);
		return;
	}

	if (rv == TLS_HANDSHAKE_AGAIN)
	{ errno = EAGAIN; }
	else
	{ TLSBackendReportError(theEnv,"tls-handshake"); }

	returnValue->lexemeValue = FalseSymbol(theEnv);
}

/*****************************************************/
/* TLSShutdownFunction: The H/L access function for  */
/*   tls-shutdown.                                   */
/*****************************************************/
void TLSShutdownFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;
	struct tlsSession *session;

	session = SessionArgument(theEnv,context,&theArg,returnValue,"tls-shutdown");
	if (session == NULL) return;

	if (session->outLen > 0)
	{ FlushOutput(theEnv,session); }

	if (session->shutdownSent)
	{
		returnValue->lexemeValue = TrueSymbol(theEnv);
		return;
	}

	session->shutdownSent = true;

	returnValue->lexemeValue = CreateBoolean(theEnv,TLSBackendShutdown(session->backendSession));
}

/***************************************************/
/* TLSPendingFunction: The H/L access function     */
/*   for tls-pending.                              */
/***************************************************/
void TLSPendingFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;
	struct socketRouter *sptr;

	sptr = GetSocketRouterFromArgument(theEnv,context,&theArg);

	if ((sptr == NULL) || (sptr->tls == NULL))
	{
		WriteString(theEnv,STDERR,"tls-pending: socket is not encrypted\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	returnValue->integerValue = CreateInteger(theEnv,TLSSessionPending(theEnv,sptr));
}

/**************************************************/
/* SessionTextFunction: The code that tls-cipher  */
/*   and tls-version share. Each takes a session  */
/*   and makes one backend call. That call gives  */
/*   the name of what the two ends agreed on.     */
/**************************************************/
static void SessionTextFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue,
		const char *(*ask)(void *),
		const char *func)
{
	UDFValue theArg;
	struct tlsSession *session;

	session = SessionArgument(theEnv,context,&theArg,returnValue,func);
	if (session == NULL) return;

	returnValue->lexemeValue = CreateSymbol(theEnv,(*ask)(session->backendSession));
}

/**************************************************/
/* TLSCipherFunction: The H/L access function     */
/*   for tls-cipher.                              */
/**************************************************/
void TLSCipherFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	SessionTextFunction(theEnv,context,returnValue,TLSBackendCipher,"tls-cipher");
}

/***************************************************/
/* TLSVersionFunction: The H/L access function     */
/*   for tls-version.                              */
/***************************************************/
void TLSVersionFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	SessionTextFunction(theEnv,context,returnValue,TLSBackendProtocol,"tls-version");
}

/**************************************************************************/
/* TLSVerifyResultFunction: The H/L access function for                   */
/* tls-verify-result.                                                     */
/*                                                                        */
/*   It gives TRUE if the code checked the peer and the check passed. It  */
/*   gives the cause as a string if the check failed. It gives FALSE if   */
/*   the code made no check.                                              */
/*                                                                        */
/*   The last condition needs care. The usual use is (if                  */
/*   (tls-verify-result ?s) then ...). In CLIPS each value except FALSE   */
/*   is true. As a result, each other value gives permission to trust the */
/*   peer. A connection with no verification must give FALSE, or that     */
/*   test is incorrect. The libraries do not give that answer. OpenSSL    */
/*   reports X509_V_OK for a server that never asked for a client         */
/*   certificate, because it made no check and no check failed.           */
/**************************************************************************/
void TLSVerifyResultFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;
	struct tlsSession *session;
	char text[TLS_TEXT_SIZE];

	session = SessionArgument(theEnv,context,&theArg,returnValue,"tls-verify-result");
	if (session == NULL) return;

	// The code asked nothing of this peer, and it knows nothing about the
	// peer. A question to the backend at this point gives the incorrect
	// answer. It does not give the correct one.
	if (! session->verifyRequired)
	{
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	if (TLSBackendVerifyOK(session->backendSession))
	{
		returnValue->lexemeValue = TrueSymbol(theEnv);
		return;
	}

	if (! TLSBackendVerifyDescription(session->backendSession,text,sizeof(text)))
	{
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	returnValue->lexemeValue = CreateString(theEnv,text);
}

/*****************************************************/
/* TLSPeerSubjectFunction: The H/L access function   */
/*   for tls-peer-subject.                           */
/*****************************************************/
void TLSPeerSubjectFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;
	struct tlsSession *session;
	char text[TLS_TEXT_SIZE];

	session = SessionArgument(theEnv,context,&theArg,returnValue,"tls-peer-subject");
	if (session == NULL) return;

	if (! TLSBackendPeerSubject(session->backendSession,text,sizeof(text)))
	{
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	returnValue->lexemeValue = CreateString(theEnv,text);
}

/*=========================*/
/* DATAGRAM FUNCTIONS      */
/*=========================*/

/*****************************************************************/
/* DTLSConnectFunction: The H/L access function for              */
/*   dtls-connect. It runs the client part of a handshake on a   */
/*   connected datagram socket. It compares the peer certificate */
/*   with the hostname.                                          */
/*****************************************************************/
void DTLSConnectFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	ClientHandshake(theEnv,context,returnValue,true,"dtls-connect");
}

/**************************************************************************/
/* AdvanceAccept: Moves a server session forward from its current stage.  */
/*                                                                        */
/*   There are two stages, and H/L cannot see which stage a session is    */
/*   in. There is no association until a ClientHello arrives with a       */
/*   cookie that agrees with the address that sent it. Before that point  */
/*   there is only an exchange in progress. After that point there is a   */
/*   usual handshake to complete. One function does the two stages, and a */
/*   caller can write one loop around it.                                 */
/**************************************************************************/
static bool AdvanceAccept(
		Environment *theEnv,
		struct socketRouter *sptr,
		struct tlsSession *session)
{
	struct sockaddr_storage peer;
	socklen_t peerLen = sizeof(peer);
	int rv;

	if (session->listening)
	{
		memset(&peer,0,sizeof(peer));

		rv = TLSBackendDTLSListen(session->backendSession,&peer,&peerLen);

		if (rv != TLS_HANDSHAKE_DONE)
		{
			// LISTEN_AGAIN and AGAIN give the caller the same
			// information: the exchange is not complete, and the caller
			// must wait on the socket. They are two constants because
			// some backends can tell them apart. Those backends must be
			// able to report the difference.
			if (rv == TLS_HANDSHAKE_FAIL)
			{ TLSBackendReportError(theEnv,"dtls-accept"); }
			else
			{ errno = EAGAIN; }

			// If the backend permits it, the code discards an incomplete
			// exchange and does not keep it. The next call then starts a
			// new exchange. This agrees with the protocol, because an
			// incomplete exchange holds nothing of value. On one backend
			// this is the only method that operates. There the listen
			// uses a call that the subsequent handshake cannot do two
			// times.
			//
			// If the backend does not permit it, the session stays. On
			// such a backend the cookie belongs to the session, and only
			// that session accepts the reply to the cookie.
			if (TLSBackendDTLSRestartable())
			{
				FreeSession(theEnv,session);
				sptr->tls = NULL;
			}

			return false;
		}

		// The address showed that it can receive the data that the server
		// sends to it. The code connects the socket to that address, and the
		// kernel then discards the datagrams from all other addresses.
		if (connect(sptr->fd,(struct sockaddr *) &peer,peerLen) < 0)
		{
			WriteString(theEnv,STDERR,"dtls-accept: could not connect the socket to the peer it just answered\n");
			return false;
		}

		// The library keeps its own record of the transport. The code must
		// tell it that the socket now sends to one address only.
		if (! TLSBackendSetSocket(session->backendSession,sptr->fd))
		{
			TLSBackendReportError(theEnv,"dtls-accept");
			return false;
		}

		session->listening = false;
	}

	rv = TLSBackendHandshake(session->backendSession);

	if (rv == TLS_HANDSHAKE_DONE)
	{
		session->handshakeDone = true;
		RefreshPayloadCap(session);
		return true;
	}

	// The handshake can also give LISTEN_AGAIN, and not only the listen.
	// This is true for a backend that runs the cookie exchange inside the
	// handshake. mbedTLS reports its HelloVerifyRequest in this manner. For
	// a caller it has the same meaning as AGAIN. A report of a failure would
	// write a message to stderr for an exchange that is correct.
	if ((rv == TLS_HANDSHAKE_AGAIN) || (rv == TLS_HANDSHAKE_LISTEN_AGAIN))
	{ errno = EAGAIN; }
	else
	{ TLSBackendReportError(theEnv,"dtls-accept"); }

	return false;
}

/**************************************************************************/
/* DTLSAcceptFunction: The H/L access function for dtls-accept. It        */
/*   replies to each ClientHello on a bound datagram socket until one     */
/*   arrives with a cookie that agrees with the address that sent it. It  */
/*   then runs the server part of the handshake with that peer.           */
/*                                                                        */
/*   A second call on the same socket continues the work. This is not an  */
/*   error. It is the usual sequence for a non-blocking server. The       */
/*   caller uses this function and not tls-handshake, because there is no */
/*   session for tls-handshake to find until the exchange completes.      */
/*   Also, nothing tells a caller which of the two stages the socket is   */
/*   in.                                                                  */
/**************************************************************************/
void DTLSAcceptFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;
	struct tlsContext *theContext;
	struct socketRouter *sptr;
	struct tlsSession *session;

	theContext = ContextArgument(theEnv,context,&theArg,returnValue,"dtls-accept");
	if (theContext == NULL) return;

	if (! RequireDatagramContext(theEnv,theContext,returnValue,"dtls-accept","DTLS_SERVER"))
	{ return; }

	sptr = GetSocketRouterFromArgument(theEnv,context,&theArg);

	if (sptr == NULL)
	{
		WriteString(theEnv,STDERR,"dtls-accept: argument was not recognized as a socket\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	if (sptr->tls != NULL)
	{
		session = (struct tlsSession *) sptr->tls;

		if (! session->datagram)
		{
			WriteString(theEnv,STDERR,"dtls-accept: socket carries a TLS session, not a DTLS one\n");
			returnValue->lexemeValue = FalseSymbol(theEnv);
			return;
		}

		if (session->handshakeDone)
		{
			returnValue->lexemeValue = TrueSymbol(theEnv);
			return;
		}

		returnValue->lexemeValue = CreateBoolean(theEnv,AdvanceAccept(theEnv,sptr,session));
		return;
	}

	if (! ValidateForHandshake(theEnv,sptr,SOCK_DGRAM,TLS_WRONG_TYPE_DGRAM,"dtls-accept"))
	{
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	session = NewSession(theEnv,theContext,sptr,false,NULL,"dtls-accept");

	if (session == NULL)
	{
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	// The code puts the session on the socket before the exchange
	// completes. A subsequent call can then continue the exchange. A caller
	// that stops an incomplete association closes the socket, and
	// close-connection frees the session with it.
	session->listening = true;
	sptr->tls = session;

	returnValue->lexemeValue = CreateBoolean(theEnv,AdvanceAccept(theEnv,sptr,session));
}

/***************************************************************************/
/* DTLSSendFunction: The H/L access function for dtls-send. It sends one   */
/*   complete record.                                                      */
/***************************************************************************/
void DTLSSendFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;
	struct tlsSession *session;
	const char *data;
	size_t len;
	long n;

	session = DatagramSession(theEnv,context,&theArg,returnValue,"dtls-send");
	if (session == NULL) return;

	UDFNextArgument(context,LEXEME_BITS,&theArg);
	data = theArg.lexemeValue->contents;
	len = strlen(data);

	// The data that printout put in the buffer goes out first. If it did
	// not, this record would go in front of text that the program wrote
	// before it.
	if (session->outLen > 0)
	{
		if (! FlushOutput(theEnv,session))
		{
			returnValue->lexemeValue = FalseSymbol(theEnv);
			return;
		}
	}

	if (len > session->maxPayload)
	{
		WriteString(theEnv,STDERR,"dtls-send: message is larger than one record holds at the current MTU\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	n = TLSBackendWrite(session->backendSession,data,len);

	if (n <= 0)
	{
		if (n == TLS_RESULT_AGAIN) errno = EAGAIN;
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	returnValue->integerValue = CreateInteger(theEnv,(long long) n);
}

/**************************************************************************/
/* DTLSRecvFunction: The H/L access function for dtls-recv. It reads one  */
/*   record and gives two fields: the byte count and the data. It has the */
/*   same shape as rcvfrom but without the address, because the           */
/*   association has only one peer.                                       */
/**************************************************************************/
void DTLSRecvFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;
	struct tlsSession *session;
	MultifieldBuilder *mb;
	size_t maxlen;
	long n;

	session = DatagramSession(theEnv,context,&theArg,returnValue,"dtls-recv");
	if (session == NULL) return;

	// Characters that the code took from a record but did not give out
	// belong to the code that reads the stream. If this function gave the
	// next record, those characters would be lost, and nothing would show
	// the loss.
	if ((session->inPos < session->inLen) || (session->pushback != -1))
	{
		WriteString(theEnv,STDERR,"dtls-recv: characters from an earlier record are still unread; finish reading them or use empty-connection first\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	// The maximum is one byte less than the buffer, because the function
	// gives the data to CLIPS as a string. The buffer must hold the
	// terminator.
	maxlen = session->bufSize - 1;

	if (UDFHasNextArgument(context))
	{
		UDFNextArgument(context,INTEGER_BIT,&theArg);

		if ((theArg.integerValue->contents > 0) &&
		    ((size_t) theArg.integerValue->contents < maxlen))
		{ maxlen = (size_t) theArg.integerValue->contents; }
	}

	// This is the read buffer of the session, and the check above showed
	// that the buffer is empty. A request for less data than the record
	// holds does not lose the remainder. Each backend here keeps the
	// remainder, and the next call gives it. This is the opposite of a
	// plaintext datagram read. There the kernel discards the data that does
	// not fit, as tests/integration/udp-router-io.clp shows.
	n = TLSBackendRead(session->backendSession,session->inBuf,maxlen);

	if (n <= 0)
	{
		if (n == TLS_RESULT_AGAIN) errno = EAGAIN;
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	session->inBuf[n] = '\0';

	mb = CreateMultifieldBuilder(theEnv,2L);
	MBAppendInteger(mb,(long long) n);
	MBAppendString(mb,(char *) session->inBuf);
	returnValue->multifieldValue = MBCreate(mb);
	MBDispose(mb);
}

/**************************************************************************/
/* DTLSTimeoutFunction: The H/L access function for dtls-timeout. It      */
/*   gives the time until the code must send the last flight again. It    */
/*   gives FALSE if there is no flight to send again.                     */
/**************************************************************************/
void DTLSTimeoutFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;
	struct tlsSession *session;
	long ms;

	session = DatagramSession(theEnv,context,&theArg,returnValue,"dtls-timeout");
	if (session == NULL) return;

	ms = TLSBackendDTLSTimeout(session->backendSession);

	if (ms < 0)
	{
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	returnValue->integerValue = CreateInteger(theEnv,(long long) ms);
}

/**************************************************************************/
/* DTLSHandleTimeoutFunction: The H/L access function for                 */
/*   dtls-handle-timeout. It sends the last flight again.                 */
/*                                                                        */
/*   A handshake that never does this operates correctly on loopback,     */
/*   where no packet is lost. It stops on the first network that discards */
/*   a packet. The caller controls this function, because the caller has  */
/*   the poll loop.                                                       */
/**************************************************************************/
void DTLSHandleTimeoutFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;
	struct tlsSession *session;
	int rv;

	session = DatagramSession(theEnv,context,&theArg,returnValue,"dtls-handle-timeout");
	if (session == NULL) return;

	rv = TLSBackendDTLSHandleTimeout(session->backendSession);

	if (rv == TLS_HANDSHAKE_FAIL)
	{
		TLSBackendReportError(theEnv,"dtls-handle-timeout");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	returnValue->lexemeValue = TrueSymbol(theEnv);
}

/**************************************************************************/
/* DTLSSetMTUFunction: The H/L access function for dtls-set-mtu. It sets  */
/*   the link MTU. The library uses the MTU to calculate how much data    */
/*   one record holds.                                                    */
/**************************************************************************/
void DTLSSetMTUFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;
	struct tlsSession *session;
	long long mtu;

	session = DatagramSession(theEnv,context,&theArg,returnValue,"dtls-set-mtu");
	if (session == NULL) return;

	UDFNextArgument(context,INTEGER_BIT,&theArg);
	mtu = theArg.integerValue->contents;

	if ((mtu <= 0) || (mtu > TLS_DGRAM_BUFFER_MAX))
	{
		WriteString(theEnv,STDERR,"dtls-set-mtu: that is not a usable link MTU\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	// The data in the buffer agrees with the old record size. The code sends
	// it under the setting that it collected the data for.
	if (session->outLen > 0)
	{
		if (! FlushOutput(theEnv,session))
		{
			returnValue->lexemeValue = FalseSymbol(theEnv);
			return;
		}
	}

	if (! TLSBackendSetMTU(session->backendSession,(int) mtu))
	{
		TLSBackendReportError(theEnv,"dtls-set-mtu");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	// The fill limit changes with the MTU. The buffers do not change. Their
	// size is the largest record that DTLS allows, and no permitted MTU is
	// larger than that.
	RefreshPayloadCap(session);

	returnValue->lexemeValue = TrueSymbol(theEnv);
}

/**************************************************************************/
/* TLSSupportsDTLSFunction: The H/L access function for                   */
/*   tls-supports-dtls. It tells if the library does DTLS. An optional    */
/*   argument asks about one direction only.                              */
/*                                                                        */
/*   This function lets a test ask the question and not declare the       */
/*   answer. The two answers can be different. A backend without a        */
/*   stateless cookie exchange can be a client, but it cannot be a safe   */
/*   server.                                                              */
/**************************************************************************/
void TLSSupportsDTLSFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;

	if (! UDFHasNextArgument(context))
	{
		returnValue->lexemeValue = CreateBoolean(theEnv,
				TLSBackendSupportsDTLS(false) || TLSBackendSupportsDTLS(true));
		return;
	}

	UDFNextArgument(context,SYMBOL_BIT,&theArg);

	if (strcmp(theArg.lexemeValue->contents,"DTLS_CLIENT") == 0)
	{
		returnValue->lexemeValue = CreateBoolean(theEnv,TLSBackendSupportsDTLS(false));
		return;
	}

	if (strcmp(theArg.lexemeValue->contents,"DTLS_SERVER") == 0)
	{
		returnValue->lexemeValue = CreateBoolean(theEnv,TLSBackendSupportsDTLS(true));
		return;
	}

	WriteString(theEnv,STDERR,"tls-supports-dtls: role '");
	WriteString(theEnv,STDERR,theArg.lexemeValue->contents);
	WriteString(theEnv,STDERR,"' not supported; use DTLS_CLIENT or DTLS_SERVER\n");
	returnValue->lexemeValue = FalseSymbol(theEnv);
}

/******************************************************************/
/* TLSBackendFunction: The H/L access function for tls-backend.   */
/*   It gives the name of the library that the build used, as a   */
/*   symbol.                                                      */
/******************************************************************/
void TLSBackendFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	returnValue->lexemeValue = CreateSymbol(theEnv,TLSBackendName());
}

/*********************************************************************/
/* TLSBackendVersionFunction: The H/L access function for            */
/*   tls-backend-version. It gives the version of that library as a  */
/*   string, because each library writes the version differently:    */
/*   "OpenSSL 3.0.13 30 Jan 2024", "LibreSSL 4.3.1", "2.28.8".       */
/*                                                                   */
/*   This is the version that the headers gave when the build        */
/*   compiled the binary. The build does not link headers and a      */
/*   library that do not agree. As a result, this is also the        */
/*   version of the library in use.                                  */
/*********************************************************************/
void TLSBackendVersionFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	returnValue->lexemeValue = CreateString(theEnv,TLSBackendVersion());
}

/*=====================================*/
/* HELPERS THAT THE BACKENDS SHARE     */
/*                                     */
/* socktlsbe.h declares these. None of */
/* them uses a TLS library. More than  */
/* one backend needs them, and no one  */
/* backend must own them.              */
/*=====================================*/

/*****************************************************************/
/* TLSLooksLikeAddress: Tells if the given name is an address    */
/*   and not a host name.                                        */
/*                                                               */
/*   An address has only digits and dots, or it has a colon. The */
/*   test is not exact, and that is on purpose. It only selects  */
/*   one of two comparisons, and the address comparison refuses  */
/*   a value that it cannot read.                                */
/*****************************************************************/
bool TLSLooksLikeAddress(
		const char *name)
{
	const char *p;

	// No IPv6 address is a host name. An IPv6 address has only hexadecimal
	// digits, colons and dots.
	if (strchr(name,':') != NULL) return true;

	for (p = name; *p != '\0'; p++)
	{
		if ((*p == '.') || ((*p >= '0') && (*p <= '9'))) continue;
		return false;
	}

	return true;
}

/****************************************************************/
/* TLSForgetPath: Frees the path in a slot and puts NULL there. */
/*   A slot that is already NULL is not an error.               */
/*                                                              */
/*   The length comes from strlen, and this is exact. The only  */
/*   writer of a slot is TLSRememberPath below, which allocates */
/*   strlen(path) + 1 bytes and copies a string of that length  */
/*   into them.                                                 */
/****************************************************************/
void TLSForgetPath(
		Environment *theEnv,
		char **slot)
{
	if (*slot == NULL) return;

	rm(theEnv,*slot,strlen(*slot) + 1);
	*slot = NULL;
}

/****************************************************************/
/* TLSRememberPath: Keeps a copy of a path for the next time    */
/*   that a backend must make its credentials again. An empty   */
/*   path is not a failure. A caller that gives a file and no   */
/*   directory gives a correct value for the directory.         */
/****************************************************************/
bool TLSRememberPath(
		Environment *theEnv,
		char **slot,
		const char *path)
{
	size_t length;

	TLSForgetPath(theEnv,slot);

	if ((path == NULL) || (path[0] == '\0')) return true;

	length = strlen(path);

	*slot = (char *) gm2(theEnv,length + 1);
	if (*slot == NULL) return false;

	memcpy(*slot,path,length + 1);

	return true;
}

/****************************************************************/
/* TLSReportBackendError: Writes one failure. The               */
/*   TLSBackendReportError of each backend ends here. Each of   */
/*   them first changes its data into text, or into NULL if it  */
/*   has no data.                                               */
/*                                                              */
/*   A failure with no detail is still a failure. A message     */
/*   with no text would leave the FALSE of the caller without   */
/*   a cause. In that condition this function writes the name   */
/*   of the backend, because the name of the library that gave  */
/*   no detail is the one useful item that is available.        */
/****************************************************************/
void TLSReportBackendError(
		Environment *theEnv,
		const char *what,
		const char *detail)
{
	WriteString(theEnv,STDERR,what);
	WriteString(theEnv,STDERR,": ");

	if ((detail != NULL) && (detail[0] != '\0'))
	{ WriteString(theEnv,STDERR,detail); }
	else
	{
		WriteString(theEnv,STDERR,"failed with no further detail from ");
		WriteString(theEnv,STDERR,TLSBackendName());
	}

	WriteString(theEnv,STDERR,"\n");
}

#endif /* USE_TLS */
