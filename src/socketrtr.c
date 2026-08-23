/*******************************************************/
/*      "C" Language Integrated Production System      */
/*                                                     */
/*            CLIPS Version ?.??  05/07/24             */
/*                                                     */
/*               SOCKET I/O ROUTER MODULE              */
/*******************************************************/

/***********************************************************************/
/* Purpose: Provides TLS Socket I/O Router routines                    */
/*   as input and output sources.                                      */
/*                                                                     */
/* Principal Programmer(s):                                            */
/*      Ryan P. Johnston                                               */   
/*                                                                     */
/* Revision History:                                                   */
/*                                                                     */
/*      ?.??: Added this file.                                         */
/*                                                                     */
/**********************************************************************/

#define _POSIX_C_SOURCE 200112L
#define NI_MAXHOST      1025

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "setup.h"

#include "constant.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#include "filertr.h"
#include "memalloc.h"
#include "prntutil.h"
#include "router.h"
#include "symbol.h"
#include "sysdep.h"
#include "utility.h"

#include "socketrtr.h"
#include "socktls.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static void                    WriteSocket(Environment *, const char *, const char *, void *);
static int                     ReadSocket(Environment *, const char *, void *);
static int                     UnreadSocket(Environment *, const char *, int, void *);
static void                    ExitSocket(Environment *, int, void *);
static void                    DeallocateSocketRouterData(Environment *);
static bool                    DecryptedBytesWaiting(Environment *, int, int);
static void                    WriteSocketBytes(Environment *, struct socketRouter *, const char *, size_t);
static bool                    PushPending(Environment *, struct socketRouter *);
static void                    RetainTail(Environment *, struct socketRouter *, const char *, size_t);
static void                    DisposeStream(Environment *, struct socketRouter *);

/* The first size of the buffer for retained data. The code multiplies the size
   by two from this value, and the buffer then holds the data that a write
   leaves. There is no maximum unless set-retained-limit gives the socket one. */
#define SOCKET_PENDING_MIN 4096

/* This size holds each address that this library writes as text, and it also
   holds the terminator. The size comes from the unix path, because that path
   is much the longest. An IPv6 address needs INET6_ADDRSTRLEN, which is 46,
   and sun_path is 108. */
#define SOCKET_ADDRESS_TEXT (sizeof(((struct sockaddr_un *) 0)->sun_path) + 1)

/********************************************************************/
/* InitializeSocketRouter: Initializes socket router structure. */
/********************************************************************/
void InitializeSocketRouter(
		Environment *theEnv)
{
	AllocateEnvironmentData(
			theEnv,
			SOCKET_ROUTER_DATA,
			sizeof(struct socketRouterData),
			DeallocateSocketRouterData);

	AddRouter(theEnv,"socketio",0,FindSocket,
			WriteSocket,ReadSocket,UnreadSocket,ExitSocket,NULL);

	TLSInitialize(theEnv);
}

/*******************************************/
/* DeallocateSocketRouterData: Deallocates */
/*    environment data for socket routers. */
/*******************************************/
static void DeallocateSocketRouterData(
		Environment *theEnv)
{
	CloseAllSockets(theEnv);
}

/*************************************************************************/
/* FindSocket: high level function for Router Query Callback.            */
/*************************************************************************/
bool FindSocket(
		Environment *theEnv,
		const char *logicalName,
		void *context)
{
	struct socketRouter *sptr;

	sptr = LogicalNameToSocketRouter(theEnv, logicalName);

	// A socket with a TLS session answers through the tlsio router. The two
	// query callbacks must disagree about each name. Without that, the first
	// router takes the data of the other router.
	return (sptr != NULL) && (sptr->tls == NULL);
}

/*****************************************************************/
/* WriteSocket: Write callback for socket router.                */
/*****************************************************************/
static void WriteSocket(
		Environment *theEnv,
		const char *logicalName,
		const char *str,
		void *context)
{
	struct socketRouter *sptr;

	sptr = LogicalNameToSocketRouter(theEnv,logicalName);

	if (sptr == NULL) return;

	sptr->ioStarted = true;

	WriteSocketBytes(theEnv,sptr,str,strlen(str));
}

/*****************************************************************/
/* WriteSocketBytes: The output part of the socketio router.     */
/*   It sends the data that the socket accepts and keeps the     */
/*   data that the socket refuses.                               */
/*****************************************************************/
static void WriteSocketBytes(
		Environment *theEnv,
		struct socketRouter *sptr,
		const char *data,
		size_t len)
{
	size_t sent;

	if (len == 0) return;

	// The data from earlier writes goes out before this data. If that data
	// cannot go out, this data waits behind it. A direct write to the stream
	// here would put these bytes on the network before the earlier bytes.
	if (sptr->pendingLen > 0)
	{
		if (! PushPending(theEnv,sptr))
		{
			RetainTail(theEnv,sptr,data,len);
			return;
		}
	}

	sent = fwrite(data,1,len,sptr->stream);

	// A short count shows that the socket did not take all of the data. That
	// is usual on a non-blocking socket with a full send buffer. The code uses
	// fwrite and not fprintf, because fprintf cannot report this condition. It
	// gives a negative value and says nothing about the quantity that it sent,
	// and the code then cannot find the refused data again. To discard that
	// data is a write that loses data with no message, and this code prevents
	// that.
	//
	// The code does not change errno here. The write below sets errno when it
	// fails. If this code cleared errno first, each printout that succeeded
	// would remove the errno of an earlier failure.
	if (sent < len)
	{
		if (errno == 0) errno = EAGAIN;
		RetainTail(theEnv,sptr,data + sent,len - sent);
	}
}

/*****************************************************************/
/* PushPending: Sends the data that the socket refused before,   */
/*   oldest byte first. Gives true when no data is left to       */
/*   send.                                                       */
/*****************************************************************/
static bool PushPending(
		Environment *theEnv,
		struct socketRouter *sptr)
{
	ssize_t n;

	// The error flag of stdio stays set. After a write on the stream fails, an
	// implementation can refuse each later write until the code clears that
	// flag. Without this call, a socket that recovered would stay unusable.
	clearerr(sptr->stream);

	// The code wrote the data in stdio before the data in the pending buffer.
	// As a result, the stdio data must go out first, or the two parts arrive
	// in the incorrect sequence.
	if (0 != GenFlush(theEnv,sptr->stream))
	{
		if (errno == 0) errno = EAGAIN;
		return false;
	}

	// The code writes to the descriptor and not through the stream. stdio is
	// empty at this point, and a write that goes around it changes no
	// sequence. A direct write also reports the exact number of bytes that it
	// took, and the code can then find the remainder at the next attempt.
	while (sptr->pendingLen > 0)
	{
		n = write(sptr->fd,sptr->pending,sptr->pendingLen);

		if (n < 0)
		{
			if (errno == EINTR) continue;
			return false;
		}

		if (n == 0)
		{
			errno = EAGAIN;
			return false;
		}

		// The code keeps the data and does not discard it, for the cause that
		// src/socktls.c gives for the same operation. A caller that flushes
		// again after the socket is ready sends the remainder of the same
		// data, and not a shorter version of it.
		if ((size_t) n < sptr->pendingLen)
		{ memmove(sptr->pending,sptr->pending + n,sptr->pendingLen - (size_t) n); }

		sptr->pendingLen -= (size_t) n;
	}

	return true;
}

/*****************************************************************/
/* RetainTail: Keeps the bytes that the socket did not accept.   */
/*   A later flush sends them.                                   */
/*****************************************************************/
static void RetainTail(
		Environment *theEnv,
		struct socketRouter *sptr,
		const char *data,
		size_t len)
{
	size_t wanted;
	size_t room;

	// A limit is the maximum quantity of data that the code keeps. As a
	// result, this code refuses the data above the limit. errno already gives
	// the cause, and the flush after this still gives FALSE. With no limit the
	// code keeps each byte.
	wanted = sptr->pendingLen + len;

	if ((sptr->retainedLimit > 0) && (wanted > sptr->retainedLimit))
	{ wanted = sptr->retainedLimit; }

	if (wanted > sptr->pendingCap)
	{
		size_t cap;
		unsigned char *bigger;

		cap = (sptr->pendingCap == 0) ? SOCKET_PENDING_MIN : sptr->pendingCap;

		while (cap < wanted) cap *= 2;

		bigger = (unsigned char *) gm2(theEnv,cap);

		if (sptr->pendingLen > 0)
		{ memcpy(bigger,sptr->pending,sptr->pendingLen); }

		if (sptr->pending != NULL)
		{ rm(theEnv,sptr->pending,sptr->pendingCap); }

		sptr->pending = bigger;
		sptr->pendingCap = cap;
	}

	room = sptr->pendingCap - sptr->pendingLen;

	// A new limit below the quantity of data in the buffer does not discard
	// the bytes that this socket accepted before. It only stops the socket
	// from more bytes. As a result, the available space is the limit minus
	// pendingLen, and it is zero when the limit is below pendingLen.
	if (sptr->retainedLimit > 0)
	{
		size_t allowed = (sptr->retainedLimit > sptr->pendingLen)
		                 ? sptr->retainedLimit - sptr->pendingLen
		                 : 0;

		if (room > allowed) room = allowed;
	}

	if (len > room) len = room;

	if (len == 0) return;

	memcpy(sptr->pending + sptr->pendingLen,data,len);
	sptr->pendingLen += len;
}

/******************************************************************************/
/* SetRetainedLimitFunction: The H/L function that sets the maximum number of */
/*   bytes that a socket keeps for a peer that does not read. A value of 0 is */
/*   no limit, and 0 is the default. It gives FALSE for a socket that this    */
/*   library does not know and for a negative limit.                          */
/******************************************************************************/
void SetRetainedLimitFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;
	struct socketRouter *sptr;
	long long limit;

	if (NULL == (sptr = GetSocketRouterFromArgument(theEnv,context,&theArg)))
	{
		WriteString(theEnv,STDERR,"set-retained-limit: Could not find socket with that logical name\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	if (! UDFNextArgument(context,INTEGER_BIT,&theArg))
	{
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	limit = theArg.integerValue->contents;

	if (limit < 0)
	{
		WriteString(theEnv,STDERR,"set-retained-limit: a limit cannot be negative\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	// The socket already accepted the bytes in the buffer, and this call does
	// not discard them. A limit below the quantity in the buffer stops the
	// socket from more bytes, and it discards no data. This call also does not
	// change the buffer, for the same cause. The code frees that buffer when
	// the socket closes, and a smaller buffer here would move data that this
	// call must not move.
	sptr->retainedLimit = (size_t) limit;

	returnValue->lexemeValue = TrueSymbol(theEnv);
}

/******************************************************************************/
/* GetRetainedLimitFunction: The H/L function that gives the limit from       */
/*   set-retained-limit. It gives 0 when there is no limit.                   */
/******************************************************************************/
void GetRetainedLimitFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;
	struct socketRouter *sptr;

	if (NULL == (sptr = GetSocketRouterFromArgument(theEnv,context,&theArg)))
	{
		WriteString(theEnv,STDERR,"get-retained-limit: Could not find socket with that logical name\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	returnValue->integerValue = CreateInteger(theEnv,(long long) sptr->retainedLimit);
}

/******************************************************************************/
/* GetRetainedBytesFunction: The H/L function that gives the number of bytes  */
/*   that this socket keeps because the peer did not accept them.             */
/******************************************************************************/
void GetRetainedBytesFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;
	struct socketRouter *sptr;

	if (NULL == (sptr = GetSocketRouterFromArgument(theEnv,context,&theArg)))
	{
		WriteString(theEnv,STDERR,"get-retained-bytes: Could not find socket with that logical name\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	// An encrypted socket keeps its bytes in the session and not here. As a
	// result, pendingLen would give 0 for a socket that holds much data.
	if (SocketIsTLS(sptr))
	{
		returnValue->integerValue =
			CreateInteger(theEnv,(long long) TLSSessionRetained(theEnv,sptr));
		return;
	}

	returnValue->integerValue = CreateInteger(theEnv,(long long) sptr->pendingLen);
}

/*****************************************************************/
/* DisposeStream: The last opportunity to send the data that     */
/*   the socket owes. The function then frees the stream and     */
/*   the buffer for that data.                                   */
/*****************************************************************/
static void DisposeStream(
		Environment *theEnv,
		struct socketRouter *sptr)
{
	if (sptr->pendingLen > 0) PushPending(theEnv,sptr);

	if (sptr->pending != NULL)
	{
		rm(theEnv,sptr->pending,sptr->pendingCap);
		sptr->pending = NULL;
		sptr->pendingCap = 0;
		sptr->pendingLen = 0;
	}

	GenClose(theEnv,sptr->stream);
}

/***************************************************************/
/* ReadSocket: Read callback for socket router.                */
/***************************************************************/
static int ReadSocket(
		Environment *theEnv,
		const char *logicalName,
		void *context)
{
	struct socketRouter *sptr;
	int theChar;

	sptr = LogicalNameToSocketRouter(theEnv,logicalName);

	if (sptr == NULL) return EOF;

	sptr->ioStarted = true;

	theChar = getc(sptr->stream);

	return theChar;
}

/*******************************************************************/
/* UnreadSocket: Unread callback for socket router.                */
/*******************************************************************/
static int UnreadSocket(
		Environment *theEnv,
		const char *logicalName,
		int ch,
		void *context)
{
	struct socketRouter *sptr;

	sptr = LogicalNameToSocketRouter(theEnv,logicalName);

	if (sptr == NULL) return EOF;

	return ungetc(ch,sptr->stream);
}

/******************************************************/
/* LogicalNameToSocketRouter: Loop through            */
/* all socket routers                                 */
/* and return the one with the matching logical name. */
/* return NULL if such router does not exist.         */
/******************************************************/
struct socketRouter *LogicalNameToSocketRouter(
		Environment *theEnv,
		const char *logicalName)
{
	struct socketRouter *sptr;

	sptr = SocketRouterData(theEnv)->ListOfSocketRouters;
	while (sptr != NULL)
	{
		if (sptr->logicalName != NULL && 0 == strcmp(logicalName, sptr->logicalName))
		{ return sptr; }

		sptr = sptr->next;
	}

	return NULL;
}

/*********************************************************/
/* FileDescriptorToSocketRouter: Loop through all        */
/* socket routers                                        */
/* and return the one with the matching file descriptor. */
/* return NULL if such router does not exist.            */
/*********************************************************/
struct socketRouter *FileDescriptorToSocketRouter(
		Environment *theEnv,
		int sockfd)
{
	struct socketRouter *sptr;

	sptr = SocketRouterData(theEnv)->ListOfSocketRouters;
	while ((sptr != NULL) ? (sptr->fd != sockfd) : false)
	{ sptr = sptr->next; }

	if (sptr != NULL) return sptr;

	return NULL;
}

/*************************************************************************************/
/* GetFilenoFromArgument: Return the integer socket file descriptor                  */
/*    from the first argument which can either be the socket file descriptor integer */
/*    or the logical name of the socket after it's been bound or connected.          */
/*    Returns the file descriptor integer or -1 on failure.                          */
/*************************************************************************************/
int GetFilenoFromArgument(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *theArg)
{
	struct socketRouter *sptr;
	int sockfd = -1;
	UDFNextArgument(context,INTEGER_BIT|LEXEME_BITS,theArg);
	if (theArg->header->type == INTEGER_TYPE)
	{
		sockfd = theArg->integerValue->contents;
	}
	else if (theArg->header->type == STRING_TYPE || theArg->header->type == SYMBOL_TYPE)
	{
		sptr = LogicalNameToSocketRouter(theEnv, theArg->lexemeValue->contents);
		if (sptr != NULL)
		{
			sockfd = sptr->fd;
		}
	}
	return sockfd;
}

struct socketRouter *GetSocketRouterFromArgument(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *theArg
		)
{
	struct socketRouter *sptr = NULL;

	UDFNextArgument(context,INTEGER_BIT|LEXEME_BITS,theArg);
	if (theArg->header->type == INTEGER_TYPE)
	{
		sptr = FileDescriptorToSocketRouter(theEnv, theArg->integerValue->contents);
	}
	else if (theArg->header->type == STRING_TYPE || theArg->header->type == SYMBOL_TYPE)
	{
		sptr = LogicalNameToSocketRouter(theEnv, theArg->lexemeValue->contents);
	}

	return sptr;
}

/*********************************************************/
/* ExitSocket: Exit routine for the socket router.       */
/*********************************************************/
static void ExitSocket(
		Environment *theEnv,
		int num,
		void *context)
{
	CloseAllSockets(theEnv);
}

/******************************************************/
/* GenCloseSocket: Trap routine for closing a socket. */
/******************************************************/
int GenCloseSocket(
		Environment *theEnv,
		int sockfd)
{
	int rv = -1;

	rv = close(sockfd);

	return rv;
}

/********************************************************/
/* GenFcntl: Trap routine for fcntl function.           */
/********************************************************/
int GenFcntl(Environment *theEnv, int fd, int cmd, int arg)
{
	int rv = -1;

	rv = fcntl(fd, cmd, arg);

	return rv;
}

/********************************************************/
/* GenGetsockopt: Trap routine for getsockopt function. */
/********************************************************/
int GenGetsockopt(
		Environment *theEnv,
		int sockfd, 
		int level, 
		int optname,
		void *optval,
		socklen_t *optlen)
{
	int rv;

	rv = getsockopt(sockfd, level, optname, optval, optlen);

	return rv;
}


/*************************************************************/
/* GenPoll: Polls the socket or connection associated        */
/* with the specified logical name.                          */
/* Returns true if the connection has a message to read      */
/*   or the socket has a client waiting. otherwise false.    */
/*************************************************************/
bool GenPoll(
		Environment *theEnv,
		int sockfd,
		int timeout,
		int flags)
{
	struct pollfd fds;
	int retval;

	fds.fd = sockfd;
	fds.events = flags;

	retval = poll(&fds, 1, timeout);

	if (retval == -1) {
		perror("perror");
		return false;
	} else if (retval > 0) {
		return true;
	}

	return false;
}

/********************************************************/
/* GenSetsockopt: Trap routine for setsockopt function. */
/********************************************************/
int GenSetsockopt(
		Environment *theEnv,
		int sockfd, 
		int level, 
		int optname,
		const void *optval,
		socklen_t optlen)
{
	int rv = -1;

	rv = setsockopt(sockfd, level, optname, optval, optlen);

	return rv;
}

/*****************************************************/
/* GenSetvbuf: Trap routine for setvbuf function.    */
/*****************************************************/
int GenSetvbuf(Environment *theEnv, FILE *stream, char *buf, int mode, size_t size)
{
	int rv = -1;

	rv = setvbuf(stream, buf, mode, size);

	return rv;
}

/*********************************************************/
/* GenShutdown: Trap routine for shutting down a socket. */
/*********************************************************/
int GenShutdown(
		Environment *theEnv,
		int sockfd,
		int how)
{
	int rv;

	rv = shutdown(sockfd, how);

	return rv;
}

/**************************************************/
/* GenSocket: Trap routine for creating a socket. */
/**************************************************/
int GenSocket(
		Environment *theEnv,
		int domain, 
		int type, 
		int protocol)
{
	int rv = -1;

	rv = socket(domain, type, protocol);

	return rv;
}

struct socketOptionName
{
	const char *name;
	int value;
};

static const struct socketOptionName SocketDomains[] =
{
	{ "AF_INET",  AF_INET  },
	{ "AF_INET6", AF_INET6 },
	{ "AF_UNIX",  AF_UNIX  },
	{ NULL,       0        }
};

static const struct socketOptionName SocketTypes[] =
{
	{ "SOCK_STREAM", SOCK_STREAM },
	{ "SOCK_DGRAM",  SOCK_DGRAM  },
	{ NULL,          0           }
};

static const struct socketOptionName SocketOptionLevels[] =
{
	{ "SOL_SOCKET",  SOL_SOCKET  },
	{ "IPPROTO_TCP", IPPROTO_TCP },
	{ NULL,          0           }
};

static const struct socketOptionName SocketOptionNames[] =
{
	{ "SO_REUSEADDR", SO_REUSEADDR },
	{ "SO_SNDBUF",    SO_SNDBUF    },
	{ "SO_RCVBUF",    SO_RCVBUF    },
	{ "TCP_NODELAY",  TCP_NODELAY  },
	{ NULL,           0            }
};

static const struct socketOptionName FileStatusFlags[] =
{
	{ "O_NONBLOCK", O_NONBLOCK },
	{ "O_APPEND",   O_APPEND   },
	{ "O_ASYNC",    O_ASYNC    },
	{ NULL,         0          }
};

static const struct socketOptionName ShutdownDirections[] =
{
	{ "SHUT_RD",    SHUT_RD    },
	{ "SHUT_WR",    SHUT_WR    },
	{ "SHUT_RDWR",  SHUT_RDWR  },
	{ NULL,         0          }
};

static const struct socketOptionName PollEvents[] =
{
	{ "POLLIN",    POLLIN    },
	{ "POLLOUT",   POLLOUT   },
	{ "POLLERR",   POLLERR   },
	{ "POLLHUP",   POLLHUP   },
	{ "POLLNVAL",  POLLNVAL  },
	{ "POLLPRI",   POLLPRI   },
	{ NULL,        0         }
};

/* The flags that rcvfrom takes. These are the flags of recv(2) that have a
   meaning for one datagram. */
static const struct socketOptionName ReceiveFlags[] =
{
	{ "MSG_PEEK",    MSG_PEEK    },
	{ "MSG_OOB",     MSG_OOB     },
	{ "MSG_WAITALL", MSG_WAITALL },
	{ NULL,          0           }
};

/* The flags that sendto takes. MSG_MORE and MSG_NOSIGNAL have guards, because
   POSIX has neither of them. A platform without them must build and must not
   fail, and this file then does not offer the two names. */
static const struct socketOptionName SendFlags[] =
{
	{ "MSG_CONFIRM",   MSG_CONFIRM   },
	{ "MSG_DONTROUTE", MSG_DONTROUTE },
	{ "MSG_DONTWAIT",  MSG_DONTWAIT  },
	{ "MSG_EOR",       MSG_EOR       },
#ifdef MSG_MORE
	{ "MSG_MORE",      MSG_MORE      },
#endif
#ifdef MSG_NOSIGNAL
	{ "MSG_NOSIGNAL",  MSG_NOSIGNAL  },
#endif
	{ "MSG_OOB",       MSG_OOB       },
	{ NULL,            0             }
};

/*****************************************************************/
/* LookupOptionName: Looks up one name in one table. It gives    */
/*   false when the table does not have the name, and it writes  */
/*   no message. The correct action for an unknown name differs  */
/*   between the callers.                                        */
/*****************************************************************/
static bool LookupOptionName(
		const struct socketOptionName *table,
		const char *name,
		int *result)
{
	const struct socketOptionName *entry;

	for (entry = table; entry->name != NULL; entry++)
	{
		if (0 == strcmp(name,entry->name))
		{
			*result = entry->value;
			return true;
		}
	}

	return false;
}

/*****************************************************************/
/* LookupOptionValue: It gives the name                          */
/*   of a value, or NULL for a value that the table does not     */
/*   have.                                                       */
/*                                                               */
/*   This is for rcvfrom, which must give the caller the family  */
/*   that the kernel reported.                                   */
/*****************************************************************/
static const char *LookupOptionValue(
		const struct socketOptionName *table,
		int value)
{
	const struct socketOptionName *entry;

	for (entry = table; entry->name != NULL; entry++)
	{
		if (entry->value == value) return entry->name;
	}

	return NULL;
}

/*****************************************************************/
/* LookupSocketOptionName: Looks up one symbol argument in one   */
/*   of the tables above. It gives false and writes the cause,   */
/*   and the message names the type of name that it looked for.  */
/*****************************************************************/
static bool LookupSocketOptionName(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *theArg,
		const struct socketOptionName *table,
		const char *kind,
		int *result)
{
	UDFNextArgument(context,SYMBOL_BIT,theArg);

	if (LookupOptionName(table,theArg->lexemeValue->contents,result))
	{ return true; }

	WriteString(theEnv,STDERR,kind);
	WriteString(theEnv,STDERR," '");
	WriteString(theEnv,STDERR,theArg->lexemeValue->contents);
	WriteString(theEnv,STDERR,"' not supported.\n");

	return false;
}

/*****************************************************************/
/* CollectFlags: A set of flags. The caller gives them as a      */
/*   multifield of symbols, as one symbol, or as an integer. The */
/*   integer form is for a caller that knows the number and      */
/*   needs a flag that this library does not name.               */
/*                                                               */
/*   The function ignores a symbol that the table does not have. */
/*****************************************************************/
static int CollectFlags(
		UDFValue *theArg,
		const struct socketOptionName *table)
{
	int flags = 0;
	int flag;
	size_t i;

	if (theArg->header->type == INTEGER_TYPE)
	{ return (int) theArg->integerValue->contents; }

	if (theArg->header->type != MULTIFIELD_TYPE)
	{
		if (LookupOptionName(table,theArg->lexemeValue->contents,&flag))
		{ flags = flag; }

		return flags;
	}

	for (i = 0; i < theArg->multifieldValue->length; i++)
	{
		CLIPSValue *cv = &theArg->multifieldValue->contents[i];

		if (cv->header->type != SYMBOL_TYPE) continue;

		if (LookupOptionName(table,cv->lexemeValue->contents,&flag))
		{ flags |= flag; }
	}

	return flags;
}

/*****************************************************************/
/* SocketOptionArguments: The three arguments that getsockopt    */
/*   and setsockopt both start with: a socket, a level and an    */
/*   option. This function reads and checks them in one place.   */
/*                                                               */
/*   The parameter func is the name that the caller used. As a   */
/*   result, a message names the function that the caller        */
/*   called.                                                     */
/*****************************************************************/
static bool SocketOptionArguments(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *theArg,
		const char *func,
		int *sockfd,
		int *level,
		int *optname)
{
	if (-1 == (*sockfd = GetFilenoFromArgument(theEnv,context,theArg)))
	{
		WriteString(theEnv,STDERR,func);
		WriteString(theEnv,STDERR,": could not find router for socket file descriptor\n");
		return false;
	}

	if (! LookupSocketOptionName(theEnv,context,theArg,SocketOptionLevels,"Level",level))
	{ return false; }

	if (! LookupSocketOptionName(theEnv,context,theArg,SocketOptionNames,"optname",optname))
	{ return false; }

	return true;
}

/******************************************************************/
/* GetsockoptFunction: HL function for getsockopt socket function */
/******************************************************************/
void GetsockoptFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;
	int sockfd, level, optname, flag;
	socklen_t flag_len = sizeof(flag);

	if (! SocketOptionArguments(theEnv,context,&theArg,"getsockopt",&sockfd,&level,&optname))
	{
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	flag = -1;
	if (0 > GenGetsockopt(theEnv, sockfd, level, optname, &flag, &flag_len))
	{
		WriteString(theEnv,STDERR,"Something went wrong with getsockopt\n");
		perror("perror");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	returnValue->integerValue = CreateInteger(theEnv, flag);
}

/******************************************************************/
/* SetsockoptFunction: HL function for setsockopt socket function */
/******************************************************************/
void SetsockoptFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;
	int sockfd, level, optname, flag;

	if (! SocketOptionArguments(theEnv,context,&theArg,"setsockopt",&sockfd,&level,&optname))
	{
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	/*====================*/
	/* Get the flag.      */
	/*====================*/
	UDFNextArgument(context,INTEGER_BIT,&theArg);
	flag = theArg.integerValue->contents;

	if (0 > GenSetsockopt(theEnv, sockfd, level, optname, (const void *)&flag, sizeof(flag)))
	{
		WriteString(theEnv,STDERR,"Something went wrong with setsockopt\n");
		perror("perror");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	returnValue->lexemeValue = TrueSymbol(theEnv);
}

/******************************************************************/
/* CreateSocketFunction: Creates a socket                         */
/*   with the specified access mode                               */
/*   and stores the opened stream on the list of sockets          */
/*   associated with logical names. Returns true if the           */
/*   socket was succesfully opened, otherwise false.              */
/******************************************************************/
void CreateSocketFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	int sock, domain, type, protocol;
	struct socketRouter *newRouter;
	UDFValue theArg;

	/*====================*/
	/* Get the domain.    */
	/*====================*/

	if (! LookupSocketOptionName(theEnv,context,&theArg,SocketDomains,"Domain",&domain))
	{
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	/*====================*/
	/* Get the type.      */
	/*====================*/

	if (! LookupSocketOptionName(theEnv,context,&theArg,SocketTypes,"Type",&type))
	{
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	/*====================*/
	/* Get the protocol.  */
	/*====================*/
	if (!UDFHasNextArgument(context))
	{
		protocol = 0;
	}
	else
	{
		UDFNextArgument(context,INTEGER_BIT,&theArg);
		protocol = theArg.integerValue->contents;
	}


	/*=========================================*/
	/* Make sure the socket can be opened      */
	/* with the specified domain and protocol. */
	/*=========================================*/

	if (0 > (sock = GenSocket(theEnv,domain,type,protocol)))
	{
		WriteString(theEnv,STDERR,"Could not create socket\n");
		perror("perror");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	/*=============================*/
	/* Create a new socket router. */
	/*=============================*/
	newRouter = get_struct(theEnv,socketRouter);
	newRouter->domain = domain;
	newRouter->type = type;
	newRouter->fd = sock;
	newRouter->tls = NULL;
	newRouter->ioStarted = false;
	// get_struct gives memory from a free list. As a result, no field here is
	// zero until the code sets it. An old value in the pending pointer would
	// go to a free call as if this code had allocated it.
	newRouter->pending = NULL;
	newRouter->pendingLen = 0;
	newRouter->pendingCap = 0;
	newRouter->retainedLimit = 0;
	// A socket has no logical name until it is bound, connected or accepted.
	newRouter->logicalName = NULL;
	newRouter->stream = fdopen(sock, "r+");

	/*=========================================*/
	/* Wrap the opened socket in a FILE        */
	/* with fdopen.                            */
	/*=========================================*/
	if (NULL == (newRouter->stream))
	{
		WriteString(theEnv,STDERR,"Could not fdopen socket\n");
		perror("perror");
		GenCloseSocket(theEnv,sock);
		rm(theEnv,newRouter,sizeof(struct socketRouter));
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	/*==========================================*/
	/* Add the newly opened file to the list of */
	/* socket file descriptors with FILE *.     */
	/*==========================================*/

	newRouter->next = SocketRouterData(theEnv)->ListOfSocketRouters;
	SocketRouterData(theEnv)->ListOfSocketRouters = newRouter;

	returnValue->integerValue = CreateInteger(theEnv, sock);
}

/******************************************************************************/
/* FlushConnection: Sends each byte that the connection owes. It gives true   */
/*   when no data is left. It gives false when the socket did not take all of */
/*   the data, and it then sets errno. errno is EAGAIN for a full send        */
/*   buffer. A false here is not a lost write. The code keeps the data that   */
/*   could not go out, and a second flush sends the remainder after the peer  */
/*   reads enough data.                                                       */
/******************************************************************************/
bool FlushConnection(
		Environment *theEnv,
		struct socketRouter *sptr)
{
	if (SocketIsTLS(sptr))
	{ return TLSFlushSession(theEnv,sptr); }

	return PushPending(theEnv,sptr);
}

/******************************************************************************/
/* FlushConnectionFunction: Flushed the connection                            */
/* associated with the specified logical name or file descriptor.             */
/* Returns TRUE on success, FALSE on failure.                                 */
/******************************************************************************/
void
FlushConnectionFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;
	struct socketRouter *sptr;

	if (NULL == (sptr = GetSocketRouterFromArgument(theEnv,context,&theArg)))
	{
		WriteString(theEnv,STDERR,"flush-connection: Could not find socket with that logical name\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	returnValue->lexemeValue = CreateBoolean(theEnv,FlushConnection(theEnv,sptr));
}

bool EmptyConnection(
		Environment *theEnv,
		struct socketRouter *sptr)
{
	int ch;

	if (SocketIsTLS(sptr))
	{ return TLSEmptySession(theEnv,sptr); }

	while ((ch = fgetc(sptr->stream)) != EOF);

	return true;
}

/******************************************************************************/
/* EmptyConnectionFunction: Empty the stream buffer                           */
/* associated with the specified logical name or file descriptor.             */
/* Returns TRUE on success, FALSE on failure.                                 */
/******************************************************************************/
void
EmptyConnectionFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;
	struct socketRouter *sptr;

	if (NULL == (sptr = GetSocketRouterFromArgument(theEnv,context,&theArg)))
	{
		WriteString(theEnv,STDERR,"empty-connection: Could not find socket; are you sure it's accepted or connected?\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	returnValue->lexemeValue = CreateBoolean(theEnv,EmptyConnection(theEnv,sptr));
}

/***************************************************************************************/
/* CloseSocketRouter: Ends one connection and removes it from the list. The            */
/*   parameter prev is the router before it in the list, or NULL when the              */
/*   connection is the first one.                                                      */
/***************************************************************************************/
static void CloseSocketRouter(
		Environment *theEnv,
		struct socketRouter *sptr,
		struct socketRouter *prev)
{
	// The session sends its close message on the network while the descriptor
	// is open. The close of the stream ends the connection.
	if (SocketIsTLS(sptr)) TLSCloseSession(theEnv,sptr);
	DisposeStream(theEnv,sptr);

	// A socket that the code made but never bound, connected or accepted has
	// no logical name to release.
	if (sptr->logicalName != NULL)
	{ rm(theEnv,(void *) sptr->logicalName,strlen(sptr->logicalName) + 1); }

	if (prev == NULL)
	{ SocketRouterData(theEnv)->ListOfSocketRouters = sptr->next; }
	else
	{ prev->next = sptr->next; }

	rm(theEnv,sptr,sizeof(struct socketRouter));
}

/***************************************************************************************/
/* CloseFileDescriptorConnection: Closes the connection associated with the specified  */
/*   connection file descriptor. Returns true if the connection was successfully       */
/*   closed, otherwise false.                                                          */
/***************************************************************************************/
bool CloseFileDescriptorConnection(
		Environment *theEnv,
		int socketfd)
{
	struct socketRouter *sptr, *prev;

	for (sptr = SocketRouterData(theEnv)->ListOfSocketRouters, prev = NULL;
			sptr != NULL;
			prev = sptr, sptr = sptr->next)
	{
		if (sptr->fd == socketfd)
		{
			CloseSocketRouter(theEnv,sptr,prev);
			return true;
		}
	}

	return false;
}

/******************************************************************************/
/* CloseNamedConnection: Closes the connection associated with the specified  */
/*   connection logical name. Returns true if the connection was successfully */
/*   closed, otherwise false.                                                 */
/******************************************************************************/
bool CloseNamedConnection(
		Environment *theEnv,
		const char *logicalName)
{
	struct socketRouter *sptr, *prev;

	for (sptr = SocketRouterData(theEnv)->ListOfSocketRouters, prev = NULL;
			sptr != NULL;
			prev = sptr, sptr = sptr->next)
	{
		if (sptr->logicalName != NULL && strcmp(sptr->logicalName,logicalName) == 0)
		{
			CloseSocketRouter(theEnv,sptr,prev);
			return true;
		}
	}

	return false;
}

/************************************************/
/* CloseConnectionFunction: H/L access function */
/*    for closing all bound/connected sockets   */
/*    currently associated with an I/O Router.  */
/************************************************/
void CloseConnectionFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;

	UDFNextArgument(context,INTEGER_BIT|LEXEME_BITS,&theArg);
	if (theArg.header->type == INTEGER_TYPE)
	{
		returnValue->lexemeValue = CreateBoolean(theEnv, CloseFileDescriptorConnection(theEnv, theArg.integerValue->contents));
	}
	else
	{
		returnValue->lexemeValue = CreateBoolean(theEnv, CloseNamedConnection(theEnv, theArg.lexemeValue->contents));
	}
}

/***************************************************/
/* ShutdownConnectionFunction: H/L access function */
/*    for running shutown on all bound/connected   */
/*    sockets associated with an I/O Router.       */
/*    Lets you specify that no more data can be    */
/*    sent, received or both.                      */
/***************************************************/
void ShutdownConnectionFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;

	int sockfd, how;
	if (-1 == (sockfd = GetFilenoFromArgument(theEnv,context,&theArg)))
	{
		WriteString(theEnv,STDERR,"shutdown-connection: could not find router for socket file descriptor\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	// An absent direction, or a direction with a symbol that this library does
	// not know, means both directions.
	how = SHUT_RDWR;
	if (UDFHasNextArgument(context))
	{
		UDFNextArgument(context,SYMBOL_BIT,&theArg);

		if (! LookupOptionName(ShutdownDirections,theArg.lexemeValue->contents,&how))
		{ how = SHUT_RDWR; }
	}
	// shutdown() reports success as 0, so the return value has to be compared
	// rather than handed to CreateBoolean directly -- otherwise success comes
	// back as FALSE and failure as TRUE.
	returnValue->lexemeValue = CreateBoolean(theEnv,0 == GenShutdown(theEnv,sockfd,how));
}

/*************************************************************************/
/* DecryptedBytesWaiting: Tells if a TLS socket holds input that poll(2) */
/*   cannot see. The library decrypts a record that already arrived, and */
/*   there is then nothing on the descriptor to report. As a result, a   */
/*   poll about readability would wait for data that already arrived.    */
/*************************************************************************/
static bool DecryptedBytesWaiting(
		Environment *theEnv,
		int sockfd,
		int flags)
{
	struct socketRouter *sptr;

	if ((flags & POLLIN) == 0) return false;

	sptr = FileDescriptorToSocketRouter(theEnv,sockfd);

	if ((sptr == NULL) || (! SocketIsTLS(sptr))) return false;

	return TLSSessionPending(theEnv,sptr) > 0;
}

/****************************************************/
/* PollFunction: H/L access function                */
/*    to check whether certain events have occurred */
/*    in the socket, such as data received          */
/*    or client is waiting to be accepted.          */
/****************************************************/
void PollFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;

	int sockfd, timeout;
	if (-1 == (sockfd = GetFilenoFromArgument(theEnv,context,&theArg)))
	{
		WriteString(theEnv,STDERR,"poll: could not find router for socket file descriptor\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	UDFNextArgument(context,INTEGER_BIT,&theArg);
	timeout = theArg.integerValue->contents;

	// by default, poll for all flags
	if (!UDFHasNextArgument(context))
	{
		if (DecryptedBytesWaiting(theEnv,sockfd,POLLIN))
		{
			returnValue->lexemeValue = TrueSymbol(theEnv);
			return;
		}

		returnValue->lexemeValue =
			CreateBoolean(theEnv,GenPoll(theEnv,sockfd,timeout,POLLIN | POLLOUT | POLLERR | POLLHUP | POLLNVAL | POLLPRI));
		return;
	}

	// This code refuses an unknown name and does not ignore it. The flags of
	// rcvfrom and sendto are different. A poll with an event that it does not
	// know would watch for a different event. With no events at all it would
	// watch for nothing, and it would then answer as if the socket were
	// idle.
	int flags = 0;
	while (UDFHasNextArgument(context))
	{
		int event;

		if (! LookupSocketOptionName(theEnv,context,&theArg,PollEvents,
				"Event for poll",&event))
		{
			returnValue->lexemeValue = FalseSymbol(theEnv);
			return;
		}

		flags |= event;
	}

	if (DecryptedBytesWaiting(theEnv,sockfd,flags))
	{
		returnValue->lexemeValue = TrueSymbol(theEnv);
		return;
	}

	returnValue->lexemeValue = CreateBoolean(theEnv,GenPoll(theEnv,sockfd,timeout,flags));
}


/*****************************************************************/
/* AppendEndpointName: The logical name of one endpoint, in the  */
/*   one shape that this library uses.                           */
/*                                                               */
/*   This is one function because three callers make these       */
/*   names: bind-socket, connect and accept. A logical name      */
/*   identifies a socket only when the three write it in the     */
/*   same manner.                                                */
/*****************************************************************/
static void AppendEndpointName(
		StringBuilder *sb,
		int domain,
		const char *address,
		long long port)
{
	if (domain == AF_INET6)
	{
		SBAddChar(sb,'[');
		SBAppend(sb,address);
		SBAddChar(sb,']');
	}
	else
	{ SBAppend(sb,address); }

	// A unix address is a file path and has no port.
	if (domain == AF_UNIX) return;

	SBAddChar(sb,':');
	SBAppendInteger(sb,port);
}

/*****************************************************************/
/* FillSocketAddress: Takes an address and a port as the caller  */
/*   wrote them, and makes the sockaddr that a system call       */
/*   needs. It gives false and writes the cause to STDERR.       */
/*****************************************************************/
static bool FillSocketAddress(
		Environment *theEnv,
		const char *func,
		int domain,
		const char *address,
		bool havePort,
		long long port,
		struct sockaddr_storage *addr,
		socklen_t *addrLen)
{
	memset(addr,0,sizeof(*addr));

	switch (domain)
	{
		case AF_INET:
		{
			struct sockaddr_in *in = (struct sockaddr_in *) addr;

			if (! havePort)
			{
				WriteString(theEnv,STDERR,func);
				WriteString(theEnv,STDERR,": an AF_INET address needs a port\n");
				return false;
			}

			if (1 != inet_pton(AF_INET,address,&in->sin_addr))
			{
				WriteString(theEnv,STDERR,func);
				WriteString(theEnv,STDERR,": '");
				WriteString(theEnv,STDERR,address);
				WriteString(theEnv,STDERR,"' is not a dotted-quad IPv4 address\n");
				return false;
			}

			in->sin_family = AF_INET;
			in->sin_port = htons((uint16_t) port);
			*addrLen = sizeof(*in);
			break;
		}

		case AF_INET6:
		{
			struct sockaddr_in6 *in6 = (struct sockaddr_in6 *) addr;

			if (! havePort)
			{
				WriteString(theEnv,STDERR,func);
				WriteString(theEnv,STDERR,": an AF_INET6 address needs a port\n");
				return false;
			}

			if (1 != inet_pton(AF_INET6,address,&in6->sin6_addr))
			{
				WriteString(theEnv,STDERR,func);
				WriteString(theEnv,STDERR,": '");
				WriteString(theEnv,STDERR,address);
				WriteString(theEnv,STDERR,"' is not an IPv6 address\n");
				return false;
			}

			in6->sin6_family = AF_INET6;
			in6->sin6_port = htons((uint16_t) port);
			*addrLen = sizeof(*in6);
			break;
		}

		case AF_UNIX:
		{
			struct sockaddr_un *un = (struct sockaddr_un *) addr;

			// Without this check, the code would bind a shorter form of a
			// path that is too long, and that shorter path names a
			// different file.
			if (strlen(address) >= sizeof(un->sun_path))
			{
				WriteString(theEnv,STDERR,func);
				WriteString(theEnv,STDERR,": '");
				WriteString(theEnv,STDERR,address);
				WriteString(theEnv,STDERR,"' is too long for a unix socket path\n");
				return false;
			}

			un->sun_family = AF_UNIX;
			genstrcpy(un->sun_path,address);
			*addrLen = (socklen_t) (offsetof(struct sockaddr_un,sun_path)
			                        + strlen(un->sun_path));
			break;
		}

		case AF_UNSPEC:
		default:
			WriteString(theEnv,STDERR,func);
			WriteString(theEnv,STDERR,": '");
			WriteString(theEnv,STDERR,address);
			WriteString(theEnv,STDERR,"': socket domain not supported.\n");
			return false;
	}

	return true;
}

/*****************************************************************/
/* BuildSocketAddress: Reads the address and port arguments that */
/*   bind-socket and connect share, makes a sockaddr from them,  */
/*   and makes the logical name that the socket answers to. It   */
/*   gives false and writes the cause to STDERR.                 */
/*****************************************************************/
static bool BuildSocketAddress(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *theArg,
		struct socketRouter *sptr,
		const char *func,
		struct sockaddr_storage *addr,
		socklen_t *addrLen,
		StringBuilder *sb)
{
	UDFValue portArg;
	const char *address;
	long long port = 0;
	bool havePort = false;

	UDFNextArgument(context,LEXEME_BITS,theArg);
	address = theArg->lexemeValue->contents;

	if (UDFHasNextArgument(context))
	{
		UDFNextArgument(context,INTEGER_BIT,&portArg);
		port = portArg.integerValue->contents;
		havePort = true;
	}

	if (! FillSocketAddress(theEnv,func,sptr->domain,address,havePort,port,
			addr,addrLen))
	{ return false; }

	AppendEndpointName(sb,sptr->domain,address,port);

	return true;
}

/*****************************************************************/
/* AddressText: Gives the address and port that the kernel       */
/*   wrote, as the text that a caller writes. It gives false for */
/*   a family that this library does not have, for an address    */
/*   that it cannot write, and for a unix address with no path.  */
/*****************************************************************/
static bool AddressText(
		int domain,
		const struct sockaddr_storage *addr,
		socklen_t addrLen,
		char *text,
		size_t textLen,
		long long *port)
{
	*port = 0;

	switch (domain)
	{
		case AF_INET:
		{
			const struct sockaddr_in *in = (const struct sockaddr_in *) addr;

			if (NULL == inet_ntop(AF_INET,&in->sin_addr,text,(socklen_t) textLen))
			{ return false; }

			*port = ntohs(in->sin_port);
			return true;
		}

		case AF_INET6:
		{
			const struct sockaddr_in6 *in6 = (const struct sockaddr_in6 *) addr;

			if (NULL == inet_ntop(AF_INET6,&in6->sin6_addr,text,(socklen_t) textLen))
			{ return false; }

			*port = ntohs(in6->sin6_port);
			return true;
		}

		case AF_UNIX:
		{
			const struct sockaddr_un *un = (const struct sockaddr_un *) addr;
			size_t pathLen;

			if (addrLen <= (socklen_t) offsetof(struct sockaddr_un,sun_path))
			{ return false; }

			// The code measures inside sun_path and does not use strlen.
			// strlen would read after the end of the field and look for a
			// terminator. A path that fills the field exactly has no
			// terminator. The kernel does terminate the paths that it
			// reports, and this code is the second protection.
			for (pathLen = 0; pathLen < sizeof(un->sun_path); pathLen++)
			{
				if (un->sun_path[pathLen] == '\0') break;
			}

			if (pathLen >= textLen) return false;

			memcpy(text,un->sun_path,pathLen);
			text[pathLen] = '\0';
			return true;
		}
	}

	return false;
}

/*****************************************************************/
/* AppendPeerName: Names the far end of an accepted connection   */
/*   from the address that accept reported.                      */
/*****************************************************************/
static bool AppendPeerName(
		Environment *theEnv,
		struct socketRouter *sptr,
		const struct sockaddr_storage *peer,
		socklen_t peerLen,
		StringBuilder *sb)
{
	char text[SOCKET_ADDRESS_TEXT];
	long long port;

	switch (sptr->domain)
	{
		case AF_INET:
		case AF_INET6:
			if (! AddressText(sptr->domain,peer,peerLen,text,sizeof(text),&port))
			{ return false; }

			AppendEndpointName(sb,sptr->domain,text,port);
			return true;

		case AF_UNIX:
			// A unix client does not have to bind a path, and almost no
			// client does. As a result, there is usually no peer address
			// for this name. accept reports a length for the family and
			// nothing more. In that condition the useful name is the path
			// of this connection, and the listen socket already has that
			// name.
			if (AddressText(AF_UNIX,peer,peerLen,text,sizeof(text),&port)
			    && (text[0] != '\0'))
			{ AppendEndpointName(sb,AF_UNIX,text,0); }
			else if (sptr->logicalName != NULL)
			{ AppendEndpointName(sb,AF_UNIX,sptr->logicalName,0); }

			return true;
	}

	return false;
}

/**************************************************************/
/* BindSocketFunction: Binds a socket to an address           */
/*   for example ip:address                                   */
/*   and stores the opened stream on the list of sockets      */
/*   associated with logical names. Returns the logical name  */
/*   of the I/O Router used to index the socket FILE *.       */
/**************************************************************/
void BindSocketFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	struct socketRouter *sptr;
	struct sockaddr_storage serv_addr;
	UDFValue theArg;
	StringBuilder *logicalNameStringBuilder = CreateStringBuilder(theEnv, 0);
	socklen_t addr_len;
	char *theName;

	UDFNextArgument(context,INTEGER_BIT,&theArg);
	if (NULL == (sptr = FileDescriptorToSocketRouter(theEnv, theArg.integerValue->contents)))
	{
		WriteString(theEnv,STDERR,"bind-socket: argument was not recognized as a socket file descriptor\n");
		SBDispose(logicalNameStringBuilder);
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	if (! BuildSocketAddress(theEnv,context,&theArg,sptr,"bind-socket",
			&serv_addr,&addr_len,logicalNameStringBuilder))
	{
		SBDispose(logicalNameStringBuilder);
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	if (sptr->domain == AF_UNIX)
	{ unlink(theArg.lexemeValue->contents); }

	/*====================================*/
	/* Bind the socket with the address.  */
	/*====================================*/

	if (bind(sptr->fd, (struct sockaddr *)&serv_addr, addr_len) < 0)
	{
		WriteString(theEnv,STDERR,"Could not bind ");
		WriteString(theEnv,STDERR,theArg.lexemeValue->contents);
		WriteString(theEnv,STDERR,"\n");
		perror("perror");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		SBDispose(logicalNameStringBuilder);
		return;
	}

	theName = (char *) gm2(theEnv,strlen(logicalNameStringBuilder->contents) + 1);
	genstrcpy(theName,logicalNameStringBuilder->contents);
	sptr->logicalName = theName;
	SBDispose(logicalNameStringBuilder);

	returnValue->lexemeValue = CreateSymbol(theEnv, sptr->logicalName);
}

/*********************************************/
/* ListenFunction: Marks a socket as passive */
/*   so that connections may be accept()ed   */
/*********************************************/
void ListenFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	struct socketRouter *sptr;
	int sockfd, backlog;
	UDFValue theArg;

	if (NULL == (sptr = GetSocketRouterFromArgument(theEnv,context,&theArg)))
	{
		WriteString(theEnv,STDERR,"listen: Could not find bound socket; are you sure it's bound?\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	sockfd = sptr->fd;

	if (UDFHasNextArgument(context))
	{
		UDFNextArgument(context,INTEGER_BIT,&theArg);
		backlog = theArg.integerValue->contents;
	}
	else
	{
		backlog = 15;
	}

	/*==========================================*/
	/* Put the server socket into listen state. */
	/*==========================================*/

	if (listen(sockfd, backlog) < 0)
	{
		WriteString(theEnv,STDERR,"Could not listen to socket");
		WriteInteger(theEnv,STDERR,theArg.integerValue->contents);
		perror("perror");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	returnValue->lexemeValue = TrueSymbol(theEnv);
}

/********************************************************/
/* GetSocketLogicalNameFunction: H/L function to return */
/*      the logical name of an I/O Router               */
/*      for the socket id.                              */
/********************************************************/
void GetSocketLogicalNameFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;

	// sockfd
	UDFNextArgument(context,INTEGER_BIT,&theArg);
	int sockfd = theArg.integerValue->contents;

	struct socketRouter *sptr;
	if (NULL == (sptr = FileDescriptorToSocketRouter(theEnv, sockfd)))
	{
		WriteString(theEnv,STDERR,"get-socket-logical-name: argument was not recognized as a socket file descriptor\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	if (NULL == sptr->logicalName)
	{
		WriteString(theEnv,STDERR,"get-socket-logical-name: socket has no logical name until it is bound, connected or accepted\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	returnValue->lexemeValue = CreateSymbol(theEnv, sptr->logicalName);
}

/************************************************/
/* AcceptFunction a connection on the socket    */
/* and return an integer representing           */
/* the file descriptor for the connected client */
/************************************************/
void AcceptFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	struct socketRouter *sptr = NULL;
	FILE *newstream;
	StringBuilder *logicalNameStringBuilder;
	UDFValue theArg;
	struct sockaddr_storage client_addr;
	int connection_fd;
	struct socketRouter *newRouter;
	socklen_t client_addr_len;
	char *theName;

	if (NULL == (sptr = GetSocketRouterFromArgument(theEnv, context, &theArg)))
	{
		WriteString(theEnv,STDERR,"accept: argument was not recognized as a socket file descriptor\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	// accept writes only the part of this structure that the address of the
	// peer needs, and it gives that length in client_addr_len. A unix client
	// that bound no path leaves sun_path unchanged. Without this code, the
	// name below would come from the old value of the stack.
	memset(&client_addr, 0, sizeof(client_addr));
	client_addr_len = sizeof(client_addr);
	logicalNameStringBuilder = CreateStringBuilder(theEnv, 0);

	/*====================================*/
	/* Accept a connection on the socket. */
	/*====================================*/
	if ((connection_fd = accept(sptr->fd, (struct sockaddr *)&client_addr, &client_addr_len)) < 0)
	{
		WriteString(theEnv,STDERR,"Could not accept connection on socket '");
		WriteString(theEnv,STDERR,sptr->logicalName != NULL ? sptr->logicalName : "(unnamed socket)");
		WriteString(theEnv,STDERR,"'\n");
		perror("perror");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	/*========================================*/
	/* Build logical name for accepted client */
	/*========================================*/

	if (! AppendPeerName(theEnv,sptr,&client_addr,client_addr_len,
			logicalNameStringBuilder))
	{
		WriteString(theEnv,STDERR,"Could not accept; socket domain '");
		WriteInteger(theEnv,STDERR,sptr->domain);
		WriteString(theEnv,STDERR,"' not supported.\n");
		SBDispose(logicalNameStringBuilder);
		GenCloseSocket(theEnv,connection_fd);
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	// The name is the name of the peer, and a peer does not identify one
	// socket. Two clients can reach this process from the same address. On a
	// unix path no client has an address. On IP a client can bind one source
	// port and connect it to two listen sockets here. The descriptor is unique
	// inside the process. As a result, the descriptor at the end keeps the
	// peer readable in the name and also tells the connections apart. connect
	// names the other end of a connection in the same manner and for the same
	// cause.
	SBAddChar(logicalNameStringBuilder, '#');
	SBAppendInteger(logicalNameStringBuilder, connection_fd);

	/*=========================================*/
	/* Wrap the opened socket in a FILE        */
	/* with fdopen.                            */
	/*=========================================*/
	if (NULL == (newstream = fdopen(connection_fd, "r+")))
	{
		WriteString(theEnv,STDERR,"Could not fdopen sock file descriptor for ");
		WriteString(theEnv,STDERR,logicalNameStringBuilder->contents);
		WriteString(theEnv,STDERR,"\n");
		perror("perror");
		SBDispose(logicalNameStringBuilder);
		GenCloseSocket(theEnv,connection_fd);
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	/*=============================*/
	/* Create a new socket router. */
	/*=============================*/

	newRouter = get_struct(theEnv,socketRouter);
	theName = (char *) gm2(theEnv,strlen(logicalNameStringBuilder->contents) + 1);
	genstrcpy(theName,logicalNameStringBuilder->contents);
	newRouter->logicalName = theName;
	SBDispose(logicalNameStringBuilder);
	newRouter->stream = newstream;
	newRouter->fd = connection_fd;
	// An accepted connection starts as plaintext, whatever the settings of the
	// listen socket are. A handshake changes this field.
	newRouter->tls = NULL;
	newRouter->ioStarted = false;
	newRouter->pending = NULL;
	newRouter->pendingLen = 0;
	newRouter->pendingCap = 0;
	newRouter->retainedLimit = 0;
	// An accepted connection is in the same domain and of the same type as the
	// socket that was listening. Without this the fields hold whatever was in
	// the recycled struct, and every switch on sptr->domain reads garbage.
	newRouter->domain = sptr->domain;
	newRouter->type = sptr->type;

	/*==========================================*/
	/* Add the newly opened file to the list of */
	/* files associated with logical names.     */
	/*==========================================*/

	newRouter->next = SocketRouterData(theEnv)->ListOfSocketRouters;
	SocketRouterData(theEnv)->ListOfSocketRouters = newRouter;
	returnValue->integerValue = CreateInteger(theEnv, connection_fd);
	return;
}

/********************************************************/
/* GetTimeoutFunction: H/l access function              */
/*    for get-timeout.                                  */
/*    Returns timeout in number of microseconds         */
/*    for the socket fd or logical name, FALSE on error */
/********************************************************/
void GetTimeoutFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;
	int getsockopt_return_value;
	struct timeval timeout;

	int sockfd;

	if (-1 == (sockfd = GetFilenoFromArgument(theEnv,context,&theArg)))
	{
		WriteString(theEnv,STDERR,"get-timeout: could not find router for socket file descriptor\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	socklen_t timeout_len = sizeof(timeout);
	getsockopt_return_value = GenGetsockopt(
			theEnv,
			sockfd,
			SOL_SOCKET,
			SO_RCVTIMEO,
			&timeout,
			&timeout_len);
	if (getsockopt_return_value < 0)
	{
		WriteString(theEnv,STDERR,"Could not get timeout on '");
		WriteInteger(theEnv,STDERR,sockfd);
		WriteString(theEnv,STDERR,"'");
		perror("perror");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}
	returnValue->integerValue = CreateInteger(theEnv, timeout.tv_sec * 1000000 + timeout.tv_usec);
}

/********************************************************/
/* SetTimeoutFunction: H/l access function              */
/*    for set-timeout.                                  */
/*    Sets timeout in number of microseconds            */
/*    for the socket fd or logical name, FALSE on error */
/********************************************************/
void SetTimeoutFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	struct timeval tv;
	UDFValue theArg;
	long long seconds, microseconds;

	int sockfd;

	if (-1 == (sockfd = GetFilenoFromArgument(theEnv,context,&theArg)))
	{
		WriteString(theEnv,STDERR,"set-timeout: could not find router for socket file descriptor\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	// Two arguments after the socket are the seconds and then the
	// microseconds, in the sequence of struct timeval. One argument is the
	// microseconds alone.
	if (! UDFNextArgument(context,INTEGER_BIT,&theArg))
	{
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	if (UDFHasNextArgument(context))
	{
		seconds = theArg.integerValue->contents;
		if (! UDFNextArgument(context,INTEGER_BIT,&theArg))
		{
			returnValue->lexemeValue = FalseSymbol(theEnv);
			return;
		}
		microseconds = theArg.integerValue->contents;
	}
	else
	{
		seconds = 0;
		microseconds = theArg.integerValue->contents;
	}

	if (seconds < 0 || microseconds < 0)
	{
		WriteString(theEnv,STDERR,"set-timeout: a timeout cannot be negative\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	// The kernel refuses a microseconds field of 1000000 or more, and it gives
	// EDOM. As a result, the whole seconds move out of that field and into the
	// seconds field. Without this code, a caller that asks for one second in
	// microseconds gets a call that fails and a socket with no limit. The form
	// with one argument gives no other method to write one second.
	seconds += microseconds / 1000000;
	microseconds %= 1000000;

	tv.tv_sec = (time_t) seconds;
	tv.tv_usec = (suseconds_t) microseconds;

	if (0 > GenSetsockopt(theEnv,sockfd,SOL_SOCKET,SO_RCVTIMEO,
	                      (const void *)&tv,sizeof(tv)))
	{
		WriteString(theEnv,STDERR,"set-timeout: could not set the timeout\n");
		perror("perror");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	returnValue->lexemeValue = TrueSymbol(theEnv);
}

/********************************************************/
/* ConnectFunction: H/l access function                 */
/*    for connect.                                      */
/*    Connects a given socket to an address             */
/*    and optional port, FALSE on error                 */
/********************************************************/
void ConnectFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	StringBuilder *logicalNameStringBuilder;
	struct socketRouter *sptr;
	struct sockaddr_storage serv_addr;
	socklen_t addr_len;
	UDFValue theArg;
	char *theName;

	logicalNameStringBuilder = CreateStringBuilder(theEnv, 0);

	/*********************/
	/* get socket fd     */
	/*********************/
	UDFNextArgument(context,INTEGER_BIT,&theArg);
	if (NULL == (sptr = FileDescriptorToSocketRouter(theEnv, theArg.integerValue->contents)))
	{
		WriteString(theEnv,STDERR,"connect: argument was not recognized as a socket file descriptor\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	if (! BuildSocketAddress(theEnv,context,&theArg,sptr,"connect",
			&serv_addr,&addr_len,logicalNameStringBuilder))
	{
		SBDispose(logicalNameStringBuilder);
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	if (0 > connect(sptr->fd, (struct sockaddr*)&serv_addr, addr_len))
	{
		WriteString(theEnv,STDERR,"Could not connect to '");
		WriteString(theEnv,STDERR,logicalNameStringBuilder->contents);
		WriteString(theEnv,STDERR,"'\n");
		perror("perror");
		SBDispose(logicalNameStringBuilder);
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	SBAddChar(logicalNameStringBuilder, '#');
	SBAppendInteger(logicalNameStringBuilder, sptr->fd);

	theName = (char *) gm2(theEnv,strlen(logicalNameStringBuilder->contents) + 1);
	genstrcpy(theName,logicalNameStringBuilder->contents);
	sptr->logicalName = theName;
	SBDispose(logicalNameStringBuilder);

	returnValue->lexemeValue = CreateSymbol(theEnv, sptr->logicalName);
}

bool GenSetBuffered(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue,
		int mode,
		const char *func)
{
	UDFValue theArg;
	struct socketRouter *sptr;

	if (NULL == (sptr = GetSocketRouterFromArgument(theEnv,context,&theArg)))
	{
		WriteString(theEnv,STDERR,"set-");
		WriteString(theEnv,STDERR,func);
		WriteString(theEnv,STDERR,"-buffered: Could not find bound socket; are you sure it's bound?\n");
		return false;
	}

	if (SocketIsTLS(sptr))
	{
		if (TLSSetSessionBuffered(theEnv,sptr,mode)) return true;

		WriteString(theEnv,STDERR,"set-");
		WriteString(theEnv,STDERR,func);
		WriteString(theEnv,STDERR,"-buffered failed\n");
		return false;
	}

	if (0 > GenSetvbuf(theEnv, sptr->stream, NULL, mode, 0))
	{
		WriteString(theEnv,STDERR,"set-");
		WriteString(theEnv,STDERR,func);
		WriteString(theEnv,STDERR,"-buffered failed\n");
		return false;
	}

	return true;
}

/*********************************************************************/
/* SetNotBufferedFunction: H/L access function for set-not-buffered. */
/* Sets the buffer type to unbuffered for a FILE * (stream)          */
/* so that information appears on the destination file or terminal   */
/* as soon as it is written.                                         */
/* Normally stderr is line unbuffered.                               */
/*********************************************************************/
void SetNotBufferedFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	returnValue->lexemeValue = CreateBoolean(theEnv, GenSetBuffered(theEnv, context, returnValue, _IONBF, "not"));
}

/***********************************************************************/
/* SetLineBufferedFunction: H/L access function for set-line-buffered. */
/* Sets the buffer type to line buffered for a FILE * (stream)         */
/* so that information is buffered until a newline is output           */
/* or input is read from any stream attached to a terminal device.     */
/* Normally stdout is line buffered.                                   */
/***********************************************************************/
void SetLineBufferedFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	returnValue->lexemeValue = CreateBoolean(theEnv, GenSetBuffered(theEnv, context, returnValue, _IOLBF, "line"));
}

/*************************************************************************/
/* SetFullyBufferedFunction: H/L access function for set-fully-buffered. */
/* Sets the buffer type to fully buffered for a FILE * (stream)          */
/* so that information is buffered as a block.                           */
/* Normally all files are block buffered.                                */
/*************************************************************************/
void SetFullyBufferedFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	returnValue->lexemeValue = CreateBoolean(theEnv, GenSetBuffered(theEnv, context, returnValue, _IOFBF, "fully"));
}

/*****************************************************************/
/* ChangeStatusFlags: Adds the given flags to the file status    */
/*   flags of a socket, or removes them. The two UDFs below use  */
/*   this function.                                              */
/*****************************************************************/
static void ChangeStatusFlags(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue,
		bool adding,
		const char *func)
{
	UDFValue theArg;
	int sockfd, flags, flag;

	if (-1 == (sockfd = GetFilenoFromArgument(theEnv,context,&theArg)))
	{
		WriteString(theEnv,STDERR,func);
		WriteString(theEnv,STDERR,": could not find router for socket file descriptor\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	// The code reads the flags before it changes them. As a result, the value
	// below is the current set of flags of this socket, with the requested
	// flags added or removed. It is not the requested flags alone. A failure
	// here gives -1. Each bit of -1 is set, and the code would write that
	// value back.
	flags = GenFcntl(theEnv, sockfd, F_GETFL, 0);
	if (flags == -1)
	{
		WriteString(theEnv,STDERR,func);
		WriteString(theEnv,STDERR,": could not read the current flags of socket ");
		WriteInteger(theEnv,STDERR,sockfd);
		WriteString(theEnv,STDERR,"\n");
		perror("fcntl");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	while (UDFHasNextArgument(context))
	{
		if (! LookupSocketOptionName(theEnv,context,&theArg,FileStatusFlags,
				adding ? "Flag for fcntl-add-status-flags"
				       : "Flag for fcntl-remove-status-flags",
				&flag))
		{
			returnValue->lexemeValue = FalseSymbol(theEnv);
			return;
		}

		if (adding)
		{ flags |= flag; }
		else
		{ flags &= ~flag; }
	}

	if (GenFcntl(theEnv, sockfd, F_SETFL, flags) == -1)
	{
		WriteString(theEnv,STDERR,func);
		WriteString(theEnv,STDERR,": could not set the flags of socket ");
		WriteInteger(theEnv,STDERR,sockfd);
		WriteString(theEnv,STDERR,"\n");
		perror("fcntl");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	returnValue->lexemeValue = TrueSymbol(theEnv);
}

/*******************************************************************/
/* FcntlAddStatusFlagsFunction: The H/L access function that adds  */
/*   a flag to the file descriptor of a socket, with fcntl.        */
/*******************************************************************/
void FcntlAddStatusFlagsFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	ChangeStatusFlags(theEnv,context,returnValue,true,"add-status-flags");
}

/********************************************************/
/* FcntlRemoveStatusFlagsFunction: H/L access routine   */
/*   for removing status flags                          */
/*   from a socket file descriptor using fcntl          */
/********************************************************/
void FcntlRemoveStatusFlagsFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	ChangeStatusFlags(theEnv,context,returnValue,false,"remove-status-flags");
}

/*********************************/
/* ResolveDomainNameFunction:    */
/* Resolve a domain name         */
/* to a multifield of addresses. */
/*********************************/
void ResolveDomainNameFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
    UDFValue theArg;
    UDFNextArgument(context,LEXEME_BITS,&theArg);
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // Allow IPv4 or IPv6
    hints.ai_flags = AI_ALL; // AI_V4MAPPED | AI_ADDRCONFIG;

    int ret = getaddrinfo(theArg.lexemeValue->contents, NULL, &hints, &result);
    if (ret != 0) {
	WriteString(theEnv,STDERR,"Could not resolve domain name '");
	WriteString(theEnv,STDERR,theArg.lexemeValue->contents);
	WriteString(theEnv,STDERR,"': ");
	WriteString(theEnv,STDERR,gai_strerror(ret));
	WriteString(theEnv,STDERR,".\n");
	returnValue->lexemeValue = FalseSymbol(theEnv);
	return;
    }

    struct addrinfo *res = result;
    MultifieldBuilder *mb = CreateMultifieldBuilder(theEnv, 0);
    while (res) {
	    char host[NI_MAXHOST];
	    getnameinfo(res->ai_addr, res->ai_addrlen, host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
	    MBAppendSymbol(mb, host);
	    res = res->ai_next;
    }

    freeaddrinfo(result);
    returnValue->multifieldValue = MBCreate(mb);
    MBDispose(mb);
}

/************************************************/
/* CloseAllSockets: Close all sockets           */
/*    currently registered as socketio routers. */
/************************************************/
void CloseAllSockets(Environment *theEnv)
{
	struct socketRouter *sptr, *prev;

	if (SocketRouterData(theEnv)->ListOfSocketRouters == NULL) return;

	sptr = SocketRouterData(theEnv)->ListOfSocketRouters;

	while (sptr != NULL)
	{
		if (SocketIsTLS(sptr)) TLSCloseSession(theEnv,sptr);
		DisposeStream(theEnv,sptr);
		prev = sptr;
		// Unnamed sockets (created but never bound/connected/accepted) have
		// no logical name allocation to release.
		if (sptr->logicalName != NULL)
		{ rm(theEnv,(void *) sptr->logicalName,strlen(sptr->logicalName) + 1); }
		sptr = sptr->next;
		rm(theEnv,prev,sizeof(struct socketRouter));
	}

	SocketRouterData(theEnv)->ListOfSocketRouters = NULL;
}

/************************************************/
/* RecvfromFunction: recvfrom on a socket       */
/* Returns a multifield:                        */
/*   (address:port family bytes buffer peerlen) */
/* Optional args (in this order):               */
/*   flags (multifield of symbols OR integer)   */
/*   maxlen (integer)                           */
/************************************************/
void RecvfromFunction(
                Environment *theEnv,
                UDFContext *context,
                UDFValue *returnValue)
{
        struct socketRouter *sptr = NULL;
        UDFValue theArg;
        struct sockaddr_storage peer;
        socklen_t peer_len = sizeof(peer);
        ssize_t nread;
        int fd;
        long maxlen = 65535;
        char buf[65536 + 1];
        int flags = 0;

        if (NULL == (sptr = GetSocketRouterFromArgument(theEnv, context, &theArg)))
        {
                WriteString(theEnv,STDERR,"recvfrom: argument was not recognized as a socket file descriptor\n");
                returnValue->lexemeValue = FalseSymbol(theEnv);
                return;
        }

        if (UDFHasNextArgument(context))
        {
                UDFNextArgument(context, INTEGER_BIT|LEXEME_BITS|MULTIFIELD_BIT, &theArg);
                flags = CollectFlags(&theArg, ReceiveFlags);
        }

        if (UDFHasNextArgument(context))
        {
                UDFNextArgument(context, INTEGER_BIT, &theArg);
                if (theArg.integerValue->contents > 0 && theArg.integerValue->contents <= 65536)
                        maxlen = (long) theArg.integerValue->contents;
        }

        fd = sptr->fd;
        memset(&peer, 0, sizeof(peer));
        nread = recvfrom(fd, buf, (size_t)maxlen, flags, (struct sockaddr *)&peer, &peer_len);
        if (nread < 0)
        {
                WriteString(theEnv,STDERR,"recvfrom failed on '");
                WriteString(theEnv,STDERR,sptr->logicalName != NULL ? sptr->logicalName : "(unnamed socket)");
                WriteString(theEnv,STDERR,"'\n");
                perror("perror");
                returnValue->lexemeValue = FalseSymbol(theEnv);
                return;
        }

        MultifieldBuilder *mb = CreateMultifieldBuilder(theEnv, 6L);

        {
                const char *family = LookupOptionValue(SocketDomains, peer.ss_family);
                char text[SOCKET_ADDRESS_TEXT];
                long long port;

                if (family == NULL)
                {
                        MBAppendSymbol(mb, "AF_UNSPEC");
                }
                else
                {
                        MBAppendSymbol(mb, family);

                        if (AddressText(peer.ss_family, &peer, peer_len, text, sizeof(text), &port))
                        {
                                MBAppendSymbol(mb, text);

                                // A unix path is the full address. There is no
                                // port to report with it.
                                if (peer.ss_family != AF_UNIX) MBAppendInteger(mb, port);
                        }
                }
        }

        if (nread >= (ssize_t)sizeof(buf)) nread = (ssize_t)sizeof(buf) - 1;
        if (nread < 0) nread = 0;
        buf[nread] = '\0';

        MBAppendInteger(mb, (long long)nread);
        MBAppendString(mb, buf);

        returnValue->multifieldValue = MBCreate(mb);

	MBDispose(mb);
}

/****************************************************************/
/* SendtoFunction: sendto on a socket                           */
/* Usage:                                                       */
/*   (sendto <socket> AF_UNIX  <path>           <data> [flags]) */
/*   (sendto <socket> AF_INET  <ip> <port-int>  <data> [flags]) */
/*   (sendto <socket> AF_INET6 <ip> <port-int>  <data> [flags]) */
/* Returns: bytes sent (integer) or FALSE on error              */
/****************************************************************/
void SendtoFunction(
                Environment *theEnv,
                UDFContext *context,
                UDFValue *returnValue)
{
        struct socketRouter *sptr = NULL;
        UDFValue theArg;
        int fd, flags = 0, domain;
        const char *family = NULL;
        const char *address = NULL;
        const char *data = NULL;
        size_t data_len = 0;
        long long port = 0;
        bool havePort = false;
        struct sockaddr_storage dst;
        socklen_t dst_len = 0;

        if (NULL == (sptr = GetSocketRouterFromArgument(theEnv, context, &theArg)))
        {
                WriteString(theEnv,STDERR,"sendto: argument was not recognized as a socket file descriptor\n");
                returnValue->lexemeValue = FalseSymbol(theEnv);
                return;
        }

        /* The family gives the arguments after it, and the code reads the
           family first. The code also compares the family with the domain of
           the socket. A difference between the two means that the socket
           cannot send to the sockaddr below. The EINVAL of the kernel does not
           say which of the two values is incorrect. */
        if (! UDFHasNextArgument(context)) { returnValue->lexemeValue = FalseSymbol(theEnv); return; }
        UDFNextArgument(context, LEXEME_BITS, &theArg);
        family = theArg.lexemeValue->contents;

        if (! LookupOptionName(SocketDomains, family, &domain))
        {
                WriteString(theEnv,STDERR,"sendto: unsupported family (use AF_UNIX | AF_INET | AF_INET6)\n");
                returnValue->lexemeValue = FalseSymbol(theEnv);
                return;
        }

        if (domain != sptr->domain)
        {
                const char *itsOwn = LookupOptionValue(SocketDomains, sptr->domain);

                WriteString(theEnv,STDERR,"sendto: the socket was created as ");
                WriteString(theEnv,STDERR,itsOwn != NULL ? itsOwn : "another domain");
                WriteString(theEnv,STDERR,", not ");
                WriteString(theEnv,STDERR,family);
                WriteString(theEnv,STDERR,"\n");
                returnValue->lexemeValue = FalseSymbol(theEnv);
                return;
        }

        /* The destination: an address, and then a port for the families with a
           port. */
        if (! UDFHasNextArgument(context)) { returnValue->lexemeValue = FalseSymbol(theEnv); return; }
        UDFNextArgument(context, LEXEME_BITS, &theArg);
        address = theArg.lexemeValue->contents;

        if (domain != AF_UNIX)
        {
                if (! UDFHasNextArgument(context)) { returnValue->lexemeValue = FalseSymbol(theEnv); return; }
                UDFNextArgument(context, INTEGER_BIT, &theArg);
                port = theArg.integerValue->contents;
                havePort = true;
        }

        /* The data to send, as a string or a symbol. */
        if (! UDFHasNextArgument(context)) { returnValue->lexemeValue = FalseSymbol(theEnv); return; }
        UDFNextArgument(context, LEXEME_BITS, &theArg);
        data = theArg.lexemeValue->contents;
        data_len = strlen(data);

        /* This is the same address code that bind-socket and connect use. As a
           result, this function refuses an incorrect address under the same
           rules. It also refuses a unix path that is too long, and it does not
           cut that path into a path that names a different socket. */
        if (! FillSocketAddress(theEnv, "sendto", domain, address, havePort, port,
                        &dst, &dst_len))
        {
                returnValue->lexemeValue = FalseSymbol(theEnv);
                return;
        }

        /* Optional flags (multifield of symbols, single symbol, or integer) */
        if (UDFHasNextArgument(context))
        {
                UDFNextArgument(context, INTEGER_BIT|LEXEME_BITS|MULTIFIELD_BIT, &theArg);
                flags = CollectFlags(&theArg, SendFlags);
        }

        fd = sptr->fd;

        ssize_t nsent = sendto(fd, data, data_len, flags, (struct sockaddr *)&dst, dst_len);
        if (nsent < 0)
        {
                WriteString(theEnv,STDERR,"sendto failed on '");
                WriteString(theEnv,STDERR,sptr->logicalName != NULL ? sptr->logicalName : "(unnamed socket)");
                WriteString(theEnv,STDERR,"'\n");
                perror("perror");
                returnValue->lexemeValue = FalseSymbol(theEnv);
                return;
        }

        returnValue->integerValue = CreateInteger(theEnv, (long long)nsent);
}
