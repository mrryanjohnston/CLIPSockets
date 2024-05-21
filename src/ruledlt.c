   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 7.00  01/22/24             */
   /*                                                     */
   /*                 RULE DELETION MODULE                */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides routines for deleting a rule including  */
/*   freeing the defrule data structures and removing the    */
/*   appropriate joins from the join network.                */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Removed DYNAMIC_SALIENCE compilation flag.     */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            Added support for hashed memories.             */
/*                                                           */
/*            Fixed linkage issue when BLOAD_ONLY compiler   */
/*            flag is set to 1.                              */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*      7.00: Support for data driven backward chaining.     */
/*                                                           */
/*            Construct hashing for quick lookup.            */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if DEFRULE_CONSTRUCT

#include <stdio.h>
#include <string.h>

#include "agenda.h"
#if BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE
#include "bload.h"
#endif
#include "constrct.h"
#include "drive.h"
#include "engine.h"
#if DEFTEMPLATE_CONSTRUCT
#include "factgoal.h"
#endif
#include "envrnmnt.h"
#include "memalloc.h"
#include "pattern.h"
#include "reteutil.h"
#include "retract.h"

#include "ruledlt.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if (! RUN_TIME) && (! BLOAD_ONLY)
   static void                    RemoveIntranetworkLink(Environment *,struct joinNode *);
#endif
   static void                    DetachJoins(Environment *,struct joinNode *,bool);
   static void                    DetachJoinsDriver(Environment *,Defrule *,bool);

/**********************************************************************/
/* ReturnDefrule: Returns a defrule data structure and its associated */
/*   data structures to the memory manager. Note that the first       */
/*   disjunct of a rule is the only disjunct which allocates storage  */
/*   for the rule's dynamic salience and pretty print form (so these  */
/*   are only deallocated for the first disjunct).                    */
/**********************************************************************/
void ReturnDefrule(
  Environment *theEnv,
  Defrule *theDefrule)
  {
#if (! RUN_TIME) && (! BLOAD_ONLY)
   bool first = true;
   Defrule *nextPtr, *tmpPtr;

   if (theDefrule == NULL) return;
   
   RemoveConstructFromHashMap(theEnv,&theDefrule->header,theDefrule->header.whichModule);

   /*======================================*/
   /* If a rule is redefined, then we want */
   /* to save its breakpoint status.       */
   /*======================================*/

#if DEBUGGING_FUNCTIONS
   DefruleData(theEnv)->DeletedRuleDebugFlags = 0;
   if (theDefrule->afterBreakpoint) BitwiseSet(DefruleData(theEnv)->DeletedRuleDebugFlags,0);
   if (theDefrule->watchActivation) BitwiseSet(DefruleData(theEnv)->DeletedRuleDebugFlags,1);
   if (theDefrule->watchFiring) BitwiseSet(DefruleData(theEnv)->DeletedRuleDebugFlags,2);
#endif

   /*================================*/
   /* Clear the agenda of all the    */
   /* activations added by the rule. */
   /*================================*/

   ClearRuleFromAgenda(theEnv,theDefrule);

   /*======================*/
   /* Get rid of the rule. */
   /*======================*/

   while (theDefrule != NULL)
     {
      /*================================================*/
      /* Remove the rule's joins from the join network. */
      /*================================================*/

      DetachJoinsDriver(theEnv,theDefrule,false);

      /*=============================================*/
      /* If this is the first disjunct, get rid of   */
      /* the dynamic salience and pretty print form. */
      /*=============================================*/

      if (first)
        {
         if (theDefrule->dynamicSalience != NULL)
          {
           ExpressionDeinstall(theEnv,theDefrule->dynamicSalience);
           ReturnPackedExpression(theEnv,theDefrule->dynamicSalience);
           theDefrule->dynamicSalience = NULL;
          }
         if (theDefrule->header.ppForm != NULL)
           {
            rm(theEnv,(void *) theDefrule->header.ppForm,strlen(theDefrule->header.ppForm) + 1);
            theDefrule->header.ppForm = NULL;

            /*=======================================================*/
            /* All of the rule disjuncts share the same pretty print */
            /* form, so we want to avoid deleting it again.          */
            /*=======================================================*/

            for (tmpPtr = theDefrule->disjunct; tmpPtr != NULL; tmpPtr = tmpPtr->disjunct)
              { tmpPtr->header.ppForm = NULL; }
           }

         first = false;
        }

      /*===========================*/
      /* Get rid of any user data. */
      /*===========================*/

      if (theDefrule->header.usrData != NULL)
        { ClearUserDataList(theEnv,theDefrule->header.usrData); }

      /*===========================================*/
      /* Decrement the count for the defrule name. */
      /*===========================================*/

      ReleaseLexeme(theEnv,theDefrule->header.name);

      /*========================================*/
      /* Get rid of the the rule's RHS actions. */
      /*========================================*/

      if (theDefrule->actions != NULL)
        {
         ExpressionDeinstall(theEnv,theDefrule->actions);
         ReturnPackedExpression(theEnv,theDefrule->actions);
        }

      /*===============================*/
      /* Move on to the next disjunct. */
      /*===============================*/

      nextPtr = theDefrule->disjunct;
      rtn_struct(theEnv,defrule,theDefrule);
      theDefrule = nextPtr;
     }

   /*==========================*/
   /* Free up partial matches. */
   /*==========================*/

   if (EngineData(theEnv)->ExecutingRule == NULL) FlushGarbagePartialMatches(theEnv);

#if DEFTEMPLATE_CONSTRUCT
   ProcessGoalQueue(theEnv);
#endif

#endif
  }

/********************************************************/
/* DestroyDefrule: Action used to remove defrules       */
/*   as a result of DestroyEnvironment.                 */
/********************************************************/
void DestroyDefrule(
  Environment *theEnv,
  Defrule *theDefrule)
  {
   Defrule *nextDisjunct;
   bool first = true;

   if (theDefrule == NULL) return;

   while (theDefrule != NULL)
     {
      DetachJoinsDriver(theEnv,theDefrule,true);

      if (first)
        {
#if (! BLOAD_ONLY) && (! RUN_TIME)
         if (theDefrule->dynamicSalience != NULL)
           { ReturnPackedExpression(theEnv,theDefrule->dynamicSalience); }

         if (theDefrule->header.ppForm != NULL)
           {
            Defrule *tmpPtr;

            rm(theEnv,(void *) theDefrule->header.ppForm,strlen(theDefrule->header.ppForm) + 1);

            /*=======================================================*/
            /* All of the rule disjuncts share the same pretty print */
            /* form, so we want to avoid deleting it again.          */
            /*=======================================================*/

            for (tmpPtr = theDefrule->disjunct; tmpPtr != NULL; tmpPtr = tmpPtr->disjunct)
              { tmpPtr->header.ppForm = NULL; }
           }
#endif

         first = false;
        }

      if (theDefrule->header.usrData != NULL)
        { ClearUserDataList(theEnv,theDefrule->header.usrData); }

#if (! BLOAD_ONLY) && (! RUN_TIME)
      if (theDefrule->actions != NULL)
        { ReturnPackedExpression(theEnv,theDefrule->actions); }
#endif

      nextDisjunct = theDefrule->disjunct;

#if (! BLOAD_ONLY) && (! RUN_TIME)
      rtn_struct(theEnv,defrule,theDefrule);
#endif

      theDefrule = nextDisjunct;
     }
  }

/**********************/
/* DetachJoinsDriver: */
/**********************/
static void DetachJoinsDriver(
  Environment *theEnv,
  Defrule *theRule,
  bool destroy)
  {
   struct joinNode *join;

   /*==================================*/
   /* Find the last join for the rule. */
   /*==================================*/

   join = theRule->lastJoin;
   theRule->lastJoin = NULL;
   if (join == NULL) return;

   /*===================================================*/
   /* Remove the activation link from the last join. If */
   /* there are joins below this join, then all of the  */
   /* joins for this rule were shared with another rule */
   /* and thus no joins can be deleted.                 */
   /*===================================================*/

   join->ruleToActivate = NULL;
   if (join->nextLinks != NULL) return;

   DetachJoins(theEnv,join,destroy);
  }

/**********************************************************************/
/* DetachJoins: Removes a join node and all of its parent nodes from  */
/*   the join network. Nodes are only removed if they are no required */
/*   by other rules (the same join can be shared by multiple rules).  */
/*   Any partial matches associated with the join are also removed.   */
/*   A rule's joins are typically removed by removing the bottom most */
/*   join used by the rule and then removing any parent joins which   */
/*   are not shared by other rules.                                   */
/**********************************************************************/
static void DetachJoins(
  Environment *theEnv,
  struct joinNode *join,
  bool destroy)
  {
   struct joinNode *prevJoin, *rightJoin;
   unsigned lastMark;

   /*===========================*/
   /* Begin removing the joins. */
   /*===========================*/

   while (join != NULL)
     {
      if (join->marked) return;

      /*==========================================================*/
      /* Remember the join "above" this join (the one that enters */
      /* from the left). If the join is entered from the right by */
      /* another join, remember the right entering join as well.  */
      /*==========================================================*/

      prevJoin = join->lastLevel;
      if (join->joinFromTheRight)
        { rightJoin = (struct joinNode *) join->rightSideEntryStructure; }
      else
        { rightJoin = NULL; }

      /*=================================================*/
      /* If the join was attached to a pattern, remove   */
      /* any structures associated with the pattern that */
      /* are no longer needed.                           */
      /*=================================================*/

#if (! RUN_TIME) && (! BLOAD_ONLY)
      if (! destroy)
        {
         if ((join->rightSideEntryStructure != NULL) && (join->joinFromTheRight == false))
           { RemoveIntranetworkLink(theEnv,join); }
        }
#endif

      /*======================================*/
      /* Remove any partial matches contained */
      /* in the beta memory of the join.      */
      /*======================================*/

      if (destroy)
        {
         DestroyBetaMemory(theEnv,join,LHS);
         DestroyBetaMemory(theEnv,join,RHS);
        }
      else
        {
         FlushBetaMemory(theEnv,join,LHS);
         FlushBetaMemory(theEnv,join,RHS);
        }

      ReturnLeftMemory(theEnv,join);
      ReturnRightMemory(theEnv,join);

      /*===================================*/
      /* Remove the expressions associated */
      /* with the join.                    */
      /*===================================*/

#if (! RUN_TIME) && (! BLOAD_ONLY)
      if (! destroy)
        {
         RemoveHashedExpression(theEnv,join->networkTest);
         RemoveHashedExpression(theEnv,join->secondaryNetworkTest);
         RemoveHashedExpression(theEnv,join->goalExpression);
         RemoveHashedExpression(theEnv,join->leftHash);
         RemoveHashedExpression(theEnv,join->rightHash);
        }
#endif

      /*============================*/
      /* Fix the right prime links. */
      /*============================*/

      if (join->firstJoin && (join->rightSideEntryStructure == NULL))
        { DefruleData(theEnv)->RightPrimeJoins = DetachLink(theEnv,DefruleData(theEnv)->RightPrimeJoins,join); }

      /*===========================*/
      /* Fix the left prime links. */
      /*===========================*/

      if (join->firstJoin && (join->patternIsNegated || join->joinFromTheRight) && (! join->patternIsExists))
        { DefruleData(theEnv)->LeftPrimeJoins = DetachLink(theEnv,DefruleData(theEnv)->LeftPrimeJoins,join); }
        
      /*===========================*/
      /* Fix the goal prime links. */
      /*===========================*/

      if (join->firstJoin && join->goalJoin)
        { DefruleData(theEnv)->GoalPrimeJoins = DetachLink(theEnv,DefruleData(theEnv)->GoalPrimeJoins,join); }
        
      /*==================================================*/
      /* Remove the link to the join from the join above. */
      /*==================================================*/

      if (prevJoin != NULL)
        { prevJoin->nextLinks = DetachLink(theEnv,prevJoin->nextLinks,join); }

      /*==========================================*/
      /* Remove the right join link if it exists. */
      /*==========================================*/

      if (rightJoin != NULL)
        {
         rightJoin->nextLinks = DetachLink(theEnv,rightJoin->nextLinks,join);

         if ((rightJoin->nextLinks == NULL) &&
             (rightJoin->ruleToActivate == NULL))
           {
            if (prevJoin != NULL)
              {
               lastMark = prevJoin->marked;
               prevJoin->marked = true;
               DetachJoins(theEnv,rightJoin,destroy);
               prevJoin->marked = lastMark;
              }
            else
              { DetachJoins(theEnv,rightJoin,destroy); }
           }
        }

      /*==================*/
      /* Delete the join. */
      /*==================*/

#if (! RUN_TIME) && (! BLOAD_ONLY)
      rtn_struct(theEnv,joinNode,join);
#endif

      /*===========================================================*/
      /* Move on to the next join to be removed. All the joins of  */
      /* a rule can be deleted by following the right joins links  */
      /* (when these links exist) and then following the left join */
      /* links. This works because if join A enters join B from    */
      /* the right, the right/left links of join A eventually lead */
      /* to the join which enters join B from the left.            */
      /*===========================================================*/

      if (prevJoin == NULL)
        { return; }
      else if (prevJoin->ruleToActivate != NULL)
        { return; }
      else if (prevJoin->nextLinks == NULL)
        { join = prevJoin; }
      else
        { return; }
     }
  }

/***************/
/* DetachLink: */
/***************/
struct joinLink *DetachLink(
  Environment *theEnv,
  struct joinLink *first,
  struct joinNode *join)
  {
   struct joinLink *lastLink, *theLink;

   lastLink = NULL;
   
   for (theLink = first; theLink != NULL; theLink = theLink->next)
     {
      if (theLink->join == join)
        {
         if (lastLink == NULL)
           { first = theLink->next; }
         else
           { lastLink->next = theLink->next; }

#if (! RUN_TIME) && (! BLOAD_ONLY)
         rtn_struct(theEnv,joinLink,theLink);
#endif

         return first;
        }
      
      lastLink = theLink;
     }
     
   return first;
  }
        
#if (! RUN_TIME) && (! BLOAD_ONLY)

/***********************************************************************/
/* RemoveIntranetworkLink: Removes the link between a join node in the */
/*   join network and its corresponding pattern node in the pattern    */
/*   network. If the pattern node is then no longer associated with    */
/*   any other joins, it is removed using the function DetachPattern.  */
/***********************************************************************/
static void RemoveIntranetworkLink(
  Environment *theEnv,
  struct joinNode *join)
  {
   struct patternNodeHeader *patternPtr;
   struct joinNode *joinPtr, *lastJoin;

   /*================================================*/
   /* Determine the pattern that enters this join.   */
   /* Determine the list of joins which this pattern */
   /* enters from the right.                         */
   /*================================================*/

   patternPtr = (struct patternNodeHeader *) join->rightSideEntryStructure;
   joinPtr = patternPtr->entryJoin;
   lastJoin = NULL;

   /*=================================================*/
   /* Loop through the list of joins that the pattern */
   /* enters until the join being removed is found.   */
   /* Remove this join from the list.                 */
   /*=================================================*/

   while (joinPtr != NULL)
     {
      if (joinPtr == join)
        {
         if (lastJoin == NULL)
           { patternPtr->entryJoin = joinPtr->rightMatchNode; }
         else
           { lastJoin->rightMatchNode = joinPtr->rightMatchNode; }

         joinPtr = NULL;
        }
      else
        {
         lastJoin = joinPtr;
         joinPtr = joinPtr->rightMatchNode;
        }
     }

   /*===================================================*/
   /* If the terminal node of the pattern doesn't point */
   /* to any joins, then start removing the pattern.    */
   /*===================================================*/

   if (patternPtr->entryJoin == NULL)
     { DetachPattern(theEnv,join->rhsType,patternPtr); }
  }

#endif /* (! RUN_TIME) && (! BLOAD_ONLY) */

#endif /* DEFRULE_CONSTRUCT */



