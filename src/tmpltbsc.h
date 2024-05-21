   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.50  09/14/23            */
   /*                                                     */
   /*       DEFTEMPLATE BASIC COMMANDS HEADER FILE        */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements core commands for the deftemplate     */
/*   construct such as clear, reset, save, undeftemplate,    */
/*   ppdeftemplate, list-deftemplates, and                   */
/*   get-deftemplate-list.                                   */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Corrected compilation errors for files         */
/*            generated by constructs-to-c. DR0861           */
/*                                                           */
/*            Changed name of variable log to logName        */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Corrected code to remove compiler warnings     */
/*            when ENVIRONMENT_API_ONLY flag is set.         */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*      6.40: Removed LOCALE definition.                     */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            ALLOW_ENVIRONMENT_GLOBALS no longer supported. */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*      6.50: Changed the function name DeftemplateGetWatch  */
/*            to DeftemplateGetWatchFacts.                   */
/*                                                           */
/*            Added functions DeftemplateGetWatchGoals and   */
/*            DeftemplateSetWatchGoals.                      */
/*                                                           */
/*************************************************************/

#ifndef _H_tmpltbsc

#pragma once

#define _H_tmpltbsc

#include "evaluatn.h"
#include "tmpltdef.h"

   void                           DeftemplateBasicCommands(Environment *);
   void                           UndeftemplateCommand(Environment *,UDFContext *,UDFValue *);
   bool                           Undeftemplate(Deftemplate *,Environment *);
   void                           GetDeftemplateListFunction(Environment *,UDFContext *,UDFValue *);
   void                           GetDeftemplateList(Environment *,CLIPSValue *,Defmodule *);
   void                           DeftemplateModuleFunction(Environment *,UDFContext *,UDFValue *);
#if DEBUGGING_FUNCTIONS
   void                           PPDeftemplateCommand(Environment *,UDFContext *,UDFValue *);
   bool                           PPDeftemplate(Environment *,const char *,const char *);
   void                           ListDeftemplatesCommand(Environment *,UDFContext *,UDFValue *);
   void                           ListDeftemplates(Environment *,const char *,Defmodule *);
   bool                           DeftemplateGetWatchFacts(Deftemplate *);
   void                           DeftemplateSetWatchFacts(Deftemplate *,bool);
   bool                           DeftemplateGetWatchGoals(Deftemplate *);
   void                           DeftemplateSetWatchGoals(Deftemplate *,bool);
   bool                           DeftemplateWatchAccess(Environment *,int,bool,struct expr *);
   bool                           DeftemplateWatchPrint(Environment *,const char *,int,struct expr *);
#endif

#endif /* _H_tmpltbsc */


