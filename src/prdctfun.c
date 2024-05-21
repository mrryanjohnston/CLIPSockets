   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.50  10/13/23             */
   /*                                                     */
   /*              PREDICATE FUNCTIONS MODULE             */
   /*******************************************************/

/*************************************************************/
/* Purpose: Contains the code for several predicate          */
/*   functions including not, and, or, eq, neq, <=, >=, <,   */
/*   >, =, <>, symbolp, stringp, lexemep, numberp, integerp, */
/*   floatp, oddp, evenp, multifieldp, sequencep, and        */
/*   pointerp.                                               */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Support for long long integers.                */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*            Removed the wordp and sequencep functions.     */
/*                                                           */
/*            Deprecated the pointerp function and added     */
/*            the external-addressp function.                */
/*                                                           */
/*      6.50: Support for data driven backward chaining.     */
/*                                                           */
/*************************************************************/

#include <stdio.h>

#include "setup.h"

#include "argacces.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "multifld.h"
#include "router.h"

#include "prdctfun.h"

/**************************************************/
/* PredicateFunctionDefinitions: Defines standard */
/*   math and predicate functions.                */
/**************************************************/
void PredicateFunctionDefinitions(
  Environment *theEnv)
  {
#if ! RUN_TIME
   AddUDF(theEnv,"not","b",1,1,NULL,NotFunction,"NotFunction",NULL);
   AddUDF(theEnv,"and","b",2,UNBOUNDED ,NULL,AndFunction,"AndFunction",NULL);
   AddUDF(theEnv,"or","b",2,UNBOUNDED ,NULL,OrFunction,"OrFunction",NULL);

   AddUDF(theEnv,"eq","b",2,UNBOUNDED,NULL,EqFunction,"EqFunction",NULL);
   AddUDF(theEnv,"neq","b",2,UNBOUNDED,NULL,NeqFunction,"NeqFunction",NULL);

   AddUDF(theEnv,"<=","b",2,UNBOUNDED ,"ld",LessThanOrEqualFunction,"LessThanOrEqualFunction",NULL);
   AddUDF(theEnv,">=","b",2,UNBOUNDED ,"ld",GreaterThanOrEqualFunction,"GreaterThanOrEqualFunction",NULL);
   AddUDF(theEnv,"<","b",2,UNBOUNDED ,"ld",LessThanFunction,"LessThanFunction",NULL);
   AddUDF(theEnv,">","b",2,UNBOUNDED ,"ld",GreaterThanFunction,"GreaterThanFunction",NULL);
   AddUDF(theEnv,"=","b",2,UNBOUNDED ,"ld",NumericEqualFunction,"NumericEqualFunction",NULL);
   AddUDF(theEnv,"<>","b",2,UNBOUNDED ,"ld",NumericNotEqualFunction,"NumericNotEqualFunction",NULL);
   AddUDF(theEnv,"!=","b",2,UNBOUNDED ,"ld",NumericNotEqualFunction,"NumericNotEqualFunction",NULL);

   AddUDF(theEnv,"symbolp","b",1,1,NULL,SymbolpFunction,"SymbolpFunction",NULL);
   AddUDF(theEnv,"stringp","b",1,1,NULL,StringpFunction,"StringpFunction",NULL);
   AddUDF(theEnv,"lexemep","b",1,1,NULL,LexemepFunction,"LexemepFunction",NULL);
   AddUDF(theEnv,"numberp","b",1,1,NULL,NumberpFunction,"NumberpFunction",NULL);
   AddUDF(theEnv,"integerp","b",1,1,NULL,IntegerpFunction,"IntegerpFunction",NULL);
   AddUDF(theEnv,"floatp","b",1,1,NULL,FloatpFunction,"FloatpFunction",NULL);
   AddUDF(theEnv,"oddp","b",1,1,"l",OddpFunction,"OddpFunction",NULL);
   AddUDF(theEnv,"evenp","b",1,1,"l",EvenpFunction,"EvenpFunction",NULL);
   AddUDF(theEnv,"multifieldp","b",1,1,NULL,MultifieldpFunction,"MultifieldpFunction",NULL);
   AddUDF(theEnv,"pointerp","b",1,1,NULL,ExternalAddresspFunction,"ExternalAddresspFunction",NULL);
   AddUDF(theEnv,"external-addressp","b",1,1,NULL,ExternalAddresspFunction,"ExternalAddresspFunction",NULL);
#else
#if MAC_XCD
#pragma unused(theEnv)
#endif
#endif
  }

/************************************/
/* EqFunction: H/L access routine   */
/*   for the eq function.           */
/************************************/
void EqFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue item, nextItem;
   unsigned int numArgs, i;
   struct expr *theExpression;

   /*====================================*/
   /* Determine the number of arguments. */
   /*====================================*/

   numArgs = UDFArgumentCount(context);
   if (numArgs == 0)
     {
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   /*==============================================*/
   /* Get the value of the first argument against  */
   /* which subsequent arguments will be compared. */
   /*==============================================*/

   theExpression = GetFirstArgument();
   EvaluateExpression(theEnv,theExpression,&item);

   /*=====================================*/
   /* Compare all arguments to the first. */
   /* If any are the same, return FALSE.  */
   /*=====================================*/

   for (i = 2, theExpression = GetNextArgument(theExpression);
        i <= numArgs;
        i++, theExpression = GetNextArgument(theExpression))
     {
      EvaluateExpression(theEnv,theExpression,&nextItem);

      if ((item.header->type == UQV_TYPE) &&
          (nextItem.header->type == UQV_TYPE) &&
          ((item.integerValue->contents == 0) ||
           (nextItem.integerValue->contents == 0)))
        { continue; }

      if (((item.header->type == UQV_TYPE) &&
           (item.integerValue->contents != 0)) ||
          ((nextItem.header->type == UQV_TYPE) &&
           (nextItem.integerValue->contents != 0)))
        { continue; }

      if (nextItem.header->type != item.header->type)
        {
         returnValue->lexemeValue = FalseSymbol(theEnv);
         return;
        }

      if (nextItem.header->type == MULTIFIELD_TYPE)
        {
         if (MultifieldDOsEqual(&nextItem,&item) == false)
           {
            returnValue->lexemeValue = FalseSymbol(theEnv);
            return;
           }
        }
      else if (nextItem.value != item.value)
        {
         returnValue->lexemeValue = FalseSymbol(theEnv);
         return;
        }
     }

   /*=====================================*/
   /* All of the arguments were different */
   /* from the first. Return TRUE.        */
   /*=====================================*/

   returnValue->lexemeValue = TrueSymbol(theEnv);
  }

/*************************************/
/* NeqFunction: H/L access routine   */
/*   for the neq function.           */
/*************************************/
void NeqFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue item, nextItem;
   unsigned int numArgs, i;
   struct expr *theExpression;

   /*====================================*/
   /* Determine the number of arguments. */
   /*====================================*/

   numArgs = UDFArgumentCount(context);
   if (numArgs == 0)
     {
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   /*==============================================*/
   /* Get the value of the first argument against  */
   /* which subsequent arguments will be compared. */
   /*==============================================*/

   theExpression = GetFirstArgument();
   EvaluateExpression(theEnv,theExpression,&item);

   /*=====================================*/
   /* Compare all arguments to the first. */
   /* If any are different, return FALSE. */
   /*=====================================*/

   for (i = 2, theExpression = GetNextArgument(theExpression);
        i <= numArgs;
        i++, theExpression = GetNextArgument(theExpression))
     {
      EvaluateExpression(theEnv,theExpression,&nextItem);
      
      if ((item.header->type == UQV_TYPE) &&
          (nextItem.header->type == UQV_TYPE) &&
          ((item.integerValue->contents == 0) ||
           (nextItem.integerValue->contents == 0)))
        { 
         returnValue->lexemeValue = FalseSymbol(theEnv);
         return;
        }
        
      if (((item.header->type == UQV_TYPE) &&
           (item.integerValue->contents != 0)) ||
          ((nextItem.header->type == UQV_TYPE) &&
           (nextItem.integerValue->contents != 0)))
        { 
         returnValue->lexemeValue = FalseSymbol(theEnv);
         return;
        }

      if (nextItem.header->type != item.header->type)
        { continue; }
      else if (nextItem.header->type == MULTIFIELD_TYPE)
        {
         if (MultifieldDOsEqual(&nextItem,&item) == true)
           {
            returnValue->lexemeValue = FalseSymbol(theEnv);
            return;
           }
        }
      else if (nextItem.value == item.value)
        {
         returnValue->lexemeValue = FalseSymbol(theEnv);
         return;
        }
     }

   /*=====================================*/
   /* All of the arguments were identical */
   /* to the first. Return TRUE.          */
   /*=====================================*/

   returnValue->lexemeValue = TrueSymbol(theEnv);
  }

/*****************************************/
/* StringpFunction: H/L access routine   */
/*   for the stringp function.           */
/*****************************************/
void StringpFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue item;

   if (! UDFFirstArgument(context,ANY_TYPE_BITS,&item))
     { return; }

   if (CVIsType(&item,STRING_BIT))
     { returnValue->lexemeValue = TrueSymbol(theEnv); }
   else
     { returnValue->lexemeValue = FalseSymbol(theEnv); }
  }

/*****************************************/
/* SymbolpFunction: H/L access routine   */
/*   for the symbolp function.           */
/*****************************************/
void SymbolpFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue item;

   if (! UDFFirstArgument(context,ANY_TYPE_BITS,&item))
     { return; }

   if (CVIsType(&item,SYMBOL_BIT))
     { returnValue->lexemeValue = TrueSymbol(theEnv); }
   else
     { returnValue->lexemeValue = FalseSymbol(theEnv); }
  }

/*****************************************/
/* LexemepFunction: H/L access routine   */
/*   for the lexemep function.           */
/*****************************************/
void LexemepFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue item;

   if (! UDFFirstArgument(context,ANY_TYPE_BITS,&item))
     { return; }

   if (CVIsType(&item,LEXEME_BITS))
     { returnValue->lexemeValue = TrueSymbol(theEnv); }
   else
     { returnValue->lexemeValue = FalseSymbol(theEnv); }
  }

/*****************************************/
/* NumberpFunction: H/L access routine   */
/*   for the numberp function.           */
/*****************************************/
void NumberpFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue item;

   if (! UDFFirstArgument(context,ANY_TYPE_BITS,&item))
     { return; }

   if (CVIsType(&item,NUMBER_BITS))
     { returnValue->lexemeValue = TrueSymbol(theEnv); }
   else
     { returnValue->lexemeValue = FalseSymbol(theEnv); }
  }

/****************************************/
/* FloatpFunction: H/L access routine   */
/*   for the floatp function.           */
/****************************************/
void FloatpFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue item;

   if (! UDFFirstArgument(context,ANY_TYPE_BITS,&item))
     { return; }

   if (CVIsType(&item,FLOAT_BIT))
     { returnValue->lexemeValue = TrueSymbol(theEnv); }
   else
     { returnValue->lexemeValue = FalseSymbol(theEnv); }
  }

/******************************************/
/* IntegerpFunction: H/L access routine   */
/*   for the integerp function.           */
/******************************************/
void IntegerpFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue item;

   if (! UDFFirstArgument(context,ANY_TYPE_BITS,&item))
     { return; }

   if (CVIsType(&item,INTEGER_BIT))
     { returnValue->lexemeValue = TrueSymbol(theEnv); }
   else
     { returnValue->lexemeValue = FalseSymbol(theEnv); }
  }

/*********************************************/
/* MultifieldpFunction: H/L access routine   */
/*   for the multifieldp function.           */
/*********************************************/
void MultifieldpFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue item;

   if (! UDFFirstArgument(context,ANY_TYPE_BITS,&item))
     { return; }

   if (CVIsType(&item,MULTIFIELD_BIT))
     { returnValue->lexemeValue = TrueSymbol(theEnv); }
   else
     { returnValue->lexemeValue = FalseSymbol(theEnv); }
  }

/************************************************/
/* ExternalAddresspFunction: H/L access routine */
/*   for the external-addressp function.        */
/************************************************/
void ExternalAddresspFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue item;

   if (! UDFFirstArgument(context,ANY_TYPE_BITS,&item))
     { return; }

   if (CVIsType(&item,EXTERNAL_ADDRESS_BIT))
     { returnValue->lexemeValue = TrueSymbol(theEnv); }
   else
     { returnValue->lexemeValue = FalseSymbol(theEnv); }
  }

/***********************************/
/* NotFunction: H/L access routine */
/*   for the not function.         */
/***********************************/
void NotFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue theArg;

   if (! UDFFirstArgument(context,ANY_TYPE_BITS,&theArg))
     { return; }

   if (theArg.value == FalseSymbol(theEnv))
     { returnValue->lexemeValue = TrueSymbol(theEnv); }
   else
     { returnValue->lexemeValue = FalseSymbol(theEnv); }
  }

/*************************************/
/* AndFunction: H/L access routine   */
/*   for the and function.           */
/*************************************/
void AndFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue theArg;

   while (UDFHasNextArgument(context))
     {
      if (! UDFNextArgument(context,ANY_TYPE_BITS,&theArg))
        { return; }

      if (theArg.value == FalseSymbol(theEnv))
        {
         returnValue->lexemeValue = FalseSymbol(theEnv);
         return;
        }
     }

   returnValue->lexemeValue = TrueSymbol(theEnv);
  }

/************************************/
/* OrFunction: H/L access routine   */
/*   for the or function.           */
/************************************/
void OrFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue theArg;

   while (UDFHasNextArgument(context))
     {
      if (! UDFNextArgument(context,ANY_TYPE_BITS,&theArg))
        { return; }

      if (theArg.value != FalseSymbol(theEnv))
        {
         returnValue->lexemeValue = TrueSymbol(theEnv);
         return;
        }
     }

   returnValue->lexemeValue = FalseSymbol(theEnv);
  }

/*****************************************/
/* LessThanOrEqualFunction: H/L access   */
/*   routine for the <= function.        */
/*****************************************/
void LessThanOrEqualFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue rv1, rv2;

   /*=========================*/
   /* Get the first argument. */
   /*=========================*/

   if (! UDFFirstArgument(context,NUMBER_BITS,&rv1))
     { return; }

   /*====================================================*/
   /* Compare each of the subsequent arguments to its    */
   /* predecessor. If any is greater, then return FALSE. */
   /*====================================================*/

   while (UDFHasNextArgument(context))
     {
      if (! UDFNextArgument(context,NUMBER_BITS,&rv2))
        { return; }

      if (CVIsType(&rv1,INTEGER_BIT) && CVIsType(&rv2,INTEGER_BIT))
        {
         if (rv1.integerValue->contents > rv2.integerValue->contents)
           {
            returnValue->lexemeValue = FalseSymbol(theEnv);
            return;
           }
        }
      else
        {
         if (CVCoerceToFloat(&rv1) > CVCoerceToFloat(&rv2))
           {
            returnValue->lexemeValue = FalseSymbol(theEnv);
            return;
           }
        }

      rv1.value = rv2.value;
     }

   /*======================================*/
   /* Each argument was less than or equal */
   /* to its predecessor. Return TRUE.     */
   /*======================================*/

   returnValue->lexemeValue = TrueSymbol(theEnv);
  }

/********************************************/
/* GreaterThanOrEqualFunction: H/L access   */
/*   routine for the >= function.           */
/********************************************/
void GreaterThanOrEqualFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue rv1, rv2;

   /*=========================*/
   /* Get the first argument. */
   /*=========================*/

   if (! UDFFirstArgument(context,NUMBER_BITS,&rv1))
     { return; }

   /*===================================================*/
   /* Compare each of the subsequent arguments to its   */
   /* predecessor. If any is lesser, then return false. */
   /*===================================================*/

   while (UDFHasNextArgument(context))
     {
      if (! UDFNextArgument(context,NUMBER_BITS,&rv2))
        { return; }

      if (CVIsType(&rv1,INTEGER_BIT) && CVIsType(&rv2,INTEGER_BIT))
        {
         if (rv1.integerValue->contents < rv2.integerValue->contents)
           {
            returnValue->lexemeValue = FalseSymbol(theEnv);
            return;
           }
        }
      else
        {
         if (CVCoerceToFloat(&rv1) < CVCoerceToFloat(&rv2))
           {
            returnValue->lexemeValue = FalseSymbol(theEnv);
            return;
           }
        }

      rv1.value = rv2.value;
     }

   /*=========================================*/
   /* Each argument was greater than or equal */
   /* to its predecessor. Return TRUE.        */
   /*=========================================*/

   returnValue->lexemeValue = TrueSymbol(theEnv);
  }

/**********************************/
/* LessThanFunction: H/L access   */
/*   routine for the < function.  */
/**********************************/
void LessThanFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue rv1, rv2;

   /*=========================*/
   /* Get the first argument. */
   /*=========================*/

   if (! UDFFirstArgument(context,NUMBER_BITS,&rv1))
     { return; }

   /*==========================================*/
   /* Compare each of the subsequent arguments */
   /* to its predecessor. If any is greater or */
   /* equal, then return FALSE.                */
   /*==========================================*/

   while (UDFHasNextArgument(context))
     {
      if (! UDFNextArgument(context,NUMBER_BITS,&rv2))
        { return; }

      if (CVIsType(&rv1,INTEGER_BIT) && CVIsType(&rv2,INTEGER_BIT))
        {
         if (rv1.integerValue->contents >= rv2.integerValue->contents)
           {
            returnValue->lexemeValue = FalseSymbol(theEnv);
            return;
           }
        }
      else
        {
         if (CVCoerceToFloat(&rv1) >= CVCoerceToFloat(&rv2))
           {
            returnValue->lexemeValue = FalseSymbol(theEnv);
            return;
           }
        }

      rv1.value = rv2.value;
     }

   /*=================================*/
   /* Each argument was less than its */
   /* predecessor. Return TRUE.       */
   /*=================================*/

   returnValue->lexemeValue = TrueSymbol(theEnv);
  }

/*************************************/
/* GreaterThanFunction: H/L access   */
/*   routine for the > function.     */
/*************************************/
void GreaterThanFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue rv1, rv2;

   /*=========================*/
   /* Get the first argument. */
   /*=========================*/

   if (! UDFFirstArgument(context,NUMBER_BITS,&rv1))
     { return; }

   /*==========================================*/
   /* Compare each of the subsequent arguments */
   /* to its predecessor. If any is lesser or  */
   /* equal, then return FALSE.                */
   /*==========================================*/

   while (UDFHasNextArgument(context))
     {
      if (! UDFNextArgument(context,NUMBER_BITS,&rv2))
        { return; }

      if (CVIsType(&rv1,INTEGER_BIT) && CVIsType(&rv2,INTEGER_BIT))
        {
         if (rv1.integerValue->contents <= rv2.integerValue->contents)
           {
            returnValue->lexemeValue = FalseSymbol(theEnv);
            return;
           }
        }
      else
        {
         if (CVCoerceToFloat(&rv1) <= CVCoerceToFloat(&rv2))
           {
            returnValue->lexemeValue = FalseSymbol(theEnv);
            return;
           }
        }

      rv1.value = rv2.value;
     }

   /*================================*/
   /* Each argument was greater than */
   /* its predecessor. Return TRUE.  */
   /*================================*/

   returnValue->lexemeValue = TrueSymbol(theEnv);
  }

/**************************************/
/* NumericEqualFunction: H/L access   */
/*   routine for the = function.      */
/**************************************/
void NumericEqualFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue rv1, rv2;

   /*=========================*/
   /* Get the first argument. */
   /*=========================*/

   if (! UDFFirstArgument(context,NUMBER_BITS,&rv1))
     { return; }

   /*=================================================*/
   /* Compare each of the subsequent arguments to the */
   /* first. If any is unequal, then return FALSE.    */
   /*=================================================*/

   while (UDFHasNextArgument(context))
     {
      if (! UDFNextArgument(context,NUMBER_BITS,&rv2))
        { return; }

      if (CVIsType(&rv1,INTEGER_BIT) && CVIsType(&rv2,INTEGER_BIT))
        {
         if (rv1.integerValue->contents != rv2.integerValue->contents)
           {
            returnValue->lexemeValue = FalseSymbol(theEnv);
            return;
           }
        }
      else
        {
         if (CVCoerceToFloat(&rv1) != CVCoerceToFloat(&rv2))
           {
            returnValue->lexemeValue = FalseSymbol(theEnv);
            return;
           }
        }
     }

   /*=================================*/
   /* All arguments were equal to the */
   /* first argument. Return TRUE.    */
   /*=================================*/

   returnValue->lexemeValue = TrueSymbol(theEnv);
  }

/*****************************************/
/* NumericNotEqualFunction: H/L access   */
/*   routine for the <> function.        */
/*****************************************/
void NumericNotEqualFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue rv1, rv2;

   /*=========================*/
   /* Get the first argument. */
   /*=========================*/

   if (! UDFFirstArgument(context,NUMBER_BITS,&rv1))
     { return; }

   /*=================================================*/
   /* Compare each of the subsequent arguments to the */
   /* first. If any is equal, then return FALSE.      */
   /*=================================================*/

   while (UDFHasNextArgument(context))
     {
      if (! UDFNextArgument(context,NUMBER_BITS,&rv2))
        { return; }

      if (CVIsType(&rv1,INTEGER_BIT) && CVIsType(&rv2,INTEGER_BIT))
        {
         if (rv1.integerValue->contents == rv2.integerValue->contents)
           {
            returnValue->lexemeValue = FalseSymbol(theEnv);
            return;
           }
        }
      else
        {
         if (CVCoerceToFloat(&rv1) == CVCoerceToFloat(&rv2))
           {
            returnValue->lexemeValue = FalseSymbol(theEnv);
            return;
           }
        }
     }

   /*===================================*/
   /* All arguments were unequal to the */
   /* first argument. Return TRUE.      */
   /*===================================*/

   returnValue->lexemeValue = TrueSymbol(theEnv);
  }

/**************************************/
/* OddpFunction: H/L access routine   */
/*   for the oddp function.           */
/**************************************/
void OddpFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue item;
   long long num, halfnum;

   /*===========================================*/
   /* Check for the correct types of arguments. */
   /*===========================================*/

   if (! UDFFirstArgument(context,INTEGER_BIT,&item))
     { return; }

   /*===========================*/
   /* Compute the return value. */
   /*===========================*/

   num = item.integerValue->contents;
   halfnum = (num / 2) * 2;

   if (num == halfnum) returnValue->lexemeValue = FalseSymbol(theEnv);
   else returnValue->lexemeValue = TrueSymbol(theEnv);
  }

/***************************************/
/* EvenpFunction: H/L access routine   */
/*   for the evenp function.           */
/***************************************/
void EvenpFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue item;
   long long num, halfnum;

   /*===========================================*/
   /* Check for the correct types of arguments. */
   /*===========================================*/

   if (! UDFFirstArgument(context,INTEGER_BIT,&item))
     { return; }

   /*===========================*/
   /* Compute the return value. */
   /*===========================*/

   num = item.integerValue->contents;;
   halfnum = (num / 2) * 2;

   if (num != halfnum) returnValue->lexemeValue = FalseSymbol(theEnv);
   else returnValue->lexemeValue = TrueSymbol(theEnv);
  }



