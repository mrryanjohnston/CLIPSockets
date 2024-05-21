   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 7.00  01/23/24             */
   /*                                                     */
   /*              DEFRULE BSAVE/BLOAD MODULE             */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements the binary save/load feature for the  */
/*    defrule construct.                                     */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*      Barry Cameron                                        */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Removed CONFLICT_RESOLUTION_STRATEGIES,        */
/*            DYNAMIC_SALIENCE, and LOGICAL_DEPENDENCIES     */
/*            compilation flags.                             */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            Added support for alpha memories.              */
/*                                                           */
/*            Added salience groups to improve performance   */
/*            with large numbers of activations of different */
/*            saliences.                                     */
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
/*************************************************************/

#include "setup.h"

#if DEFRULE_CONSTRUCT && (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME)

#include <stdio.h>
#include <string.h>

#include "agenda.h"
#include "bload.h"
#include "bsave.h"
#include "engine.h"
#include "envrnmnt.h"
#include "memalloc.h"
#include "moduldef.h"
#include "pattern.h"
#include "reteutil.h"
#include "retract.h"
#include "rulebsc.h"

#include "rulebin.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if BLOAD_AND_BSAVE
   static void                    BsaveFind(Environment *);
   static void                    BsaveExpressions(Environment *,FILE *);
   static void                    BsaveStorage(Environment *,FILE *);
   static void                    BsaveBinaryItem(Environment *,FILE *);
   static void                    BsaveJoins(Environment *,FILE *);
   static void                    BsaveJoin(Environment *,FILE *,struct joinNode *);
   static void                    BsaveDisjuncts(Environment *,FILE *,Defrule *);
   static void                    BsaveTraverseJoins(Environment *,FILE *,struct joinNode *);
   static void                    BsaveLinks(Environment *,FILE *);
   static void                    BsaveTraverseLinks(Environment *,FILE *,struct joinNode *);
   static void                    BsaveLink(FILE *,struct joinLink *);
#endif
   static void                    BloadStorage(Environment *);
   static void                    BloadBinaryItem(Environment *);
   static void                    UpdateDefruleModule(Environment *,void *,unsigned long);
   static void                    UpdateDefrule(Environment *,void *,unsigned long);
   static void                    UpdateJoin(Environment *,void *,unsigned long);
   static void                    UpdateLink(Environment *,void *,unsigned long);
   static void                    ClearBload(Environment *);
   static void                    DeallocateDefruleBloadData(Environment *);

/*****************************************************/
/* DefruleBinarySetup: Installs the binary save/load */
/*   feature for the defrule construct.              */
/*****************************************************/
void DefruleBinarySetup(
  Environment *theEnv)
  {
   AllocateEnvironmentData(theEnv,RULEBIN_DATA,sizeof(struct defruleBinaryData),DeallocateDefruleBloadData);

#if BLOAD_AND_BSAVE
   AddBinaryItem(theEnv,"defrule",20,BsaveFind,BsaveExpressions,
                             BsaveStorage,BsaveBinaryItem,
                             BloadStorage,BloadBinaryItem,
                             ClearBload);
#endif
#if BLOAD || BLOAD_ONLY
   AddBinaryItem(theEnv,"defrule",20,NULL,NULL,NULL,NULL,
                             BloadStorage,BloadBinaryItem,
                             ClearBload);
#endif
  }

/*******************************************************/
/* DeallocateDefruleBloadData: Deallocates environment */
/*    data for the defrule bsave functionality.        */
/*******************************************************/
static void DeallocateDefruleBloadData(
  Environment *theEnv)
  {
#if (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME)
   size_t space;
   unsigned long i;
   struct defruleModule *theModuleItem;
   struct activation *theActivation, *tmpActivation;
   struct salienceGroup *theGroup, *tmpGroup;

   for (i = 0; i < DefruleBinaryData(theEnv)->NumberOfJoins; i++)
     {
      DestroyBetaMemory(theEnv,&DefruleBinaryData(theEnv)->JoinArray[i],LHS);
      DestroyBetaMemory(theEnv,&DefruleBinaryData(theEnv)->JoinArray[i],RHS);
      ReturnLeftMemory(theEnv,&DefruleBinaryData(theEnv)->JoinArray[i]);
      ReturnRightMemory(theEnv,&DefruleBinaryData(theEnv)->JoinArray[i]);
     }

   for (i = 0; i < DefruleBinaryData(theEnv)->NumberOfDefruleModules; i++)
     {
      theModuleItem = &DefruleBinaryData(theEnv)->ModuleArray[i];

      theActivation = theModuleItem->agenda;
      while (theActivation != NULL)
        {
         tmpActivation = theActivation->next;

         rtn_struct(theEnv,activation,theActivation);

         theActivation = tmpActivation;
        }

      theGroup = theModuleItem->groupings;
      while (theGroup != NULL)
        {
         tmpGroup = theGroup->next;

         rtn_struct(theEnv,salienceGroup,theGroup);

         theGroup = tmpGroup;
        }
     }

   space = DefruleBinaryData(theEnv)->NumberOfDefruleModules * sizeof(struct defruleModule);
   if (space != 0) genfree(theEnv,DefruleBinaryData(theEnv)->ModuleArray,space);

   space = DefruleBinaryData(theEnv)->NumberOfDefrules * sizeof(Defrule);
   if (space != 0) genfree(theEnv,DefruleBinaryData(theEnv)->DefruleArray,space);

   space = DefruleBinaryData(theEnv)->NumberOfJoins * sizeof(struct joinNode);
   if (space != 0) genfree(theEnv,DefruleBinaryData(theEnv)->JoinArray,space);

   space = DefruleBinaryData(theEnv)->NumberOfLinks * sizeof(struct joinLink);
   if (space != 0) genfree(theEnv,DefruleBinaryData(theEnv)->LinkArray,space);

   if (Bloaded(theEnv))
     { rm(theEnv,DefruleData(theEnv)->AlphaMemoryTable,sizeof(ALPHA_MEMORY_HASH *) * ALPHA_MEMORY_HASH_SIZE); }
#endif
  }

#if BLOAD_AND_BSAVE

/*************************************************************/
/* BsaveFind: Determines the amount of memory needed to save */
/*   the defrule and joinNode data structures in addition to */
/*   the memory needed for their associated expressions.     */
/*************************************************************/
static void BsaveFind(
  Environment *theEnv)
  {
   Defrule *theDefrule, *theDisjunct;
   Defmodule *theModule;

   /*=======================================================*/
   /* If a binary image is already loaded, then temporarily */
   /* save the count values since these will be overwritten */
   /* in the process of saving the binary image.            */
   /*=======================================================*/

   SaveBloadCount(theEnv,DefruleBinaryData(theEnv)->NumberOfDefruleModules);
   SaveBloadCount(theEnv,DefruleBinaryData(theEnv)->NumberOfDefrules);
   SaveBloadCount(theEnv,DefruleBinaryData(theEnv)->NumberOfJoins);
   SaveBloadCount(theEnv,DefruleBinaryData(theEnv)->NumberOfLinks);

   /*====================================================*/
   /* Set the binary save ID for defrule data structures */
   /* and count the number of each type.                 */
   /*====================================================*/

   TagRuleNetwork(theEnv,&DefruleBinaryData(theEnv)->NumberOfDefruleModules,
                         &DefruleBinaryData(theEnv)->NumberOfDefrules,
                         &DefruleBinaryData(theEnv)->NumberOfJoins,
                         &DefruleBinaryData(theEnv)->NumberOfLinks);

   /*===========================*/
   /* Loop through each module. */
   /*===========================*/

   for (theModule = GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = GetNextDefmodule(theEnv,theModule))
     {
      /*============================*/
      /* Set the current module to  */
      /* the module being examined. */
      /*============================*/

      SetCurrentModule(theEnv,theModule);

      /*==================================================*/
      /* Loop through each defrule in the current module. */
      /*==================================================*/

      for (theDefrule = GetNextDefrule(theEnv,NULL);
           theDefrule != NULL;
           theDefrule = GetNextDefrule(theEnv,theDefrule))
        {
         /*================================================*/
         /* Initialize the construct header for the binary */
         /* save. The binary save ID has already been set. */
         /*================================================*/

         MarkConstructHeaderNeededItems(&theDefrule->header,theDefrule->header.bsaveID);

         /*===========================================*/
         /* Count and mark data structures associated */
         /* with dynamic salience.                    */
         /*===========================================*/

         ExpressionData(theEnv)->ExpressionCount += ExpressionSize(theDefrule->dynamicSalience);
         MarkNeededItems(theEnv,theDefrule->dynamicSalience);

         /*==========================================*/
         /* Loop through each disjunct of the rule   */
         /* counting and marking the data structures */
         /* associated with RHS actions.             */
         /*==========================================*/

         for (theDisjunct = theDefrule;
              theDisjunct != NULL;
              theDisjunct = theDisjunct->disjunct)
           {
            ExpressionData(theEnv)->ExpressionCount += ExpressionSize(theDisjunct->actions);
            MarkNeededItems(theEnv,theDisjunct->actions);
           }
        }
     }

   /*===============================*/
   /* Reset the bsave tags assigned */
   /* to defrule data structures.   */
   /*===============================*/

   MarkRuleNetwork(theEnv,1);
  }

/************************************************/
/* BsaveExpressions: Saves the expressions used */
/*   by defrules to the binary save file.       */
/************************************************/
static void BsaveExpressions(
  Environment *theEnv,
  FILE *fp)
  {
   Defrule *theDefrule, *theDisjunct;
   Defmodule *theModule;

   /*===========================*/
   /* Loop through each module. */
   /*===========================*/

   for (theModule = GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = GetNextDefmodule(theEnv,theModule))
     {
      /*======================================================*/
      /* Set the current module to the module being examined. */
      /*======================================================*/

      SetCurrentModule(theEnv,theModule);

      /*==================================================*/
      /* Loop through each defrule in the current module. */
      /*==================================================*/

      for (theDefrule = GetNextDefrule(theEnv,NULL);
           theDefrule != NULL;
           theDefrule = GetNextDefrule(theEnv,theDefrule))
        {
         /*===========================================*/
         /* Save the dynamic salience of the defrule. */
         /*===========================================*/

         BsaveExpression(theEnv,theDefrule->dynamicSalience,fp);

         /*===================================*/
         /* Loop through each disjunct of the */
         /* defrule and save its RHS actions. */
         /*===================================*/

         for (theDisjunct = theDefrule;
              theDisjunct != NULL;
              theDisjunct = theDisjunct->disjunct)
           { BsaveExpression(theEnv,theDisjunct->actions,fp); }
        }
     }

   /*==============================*/
   /* Set the marked flag for each */
   /* join in the join network.    */
   /*==============================*/

   MarkRuleNetwork(theEnv,1);
  }

/*****************************************************/
/* BsaveStorage: Writes out storage requirements for */
/*   all defrule structures to the binary file       */
/*****************************************************/
static void BsaveStorage(
  Environment *theEnv,
  FILE *fp)
  {
   size_t space;
   unsigned long value;

   space = sizeof(long) * 5;
   GenWrite(&space,sizeof(size_t),fp);
   GenWrite(&DefruleBinaryData(theEnv)->NumberOfDefruleModules,sizeof(long),fp);
   GenWrite(&DefruleBinaryData(theEnv)->NumberOfDefrules,sizeof(long),fp);
   GenWrite(&DefruleBinaryData(theEnv)->NumberOfJoins,sizeof(long),fp);
   GenWrite(&DefruleBinaryData(theEnv)->NumberOfLinks,sizeof(long),fp);

   if (DefruleData(theEnv)->RightPrimeJoins == NULL)
     { value = ULONG_MAX; }
   else
     { value = DefruleData(theEnv)->RightPrimeJoins->bsaveID; }

   GenWrite(&value,sizeof(unsigned long),fp);

   if (DefruleData(theEnv)->LeftPrimeJoins == NULL)
     { value = ULONG_MAX; }
   else
     { value = DefruleData(theEnv)->LeftPrimeJoins->bsaveID; }

   GenWrite(&value,sizeof(unsigned long),fp);

   if (DefruleData(theEnv)->GoalPrimeJoins == NULL)
     { value = ULONG_MAX; }
   else
     { value = DefruleData(theEnv)->GoalPrimeJoins->bsaveID; }

   GenWrite(&value,sizeof(unsigned long),fp);
  }

/*******************************************/
/* BsaveBinaryItem: Writes out all defrule */
/*   structures to the binary file.        */
/*******************************************/
static void BsaveBinaryItem(
  Environment *theEnv,
  FILE *fp)
  {
   size_t space;
   Defrule *theDefrule;
   Defmodule *theModule;
   struct defruleModule *theModuleItem;
   struct bsaveDefruleModule tempDefruleModule;

   /*===============================================*/
   /* Write out the space required by the defrules. */
   /*===============================================*/

   space = (DefruleBinaryData(theEnv)->NumberOfDefrules * sizeof(struct bsaveDefrule)) +
           (DefruleBinaryData(theEnv)->NumberOfJoins * sizeof(struct bsaveJoinNode)) +
           (DefruleBinaryData(theEnv)->NumberOfLinks * sizeof(struct bsaveJoinLink)) +
           (DefruleBinaryData(theEnv)->NumberOfDefruleModules * sizeof(struct bsaveDefruleModule));
   GenWrite(&space,sizeof(size_t),fp);

   /*===============================================*/
   /* Write out each defrule module data structure. */
   /*===============================================*/

   DefruleBinaryData(theEnv)->NumberOfDefrules = 0;
   for (theModule = GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = GetNextDefmodule(theEnv,theModule))
     {
      SetCurrentModule(theEnv,theModule);

      theModuleItem = (struct defruleModule *)
                      GetModuleItem(theEnv,NULL,FindModuleItem(theEnv,"defrule")->moduleIndex);
      AssignBsaveDefmdlItemHdrHMVals(&tempDefruleModule.header,
                                     &theModuleItem->header);
      GenWrite(&tempDefruleModule,sizeof(struct bsaveDefruleModule),fp);
     }

   /*========================================*/
   /* Write out each defrule data structure. */
   /*========================================*/

   for (theModule = GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = GetNextDefmodule(theEnv,theModule))
     {
      SetCurrentModule(theEnv,theModule);

      for (theDefrule = GetNextDefrule(theEnv,NULL);
           theDefrule != NULL;
           theDefrule = GetNextDefrule(theEnv,theDefrule))
        { BsaveDisjuncts(theEnv,fp,theDefrule); }
     }

   /*=============================*/
   /* Write out the Rete Network. */
   /*=============================*/

   MarkRuleNetwork(theEnv,1);
   BsaveJoins(theEnv,fp);

   /*===========================*/
   /* Write out the join links. */
   /*===========================*/

   MarkRuleNetwork(theEnv,1);
   BsaveLinks(theEnv,fp);

   /*=============================================================*/
   /* If a binary image was already loaded when the bsave command */
   /* was issued, then restore the counts indicating the number   */
   /* of defrules, defrule modules, and joins in the binary image */
   /* (these were overwritten by the binary save).                */
   /*=============================================================*/

   RestoreBloadCount(theEnv,&DefruleBinaryData(theEnv)->NumberOfDefruleModules);
   RestoreBloadCount(theEnv,&DefruleBinaryData(theEnv)->NumberOfDefrules);
   RestoreBloadCount(theEnv,&DefruleBinaryData(theEnv)->NumberOfJoins);
   RestoreBloadCount(theEnv,&DefruleBinaryData(theEnv)->NumberOfLinks);
  }

/************************************************************/
/* BsaveDisjuncts: Writes out all the disjunct defrule data */
/*   structures for a specific rule to the binary file.     */
/************************************************************/
static void BsaveDisjuncts(
  Environment *theEnv,
  FILE *fp,
  Defrule *theDefrule)
  {
   Defrule *theDisjunct;
   struct bsaveDefrule tempDefrule;
   unsigned long disjunctExpressionCount = 0;
   bool first;

   /*=========================================*/
   /* Loop through each disjunct of the rule. */
   /*=========================================*/

   for (theDisjunct = theDefrule, first = true;
        theDisjunct != NULL;
        theDisjunct = theDisjunct->disjunct, first = false)
     {
      DefruleBinaryData(theEnv)->NumberOfDefrules++;

      /*======================================*/
      /* Set header and miscellaneous values. */
      /*======================================*/

      AssignBsaveConstructHeaderVals(&tempDefrule.header,
                                     &theDisjunct->header);
      tempDefrule.salience = theDisjunct->salience;
      tempDefrule.localVarCnt = theDisjunct->localVarCnt;
      tempDefrule.complexity = theDisjunct->complexity;
      tempDefrule.autoFocus = theDisjunct->autoFocus;

      /*=======================================*/
      /* Set dynamic salience data structures. */
      /*=======================================*/

      if (theDisjunct->dynamicSalience != NULL)
        {
         if (first)
           {
            tempDefrule.dynamicSalience = ExpressionData(theEnv)->ExpressionCount;
            disjunctExpressionCount = ExpressionData(theEnv)->ExpressionCount;
            ExpressionData(theEnv)->ExpressionCount += ExpressionSize(theDisjunct->dynamicSalience);
           }
         else
           { tempDefrule.dynamicSalience = disjunctExpressionCount; }
        }
      else
        { tempDefrule.dynamicSalience = ULONG_MAX; }

      /*==============================================*/
      /* Set the index to the disjunct's RHS actions. */
      /*==============================================*/

      if (theDisjunct->actions != NULL)
        {
         tempDefrule.actions = ExpressionData(theEnv)->ExpressionCount;
         ExpressionData(theEnv)->ExpressionCount += ExpressionSize(theDisjunct->actions);
        }
      else
        { tempDefrule.actions = ULONG_MAX; }

      /*=================================*/
      /* Set the index to the disjunct's */
      /* logical join and last join.     */
      /*=================================*/

      tempDefrule.logicalJoin = BsaveJoinIndex(theDisjunct->logicalJoin);
      tempDefrule.lastJoin = BsaveJoinIndex(theDisjunct->lastJoin);

      /*=====================================*/
      /* Set the index to the next disjunct. */
      /*=====================================*/

      if (theDisjunct->disjunct != NULL)
        { tempDefrule.disjunct = DefruleBinaryData(theEnv)->NumberOfDefrules; }
      else
        { tempDefrule.disjunct = ULONG_MAX; }

      /*=================================*/
      /* Write the disjunct to the file. */
      /*=================================*/

      GenWrite(&tempDefrule,sizeof(struct bsaveDefrule),fp);
     }
  }

/********************************************/
/* BsaveJoins: Writes out all the join node */
/*   data structures to the binary file.    */
/********************************************/
static void BsaveJoins(
  Environment *theEnv,
  FILE *fp)
  {
   Defrule *rulePtr, *disjunctPtr;
   Defmodule *theModule;

   /*===========================*/
   /* Loop through each module. */
   /*===========================*/

   for (theModule = GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = GetNextDefmodule(theEnv,theModule))
     {
      SetCurrentModule(theEnv,theModule);

      /*===========================================*/
      /* Loop through each rule and its disjuncts. */
      /*===========================================*/

      rulePtr = GetNextDefrule(theEnv,NULL);
      while (rulePtr != NULL)
        {
         /*=========================================*/
         /* Loop through each join of the disjunct. */
         /*=========================================*/

         for (disjunctPtr = rulePtr; disjunctPtr != NULL; disjunctPtr = disjunctPtr->disjunct)
           { BsaveTraverseJoins(theEnv,fp,disjunctPtr->lastJoin); }

         /*===========================*/
         /* Move on to the next rule. */
         /*===========================*/

         rulePtr = GetNextDefrule(theEnv,rulePtr);
        }
     }
  }

/**************************************************************/
/* BsaveTraverseJoins: Traverses the join network for a rule. */
/**************************************************************/
static void BsaveTraverseJoins(
  Environment *theEnv,
  FILE *fp,
  struct joinNode *joinPtr)
  {
   for (;
        joinPtr != NULL;
        joinPtr = joinPtr->lastLevel)
     {
      if (joinPtr->marked) BsaveJoin(theEnv,fp,joinPtr);

      if (joinPtr->joinFromTheRight)
        { BsaveTraverseJoins(theEnv,fp,(struct joinNode *) joinPtr->rightSideEntryStructure); }
     }
  }

/********************************************/
/* BsaveJoin: Writes out a single join node */
/*   data structure to the binary file.     */
/********************************************/
static void BsaveJoin(
  Environment *theEnv,
  FILE *fp,
  struct joinNode *joinPtr)
  {
   struct bsaveJoinNode tempJoin;

   joinPtr->marked = 0;
   tempJoin.depth = joinPtr->depth;
   tempJoin.rhsType = joinPtr->rhsType;
   tempJoin.firstJoin = joinPtr->firstJoin;
   tempJoin.logicalJoin = joinPtr->logicalJoin;
   tempJoin.goalJoin = joinPtr->goalJoin;
   tempJoin.explicitJoin = joinPtr->explicitJoin;
   tempJoin.joinFromTheRight = joinPtr->joinFromTheRight;
   tempJoin.patternIsNegated = joinPtr->patternIsNegated;
   tempJoin.patternIsExists = joinPtr->patternIsExists;

   if (joinPtr->joinFromTheRight)
     { tempJoin.rightSideEntryStructure = BsaveJoinIndex(joinPtr->rightSideEntryStructure); }
   else
     { tempJoin.rightSideEntryStructure = ULONG_MAX; }

   tempJoin.lastLevel =  BsaveJoinIndex(joinPtr->lastLevel);
   tempJoin.nextLinks =  BsaveJoinLinkIndex(joinPtr->nextLinks);
   tempJoin.rightMatchNode =  BsaveJoinIndex(joinPtr->rightMatchNode);
   tempJoin.networkTest = HashedExpressionIndex(theEnv,joinPtr->networkTest);
   tempJoin.secondaryNetworkTest = HashedExpressionIndex(theEnv,joinPtr->secondaryNetworkTest);
   tempJoin.goalExpression = HashedExpressionIndex(theEnv,joinPtr->goalExpression);
   tempJoin.leftHash = HashedExpressionIndex(theEnv,joinPtr->leftHash);
   tempJoin.rightHash = HashedExpressionIndex(theEnv,joinPtr->rightHash);

   if (joinPtr->ruleToActivate != NULL)
     {
      tempJoin.ruleToActivate =
         GetDisjunctIndex(joinPtr->ruleToActivate);
     }
   else
     { tempJoin.ruleToActivate = ULONG_MAX; }

   GenWrite(&tempJoin,sizeof(struct bsaveJoinNode),fp);
  }

/********************************************/
/* BsaveLinks: Writes out all the join link */
/*   data structures to the binary file.    */
/********************************************/
static void BsaveLinks(
  Environment *theEnv,
  FILE *fp)
  {
   Defrule *rulePtr, *disjunctPtr;
   Defmodule *theModule;
   struct joinLink *theLink;

   for (theLink = DefruleData(theEnv)->LeftPrimeJoins;
        theLink != NULL;
        theLink = theLink->next)
     { BsaveLink(fp,theLink);  }

   for (theLink = DefruleData(theEnv)->RightPrimeJoins;
        theLink != NULL;
        theLink = theLink->next)
     { BsaveLink(fp,theLink);  }

   for (theLink = DefruleData(theEnv)->GoalPrimeJoins;
        theLink != NULL;
        theLink = theLink->next)
     { BsaveLink(fp,theLink);  }

   /*===========================*/
   /* Loop through each module. */
   /*===========================*/

   for (theModule = GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = GetNextDefmodule(theEnv,theModule))
     {
      SetCurrentModule(theEnv,theModule);

      /*===========================================*/
      /* Loop through each rule and its disjuncts. */
      /*===========================================*/

      rulePtr = GetNextDefrule(theEnv,NULL);
      while (rulePtr != NULL)
        {
         /*=========================================*/
         /* Loop through each join of the disjunct. */
         /*=========================================*/

         for (disjunctPtr = rulePtr; disjunctPtr != NULL; disjunctPtr = disjunctPtr->disjunct)
           { BsaveTraverseLinks(theEnv,fp,disjunctPtr->lastJoin); }

         /*=======================================*/
         /* Move on to the next rule or disjunct. */
         /*=======================================*/

         rulePtr = GetNextDefrule(theEnv,rulePtr);
        }
     }
  }

/***************************************************/
/* BsaveTraverseLinks: Traverses the join network */
/*   for a rule saving the join links.            */
/**************************************************/
static void BsaveTraverseLinks(
  Environment *theEnv,
  FILE *fp,
  struct joinNode *joinPtr)
  {
   struct joinLink *theLink;

   for (;
        joinPtr != NULL;
        joinPtr = joinPtr->lastLevel)
     {
      if (joinPtr->marked)
        {
         for (theLink = joinPtr->nextLinks;
              theLink != NULL;
              theLink = theLink->next)
           { BsaveLink(fp,theLink); }

         joinPtr->marked = 0;
        }

      if (joinPtr->joinFromTheRight)
        { BsaveTraverseLinks(theEnv,fp,(struct joinNode *) joinPtr->rightSideEntryStructure); }
     }
  }

/********************************************/
/* BsaveLink: Writes out a single join link */
/*   data structure to the binary file.     */
/********************************************/
static void BsaveLink(
  FILE *fp,
  struct joinLink *linkPtr)
  {
   struct bsaveJoinLink tempLink;

   tempLink.enterDirection = linkPtr->enterDirection;
   tempLink.join =  BsaveJoinIndex(linkPtr->join);
   tempLink.next =  BsaveJoinLinkIndex(linkPtr->next);

   GenWrite(&tempLink,sizeof(struct bsaveJoinLink),fp);
  }

/***********************************************************/
/* AssignBsavePatternHeaderValues: Assigns the appropriate */
/*   values to a bsave pattern header record.              */
/***********************************************************/
void AssignBsavePatternHeaderValues(
  Environment *theEnv,
  struct bsavePatternNodeHeader *theBsaveHeader,
  struct patternNodeHeader *theHeader)
  {
   theBsaveHeader->multifieldNode = theHeader->multifieldNode;
   theBsaveHeader->entryJoin = BsaveJoinIndex(theHeader->entryJoin);
   theBsaveHeader->rightHash = HashedExpressionIndex(theEnv,theHeader->rightHash);
   theBsaveHeader->singlefieldNode = theHeader->singlefieldNode;
   theBsaveHeader->stopNode = theHeader->stopNode;
   theBsaveHeader->beginSlot = theHeader->beginSlot;
   theBsaveHeader->endSlot = theHeader->endSlot;
   theBsaveHeader->selector = theHeader->selector;
  }

#endif /* BLOAD_AND_BSAVE */

/************************************************/
/* BloadStorage: Loads storage requirements for */
/*   the defrules used by this binary image.    */
/************************************************/
static void BloadStorage(
  Environment *theEnv)
  {
   size_t space;

   /*=================================================*/
   /* Determine the number of defrule, defruleModule, */
   /* and joinNode data structures to be read.        */
   /*=================================================*/

   GenReadBinary(theEnv,&space,sizeof(size_t));
   GenReadBinary(theEnv,&DefruleBinaryData(theEnv)->NumberOfDefruleModules,sizeof(long));
   GenReadBinary(theEnv,&DefruleBinaryData(theEnv)->NumberOfDefrules,sizeof(long));
   GenReadBinary(theEnv,&DefruleBinaryData(theEnv)->NumberOfJoins,sizeof(long));
   GenReadBinary(theEnv,&DefruleBinaryData(theEnv)->NumberOfLinks,sizeof(long));
   GenReadBinary(theEnv,&DefruleBinaryData(theEnv)->RightPrimeIndex,sizeof(long));
   GenReadBinary(theEnv,&DefruleBinaryData(theEnv)->LeftPrimeIndex,sizeof(long));
   GenReadBinary(theEnv,&DefruleBinaryData(theEnv)->GoalPrimeIndex,sizeof(long));

   /*===================================*/
   /* Allocate the space needed for the */
   /* defruleModule data structures.    */
   /*===================================*/

   if (DefruleBinaryData(theEnv)->NumberOfDefruleModules == 0)
     {
      DefruleBinaryData(theEnv)->ModuleArray = NULL;
      DefruleBinaryData(theEnv)->DefruleArray = NULL;
      DefruleBinaryData(theEnv)->JoinArray = NULL;
     }

   space = DefruleBinaryData(theEnv)->NumberOfDefruleModules * sizeof(struct defruleModule);
   DefruleBinaryData(theEnv)->ModuleArray = (struct defruleModule *) genalloc(theEnv,space);

   /*===============================*/
   /* Allocate the space needed for */
   /* the defrule data structures.  */
   /*===============================*/

   if (DefruleBinaryData(theEnv)->NumberOfDefrules == 0)
     {
      DefruleBinaryData(theEnv)->DefruleArray = NULL;
      DefruleBinaryData(theEnv)->JoinArray = NULL;
      return;
     }

   space = DefruleBinaryData(theEnv)->NumberOfDefrules * sizeof(Defrule);
   DefruleBinaryData(theEnv)->DefruleArray = (Defrule *) genalloc(theEnv,space);

   /*===============================*/
   /* Allocate the space needed for */
   /* the joinNode data structures. */
   /*===============================*/

   space = DefruleBinaryData(theEnv)->NumberOfJoins * sizeof(struct joinNode);
   DefruleBinaryData(theEnv)->JoinArray = (struct joinNode *) genalloc(theEnv,space);

   /*===============================*/
   /* Allocate the space needed for */
   /* the joinNode data structures. */
   /*===============================*/

   space = DefruleBinaryData(theEnv)->NumberOfLinks * sizeof(struct joinLink);
   DefruleBinaryData(theEnv)->LinkArray = (struct joinLink *) genalloc(theEnv,space);
  }

/****************************************************/
/* BloadBinaryItem: Loads and refreshes the defrule */
/*   constructs used by this binary image.          */
/****************************************************/
static void BloadBinaryItem(
  Environment *theEnv)
  {
   size_t space;

   /*======================================================*/
   /* Read in the amount of space used by the binary image */
   /* (this is used to skip the construct in the event it  */
   /* is not available in the version being run).          */
   /*======================================================*/

   GenReadBinary(theEnv,&space,sizeof(size_t));

   /*===========================================*/
   /* Read in the defruleModule data structures */
   /* and refresh the pointers.                 */
   /*===========================================*/

   BloadandRefresh(theEnv,DefruleBinaryData(theEnv)->NumberOfDefruleModules,
                   sizeof(struct bsaveDefruleModule),UpdateDefruleModule);

   /*=====================================*/
   /* Read in the defrule data structures */
   /* and refresh the pointers.           */
   /*=====================================*/

   BloadandRefresh(theEnv,DefruleBinaryData(theEnv)->NumberOfDefrules,
                   sizeof(struct bsaveDefrule),UpdateDefrule);

   /*======================================*/
   /* Read in the joinNode data structures */
   /* and refresh the pointers.            */
   /*======================================*/

   BloadandRefresh(theEnv,DefruleBinaryData(theEnv)->NumberOfJoins,
                   sizeof(struct bsaveJoinNode),UpdateJoin);

   /*======================================*/
   /* Read in the joinLink data structures */
   /* and refresh the pointers.            */
   /*======================================*/

   BloadandRefresh(theEnv,DefruleBinaryData(theEnv)->NumberOfLinks,
                   sizeof(struct bsaveJoinLink),UpdateLink);

   DefruleData(theEnv)->RightPrimeJoins = BloadJoinLinkPointer(DefruleBinaryData(theEnv)->RightPrimeIndex);
   DefruleData(theEnv)->LeftPrimeJoins = BloadJoinLinkPointer(DefruleBinaryData(theEnv)->LeftPrimeIndex);
   DefruleData(theEnv)->GoalPrimeJoins = BloadJoinLinkPointer(DefruleBinaryData(theEnv)->GoalPrimeIndex);
  }

/**********************************************/
/* UpdateDefruleModule: Bload refresh routine */
/*   for defrule module data structures.      */
/**********************************************/
static void UpdateDefruleModule(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   struct bsaveDefruleModule *bdmPtr;

   bdmPtr = (struct bsaveDefruleModule *) buf;
   UpdateDefmoduleItemHeaderHM(theEnv,&bdmPtr->header,&DefruleBinaryData(theEnv)->ModuleArray[obji].header,
                               sizeof(Defrule),
                               (void *) DefruleBinaryData(theEnv)->DefruleArray);
   DefruleBinaryData(theEnv)->ModuleArray[obji].agenda = NULL;
   DefruleBinaryData(theEnv)->ModuleArray[obji].groupings = NULL;
   
   AssignHashMapSize(theEnv,&DefruleBinaryData(theEnv)->ModuleArray[obji].header,bdmPtr->header.itemCount);
  }

/****************************************/
/* UpdateDefrule: Bload refresh routine */
/*   for defrule data structures.       */
/****************************************/
static void UpdateDefrule(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   struct bsaveDefrule *br;

   br = (struct bsaveDefrule *) buf;
   UpdateConstructHeader(theEnv,&br->header,&DefruleBinaryData(theEnv)->DefruleArray[obji].header,DEFRULE,
                         sizeof(struct defruleModule),(void *) DefruleBinaryData(theEnv)->ModuleArray,
                         sizeof(Defrule),(void *) DefruleBinaryData(theEnv)->DefruleArray);

   DefruleBinaryData(theEnv)->DefruleArray[obji].dynamicSalience = ExpressionPointer(br->dynamicSalience);

   DefruleBinaryData(theEnv)->DefruleArray[obji].actions = ExpressionPointer(br->actions);
   DefruleBinaryData(theEnv)->DefruleArray[obji].logicalJoin = BloadJoinPointer(br->logicalJoin);
   DefruleBinaryData(theEnv)->DefruleArray[obji].lastJoin = BloadJoinPointer(br->lastJoin);
   DefruleBinaryData(theEnv)->DefruleArray[obji].disjunct = BloadDefrulePointer(DefruleBinaryData(theEnv)->DefruleArray,br->disjunct);
   DefruleBinaryData(theEnv)->DefruleArray[obji].salience = br->salience;
   DefruleBinaryData(theEnv)->DefruleArray[obji].localVarCnt = br->localVarCnt;
   DefruleBinaryData(theEnv)->DefruleArray[obji].complexity = br->complexity;
   DefruleBinaryData(theEnv)->DefruleArray[obji].autoFocus = br->autoFocus;
   DefruleBinaryData(theEnv)->DefruleArray[obji].executing = 0;
   DefruleBinaryData(theEnv)->DefruleArray[obji].afterBreakpoint = 0;
#if DEBUGGING_FUNCTIONS
   DefruleBinaryData(theEnv)->DefruleArray[obji].watchActivation = AgendaData(theEnv)->WatchActivations;
   DefruleBinaryData(theEnv)->DefruleArray[obji].watchFiring = DefruleData(theEnv)->WatchRules;
#endif

   AddConstructToHashMap(theEnv,&DefruleBinaryData(theEnv)->DefruleArray[obji].header,
                         DefruleBinaryData(theEnv)->DefruleArray[obji].header.whichModule);
  }

/*************************************/
/* UpdateJoin: Bload refresh routine */
/*   for joinNode data structures.   */
/*************************************/
static void UpdateJoin(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   struct bsaveJoinNode *bj;

   bj = (struct bsaveJoinNode *) buf;
   DefruleBinaryData(theEnv)->JoinArray[obji].firstJoin = bj->firstJoin;
   DefruleBinaryData(theEnv)->JoinArray[obji].logicalJoin = bj->logicalJoin;
   DefruleBinaryData(theEnv)->JoinArray[obji].goalJoin = bj->goalJoin;
   DefruleBinaryData(theEnv)->JoinArray[obji].explicitJoin = bj->explicitJoin;
   DefruleBinaryData(theEnv)->JoinArray[obji].joinFromTheRight = bj->joinFromTheRight;
   DefruleBinaryData(theEnv)->JoinArray[obji].patternIsNegated = bj->patternIsNegated;
   DefruleBinaryData(theEnv)->JoinArray[obji].patternIsExists = bj->patternIsExists;
   DefruleBinaryData(theEnv)->JoinArray[obji].depth = bj->depth;
   DefruleBinaryData(theEnv)->JoinArray[obji].rhsType = bj->rhsType;
   DefruleBinaryData(theEnv)->JoinArray[obji].networkTest = HashedExpressionPointer(bj->networkTest);
   DefruleBinaryData(theEnv)->JoinArray[obji].secondaryNetworkTest = HashedExpressionPointer(bj->secondaryNetworkTest);
   DefruleBinaryData(theEnv)->JoinArray[obji].goalExpression = HashedExpressionPointer(bj->goalExpression);
   DefruleBinaryData(theEnv)->JoinArray[obji].leftHash = HashedExpressionPointer(bj->leftHash);
   DefruleBinaryData(theEnv)->JoinArray[obji].rightHash = HashedExpressionPointer(bj->rightHash);
   DefruleBinaryData(theEnv)->JoinArray[obji].nextLinks = BloadJoinLinkPointer(bj->nextLinks);
   DefruleBinaryData(theEnv)->JoinArray[obji].lastLevel = BloadJoinPointer(bj->lastLevel);

   if (bj->joinFromTheRight == true)
     { DefruleBinaryData(theEnv)->JoinArray[obji].rightSideEntryStructure =  (void *) BloadJoinPointer(bj->rightSideEntryStructure); }
   else
     { DefruleBinaryData(theEnv)->JoinArray[obji].rightSideEntryStructure = NULL; }

   DefruleBinaryData(theEnv)->JoinArray[obji].rightMatchNode = BloadJoinPointer(bj->rightMatchNode);
   DefruleBinaryData(theEnv)->JoinArray[obji].ruleToActivate = BloadDefrulePointer(DefruleBinaryData(theEnv)->DefruleArray,bj->ruleToActivate);
   DefruleBinaryData(theEnv)->JoinArray[obji].initialize = 0;
   DefruleBinaryData(theEnv)->JoinArray[obji].marked = 0;
   DefruleBinaryData(theEnv)->JoinArray[obji].bsaveID = 0L;
   DefruleBinaryData(theEnv)->JoinArray[obji].leftMemory = NULL;
   DefruleBinaryData(theEnv)->JoinArray[obji].rightMemory = NULL;

   AddBetaMemoriesToJoin(theEnv,&DefruleBinaryData(theEnv)->JoinArray[obji]);
  }

/*************************************/
/* UpdateLink: Bload refresh routine */
/*   for joinLink data structures.   */
/*************************************/
static void UpdateLink(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   struct bsaveJoinLink *bj;

   bj = (struct bsaveJoinLink *) buf;
   DefruleBinaryData(theEnv)->LinkArray[obji].enterDirection = bj->enterDirection;
   DefruleBinaryData(theEnv)->LinkArray[obji].next = BloadJoinLinkPointer(bj->next);
   DefruleBinaryData(theEnv)->LinkArray[obji].join = BloadJoinPointer(bj->join);
  }

/************************************************************/
/* UpdatePatternNodeHeader: Refreshes the values in pattern */
/*   node headers from the loaded binary image.             */
/************************************************************/
void UpdatePatternNodeHeader(
  Environment *theEnv,
  struct patternNodeHeader *theHeader,
  struct bsavePatternNodeHeader *theBsaveHeader)
  {
   struct joinNode *theJoin;

   theHeader->singlefieldNode = theBsaveHeader->singlefieldNode;
   theHeader->multifieldNode = theBsaveHeader->multifieldNode;
   theHeader->stopNode = theBsaveHeader->stopNode;
   theHeader->beginSlot = theBsaveHeader->beginSlot;
   theHeader->endSlot = theBsaveHeader->endSlot;
   theHeader->selector = theBsaveHeader->selector;
   theHeader->initialize = 0;
   theHeader->marked = 0;
   theHeader->firstHash = NULL;
   theHeader->lastHash = NULL;
   theHeader->rightHash = HashedExpressionPointer(theBsaveHeader->rightHash);

   theJoin = BloadJoinPointer(theBsaveHeader->entryJoin);
   theHeader->entryJoin = theJoin;

   while (theJoin != NULL)
     {
      theJoin->rightSideEntryStructure = (void *) theHeader;
      theJoin = theJoin->rightMatchNode;
     }
  }

/**************************************/
/* ClearBload: Defrule clear routine  */
/*   when a binary load is in effect. */
/**************************************/
static void ClearBload(
  Environment *theEnv)
  {
   size_t space;
   unsigned long i;
   struct patternParser *theParser = NULL;
   struct patternEntity *theEntity = NULL;
   Defmodule *theModule;
   
   /*==========================================*/
   /* Deallocate the DeftableModule hash maps. */
   /*==========================================*/

   for (i = 0; i < DefruleBinaryData(theEnv)->NumberOfDefruleModules; i++)
     { ClearDefmoduleHashMap(theEnv,&DefruleBinaryData(theEnv)->ModuleArray[i].header); }

   /*===========================================*/
   /* Delete all known entities before removing */
   /* the defrule data structures.              */
   /*===========================================*/

   GetNextPatternEntity(theEnv,&theParser,&theEntity);
   while (theEntity != NULL)
     {
      (*theEntity->theInfo->base.deleteFunction)(theEntity,theEnv);
      theEntity = NULL;
      GetNextPatternEntity(theEnv,&theParser,&theEntity);
     }

   /*=========================================*/
   /* Remove all activations from the agenda. */
   /*=========================================*/

   SaveCurrentModule(theEnv);
   for (theModule = GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = GetNextDefmodule(theEnv,theModule))
     {
      SetCurrentModule(theEnv,theModule);
      RemoveAllActivations(theEnv);
     }
   RestoreCurrentModule(theEnv);
   ClearFocusStack(theEnv);

   /*==========================================================*/
   /* Remove all partial matches from the beta memories in the */
   /* join network. Alpha memories do not need to be examined  */
   /* since all pattern entities have been deleted by now.     */
   /*==========================================================*/

   for (i = 0; i < DefruleBinaryData(theEnv)->NumberOfJoins; i++)
     {
      FlushBetaMemory(theEnv,&DefruleBinaryData(theEnv)->JoinArray[i],LHS);
      ReturnLeftMemory(theEnv,&DefruleBinaryData(theEnv)->JoinArray[i]);
      FlushBetaMemory(theEnv,&DefruleBinaryData(theEnv)->JoinArray[i],RHS);
      ReturnRightMemory(theEnv,&DefruleBinaryData(theEnv)->JoinArray[i]);
     }

   /*================================================*/
   /* Decrement the symbol count for each rule name. */
   /*================================================*/

   for (i = 0; i < DefruleBinaryData(theEnv)->NumberOfDefrules; i++)
     { UnmarkConstructHeader(theEnv,&DefruleBinaryData(theEnv)->DefruleArray[i].header); }

   /*==================================================*/
   /* Return the space allocated for the bload arrays. */
   /*==================================================*/

   space = DefruleBinaryData(theEnv)->NumberOfDefruleModules * sizeof(struct defruleModule);
   if (space != 0) genfree(theEnv,DefruleBinaryData(theEnv)->ModuleArray,space);
   DefruleBinaryData(theEnv)->NumberOfDefruleModules = 0;

   space = DefruleBinaryData(theEnv)->NumberOfDefrules * sizeof(Defrule);
   if (space != 0) genfree(theEnv,DefruleBinaryData(theEnv)->DefruleArray,space);
   DefruleBinaryData(theEnv)->NumberOfDefrules = 0;

   space = DefruleBinaryData(theEnv)->NumberOfJoins * sizeof(struct joinNode);
   if (space != 0) genfree(theEnv,DefruleBinaryData(theEnv)->JoinArray,space);
   DefruleBinaryData(theEnv)->NumberOfJoins = 0;

   space = DefruleBinaryData(theEnv)->NumberOfLinks * sizeof(struct joinLink);
   if (space != 0) genfree(theEnv,DefruleBinaryData(theEnv)->LinkArray,space);
   DefruleBinaryData(theEnv)->NumberOfLinks = 0;

   DefruleData(theEnv)->RightPrimeJoins = NULL;
   DefruleData(theEnv)->LeftPrimeJoins = NULL;
   DefruleData(theEnv)->GoalPrimeJoins = NULL;
  }

/*******************************************************/
/* BloadDefruleModuleReference: Returns the defrule    */
/*   module pointer for using with the bload function. */
/*******************************************************/
void *BloadDefruleModuleReference(
  Environment *theEnv,
  unsigned long theIndex)
  {
   return ((void *) &DefruleBinaryData(theEnv)->ModuleArray[theIndex]);
  }

#endif /* DEFRULE_CONSTRUCT && (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME) */


