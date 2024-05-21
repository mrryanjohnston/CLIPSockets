   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 7.00  12/26/23            */
   /*                                                     */
   /*                RETRACT HEADER FILE                  */
   /*******************************************************/

/*************************************************************/
/* Purpose:  Handles join network activity associated with   */
/*   with the removal of a data entity such as a fact or     */
/*   instance.                                               */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Removed LOGICAL_DEPENDENCIES compilation flag. */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Rule with exists CE has incorrect activation.  */
/*            DR0867                                         */
/*                                                           */
/*      6.30: Added support for hashed memories.             */
/*                                                           */
/*            Added additional developer statistics to help  */
/*            analyze join network performance.              */
/*                                                           */
/*            Removed pseudo-facts used in not CEs.          */
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
/*      7.00: Support for non-reactive fact patterns.        */
/*                                                           */
/*************************************************************/

#ifndef _H_retract

#pragma once

#define _H_retract

#include "match.h"
#include "network.h"

struct rdriveinfo
  {
   struct partialMatch *link;
   struct joinNode *jlist;
   struct rdriveinfo *next;
  };

void                           NetworkRetract(Environment *,struct patternMatch *);
void                           NetworkRetractMatch(Environment *,struct patternMatch *);
void                           ReturnPartialMatch(Environment *,struct partialMatch *);
void                           DestroyPartialMatch(Environment *,struct partialMatch *);
void                           FlushGarbagePartialMatches(Environment *);
void                           DeletePartialMatches(Environment *,struct partialMatch *);
void                           PosEntryRetractBeta(Environment *,struct partialMatch *,struct partialMatch *,int);
void                           PosEntryRetractAlpha(Environment *,struct partialMatch *,int);
bool                           PartialMatchWillBeDeleted(Environment *,struct partialMatch *);

#endif /* _H_retract */



