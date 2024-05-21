   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 7.00  01/23/24            */
   /*                                                     */
   /*                FACT BUILD HEADER FILE               */
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
/*      6.30: Added support for hashed alpha memories.       */
/*                                                           */
/*            Added support for hashed comparisons to        */
/*            constants.                                     */
/*                                                           */
/*      6.32: Fixed _FACTBUILD_SOURCE_ typo.                 */
/*                                                           */
/*      6.40: Removed LOCALE definition.                     */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*      7.00: Support for non-reactive fact patterns.        */
/*                                                           */
/*************************************************************/

#ifndef _H_factbld

#pragma once

#define _H_factbld

struct factPatternNode;

#include "network.h"
#include "expressn.h"

struct factPatternNode
  {
   struct patternNodeHeader header;
   unsigned long bsaveID;
   unsigned short whichField;
   unsigned short whichSlot;
   unsigned short leaveFields;
   struct expr *networkTest;
   struct factPatternNode *nextLevel;
   struct factPatternNode *lastLevel;
   struct factPatternNode *leftNode;
   struct factPatternNode *rightNode;
   CLIPSBitMap *modifySlots;
  };

   void                           InitializeFactPatterns(Environment *);
   void                           DestroyFactPatternNetwork(Environment *,struct factPatternNode *);
   void                           IncrementalResetAddGoalExpressions(Environment *);
   void                           IncrementalResetGoals(Environment *);
   void                           ReleaseGoalUpdates(Environment *);
  
#endif /* _H_factbld */
