   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 7.00  12/24/23            */
   /*                                                     */
   /*                   FACT GOAL MODULE                  */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides core routines for maintaining the fact  */
/*   list including assert/retract operations, data          */
/*   structure creation/deletion, printing, slot access,     */
/*   and other utility functions.                            */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      7.00: Support for data driven backward chaining.     */
/*                                                           */
/*************************************************************/

#include <stdio.h>

#include "setup.h"

#if DEFTEMPLATE_CONSTRUCT && DEFRULE_CONSTRUCT

#include "engine.h"
#include "factmngr.h"
#include "factgen.h"
#include "memalloc.h"
#include "multifld.h"
#include "prntutil.h"
#include "reteutil.h"
#include "router.h"

#include "factgoal.h"
  
#define DEBUG_GOAL_GENERATION 0
#define DEBUG_GOAL_COUNT 0

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static void                    FindConstantLengthAndMultifieldNodes(Environment *,struct factPatternNode *,struct extractedInfo **,Expression **);
   static void                    AddMultifieldInfo(Environment *,struct extractedInfo **,unsigned int,unsigned short);
   static struct extractedInfo   *CreateExtractionInfo(Environment *,unsigned short,bool,bool,bool,void *);
   static void                    AddConstantInfo(Environment *,struct extractedInfo **,unsigned int,unsigned short,bool,bool,Expression *);
   static void                    DetermineSlotFieldCounts(Environment *,unsigned short numberOfSlots,struct extractedInfo **,
                                                           bool *,Expression **,unsigned short *);
   static unsigned short          ComputeNumberOfFields(Environment *,struct extractedInfo **,unsigned int,bool,unsigned short);
   static void                    InferValuesFromJoinNetworkComparisons(Environment *,struct joinNode *,struct partialMatch *,
                                                                        struct extractedInfo **,unsigned short *);
   static void                    InferValueFromJoinNetworkEq(Environment *,struct joinNode *,struct partialMatch *,Expression *,
                                                              struct extractedInfo **,unsigned short *);
   static void                    InferValuesFromPatternNetworkComparisons(Environment *,struct factPatternNode *,struct extractedInfo **,
                                                                           Expression **,struct joinNode *,
                                                                           struct partialMatch *,unsigned short *);
   static void                    InferValueFromPatternNetworkEq(Environment *,Expression *,struct extractedInfo **,Expression **,
                                                                 unsigned short,struct joinNode *,
                                                                 struct partialMatch *,unsigned short *);
   static void                    AddToInfoArray(struct extractedInfo **,struct extractedInfo *,unsigned int);
   static struct extractedInfo   *FindExtractedInfo(struct extractedInfo **,unsigned short,unsigned short);
   static bool                    UnboundReferences(Expression *,struct extractedInfo **);
   static void                    InferWildcardPositions(Environment *,struct extractedInfo **,Expression **,unsigned short);
   static void                    InferWildcardPositionsForSlot(Environment *,struct extractedInfo **,unsigned int,bool,unsigned short);
   static void                    InferWildcardPositionsForSlotWithImplicitMultifield(Environment *,struct extractedInfo **,unsigned int,unsigned short);
   static void                    PopulateGoalValues(Environment *,struct fact *,unsigned short,struct extractedInfo **,
                                                     Expression **,bool *);
   static void                    TraceGoalToRuleDriver(Environment *,struct joinNode *,struct partialMatch *,const char *,bool);

#if DEBUG_GOAL_GENERATION
   static void                    PrintExtractedInfo(Environment *,struct extractedInfo **,unsigned short);
#endif

/***************/
/* AttachGoal: */
/***************/
void AttachGoal(
  Environment *theEnv,
  struct joinNode *join,
  struct partialMatch *deriveBinds,
  struct partialMatch *attachBinds,
  bool queue)
  {
   Fact *theGoal;
      
   if (FactData(theEnv)->goalGenerationDisabled)
     { return; }

   theGoal = GenerateGoal(theEnv,join,deriveBinds);
   
   if (theGoal == NULL)
     { return; }
     
   attachBinds->marker = theGoal;
   attachBinds->goalMarker = true;
   if (queue)
     { AddToGoalQueue(theGoal,attachBinds,ASSERT_ACTION); }
   else if (theGoal->pendingAssert)
     { AssertGoal(theGoal,attachBinds); }
  }

/**********************/
/* UpdateGoalSupport: */
/**********************/
void UpdateGoalSupport(
  Environment *theEnv,
  struct partialMatch *goalParent,
  bool queue)
  {
   Fact *theGoal;
   
   if (! goalParent->goalMarker)
     {
      SystemError(theEnv,"FACTGOAL",1);
      ExitRouter(theEnv,EXIT_FAILURE);
     }
   
   theGoal = goalParent->marker;
   
   if (theGoal == NULL)
     {
      SystemError(theEnv,"FACTGOAL",2);
      ExitRouter(theEnv,EXIT_FAILURE);
     }
   
   if (theGoal->supportCount == 0)
     {
      SystemError(theEnv,"FACTGOAL",3);
      ExitRouter(theEnv,EXIT_FAILURE);
     }
   
   goalParent->goalMarker = false;
   goalParent->marker = NULL;
   
   /*===========================================*/
   /* Decrement the count of partial matches    */
   /* that generated the goals and if non-zero, */
   /* no further action.                        */
   /*===========================================*/

   theGoal->supportCount--;
   if (theGoal->supportCount != 0)
     {
#if DEBUGGING_FUNCTIONS && DEBUG_GOAL_COUNT
   if (theGoal->whichDeftemplate->watchGoals)
     {
      WriteString(theEnv,STDOUT,"--- ");
      PrintFactWithIdentifier(theEnv,STDOUT,theGoal,NULL);
      WriteString(theEnv,STDOUT,"\n");
      TraceGoalToRule(theEnv,(struct joinNode *) goalParent->owner,goalParent,"    ");
     }
#endif

      return;
     }

   /*=================================*/
   /* Queue a retraction of the goal. */
   /*=================================*/
   
   if (queue)
     { AddToGoalQueue(theGoal,goalParent,RETRACT_ACTION); }
   else 
     { RetractGoal(theGoal); }
  }

/*********************/
/* ProcessGoalQueue: */
/*********************/
void ProcessGoalQueue(
  Environment *theEnv)
  {
   struct queueItem *theQueueItem;
   Fact *theGoal;
   struct partialMatch *theMatch;
   GoalQueueAction theAction;
   
   theQueueItem = FactData(theEnv)->goalQueue;
   while (theQueueItem != NULL)
     {
      FactData(theEnv)->goalQueue = theQueueItem->nextInQueue;
      
      theGoal = theQueueItem->theFact;
      theMatch = theQueueItem->theMatch;
      theAction = theQueueItem->action;
      rtn_struct(theEnv,queueItem,theQueueItem);
      
      switch(theAction)
        {
         case RETRACT_ACTION:
           RetractGoal(theGoal);
           break;
           
         case ASSERT_ACTION:
           AssertGoal(theGoal,theMatch);
           break;
        }
      
      theQueueItem = FactData(theEnv)->goalQueue;
     }
  }
  
/********************************************/
/* AddToGoalQueue: Adds a goal to the queue */
/*    of goals to be asserted or retracted. */
/********************************************/
void AddToGoalQueue(
  Fact *theGoal,
  struct partialMatch *theMatch,
  GoalQueueAction actionType)
  {
   struct queueItem *theAction, *lastAction;
   Environment *theEnv = theGoal->whichDeftemplate->header.env;

   /*=================================================*/
   /* If the goal is being retracted, but has not yet */
   /* been asserted, then search for pending assert   */
   /* action in the goal queue and remove it.         */
   /*=================================================*/
   
   if ((actionType == RETRACT_ACTION) &&
       (theGoal->pendingAssert == true) &&
       (theGoal->supportCount == 0))
     {
      lastAction = NULL;
      for (theAction = FactData(theEnv)->goalQueue;
           theAction != NULL;
           theAction = theAction->nextInQueue)
        {
         if (theAction->theFact == theGoal)
           {
            if (theAction == FactData(theEnv)->lastInGoalQueue)
              { FactData(theEnv)->lastInGoalQueue = lastAction; }
            
            if (theAction == FactData(theEnv)->goalQueue)
              { FactData(theEnv)->goalQueue = theAction->nextInQueue; }
              
            if (lastAction != NULL)
              { lastAction->nextInQueue = theAction->nextInQueue; }
              
            rtn_struct(theEnv,queueItem,theAction);
            RemoveHashedFact(theEnv,theGoal);
            ReturnFact(theEnv,theGoal);

            return;
           }
           
         lastAction = theAction;
        }
      
      SystemError(theEnv,"FACTGOAL",4);
      return;
     }
     
   /*============================================*/
   /* If the goal has already been asserted and  */
   /* and there's a pending retract and the goal */
   /* is reasserted, remove the pending retract. */
   /*============================================*/

   if ((actionType == ASSERT_ACTION) &&
       (theGoal->pendingAssert == false) &&
       (theGoal->supportCount > 0))
     {
      lastAction = NULL;
      for (theAction = FactData(theEnv)->goalQueue;
           theAction != NULL;
           theAction = theAction->nextInQueue)
        {
         if (theAction->theFact == theGoal)
           {
            if (theAction == FactData(theEnv)->lastInGoalQueue)
              { FactData(theEnv)->lastInGoalQueue = lastAction; }
            
            if (theAction == FactData(theEnv)->goalQueue)
              { FactData(theEnv)->goalQueue = theAction->nextInQueue; }
              
            if (lastAction != NULL)
              { lastAction->nextInQueue = theAction->nextInQueue; }
              
            rtn_struct(theEnv,queueItem,theAction);

            return;
           }
           
         lastAction = theAction;
        }
      
      return;
     }

   /*===================================================*/
   /* If the queue action is to assert and the goal has */
   /* already been asserted or is already in the queue, */
   /* it does not need to be queued.                    */
   /*===================================================*/
   
   if ((actionType == ASSERT_ACTION) &&
       ((theGoal->pendingAssert == false) ||
        (theGoal->supportCount > 1)))
     { return; }
     
   theAction = get_struct(theEnv,queueItem);
   theAction->action = actionType;
   theAction->theFact = theGoal;
   theAction->theMatch = theMatch;
   theAction->nextInQueue = NULL;
   
   if (FactData(theEnv)->goalQueue == NULL)
     { FactData(theEnv)->goalQueue = theAction; }
   else
     { FactData(theEnv)->lastInGoalQueue->nextInQueue = theAction; }
    
   FactData(theEnv)->lastInGoalQueue = theAction;
  }

/*****************/
/* GenerateGoal: */
/*****************/
Fact *GenerateGoal(
  Environment *theEnv,
  struct joinNode *join,
  struct partialMatch *lhsBinds)
  {
   struct deftemplate *theDeftemplate;
   struct fact *newGoal;
   unsigned short i, j;
   struct templateSlot *theSlot;
   size_t hashValue;
   Fact *existing;
   struct extractedInfo *theInfo, *nextInfo;
   bool *cardinalityArray;
   unsigned short *fieldCountArray;
   Expression **lengthArray;
   Multifield *theMultifield;
   size_t theLength;
   unsigned short numberOfSlots;
   struct extractedInfo **infoArray;

   theDeftemplate = (struct deftemplate *) join->goalExpression->value;

   newGoal = CreateFact(theDeftemplate);
   newGoal->goal = true;
   newGoal->pendingAssert = true;
   
   /*============================================*/
   /* Determine the number of slots in the goal. */
   /*============================================*/
   
   if (theDeftemplate->implied)
     { numberOfSlots = 1; }
   else
     { numberOfSlots = theDeftemplate->numberOfSlots; }

   /*=====================================*/
   /* Allocate storage for values derived */
   /* from join and pattern networks.     */
   /*=====================================*/
        
   if (numberOfSlots != 0)
     {
      lengthArray = genalloc(theEnv,sizeof(Expression *) * numberOfSlots);
      memset(lengthArray,0,sizeof(Expression *) * numberOfSlots);
      cardinalityArray = genalloc(theEnv,sizeof(bool) * numberOfSlots);
      memset(cardinalityArray,0,sizeof(bool) * numberOfSlots);
      fieldCountArray = genalloc(theEnv,sizeof(unsigned short) * numberOfSlots);
      memset(fieldCountArray,0,sizeof(unsigned short) * numberOfSlots);
      infoArray = genalloc(theEnv,sizeof(struct extractedInfo *) * numberOfSlots);
      memset(infoArray,0,sizeof(struct extractedInfo *) * numberOfSlots);
     }
   else
     {
      lengthArray = NULL;
      cardinalityArray = NULL;
      fieldCountArray = NULL;
      infoArray = NULL;
     }

   /*==================================================*/
   /* Assign the boolean flags indicating cardinality. */
   /*==================================================*/

   if (theDeftemplate->implied)
     { cardinalityArray[0] = true; }
   else
     {
      for (theSlot = theDeftemplate->slotList, i = 0;
           theSlot != NULL;
           theSlot = theSlot->next, i++)
        {
         if (theSlot->multislot)
           { cardinalityArray[i] = true; }
        }
     }
     
   FindConstantLengthAndMultifieldNodes(theEnv,(struct factPatternNode *) join->rightSideEntryStructure,infoArray,lengthArray);
   
   DetermineSlotFieldCounts(theEnv,numberOfSlots,infoArray,cardinalityArray,lengthArray,fieldCountArray);

   InferValuesFromJoinNetworkComparisons(theEnv,join,lhsBinds,infoArray,fieldCountArray);

   InferValuesFromPatternNetworkComparisons(theEnv,(struct factPatternNode *) join->rightSideEntryStructure,infoArray,
                                            lengthArray,join,lhsBinds,fieldCountArray);

   InferWildcardPositions(theEnv,infoArray,lengthArray,numberOfSlots);
   
#if DEBUG_GOAL_GENERATION
   PrintExtractedInfo(theEnv,infoArray,numberOfSlots);
#endif
   
   /*==================================*/
   /* Copy values from the info array. */
   /*==================================*/

   PopulateGoalValues(theEnv,newGoal,numberOfSlots,infoArray,lengthArray,cardinalityArray);

   /*===========================================*/
   /* Any slot values that have no been set are */
   /* are assigned the unknown symbol (??).     */
   /*===========================================*/

   for (i = 0; i < numberOfSlots; i++)
     {
      if (newGoal->theProposition.contents[i].value == VoidConstant(theEnv))
        {
         if (! cardinalityArray[i])
           {
            newGoal->theProposition.contents[i].integerValue = CreateUQV(theEnv,1); 
           }
         else
           { newGoal->theProposition.contents[i].multifieldValue = CreateUnmanagedMultifield(theEnv,0); }
        }
      else if (cardinalityArray[i])
        {
         theMultifield = newGoal->theProposition.contents[i].multifieldValue;
         theLength = theMultifield->length;
         
         for (j = 0; j < theLength; j++)
           {
            if (theMultifield->contents[j].value == NULL)
              { theMultifield->contents[j].integerValue = CreateUQV(theEnv,1); }
           }
        }
     }

   /*====================================*/
   /* Deallocate the information arrays. */
   /*====================================*/
      
   if (infoArray != NULL)
     {
      for (i = 0; i < numberOfSlots; i++)
        {
         theInfo = infoArray[i];
         while (theInfo != NULL)
           {
            nextInfo = theInfo->next;
            rtn_struct(theEnv,extractedInfo,theInfo);
            theInfo = nextInfo;
           }
        }
        
      genfree(theEnv,lengthArray,sizeof(Expression *) * numberOfSlots);
      genfree(theEnv,cardinalityArray,sizeof(bool) * numberOfSlots);
      genfree(theEnv,fieldCountArray,sizeof(unsigned short) * numberOfSlots);
      genfree(theEnv,infoArray,sizeof(struct extractedInfo *) * numberOfSlots);
     }

   /*==============================================*/
   /* If the goal already exists, use the existing */
   /* one and update its support count.            */
   /*==============================================*/

   hashValue = HashFact(newGoal);
   existing = FactExists(theEnv,newGoal,hashValue);
   
   if (existing != NULL)
     {
      /*=================================*/
      /* Discard the newly created goal. */
      /*=================================*/
      
      ReturnFact(theEnv,newGoal);
      
      /*================================*/
      /* An existing goal isn't allowed */
      /* to support itself.             */
      /*================================*/
      
      if (lhsBinds != NULL)
        {
         for (i = 0; i < lhsBinds->bcount; i++)
           {
            if ((get_nth_pm_match(lhsBinds,i) != NULL) &&
                (get_nth_pm_match(lhsBinds,i)->matchingItem == &existing->patternHeader))
              { return NULL; }
           }
        }

      /*========================================*/
      /* Otherwise, increment the support count */
      /* for the existing goal and return it.   */
      /*========================================*/

#if DEBUGGING_FUNCTIONS && DEBUG_GOAL_COUNT
   if (theGoal->whichDeftemplate->watchGoals)
     {
      WriteString(theEnv,STDOUT,"+++ ");
      PrintFactWithIdentifier(theEnv,STDOUT,existing,NULL);
      WriteString(theEnv,STDOUT,"\n");
      TraceGoalToRule(theEnv,join,lhsBinds,"    ");
     }
#endif

      existing->supportCount++;
      return existing;
     }

   /*===========================================*/
   /* Otherwise, set the support count to 1 and */
   /* add the goal to the fact hash table.      */
   /*===========================================*/

   newGoal->supportCount = 1;
   AddHashedFact(theEnv,newGoal,hashValue);

   return newGoal;
  }

/*****************************************/
/* FindConstantLengthAndMultifieldNodes: */
/*****************************************/
static void FindConstantLengthAndMultifieldNodes(
  Environment *theEnv,
  struct factPatternNode *theNode,
  struct extractedInfo **infoArray,
  Expression **lengthArray)
  {
   Expression *theExpression;
   bool fromLeft, fromRight;
   
   if (infoArray == NULL)
     { return; }

   /*===========================================================*/
   /* Generate values from the front to the end of the pattern. */
   /*===========================================================*/
   
   if (theNode->lastLevel != NULL)
     { FindConstantLengthAndMultifieldNodes(theEnv,theNode->lastLevel,infoArray,lengthArray); }
     
   /*==================================================*/
   /* Skip selector nodes since the pertinent constant */
   /* for a slot is contained in a child node.         */
   /*==================================================*/

   if (theNode->header.selector)
     {
      if ((theNode->networkTest != NULL) &&
          (theNode->networkTest->nextArg != NULL))
        {
         if (theNode->networkTest->nextArg->type == FACT_SLOT_LENGTH)
           {
            struct factCheckLengthPNCall *hack;
            
            hack = (struct factCheckLengthPNCall *) theNode->networkTest->nextArg->bitMapValue->contents;
            lengthArray[hack->whichSlot] = theNode->networkTest->nextArg;
           }
        }
      return;
     }

   /*========================================================*/
   /* Save information about multifield wildcards/variables. */
   /*========================================================*/
   
   if (theNode->header.multifieldNode)
     { AddMultifieldInfo(theEnv,infoArray,theNode->whichSlot,theNode->whichField-1); }
     
   /*======================================*/
   /* If there's no associated expression, */
   /* then no information can be derived.  */
   /*======================================*/
   
   theExpression = theNode->networkTest;
   if (theExpression == NULL)
     { return; }
     
   if (theExpression->value == ExpressionData(theEnv)->PTR_AND)
     { theExpression = theExpression->argList; }

   /*==================================*/
   /*   */
   /*==================================*/
   
   while (theExpression != NULL)
     {
      switch(theExpression->type)
        {
         case INTEGER_TYPE:
         case FLOAT_TYPE:
         case SYMBOL_TYPE:
         case STRING_TYPE:
         case INSTANCE_NAME_TYPE:
           fromLeft = false;
           fromRight = false;
           
           if ((theNode->lastLevel->header.selector) &&
                (theNode->lastLevel->networkTest != NULL) &&
                (theNode->lastLevel->networkTest->type == FACT_PN_VAR3))
              {
               const struct factGetVarPN3Call *hack;
   
               hack = (const struct factGetVarPN3Call *) ((CLIPSBitMap *) theNode->lastLevel->networkTest->value)->contents;
               fromLeft = hack->fromBeginning;
               fromRight = hack->fromEnd;
              }
         
            AddConstantInfo(theEnv,infoArray,theNode->whichSlot,theNode->whichField-1,fromLeft,fromRight,theExpression);
            break;

         case FACT_SLOT_LENGTH:
            {
             struct factCheckLengthPNCall *hack;

             hack = (struct factCheckLengthPNCall *) ((CLIPSBitMap *) theExpression->value)->contents;
             
             lengthArray[hack->whichSlot] = theExpression;
            }
            break;
        }
        
      theExpression = theExpression->nextArg;
     }
  }

/*********************/
/* AddMultifieldInfo */
/*********************/
static void AddMultifieldInfo(
  Environment *theEnv,
  struct extractedInfo **infoArray,
  unsigned int whichSlot,
  unsigned short whichField)
  {
   struct extractedInfo *infoPtr;
   struct extractedInfo *lastPtr = NULL;
   struct extractedInfo *theInfo;
   
   for (infoPtr = infoArray[whichSlot];
        infoPtr != NULL;
        infoPtr = infoPtr->next)
     {
      if (infoPtr->field == whichField)
        {
         infoPtr->multifield = true;
         return;
        }
        
      if (infoPtr->field > whichField)
        { break; }
        
      lastPtr = infoPtr;
     }

   theInfo = CreateExtractionInfo(theEnv,whichField,true,false,false,VoidConstant(theEnv));
     
   if (lastPtr == NULL)
     {
      theInfo->next = infoArray[whichSlot];
      infoArray[whichSlot] = theInfo;
     }
   else
     {
      theInfo->next = infoPtr;
      lastPtr->next = theInfo;
     }
  }
  
/*************************/
/* CreateExtractionInfo: */
/*************************/
static struct extractedInfo *CreateExtractionInfo(
  Environment *theEnv,
  unsigned short theField,
  bool isMultifield,
  bool fromLeft,
  bool fromRight,
  void *theValue)
  {
   struct extractedInfo *theInfo;
   
   theInfo = get_struct(theEnv,extractedInfo);
   theInfo->field = theField;
   theInfo->multifield = isMultifield;
   theInfo->fromLeft = fromLeft;
   theInfo->fromRight = fromRight;
   theInfo->theValue.value = theValue;
   theInfo->next = NULL;
   
   return theInfo;
  }

/*******************/
/* AddConstantInfo */
/*******************/
static void AddConstantInfo(
  Environment *theEnv,
  struct extractedInfo **infoArray,
  unsigned int whichSlot,
  unsigned short whichField,
  bool fromLeft,
  bool fromRight,
  Expression *theConstant)
  {
   struct extractedInfo *infoPtr;
   struct extractedInfo *lastPtr = NULL;
   struct extractedInfo *theInfo;
   
   for (infoPtr = infoArray[whichSlot];
        infoPtr != NULL;
        infoPtr = infoPtr->next)
     {
      if (infoPtr->field == whichField)
        {
         infoPtr->theValue.value = theConstant->value;
         return;
        }
        
      if (infoPtr->field > whichField)
        { break; }
        
      lastPtr = infoPtr;
     }
     
   theInfo = CreateExtractionInfo(theEnv,whichField,false,fromLeft,fromRight,theConstant->value);

   if (lastPtr == NULL)
     {
      theInfo->next = infoArray[whichSlot];
      infoArray[whichSlot] = theInfo;
     }
   else
     {
      theInfo->next = infoPtr;
      lastPtr->next = theInfo;
     }
  }

/****************************/
/* DetermineSlotFieldCounts */
/****************************/
static void DetermineSlotFieldCounts(
  Environment *theEnv,
  unsigned short numberOfSlots,
  struct extractedInfo **infoArray,
  bool *cardinalityArray,
  Expression **lengthArray,
  unsigned short *fieldCountArray)
  {
   unsigned short i;
   
   for (i = 0; i < numberOfSlots; i++)
     {
      if (cardinalityArray[i])
        {
         if (lengthArray[i] == NULL)
           { fieldCountArray[i] = 0; }
         else
           {
            struct factCheckLengthPNCall *hack;

            hack = (struct factCheckLengthPNCall *) ((CLIPSBitMap *) lengthArray[i]->value)->contents;
            fieldCountArray[i] = ComputeNumberOfFields(theEnv,infoArray,hack->whichSlot,hack->exactly,hack->minLength);
           }
        }
      else
        { fieldCountArray[i] = 1; }
     }
  }

/***************************************************/
/* ComputeNumberOfFields: Determines the number of */
/*   field contained in a multifield slot pattern. */
/***************************************************/
static unsigned short ComputeNumberOfFields(
  Environment *theEnv,
  struct extractedInfo **infoArray,
  unsigned int whichSlot,
  bool exactly,
  unsigned short minLength)
  {
   struct extractedInfo *infoPtr;
   unsigned short explicitMultifields = 0;
   
   if (exactly)
     { return minLength; }
     
   for (infoPtr = infoArray[whichSlot];
        infoPtr != NULL;
        infoPtr = infoPtr->next)
     {
      if (infoPtr->multifield)
        { explicitMultifields++; }
     }
     
   if (explicitMultifields > 1)
     { return minLength + explicitMultifields; }
     
   return minLength + 1;
  }

/******************************************/
/* InferValuesFromJoinNetworkComparisons: */
/******************************************/
static void InferValuesFromJoinNetworkComparisons(
  Environment *theEnv,
  struct joinNode *join,
  struct partialMatch *lhsBinds,
  struct extractedInfo **infoArray,
  unsigned short *fieldCountArray)
  {
   Expression *theExpression;
   struct extractedInfo *theInfo;
   Fact *lhsFact;

   if (infoArray == NULL)
     { return; }
     
   theExpression = join->networkTest;
   if ((theExpression != NULL) &&
       (theExpression->value == ExpressionData(theEnv)->PTR_AND))
     { theExpression = theExpression->argList; }
     
   while (theExpression != NULL)
     {
      if (theExpression->type == FACT_JN_CMP1)
        {
         const struct factCompVarsJN1Call *hack;
         hack = (const struct factCompVarsJN1Call *) ((CLIPSBitMap *) theExpression->value)->contents;
         if (hack->pass && hack->p1rhs && hack->p2lhs)
           {
            lhsFact = (Fact *) lhsBinds->binds[hack->pattern2].gm.theMatch->matchingItem;

            theInfo = CreateExtractionInfo(theEnv,0,false,false,false,lhsFact->theProposition.contents[hack->slot2].value);

            AddToInfoArray(infoArray,theInfo,hack->slot1);
           }
        }
      else if (theExpression->type == FACT_JN_CMP2)
        {
         const struct factCompVarsJN2Call *hack;
         Multifield *segment;

         hack = (const struct factCompVarsJN2Call *) ((CLIPSBitMap *) theExpression->value)->contents;
         if (hack->pass && hack->p1rhs && hack->p2lhs)
           {
            lhsFact = (Fact *) lhsBinds->binds[hack->pattern2].gm.theMatch->matchingItem;
            
            theInfo = get_struct(theEnv,extractedInfo);
            theInfo->multifield = false;
            
            if (hack->fromBeginning1)
              {
               theInfo->field = hack->offset1;
               theInfo->fromLeft = true;
               theInfo->fromRight = false;
              }
            else
              {
               theInfo->field = fieldCountArray[hack->slot1] - hack->offset1;
               theInfo->fromLeft = false;
               theInfo->fromRight = true;
              }

            if (lhsFact->theProposition.contents[hack->slot2].header->type != MULTIFIELD_TYPE)
              { theInfo->theValue.value = lhsFact->theProposition.contents[hack->slot2].value; }
            else
              {
               segment = lhsFact->theProposition.contents[hack->slot2].multifieldValue;

               if (hack->fromBeginning1)
                 { theInfo->theValue.value = segment->contents[hack->offset2].value; }
               else
                 { theInfo->theValue.value = segment->contents[segment->length - (hack->offset2 + 1)].value; }
              }

            AddToInfoArray(infoArray,theInfo,hack->slot1);
           }
        }

      else if ((theExpression->type == FCALL) &&
               (theExpression->value == ExpressionData(theEnv)->PTR_EQ))
        {
         InferValueFromJoinNetworkEq(theEnv,join,lhsBinds,theExpression,infoArray,fieldCountArray);
        }

      theExpression = theExpression->nextArg;
     }

  }

/********************************/
/* InferValueFromJoinNetworkEq: */
/********************************/
static void InferValueFromJoinNetworkEq(
  Environment *theEnv,
  struct joinNode *join,
  struct partialMatch *lhsBinds,
  Expression *theExpression,
  struct extractedInfo **infoArray,
  unsigned short *fieldCountArray)
  {
   unsigned short theSlot, theField;
   struct partialMatch *oldLHSBinds = NULL;
   struct partialMatch *oldRHSBinds = NULL;
   struct joinNode *oldJoin = NULL;
   UDFValue theResult;
   struct extractedInfo *theInfo;
   Expression *firstArg, *secondArg;
   bool isMultifield = false, fromLeft = false, fromRight = false;
   
   /*===========================================*/
   /* Only process eq calls with two arguments. */
   /*===========================================*/

   if ((theExpression->argList == NULL) ||
       (theExpression->argList->nextArg == NULL) ||
       (theExpression->argList->nextArg->nextArg != NULL))
     { return; }
   
   firstArg = theExpression->argList;
   secondArg = theExpression->argList->nextArg;
   
   /*==================================================*/
   /* The second argument can't contain any references */
   /* to a partial match from the right because goals  */
   /* are generated when there is a left partial match */
   /* but no right partial match.                      */
   /*==================================================*/
   
   if (UnboundReferences(secondArg,infoArray))
     { return; }
     
   /*=============================================*/
   /* The first argument should be a reference to */
   /* a variable from the right side of the join. */
   /*=============================================*/
   
   switch (firstArg->type)
     {
      case FACT_JN_VAR1:
        {
         const struct factGetVarJN1Call *hack;
         hack = (const struct factGetVarJN1Call *) ((CLIPSBitMap *) firstArg->value)->contents;
         
         if (! hack->rhs)
           { return; }
           
         theSlot = hack->whichSlot;
         theField = hack->whichField;
        }
        break;

      case FACT_JN_VAR2:
        {
         const struct factGetVarJN2Call *hack;
         hack = (const struct factGetVarJN2Call *) ((CLIPSBitMap *) firstArg->value)->contents;
         
         if (! hack->rhs)
           { return; }
           
         theSlot = hack->whichSlot;
         theField = 0;
        }
        break;

      case FACT_JN_VAR3:
        {
         const struct factGetVarJN3Call *hack;
         hack = (const struct factGetVarJN3Call *) ((CLIPSBitMap *) firstArg->value)->contents;
         
         if (! hack->rhs)
           { return; }
           
         theSlot = hack->whichSlot;

         if (hack->fromBeginning && hack->fromEnd)
           { isMultifield = true; }
        
         if (hack->fromBeginning)
           { theField = hack->beginOffset; }
         else
           { theField = fieldCountArray[theSlot] - hack->endOffset; }
           
         fromLeft = hack->fromBeginning;
         fromRight = hack->fromEnd;
        }
        break;
        
      default:
        return;
     }
     
   oldLHSBinds = EngineData(theEnv)->GlobalLHSBinds;
   oldRHSBinds = EngineData(theEnv)->GlobalRHSBinds;
   oldJoin = EngineData(theEnv)->GlobalJoin;

   EngineData(theEnv)->GlobalLHSBinds = lhsBinds;
   EngineData(theEnv)->GlobalRHSBinds = NULL;
   EngineData(theEnv)->GlobalJoin = join;

   FactData(theEnv)->goalInfoArray = infoArray;
   EvaluateExpression(theEnv,secondArg,&theResult);
   FactData(theEnv)->goalInfoArray = NULL;

   theInfo = FindExtractedInfo(infoArray,theSlot,theField);
   if (theInfo == NULL)
     {
      theInfo = get_struct(theEnv,extractedInfo);
      theInfo->multifield = isMultifield;
      theInfo->field = theField;
      theInfo->fromLeft = fromLeft;
      theInfo->fromRight = fromRight;
      AddToInfoArray(infoArray,theInfo,theSlot);
     }
     
   UDFToCLIPSValue(theEnv,&theResult,&theInfo->theValue);
   
   EngineData(theEnv)->GlobalLHSBinds = oldLHSBinds;
   EngineData(theEnv)->GlobalRHSBinds = oldRHSBinds;
   EngineData(theEnv)->GlobalJoin = oldJoin;
  }

/*********************************************/
/* InferValuesFromPatternNetworkComparisons: */
/*********************************************/
static void InferValuesFromPatternNetworkComparisons(
  Environment *theEnv,
  struct factPatternNode *theNode,
  struct extractedInfo **infoArray,
  Expression **lengthArray,
  struct joinNode *join,
  struct partialMatch *lhsBinds,
  unsigned short *fieldCountArray)
  {
   Expression *theExpression;
   struct extractedInfo *theInfo;
   
   if (infoArray == NULL)
     { return; }

   /*===========================================================*/
   /* Generate values from the front to the end of the pattern. */
   /*===========================================================*/
   
   if (theNode->lastLevel != NULL)
     { InferValuesFromPatternNetworkComparisons(theEnv,theNode->lastLevel,infoArray,lengthArray,join,lhsBinds,fieldCountArray); }
     
   /*======================================*/
   /* If there's no associated expression, */
   /* then no information can be derived.  */
   /*======================================*/
   
   theExpression = theNode->networkTest;
   if (theExpression == NULL)
     { return; }
     
   if (theExpression->value == ExpressionData(theEnv)->PTR_AND)
     { theExpression = theExpression->argList; }

   /*==================================*/
   /*   */
   /*==================================*/
   
   while (theExpression != NULL)
     {
      switch(theExpression->type)
        {
         case FACT_PN_CMP1:
            {
             const struct factCompVarsPN1Call *hack;
             hack = (const struct factCompVarsPN1Call *) ((CLIPSBitMap *) theExpression->value)->contents;
             if (hack->pass)
               {
                if (infoArray[hack->field2] != NULL)
                  {
                   theInfo = CreateExtractionInfo(theEnv,0,false,false,false,infoArray[hack->field2]->theValue.value);
                   AddToInfoArray(infoArray,theInfo,hack->field1);
                  }
               }
            }
            break;

         case FCALL:
            if (theExpression->value == ExpressionData(theEnv)->PTR_EQ)
              { InferValueFromPatternNetworkEq(theEnv,theExpression,infoArray,lengthArray,theNode->whichField-1,join,lhsBinds,fieldCountArray); }
            break;

         default:
           break;
        }
        
      theExpression = theExpression->nextArg;
     }
  }
  
/***********************************/
/* InferValueFromPatternNetworkEq: */
/***********************************/
static void InferValueFromPatternNetworkEq(
  Environment *theEnv,
  Expression *eqPtr,
  struct extractedInfo **infoArray,
  Expression **lengthArray,
  unsigned short theField,
  struct joinNode *join,
  struct partialMatch *lhsBinds,
  unsigned short *fieldCountArray)
  {
   Expression *firstArg, *secondArg;
   struct extractedInfo *theInfo, *newInfo;
   unsigned short sourceSlot, sourceField;
   bool isMultifield;
   struct partialMatch *oldLHSBinds = NULL;
   struct partialMatch *oldRHSBinds = NULL;
   struct joinNode *oldJoin = NULL;
   bool found = false;
   UDFValue theResult;
   CLIPSValue retrievedValue;
   firstArg = eqPtr->argList;
   secondArg = eqPtr->argList->nextArg;

   /*==================================================*/
   /* The second argument is the one which may already */
   /* be set. Its field number will be the lower one.  */
   /*==================================================*/
   
   switch (secondArg->type) // TBD How to handle fact address (hack->factAddress);
     {
      case FACT_PN_VAR1:
        {
         const struct factGetVarPN1Call *hack;
         
         hack = (const struct factGetVarPN1Call *) ((CLIPSBitMap *) secondArg->value)->contents;
         sourceSlot = hack->whichSlot;
         sourceField = hack->whichField;
         for (theInfo = infoArray[sourceSlot];
              theInfo != NULL;
              theInfo = theInfo->next)
           {
            if (theInfo->field == sourceField)
              {
               found = true;
               retrievedValue.value = theInfo->theValue.value;
               break;
              }
           }
        }
        break;

      case FACT_PN_VAR3:
        {
         const struct factGetVarPN3Call *hack;
   
         hack = (const struct factGetVarPN3Call *) ((CLIPSBitMap *) secondArg->value)->contents;
         sourceSlot = hack->whichSlot;
         if (hack->fromBeginning)
           { sourceField = hack->beginOffset; }
         else
           { sourceField = fieldCountArray[sourceSlot] - hack->endOffset; }
         
         sourceField = hack->beginOffset;
         for (theInfo = infoArray[sourceSlot];
              theInfo != NULL;
              theInfo = theInfo->next)
           {
            if (theInfo->field == sourceField)
              {
               found = true;
               retrievedValue.value = theInfo->theValue.value;
               break;
              }
           }
        }
        break;

      case FCALL:
      case GCALL:
      case PCALL:
        {
         found = true;
         oldLHSBinds = EngineData(theEnv)->GlobalLHSBinds;
         oldRHSBinds = EngineData(theEnv)->GlobalRHSBinds;
         oldJoin = EngineData(theEnv)->GlobalJoin;

         EngineData(theEnv)->GlobalLHSBinds = lhsBinds;
         EngineData(theEnv)->GlobalRHSBinds = NULL;
         EngineData(theEnv)->GlobalJoin = join;

         FactData(theEnv)->goalInfoArray = infoArray;
         EvaluateExpression(theEnv,secondArg,&theResult);
         UDFToCLIPSValue(theEnv,&theResult,&retrievedValue);
         FactData(theEnv)->goalInfoArray = NULL;

         EngineData(theEnv)->GlobalLHSBinds = oldLHSBinds;
         EngineData(theEnv)->GlobalRHSBinds = oldRHSBinds;
         EngineData(theEnv)->GlobalJoin = oldJoin;
        }
        break;

      default:
        return;
     }

   if (! found)
     { return; }
     
   /*=========================================================*/
   /* The first argument is the one which may be assigned an  */
   /* earlier value. Its field number will be the higher one. */
   /*=========================================================*/
   
   switch (firstArg->type)
     {
      case FACT_PN_VAR1:
        {
         const struct factGetVarPN1Call *hack;
         
         hack = (const struct factGetVarPN1Call *) ((CLIPSBitMap *) firstArg->value)->contents;

         newInfo = get_struct(theEnv,extractedInfo);
         newInfo->field = theField;
         newInfo->fromLeft = false;
         newInfo->fromRight = true;
         newInfo->theValue.value = retrievedValue.value;
         if (retrievedValue.header->type == MULTIFIELD_TYPE)
           { newInfo->multifield = true; }
         else
           { newInfo->multifield = false; }
         AddToInfoArray(infoArray,newInfo,hack->whichSlot);
        }
        break;

      case FACT_PN_VAR3:
        {
         const struct factGetVarPN3Call *hack;
   
         hack = (const struct factGetVarPN3Call *) ((CLIPSBitMap *) firstArg->value)->contents;
         if (hack->fromBeginning && hack->fromEnd)
           { isMultifield = true; }
         else
           { isMultifield = false; }
           
         newInfo = CreateExtractionInfo(theEnv,theField,isMultifield,hack->fromBeginning,hack->fromEnd,retrievedValue.value);
         AddToInfoArray(infoArray,newInfo,hack->whichSlot);
        }
        break;
     }
  }

/******************/
/* AddToInfoArray */
/******************/
static void AddToInfoArray(
  struct extractedInfo **infoArray,
  struct extractedInfo *theInfo,
  unsigned int whichSlot)
  {
   struct extractedInfo *infoPtr;
   struct extractedInfo *lastPtr = NULL;
   
   for (infoPtr = infoArray[whichSlot];
        infoPtr != NULL;
        infoPtr = infoPtr->next)
     {
      if (infoPtr->field >= theInfo->field)
        { break; }
        
      lastPtr = infoPtr;
     }
     
   if (lastPtr == NULL)
     {
      theInfo->next = infoArray[whichSlot];
      infoArray[whichSlot] = theInfo;
     }
   else
     {
      theInfo->next = infoPtr;
      lastPtr->next = theInfo;
     }
  }

/*********************/
/* FindExtractedInfo */
/*********************/
static struct extractedInfo *FindExtractedInfo(
  struct extractedInfo **infoArray,
  unsigned short whichSlot,
  unsigned short whichField)
  {
   struct extractedInfo *infoPtr;
   
   for (infoPtr = infoArray[whichSlot];
        infoPtr != NULL;
        infoPtr = infoPtr->next)
     {
      if (infoPtr->field == whichField)
        { return infoPtr; }
        
      if (infoPtr->field > whichField)
        { break; }
     }
     
   return NULL;
  }

/**********************/
/* UnboundReferences: */
/**********************/
static bool UnboundReferences(
   Expression *theExpression,
   struct extractedInfo **infoArray)
   {
    struct extractedInfo *theInfo;
    
    while (theExpression != NULL)
      {
       switch(theExpression->type)
         {
          case FACT_JN_VAR1:
            {
             const struct factGetVarJN1Call *hack;
             
             hack = (const struct factGetVarJN1Call *) ((CLIPSBitMap *) theExpression->value)->contents;
             if (hack->rhs)
               {
                for (theInfo = infoArray[hack->whichSlot]; theInfo != NULL; theInfo = theInfo->next)
                  {
                  
                  }

                return true;
               }
            }
            break;
            
          case FACT_JN_VAR2:
            {
             const struct factGetVarJN2Call *hack;
             
             hack = (const struct factGetVarJN2Call *) ((CLIPSBitMap *) theExpression->value)->contents;
             if (hack->rhs)
               {
                for (theInfo = infoArray[hack->whichSlot]; theInfo != NULL; theInfo = theInfo->next)
                  {
                  
                  }

                return true;
               }
            }
            break;
            
          case FACT_JN_VAR3:
            {
             const struct factGetVarJN3Call *hack;
             
             hack = (const struct factGetVarJN3Call *) ((CLIPSBitMap *) theExpression->value)->contents;
             if (hack->rhs)
               {
                bool bound = false;
                
                for (theInfo = infoArray[hack->whichSlot]; theInfo != NULL; theInfo = theInfo->next)
                  {
                   if (theInfo->field == hack->beginOffset)
                     {
                      bound = true;
                      break;
                     }
                  }

                if (! bound)
                  { return true; }
               }
            }
            break;
         }
         
       if (theExpression->argList != NULL)
         {
          if (UnboundReferences(theExpression->argList,infoArray))
            { return true; }
         }
         
       theExpression = theExpression->nextArg;
      }
      
    return false;
   }

/**************************/
/* InferWildcardPositions */
/**************************/
static void InferWildcardPositions(
  Environment *theEnv,
  struct extractedInfo **infoArray,
  Expression **lengthArray,
  unsigned short numberOfSlots)
  {
   unsigned short i;
   struct factCheckLengthPNCall *hack;

   if (lengthArray == NULL)
     { return; }
     
   for (i = 0; i < numberOfSlots; i++)
     {
      if (lengthArray[i] == NULL) continue;
      
      hack = (struct factCheckLengthPNCall *) ((CLIPSBitMap *) lengthArray[i]->value)->contents;
      InferWildcardPositionsForSlot(theEnv,infoArray,hack->whichSlot,hack->exactly,hack->minLength);
     }
  }

/*********************************/
/* InferWildcardPositionsForSlot */
/*********************************/
static void InferWildcardPositionsForSlot(
  Environment *theEnv,
  struct extractedInfo **infoArray,
  unsigned int whichSlot,
  bool exactly,
  unsigned short minLength)
  {
   struct extractedInfo *infoPtr, *theInfo, *lastPtr;
   unsigned short i;
   unsigned short explicitMultifields = 0;
   unsigned short explicitSinglefields = 0;
   
   /*======================================================*/
   /* If the number of fields wasn't exact, then determine */
   /* the number of multifields in the slot pattern.       */
   /*======================================================*/
   
   if (! exactly)
     {
      for (infoPtr = infoArray[whichSlot];
           infoPtr != NULL;
           infoPtr = infoPtr->next)
        {
         if (infoPtr->multifield)
           { explicitMultifields++; }
         else
           { explicitSinglefields++; }
        }
        
      /*=============================================*/
      /* If the number of fields isn't exact, and no */
      /* explicit multifield was found, infer where  */
      /* the implicit multifield is located.         */
      /*=============================================*/
      
      if (explicitMultifields == 0)
        {
         InferWildcardPositionsForSlotWithImplicitMultifield(theEnv,infoArray,whichSlot,minLength);
         return;
        }

      /*======================================================*/
      /* If all of the single field values are accounted for, */
      /* we don't need to determine where the missing ones    */
      /* should be placed.                                    */
      /*======================================================*/
   
      if (explicitSinglefields == minLength)
        { return; }
     }

   /*=========================================================*/
   /* All multifields have been accounted for, so all of the  */
   /* missing fields can be populated as single field values. */
   /*=========================================================*/
   
   lastPtr = NULL;
   infoPtr = infoArray[whichSlot];
      
   for (i = 0; i < (minLength + explicitMultifields); i++)
     {
      if ((infoPtr != NULL) &&
          (infoPtr->field == i))
        {
         lastPtr = infoPtr;
         infoPtr = infoPtr->next;
         continue;
        }
      
      theInfo = CreateExtractionInfo(theEnv,i,false,false,false,VoidConstant(theEnv));

      theInfo->next = infoPtr;
         
      if (lastPtr == NULL)
        { infoArray[whichSlot] = theInfo; }
      else
        { lastPtr->next = theInfo; }
           
      lastPtr = theInfo;
     }
  }

/*******************************************************/
/* InferWildcardPositionsForSlotWithImplicitMultifield */
/*******************************************************/
static void InferWildcardPositionsForSlotWithImplicitMultifield(
  Environment *theEnv,
  struct extractedInfo **infoArray,
  unsigned int whichSlot,
  unsigned short minLength)
  {
   struct extractedInfo *infoPtr, *theInfo, *lastPtr;
   unsigned short i;
   //unsigned short explicitSinglefields = 0;
   bool fromLeft = false, fromRight = false;
   unsigned short fromLeftField = 0, fromRightField = 0;
   bool multifieldAssigned = false;
   
   /*===========================================================*/
   /* Look for fields with the fromLeft or fromRight flags set. */
   /* When set these indicate that either the preceding or      */
   /* following fields are fields matching a single value. This */
   /* loop will determine the right most fromLeft field and the */
   /* left most fromRight fields.                               */
   /*===========================================================*/
   
   for (infoPtr = infoArray[whichSlot];
        infoPtr != NULL;
        infoPtr = infoPtr->next)
     {
      if (infoPtr->fromLeft)
        {
         fromLeft = true;
         fromLeftField = infoPtr->field;
        }
        
      if (infoPtr->fromRight)
        {
         if (! fromRight)
           {
            fromRight = true;
            fromRightField = infoPtr->field;
           }
        }
     }
          
   /*=====================================================*/
   /* Assign single field values to any field to the left */
   /* of the right most fromLeft field or to the right of */
   /* the left most fromRight field.                      */
   /*=====================================================*/
   
   lastPtr = NULL;
   infoPtr = infoArray[whichSlot];

   for (i = 0; i < (minLength + 1); i++)
     {
      if ((infoPtr != NULL) &&
          (infoPtr->field == i))
        {
         lastPtr = infoPtr;
         infoPtr = infoPtr->next;
         continue;
        }
           
      if ((fromLeft && (i < fromLeftField)) ||
          (fromRight && (i > fromRightField)))
        {
         //explicitSinglefields++;
                
         theInfo = CreateExtractionInfo(theEnv,i,false,false,false,VoidConstant(theEnv));

         theInfo->next = infoPtr;
         
         if (lastPtr == NULL)
           { infoArray[whichSlot] = theInfo; }
         else
           { lastPtr->next = theInfo; }
           
         lastPtr = theInfo;
        }
     }

   /*====================================================*/
   /* Assign a multifield value to the first unset field */
   /* and then assign a single field value to the reset. */
   /*====================================================*/

   lastPtr = NULL;
   infoPtr = infoArray[whichSlot];

   for (i = 0; i < (minLength + 1); i++)
     {
      if ((infoPtr != NULL) &&
          (infoPtr->field == i))
        {
         lastPtr = infoPtr;
         infoPtr = infoPtr->next;
         continue;
        }
        
      theInfo = CreateExtractionInfo(theEnv,i,(! multifieldAssigned),false,false,VoidConstant(theEnv));
      
      multifieldAssigned = true;
      theInfo->next = infoPtr;
         
      if (lastPtr == NULL)
        { infoArray[whichSlot] = theInfo; }
      else
        { lastPtr->next = theInfo; }
           
      lastPtr = theInfo;
     }

   /*====================================================*/
   /* If all of the single field values have been found, */
   /* then the remaining field must be the multifield.   */
   /*====================================================*/
/*
   if (explicitSinglefields == minLength)
     {
      for (infoPtr = infoArray[whichSlot];
           infoPtr != NULL;
           infoPtr = infoPtr->next)
        {
         if ((infoPtr == infoArray[whichSlot]) &&
             (infoPtr->field == 1))
           {
            theInfo = CreateExtractionInfo(theEnv,0,true,false,false,VoidConstant(theEnv));
            theInfo->next = infoArray[whichSlot];
            infoArray[whichSlot] = theInfo;
            break;
           }
         else if ((infoPtr->next != NULL) &&
                  (infoPtr->next->field != (infoPtr->field + 1)))
           {
            theInfo = CreateExtractionInfo(theEnv,infoPtr->field + 1,true,false,false,VoidConstant(theEnv));
            theInfo->next = infoPtr->next;
            infoPtr->next = theInfo;
            break;
           }
        }
     }
     */
  }

/***********************/
/* PopulateGoalValues: */
/***********************/
void PopulateGoalValues(
  Environment *theEnv,
  struct fact *newGoal,
  unsigned short numberOfSlots,
  struct extractedInfo **infoArray,
  Expression **lengthArray,
  bool *cardinalityArray)
  {
   unsigned short i, j;
   size_t theLength;
   struct extractedInfo *theInfo;
   Multifield *theMultifield;
   size_t thePosition;

   if (infoArray == NULL)
     { return; }

   for (i = 0; i < numberOfSlots; i++)
     {
      if (cardinalityArray[i])
        {
         /*=========================================================*/
         /* Determine the minimum size of the multifield needed to  */
         /* store all of the slot's values. This will be the number */
         /* of single field constraints in the slot's pattern.      */
         /*=========================================================*/
            
         if (lengthArray[i] != NULL)
           {
            struct factCheckLengthPNCall *hack;

            hack = (struct factCheckLengthPNCall *) ((CLIPSBitMap *) lengthArray[i]->value)->contents;
                  
            theLength = hack->minLength;
           }
         else
           { theLength = 0; }
            
         /*===============================================*/
         /* The total size needed for the multifield slot */
         /* slot value will include the total length of   */
         /* all multifields with bound values.            */
         /*===============================================*/
            
         for (theInfo = infoArray[i]; theInfo != NULL; theInfo = theInfo->next)
           {
            if (theInfo->theValue.header->type == MULTIFIELD_TYPE)
              { theLength += theInfo->theValue.multifieldValue->length; }
           }
                    
         /*============================================*/
         /* Create a multifield of sufficient size and */
         /* initialize the field values to NULL.       */
         /*============================================*/
            
         theMultifield = CreateUnmanagedMultifield(theEnv,theLength);

         for (j = 0; j < theLength; j++)
           { theMultifield->contents[j].value = NULL; }
                    
         /*========================================*/
         /* Initialize some variables for tracking */
         /* assignments to multifield variables.   */
         /*========================================*/
            
         thePosition = 0;
             
         /*======================================================*/
         /* Iterate over each field that's been specified in the */
         /* pattern slot for which a value has been identified   */
         /* from a partial match or the pattern network.         */
         /*======================================================*/
            
         for (theInfo = infoArray[i]; theInfo != NULL; theInfo = theInfo->next)
           {
            if (theInfo->multifield)
              {
               if (theInfo->theValue.header->type == MULTIFIELD_TYPE)
                 {
                  theLength = theInfo->theValue.multifieldValue->length;
                  for (j = 0; j < theLength; j++)
                    { theMultifield->contents[thePosition++].value = theInfo->theValue.multifieldValue->contents[j].value; }
                 }
              }
            else
              {
               if (theInfo->theValue.value != VoidConstant(theEnv))
                 { theMultifield->contents[thePosition].value = theInfo->theValue.value; }
                 
               thePosition++;
              }
           }

         newGoal->theProposition.contents[i].multifieldValue = theMultifield;
        }
      else
        {
         if (infoArray[i] != NULL)
           { newGoal->theProposition.contents[i].value = infoArray[i]->theValue.value; }
        }
     }
  }
  
/*********************************************/
/* TraceGoalToRule: Prints the list of rules */
/*   that triggered the creation of a goal.  */
/*********************************************/
void TraceGoalToRule(
  Environment *theEnv,
  struct joinNode *joinPtr,
  struct partialMatch *theMatch,
  const char *indentSpaces)
  {
   MarkRuleNetwork(theEnv,0);

   TraceGoalToRuleDriver(theEnv,joinPtr,theMatch,indentSpaces,false);

   MarkRuleNetwork(theEnv,0);
  }
  
/***************************************************/
/* TraceGoalToRuleDriver: Driver code for printing */
/*   out which rule caused a goal to be generated. */
/***************************************************/
static void TraceGoalToRuleDriver(
  Environment *theEnv,
  struct joinNode *joinPtr,
  struct partialMatch *theMatch,
  const char *indentSpaces,
  bool enteredJoinFromRight)
  {
   const char *name;
   struct joinLink *theLinks;

   if (joinPtr->marked)
     { /* Do Nothing */ }
   else if (joinPtr->ruleToActivate != NULL)
     {
      joinPtr->marked = 1;
      name = DefruleName(joinPtr->ruleToActivate);
      WriteString(theEnv,STDERR,indentSpaces);
      WriteString(theEnv,STDOUT,"from ");
      WriteString(theEnv,STDERR,name);
      WriteString(theEnv,STDERR,": ");
      PrintPartialMatch(theEnv,STDOUT,theMatch);
      WriteString(theEnv,STDERR,"\n");
     }
   else
     {
      joinPtr->marked = 1;

      theLinks = joinPtr->nextLinks;
      while (theLinks != NULL)
        {
         TraceGoalToRuleDriver(theEnv,theLinks->join,theMatch,indentSpaces,
                                (theLinks->enterDirection == RHS));
         theLinks = theLinks->next;
        }
     }
  }
  
/***********************/
/* PrintExtractedInfo: */
/***********************/
#if DEBUG_GOAL_GENERATION
static void PrintExtractedInfo(
  Environment *theEnv,
  struct extractedInfo **infoArray,
  unsigned short numberOfSlots)
  {
   struct extractedInfo *infoPtr;
   unsigned short i;
   
   Writeln(theEnv,"Extracted Information:");
   for (i = 0; i < numberOfSlots; i++)
     {
      for (infoPtr = infoArray[i];
           infoPtr != NULL;
           infoPtr = infoPtr->next)
        {
         Write(theEnv,"Slot: ");
         WriteInteger(theEnv,STDOUT,i);
         
         Write(theEnv," Field: ");
         WriteInteger(theEnv,STDOUT,infoPtr->field);
         
         Write(theEnv," Cardinality: ");
         if (infoPtr->multifield)
           { Write(theEnv,"M"); }
         else
           { Write(theEnv,"S"); }

         Write(theEnv," FL: ");
         if (infoPtr->fromLeft)
           { Write(theEnv,"T"); }
         else
           { Write(theEnv,"F"); }

         Write(theEnv," FR: ");
         if (infoPtr->fromRight)
           { Write(theEnv,"T"); }
         else
           { Write(theEnv,"F"); }

         Write(theEnv," Value: ");
         WriteCLIPSValue(theEnv,STDOUT,&infoPtr->theValue);
         
         Writeln(theEnv,"");
        }
     }
  }
#endif

#endif /* DEFTEMPLATE_CONSTRUCT && DEFRULE_CONSTRUCT */
