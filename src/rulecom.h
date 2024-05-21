   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  08/25/16            */
   /*                                                     */
   /*             DEFRULE COMMANDS HEADER FILE            */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides the matches command. Also provides the  */
/*   the developer commands show-joins and rule-complexity.  */
/*   Also provides the initialization routine which          */
/*   registers rule commands found in other modules.         */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Removed CONFLICT_RESOLUTION_STRATEGIES         */
/*            INCREMENTAL_RESET, and LOGICAL_DEPENDENCIES    */
/*            compilation flags.                             */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            Added support for hashed memories.             */
/*                                                           */
/*            Improvements to matches command.               */
/*                                                           */
/*            Add join-activity and join-activity-reset      */
/*            commands.                                      */
/*                                                           */
/*            Added get-beta-memory-resizing and             */
/*            set-beta-memory-resizing functions.            */
/*                                                           */
/*            Added timetag function.                        */
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
/*      6.50: Support for data driven backward chaining.     */
/*                                                           */
/*************************************************************/

#ifndef _H_rulecom

#pragma once

#define _H_rulecom

#include "evaluatn.h"

struct joinInformation
  {
   unsigned short whichCE;
   struct joinNode *theJoin;
   int patternBegin;
   int patternEnd;
   int marked;
   struct betaMemory *theMemory;
   struct joinNode *nextJoin;
  };

typedef enum
  {
   VERBOSE,
   SUCCINCT,
   TERSE
  } Verbosity;

   bool                           GetBetaMemoryResizing(Environment *);
   bool                           SetBetaMemoryResizing(Environment *,bool);
   void                           GetBetaMemoryResizingCommand(Environment *,UDFContext *,UDFValue *);
   void                           SetBetaMemoryResizingCommand(Environment *,UDFContext *,UDFValue *);
   void                           Matches(Defrule *,Verbosity,CLIPSValue *);
   void                           JoinActivity(Environment *,Defrule *,int,UDFValue *);
   void                           DefruleCommands(Environment *);
   void                           MatchesCommand(Environment *,UDFContext *,UDFValue *);
   void                           JoinActivityCommand(Environment *,UDFContext *,UDFValue *);
   void                           TimetagFunction(Environment *,UDFContext *,UDFValue *);
   unsigned short                 AlphaJoinCount(Environment *,Defrule *);
   unsigned short                 BetaJoinCount(Environment *,Defrule *);
   struct joinInformation        *CreateJoinArray(Environment *,unsigned short);
   void                           FreeJoinArray(Environment *,struct joinInformation *,unsigned short);
   void                           AlphaJoins(Environment *,Defrule *,unsigned short,struct joinInformation *);
   void                           BetaJoins(Environment *,Defrule *,unsigned short,struct joinInformation *);
   void                           JoinActivityResetCommand(Environment *,UDFContext *,UDFValue *);
   void                           WhyCommand(Environment *,UDFContext *,UDFValue *);
   void                           GetFocusFunction(Environment *,UDFContext *,UDFValue *);
   Defmodule                     *GetFocus(Environment *);
#if DEVELOPER
   void                           ShowJoinsCommand(Environment *,UDFContext *,UDFValue *);
   void                           RuleComplexityCommand(Environment *,UDFContext *,UDFValue *);
   void                           ShowAlphaHashTable(Environment *,UDFContext *,UDFValue *);
#endif

#endif /* _H_rulecom */
