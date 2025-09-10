   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 7.00  08/08/24             */
   /*                                                     */
   /*         IMPLICIT SYSTEM METHODS PARSING MODULE      */
   /*******************************************************/

/*************************************************************/
/* Purpose: Parsing routines for Implicit System Methods     */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*      6.24: Added pragmas to remove unused parameter       */
/*            warnings.                                      */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Support for long long integers.                */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*      6.40: Changed restrictions from char * to            */
/*            CLIPSLexeme * to support strings               */
/*            originating from sources that are not          */
/*            statically allocated.                          */
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
/*      7.00: Generic function support for deftemplates.     */
/*                                                           */
/*************************************************************/

/* =========================================
   *****************************************
               EXTERNAL DEFINITIONS
   =========================================
   ***************************************** */
#include "setup.h"

#if DEFGENERIC_CONSTRUCT && (! BLOAD_ONLY) && (! RUN_TIME)

#include <stdlib.h>

#if OBJECT_SYSTEM
#include "classcom.h"
#include "classfun.h"
#endif
#include "cstrnutl.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "extnfunc.h"
#include "genrcpsr.h"
#include "memalloc.h"
#include "prccode.h"

#include "immthpsr.h"

/* =========================================
   *****************************************
      INTERNALLY VISIBLE FUNCTION HEADERS
   =========================================
   ***************************************** */

   static void                    FormMethodsFromRestrictions(Environment *,Defgeneric *,struct functionDefinition *,Expression *);
   static RESTRICTION            *ParseRestrictionType(Environment *,unsigned);
   static Expression             *GenTypeExpression(Environment *,Expression *,int,int,const char *);
   static Expression             *ParseRestrictionCreateTypes(Environment *,CONSTRAINT_RECORD *);

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/********************************************************
  NAME         : AddImplicitMethods
  DESCRIPTION  : Adds a method(s) for a generic function
                   for an overloaded system function
  INPUTS       : A pointer to a gneeric function
  RETURNS      : Nothing useful
  SIDE EFFECTS : Method added
  NOTES        : Method marked as system
                 Assumes no other methods already present
 ********************************************************/
void AddImplicitMethods(
  Environment *theEnv,
  Defgeneric *gfunc)
  {
   struct functionDefinition *sysfunc;
   Expression action;

   sysfunc = FindFunction(theEnv,gfunc->header.name->contents);
   if (sysfunc == NULL)
     return;
   action.type = FCALL;
   action.value = sysfunc;
   action.nextArg = NULL;
   action.argList = NULL;
   FormMethodsFromRestrictions(theEnv,gfunc,sysfunc,&action);
  }

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/**********************************************************************
  NAME         : FormMethodsFromRestrictions
  DESCRIPTION  : Uses restriction string given in DefineFunction2()
                   for system function to create an equivalent method
  INPUTS       : 1) The generic function for the new methods
                 2) System function restriction string
                    (see DefineFunction2() last argument)
                 3) The actions to attach to a new method(s)
  RETURNS      : Nothing useful
  SIDE EFFECTS : Implicit method(s) created
  NOTES        : None
 **********************************************************************/
static void FormMethodsFromRestrictions(
  Environment *theEnv,
  Defgeneric *gfunc,
  struct functionDefinition *sysfunc,
  Expression *actions)
  {
   Defmethod *meth;
   Expression *plist,*tmp,*bot,*svBot;
   RESTRICTION *rptr;
   unsigned defaultc2, argRestriction2;
   int mposn;
   unsigned short min, max;
   bool needMinimumMethod;
   unsigned short i;
   const char *rstring;

   if (sysfunc->restrictions == NULL)
     { rstring = NULL; }
   else
     { rstring = sysfunc->restrictions->contents; }

   /*================================*/
   /* Extract the range of arguments */
   /* from the restriction string.   */
   /*================================*/

   min = sysfunc->minArgs;
   max = sysfunc->maxArgs;
   PopulateRestriction(theEnv,&defaultc2,ANY_TYPE_BITS,rstring,0);

   /*==================================================*/
   /* Form a list of method restrictions corresponding */
   /* to the minimum number of arguments.              */
   /*==================================================*/

   plist = bot = NULL;
   for (i = 0 ; i < min ; i++)
     {
      PopulateRestriction(theEnv,&argRestriction2,defaultc2,rstring,i+1);
      rptr = ParseRestrictionType(theEnv,argRestriction2);
      tmp = get_struct(theEnv,expr);
      tmp->argList = (Expression *) rptr;
      tmp->nextArg = NULL;
      if (plist == NULL)
        plist = tmp;
      else
        bot->nextArg = tmp;
      bot = tmp;
     }

   /*==================================*/
   /* Remember where restrictions end  */
   /* for minimum number of arguments. */
   /*==================================*/

   svBot = bot;
   needMinimumMethod = true;

   /*=====================================================*/
   /* Attach one or more new methods to correspond to the */
   /* possible variations of the extra arguments. Add a   */
   /* separate method for each specified extra argument.  */
   /*=====================================================*/

   i = 0;
   while (RestrictionExists(rstring,min+i+1))
     {
      if ((min + i + 1) == max)
        {
         if (! RestrictionExists(rstring,min+i+2))
           {
            PopulateRestriction(theEnv,&defaultc2,ANY_TYPE_BITS,rstring,min+i+1);
            break;
           }
        }

      PopulateRestriction(theEnv,&argRestriction2,defaultc2,rstring,min+i+1);
      rptr = ParseRestrictionType(theEnv,argRestriction2);

      tmp = get_struct(theEnv,expr);
      tmp->argList = (Expression *) rptr;
      tmp->nextArg = NULL;
      if (plist == NULL)
        plist = tmp;
      else
        bot->nextArg = tmp;
      bot = tmp;
      i++;
      if (RestrictionExists(rstring,min+i+1) ||
          ((min + i) == max))
        {
         FindMethodByRestrictions(theEnv,gfunc,plist,min + i,NULL,&mposn);
         meth = AddMethod(theEnv,gfunc,NULL,mposn,0,plist,min + i,0,NULL,
                          PackExpression(theEnv,actions),NULL,true);
         meth->system = 1;
        }
     }

   /*================================================*/
   /* Add a method to account for wildcard arguments */
   /* and attach a query in case there is a limit.   */
   /*================================================*/

   if ((min + i) != max)
     {
      /*==================================================*/
      /* If a wildcard is present immediately after the   */
      /* minimum number of args - then the minimum case   */
      /* will already be handled by this method. We don't */
      /* need to add an extra method for that case.       */
      /*==================================================*/

      if (i == 0)
        { needMinimumMethod = false; }

      rptr = ParseRestrictionType(theEnv,defaultc2);

      if (max != UNBOUNDED)
        {
         rptr->query = GenConstant(theEnv,FCALL,FindFunction(theEnv,"<="));
         rptr->query->argList = GenConstant(theEnv,FCALL,FindFunction(theEnv,"length$"));
         rptr->query->argList->argList = GenProcWildcardReference(theEnv,min + i + 1);
         rptr->query->argList->nextArg =
               GenConstant(theEnv,INTEGER_TYPE,CreateInteger(theEnv,(long long) (max - min - i)));
        }
      tmp = get_struct(theEnv,expr);
      tmp->argList = (Expression *) rptr;
      tmp->nextArg = NULL;
      if (plist == NULL)
        plist = tmp;
      else
        bot->nextArg = tmp;
      FindMethodByRestrictions(theEnv,gfunc,plist,min + i + 1,TrueSymbol(theEnv),&mposn);
      meth = AddMethod(theEnv,gfunc,NULL,mposn,0,plist,min + i + 1,0,TrueSymbol(theEnv),
                       PackExpression(theEnv,actions),NULL,false);
      meth->system = 1;
     }

   /*=====================================================*/
   /* When extra methods had to be added because of       */
   /* different restrictions on the optional arguments OR */
   /* the system function accepts a fixed number of args, */
   /* we must add a specific method for the minimum case. */
   /* Otherwise, the method with the wildcard covers it.  */
   /*=====================================================*/

   if (needMinimumMethod)
     {
      if (svBot != NULL)
        {
         bot = svBot->nextArg;
         svBot->nextArg = NULL;
         DeleteTempRestricts(theEnv,bot);
        }
      FindMethodByRestrictions(theEnv,gfunc,plist,min,NULL,&mposn);
      meth = AddMethod(theEnv,gfunc,NULL,mposn,0,plist,min,0,NULL,
                       PackExpression(theEnv,actions),NULL,true);
      meth->system = 1;
     }
   DeleteTempRestricts(theEnv,plist);
  }

/*******************************/
/* ParseRestrictionCreateTypes */
/*******************************/
static Expression *ParseRestrictionCreateTypes(
  Environment *theEnv,
  CONSTRAINT_RECORD *rv)
  {
   Expression *types = NULL;

   if (rv->anyAllowed == false)
     {
      if (rv->symbolsAllowed && rv->stringsAllowed)
        types = GenTypeExpression(theEnv,types,LEXEME_TYPE_CODE,-1,LEXEME_TYPE_NAME);
      else if (rv->symbolsAllowed)
        types = GenTypeExpression(theEnv,types,SYMBOL_TYPE,SYMBOL_TYPE,NULL);
      else if (rv->stringsAllowed)
        types = GenTypeExpression(theEnv,types,STRING_TYPE,STRING_TYPE,NULL);

      if (rv->floatsAllowed && rv->integersAllowed)
        types = GenTypeExpression(theEnv,types,NUMBER_TYPE_CODE,-1,NUMBER_TYPE_NAME);
      else if (rv->integersAllowed)
        types = GenTypeExpression(theEnv,types,INTEGER_TYPE,INTEGER_TYPE,NULL);
      else if (rv->floatsAllowed)
        types = GenTypeExpression(theEnv,types,FLOAT_TYPE,FLOAT_TYPE,NULL);

      if (rv->instanceNamesAllowed && rv->instanceAddressesAllowed)
        types = GenTypeExpression(theEnv,types,INSTANCE_TYPE_CODE,-1,INSTANCE_TYPE_NAME);
      else if (rv->instanceNamesAllowed)
        types = GenTypeExpression(theEnv,types,INSTANCE_NAME_TYPE,INSTANCE_NAME_TYPE,NULL);
      else if (rv->instanceAddressesAllowed)
        types = GenTypeExpression(theEnv,types,INSTANCE_ADDRESS_TYPE,INSTANCE_ADDRESS_TYPE,NULL);

      if (rv->externalAddressesAllowed && rv->instanceAddressesAllowed &&
          rv->factAddressesAllowed)
        types = GenTypeExpression(theEnv,types,ADDRESS_TYPE_CODE,-1,ADDRESS_TYPE_NAME);
      else
        {
         if (rv->externalAddressesAllowed)
           types = GenTypeExpression(theEnv,types,EXTERNAL_ADDRESS_TYPE,EXTERNAL_ADDRESS_TYPE,NULL);
         if (rv->instanceAddressesAllowed && (rv->instanceNamesAllowed == 0))
           types = GenTypeExpression(theEnv,types,INSTANCE_ADDRESS_TYPE,INSTANCE_ADDRESS_TYPE,NULL);
         if (rv->factAddressesAllowed)
           types = GenTypeExpression(theEnv,types,FACT_ADDRESS_TYPE,FACT_ADDRESS_TYPE,NULL);
        }

      if (rv->multifieldsAllowed)
        types = GenTypeExpression(theEnv,types,MULTIFIELD_TYPE,MULTIFIELD_TYPE,NULL);
     }

   return(types);
   }

/*******************************************************************
  NAME         : ParseRestrictionType
  DESCRIPTION  : Takes a string of type character codes (as given in
                 DefineFunction2()) and converts it into a method
                 restriction structure
  INPUTS       : The type character code
  RETURNS      : The restriction
  SIDE EFFECTS : Restriction allocated
  NOTES        : None
 *******************************************************************/
static RESTRICTION *ParseRestrictionType(
  Environment *theEnv,
  unsigned code)
  {
   RESTRICTION *rptr;
   CONSTRAINT_RECORD *rv;
   Expression *types = NULL;

   rptr = get_struct(theEnv,restriction);
   rptr->query = NULL;
   rv = ArgumentTypeToConstraintRecord(theEnv,code);

   types = ParseRestrictionCreateTypes(theEnv,rv);
   RemoveConstraint(theEnv,rv);
   PackRestrictionTypes(theEnv,rptr,types);
   return(rptr);
  }

/***************************************************
  NAME         : GenTypeExpression
  DESCRIPTION  : Creates an expression corresponding
                 to the type specified and adds it
                 to the front of a temporary type
                 list for a method restriction
  INPUTS       : 1) The top of the current type list
                 2) The type code when COOL is
                    not installed
                 3) The primitive type (-1 if not
                    a primitive type)
                 4) The name of the COOL class if
                    it is not a primitive type
  RETURNS      : The new top of the types list
  SIDE EFFECTS : Type node allocated and attached
  NOTES        : Restriction types in a non-COOL
                 environment are the type codes
                 given in CONSTANT.H.  In a COOL
                 environment, they are pointers
                 to classes
 ***************************************************/
static Expression *GenTypeExpression(
  Environment *theEnv,
  Expression *top,
  int nonCOOLCode,
  int primitiveCode,
  const char *COOLName)
  {
#if OBJECT_SYSTEM
#if MAC_XCD
#pragma unused(nonCOOLCode)
#endif
#else
#if MAC_XCD
#pragma unused(primitiveCode)
#pragma unused(COOLName)
#endif
#endif
   Expression *tmp;

#if OBJECT_SYSTEM
   if (primitiveCode != -1)
     tmp = GenConstant(theEnv,DEFCLASS_PTR,DefclassData(theEnv)->PrimitiveClassMap[primitiveCode]);
   else
     tmp = GenConstant(theEnv,DEFCLASS_PTR,LookupDefclassByMdlOrScope(theEnv,COOLName));
#else
   tmp = GenConstant(theEnv,INTEGER_TYPE,CreateInteger(theEnv,nonCOOLCode));
#endif
   tmp->nextArg = top;
   return(tmp);
  }

#endif /* DEFGENERIC_CONSTRUCT && (! BLOAD_ONLY) && (! RUN_TIME) */

