/*******************************************************/
/*      "C" Language Integrated Production System      */
/*                                                     */
/*            CLIPS Version ?.??  05/07/24             */
/*                                                     */
/*               SOCKET I/O ROUTER MODULE              */
/*******************************************************/

/***********************************************************************/
/* Purpose: I/O Router routines which allow network sockets to be used */
/*   as input and output sources.                                      */
/*								       */
/* Principal Programmer(s):                                            */
/*      Ryan P. Johnston                                               */   
/*								       */
/* Revision History:                                                   */
/*								       */
/*      ?.??: Added this file.                                         */
/*                                                                     */
/**********************************************************************/

#define _POSIX_C_SOURCE 1

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
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

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static void                    WriteSocket(Environment *, const char *, const char *, void *);
static int                     ReadSocket(Environment *, const char *, void *);
static int                     UnreadSocket(Environment *, const char *, int, void *);
static void                    ExitSocket(Environment *, int, void *);
static void                    DeallocateSocketRouterData(Environment *);

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

/*******************************************/
/* FindSptr: Returns a pointer to a socket */
/*   stream for a given logical name.      */
/*******************************************/
FILE *FindSptr(
		Environment *theEnv,
		const char *logicalName)
{
	struct socketRouter *sptr;

	sptr = LogicalNameToSocketRouter(theEnv, logicalName);

	if (sptr != NULL) return sptr->stream;

	return NULL;
}

/*************************************************************************/
/* FindSocket high level function for Router Query Callback.             */
/*************************************************************************/
bool FindSocket(
		Environment *theEnv,
		const char *logicalName,
		void *context)
{
	return LogicalNameToSocketRouter(theEnv, logicalName) != NULL;
}

/*****************************************************************/
/* WriteSocket: Write callback for socket router. */
/*****************************************************************/
static void WriteSocket(
		Environment *theEnv,
		const char *logicalName,
		const char *str,
		void *context)
{
	FILE *sptr;

	sptr = FindSptr(theEnv,logicalName);

	genprintfile(theEnv,sptr,str);
}

/***************************************************************/
/* ReadSocket: Read callback for socket router. */
/***************************************************************/
static int ReadSocket(
		Environment *theEnv,
		const char *logicalName,
		void *context)
{
	FILE *sptr;
	int theChar;

	sptr = FindSptr(theEnv,logicalName);

	theChar = getc(sptr);

	return theChar;
}

/*******************************************************************/
/* UnreadSocket: Unread callback for socket router. */
/*******************************************************************/
static int UnreadSocket(
		Environment *theEnv,
		const char *logicalName,
		int ch,
		void *context)
{
	FILE *sptr;

	sptr = FindSptr(theEnv,logicalName);

	return ungetc(ch,sptr);
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
	while ((sptr != NULL) ? 0 != strcmp(logicalName, sptr->logicalName) : false)
	{ sptr = sptr->next; }

	if (sptr != NULL) return sptr;

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
	while ((sptr != NULL) ? (fileno(sptr->stream) != sockfd) : false)
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
	FILE *stream;
	int sockfd = -1;
	UDFNextArgument(context,INTEGER_BIT|LEXEME_BITS,theArg);
	if (theArg->header->type == INTEGER_TYPE)
	{
		sockfd = theArg->integerValue->contents;
	}
	else if (theArg->header->type == STRING_TYPE || theArg->header->type == SYMBOL_TYPE)
	{
		stream = FindSptr(theEnv, theArg->lexemeValue->contents);
		if (stream != NULL)
		{
			sockfd = fileno(stream);
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

FILE *GetBoundOrConnectedFilenoFromArgument(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *theArg)
{
	struct socketRouter *sptr = NULL;

	if (NULL == (sptr = GetSocketRouterFromArgument(theEnv, context, theArg)))
	{
		return NULL;
	}

	return sptr->stream;
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
	/*====================*/
	/* Get the sockfd.    */
	/*====================*/
	if (-1 == (sockfd = GetFilenoFromArgument(theEnv,context,&theArg)))
	{
		WriteString(theEnv,STDERR,"remove-status-flags: could not find router for socket file descriptor\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}
	/*====================*/
	/* Get the level. */
	/*====================*/
	UDFNextArgument(context,SYMBOL_BIT,&theArg);
	const char *UDFlevel = theArg.lexemeValue->contents;
	if (0 == strcmp(UDFlevel,"SOL_SOCKET"))
	{
		level = SOL_SOCKET;
	}
	else if (0 == strcmp(UDFlevel,"IPPROTO_TCP"))
	{
		level = IPPROTO_TCP;
	}
	else
	{
		WriteString(theEnv,STDERR,"Level '");
		WriteString(theEnv,STDERR,theArg.lexemeValue->contents);
		WriteString(theEnv,STDERR,"' not supported.\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}
	/*====================*/
	/* Get the optname. */
	/*====================*/
	UDFNextArgument(context,SYMBOL_BIT,&theArg);
	const char *UDFoptname = theArg.lexemeValue->contents;
	if (0 == strcmp(UDFoptname,"SO_REUSEADDR"))
	{
		optname = SO_REUSEADDR;
	}
	
	else if (0 == strcmp(UDFoptname,"TCP_NODELAY"))
	{
		optname = TCP_NODELAY;
	}
	else
	{
		WriteString(theEnv,STDERR,"optname '");
		WriteString(theEnv,STDERR,theArg.lexemeValue->contents);
		WriteString(theEnv,STDERR,"' not supported.\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	flag = -1;
	socklen_t flag_len = sizeof(flag);
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
	/*====================*/
	/* Get the sockfd.    */
	/*====================*/
	if (-1 == (sockfd = GetFilenoFromArgument(theEnv,context,&theArg)))
	{
		WriteString(theEnv,STDERR,"remove-status-flags: could not find router for socket file descriptor\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}
	/*====================*/
	/* Get the level.     */
	/*====================*/
	UDFNextArgument(context,SYMBOL_BIT,&theArg);
	const char *UDFlevel = theArg.lexemeValue->contents;
	if (0 == strcmp(UDFlevel,"SOL_SOCKET"))
	{
		level = SOL_SOCKET;
	}
	else if (0 == strcmp(UDFlevel,"IPPROTO_TCP"))
	{
		level = IPPROTO_TCP;
	}
	else
	{
		WriteString(theEnv,STDERR,"Level '");
		WriteString(theEnv,STDERR,theArg.lexemeValue->contents);
		WriteString(theEnv,STDERR,"' not supported.\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}
	/*====================*/
	/* Get the optname. */
	/*====================*/
	UDFNextArgument(context,SYMBOL_BIT,&theArg);
	const char *UDFoptname = theArg.lexemeValue->contents;
	if (0 == strcmp(UDFoptname,"SO_REUSEADDR"))
	{
		optname = SO_REUSEADDR;
	}
	else if (0 == strcmp(UDFoptname,"TCP_NODELAY"))
	{
		optname = TCP_NODELAY;
	}
	else
	{
		WriteString(theEnv,STDERR,"optname '");
		WriteString(theEnv,STDERR,theArg.lexemeValue->contents);
		WriteString(theEnv,STDERR,"' not supported.\n");
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

	UDFNextArgument(context,SYMBOL_BIT,&theArg);

	if (0 == strcmp(theArg.lexemeValue->contents, "AF_INET"))
	{
		domain = AF_INET;
	}
	else if (0 == strcmp(theArg.lexemeValue->contents, "AF_INET6"))
	{
		domain = AF_INET6;
	}
	else if (0 == strcmp(theArg.lexemeValue->contents, "AF_UNIX"))
	{
		domain = AF_UNIX;
	}
	else
	{
		WriteString(theEnv,STDERR,"Domain '");
		WriteString(theEnv,STDERR,theArg.lexemeValue->contents);
		WriteString(theEnv,STDERR,"' not supported.\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	/*====================*/
	/* Get the type.      */
	/*====================*/
	UDFNextArgument(context,SYMBOL_BIT,&theArg);

	if (0 == strcmp(theArg.lexemeValue->contents, "SOCK_STREAM"))
	{
		type = SOCK_STREAM;
	}
	else if (0 == strcmp(theArg.lexemeValue->contents, "SOCK_DGRAM"))
	{
		type = SOCK_DGRAM;
	}
	else
	{
		WriteString(theEnv,STDERR,"Type '");
		WriteString(theEnv,STDERR,theArg.lexemeValue->contents);
		WriteString(theEnv,STDERR,"' not supported.\n");
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
	newRouter->stream = fdopen(sock, "r+");

	/*=========================================*/
	/* Wrap the opened socket in a FILE        */
	/* with fdopen.                            */
	/*=========================================*/
	if (NULL == (newRouter->stream))
	{
		WriteString(theEnv,STDERR,"Could not fdopen ");
		WriteString(theEnv,STDERR,theArg.lexemeValue->contents);
		WriteString(theEnv,STDERR,"\n");
		perror("perror");
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
/* FlushConnection: Flushed the connection associated with the specified      */
/*   connection logical name. Simply wraps GenFlush and discards return value */
/*   for now.                                                                 */
/******************************************************************************/
bool FlushConnection(
		Environment *theEnv,
		FILE *stream)
{
	GenFlush(theEnv,stream);
	return true;
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
	FILE *socketStream;

	if (NULL == (socketStream = GetBoundOrConnectedFilenoFromArgument(theEnv,context,&theArg)))
	{
		WriteString(theEnv,STDERR,"flush-connection: Could not find socket with that logical name\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	returnValue->lexemeValue = CreateBoolean(theEnv,FlushConnection(theEnv,socketStream));
}

bool EmptyConnection(
		Environment *theEnv,
		FILE *stream)
{
	int ch;

	while ((ch = fgetc(stream)) != EOF);

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
	FILE *socketStream;

	if (NULL == (socketStream = GetBoundOrConnectedFilenoFromArgument(theEnv,context,&theArg)))
	{
		WriteString(theEnv,STDERR,"empty-connection: Could not find socket; are you sure it's accepted or connected?\n");
		return;
	}

	returnValue->lexemeValue = CreateBoolean(theEnv,EmptyConnection(theEnv,socketStream));
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
			sptr = sptr->next)
	{
		if (fileno(sptr->stream) == socketfd)
		{
			GenClose(theEnv,sptr->stream);
			rm(theEnv,(void *) sptr->logicalName,strlen(sptr->logicalName) + 1);

			if (prev == NULL)
			{ SocketRouterData(theEnv)->ListOfSocketRouters = sptr->next; }
			else
			{ prev->next = sptr->next; }
			rm(theEnv,sptr,sizeof(struct socketRouter));

			return true;
		}

		prev = sptr;
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
			sptr = sptr->next)
	{
		if (strcmp(sptr->logicalName,logicalName) == 0)
		{
			GenClose(theEnv,sptr->stream);
			rm(theEnv,(void *) sptr->logicalName,strlen(sptr->logicalName) + 1);
			if (prev == NULL)
			{ SocketRouterData(theEnv)->ListOfSocketRouters = sptr->next; }
			else
			{ prev->next = sptr->next; }
			rm(theEnv,sptr,sizeof(struct socketRouter));

			return true;
		}

		prev = sptr;
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
	else if (theArg.header->type == STRING_TYPE || theArg.header->type == SYMBOL_TYPE)
	{
		returnValue->lexemeValue = CreateBoolean(theEnv, CloseNamedConnection(theEnv, theArg.lexemeValue->contents));
	}
	else
	{
		returnValue->lexemeValue = FalseSymbol(theEnv);
	}
}

/***************************************************/
/* ShutdownConnectionFunction: H/L access function */
/*    for running shutown on all bound/connected   */
/*    sockets associated with an I/O Router.       */
/*    Let's you specify that no more data can be   */
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
		WriteString(theEnv,STDERR,"get-timeout: could not find router for socket file descriptor\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	how = SHUT_RDWR;
	if (UDFHasNextArgument(context))
	{
		UDFNextArgument(context,SYMBOL_BIT,&theArg);

		if (0 == strcmp(theArg.lexemeValue->contents, "SHUT_RD"))
		{
			how = SHUT_RD;
		}
		else if (0 == strcmp(theArg.lexemeValue->contents, "SHUT_WR"))
		{
			how = SHUT_WR;
		}
		else if (0 == strcmp(theArg.lexemeValue->contents, "SHUT_RDWR"))
		{
			how = SHUT_RDWR;
		}
	}
	returnValue->lexemeValue = CreateBoolean(theEnv,GenShutdown(theEnv,sockfd,how));
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
		WriteString(theEnv,STDERR,"get-timeout: could not find router for socket file descriptor\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	UDFNextArgument(context,INTEGER_BIT,&theArg);
	timeout = theArg.integerValue->contents;

	// by default, poll for all flags
	if (!UDFHasNextArgument(context))
	{
		returnValue->lexemeValue =
			CreateBoolean(theEnv,GenPoll(theEnv,sockfd,timeout,POLLIN | POLLOUT | POLLERR | POLLHUP | POLLNVAL | POLLPRI));
		return;
	}

	int flags = 0;
	while (UDFHasNextArgument(context))
	{
		UDFNextArgument(context,SYMBOL_BIT,&theArg);
		if (0 == strcmp(theArg.lexemeValue->contents, "POLLIN"))
		{
			flags |= POLLIN;
		}
		else if (0 == strcmp(theArg.lexemeValue->contents, "POLLOUT"))
		{
			flags |= POLLOUT;
		}
		else if (0 == strcmp(theArg.lexemeValue->contents, "POLLERR"))
		{
			flags |= POLLERR;
		}
		else if (0 == strcmp(theArg.lexemeValue->contents, "POLLHUP"))
		{
			flags |= POLLHUP;
		}
		else if (0 == strcmp(theArg.lexemeValue->contents, "POLLNVAL"))
		{
			flags |= POLLNVAL;
		}
		else if (0 == strcmp(theArg.lexemeValue->contents, "POLLPRI"))
		{
			flags |= POLLPRI;
		}
		else
		{
			WriteString(theEnv,STDERR,"Unsupported flag for poll ");
			WriteString(theEnv,STDERR,theArg.lexemeValue->contents);
			WriteString(theEnv,STDERR,"\n");
			perror("perror");
			returnValue->lexemeValue = FalseSymbol(theEnv);
			return;
		}

	}
	returnValue->lexemeValue = CreateBoolean(theEnv,GenPoll(theEnv,sockfd,timeout,flags));
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
	UDFValue theArg, optionalArg;
	StringBuilder *logicalNameStringBuilder = CreateStringBuilder(theEnv, 0);
	size_t addr_len;
	char *theName;

	UDFNextArgument(context,INTEGER_BIT,&theArg);
	sptr = FileDescriptorToSocketRouter(theEnv, theArg.integerValue->contents);

	// address
	UDFNextArgument(context,LEXEME_BITS,&theArg);
	// port
	if (UDFHasNextArgument(context))
	{
		UDFNextArgument(context,INTEGER_BIT,&optionalArg);
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	switch (sptr->domain)
	{
		case AF_INET:
			struct sockaddr_in *addr = (struct sockaddr_in *)&serv_addr;
			addr->sin_family = sptr->domain;
			addr->sin_addr.s_addr = inet_addr(theArg.lexemeValue->contents);
			addr->sin_port = htons(optionalArg.integerValue->contents);
			SBAppend(logicalNameStringBuilder, theArg.lexemeValue->contents);
			SBAddChar(logicalNameStringBuilder, ':');
			SBAppendInteger(logicalNameStringBuilder, optionalArg.integerValue->contents);
			addr_len = sizeof(*addr);
			break;
		case AF_INET6:
			struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&serv_addr;
			addr6->sin6_family = sptr->domain;
			inet_pton(AF_INET6, theArg.lexemeValue->contents, &(addr6->sin6_addr));
			addr6->sin6_port = htons(optionalArg.integerValue->contents);
			SBAddChar(logicalNameStringBuilder, '[');
			SBAppend(logicalNameStringBuilder, theArg.lexemeValue->contents);
			SBAddChar(logicalNameStringBuilder, ']');
			SBAddChar(logicalNameStringBuilder, ':');
			SBAppendInteger(logicalNameStringBuilder, optionalArg.integerValue->contents);
			addr_len = sizeof(*addr6);
			break;
		case AF_UNIX:
			struct sockaddr_un *addrun = (struct sockaddr_un *)&serv_addr;
			addrun->sun_family = sptr->domain;
			strncpy(addrun->sun_path, theArg.lexemeValue->contents, sizeof(addrun->sun_path) - 1);
			addrun->sun_path[sizeof(addrun->sun_path) - 1] = '\0';
			SBAppend(logicalNameStringBuilder, theArg.lexemeValue->contents);
			addr_len = offsetof(struct sockaddr_un, sun_path) + strlen(addrun->sun_path);
			unlink(theArg.lexemeValue->contents);
			break;
		case AF_UNSPEC:
		default:
			WriteString(theEnv,STDERR,"Could not bind '");
			WriteString(theEnv,STDERR,theArg.lexemeValue->contents);
			WriteString(theEnv,STDERR,"': socket domain not supported.\n");
			returnValue->lexemeValue = FalseSymbol(theEnv);
			SBDispose(logicalNameStringBuilder);
			return;
	}

	/*====================================*/
	/* Bind the socket with the address.  */
	/*====================================*/

	if (bind(fileno(sptr->stream), (struct sockaddr *)&serv_addr, addr_len) < 0)
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
	FILE *socketStream;
	int sockfd, backlog;
	UDFValue theArg;

	if (NULL == (socketStream = GetBoundOrConnectedFilenoFromArgument(theEnv,context,&theArg)))
	{
		WriteString(theEnv,STDERR,"listen: Could not find bound socket; are you sure it's bound?\n");
		return;
	}

	sockfd = fileno(socketStream);

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
	sptr = FileDescriptorToSocketRouter(theEnv, sockfd);
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
		return;
	}

	logicalNameStringBuilder = CreateStringBuilder(theEnv, 0);

	/*====================================*/
	/* Accept a connection on the socket.  */
	/*====================================*/
	if ((connection_fd = accept(fileno(sptr->stream), (struct sockaddr *)&client_addr, &client_addr_len)) < 0)
	{
		WriteString(theEnv,STDERR,"Could not accept connection on socket '");
		WriteString(theEnv,STDERR,sptr->logicalName);
		WriteString(theEnv,STDERR,"'\n");
		perror("perror");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	/*========================================*/
	/* Build logical name for accepted client */
	/*========================================*/

	switch (sptr->domain)
	{
		case AF_INET:
			struct sockaddr_in *addr = (struct sockaddr_in *)&client_addr;
			char client_ip[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &(addr->sin_addr), client_ip, INET_ADDRSTRLEN);
			int client_port = ntohs(addr->sin_port);

			SBAppend(logicalNameStringBuilder, client_ip);
			SBAddChar(logicalNameStringBuilder, ':');
			SBAppendInteger(logicalNameStringBuilder, client_port);
			break;
		case AF_INET6:
			struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&client_addr;
			char client_ip6[INET6_ADDRSTRLEN];
			inet_ntop(AF_INET6, &(addr6->sin6_addr), client_ip6, INET6_ADDRSTRLEN);
			int client_port6 = ntohs(addr6->sin6_port);

			SBAddChar(logicalNameStringBuilder, '[');
			SBAppend(logicalNameStringBuilder, client_ip6);
			SBAddChar(logicalNameStringBuilder, ':');
			SBAppendInteger(logicalNameStringBuilder, client_port6);
			SBAddChar(logicalNameStringBuilder, ']');
			break;
		case AF_UNIX:
			struct sockaddr_un *addrun = (struct sockaddr_un *)&client_addr;
			char *socket_path = addrun->sun_path;
			SBAppend(logicalNameStringBuilder, socket_path);
			break;
		case AF_UNSPEC:
		default:
			WriteString(theEnv,STDERR,"Could not accept; socket domain '");
			WriteInteger(theEnv,STDERR,sptr->domain);
			WriteString(theEnv,STDERR,"' not supported.\n");
			SBDispose(logicalNameStringBuilder);
			returnValue->lexemeValue = FalseSymbol(theEnv);
			return;
	}

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

	int sockfd;

	if (-1 == (sockfd = GetFilenoFromArgument(theEnv,context,&theArg)))
	{
		WriteString(theEnv,STDERR,"set-timeout: could not find router for socket file descriptor\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	UDFNextArgument(context,INTEGER_BIT,&theArg);
	tv.tv_sec = 0;
	tv.tv_usec = theArg.integerValue->contents;
	returnValue->integerValue = CreateInteger(theEnv, GenSetsockopt(
				theEnv,
				sockfd,
				SOL_SOCKET,
				SO_RCVTIMEO,
				(const void *)&tv,
				sizeof(tv)
				));
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
	UDFValue theArg, optionalArg;
	char *theName;

	logicalNameStringBuilder = CreateStringBuilder(theEnv, 0);

	/*********************/
	/* get socket fd     */
	/*********************/
	UDFNextArgument(context,INTEGER_BIT,&theArg);
	if (NULL == (sptr = FileDescriptorToSocketRouter(theEnv, theArg.integerValue->contents)))
	{
		WriteString(theEnv,STDERR,"accept: argument was not recognized as a socket file descriptor\n");
		return;
	}

	addr_len = sizeof(serv_addr);

	// address
	UDFNextArgument(context,LEXEME_BITS,&theArg);
	// port
	if (UDFHasNextArgument(context))
	{
		UDFNextArgument(context,INTEGER_BIT,&optionalArg);
	}
	memset(&serv_addr, 0, sizeof(serv_addr));
	switch (sptr->domain)
	{
		case AF_INET:
			struct sockaddr_in *addr = (struct sockaddr_in *)&serv_addr;
			addr->sin_family = sptr->domain;
			addr->sin_addr.s_addr = inet_addr(theArg.lexemeValue->contents);
			addr->sin_port = htons(optionalArg.integerValue->contents);
			SBAppend(logicalNameStringBuilder, theArg.lexemeValue->contents);
			SBAddChar(logicalNameStringBuilder, ':');
			SBAppendInteger(logicalNameStringBuilder, optionalArg.integerValue->contents);
			addr_len = sizeof(*addr);
			break;
		case AF_INET6:
			struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&serv_addr;
			addr6->sin6_family = sptr->domain;
			inet_pton(AF_INET6, theArg.lexemeValue->contents, &(addr6->sin6_addr));
			addr6->sin6_port = htons(optionalArg.integerValue->contents);
			SBAddChar(logicalNameStringBuilder, '[');
			SBAppend(logicalNameStringBuilder, theArg.lexemeValue->contents);
			SBAddChar(logicalNameStringBuilder, ']');
			SBAddChar(logicalNameStringBuilder, ':');
			SBAppendInteger(logicalNameStringBuilder, optionalArg.integerValue->contents);
			addr_len = sizeof(*addr6);
			break;
		case AF_UNIX:
			struct sockaddr_un *addrun = (struct sockaddr_un *)&serv_addr;
			addrun->sun_family = sptr->domain;
			strncpy(addrun->sun_path, theArg.lexemeValue->contents, sizeof(addrun->sun_path) - 1);
			addrun->sun_path[sizeof(addrun->sun_path) - 1] = '\0';
			SBAppend(logicalNameStringBuilder, theArg.lexemeValue->contents);
			addr_len = offsetof(struct sockaddr_un, sun_path) + strlen(addrun->sun_path);
			break;
		case AF_UNSPEC:
		default:
			WriteString(theEnv,STDERR,"Could not connect; socket domain not supported'");
			SBDispose(logicalNameStringBuilder);
			returnValue->lexemeValue = FalseSymbol(theEnv);
			return;
	}

	if (0 > connect(fileno(sptr->stream), (struct sockaddr*)&serv_addr, addr_len))
	{
		WriteString(theEnv,STDERR,"Could not connect to '");
		WriteString(theEnv,STDERR,logicalNameStringBuilder->contents);
		WriteString(theEnv,STDERR,"'\n");
		perror("perror");
		SBDispose(logicalNameStringBuilder);
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

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
	FILE *socketStream;

	if (NULL == (socketStream = GetBoundOrConnectedFilenoFromArgument(theEnv,context,&theArg)))
	{
		WriteString(theEnv,STDERR,"set-");
		WriteString(theEnv,STDERR,func);
		WriteString(theEnv,STDERR,"-buffered: Could not find bound socket; are you sure it's bound?\n");
		return false;
	}

	if (0 > GenSetvbuf(theEnv, socketStream, NULL, mode, 0))
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
	GenSetBuffered(theEnv, context, returnValue, _IONBF, "not");
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
	GenSetBuffered(theEnv, context, returnValue, _IOLBF, "line");
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
	GenSetBuffered(theEnv, context, returnValue, _IOFBF, "fully");
}

/*******************************************************************/
/* FcntlAddStatusFlagsFunction: H/L access routine for adding flag */
/*   to a socket file descriptor using fcntl                       */
/*******************************************************************/
void FcntlAddStatusFlagsFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	int sockfd, flags;
	/*====================*/
	/* Get the sockfd.    */
	/*====================*/
	UDFValue theArg;
	if (-1 == (sockfd = GetFilenoFromArgument(theEnv,context,&theArg)))
	{
		WriteString(theEnv,STDERR,"add-status-flags: could not find router for socket file descriptor\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}
	/*====================*/
	/* Get the flags.     */
	/*====================*/
	flags = GenFcntl(theEnv, sockfd, F_GETFL, 0);
	while (UDFHasNextArgument(context))
	{
		UDFNextArgument(context,SYMBOL_BIT,&theArg);
		if (0 == strcmp(theArg.lexemeValue->contents, "O_NONBLOCK"))
		{
			flags |= O_NONBLOCK;
		}
		else if (0 == strcmp(theArg.lexemeValue->contents, "O_APPEND"))
		{
			flags |= O_APPEND;
		}
		else if (0 == strcmp(theArg.lexemeValue->contents, "O_ASYNC"))
		{
			flags |= O_ASYNC;
		}
		else
		{
			WriteString(theEnv,STDERR,"Unsupported flag for poll ");
			WriteString(theEnv,STDERR,theArg.lexemeValue->contents);
			WriteString(theEnv,STDERR,"\n");
			perror("perror");
			returnValue->lexemeValue = FalseSymbol(theEnv);
			return;
		}

	}
	// Add the arg as a flag to this socket
	if (GenFcntl(theEnv, sockfd, F_SETFL, flags) == -1) {
		WriteString(theEnv,STDERR,"Could not set flags for sockfd '");
		WriteInteger(theEnv,STDERR,sockfd);
		WriteString(theEnv,STDERR,"'.");
		perror("fcntl");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}
	returnValue->lexemeValue = TrueSymbol(theEnv);
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
	int sockfd, flags;
	/*====================*/
	/* Get the sockfd.    */
	/*====================*/
	UDFValue theArg;
	if (-1 == (sockfd = GetFilenoFromArgument(theEnv,context,&theArg)))
	{
		WriteString(theEnv,STDERR,"remove-status-flags: could not find router for socket file descriptor\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}
	/*====================*/
	/* Get the flags.     */
	/*====================*/
	flags = GenFcntl(theEnv, sockfd, F_GETFL, 0);
	while (UDFHasNextArgument(context))
	{
		UDFNextArgument(context,SYMBOL_BIT,&theArg);
		if (0 == strcmp(theArg.lexemeValue->contents, "O_NONBLOCK"))
		{
			flags &= ~O_NONBLOCK;
		}
		else if (0 == strcmp(theArg.lexemeValue->contents, "O_APPEND"))
		{
			flags &= ~O_APPEND;
		}
		else if (0 == strcmp(theArg.lexemeValue->contents, "O_ASYNC"))
		{
			flags &= ~O_ASYNC;
		}
		else
		{
			WriteString(theEnv,STDERR,"Unsupported flag for poll ");
			WriteString(theEnv,STDERR,theArg.lexemeValue->contents);
			WriteString(theEnv,STDERR,"\n");
			perror("perror");
			returnValue->lexemeValue = FalseSymbol(theEnv);
			return;
		}

	}
	if (GenFcntl(theEnv, sockfd, F_SETFL, flags) == -1) {
		WriteString(theEnv,STDERR,"Could not set flags for sockfd '");
		WriteInteger(theEnv,STDERR,sockfd);
		WriteString(theEnv,STDERR,"'.");
		perror("fcntl");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}
	returnValue->lexemeValue = TrueSymbol(theEnv);

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
		GenClose(theEnv,sptr->stream);
		prev = sptr;
		rm(theEnv,(void *) sptr->logicalName,strlen(sptr->logicalName) + 1);
		sptr = sptr->next;
		rm(theEnv,prev,sizeof(struct socketRouter));
	}

	SocketRouterData(theEnv)->ListOfSocketRouters = NULL;
}
