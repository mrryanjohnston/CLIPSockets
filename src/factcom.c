   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 7.00  01/22/24             */
   /*                                                     */
   /*                FACT COMMANDS MODULE                 */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides the facts, assert, retract, save-facts, */
/*   load-facts, set-fact-duplication, get-fact-duplication, */
/*   assert-string, and fact-index commands and functions.   */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*      6.24: Added environment parameter to GenClose.       */
/*            Added environment parameter to GenOpen.        */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Support for long long integers.                */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Added code to prevent a clear command from     */
/*            being executed during fact assertions via      */
/*            Increment/DecrementClearReadyLocks API.        */
/*                                                           */
/*            Changed find construct functionality so that   */
/*            imported modules are search when locating a    */
/*            named construct.                               */
/*                                                           */
/*      6.31: Error messages are now generated when the      */
/*            fact-index function is given a retracted       */
/*            fact.                                          */
/*                                                           */
/*            Added code to keep track of pointers to        */
/*            constructs that are contained externally to    */
/*            to constructs, DanglingConstructs.             */
/*                                                           */
/*            If embedded, LoadFacts cleans the current      */
/*            garbage frame.                                 */
/*                                                           */
/*      6.32: Fixed embedded reset of error flags.           */
/*                                                           */
/*      6.40: Added Env prefix to GetEvaluationError and     */
/*            SetEvaluationError functions.                  */
/*                                                           */
/*            Added Env prefix to GetHaltExecution and       */
/*            SetHaltExecution functions.                    */
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
/*            Eval support for run time and bload only.      */
/*                                                           */
/*            Watch facts for modify command only prints     */
/*            changed slots.                                 */
/*                                                           */
/*            Moved load-facts and save-facts functions to   */
/*            factfile.c and factfile.h.                     */
/*                                                           */
/*      6.41: Used gensnprintf in place of gensprintf and.   */
/*            sprintf.                                       */
/*                                                           */
/*      7.00: Support for data driven backward chaining.     */
/*                                                           */
/*            Support for non-reactive fact patterns.        */
/*                                                           */
/*            Construct hashing for quick lookup.            */
/*                                                           */
/*************************************************************/

#include <stdio.h>
#include <string.h>

#include "setup.h"

#if DEFTEMPLATE_CONSTRUCT

#include "argacces.h"
#include "extnfunc.h"
#include "factmngr.h"
#include "factrhs.h"
#include "multifld.h"
#include "pprint.h"
#include "prntutil.h"
#include "router.h"
#include "scanner.h"
#include "sysdep.h"
#include "tmpltdef.h"
#include "tmpltutl.h"

#include "factcom.h"

#define INVALID     -2L
#define UNSPECIFIED -1L

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static struct expr            *AssertParse(Environment *,struct expr *,const char *);
   static void                    FactsGoalsCommand(Environment *,UDFContext *,UDFValue *,bool);
#if DEBUGGING_FUNCTIONS
   static long long               GetFactsArgument(UDFContext *);
#endif

/***************************************/
/* FactCommandDefinitions: Initializes */
/*   fact commands and functions.      */
/***************************************/
void FactCommandDefinitions(
  Environment *theEnv)
  {
#if ! RUN_TIME
#if DEBUGGING_FUNCTIONS
   AddUDF(theEnv,"facts","v",0,4,"l;*",FactsCommand,"FactsCommand",NULL);
   AddUDF(theEnv,"goals","v",0,4,"l;*",GoalsCommand,"GoalsCommand",NULL);
#endif

   AddUDF(theEnv,"assert","bf",0,UNBOUNDED,NULL,AssertCommand,"AssertCommand",NULL);
   AddUDF(theEnv,"retract", "v",1,UNBOUNDED,"fly",RetractCommand,"RetractCommand",NULL);
   AddUDF(theEnv,"assert-string","bf",1,1,"s",AssertStringFunction,"AssertStringFunction",NULL);
   AddUDF(theEnv,"str-assert","bf",1,1,"s",AssertStringFunction,"AssertStringFunction",NULL);

   AddUDF(theEnv,"get-fact-duplication","b",0,0,NULL,GetFactDuplicationCommand,"GetFactDuplicationCommand", NULL);
   AddUDF(theEnv,"set-fact-duplication","b",1,1,NULL,SetFactDuplicationCommand,"SetFactDuplicationCommand", NULL);

   AddUDF(theEnv,"fact-index","l",1,1,"f",FactIndexFunction,"FactIndexFunction",NULL);

   FuncSeqOvlFlags(theEnv,"assert",false,false);
#else
#if MAC_XCD
#pragma unused(theEnv)
#endif
#endif
   AddFunctionParser(theEnv,"assert",AssertParse);
  }

/***************************************/
/* AssertCommand: H/L access routine   */
/*   for the assert function.          */
/***************************************/
void AssertCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   Deftemplate *theDeftemplate;
   CLIPSValue *theField;
   UDFValue theValue;
   struct expr *theExpression;
   struct templateSlot *slotPtr;
   Fact *newFact;
   bool error = false;
   int i;
   Fact *theFact;

   /*================================*/
   /* Get the deftemplate associated */
   /* with the fact being asserted.  */
   /*================================*/

   theExpression = GetFirstArgument();
   theDeftemplate = (Deftemplate *) theExpression->value;

   /*=======================================*/
   /* Create the fact and store the name of */
   /* the deftemplate as the 1st field.     */
   /*=======================================*/

   if (theDeftemplate->implied == false)
     {
      newFact = CreateFactBySize(theEnv,theDeftemplate->numberOfSlots);
      slotPtr = theDeftemplate->slotList;
     }
   else
     {
      newFact = CreateFactBySize(theEnv,1);
      if (theExpression->nextArg == NULL)
        { newFact->theProposition.contents[0].multifieldValue = CreateUnmanagedMultifield(theEnv,0L); }
      slotPtr = NULL;
     }

   newFact->whichDeftemplate = theDeftemplate;

   /*===================================================*/
   /* Evaluate the expression associated with each slot */
   /* and store the result in the appropriate slot of   */
   /* the newly created fact.                           */
   /*===================================================*/

   IncrementClearReadyLocks(theEnv);

   theField = newFact->theProposition.contents;

   for (theExpression = theExpression->nextArg, i = 0;
        theExpression != NULL;
        theExpression = theExpression->nextArg, i++)
     {
      /*===================================================*/
      /* Evaluate the expression to be stored in the slot. */
      /*===================================================*/

      EvaluateExpression(theEnv,theExpression,&theValue);

      /*============================================================*/
      /* A multifield value can't be stored in a single field slot. */
      /*============================================================*/

      if ((slotPtr != NULL) ?
          (slotPtr->multislot == false) && (theValue.header->type == MULTIFIELD_TYPE) :
          false)
        {
         MultiIntoSingleFieldSlotError(theEnv,slotPtr,theDeftemplate);
         theValue.value = FalseSymbol(theEnv);
         error = true;
        }

      /*==============================*/
      /* Store the value in the slot. */
      /*==============================*/

      theField[i].value = theValue.value;

      /*========================================*/
      /* Get the information for the next slot. */
      /*========================================*/

      if (slotPtr != NULL) slotPtr = slotPtr->next;
     }

   DecrementClearReadyLocks(theEnv);

   /*============================================*/
   /* If an error occured while generating the   */
   /* fact's slot values, then abort the assert. */
   /*============================================*/

   if (error)
     {
      ReturnFact(theEnv,newFact);
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   /*================================*/
   /* Add the fact to the fact-list. */
   /*================================*/

   theFact = Assert(newFact);

   /*========================================*/
   /* The asserted fact is the return value. */
   /*========================================*/

   if (theFact != NULL)
     { returnValue->factValue = theFact; }
   else
     { returnValue->lexemeValue = FalseSymbol(theEnv); }

   return;
  }

/****************************************/
/* RetractCommand: H/L access routine   */
/*   for the retract command.           */
/****************************************/
void RetractCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   long long factIndex;
   Fact *ptr;
   UDFValue theArg;

   /*================================*/
   /* Iterate through each argument. */
   /*================================*/

   while (UDFHasNextArgument(context))
     {
      /*========================*/
      /* Evaluate the argument. */
      /*========================*/

      if (! UDFNextArgument(context,INTEGER_BIT | FACT_ADDRESS_BIT | SYMBOL_BIT,&theArg))
        { return; }

      /*======================================*/
      /* If the argument evaluates to a fact  */
      /* address, we can directly retract it. */
      /*======================================*/

      if (CVIsType(&theArg,FACT_ADDRESS_BIT))
        { Retract(theArg.factValue); }

      /*===============================================*/
      /* If the argument evaluates to an integer, then */
      /* it's assumed to be the fact index of the fact */
      /* to be retracted.                              */
      /*===============================================*/

      else if (CVIsType(&theArg,INTEGER_BIT))
        {
         /*==========================================*/
         /* A fact index must be a positive integer. */
         /*==========================================*/

         factIndex = theArg.integerValue->contents;
         if (factIndex < 0)
           {
            UDFInvalidArgumentMessage(context,"fact-address, fact-index, or the symbol *");
            return;
           }

         /*================================================*/
         /* See if a fact with the specified index exists. */
         /*================================================*/

         ptr = FindIndexedFact(theEnv,factIndex);

         /*=====================================*/
         /* If the fact exists then retract it, */
         /* otherwise print an error message.   */
         /*=====================================*/

         if (ptr != NULL)
           { Retract(ptr); }
         else
           {
            char tempBuffer[20];
            gensnprintf(tempBuffer,sizeof(tempBuffer),"f-%lld",factIndex);
            CantFindItemErrorMessage(theEnv,"fact",tempBuffer,false);
           }
        }

      /*============================================*/
      /* Otherwise if the argument evaluates to the */
      /* symbol *, then all facts are retracted.    */
      /*============================================*/

      else if ((CVIsType(&theArg,SYMBOL_BIT)) ?
               (strcmp(theArg.lexemeValue->contents,"*") == 0) : false)
        {
         RetractAllFacts(theEnv);
         return;
        }

      /*============================================*/
      /* Otherwise the argument has evaluated to an */
      /* illegal value for the retract command.     */
      /*============================================*/

      else
        {
         UDFInvalidArgumentMessage(context,"fact-address, fact-index, or the symbol *");
         SetEvaluationError(theEnv,true);
        }
     }
  }

/***************************************************/
/* SetFactDuplicationCommand: H/L access routine   */
/*   for the set-fact-duplication command.         */
/***************************************************/
void SetFactDuplicationCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue theArg;

   /*=====================================================*/
   /* Get the old value of the fact duplication behavior. */
   /*=====================================================*/

   returnValue->lexemeValue = CreateBoolean(theEnv,GetFactDuplication(theEnv));

   /*========================*/
   /* Evaluate the argument. */
   /*========================*/

   if (! UDFFirstArgument(context,ANY_TYPE_BITS,&theArg))
     { return; }

   /*===============================================================*/
   /* If the argument evaluated to false, then the fact duplication */
   /* behavior is disabled, otherwise it is enabled.                */
   /*===============================================================*/

   SetFactDuplication(theEnv,theArg.value != FalseSymbol(theEnv));
  }

/***************************************************/
/* GetFactDuplicationCommand: H/L access routine   */
/*   for the get-fact-duplication command.         */
/***************************************************/
void GetFactDuplicationCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   returnValue->lexemeValue = CreateBoolean(theEnv,GetFactDuplication(theEnv));
  }

/*******************************************/
/* FactIndexFunction: H/L access routine   */
/*   for the fact-index function.          */
/*******************************************/
void FactIndexFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue theArg;

   /*======================================*/
   /* The argument must be a fact address. */
   /*======================================*/

   if (! UDFFirstArgument(context,FACT_ADDRESS_BIT,&theArg))
     { return; }

   /*================================================*/
   /* Return the fact index associated with the fact */
   /* address. If the fact has been retracted, then  */
   /* return -1 for the fact index.                  */
   /*================================================*/

   if (theArg.factValue->garbage)
     {
      FactRetractedErrorMessage(theEnv,theArg.factValue);
      returnValue->integerValue = CreateInteger(theEnv,-1L);
      return;
     }

   returnValue->integerValue = CreateInteger(theEnv,FactIndex(theArg.factValue));
  }

#if DEBUGGING_FUNCTIONS

/**************************************/
/* FactsCommand: H/L access routine   */
/*   for the facts command.           */
/**************************************/
void FactsCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   FactsGoalsCommand(theEnv,context,returnValue,true);
  }

/**************************************/
/* GoalsCommand: H/L access routine   */
/*   for the goals command.           */
/**************************************/
void GoalsCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   FactsGoalsCommand(theEnv,context,returnValue,false);
  }

/*****************************************/
/* FactsGoalsCommand: Driver routine for */
/*   the H/L facts and goals commands.   */
/*****************************************/
static void FactsGoalsCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue,
  bool factList)
  {
   long long start = UNSPECIFIED, end = UNSPECIFIED, max = UNSPECIFIED;
   Defmodule *theModule;
   UDFValue theArg;

   /*==================================*/
   /* The default module for the facts */
   /* command is the current module.   */
   /*==================================*/

   theModule = GetCurrentModule(theEnv);

   /*==========================================*/
   /* If no arguments were specified, then use */
   /* the default values to list the facts.    */
   /*==========================================*/

   if (! UDFHasNextArgument(context))
     {
      if (factList)
        { Facts(theEnv,STDOUT,theModule,start,end,max); }
      else
        { Goals(theEnv,STDOUT,theModule,start,end,max); }
      return;
     }

   /*========================================================*/
   /* Since there are one or more arguments, see if a module */
   /* or start index was specified as the first argument.    */
   /*========================================================*/

   if (! UDFFirstArgument(context,SYMBOL_BIT | INTEGER_BIT,&theArg)) return;

   /*===============================================*/
   /* If the first argument is a symbol, then check */
   /* to see that a valid module was specified.     */
   /*===============================================*/

   if (CVIsType(&theArg,SYMBOL_BIT))
     {
      theModule = LookupDefmodule(theEnv,theArg.lexemeValue);
      if ((theModule == NULL) && (strcmp(theArg.lexemeValue->contents,"*") != 0))
        {
         SetEvaluationError(theEnv,true);
         CantFindItemErrorMessage(theEnv,"defmodule",theArg.lexemeValue->contents,true);
         return;
        }

      if ((start = GetFactsArgument(context)) == INVALID) return;
     }

   /*================================================*/
   /* Otherwise if the first argument is an integer, */
   /* check to see that a valid index was specified. */
   /*================================================*/

   else if (CVIsType(&theArg,INTEGER_BIT))
     {
      start = theArg.integerValue->contents;
      if (start < 0)
        {
         if (factList)
           { ExpectedTypeError1(theEnv,"facts",1,"symbol or 'positive number'"); }
         else
           { ExpectedTypeError1(theEnv,"goals",1,"symbol or 'positive number'"); }
         UDFThrowError(context);
         return;
        }
     }

   /*==========================================*/
   /* Otherwise the first argument is invalid. */
   /*==========================================*/

   else
     {
      UDFInvalidArgumentMessage(context,"symbol or 'positive number'");
      UDFThrowError(context);
      return;
     }

   /*==========================*/
   /* Get the other arguments. */
   /*==========================*/

   if ((end = GetFactsArgument(context)) == INVALID) return;
   if ((max = GetFactsArgument(context)) == INVALID) return;

   /*=================*/
   /* List the facts. */
   /*=================*/

   if (factList)
     { Facts(theEnv,STDOUT,theModule,start,end,max); }
   else
     { Goals(theEnv,STDOUT,theModule,start,end,max); }
  }
  
/**************************************************/
/* Facts: C access routine for the facts command. */
/**************************************************/
void Facts(
  Environment *theEnv,
  const char *logicalName,
  Defmodule *theModule,
  long long start,
  long long end,
  long long max)
  {
   FactsGoalsDriver(theEnv,logicalName,theModule,start,end,max,true);
  }

/**************************************************/
/* Goals: C access routine for the goals command. */
/**************************************************/
void Goals(
  Environment *theEnv,
  const char *logicalName,
  Defmodule *theModule,
  long long start,
  long long end,
  long long max)
  {
   FactsGoalsDriver(theEnv,logicalName,theModule,start,end,max,false);
  }

/****************************************/
/* FactsGoalsDriver: Driver routine for */
/*   the facts and goals commands.      */
/****************************************/
void FactsGoalsDriver(
  Environment *theEnv,
  const char *logicalName,
  Defmodule *theModule,
  long long start,
  long long end,
  long long max,
  bool factList)
  {
   Fact *factPtr;
   unsigned long count = 0;
   Defmodule *oldModule;
   bool allModules = false;
   
   /*=====================================*/
   /* If embedded, clear the error flags. */
   /*=====================================*/
   
   if (EvaluationData(theEnv)->CurrentExpression == NULL)
     { ResetErrorFlags(theEnv); }
     
   /*==========================*/
   /* Save the current module. */
   /*==========================*/

   oldModule = GetCurrentModule(theEnv);

   /*=========================================================*/
   /* Determine if facts from all modules are to be displayed */
   /* or just facts from the current module.                  */
   /*=========================================================*/

   if (theModule == NULL) allModules = true;
   else SetCurrentModule(theEnv,theModule);

   /*=====================================*/
   /* Get the first fact to be displayed. */
   /*=====================================*/

   if (factList)
     {
      if (allModules) factPtr = GetNextFact(theEnv,NULL);
      else factPtr = GetNextFactInScope(theEnv,NULL);
     }
   else
     {
      if (allModules) factPtr = GetNextGoal(theEnv,NULL);
      else factPtr = GetNextGoalInScope(theEnv,NULL);
     }

   /*===============================*/
   /* Display facts until there are */
   /* no more facts to display.     */
   /*===============================*/

   while (factPtr != NULL)
     {
      /*==================================================*/
      /* Abort the display of facts if the Halt Execution */
      /* flag has been set (normally by user action).     */
      /*==================================================*/

      if (GetHaltExecution(theEnv) == true)
        {
         SetCurrentModule(theEnv,oldModule);
         return;
        }

      /*===============================================*/
      /* If the maximum fact index of facts to display */
      /* has been reached, then stop displaying facts. */
      /*===============================================*/

      if ((factPtr->factIndex > end) && (end != UNSPECIFIED))
        {
         if (factList)
           { PrintTally(theEnv,logicalName,count,"fact","facts"); }
         else
           { PrintTally(theEnv,logicalName,count,"goal","goals"); }
         SetCurrentModule(theEnv,oldModule);
         return;
        }

      /*================================================*/
      /* If the maximum number of facts to be displayed */
      /* has been reached, then stop displaying facts.  */
      /*================================================*/

      if (max == 0)
        {
         if (factList)
           { PrintTally(theEnv,logicalName,count,"fact","facts"); }
         else
           { PrintTally(theEnv,logicalName,count,"goal","goals"); }
         SetCurrentModule(theEnv,oldModule);
         return;
        }

      /*======================================================*/
      /* If the index of the fact is greater than the minimum */
      /* starting fact index, then display the fact.          */
      /*======================================================*/

      if (factPtr->factIndex >= start)
        {
         PrintFactWithIdentifier(theEnv,logicalName,factPtr,NULL);
         WriteString(theEnv,logicalName,"\n");
         count++;
         if (max > 0) max--;
        }

      /*========================================*/
      /* Proceed to the next fact to be listed. */
      /*========================================*/

      if (factList)
        {
         if (allModules) factPtr = GetNextFact(theEnv,factPtr);
         else factPtr = GetNextFactInScope(theEnv,factPtr);
        }
      else
        {
         if (allModules) factPtr = GetNextGoal(theEnv,factPtr);
         else factPtr = GetNextGoalInScope(theEnv,factPtr);
        }
     }

   /*===================================================*/
   /* Print the total of the number of facts displayed. */
   /*===================================================*/

   if (factList)
     { PrintTally(theEnv,logicalName,count,"fact","facts"); }
   else
     { PrintTally(theEnv,logicalName,count,"goal","goals"); }

   /*=============================*/
   /* Restore the current module. */
   /*=============================*/

   SetCurrentModule(theEnv,oldModule);
  }
  
/****************************************************************/
/* GetFactsArgument: Returns an argument for the facts command. */
/*  A return value of -1 indicates that no value was specified. */
/*  A return value of -2 indicates that the value specified is  */
/*  invalid.                                                    */
/****************************************************************/
static long long GetFactsArgument(
  UDFContext *context)
  {
   long long factIndex;
   UDFValue theArg;

   if (! UDFHasNextArgument(context)) return(UNSPECIFIED);

   if (! UDFNextArgument(context,INTEGER_BIT,&theArg))
     { return(INVALID); }

   factIndex = theArg.integerValue->contents;

   if (factIndex < 0)
     {
      UDFInvalidArgumentMessage(context,"positive number");
      UDFThrowError(context);
      return(INVALID);
     }

   return(factIndex);
  }

#endif /* DEBUGGING_FUNCTIONS */

/**********************************************/
/* AssertStringFunction: H/L access routine   */
/*   for the assert-string function.          */
/**********************************************/
void AssertStringFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue theArg;
   Fact *theFact;

   /*=====================================================*/
   /* Check for the correct number and type of arguments. */
   /*=====================================================*/

   if (! UDFFirstArgument(context,STRING_BIT,&theArg))
     { return; }

   /*==========================================*/
   /* Call the driver routine for converting a */
   /* string to a fact and then assert it.     */
   /*==========================================*/

   theFact = AssertString(theEnv,theArg.lexemeValue->contents);
   if (theFact != NULL)
     { returnValue->factValue = theFact; }
   else
     { returnValue->lexemeValue = FalseSymbol(theEnv); }
  }

/****************************************************************/
/* AssertParse: Driver routine for parsing the assert function. */
/****************************************************************/
static struct expr *AssertParse(
  Environment *theEnv,
  struct expr *top,
  const char *logicalName)
  {
   bool error;
   struct expr *rv;
   struct token theToken;

   ReturnExpression(theEnv,top);
   SavePPBuffer(theEnv," ");
   IncrementIndentDepth(theEnv,8);
   rv = BuildRHSAssert(theEnv,logicalName,&theToken,&error,true,true,"assert command");
   DecrementIndentDepth(theEnv,8);
   return(rv);
  }

#endif /* DEFTEMPLATE_CONSTRUCT */
