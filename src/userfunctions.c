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
#include <errno.h>

#include "clips.h"
#include "socketrtr.h"

void UserFunctions(Environment *);

void ErrnoSymFunction(
		Environment *theEnv,
		UDFContext *context,
		UDFValue *returnValue)
{
	CLIPSLexeme *err;
	switch(errno)
	{
		case EPERM:
			err = CreateSymbol(theEnv, "EPERM");
			break;
		case ENOENT:
			err = CreateSymbol(theEnv, "ENOENT");
			break;
		case ESRCH:
			err = CreateSymbol(theEnv, "ESRCH");
			break;
		case EINTR:
			err = CreateSymbol(theEnv, "EINTR");
			break;
		case EIO:
			err = CreateSymbol(theEnv, "EIO");
			break;
		case ENXIO:
			err = CreateSymbol(theEnv, "ENXIO");
			break;
		case E2BIG:
			err = CreateSymbol(theEnv, "E2BIG");
			break;
		case ENOEXEC:
			err = CreateSymbol(theEnv, "ENOEXEC");
			break;
		case EBADF:
			err = CreateSymbol(theEnv, "EBADF");
			break;
		case ECHILD:
			err = CreateSymbol(theEnv, "ECHILD");
			break;
		case EAGAIN:
			err = CreateSymbol(theEnv, "EAGAIN");
			break;
		case ENOMEM:
			err = CreateSymbol(theEnv, "ENOMEM");
			break;
		case EACCES:
			err = CreateSymbol(theEnv, "EACCES");
			break;
		case EFAULT:
			err = CreateSymbol(theEnv, "EFAULT");
			break;
		case ENOTBLK:
			err = CreateSymbol(theEnv, "ENOTBLK");
			break;
		case EBUSY:
			err = CreateSymbol(theEnv, "EBUSY");
			break;
		case EEXIST:
			err = CreateSymbol(theEnv, "EEXIST");
			break;
		case EXDEV:
			err = CreateSymbol(theEnv, "EXDEV");
			break;
		case ENODEV:
			err = CreateSymbol(theEnv, "ENODEV");
			break;
		case ENOTDIR:
			err = CreateSymbol(theEnv, "ENOTDIR");
			break;
		case EISDIR:
			err = CreateSymbol(theEnv, "EISDIR");
			break;
		case EINVAL:
			err = CreateSymbol(theEnv, "EINVAL");
			break;
		case ENFILE:
			err = CreateSymbol(theEnv, "ENFILE");
			break;
		case EMFILE:
			err = CreateSymbol(theEnv, "EMFILE");
			break;
		case ENOTTY:
			err = CreateSymbol(theEnv, "ENOTTY");
			break;
		case ETXTBSY:
			err = CreateSymbol(theEnv, "ETXTBSY");
			break;
		case EFBIG:
			err = CreateSymbol(theEnv, "EFBIG");
			break;
		case ENOSPC:
			err = CreateSymbol(theEnv, "ENOSPC");
			break;
		case ESPIPE:
			err = CreateSymbol(theEnv, "ESPIPE");
			break;
		case EROFS:
			err = CreateSymbol(theEnv, "EROFS");
			break;
		case EMLINK:
			err = CreateSymbol(theEnv, "EMLINK");
			break;
		case EPIPE:
			err = CreateSymbol(theEnv, "EPIPE");
			break;
		case EDOM:
			err = CreateSymbol(theEnv, "EDOM");
			break;
		case ERANGE:
			err = CreateSymbol(theEnv, "ERANGE");
			break;
		case EDEADLK:
			err = CreateSymbol(theEnv, "EDEADLK");
			break;
		case ENAMETOOLONG:
			err = CreateSymbol(theEnv, "ENAMETOOLONG");
			break;
		case ENOLCK:
			err = CreateSymbol(theEnv, "ENOLCK");
			break;
		case ENOSYS:
			err = CreateSymbol(theEnv, "ENOSYS");
			break;
		case ENOTEMPTY:
			err = CreateSymbol(theEnv, "ENOTEMPTY");
			break;
		case ELOOP:
			err = CreateSymbol(theEnv, "ELOOP");
			break;
		case ENOMSG:
			err = CreateSymbol(theEnv, "ENOMSG");
			break;
		case EIDRM:
			err = CreateSymbol(theEnv, "EIDRM");
			break;
		case ECHRNG:
			err = CreateSymbol(theEnv, "ECHRNG");
			break;
		case EL2NSYNC:
			err = CreateSymbol(theEnv, "EL2NSYNC");
			break;
		case EL3HLT:
			err = CreateSymbol(theEnv, "EL3HLT");
			break;
		case EL3RST:
			err = CreateSymbol(theEnv, "EL3RST");
			break;
		case ELNRNG:
			err = CreateSymbol(theEnv, "ELNRNG");
			break;
		case EUNATCH:
			err = CreateSymbol(theEnv, "EUNATCH");
			break;
		case ENOCSI:
			err = CreateSymbol(theEnv, "ENOCSI");
			break;
		case EL2HLT:
			err = CreateSymbol(theEnv, "EL2HLT");
			break;
		case EBADE:
			err = CreateSymbol(theEnv, "EBADE");
			break;
		case EBADR:
			err = CreateSymbol(theEnv, "EBADR");
			break;
		case EXFULL:
			err = CreateSymbol(theEnv, "EXFULL");
			break;
		case ENOANO:
			err = CreateSymbol(theEnv, "ENOANO");
			break;
		case EBADRQC:
			err = CreateSymbol(theEnv, "EBADRQC");
			break;
		case EBADSLT:
			err = CreateSymbol(theEnv, "EBADSLT");
			break;
		case EBFONT:
			err = CreateSymbol(theEnv, "EBFONT");
			break;
		case ENOSTR:
			err = CreateSymbol(theEnv, "ENOSTR");
			break;
		case ENODATA:
			err = CreateSymbol(theEnv, "ENODATA");
			break;
		case ETIME:
			err = CreateSymbol(theEnv, "ETIME");
			break;
		case ENOSR:
			err = CreateSymbol(theEnv, "ENOSR");
			break;
		case ENONET:
			err = CreateSymbol(theEnv, "ENONET");
			break;
		case ENOPKG:
			err = CreateSymbol(theEnv, "ENOPKG");
			break;
		case EREMOTE:
			err = CreateSymbol(theEnv, "EREMOTE");
			break;
		case ENOLINK:
			err = CreateSymbol(theEnv, "ENOLINK");
			break;
		case EADV:
			err = CreateSymbol(theEnv, "EADV");
			break;
		case ESRMNT:
			err = CreateSymbol(theEnv, "ESRMNT");
			break;
		case ECOMM:
			err = CreateSymbol(theEnv, "ECOMM");
			break;
		case EPROTO:
			err = CreateSymbol(theEnv, "EPROTO");
			break;
		case EMULTIHOP:
			err = CreateSymbol(theEnv, "EMULTIHOP");
			break;
		case EDOTDOT:
			err = CreateSymbol(theEnv, "EDOTDOT");
			break;
		case EBADMSG:
			err = CreateSymbol(theEnv, "EBADMSG");
			break;
		case EOVERFLOW:
			err = CreateSymbol(theEnv, "EOVERFLOW");
			break;
		case ENOTUNIQ:
			err = CreateSymbol(theEnv, "ENOTUNIQ");
			break;
		case EBADFD:
			err = CreateSymbol(theEnv, "EBADFD");
			break;
		case EREMCHG:
			err = CreateSymbol(theEnv, "EREMCHG");
			break;
		case ELIBACC:
			err = CreateSymbol(theEnv, "ELIBACC");
			break;
		case ELIBBAD:
			err = CreateSymbol(theEnv, "ELIBBAD");
			break;
		case ELIBSCN:
			err = CreateSymbol(theEnv, "ELIBSCN");
			break;
		case ELIBMAX:
			err = CreateSymbol(theEnv, "ELIBMAX");
			break;
		case EILSEQ:
			err = CreateSymbol(theEnv, "EILSEQ");
			break;
		case ERESTART:
			err = CreateSymbol(theEnv, "ERESTART");
			break;
		case ESTRPIPE:
			err = CreateSymbol(theEnv, "ESTRPIPE");
			break;
		case EUSERS:
			err = CreateSymbol(theEnv, "EUSERS");
			break;
		case ENOTSOCK:
			err = CreateSymbol(theEnv, "ENOTSOCK");
			break;
		case EDESTADDRREQ:
			err = CreateSymbol(theEnv, "EDESTADDRREQ");
			break;
		case EMSGSIZE:
			err = CreateSymbol(theEnv, "EMSGSIZE");
			break;
		case EPROTOTYPE:
			err = CreateSymbol(theEnv, "EPROTOTYPE");
			break;
		case ENOPROTOOPT:
			err = CreateSymbol(theEnv, "ENOPROTOOPT");
			break;
		case EPROTONOSUPPORT:
			err = CreateSymbol(theEnv, "EPROTONOSUPPORT");
			break;
		case ESOCKTNOSUPPORT:
			err = CreateSymbol(theEnv, "ESOCKTNOSUPPORT");
			break;
		case EOPNOTSUPP:
			err = CreateSymbol(theEnv, "EOPNOTSUPP");
			break;
		case EPFNOSUPPORT:
			err = CreateSymbol(theEnv, "EPFNOSUPPORT");
			break;
		case EAFNOSUPPORT:
			err = CreateSymbol(theEnv, "EAFNOSUPPORT");
			break;
		case EADDRINUSE:
			err = CreateSymbol(theEnv, "EADDRINUSE");
			break;
		case EADDRNOTAVAIL:
			err = CreateSymbol(theEnv, "EADDRNOTAVAIL");
			break;
		case ENETDOWN:
			err = CreateSymbol(theEnv, "ENETDOWN");
			break;
		case ENETUNREACH:
			err = CreateSymbol(theEnv, "ENETUNREACH");
			break;
		case ENETRESET:
			err = CreateSymbol(theEnv, "ENETRESET");
			break;
		case ECONNABORTED:
			err = CreateSymbol(theEnv, "ECONNABORTED");
			break;
		case ECONNRESET:
			err = CreateSymbol(theEnv, "ECONNRESET");
			break;
		case ENOBUFS:
			err = CreateSymbol(theEnv, "ENOBUFS");
			break;
		case EISCONN:
			err = CreateSymbol(theEnv, "EISCONN");
			break;
		case ENOTCONN:
			err = CreateSymbol(theEnv, "ENOTCONN");
			break;
		case ESHUTDOWN:
			err = CreateSymbol(theEnv, "ESHUTDOWN");
			break;
		case ETOOMANYREFS:
			err = CreateSymbol(theEnv, "ETOOMANYREFS");
			break;
		case ETIMEDOUT:
			err = CreateSymbol(theEnv, "ETIMEDOUT");
			break;
		case ECONNREFUSED:
			err = CreateSymbol(theEnv, "ECONNREFUSED");
			break;
		case EHOSTDOWN:
			err = CreateSymbol(theEnv, "EHOSTDOWN");
			break;
		case EHOSTUNREACH:
			err = CreateSymbol(theEnv, "EHOSTUNREACH");
			break;
		case EALREADY:
			err = CreateSymbol(theEnv, "EALREADY");
			break;
		case EINPROGRESS:
			err = CreateSymbol(theEnv, "EINPROGRESS");
			break;
		case ESTALE:
			err = CreateSymbol(theEnv, "ESTALE");
			break;
		case EUCLEAN:
			err = CreateSymbol(theEnv, "EUCLEAN");
			break;
		case ENOTNAM:
			err = CreateSymbol(theEnv, "ENOTNAM");
			break;
		case ENAVAIL:
			err = CreateSymbol(theEnv, "ENAVAIL");
			break;
		case EISNAM:
			err = CreateSymbol(theEnv, "EISNAM");
			break;
		case EREMOTEIO:
			err = CreateSymbol(theEnv, "EREMOTEIO");
			break;
		case EDQUOT:
			err = CreateSymbol(theEnv, "EDQUOT");
			break;
		case ENOMEDIUM:
			err = CreateSymbol(theEnv, "ENOMEDIUM");
			break;
		case EMEDIUMTYPE:
			err = CreateSymbol(theEnv, "EMEDIUMTYPE");
			break;
		case ECANCELED:
			err = CreateSymbol(theEnv, "ECANCELED");
			break;
		case ENOKEY:
			err = CreateSymbol(theEnv, "ENOKEY");
			break;
		case EKEYEXPIRED:
			err = CreateSymbol(theEnv, "EKEYEXPIRED");
			break;
		case EKEYREVOKED:
			err = CreateSymbol(theEnv, "EKEYREVOKED");
			break;
		case EKEYREJECTED:
			err = CreateSymbol(theEnv, "EKEYREJECTED");
			break;
		case EOWNERDEAD:
			err = CreateSymbol(theEnv, "EOWNERDEAD");
			break;
		case ENOTRECOVERABLE:
			err = CreateSymbol(theEnv, "ENOTRECOVERABLE");
			break;
		case ERFKILL:
			err = CreateSymbol(theEnv, "ERFKILL");
			break;
		case EHWPOISON:
			err = CreateSymbol(theEnv, "EHWPOISON");
			break;
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
	  AddUDF(env,"accept","bl",1,1,"lsy",AcceptFunction,"AcceptFunction",NULL);
	  AddUDF(env,"bind-socket","bsy",2,3,";l;sy;l",BindSocketFunction,"BindSocketFunction",NULL);
	  AddUDF(env,"connect","bl",2,3,";l;sy;l",ConnectFunction,"ConnectFunction",NULL);
	  AddUDF(env,"close-connection","b",1,1,";lsy",CloseConnectionFunction,"CloseConnectionFunction",NULL);
	  AddUDF(env,"create-socket","bl",2,3,"sy",CreateSocketFunction,"CreateSocketFunction",NULL);
	  AddUDF(env,"empty-connection","bl",1,1,"lsy",EmptyConnectionFunction,"EmptyConnectionFunction",NULL);
	  AddUDF(env,"fcntl-add-status-flags","bl",2,UNBOUNDED,"sy;syl;",FcntlAddStatusFlagsFunction,"FcntlAddStatusFlagsFunction",NULL);
	  AddUDF(env,"fcntl-remove-status-flags","bl",2,UNBOUNDED,"sy;syl;",FcntlRemoveStatusFlagsFunction,"FcntlRemoveStatusFlagsFunction",NULL);
	  AddUDF(env,"flush-connection","l",1,1,"lsy",FlushConnectionFunction,"FlushConnectionFunction",NULL);
	  AddUDF(env,"get-socket-logical-name","y",1,1,"l",GetSocketLogicalNameFunction,"GetSocketLogicalNameFunction",NULL);
	  AddUDF(env,"get-timeout","l",1,1,"lsy",GetTimeoutFunction,"GetTimeoutFunction",NULL);
	  AddUDF(env,"getsockopt","bl",3,3,";lsy;sy;sy",GetsockoptFunction,"GetsockoptFunction",NULL);
	  AddUDF(env,"listen","b",1,2,";lsy;l",ListenFunction,"ListenFunction",NULL);
	  AddUDF(env,"poll","b",1,11,"sy;lsy;l;sy;",PollFunction,"PollFunction",NULL);
	  AddUDF(env,"set-fully-buffered","b",1,1,"lsy",SetFullyBufferedFunction,"SetFullyBufferedFunction",NULL);
	  AddUDF(env,"set-not-buffered","b",1,1,"lsy",SetNotBufferedFunction,"SetNotBufferedFunction",NULL);
	  AddUDF(env,"set-line-buffered","b",1,1,"lsy",SetLineBufferedFunction,"SetLineBufferedFunction",NULL);
	  AddUDF(env,"set-timeout","l",2,2,";lsy;l",SetTimeoutFunction,"SetTimeoutFunction",NULL);
	  AddUDF(env,"setsockopt","l",4,4,";lsy;sy;sy;l",SetsockoptFunction,"SetsockoptFunction",NULL);
	  AddUDF(env,"shutdown-connection","l",1,1,"lsy",ShutdownConnectionFunction,"ShutdownConnectionFunction",NULL);
	  AddUDF(env,"resolve-domain-name","bm",1,1,"sy",ResolveDomainNameFunction,"ResolveDomainNameFunction",NULL);

	  AddUDF(env,"errno","l",0,0,NULL,ErrnoFunction,"ErrnoFunction",NULL);
	  AddUDF(env,"errno-sym","yv",0,0,NULL,ErrnoSymFunction,"ErrnoSymFunction",NULL);
  }
