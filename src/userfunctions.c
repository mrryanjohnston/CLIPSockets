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

#include "clips.h"
#include "socketrtr.h"

void UserFunctions(Environment *);


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
	  AddUDF(env,"fcntl-add-status-flags","bl",2,UNBOUNDED,"sy;l;",FcntlAddStatusFlagsFunction,"FcntlAddStatusFlagsFunction",NULL);
	  AddUDF(env,"fcntl-remove-status-flags","bl",2,UNBOUNDED,"sy;l;",FcntlRemoveStatusFlagsFunction,"FcntlRemoveStatusFlagsFunction",NULL);
	  AddUDF(env,"flush-connection","l",1,1,"lsy",FlushConnectionFunction,"FlushConnectionFunction",NULL);
	  AddUDF(env,"get-socket-logical-name","y",1,1,"l",GetSocketLogicalNameFunction,"GetSocketLogicalNameFunction",NULL);
	  AddUDF(env,"get-timeout","l",1,1,"lsy",GetTimeoutFunction,"GetTimeoutFunction",NULL);
	  AddUDF(env,"getsockopt","bl",3,3,";l;sy;sy",GetsockoptFunction,"GetsockoptFunction",NULL);
	  AddUDF(env,"listen","b",1,2,";lsy;l",ListenFunction,"ListenFunction",NULL);
	  AddUDF(env,"poll","b",1,11,"sy;lsy;l;sy;",PollFunction,"PollFunction",NULL);
	  AddUDF(env,"set-fully-buffered","b",1,1,"lsy",SetFullyBufferedFunction,"SetFullyBufferedFunction",NULL);
	  AddUDF(env,"set-not-buffered","b",1,1,"lsy",SetNotBufferedFunction,"SetNotBufferedFunction",NULL);
	  AddUDF(env,"set-line-buffered","b",1,1,"lsy",SetLineBufferedFunction,"SetLineBufferedFunction",NULL);
	  AddUDF(env,"set-timeout","l",2,2,";lsy;l",SetTimeoutFunction,"SetTimeoutFunction",NULL);
	  AddUDF(env,"setsockopt","l",4,4,";l;sy;sy;l",SetsockoptFunction,"SetsockoptFunction",NULL);
	  AddUDF(env,"shutdown-connection","l",1,1,"lsy",ShutdownConnectionFunction,"ShutdownConnectionFunction",NULL);
  }
