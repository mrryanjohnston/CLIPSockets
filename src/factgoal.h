   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 7.00  11/03/23            */
   /*                                                     */
   /*                FACT GOAL HEADER FILE                */
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
/*      7.00: Support for data driven backward chaining.     */
/*                                                           */
/*************************************************************/

#ifndef _H_factgoal

#pragma once

#define _H_factgoal

#include "entities.h"
#include "conscomp.h"
#include "tmpltdef.h"

typedef enum
  {
   RETRACT_ACTION,
   ASSERT_ACTION
  } GoalQueueAction;

struct queueItem
  {
   GoalQueueAction action;
   struct fact *theFact;
   struct partialMatch *theMatch;
   struct queueItem *nextInQueue;
  };

struct extractedInfo
  {
   unsigned short field;
   bool multifield;
   bool fromLeft;
   bool fromRight;
   unsigned short offset;
   CLIPSValue theValue;
   struct extractedInfo *next;
  };

   void                           AttachGoal(Environment *,struct joinNode *,struct partialMatch *,struct partialMatch *,bool);
   Fact                          *GenerateGoal(Environment *,struct joinNode *,struct partialMatch *);
   void                           UpdateGoalSupport(Environment *,struct partialMatch *,bool);
   void                           ProcessGoalQueue(Environment *);
   void                           AddToGoalQueue(Fact *,struct partialMatch *,GoalQueueAction);
   void                           TraceGoalToRule(Environment *,struct joinNode *,struct partialMatch *,const char *);

#endif /* _H_factgoal */
