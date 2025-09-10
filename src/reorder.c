   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 7.00  10/13/23             */
   /*                                                     */
   /*                    REORDER MODULE                   */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides routines necessary for converting the   */
/*   the LHS of a rule into an appropriate form suitable for */
/*   the KB Rete topology. This includes transforming the    */
/*   LHS so there is at most one "or" CE (and this is the    */
/*   first CE of the LHS if it is used), adding initial      */
/*   patterns to the LHS (if no LHS is specified or a "test" */
/*   or "not" CE is the first pattern within an "and" CE),   */
/*   removing redundant CEs, and determining appropriate     */
/*   information on nesting for implementing joins from the  */
/*   right.                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Support for join network changes.              */
/*                                                           */
/*            Changes to the algorithm for processing        */
/*            not/and CE groups.                             */
/*                                                           */
/*            Additional optimizations for combining         */
/*            conditional elements.                          */
/*                                                           */
/*            Added support for hashed alpha memories.       */
/*                                                           */
/*      6.31: Fixed crash bug that occurred from             */
/*            AssignPatternIndices incorrectly               */
/*            assigning the wrong join depth to              */
/*            multiply nested nand  groups.                  */
/*                                                           */
/*            Removed the marked flag used for not/and       */
/*            unification.                                   */
/*                                                           */
/*            Fix for incorrect join depth computed by       */
/*            AssignPatternIndices for not/and groups.       */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            Removed initial-fact support.                  */
/*                                                           */
/*      7.00: Support for data driven backward chaining.     */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if (! RUN_TIME) && (! BLOAD_ONLY) && DEFRULE_CONSTRUCT

#include <stdio.h>

#include "cstrnutl.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#include "memalloc.h"
#include "pattern.h"
#include "prntutil.h"
#include "router.h"
#include "rulelhs.h"

#if DEVELOPER && DEBUGGING_FUNCTIONS
#include "watch.h"
#include "rulepsr.h"
#endif

#include "reorder.h"

struct variableReference
   {
    CLIPSLexeme *name;
    int depth;
    struct variableReference *next;
   };

struct groupReference
   {
    struct lhsParseNode *theGroup;
    int depth;
    struct groupReference *next;
   };

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static struct lhsParseNode    *ReverseAndOr(Environment *,struct lhsParseNode *,struct lhsParseNode *,int);
   static struct lhsParseNode    *PerformReorder1(Environment *,struct lhsParseNode *,bool *,int);
   static struct lhsParseNode    *PerformReorder2(Environment *,struct lhsParseNode *,bool *,int);
   static struct lhsParseNode    *CompressCEs(Environment *,struct lhsParseNode *,bool *,int);
   static void                    IncrementNandDepth(Environment *,struct lhsParseNode *,bool);
   static struct lhsParseNode    *CreateInitialPattern(Environment *);
   static struct lhsParseNode    *ReorderDriver(Environment *,struct lhsParseNode *,bool *,int,int);
   static struct lhsParseNode    *AddRemainingInitialPatterns(Environment *,struct lhsParseNode *);
   static struct lhsParseNode    *AssignPatternIndices(struct lhsParseNode *,short,int,unsigned short);
   static void                    PropagateIndexSlotPatternValues(struct lhsParseNode *,short,unsigned short,
                                                                  CLIPSLexeme *,unsigned short);
   static void                    PropagateJoinDepth(struct lhsParseNode *,unsigned short);
   static void                    PropagateNandDepth(struct lhsParseNode *,int,int);
   static void                    MarkExistsNands(struct lhsParseNode *);
   static unsigned short          PropagateWhichCE(struct lhsParseNode *,unsigned short);
   /*
   static void                    PrintNodes(void *,const char *,struct lhsParseNode *);
   */

/********************************************/
/* ReorderPatterns: Reorders a group of CEs */
/*   to accommodate KB Rete topology.       */
/********************************************/
struct lhsParseNode *ReorderPatterns(
  Environment *theEnv,
  struct lhsParseNode *theLHS,
  bool *anyChange)
  {
   struct lhsParseNode *newLHS, *tempLHS, *lastLHS;

   /*=============================================*/
   /* If the LHS of the rule was left unspecified */
   /* (e.g., (defrule x => ...)), then nothing    */
   /* more needs to be done.                      */
   /*=============================================*/

   if (theLHS == NULL) return(theLHS);

   /*===========================================================*/
   /* The LHS of a rule is enclosed within an implied "and" CE. */
   /*===========================================================*/

   newLHS = GetLHSParseNode(theEnv);
   newLHS->pnType = AND_CE_NODE;
   newLHS->right = theLHS;

   /*==============================================================*/
   /* Mark the nodes to indicate which CE they're associated with. */
   /*==============================================================*/

   PropagateWhichCE(newLHS,0);

   /*=======================================================*/
   /* Reorder the patterns to support the KB Rete topology. */
   /*=======================================================*/

   newLHS = ReorderDriver(theEnv,newLHS,anyChange,1,1);
   newLHS = ReorderDriver(theEnv,newLHS,anyChange,2,1);

   /*===========================================*/
   /* The top level and CE may have disappeared */
   /* as a result of pattern compression.       */
   /*===========================================*/

   if (newLHS->pnType == OR_CE_NODE)
     {
      for (tempLHS = newLHS->right, lastLHS = NULL;
           tempLHS != NULL;
           lastLHS = tempLHS, tempLHS = tempLHS->bottom)
        {
         if (tempLHS->pnType != AND_CE_NODE)
           {
            theLHS = GetLHSParseNode(theEnv);
            theLHS->pnType = AND_CE_NODE;
            theLHS->right = tempLHS;
            theLHS->bottom = tempLHS->bottom;
            tempLHS->bottom = NULL;
            if (lastLHS == NULL)
              { newLHS->right = theLHS; }
            else
              { lastLHS->bottom = theLHS; }
            tempLHS = theLHS;
           }
        }
     }
   else if (newLHS->pnType != AND_CE_NODE)
     {
      theLHS = newLHS;
      newLHS = GetLHSParseNode(theEnv);
      newLHS->pnType = AND_CE_NODE;
      newLHS->right = theLHS;
     }

   /*================================================*/
   /* Mark exist not/and groups within the patterns. */
   /*================================================*/

   if (newLHS->pnType == OR_CE_NODE)
     {
      for (theLHS = newLHS->right;
           theLHS != NULL;
           theLHS = theLHS->bottom)
        { MarkExistsNands(theLHS->right); }
     }
   else
     { MarkExistsNands(newLHS->right); }

   /*=====================================================*/
   /* Add initial patterns where needed (such as before a */
   /* "test" CE or "not" CE which is the first CE within  */
   /* an "and" CE).                                       */
   /*=====================================================*/

   AddInitialPatterns(theEnv,newLHS);

   /*===========================================================*/
   /* Number the user specified patterns. Patterns added while  */
   /* analyzing the rule are not numbered so that there is no   */
   /* confusion when an error message refers to a CE. Also      */
   /* propagate field and slot values throughout each pattern.  */
   /*===========================================================*/

   if (newLHS->pnType == OR_CE_NODE) theLHS = newLHS->right;
   else theLHS = newLHS;

   for (;
        theLHS != NULL;
        theLHS = theLHS->bottom)
     { AssignPatternIndices(theLHS->right,1,1,0); }

   /*===========================*/
   /* Return the processed LHS. */
   /*===========================*/

   return(newLHS);
  }

/******************************************/
/* ReorderDriver: Reorders a group of CEs */
/*   to accommodate KB Rete topology.     */
/******************************************/
static struct lhsParseNode *ReorderDriver(
  Environment *theEnv,
  struct lhsParseNode *theLHS,
  bool *anyChange,
  int pass,
  int depth)
  {
   struct lhsParseNode *argPtr;
   struct lhsParseNode *before, *save;
   bool change, newChange;
   *anyChange = false;

   /*===================================*/
   /* Continue processing the LHS until */
   /* no more changes have been made.   */
   /*===================================*/

   change = true;
   while (change)
     {
      /*==================================*/
      /* No change yet on this iteration. */
      /*==================================*/

      change = false;

      /*=======================================*/
      /* Reorder the current level of the LHS. */
      /*=======================================*/

      if ((theLHS->pnType == AND_CE_NODE) ||
          (theLHS->pnType == NOT_CE_NODE) ||
          (theLHS->pnType == OR_CE_NODE))
        {
         if (pass == 1) theLHS = PerformReorder1(theEnv,theLHS,&newChange,depth);
         else theLHS = PerformReorder2(theEnv,theLHS,&newChange,depth);

         if (newChange)
           {
            *anyChange = true;
            change = true;
           }

         theLHS = CompressCEs(theEnv,theLHS,&newChange,depth);

         if (newChange)
           {
            *anyChange = true;
            change = true;
           }
        }

      /*=====================================================*/
      /* Recursively reorder CEs at lower levels in the LHS. */
      /*=====================================================*/

      before = NULL;
      argPtr = theLHS->right;

      while (argPtr != NULL)
        {
         /*==================================*/
         /* Remember the next CE to reorder. */
         /*==================================*/

         save = argPtr->bottom;

         /*============================================*/
         /* Reorder the current CE at the lower level. */
         /*============================================*/

         if ((argPtr->pnType == AND_CE_NODE) ||
             (argPtr->pnType == NOT_CE_NODE) ||
             (argPtr->pnType == OR_CE_NODE))
           {
            if (before == NULL)
              {
               argPtr->bottom = NULL;
               theLHS->right = ReorderDriver(theEnv,argPtr,&newChange,pass,depth+1);
               theLHS->right->bottom = save;
               before = theLHS->right;
              }
            else
              {
               argPtr->bottom = NULL;
               before->bottom = ReorderDriver(theEnv,argPtr,&newChange,pass,depth+1);
               before->bottom->bottom = save;
               before = before->bottom;
              }

            if (newChange)
              {
               *anyChange = true;
               change = true;
              }
           }
         else
           { before = argPtr; }

         /*====================================*/
         /* Move on to the next CE to reorder. */
         /*====================================*/

         argPtr = save;
        }
     }

   /*===========================*/
   /* Return the reordered LHS. */
   /*===========================*/

   return(theLHS);
  }

/********************/
/* MarkExistsNands: */
/********************/
static void MarkExistsNands(
  struct lhsParseNode *theLHS)
  {
   int currentDepth = 1;
   struct lhsParseNode *tmpLHS;

   while (theLHS != NULL)
     {
      if (IsExistsSubjoin(theLHS,currentDepth))
        {
         theLHS->existsNand = true;

         for (tmpLHS = theLHS;
              tmpLHS != NULL;
              tmpLHS = tmpLHS->bottom)
           {
            tmpLHS->beginNandDepth--;
            if (tmpLHS->endNandDepth <= currentDepth)
              { break; }
            else
              { tmpLHS->endNandDepth--; }
           }
        }

      currentDepth = theLHS->endNandDepth;
      theLHS = theLHS->bottom;
     }
  }

/****************************************************************/
/* AddInitialPatterns: Add initial patterns to CEs where needed */
/*   (such as before a "test" CE or "not" CE which is the first */
/*   CE within an "and" CE).                                    */
/****************************************************************/
void AddInitialPatterns(
  Environment *theEnv,
  struct lhsParseNode *theLHS)
  {
   struct lhsParseNode *thePattern;

   /*====================================================*/
   /* If there are multiple disjuncts for the rule, then */
   /* add initial patterns to each disjunct separately.  */
   /*====================================================*/

   if (theLHS->pnType == OR_CE_NODE)
     {
      for (thePattern = theLHS->right;
           thePattern != NULL;
           thePattern = thePattern->bottom)
        { AddInitialPatterns(theEnv,thePattern); }

      return;
     }

   /*================================*/
   /* Handle the remaining patterns. */
   /*================================*/

   theLHS->right = AddRemainingInitialPatterns(theEnv,theLHS->right);
  }

/***********************************************************/
/* PerformReorder1: Reorders a group of CEs to accommodate */
/*   KB Rete topology. The first pass of this function     */
/*   transforms or CEs into equivalent forms.              */
/***********************************************************/
static struct lhsParseNode *PerformReorder1(
  Environment *theEnv,
  struct lhsParseNode *theLHS,
  bool *newChange,
  int depth)
  {
   struct lhsParseNode *argPtr, *lastArg, *nextArg;
   struct lhsParseNode *tempArg, *newNode;
   int count;
   bool change;

   /*======================================================*/
   /* Loop through the CEs as long as changes can be made. */
   /*======================================================*/

   change = true;
   *newChange = false;

   while (change)
     {
      change = false;
      count = 1;
      lastArg = NULL;

      for (argPtr = theLHS->right;
           argPtr != NULL;)
        {
         /*=============================================================*/
         /* Convert and/or CE combinations into or/and CE combinations. */
         /*=============================================================*/

         if ((theLHS->pnType == AND_CE_NODE) && (argPtr->pnType == OR_CE_NODE))
           {
            theLHS = ReverseAndOr(theEnv,theLHS,argPtr->right,count);

            change = true;
            *newChange = true;
            break;
           }

         /*==============================================================*/
         /* Convert not/or CE combinations into and/not CE combinations. */
         /*==============================================================*/

         else if ((theLHS->pnType == NOT_CE_NODE) && (argPtr->pnType == OR_CE_NODE))
           {
            change = true;
            *newChange = true;

            tempArg = argPtr->right;

            argPtr->right = NULL;
            argPtr->bottom = NULL;
            ReturnLHSParseNodes(theEnv,argPtr);
            theLHS->pnType = AND_CE_NODE;
            theLHS->right = tempArg;

            while (tempArg != NULL)
              {
               newNode = GetLHSParseNode(theEnv);
               CopyLHSParseNode(theEnv,newNode,tempArg,false);
               newNode->right = tempArg->right;
               newNode->bottom = NULL;

               tempArg->pnType = NOT_CE_NODE;
               tempArg->negated = false;
               tempArg->exists = false;
               tempArg->existsNand = false;
               tempArg->logical = false;
               tempArg->value = NULL;
               tempArg->expression = NULL;
               tempArg->secondaryExpression = NULL;
               tempArg->right = newNode;

               tempArg = tempArg->bottom;
              }

            break;
           }

         /*=====================================*/
         /* Remove duplication of or CEs within */
         /* or CEs and and CEs within and CEs.  */
         /*=====================================*/

         else if (((theLHS->pnType == OR_CE_NODE) && (argPtr->pnType == OR_CE_NODE)) ||
                  ((theLHS->pnType == AND_CE_NODE) && (argPtr->pnType == AND_CE_NODE)))
           {
            if (argPtr->logical) theLHS->logical = true;

            change = true;
            *newChange = true;
            tempArg = argPtr->right;
            nextArg = argPtr->bottom;
            argPtr->right = NULL;
            argPtr->bottom = NULL;
            ReturnLHSParseNodes(theEnv,argPtr);

            if (lastArg == NULL)
              { theLHS->right = tempArg; }
            else
              { lastArg->bottom = tempArg; }

            argPtr = tempArg;
            while (tempArg->bottom != NULL) tempArg = tempArg->bottom;
            tempArg->bottom = nextArg;
           }

         /*===================================================*/
         /* If no changes are needed, move on to the next CE. */
         /*===================================================*/

         else
           {
            count++;
            lastArg = argPtr;
            argPtr = argPtr->bottom;
           }
        }
     }

   /*===========================*/
   /* Return the reordered LHS. */
   /*===========================*/

   return(theLHS);
  }

/***********************************************************/
/* PerformReorder2: Reorders a group of CEs to accommodate */
/*   KB Rete topology. The second pass performs all other  */
/*   transformations not associated with the or CE.        */
/***********************************************************/
static struct lhsParseNode *PerformReorder2(
  Environment *theEnv,
  struct lhsParseNode *theLHS,
  bool *newChange,
  int depth)
  {
   struct lhsParseNode *argPtr;
   bool change;

   /*======================================================*/
   /* Loop through the CEs as long as changes can be made. */
   /*======================================================*/

   change = true;
   *newChange = false;

   while (change)
     {
      change = false;

      for (argPtr = theLHS->right;
           argPtr != NULL;)
        {
         /*=======================================================*/
         /* A sequence of three not CEs grouped within each other */
         /* can be replaced with a single not CE. For example,    */
         /* (not (not (not (a)))) can be replaced with (not (a)). */
         /*=======================================================*/

         if ((theLHS->pnType == NOT_CE_NODE) &&
             (argPtr->pnType == NOT_CE_NODE) &&
             (argPtr->right != NULL) &&
             (argPtr->right->pnType == NOT_CE_NODE))
           {
            change = true;
            *newChange = true;

            theLHS->right = argPtr->right->right;

            argPtr->right->right = NULL;
            ReturnLHSParseNodes(theEnv,argPtr);

            break;
           }

         /*==========================================*/
         /* Replace two not CEs containing a pattern */
         /* CE with an exists pattern CE.            */
         /*==========================================*/

         else if ((theLHS->pnType == NOT_CE_NODE) &&
                  (argPtr->pnType == NOT_CE_NODE) &&
                  (argPtr->right != NULL) &&
                  (argPtr->right->pnType == PATTERN_CE_NODE))
           {
            change = true;
            *newChange = true;

            CopyLHSParseNode(theEnv,theLHS,argPtr->right,false);

            theLHS->negated = true;
            theLHS->exists = true;
            theLHS->existsNand = false;
            theLHS->right = argPtr->right->right;

            argPtr->right->networkTest = NULL;
            argPtr->right->externalNetworkTest = NULL;
            argPtr->right->secondaryNetworkTest = NULL;
            argPtr->right->externalRightHash = NULL;
            argPtr->right->externalLeftHash = NULL;
            argPtr->right->leftHash = NULL;
            argPtr->right->rightHash = NULL;
            argPtr->right->betaHash = NULL;
            argPtr->right->expression = NULL;
            argPtr->right->secondaryExpression = NULL;
            argPtr->right->userData = NULL;
            argPtr->right->right = NULL;
            argPtr->right->bottom = NULL;
            ReturnLHSParseNodes(theEnv,argPtr);
            break;
           }

         /*======================================*/
         /* Replace not CEs containing a pattern */
         /* CE with a negated pattern CE.        */
         /*======================================*/

         else if ((theLHS->pnType == NOT_CE_NODE) && (argPtr->pnType == PATTERN_CE_NODE))
           {
            change = true;
            *newChange = true;

            CopyLHSParseNode(theEnv,theLHS,argPtr,false);

            theLHS->negated = true;
            theLHS->exists = false;
            theLHS->existsNand = false;
            theLHS->right = argPtr->right;

            argPtr->networkTest = NULL;
            argPtr->externalNetworkTest = NULL;
            argPtr->secondaryNetworkTest = NULL;
            argPtr->externalRightHash = NULL;
            argPtr->externalLeftHash = NULL;
            argPtr->constantSelector = NULL;
            argPtr->constantValue = NULL;
            argPtr->leftHash = NULL;
            argPtr->rightHash = NULL;
            argPtr->betaHash = NULL;
            argPtr->expression = NULL;
            argPtr->secondaryExpression = NULL;
            argPtr->userData = NULL;
            argPtr->right = NULL;
            argPtr->bottom = NULL;
            ReturnLHSParseNodes(theEnv,argPtr);
            break;
           }

         /*============================================================*/
         /* Replace "and" and "not" CEs contained within a not CE with */
         /* just the and CE, but increment the nand depths of the      */
         /* pattern contained within.                                  */
         /*============================================================*/

         else if ((theLHS->pnType == NOT_CE_NODE) &&
                  ((argPtr->pnType == AND_CE_NODE) ||  (argPtr->pnType == NOT_CE_NODE)))
           {
            change = true;
            *newChange = true;

            theLHS->pnType = argPtr->pnType;

            theLHS->negated = argPtr->negated;
            theLHS->exists = argPtr->exists;
            theLHS->existsNand = argPtr->existsNand;
            theLHS->value = argPtr->value;
            theLHS->logical = argPtr->logical;
            theLHS->right = argPtr->right;
            argPtr->right = NULL;
            argPtr->bottom = NULL;
            ReturnLHSParseNodes(theEnv,argPtr);

            IncrementNandDepth(theEnv,theLHS->right,true);
            break;
           }

         /*===================================================*/
         /* If no changes are needed, move on to the next CE. */
         /*===================================================*/

         else
           {
            argPtr = argPtr->bottom;
           }
        }
     }

   /*===========================*/
   /* Return the reordered LHS. */
   /*===========================*/

   return(theLHS);
  }

/**************************************************/
/* ReverseAndOr: Switches and/or CEs into         */
/*   equivalent or/and CEs. For example:          */
/*                                                */
/*     (and (or a b) (or c d))                    */
/*                                                */
/*   would be converted to                        */
/*                                                */
/*     (or (and a (or c d)) (and b (or c d))),    */
/*                                                */
/*   if the "or" CE being expanded was (or a b).  */
/**************************************************/
static struct lhsParseNode *ReverseAndOr(
  Environment *theEnv,
  struct lhsParseNode *listOfCEs,
  struct lhsParseNode *orCE,
  int orPosition)
  {
   int count;
   struct lhsParseNode *listOfExpandedOrCEs = NULL;
   struct lhsParseNode *lastExpandedOrCE = NULL;
   struct lhsParseNode *copyOfCEs, *replaceCE;

   /*========================================================*/
   /* Loop through each of the CEs contained within the "or" */
   /* CE that is being expanded into the enclosing "and" CE. */
   /*========================================================*/

   while (orCE != NULL)
     {
      /*===============================*/
      /* Make a copy of the and/or CE. */
      /*===============================*/

      copyOfCEs = CopyLHSParseNodes(theEnv,listOfCEs);

      /*====================================================*/
      /* Get a pointer to the "or" CE being expanded in the */
      /* copy just made based on the position of the "or"   */
      /* CE in the original and/or CE (e.g., 1st, 2nd).     */
      /*====================================================*/

      for (count = 1, replaceCE = copyOfCEs->right;
           count != orPosition;
           count++, replaceCE = replaceCE->bottom)
        { /* Do Nothing*/ }

      /*===================================================*/
      /* Delete the contents of the "or" CE being expanded */
      /* in the copy of the and/or CE. From the example    */
      /* above, (and (or a b) (or c d)) would be replaced  */
      /* with (and (or) (or c d)). Note that the "or" CE   */
      /* is still left as a placeholder.                   */
      /*===================================================*/

      ReturnLHSParseNodes(theEnv,replaceCE->right);

      /*======================================================*/
      /* Copy the current CE being examined in the "or" CE to */
      /* the placeholder left in the and/or CE. From the      */
      /* example above, (and (or) (or c d)) would be replaced */
      /* with (and a (or c d)) if the "a" pattern from the    */
      /* "or" CE was being examined or (and b (or c d)) if    */
      /* the "b" pattern from the "or" CE was being examined. */
      /*======================================================*/

      CopyLHSParseNode(theEnv,replaceCE,orCE,true);
      replaceCE->right = CopyLHSParseNodes(theEnv,orCE->right);

      /*====================================*/
      /* Add the newly expanded "and" CE to */
      /* the list of CEs already expanded.  */
      /*====================================*/

      if (lastExpandedOrCE == NULL)
        {
         listOfExpandedOrCEs = copyOfCEs;
         copyOfCEs->bottom = NULL;
         lastExpandedOrCE = copyOfCEs;
        }
      else
        {
         lastExpandedOrCE->bottom = copyOfCEs;
         copyOfCEs->bottom = NULL;
         lastExpandedOrCE = copyOfCEs;
        }

      /*=======================================================*/
      /* Move on to the next CE in the "or" CE being expanded. */
      /*=======================================================*/

      orCE = orCE->bottom;
     }

   /*=====================================================*/
   /* Release the original and/or CE list to free memory. */
   /*=====================================================*/

   ReturnLHSParseNodes(theEnv,listOfCEs);

   /*================================================*/
   /* Wrap an or CE around the list of expanded CEs. */
   /*================================================*/

   copyOfCEs = GetLHSParseNode(theEnv);
   copyOfCEs->pnType = OR_CE_NODE;
   copyOfCEs->right = listOfExpandedOrCEs;

   /*================================*/
   /* Return the newly expanded CEs. */
   /*================================*/

   return(copyOfCEs);
  }

/****************/
/* CompressCEs: */
/****************/
static struct lhsParseNode *CompressCEs(
  Environment *theEnv,
  struct lhsParseNode *theLHS,
  bool *newChange,
  int depth)
  {
   struct lhsParseNode *argPtr, *lastArg, *nextArg;
   struct lhsParseNode *tempArg;
   bool change;
   struct expr *e1, *e2;

   /*======================================================*/
   /* Loop through the CEs as long as changes can be made. */
   /*======================================================*/

   change = true;
   *newChange = false;

   while (change)
     {
      change = false;
      lastArg = NULL;

      for (argPtr = theLHS->right;
           argPtr != NULL;)
        {
         /*=====================================*/
         /* Remove duplication of or CEs within */
         /* or CEs and and CEs within and CEs.  */
         /*=====================================*/

         if (((theLHS->pnType == OR_CE_NODE) && (argPtr->pnType == OR_CE_NODE)) ||
             ((theLHS->pnType == AND_CE_NODE) && (argPtr->pnType == AND_CE_NODE)))
           {
            if (argPtr->logical) theLHS->logical = true;

            change = true;
            *newChange = true;
            tempArg = argPtr->right;
            nextArg = argPtr->bottom;
            argPtr->right = NULL;
            argPtr->bottom = NULL;
            ReturnLHSParseNodes(theEnv,argPtr);

            if (lastArg == NULL)
              { theLHS->right = tempArg; }
            else
              { lastArg->bottom = tempArg; }

            argPtr = tempArg;
            while (tempArg->bottom != NULL) tempArg = tempArg->bottom;
            tempArg->bottom = nextArg;
           }

         /*=======================================================*/
         /* Replace not CEs containing a test CE with just a test */
         /* CE with the original test CE condition negated.       */
         /*=======================================================*/

         else if ((theLHS->pnType == NOT_CE_NODE) && (argPtr->pnType == TEST_CE_NODE))
           {
            change = true;
            *newChange = true;

            tempArg = GetLHSParseNode(theEnv);
            tempArg->pnType = FCALL_NODE;
            tempArg->value = ExpressionData(theEnv)->PTR_NOT;
            tempArg->bottom = argPtr->expression;
            argPtr->expression = tempArg;

            CopyLHSParseNode(theEnv,theLHS,argPtr,true);
            ReturnLHSParseNodes(theEnv,argPtr);
            theLHS->right = NULL;
            break;
           }

         /*==============================*/
         /* Two adjacent test CEs within */
         /* an and CE can be combined.   */
         /*==============================*/

         else if ((theLHS->pnType == AND_CE_NODE) && (argPtr->pnType == TEST_CE_NODE) &&
                  ((argPtr->bottom != NULL) ? argPtr->bottom->pnType == TEST_CE_NODE :
                                              false) &&
                   (argPtr->beginNandDepth == argPtr->endNandDepth) &&
                   (argPtr->endNandDepth == argPtr->bottom->beginNandDepth))
           {
            change = true;
            *newChange = true;

            argPtr->expression = CombineLHSParseNodes(theEnv,argPtr->expression,argPtr->bottom->expression);
            argPtr->bottom->expression = NULL;

            tempArg = argPtr->bottom;
            argPtr->bottom = tempArg->bottom;
            tempArg->bottom = NULL;

            ReturnLHSParseNodes(theEnv,tempArg);
           }

         /*========================================================*/
         /* A test CE can be attached to the preceding pattern CE. */
         /*========================================================*/

         else if ((theLHS->pnType == AND_CE_NODE) && (argPtr->pnType == PATTERN_CE_NODE) &&
                  ((argPtr->bottom != NULL) ? argPtr->bottom->pnType == TEST_CE_NODE :
                                              false) &&
                   (argPtr->negated == false) &&
                   (argPtr->exists == false) &&
                   (argPtr->beginNandDepth == argPtr->endNandDepth) &&
                   (argPtr->endNandDepth == argPtr->bottom->beginNandDepth))
           {
            int endNandDepth;
            change = true;
            *newChange = true;

            endNandDepth = argPtr->bottom->endNandDepth;

            if (argPtr->negated || argPtr->exists)
              {
               e1 = LHSParseNodesToExpression(theEnv,argPtr->secondaryExpression);
               e2 = LHSParseNodesToExpression(theEnv,argPtr->bottom->expression);
               e1 = CombineExpressions(theEnv,e1,e2);
               ReturnLHSParseNodes(theEnv,argPtr->secondaryExpression);
               argPtr->secondaryExpression = ExpressionToLHSParseNodes(theEnv,e1);
               ReturnExpression(theEnv,e1);
              }
            else
              {
               argPtr->expression = CombineLHSParseNodes(theEnv,argPtr->expression,argPtr->bottom->expression);
               argPtr->bottom->expression = NULL;
              }

            if ((theLHS->right == argPtr) && ((argPtr->beginNandDepth - 1) == endNandDepth))
              {
               if (argPtr->negated)
                 {
                  argPtr->negated = false;
                  argPtr->exists = true;
                  e1 = LHSParseNodesToExpression(theEnv,argPtr->secondaryExpression);
                  e1 = NegateExpression(theEnv,e1);
                  ReturnLHSParseNodes(theEnv,argPtr->secondaryExpression);
                  argPtr->secondaryExpression = ExpressionToLHSParseNodes(theEnv,e1);
                  ReturnExpression(theEnv,e1);
                 }
               else if (argPtr->exists)
                 {
                  argPtr->negated = true;
                  argPtr->exists = false;
                  e1 = LHSParseNodesToExpression(theEnv,argPtr->secondaryExpression);
                  e1 = NegateExpression(theEnv,e1);
                  ReturnLHSParseNodes(theEnv,argPtr->secondaryExpression);
                  argPtr->secondaryExpression = ExpressionToLHSParseNodes(theEnv,e1);
                  ReturnExpression(theEnv,e1);
                 }
               else
                 {
                  argPtr->negated = true;
                 }
               PropagateNandDepth(argPtr,endNandDepth,endNandDepth);
              }

            /*========================================*/
            /* Detach the test CE from its parent and */
            /* dispose of the data structures.        */
            /*========================================*/

            tempArg = argPtr->bottom;
            argPtr->bottom = tempArg->bottom;
            tempArg->bottom = NULL;

            ReturnLHSParseNodes(theEnv,tempArg);
           }

         /*=====================================*/
         /* Replace and CEs containing a single */
         /* test CE with just a test CE.        */
         /*=====================================*/

         else if ((theLHS->pnType == AND_CE_NODE) && (argPtr->pnType == TEST_CE_NODE) &&
                  (theLHS->right == argPtr) && (argPtr->bottom == NULL))
           {
            change = true;
            *newChange = true;

            CopyLHSParseNode(theEnv,theLHS,argPtr,true);
            theLHS->right = NULL;
            ReturnLHSParseNodes(theEnv,argPtr);
            break;
           }

         /*=======================================================*/
         /* Replace and CEs containing a single pattern CE with   */
         /* just a pattern CE if this is not the top most and CE. */
         /*=======================================================*/

         else if ((theLHS->pnType == AND_CE_NODE) && (argPtr->pnType == PATTERN_CE_NODE) &&
                  (theLHS->right == argPtr) && (argPtr->bottom == NULL) && (depth > 1))
           {
            change = true;
            *newChange = true;

            CopyLHSParseNode(theEnv,theLHS,argPtr,false);

            theLHS->right = argPtr->right;

            argPtr->networkTest = NULL;
            argPtr->externalNetworkTest = NULL;
            argPtr->secondaryNetworkTest = NULL;
            argPtr->externalRightHash = NULL;
            argPtr->externalLeftHash = NULL;
            argPtr->constantSelector = NULL;
            argPtr->constantValue = NULL;
            argPtr->leftHash = NULL;
            argPtr->rightHash = NULL;
            argPtr->betaHash = NULL;
            argPtr->expression = NULL;
            argPtr->secondaryExpression = NULL;
            argPtr->userData = NULL;
            argPtr->right = NULL;
            argPtr->bottom = NULL;
            ReturnLHSParseNodes(theEnv,argPtr);
            break;
           }

         /*===================================================*/
         /* If no changes are needed, move on to the next CE. */
         /*===================================================*/

         else
           {
            lastArg = argPtr;
            argPtr = argPtr->bottom;
           }
        }
     }

   /*===========================*/
   /* Return the reordered LHS. */
   /*===========================*/

   return(theLHS);
  }

/*********************************************************************/
/* CopyLHSParseNodes: Copies a linked group of conditional elements. */
/*********************************************************************/
struct lhsParseNode *CopyLHSParseNodes(
  Environment *theEnv,
  struct lhsParseNode *listOfCEs)
  {
   struct lhsParseNode *newList;

   if (listOfCEs == NULL)
     { return NULL; }

   newList = get_struct(theEnv,lhsParseNode);
   CopyLHSParseNode(theEnv,newList,listOfCEs,true);

   newList->right = CopyLHSParseNodes(theEnv,listOfCEs->right);
   newList->bottom = CopyLHSParseNodes(theEnv,listOfCEs->bottom);

   return(newList);
  }

/**********************************************************/
/* CopyLHSParseNode: Copies a single conditional element. */
/**********************************************************/
void CopyLHSParseNode(
  Environment *theEnv,
  struct lhsParseNode *dest,
  struct lhsParseNode *src,
  bool duplicate)
  {
   dest->pnType = src->pnType;
   dest->value = src->value;
   dest->negated = src->negated;
   dest->exists = src->exists;
   dest->existsNand = src->existsNand;
   dest->goalCE = src->goalCE;
   dest->explicitCE = src->explicitCE;
   dest->bindingVariable = src->bindingVariable;
   dest->withinMultifieldSlot = src->withinMultifieldSlot;
   dest->multifieldSlot = src->multifieldSlot;
   dest->multiFieldsBefore = src->multiFieldsBefore;
   dest->multiFieldsAfter = src->multiFieldsAfter;
   dest->singleFieldsBefore = src->singleFieldsBefore;
   dest->singleFieldsAfter = src->singleFieldsAfter;
   dest->logical = src->logical;
   dest->userCE = src->userCE;
   //dest->marked = src->marked;
   dest->whichCE = src->whichCE;
   dest->referringNode = src->referringNode;
   dest->patternType = src->patternType;
   dest->pattern = src->pattern;
   dest->index = src->index;
   dest->slot = src->slot;
   dest->slotNumber = src->slotNumber;
   dest->beginNandDepth = src->beginNandDepth;
   dest->endNandDepth = src->endNandDepth;
   dest->joinDepth = src->joinDepth;

   /*==========================================================*/
   /* The duplicate flag controls whether pointers to existing */
   /* data structures are used when copying some slots or if   */
   /* new copies of the data structures are made.              */
   /*==========================================================*/

   if (duplicate)
     {
      dest->networkTest = CopyExpression(theEnv,src->networkTest);
      dest->externalNetworkTest = CopyExpression(theEnv,src->externalNetworkTest);
      dest->secondaryNetworkTest = CopyExpression(theEnv,src->secondaryNetworkTest);
      dest->externalRightHash = CopyExpression(theEnv,src->externalRightHash);
      dest->externalLeftHash = CopyExpression(theEnv,src->externalLeftHash);
      dest->constantSelector = CopyExpression(theEnv,src->constantSelector);
      dest->constantValue = CopyExpression(theEnv,src->constantValue);
      dest->leftHash = CopyExpression(theEnv,src->leftHash);
      dest->betaHash = CopyExpression(theEnv,src->betaHash);
      dest->rightHash = CopyExpression(theEnv,src->rightHash);
      if (src->userData == NULL)
        { dest->userData = NULL; }
      else if (src->patternType->copyUserDataFunction == NULL)
        { dest->userData = src->userData; }
      else
        { dest->userData = (*src->patternType->copyUserDataFunction)(theEnv,src->userData); }
      dest->expression = CopyLHSParseNodes(theEnv,src->expression);
      dest->secondaryExpression = CopyLHSParseNodes(theEnv,src->secondaryExpression);
      dest->constraints = CopyConstraintRecord(theEnv,src->constraints);
      if (dest->constraints != NULL) dest->derivedConstraints = true;
      else dest->derivedConstraints = false;
     }
   else
     {
      dest->networkTest = src->networkTest;
      dest->externalNetworkTest = src->externalNetworkTest;
      dest->secondaryNetworkTest = src->secondaryNetworkTest;
      dest->externalRightHash = src->externalRightHash;
      dest->externalLeftHash = src->externalLeftHash;
      dest->constantSelector = src->constantSelector;
      dest->constantValue = src->constantValue;
      dest->leftHash = src->leftHash;
      dest->betaHash = src->betaHash;
      dest->rightHash = src->rightHash;
      dest->userData = src->userData;
      dest->expression = src->expression;
      dest->secondaryExpression = src->secondaryExpression;
      dest->derivedConstraints = false;
      dest->constraints = src->constraints;
     }
  }

/****************************************************/
/* GetLHSParseNode: Creates an empty node structure */
/*   used for building conditional elements.        */
/****************************************************/
struct lhsParseNode *GetLHSParseNode(
  Environment *theEnv)
  {
   struct lhsParseNode *newNode;

   newNode = get_struct(theEnv,lhsParseNode);
   newNode->pnType = UNKNOWN_NODE;
   newNode->value = NULL;
   newNode->negated = false;
   newNode->exists = false;
   newNode->existsNand = false;
   newNode->goalCE = false;
   newNode->explicitCE = false;
   newNode->bindingVariable = false;
   newNode->withinMultifieldSlot = false;
   newNode->multifieldSlot = false;
   newNode->multiFieldsBefore = 0;
   newNode->multiFieldsAfter = 0;
   newNode->singleFieldsBefore = 0;
   newNode->singleFieldsAfter = 0;
   newNode->logical = false;
   newNode->derivedConstraints = false;
   newNode->userCE = true;
   //newNode->marked = false;
   newNode->whichCE = 0;
   newNode->constraints = NULL;
   newNode->referringNode = NULL;
   newNode->patternType = NULL;
   newNode->pattern = -1;
   newNode->index = NO_INDEX;
   newNode->slot = NULL;
   newNode->slotNumber = UNSPECIFIED_SLOT;
   newNode->beginNandDepth = 1;
   newNode->endNandDepth = 1;
   newNode->joinDepth = 0;
   newNode->userData = NULL;
   newNode->networkTest = NULL;
   newNode->externalNetworkTest = NULL;
   newNode->secondaryNetworkTest = NULL;
   newNode->externalRightHash = NULL;
   newNode->externalLeftHash = NULL;
   newNode->constantSelector = NULL;
   newNode->constantValue = NULL;
   newNode->leftHash = NULL;
   newNode->betaHash = NULL;
   newNode->rightHash = NULL;
   newNode->expression = NULL;
   newNode->secondaryExpression = NULL;
   newNode->right = NULL;
   newNode->bottom = NULL;

   return(newNode);
  }

/********************************************************/
/* ReturnLHSParseNodes:  Returns a multiply linked list */
/*   of lhsParseNode structures to the memory manager.  */
/********************************************************/
void ReturnLHSParseNodes(
  Environment *theEnv,
  struct lhsParseNode *waste)
  {
   if (waste != NULL)
     {
      ReturnExpression(theEnv,waste->networkTest);
      ReturnExpression(theEnv,waste->externalNetworkTest);
      ReturnExpression(theEnv,waste->secondaryNetworkTest);
      ReturnExpression(theEnv,waste->externalRightHash);
      ReturnExpression(theEnv,waste->externalLeftHash);
      ReturnExpression(theEnv,waste->constantSelector);
      ReturnExpression(theEnv,waste->constantValue);
      ReturnExpression(theEnv,waste->leftHash);
      ReturnExpression(theEnv,waste->betaHash);
      ReturnExpression(theEnv,waste->rightHash);
      ReturnLHSParseNodes(theEnv,waste->right);
      ReturnLHSParseNodes(theEnv,waste->bottom);
      ReturnLHSParseNodes(theEnv,waste->expression);
      ReturnLHSParseNodes(theEnv,waste->secondaryExpression);
      if (waste->derivedConstraints) RemoveConstraint(theEnv,waste->constraints);
      if ((waste->userData != NULL) &&
          (waste->patternType->returnUserDataFunction != NULL))
        { (*waste->patternType->returnUserDataFunction)(theEnv,waste->userData); }
      rtn_struct(theEnv,lhsParseNode,waste);
     }
  }

/********************************************************/
/* ExpressionToLHSParseNodes: Copies an expression into */
/*   the equivalent lhsParseNode data structures.       */
/********************************************************/
struct lhsParseNode *ExpressionToLHSParseNodes(
  Environment *theEnv,
  struct expr *expressionList)
  {
   struct lhsParseNode *newList, *theList;
   struct functionDefinition *theFunction;
   unsigned int i;
   unsigned theRestriction2;

   /*===========================================*/
   /* A NULL expression requires no conversion. */
   /*===========================================*/

   if (expressionList == NULL) return NULL;

   /*====================================*/
   /* Recursively convert the expression */
   /* to lhsParseNode data structures.   */
   /*====================================*/

   newList = GetLHSParseNode(theEnv);
   newList->pnType = TypeToNodeType(expressionList->type);
   newList->value = expressionList->value;
   newList->right = ExpressionToLHSParseNodes(theEnv,expressionList->nextArg);
   newList->bottom = ExpressionToLHSParseNodes(theEnv,expressionList->argList);

   /*==================================================*/
   /* If the expression is a function call, then store */
   /* the constraint information for the functions     */
   /* arguments in the lshParseNode data structures.   */
   /*==================================================*/

   if (newList->pnType != FCALL_NODE) return(newList);

   theFunction = newList->functionValue;
   for (theList = newList->bottom, i = 1;
        theList != NULL;
        theList = theList->right, i++)
     {
      if (theList->pnType == SF_VARIABLE_NODE)
        {
         theRestriction2 = GetNthRestriction(theEnv,theFunction,i);
         theList->constraints = ArgumentTypeToConstraintRecord(theEnv,theRestriction2);
         theList->derivedConstraints = true;
        }
     }

   /*==================================*/
   /* Return the converted expression. */
   /*==================================*/

   return(newList);
  }

/******************************************************************/
/* LHSParseNodesToExpression: Copies lhsParseNode data structures */
/*   into the equivalent expression data structures.              */
/******************************************************************/
struct expr *LHSParseNodesToExpression(
  Environment *theEnv,
  struct lhsParseNode *nodeList)
  {
   struct expr *newList;

   if (nodeList == NULL)
     { return NULL; }

   newList = get_struct(theEnv,expr);
   newList->type = NodeTypeToType(nodeList);
   newList->value = nodeList->value;
   newList->nextArg = LHSParseNodesToExpression(theEnv,nodeList->right);
   newList->argList = LHSParseNodesToExpression(theEnv,nodeList->bottom);

   return(newList);
  }

/******************************************/
/* ConstantNode: Returns true if the node */
/*   is a constant, otherwise false.      */
/******************************************/
bool ConstantNode(
  struct lhsParseNode *theNode)
  {
   switch (theNode->pnType)
     {
      case SYMBOL_NODE:
      case STRING_NODE:
      case INTEGER_NODE:
      case FLOAT_NODE:
#if OBJECT_SYSTEM
      case INSTANCE_NAME_NODE:
#endif
        return true;
        
      default:
        return false;
     }

   return false;
  }

/*******************/
/* NodeTypeToType: */
/*******************/
unsigned short NodeTypeToType(
  struct lhsParseNode *theNode)
  {
   switch (theNode->pnType)
     {
      case FLOAT_NODE:
        return FLOAT_TYPE;
      case INTEGER_NODE:
        return INTEGER_TYPE;
      case SYMBOL_NODE:
        return SYMBOL_TYPE;
      case STRING_NODE:
        return STRING_TYPE;
      case INSTANCE_NAME_NODE:
        return INSTANCE_NAME_TYPE;
      case SF_VARIABLE_NODE:
        return SF_VARIABLE;
      case MF_VARIABLE_NODE:
        return MF_VARIABLE;
      case GBL_VARIABLE_NODE:
        return GBL_VARIABLE;
      case FCALL_NODE:
        return FCALL;
      case PCALL_NODE:
        return PCALL;
      case GCALL_NODE:
        return GCALL;
      case FACT_STORE_MULTIFIELD_NODE:
        return FACT_STORE_MULTIFIELD;
      case DEFTEMPLATE_PTR_NODE:
        return DEFTEMPLATE_PTR;
      case DEFCLASS_PTR_NODE:
        return DEFCLASS_PTR;

      default:
        return VOID_TYPE;
     }
  }

/*******************/
/* TypeToNodeType: */
/*******************/
ParseNodeType TypeToNodeType(
  unsigned short theType)
  {
   switch (theType)
     {
      case FLOAT_TYPE:
        return FLOAT_NODE;
      case INTEGER_TYPE:
        return INTEGER_NODE;
      case SYMBOL_TYPE:
        return SYMBOL_NODE;
      case STRING_TYPE:
        return STRING_NODE;
      case INSTANCE_NAME_TYPE:
        return INSTANCE_NAME_NODE;
      case SF_VARIABLE:
        return SF_VARIABLE_NODE;
      case MF_VARIABLE:
        return MF_VARIABLE_NODE;
      case GBL_VARIABLE:
        return GBL_VARIABLE_NODE;
      case FCALL:
        return FCALL_NODE;
      case PCALL:
        return PCALL_NODE;
      case GCALL:
        return GCALL_NODE;
      case FACT_STORE_MULTIFIELD:
        return FACT_STORE_MULTIFIELD_NODE;
      case DEFTEMPLATE_PTR:
        return DEFTEMPLATE_PTR_NODE;
      case DEFCLASS_PTR:
        return DEFCLASS_PTR_NODE;

      default:
        return UNKNOWN_NODE;
     }
  }

/************************************************************/
/* IncrementNandDepth: Increments the nand depth of a group */
/*   of CEs. The nand depth is used to indicate the nesting */
/*   of not/and or not/not CEs which are implemented using  */
/*   joins from the right. A single pattern within a "not"  */
/*   CE does not require a join from the right and its nand */
/*   depth is normally not increased (except when it's      */
/*   within a not/and or not/not CE. The begin nand depth   */
/*   indicates the current nesting for a CE. The end nand   */
/*   depth indicates the nand depth in the following CE     */
/*   (assuming that the next CE is not the beginning of a   */
/*   new group of nand CEs). All but the last CE in a nand  */
/*   group should have the same begin and end nand depths.  */
/*   Since a single CE can be the last CE of several nand   */
/*   groups, it is possible to have an end nand depth that  */
/*   is more than 1 less than the begin nand depth of the   */
/*   CE.                                                    */
/************************************************************/
static void IncrementNandDepth(
  Environment *theEnv,
  struct lhsParseNode *theLHS,
  bool lastCE)
  {
   /*======================================*/
   /* Loop through each CE in the group of */
   /* CEs having its nand depth increased. */
   /*======================================*/

   for (;
        theLHS != NULL;
        theLHS = theLHS->bottom)
     {
      /*=========================================================*/
      /* Increment the begin nand depth of pattern and test CEs. */
      /* The last CE in the original list doesn't have its end   */
      /* nand depth incremented. All other last CEs in other CEs */
      /* entered recursively do have their end depth incremented */
      /* (unless the last CE in the recursively entered CE is    */
      /* the same last CE as contained in the original group     */
      /* when this function was first entered).                  */
      /*=========================================================*/

      if ((theLHS->pnType == PATTERN_CE_NODE) || (theLHS->pnType == TEST_CE_NODE))
        {
         theLHS->beginNandDepth++;

         if (lastCE == false) theLHS->endNandDepth++;
         else if (theLHS->bottom != NULL) theLHS->endNandDepth++;
        }

      /*==============================================*/
      /* Recursively increase the nand depth of other */
      /* CEs contained within the CE having its nand  */
      /* depth increased.                             */
      /*==============================================*/

      else if ((theLHS->pnType == AND_CE_NODE) || (theLHS->pnType == NOT_CE_NODE))
        {
         IncrementNandDepth(theEnv,theLHS->right,
                            (lastCE ? (theLHS->bottom == NULL) : false));
        }

      /*=====================================*/
      /* All or CEs should have been removed */
      /* from the LHS at this point.         */
      /*=====================================*/

      else if (theLHS->pnType == OR_CE_NODE)
        { SystemError(theEnv,"REORDER",1); }
     }
  }

/***********************************************************/
/* CreateInitialPattern: Creates a default pattern used in */
/*  the LHS of a rule under certain cirmustances (such as  */
/*  when a "not" or "test" CE is the first CE in an "and"  */
/*  CE or when no CEs are specified in the LHS of a rule.  */
/***********************************************************/
static struct lhsParseNode *CreateInitialPattern(
  Environment *theEnv)
  {
   struct lhsParseNode *topNode;

   /*==========================================*/
   /* Create the top most node of the pattern. */
   /*==========================================*/

   topNode = GetLHSParseNode(theEnv);
   topNode->pnType = PATTERN_CE_NODE;
   topNode->userCE = false;
   topNode->bottom = NULL;

   return(topNode);
  }

/*****************************************************************/
/* AddRemainingInitialPatterns: Finishes adding initial patterns */
/*   where needed on the LHS of a rule. Assumes that an initial  */
/*   pattern has been added to the beginning of the rule if one  */
/*   was needed.                                                 */
/*****************************************************************/
static struct lhsParseNode *AddRemainingInitialPatterns(
  Environment *theEnv,
  struct lhsParseNode *theLHS)
  {
   struct lhsParseNode *lastNode = NULL, *thePattern, *rv = theLHS;
   int currentDepth = 1;

   while (theLHS != NULL)
     {
      if ((theLHS->pnType == TEST_CE_NODE) &&
          (theLHS->beginNandDepth  > currentDepth))
        {
         thePattern = CreateInitialPattern(theEnv);
         thePattern->beginNandDepth = theLHS->beginNandDepth;
         thePattern->endNandDepth = theLHS->beginNandDepth;
         thePattern->logical = theLHS->logical;
         thePattern->existsNand = theLHS->existsNand;
         theLHS->existsNand = false;

         thePattern->bottom = theLHS;

         if (lastNode == NULL)
           { rv = thePattern; }
         else
           { lastNode->bottom = thePattern; }
        }

      lastNode = theLHS;
      currentDepth = theLHS->endNandDepth;
      theLHS = theLHS->bottom;
     }

   return(rv);
  }

/*************************************************************/
/* AssignPatternIndices: For each pattern CE in the LHS of a */
/*   rule, determines the pattern index for the CE. A simple */
/*   1 to N numbering can't be used since a join from the    */
/*   right only counts as a single CE to other CEs outside   */
/*   the lexical scope of the join from the right. For       */
/*   example, the patterns in the following LHS              */
/*                                                           */
/*     (a) (not (b) (c) (d)) (e)                             */
/*                                                           */
/*   would be assigned the following numbers: a-1, b-2, c-3, */
/*   d-4, and e-3.                                           */
/*************************************************************/
static struct lhsParseNode *AssignPatternIndices(
  struct lhsParseNode *theLHS,
  short startIndex,
  int nandDepth,
  unsigned short joinDepth)
  {
   struct lhsParseNode *theField;

   /*====================================*/
   /* Loop through the CEs at this level */
   /* assigning each CE a pattern index. */
   /*====================================*/

   while (theLHS != NULL)
     {
      /*============================================================*/
      /* If we're entering a group of CEs requiring a join from the */
      /* right, compute the pattern indices for that group and then */
      /* proceed with the next CE in this group. A join from the    */
      /* right only counts as one CE on this level regardless of    */
      /* the number of CEs it contains. If the end of this level is */
      /* encountered while processing the join from right, then     */
      /* return to the previous level.                              */
      /*============================================================*/

      if (theLHS->beginNandDepth > nandDepth)
        {
         theLHS = AssignPatternIndices(theLHS,startIndex,nandDepth+1,joinDepth);

         if (theLHS->endNandDepth < nandDepth) return(theLHS);
         startIndex++;
         joinDepth++;
        }

      /*=====================================================*/
      /* A test CE is not assigned a pattern index, however, */
      /* if it is the last CE at the end of this level, then */
      /* return to the previous level. If this is the first  */
      /* CE in a group, it will have a join created so the   */
      /* depth should be incremented.                        */
      /*=====================================================*/

      else if (theLHS->pnType == TEST_CE_NODE)
        {
         if (joinDepth == 0)
           { joinDepth++; }
         theLHS->joinDepth = joinDepth - 1;
         PropagateJoinDepth(theLHS->expression,joinDepth - 1);
         PropagateNandDepth(theLHS->expression,theLHS->beginNandDepth,theLHS->endNandDepth);
         if (theLHS->endNandDepth < nandDepth) return(theLHS);
        }

      /*==========================================================*/
      /* The fields of a pattern CE need to be assigned a pattern */
      /* index, field index, and/or slot names. If this CE is the */
      /* last CE at the end of this level, then return to the     */
      /* previous level.                                          */
      /*==========================================================*/

      else if (theLHS->pnType == PATTERN_CE_NODE)
        {
         if (theLHS->expression != NULL)
           {
            PropagateJoinDepth(theLHS->expression,joinDepth);
            PropagateNandDepth(theLHS->expression,theLHS->beginNandDepth,theLHS->endNandDepth);
           }

         theLHS->pattern = startIndex;
         theLHS->joinDepth = joinDepth;
         PropagateJoinDepth(theLHS->right,joinDepth);
         PropagateNandDepth(theLHS->right,theLHS->beginNandDepth,theLHS->endNandDepth);
         for (theField = theLHS->right; theField != NULL; theField = theField->right)
           {
            theField->pattern = startIndex;
            PropagateIndexSlotPatternValues(theField,theField->pattern,
                                            theField->index,theField->slot,
                                            theField->slotNumber);
           }

         if (theLHS->endNandDepth < nandDepth) return(theLHS);
         startIndex++;
         joinDepth++;
        }

      /*=========================*/
      /* Move on to the next CE. */
      /*=========================*/

      theLHS = theLHS->bottom;
     }

   /*========================================*/
   /* There are no more CEs left to process. */
   /* Return to the previous level.          */
   /*========================================*/

   return NULL;
  }

/***********************************************************/
/* PropagateIndexSlotPatternValues: Assigns pattern, field */
/*   and slot identifiers to a field in a pattern.         */
/***********************************************************/
static void PropagateIndexSlotPatternValues(
  struct lhsParseNode *theField,
  short thePattern,
  unsigned short theIndex,
  CLIPSLexeme *theSlot,
  unsigned short theSlotNumber)
  {
   struct lhsParseNode *tmpNode, *andField;

   /*=============================================*/
   /* A NULL field does not have to be processed. */
   /*=============================================*/

   if (theField == NULL) return;

   /*=====================================================*/
   /* Assign the appropriate identifiers for a multifield */
   /* slot by calling this routine recursively.           */
   /*=====================================================*/

   if (theField->multifieldSlot)
     {
      theField->pattern = thePattern;
      if ((theIndex > 0) && (theIndex != NO_INDEX))
         { theField->index = theIndex; }
      theField->slot = theSlot;
      theField->slotNumber = theSlotNumber;

      for (tmpNode = theField->bottom;
           tmpNode != NULL;
           tmpNode = tmpNode->right)
        {
         tmpNode->pattern = thePattern;
         tmpNode->slot = theSlot;
         PropagateIndexSlotPatternValues(tmpNode,thePattern,tmpNode->index,
                                         theSlot,theSlotNumber);
        }

      return;
     }

   /*=======================================================*/
   /* Loop through each of the or'ed constraints (connected */
   /* by a |) in this field of the pattern.                 */
   /*=======================================================*/

   for (theField = theField->bottom;
        theField != NULL;
        theField = theField->bottom)
     {
      /*===========================================================*/
      /* Loop through each of the and'ed constraints (connected by */
      /* a &) in this field of the pattern. Assign the pattern,    */
      /* field, and slot identifiers.                              */
      /*===========================================================*/

      for (andField = theField; andField != NULL; andField = andField->right)
        {
         andField->pattern = thePattern;
         if ((theIndex > 0) && (theIndex != NO_INDEX))
           { andField->index = theIndex; }
         andField->slot = theSlot;
         andField->slotNumber = theSlotNumber;
        }
     }
   }

/***************************************************/
/* AssignPatternMarkedFlag: Recursively assigns    */
/*   value to the marked field of a LHSParseNode.  */
/***************************************************/
/*
void AssignPatternMarkedFlag(
  struct lhsParseNode *theField,
  short markedValue)
  {
   while (theField != NULL)
     {
      theField->marked = markedValue;
      if (theField->bottom != NULL)
        { AssignPatternMarkedFlag(theField->bottom,markedValue); }
      if (theField->expression != NULL)
        { AssignPatternMarkedFlag(theField->expression,markedValue); }
      if (theField->secondaryExpression != NULL)
        { AssignPatternMarkedFlag(theField->secondaryExpression,markedValue); }
      theField = theField->right;
     }
  }
*/
/*****************************************************************/
/* PropagateJoinDepth: Recursively assigns a joinDepth to each   */
/*   node in a LHS node by following the right and bottom links. */
/*****************************************************************/
static void PropagateJoinDepth(
  struct lhsParseNode *theField,
  unsigned short joinDepth)
  {
   while (theField != NULL)
     {
      theField->joinDepth = joinDepth;
      if (theField->bottom != NULL)
        { PropagateJoinDepth(theField->bottom,joinDepth); }
      if (theField->expression != NULL)
        { PropagateJoinDepth(theField->expression,joinDepth); }
      if (theField->secondaryExpression != NULL)
        { PropagateJoinDepth(theField->secondaryExpression,joinDepth); }
      theField = theField->right;
     }
  }

/**************************************************************/
/* PropagateNandDepth: Recursively assigns the not/and (nand) */
/*   depth to each node in a LHS node by following the right, */
/*   bottom, and expression links.                            */
/**************************************************************/
static void PropagateNandDepth(
  struct lhsParseNode *theField,
  int beginDepth,
  int endDepth)
  {
   if (theField == NULL) return;

   for (; theField != NULL; theField = theField->right)
      {
       theField->beginNandDepth = beginDepth;
       theField->endNandDepth = endDepth;
       PropagateNandDepth(theField->expression,beginDepth,endDepth);
       PropagateNandDepth(theField->secondaryExpression,beginDepth,endDepth);
       PropagateNandDepth(theField->bottom,beginDepth,endDepth);
      }
  }

/*****************************************/
/* PropagateWhichCE: Recursively assigns */
/*   an index indicating the user CE.    */
/*****************************************/
static unsigned short PropagateWhichCE(
  struct lhsParseNode *theField,
  unsigned short whichCE)
  {
   while (theField != NULL)
     {
      if ((theField->pnType == PATTERN_CE_NODE) || (theField->pnType == TEST_CE_NODE))
        { whichCE++; }

      theField->whichCE = whichCE;

      whichCE = PropagateWhichCE(theField->right,whichCE);
      PropagateWhichCE(theField->expression,whichCE);

      theField = theField->bottom;
     }

   return whichCE;
  }

/********************/
/* IsExistsSubjoin: */
/********************/
bool IsExistsSubjoin(
  struct lhsParseNode *theLHS,
  int parentDepth)
  {
   int startDepth = theLHS->beginNandDepth;

   if ((startDepth - parentDepth) != 2)
     { return false; }

   while (theLHS->endNandDepth >= startDepth)
     { theLHS = theLHS->bottom; }

   if (theLHS->endNandDepth <= parentDepth)
     { return true; }

   return false;
  }

/***************************************************************************/
/* CombineLHSParseNodes: Combines two expressions into a single equivalent */
/*   expression. Mainly serves to merge expressions containing "and"       */
/*   and "or" expressions without unnecessary duplication of the "and"     */
/*   and "or" expressions (i.e., two "and" expressions can be merged by    */
/*   placing them as arguments within another "and" expression, but it     */
/*   is more efficient to add the arguments of one of the "and"            */
/*   expressions to the list of arguments for the other and expression).   */
/***************************************************************************/
struct lhsParseNode *CombineLHSParseNodes(
  Environment *theEnv,
  struct lhsParseNode *expr1,
  struct lhsParseNode *expr2)
  {
   struct lhsParseNode *tempPtr;

   /*===========================================================*/
   /* If the 1st expression is NULL, return the 2nd expression. */
   /*===========================================================*/

   if (expr1 == NULL) return(expr2);

   /*===========================================================*/
   /* If the 2nd expression is NULL, return the 1st expression. */
   /*===========================================================*/

   if (expr2 == NULL) return(expr1);

   /*============================================================*/
   /* If the 1st expression is an "and" expression, and the 2nd  */
   /* expression is not an "and" expression, then include the    */
   /* 2nd expression in the argument list of the 1st expression. */
   /*============================================================*/

   if ((expr1->value == ExpressionData(theEnv)->PTR_AND) &&
       (expr2->value != ExpressionData(theEnv)->PTR_AND))
     {
      tempPtr = expr1->bottom;
      if (tempPtr == NULL)
        {
         rtn_struct(theEnv,lhsParseNode,expr1);
         return(expr2);
        }

      while (tempPtr->right != NULL)
        { tempPtr = tempPtr->right; }

      tempPtr->right = expr2;
      return(expr1);
     }

   /*============================================================*/
   /* If the 2nd expression is an "and" expression, and the 1st  */
   /* expression is not an "and" expression, then include the    */
   /* 1st expression in the argument list of the 2nd expression. */
   /*============================================================*/

   if ((expr1->value != ExpressionData(theEnv)->PTR_AND) &&
       (expr2->value == ExpressionData(theEnv)->PTR_AND))
     {
      tempPtr = expr2->bottom;
      if (tempPtr == NULL)
        {
         rtn_struct(theEnv,lhsParseNode,expr2);
         return(expr1);
        }

      expr2->bottom = expr1;
      expr1->right = tempPtr;

      return(expr2);
     }

   /*===========================================================*/
   /* If both expressions are "and" expressions, then add the   */
   /* 2nd expression to the argument list of the 1st expression */
   /* and throw away the extraneous "and" expression.           */
   /*===========================================================*/

   if ((expr1->value == ExpressionData(theEnv)->PTR_AND) &&
       (expr2->value == ExpressionData(theEnv)->PTR_AND))
     {
      tempPtr = expr1->bottom;
      if (tempPtr == NULL)
        {
         rtn_struct(theEnv,lhsParseNode,expr1);
         return(expr2);
        }

      while (tempPtr->right != NULL)
        { tempPtr = tempPtr->right; }

      tempPtr->right = expr2->bottom;
      rtn_struct(theEnv,lhsParseNode,expr2);

      return(expr1);
     }

   /*=====================================================*/
   /* If neither expression is an "and" expression, then  */
   /* create an "and" expression and add both expressions */
   /* to the argument list of that "and" expression.      */
   /*=====================================================*/

   tempPtr = GetLHSParseNode(theEnv);
   tempPtr->pnType = FCALL_NODE;
   tempPtr->value = ExpressionData(theEnv)->PTR_AND;

   tempPtr->bottom = expr1;
   expr1->right = expr2;
   return(tempPtr);
  }

/**********************************************/
/* PrintNodes: Debugging routine which prints */
/*   the representation of a CE.              */
/**********************************************/
/*
static void PrintNodes(
  Environment *theEnv,
  const char *fileid,
  struct lhsParseNode *theNode)
  {
   if (theNode == NULL) return;

   while (theNode != NULL)
     {
      switch (theNode->type)
        {
         case PATTERN_CE:
           WriteString(theEnv,fileid,"(");
           if (theNode->negated) WriteString(theEnv,fileid,"n");
           if (theNode->exists) WriteString(theEnv,fileid,"x");
           if (theNode->logical) WriteString(theEnv,fileid,"l");
           PrintUnsignedInteger(theEnv,fileid,theNode->beginNandDepth);
           WriteString(theEnv,fileid,"-");
           PrintUnsignedInteger(theEnv,fileid,theNode->endNandDepth);
           WriteString(theEnv,fileid," ");
           WriteString(theEnv,fileid,ValueToString(theNode->right->bottom->value));
           WriteString(theEnv,fileid,")");
           break;

         case TEST_CE:
           WriteString(theEnv,fileid,"(test ");
           PrintUnsignedInteger(theEnv,fileid,theNode->beginNandDepth);
           WriteString(theEnv,fileid,"-");
           PrintUnsignedInteger(theEnv,fileid,theNode->endNandDepth);
           WriteString(theEnv,fileid,")");
           break;

         case NOT_CE:
           if (theNode->logical) WriteString(theEnv,fileid,"(lnot ");
           else WriteString(theEnv,fileid,"(not ");;
           PrintNodes(theEnv,fileid,theNode->right);
           WriteString(theEnv,fileid,")");
           break;

         case OR_CE:
           if (theNode->logical) WriteString(theEnv,fileid,"(lor ");
           else WriteString(theEnv,fileid,"(or ");
           PrintNodes(theEnv,fileid,theNode->right);
           WriteString(theEnv,fileid,")");
           break;

         case AND_CE:
           if (theNode->logical) WriteString(theEnv,fileid,"(land ");
           else WriteString(theEnv,fileid,"(and ");
           PrintNodes(theEnv,fileid,theNode->right);
           WriteString(theEnv,fileid,")");
           break;

         default:
           WriteString(theEnv,fileid,"(unknown)");
           break;

        }

      theNode = theNode->bottom;
      if (theNode != NULL) WriteString(theEnv,fileid," ");
     }

   return;
  }
*/

#endif

