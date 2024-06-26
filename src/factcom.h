   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 7.00  12/31/23            */
   /*                                                     */
   /*               FACT COMMANDS HEADER FILE             */
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
/* Revision History:                                         */
/*                                                           */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*      6.24: Added environment parameter to GenClose.       */
/*            Added environment parameter to GenOpen.        */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Support for long long integers.                */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Added code to prevent a clear command from     */
/*            being executed during fact assertions via      */
/*            Increment/DecrementClearReadyLocks API.        */
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
/*      7.00: Support for data driven backward chaining.     */
/*                                                           */
/*            Support for non-reactive fact patterns.        */
/*                                                           */
/*************************************************************/

#ifndef _H_factcom

#pragma once

#define _H_factcom

#if DEFTEMPLATE_CONSTRUCT

#include "evaluatn.h"

   void                           FactCommandDefinitions(Environment *);
   void                           AssertCommand(Environment *,UDFContext *,UDFValue *);
   void                           RetractCommand(Environment *,UDFContext *,UDFValue *);
   void                           AssertStringFunction(Environment *,UDFContext *,UDFValue *);
   void                           FactsCommand(Environment *,UDFContext *,UDFValue *);
   void                           GoalsCommand(Environment *,UDFContext *,UDFValue *);
   void                           Facts(Environment *,const char *,Defmodule *,long long,long long,long long);
   void                           Goals(Environment *,const char *,Defmodule *,long long,long long,long long);
   void                           SetFactDuplicationCommand(Environment *,UDFContext *,UDFValue *);
   void                           GetFactDuplicationCommand(Environment *,UDFContext *,UDFValue *);
   void                           FactIndexFunction(Environment *,UDFContext *,UDFValue *);
   void                           FactsGoalsDriver(Environment *,const char *,Defmodule *,long long,long long,long long,bool);

#endif /* DEFTEMPLATE_CONSTRUCT */

#endif /* _H_factcom */
