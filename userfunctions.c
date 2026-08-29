   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/30/16             */
   /*                                                     */
   /*                USER FUNCTIONS MODULE                */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Created file to seperate UserFunctions and     */
/*            EnvUserFunctions from main.c.                  */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*************************************************************/

/***************************************************************************/
/*                                                                         */
/* Permission is hereby granted, free of charge, to any person obtaining   */
/* a copy of this software and associated documentation files (the         */
/* "Software"), to deal in the Software without restriction, including     */
/* without limitation the rights to use, copy, modify, merge, publish,     */
/* distribute, and/or sell copies of the Software, and to permit persons   */
/* to whom the Software is furnished to do so.                             */
/*                                                                         */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS */
/* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF              */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT   */
/* OF THIRD PARTY RIGHTS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY  */
/* CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES */
/* WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN   */
/* ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF */
/* OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.          */
/*                                                                         */
/***************************************************************************/
#define _POSIX_C_SOURCE 200112L

#define _DEFAULT_SOURCE
#include <dirent.h>
#include <errno.h>

/* libmagic is optional. Build with USE_LIBMAGIC defined (make MAGIC=1) to
   get the (mimetype) function. */
#ifdef USE_LIBMAGIC
#include <magic.h>
#endif

#include <math.h>
#include <signal.h>
#include <time.h>

#include "clips.h"
#include "socketrtr.h"
#include "socktls.h"

void UserFunctions(Environment *);

/* Translates an errno to its symbolic name. With no argument the current
   errno is used; with an integer argument that value is translated instead,
   which lets a saved errno be named later. Unrecognised values, including 0,
   return void. */
#define ERRNO_SYM(code) case code: err = CreateSymbol(theEnv,#code); break

void ErrnoSymFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	CLIPSLexeme *err;
	UDFValue theArg;
	int theError;

	if (UDFHasNextArgument(context))
	{
		UDFNextArgument(context,INTEGER_BIT,&theArg);
		theError = (int) theArg.integerValue->contents;
	}
	else
	{
		theError = errno;
	}

	switch(theError)
	{
		ERRNO_SYM(EPERM);
		ERRNO_SYM(ENOENT);
		ERRNO_SYM(ESRCH);
		ERRNO_SYM(EINTR);
		ERRNO_SYM(EIO);
		ERRNO_SYM(ENXIO);
		ERRNO_SYM(E2BIG);
		ERRNO_SYM(ENOEXEC);
		ERRNO_SYM(EBADF);
		ERRNO_SYM(ECHILD);
		ERRNO_SYM(EAGAIN);
		ERRNO_SYM(ENOMEM);
		ERRNO_SYM(EACCES);
		ERRNO_SYM(EFAULT);
		ERRNO_SYM(ENOTBLK);
		ERRNO_SYM(EBUSY);
		ERRNO_SYM(EEXIST);
		ERRNO_SYM(EXDEV);
		ERRNO_SYM(ENODEV);
		ERRNO_SYM(ENOTDIR);
		ERRNO_SYM(EISDIR);
		ERRNO_SYM(EINVAL);
		ERRNO_SYM(ENFILE);
		ERRNO_SYM(EMFILE);
		ERRNO_SYM(ENOTTY);
		ERRNO_SYM(ETXTBSY);
		ERRNO_SYM(EFBIG);
		ERRNO_SYM(ENOSPC);
		ERRNO_SYM(ESPIPE);
		ERRNO_SYM(EROFS);
		ERRNO_SYM(EMLINK);
		ERRNO_SYM(EPIPE);
		ERRNO_SYM(EDOM);
		ERRNO_SYM(ERANGE);
		ERRNO_SYM(EDEADLK);
		ERRNO_SYM(ENAMETOOLONG);
		ERRNO_SYM(ENOLCK);
		ERRNO_SYM(ENOSYS);
		ERRNO_SYM(ENOTEMPTY);
		ERRNO_SYM(ELOOP);
		ERRNO_SYM(ENOMSG);
		ERRNO_SYM(EIDRM);
		ERRNO_SYM(ECHRNG);
		ERRNO_SYM(EL2NSYNC);
		ERRNO_SYM(EL3HLT);
		ERRNO_SYM(EL3RST);
		ERRNO_SYM(ELNRNG);
		ERRNO_SYM(EUNATCH);
		ERRNO_SYM(ENOCSI);
		ERRNO_SYM(EL2HLT);
		ERRNO_SYM(EBADE);
		ERRNO_SYM(EBADR);
		ERRNO_SYM(EXFULL);
		ERRNO_SYM(ENOANO);
		ERRNO_SYM(EBADRQC);
		ERRNO_SYM(EBADSLT);
		ERRNO_SYM(EBFONT);
		ERRNO_SYM(ENOSTR);
		ERRNO_SYM(ENODATA);
		ERRNO_SYM(ETIME);
		ERRNO_SYM(ENOSR);
		ERRNO_SYM(ENONET);
		ERRNO_SYM(ENOPKG);
		ERRNO_SYM(EREMOTE);
		ERRNO_SYM(ENOLINK);
		ERRNO_SYM(EADV);
		ERRNO_SYM(ESRMNT);
		ERRNO_SYM(ECOMM);
		ERRNO_SYM(EPROTO);
		ERRNO_SYM(EMULTIHOP);
		ERRNO_SYM(EDOTDOT);
		ERRNO_SYM(EBADMSG);
		ERRNO_SYM(EOVERFLOW);
		ERRNO_SYM(ENOTUNIQ);
		ERRNO_SYM(EBADFD);
		ERRNO_SYM(EREMCHG);
		ERRNO_SYM(ELIBACC);
		ERRNO_SYM(ELIBBAD);
		ERRNO_SYM(ELIBSCN);
		ERRNO_SYM(ELIBMAX);
		ERRNO_SYM(EILSEQ);
		ERRNO_SYM(ERESTART);
		ERRNO_SYM(ESTRPIPE);
		ERRNO_SYM(EUSERS);
		ERRNO_SYM(ENOTSOCK);
		ERRNO_SYM(EDESTADDRREQ);
		ERRNO_SYM(EMSGSIZE);
		ERRNO_SYM(EPROTOTYPE);
		ERRNO_SYM(ENOPROTOOPT);
		ERRNO_SYM(EPROTONOSUPPORT);
		ERRNO_SYM(ESOCKTNOSUPPORT);
		ERRNO_SYM(EOPNOTSUPP);
		ERRNO_SYM(EPFNOSUPPORT);
		ERRNO_SYM(EAFNOSUPPORT);
		ERRNO_SYM(EADDRINUSE);
		ERRNO_SYM(EADDRNOTAVAIL);
		ERRNO_SYM(ENETDOWN);
		ERRNO_SYM(ENETUNREACH);
		ERRNO_SYM(ENETRESET);
		ERRNO_SYM(ECONNABORTED);
		ERRNO_SYM(ECONNRESET);
		ERRNO_SYM(ENOBUFS);
		ERRNO_SYM(EISCONN);
		ERRNO_SYM(ENOTCONN);
		ERRNO_SYM(ESHUTDOWN);
		ERRNO_SYM(ETOOMANYREFS);
		ERRNO_SYM(ETIMEDOUT);
		ERRNO_SYM(ECONNREFUSED);
		ERRNO_SYM(EHOSTDOWN);
		ERRNO_SYM(EHOSTUNREACH);
		ERRNO_SYM(EALREADY);
		ERRNO_SYM(EINPROGRESS);
		ERRNO_SYM(ESTALE);
		ERRNO_SYM(EUCLEAN);
		ERRNO_SYM(ENOTNAM);
		ERRNO_SYM(ENAVAIL);
		ERRNO_SYM(EISNAM);
		ERRNO_SYM(EREMOTEIO);
		ERRNO_SYM(EDQUOT);
		ERRNO_SYM(ENOMEDIUM);
		ERRNO_SYM(EMEDIUMTYPE);
		ERRNO_SYM(ECANCELED);
		ERRNO_SYM(ENOKEY);
		ERRNO_SYM(EKEYEXPIRED);
		ERRNO_SYM(EKEYREVOKED);
		ERRNO_SYM(EKEYREJECTED);
		ERRNO_SYM(EOWNERDEAD);
		ERRNO_SYM(ENOTRECOVERABLE);
		ERRNO_SYM(ERFKILL);
		ERRNO_SYM(EHWPOISON);

		/* The value 0 is not an error and has no name. Each other value is an
		   errno of this platform that this list does not have, and the integer
		   from (errno) still gives that value. */
		case 0:
		default:
			returnValue->voidValue = VoidConstant(theEnv);
			return;
	}

	returnValue->lexemeValue = err;
}

void ErrnoFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	returnValue->integerValue = CreateInteger(theEnv, errno);
}
void SleepFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	double seconds;
	int res;
	struct timespec ts;
	UDFValue theArg;

	UDFNextArgument(context,NUMBER_BITS,&theArg);
	if (theArg.header->type == FLOAT_TYPE)
	{
		seconds = theArg.floatValue->contents;
	}
	else
	{
		seconds = (double) theArg.integerValue->contents;
	}

	if (seconds < 0)
	{
		WriteString(theEnv,STDERR,"sleep: seconds must be greater than 0\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	ts.tv_sec = floor(seconds);
	ts.tv_nsec = (seconds - ts.tv_sec) * 1000000000;

	do {
		res = nanosleep(&ts, &ts);
	} while (res && errno == EINTR);

	returnValue->integerValue = CreateInteger(theEnv, res);
}

/* SIGKILL and SIGSTOP are listed so that asking for them fails the way the
   kernel says it does rather than as an unknown name. The fault signals
   (SIGSEGV, SIGBUS, SIGFPE, SIGILL) are deliberately absent: ignoring one of
   those re-runs the faulting instruction forever. */
static const char *SignalNumberToSymbol(
		int sig)
{
	switch(sig)
	{
		case SIGHUP:    return "SIGHUP";
		case SIGINT:    return "SIGINT";
		case SIGQUIT:   return "SIGQUIT";
		case SIGKILL:   return "SIGKILL";
		case SIGPIPE:   return "SIGPIPE";
		case SIGALRM:   return "SIGALRM";
		case SIGTERM:   return "SIGTERM";
		case SIGUSR1:   return "SIGUSR1";
		case SIGUSR2:   return "SIGUSR2";
		case SIGCHLD:   return "SIGCHLD";
		case SIGCONT:   return "SIGCONT";
		case SIGSTOP:   return "SIGSTOP";
		case SIGTSTP:   return "SIGTSTP";
		case SIGWINCH:  return "SIGWINCH";
		default:        return NULL;
	}
}

/* Reverse of the above, driven by the same switch so the two directions
   cannot disagree. */
static int SignalSymbolToNumber(
		const char *sym)
{
	int sig;
	const char *name;

	for (sig = 1; sig < NSIG; sig++)
	{
		name = SignalNumberToSymbol(sig);
		if (name != NULL && 0 == strcmp(name,sym)) return sig;
	}

	return -1;
}

/* (signal <signal-symbol> SIG_IGN|SIG_DFL)
   Sets what the process does with a signal. Only the two dispositions are
   available: a handler would have to run CLIPS from an asynchronous context,
   which is not safe. */
void SignalFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	UDFValue theArg;
	int sig;
	void (*disposition)(int);

	UDFNextArgument(context,SYMBOL_BIT,&theArg);
	if (-1 == (sig = SignalSymbolToNumber(theArg.lexemeValue->contents)))
	{
		WriteString(theEnv,STDERR,"signal: unsupported signal '");
		WriteString(theEnv,STDERR,theArg.lexemeValue->contents);
		WriteString(theEnv,STDERR,"'.\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	UDFNextArgument(context,SYMBOL_BIT,&theArg);
	if (0 == strcmp(theArg.lexemeValue->contents,"SIG_IGN"))
	{
		disposition = SIG_IGN;
	}
	else if (0 == strcmp(theArg.lexemeValue->contents,"SIG_DFL"))
	{
		disposition = SIG_DFL;
	}
	else
	{
		WriteString(theEnv,STDERR,"signal: disposition must be SIG_IGN or SIG_DFL, not '");
		WriteString(theEnv,STDERR,theArg.lexemeValue->contents);
		WriteString(theEnv,STDERR,"'.\n");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	if (SIG_ERR == signal(sig,disposition))
	{
		WriteString(theEnv,STDERR,"signal: could not set disposition for ");
		WriteString(theEnv,STDERR,SignalNumberToSymbol(sig));
		WriteString(theEnv,STDERR,"\n");
		perror("signal");
		returnValue->lexemeValue = FalseSymbol(theEnv);
		return;
	}

	returnValue->lexemeValue = CreateBoolean(theEnv,true);
}

void ScandirFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	struct dirent **namelist;
	int n;
	UDFValue theArg;
	MultifieldBuilder *mb = CreateMultifieldBuilder(theEnv, 0);

	UDFNextArgument(context,LEXEME_BITS,&theArg);

	n = scandir(theArg.lexemeValue->contents, &namelist, NULL, alphasort);
	if (n < 0)
	{
		returnValue->lexemeValue = FalseSymbol(theEnv);
	}
	else
	{
		while (n--) {
			MBAppendSymbol(mb, namelist[n]->d_name);
			free(namelist[n]);
		}
		free(namelist);
		returnValue->multifieldValue = MBCreate(mb);
		MBDispose(mb);
	}
}

#ifdef USE_LIBMAGIC
void MimetypeFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	const char *mime;
	magic_t magic;
	UDFValue theArg;

	UDFNextArgument(context,LEXEME_BITS,&theArg);

	magic = magic_open(MAGIC_MIME_TYPE); 
	magic_load(magic, NULL);
	mime = magic_file(magic, theArg.lexemeValue->contents);

	returnValue->lexemeValue = CreateSymbol(theEnv, mime);

	magic_close(magic);
}
#endif

/*********************************************************/
/* UserFunctions: Informs the expert system environment  */
/*   of any user defined functions. In the default case, */
/*   there are no user defined functions. To define      */
/*   functions, either this function must be replaced by */
/*   a function with the same name within this file, or  */
/*   this function can be deleted from this file and     */
/*   included in another file.                           */
/*********************************************************/
void UserFunctions(
  Environment *env)
  {
	  /* The socket and TLS routers.
	     Registering here leaves every file of the CLIPS source
	     untouched, which is what lets one copy of this project build against
	     6.4.2, the 6.x branch and the 7.x branch. */
	  InitializeSocketRouter(env);

	  AddUDF(env,"accept","bl",1,1,"lsy",AcceptFunction,"AcceptFunction",NULL);
	  AddUDF(env,"bind-socket","bsy",2,3,";l;sy;l",BindSocketFunction,"BindSocketFunction",NULL);
	  AddUDF(env,"connect","bl",2,3,";l;sy;l",ConnectFunction,"ConnectFunction",NULL);
	  AddUDF(env,"close-connection","b",1,1,";lsy",CloseConnectionFunction,"CloseConnectionFunction",NULL);
	  AddUDF(env,"create-socket","bl",2,3,";sy;sy;l",CreateSocketFunction,"CreateSocketFunction",NULL);
	  AddUDF(env,"empty-connection","bl",1,1,"lsy",EmptyConnectionFunction,"EmptyConnectionFunction",NULL);
	  AddUDF(env,"fcntl-add-status-flags","bl",2,UNBOUNDED,"sy;syl;",FcntlAddStatusFlagsFunction,"FcntlAddStatusFlagsFunction",NULL);
	  AddUDF(env,"fcntl-remove-status-flags","bl",2,UNBOUNDED,"sy;syl;",FcntlRemoveStatusFlagsFunction,"FcntlRemoveStatusFlagsFunction",NULL);
	  AddUDF(env,"flush-connection","l",1,1,"lsy",FlushConnectionFunction,"FlushConnectionFunction",NULL);
	  AddUDF(env,"get-socket-logical-name","y",1,1,"l",GetSocketLogicalNameFunction,"GetSocketLogicalNameFunction",NULL);
	  AddUDF(env,"get-retained-bytes","bl",1,1,"lsy",GetRetainedBytesFunction,"GetRetainedBytesFunction",NULL);
	  AddUDF(env,"get-retained-limit","bl",1,1,"lsy",GetRetainedLimitFunction,"GetRetainedLimitFunction",NULL);
	  AddUDF(env,"get-timeout","l",1,1,"lsy",GetTimeoutFunction,"GetTimeoutFunction",NULL);
	  AddUDF(env,"getsockopt","bl",3,3,";lsy;sy;sy",GetsockoptFunction,"GetsockoptFunction",NULL);
	  AddUDF(env,"listen","b",1,2,";lsy;l",ListenFunction,"ListenFunction",NULL);
	  AddUDF(env,"poll","b",1,11,"sy;lsy;l;sy;",PollFunction,"PollFunction",NULL);
	  AddUDF(env,"set-fully-buffered","b",1,1,"lsy",SetFullyBufferedFunction,"SetFullyBufferedFunction",NULL);
	  AddUDF(env,"set-not-buffered","b",1,1,"lsy",SetNotBufferedFunction,"SetNotBufferedFunction",NULL);
	  AddUDF(env,"set-line-buffered","b",1,1,"lsy",SetLineBufferedFunction,"SetLineBufferedFunction",NULL);
	  AddUDF(env,"set-retained-limit","b",2,2,";lsy;l",SetRetainedLimitFunction,"SetRetainedLimitFunction",NULL);
	  AddUDF(env,"set-timeout","b",2,3,";lsy;l;l",SetTimeoutFunction,"SetTimeoutFunction",NULL);
	  AddUDF(env,"setsockopt","l",4,4,";lsy;sy;sy;l",SetsockoptFunction,"SetsockoptFunction",NULL);
	  AddUDF(env,"shutdown-connection","b",1,2,";lsy;y",ShutdownConnectionFunction,"ShutdownConnectionFunction",NULL);
	  AddUDF(env,"resolve-domain-name","bm",1,1,"sy",ResolveDomainNameFunction,"ResolveDomainNameFunction",NULL);

	  AddUDF(env,"errno","l",0,0,NULL,ErrnoFunction,"ErrnoFunction",NULL);
	  AddUDF(env,"errno-sym","yv",0,1,"l",ErrnoSymFunction,"ErrnoSymFunction",NULL);

#ifdef USE_LIBMAGIC
	  AddUDF(env,"mimetype","by",1,1,"sy",MimetypeFunction,"MimetypeFunction",NULL);
#endif
	  AddUDF(env,"scandir","bm",1,1,"sy",ScandirFunction,"ScandirFunction",NULL);
	  AddUDF(env,"signal","b",2,2,";y;y",SignalFunction,"SignalFunction",NULL);
	  AddUDF(env,"sleep","bl",1,1,"ld",SleepFunction,"SleepFunction",NULL);

#ifdef USE_TLS
	  AddUDF(env,"tls-create-context","bl",1,1,"y",TLSCreateContextFunction,"TLSCreateContextFunction",NULL);
	  AddUDF(env,"tls-free-context","b",1,1,"l",TLSFreeContextFunction,"TLSFreeContextFunction",NULL);
	  AddUDF(env,"tls-context-load-verify-locations","b",2,3,";l;sy;sy",TLSContextLoadVerifyLocationsFunction,"TLSContextLoadVerifyLocationsFunction",NULL);
	  AddUDF(env,"tls-context-set-default-verify-paths","b",1,1,"l",TLSContextSetDefaultVerifyPathsFunction,"TLSContextSetDefaultVerifyPathsFunction",NULL);
	  AddUDF(env,"tls-context-use-certificate-file","b",2,2,";l;sy",TLSContextUseCertificateFileFunction,"TLSContextUseCertificateFileFunction",NULL);
	  AddUDF(env,"tls-context-use-private-key-file","b",2,2,";l;sy",TLSContextUsePrivateKeyFileFunction,"TLSContextUsePrivateKeyFileFunction",NULL);
	  AddUDF(env,"tls-context-set-verify","b",2,2,";l;y",TLSContextSetVerifyFunction,"TLSContextSetVerifyFunction",NULL);
	  AddUDF(env,"tls-context-set-min-proto-version","b",2,2,";l;y",TLSContextSetMinProtoVersionFunction,"TLSContextSetMinProtoVersionFunction",NULL);
	  AddUDF(env,"tls-connect","b",3,3,";l;lsy;sy",TLSConnectFunction,"TLSConnectFunction",NULL);
	  AddUDF(env,"tls-accept","b",2,2,";l;lsy",TLSAcceptFunction,"TLSAcceptFunction",NULL);
	  AddUDF(env,"tls-handshake","b",1,1,"lsy",TLSHandshakeFunction,"TLSHandshakeFunction",NULL);
	  AddUDF(env,"tls-shutdown","b",1,1,"lsy",TLSShutdownFunction,"TLSShutdownFunction",NULL);
	  AddUDF(env,"tls-pending","bl",1,1,"lsy",TLSPendingFunction,"TLSPendingFunction",NULL);
	  AddUDF(env,"tls-cipher","by",1,1,"lsy",TLSCipherFunction,"TLSCipherFunction",NULL);
	  AddUDF(env,"tls-version","by",1,1,"lsy",TLSVersionFunction,"TLSVersionFunction",NULL);
	  AddUDF(env,"tls-verify-result","bs",1,1,"lsy",TLSVerifyResultFunction,"TLSVerifyResultFunction",NULL);
	  AddUDF(env,"tls-peer-subject","bs",1,1,"lsy",TLSPeerSubjectFunction,"TLSPeerSubjectFunction",NULL);
	  AddUDF(env,"tls-backend","y",0,0,NULL,TLSBackendFunction,"TLSBackendFunction",NULL);
	  AddUDF(env,"tls-backend-version","s",0,0,NULL,TLSBackendVersionFunction,"TLSBackendVersionFunction",NULL);
	  AddUDF(env,"tls-supports-dtls","b",0,1,"y",TLSSupportsDTLSFunction,"TLSSupportsDTLSFunction",NULL);
	  AddUDF(env,"dtls-connect","b",3,3,";l;lsy;sy",DTLSConnectFunction,"DTLSConnectFunction",NULL);
	  AddUDF(env,"dtls-accept","b",2,2,";l;lsy",DTLSAcceptFunction,"DTLSAcceptFunction",NULL);
	  AddUDF(env,"dtls-send","bl",2,2,";lsy;sy",DTLSSendFunction,"DTLSSendFunction",NULL);
	  AddUDF(env,"dtls-recv","bm",1,2,";lsy;l",DTLSRecvFunction,"DTLSRecvFunction",NULL);
	  AddUDF(env,"dtls-timeout","bl",1,1,"lsy",DTLSTimeoutFunction,"DTLSTimeoutFunction",NULL);
	  AddUDF(env,"dtls-handle-timeout","b",1,1,"lsy",DTLSHandleTimeoutFunction,"DTLSHandleTimeoutFunction",NULL);
	  AddUDF(env,"dtls-set-mtu","b",2,2,";lsy;l",DTLSSetMTUFunction,"DTLSSetMTUFunction",NULL);
#endif

	  AddUDF(env,"rcvfrom","bm",1,3,";lsy;lmsy;l",RecvfromFunction,"RecvfromFunction",NULL);
	  AddUDF(env,"sendto","bl",3,6,";lsy;sy;sy;lsy;lmsy;lmsy",SendtoFunction,"SendtoFunction",NULL);
  }
