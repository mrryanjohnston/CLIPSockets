   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 7.00  09/07/23             */
   /*                                                     */
   /*                  MATCH HEADER FILE                  */
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
/*      6.30: Added support for hashed memories.             */
/*                                                           */
/*            Added additional members to partialMatch to    */
/*            support changes to the matching algorithm.     */
/*                                                           */
/*      6.31: Bug fix to prevent rule activations for        */
/*            partial matches being deleted.                 */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*      7.00: Support for data driven backward chaining.     */
/*                                                           */
/*************************************************************/

#ifndef _H_match

#pragma once

#define _H_match

typedef struct genericMatch GenericMatch;
typedef struct patternMatch PatternMatch;
typedef struct partialMatch PartialMatch;
typedef struct alphaMatch AlphaMatch;
typedef struct multifieldMarker MultifieldMarker;

#include "entities.h"
#include "network.h"

/****************/
/* patternMatch */
/****************/
struct patternMatch
  {
   PatternMatch *next;
   PartialMatch *theMatch;
   PatternNodeHeader *matchingPattern;
  };

/****************/
/* genericMatch */
/****************/
struct genericMatch
  {
   union
     {
      void *theValue;
      AlphaMatch *theMatch;
     } gm;
  };

/****************/
/* partialMatch */
/****************/
struct partialMatch
  {
   unsigned int betaMemory  :  1;
   unsigned int busy        :  1;
   unsigned int rhsMemory   :  1;
   unsigned int deleting    :  1;
   unsigned int goalMarker  :  1;
   unsigned short bcount;
   unsigned long hashValue;
   void *owner;
   void *marker;
   void *dependents;
   PartialMatch *nextInMemory;
   PartialMatch *prevInMemory;
   PartialMatch *children;
   PartialMatch *rightParent;
   PartialMatch *nextRightChild;
   PartialMatch *prevRightChild;
   PartialMatch *leftParent;
   PartialMatch *nextLeftChild;
   PartialMatch *prevLeftChild;
   PartialMatch *blockList;
   PartialMatch *nextBlocked;
   PartialMatch *prevBlocked;
   GenericMatch binds[1];
  };

/**************/
/* alphaMatch */
/**************/
struct alphaMatch
  {
   PatternEntity *matchingItem;
   MultifieldMarker *markers;
   AlphaMatch *next;
   unsigned long bucket;
  };

/******************************************************/
/* multifieldMarker: Used in the pattern matching     */
/*    process to mark the range of fields that the $? */
/*    and $?variables match because a single pattern  */
/*    restriction may span zero or more fields.       */
/******************************************************/
struct multifieldMarker
  {
   unsigned short whichField;
   union
     {
      void *whichSlot;
      unsigned short whichSlotNumber;
     } where;
    size_t startPosition;
    size_t range;
    MultifieldMarker *next;
   };

#define get_nth_pm_value(thePM,thePos) (thePM->binds[thePos].gm.theValue)
#define get_nth_pm_match(thePM,thePos) (thePM->binds[thePos].gm.theMatch)

#define set_nth_pm_value(thePM,thePos,theVal) (thePM->binds[thePos].gm.theValue = (void *) theVal)
#define set_nth_pm_match(thePM,thePos,theVal) (thePM->binds[thePos].gm.theMatch = theVal)

#endif /* _H_match */






