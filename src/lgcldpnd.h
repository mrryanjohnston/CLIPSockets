   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 7.00  06/11/24            */
   /*                                                     */
   /*          LOGICAL DEPENDENCIES HEADER FILE           */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provide support routines for managing truth      */
/*   maintenance using the logical conditional element.      */
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
/*      6.40: Removed LOCALE definition.                     */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*      7.00: Support for certainty factors.                 */
/*                                                           */
/*************************************************************/

#ifndef _H_lgcldpnd

#pragma once

#define _H_lgcldpnd

struct dependency
  {
   void *dPtr;
   struct dependency *next;
#if CERTAINTY_FACTORS
   short cf;
#endif
  };

#include "entities.h"
#include "match.h"

   bool                           AddLogicalDependencies(Environment *,PatternEntity *,bool,short);
   void                           RemoveEntityDependencies(Environment *,PatternEntity *);
   void                           RemovePMDependencies(Environment *,PartialMatch *);
   void                           DestroyPMDependencies(Environment *,PartialMatch *);
   void                           RemoveLogicalSupport(Environment *,PartialMatch *);
   void                           ForceLogicalRetractions(Environment *);
   void                           Dependencies(Environment *,PatternEntity *);
   void                           Dependents(Environment *,PatternEntity *);
   void                           DependenciesCommand(Environment *,UDFContext *,UDFValue *);
   void                           DependentsCommand(Environment *,UDFContext *,UDFValue *);
   void                           ReturnEntityDependencies(Environment *,PatternEntity *);
   PartialMatch                  *FindLogicalBind(struct joinNode *,PartialMatch *);

#endif /* _H_lgcldpnd */





