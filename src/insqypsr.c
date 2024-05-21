   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 7.00  02/14/24             */
   /*                                                     */
   /*         INSTANCE-SET QUERIES PARSER MODULE          */
   /*******************************************************/

/*************************************************************/
/* Purpose: Instance_set Queries Parsing Routines            */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Changed name of variable exp to theExp         */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Fixed memory leaks when error occurred.        */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Support for long long integers.                */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Added code to keep track of pointers to        */
/*            constructs that are contained externally to    */
/*            to constructs, DanglingConstructs.             */
/*                                                           */
/*      6.31: Error check for non-symbolic slot names.       */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            Eval support for run time and bload only.      */
/*                                                           */
/*************************************************************/

/* =========================================
   *****************************************
               EXTERNAL DEFINITIONS
   =========================================
   ***************************************** */
#include "setup.h"

#if INSTANCE_SET_QUERIES

#include <string.h>

#include "classcom.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "insquery.h"
#include "prcdrpsr.h"
#include "pprint.h"
#include "prntutil.h"
#include "router.h"
#include "scanner.h"
#include "strngrtr.h"

#include "insqypsr.h"

/* =========================================
   *****************************************
                   CONSTANTS
   =========================================
   ***************************************** */
#define INSTANCE_SLOT_REF ':'

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static Expression             *ParseQueryRestrictions(Environment *,Expression *,const char *,struct token *);
   static bool                    ReplaceClassNameWithReference(Environment *,Expression *);
   static bool                    ParseQueryTestExpression(Environment *,Expression *,const char *);
   static bool                    ParseQueryActionExpression(Environment *,Expression *,const char *,Expression *,struct token *);
   static bool                    ReplaceInstanceVariables(Environment *,Expression *,Expression *,bool,int);
   static bool                    ReplaceSlotReference(Environment *,Expression *,Expression *,
                                                       struct functionDefinition *,int);
   static bool                    IsQueryFunction(Expression *);

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/***********************************************************************
  NAME         : ParseQueryNoAction
  DESCRIPTION  : Parses the following functions :
                   (any-instancep)
                   (find-first-instance)
                   (find-all-instances)
  INPUTS       : 1) The address of the top node of the query function
                 2) The logical name of the input
  RETURNS      : The completed expression chain, or NULL on errors
  SIDE EFFECTS : The expression chain is extended, or the "top" node
                 is deleted on errors
  NOTES        : H/L Syntax :

                 (<function> <query-block>)

                 <query-block>  :== (<instance-var>+) <query-expression>
                 <instance-var> :== (<var-name> <class-name>+)

                 Parses into following form :

                 <query-function>
                      |
                      V
                 <query-expression>  ->

                 <class-1a> -> <class-1b> -> (QDS) ->

                 <class-2a> -> <class-2b> -> (QDS) -> ...
 ***********************************************************************/
Expression *ParseQueryNoAction(
  Environment *theEnv,
  Expression *top,
  const char *readSource)
  {
   Expression *insQuerySetVars;
   struct token queryInputToken;

   insQuerySetVars = ParseQueryRestrictions(theEnv,top,readSource,&queryInputToken);
   if (insQuerySetVars == NULL)
     return NULL;
   IncrementIndentDepth(theEnv,3);
   PPCRAndIndent(theEnv);
   if (ParseQueryTestExpression(theEnv,top,readSource) == false)
     {
      DecrementIndentDepth(theEnv,3);
      ReturnExpression(theEnv,insQuerySetVars);
      return NULL;
     }
   DecrementIndentDepth(theEnv,3);
   GetToken(theEnv,readSource,&queryInputToken);
   if (queryInputToken.tknType != RIGHT_PARENTHESIS_TOKEN)
     {
      SyntaxErrorMessage(theEnv,"instance-set query function");
      ReturnExpression(theEnv,top);
      ReturnExpression(theEnv,insQuerySetVars);
      return NULL;
     }
   
   if (ReplaceInstanceVariables(theEnv,insQuerySetVars,top->argList,true,0))
     {
      ReturnExpression(theEnv,top);
      ReturnExpression(theEnv,insQuerySetVars);
      return NULL;
     }
   
   ReturnExpression(theEnv,insQuerySetVars);
   return top;
  }

/***********************************************************************
  NAME         : ParseQueryAction
  DESCRIPTION  : Parses the following functions :
                   (do-for-instance)
                   (do-for-all-instances)
                   (delayed-do-for-all-instances)
  INPUTS       : 1) The address of the top node of the query function
                 2) The logical name of the input
  RETURNS      : The completed expression chain, or NULL on errors
  SIDE EFFECTS : The expression chain is extended, or the "top" node
                 is deleted on errors
  NOTES        : H/L Syntax :

                 (<function> <query-block> <query-action>)

                 <query-block>  :== (<instance-var>+) <query-expression>
                 <instance-var> :== (<var-name> <class-name>+)

                 Parses into following form :

                 <query-function>
                      |
                      V
                 <query-expression> -> <query-action>  ->

                 <class-1a> -> <class-1b> -> (QDS) ->

                 <class-2a> -> <class-2b> -> (QDS) -> ...
 ***********************************************************************/
Expression *ParseQueryAction(
  Environment *theEnv,
  Expression *top,
  const char *readSource)
  {
   Expression *insQuerySetVars;
   struct token queryInputToken;

   insQuerySetVars = ParseQueryRestrictions(theEnv,top,readSource,&queryInputToken);
   if (insQuerySetVars == NULL)
     return NULL;
   IncrementIndentDepth(theEnv,3);
   PPCRAndIndent(theEnv);
   if (ParseQueryTestExpression(theEnv,top,readSource) == false)
     {
      DecrementIndentDepth(theEnv,3);
      ReturnExpression(theEnv,insQuerySetVars);
      return NULL;
     }
   PPCRAndIndent(theEnv);
   if (ParseQueryActionExpression(theEnv,top,readSource,insQuerySetVars,&queryInputToken) == false)
     {
      DecrementIndentDepth(theEnv,3);
      ReturnExpression(theEnv,insQuerySetVars);
      return NULL;
     }
   DecrementIndentDepth(theEnv,3);

   if (queryInputToken.tknType != RIGHT_PARENTHESIS_TOKEN)
     {
      SyntaxErrorMessage(theEnv,"instance-set query function");
      ReturnExpression(theEnv,top);
      ReturnExpression(theEnv,insQuerySetVars);
      return NULL;
     }

   if (ReplaceInstanceVariables(theEnv,insQuerySetVars,top->argList,true,0))
     {
      ReturnExpression(theEnv,top);
      ReturnExpression(theEnv,insQuerySetVars);
      return NULL;
     }

   ReturnExpression(theEnv,insQuerySetVars);

   return top;
  }

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/***************************************************************
  NAME         : ParseQueryRestrictions
  DESCRIPTION  : Parses the class restrictions for a query
  INPUTS       : 1) The top node of the query expression
                 2) The logical name of the input
                 3) Caller's token buffer
  RETURNS      : The instance-variable expressions
  SIDE EFFECTS : Entire query expression deleted on errors
                 Nodes allocated for restrictions and instance
                   variable expressions
                 Class restrictions attached to query-expression
                   as arguments
  NOTES        : Expects top != NULL
 ***************************************************************/
static Expression *ParseQueryRestrictions(
  Environment *theEnv,
  Expression *top,
  const char *readSource,
  struct token *queryInputToken)
  {
   Expression *insQuerySetVars = NULL,*lastInsQuerySetVars = NULL,
              *classExp = NULL,*lastClassExp,
              *tmp,*lastOne = NULL;
   bool error = false;

   SavePPBuffer(theEnv," ");
   GetToken(theEnv,readSource,queryInputToken);
   if (queryInputToken->tknType != LEFT_PARENTHESIS_TOKEN)
     goto ParseQueryRestrictionsError1;
   GetToken(theEnv,readSource,queryInputToken);
   if (queryInputToken->tknType != LEFT_PARENTHESIS_TOKEN)
     goto ParseQueryRestrictionsError1;
   while (queryInputToken->tknType == LEFT_PARENTHESIS_TOKEN)
     {
      GetToken(theEnv,readSource,queryInputToken);
      if (queryInputToken->tknType != SF_VARIABLE_TOKEN)
        goto ParseQueryRestrictionsError1;
      tmp = insQuerySetVars;
      while (tmp != NULL)
        {
         if (tmp->value == queryInputToken->value)
           {
            PrintErrorID(theEnv,"INSQYPSR",1,false);
            WriteString(theEnv,STDERR,"Duplicate instance member variable name in function '");
            WriteString(theEnv,STDERR,ExpressionFunctionCallName(top)->contents);
            WriteString(theEnv,STDERR,"'.\n");
            goto ParseQueryRestrictionsError2;
           }
         tmp = tmp->nextArg;
        }
      tmp = GenConstant(theEnv,SF_VARIABLE,queryInputToken->value);
      if (insQuerySetVars == NULL)
        insQuerySetVars = tmp;
      else
        lastInsQuerySetVars->nextArg = tmp;
      lastInsQuerySetVars = tmp;
      SavePPBuffer(theEnv," ");
      classExp = ArgumentParse(theEnv,readSource,&error);
      if (error)
        goto ParseQueryRestrictionsError2;
      if (classExp == NULL)
        goto ParseQueryRestrictionsError1;
      if (ReplaceClassNameWithReference(theEnv,classExp) == false)
        goto ParseQueryRestrictionsError2;
      lastClassExp = classExp;
      SavePPBuffer(theEnv," ");
      while ((tmp = ArgumentParse(theEnv,readSource,&error)) != NULL)
        {
         if (ReplaceClassNameWithReference(theEnv,tmp) == false)
           goto ParseQueryRestrictionsError2;
         lastClassExp->nextArg = tmp;
         lastClassExp = tmp;
         SavePPBuffer(theEnv," ");
        }
      if (error)
        goto ParseQueryRestrictionsError2;
      PPBackup(theEnv);
      PPBackup(theEnv);
      SavePPBuffer(theEnv,")");
      tmp = GenConstant(theEnv,SYMBOL_TYPE,InstanceQueryData(theEnv)->QUERY_DELIMITER_SYMBOL);
      lastClassExp->nextArg = tmp;
      lastClassExp = tmp;
      if (top->argList == NULL)
        top->argList = classExp;
      else
        lastOne->nextArg = classExp;
      lastOne = lastClassExp;
      classExp = NULL;
      SavePPBuffer(theEnv," ");
      GetToken(theEnv,readSource,queryInputToken);
     }
   if (queryInputToken->tknType != RIGHT_PARENTHESIS_TOKEN)
     goto ParseQueryRestrictionsError1;
   PPBackup(theEnv);
   PPBackup(theEnv);
   SavePPBuffer(theEnv,")");
   return(insQuerySetVars);

ParseQueryRestrictionsError1:
   SyntaxErrorMessage(theEnv,"instance-set query function");

ParseQueryRestrictionsError2:
   ReturnExpression(theEnv,classExp);
   ReturnExpression(theEnv,top);
   ReturnExpression(theEnv,insQuerySetVars);
   return NULL;
  }

/***************************************************
  NAME         : ReplaceClassNameWithReference
  DESCRIPTION  : In parsing an instance-set query,
                 this function replaces a constant
                 class name with an actual pointer
                 to the class
  INPUTS       : The expression
  RETURNS      : True if all OK, false
                 if class cannot be found
  SIDE EFFECTS : The expression type and value are
                 modified if class is found
  NOTES        : Searches current and imported
                 modules for reference
 ***************************************************/
static bool ReplaceClassNameWithReference(
  Environment *theEnv,
  Expression *theExp)
  {
   const char *theClassName;
   Defclass *theDefclass;

   if (theExp->type == SYMBOL_TYPE)
     {
      theClassName = theExp->lexemeValue->contents;
      theDefclass = LookupDefclassByMdlOrScope(theEnv,theClassName);
      if (theDefclass == NULL)
        {
         CantFindItemErrorMessage(theEnv,"class",theClassName,true);
         return false;
        }
      theExp->type = DEFCLASS_PTR;
      theExp->value = theDefclass;

#if (! RUN_TIME) && (! BLOAD_ONLY)
      if (! ConstructData(theEnv)->ParsingConstruct)
        { ConstructData(theEnv)->DanglingConstructs++; }
#endif
     }
   return true;
  }

/*************************************************************
  NAME         : ParseQueryTestExpression
  DESCRIPTION  : Parses the test-expression for a query
  INPUTS       : 1) The top node of the query expression
                 2) The logical name of the input
  RETURNS      : True if all OK, false otherwise
  SIDE EFFECTS : Entire query-expression deleted on errors
                 Nodes allocated for new expression
                 Test shoved in front of class-restrictions on
                    query argument list
  NOTES        : Expects top != NULL
 *************************************************************/
static bool ParseQueryTestExpression(
  Environment *theEnv,
  Expression *top,
  const char *readSource)
  {
   Expression *qtest;
   bool error;
   struct BindInfo *oldBindList;

   error = false;
   oldBindList = GetParsedBindNames(theEnv);
   SetParsedBindNames(theEnv,NULL);
   qtest = ArgumentParse(theEnv,readSource,&error);
   if (error == true)
     {
      ClearParsedBindNames(theEnv);
      SetParsedBindNames(theEnv,oldBindList);
      ReturnExpression(theEnv,top);
      return false;
     }
   if (qtest == NULL)
     {
      ClearParsedBindNames(theEnv);
      SetParsedBindNames(theEnv,oldBindList);
      SyntaxErrorMessage(theEnv,"instance-set query function");
      ReturnExpression(theEnv,top);
      return false;
     }
   qtest->nextArg = top->argList;
   top->argList = qtest;
   if (ParsedBindNamesEmpty(theEnv) == false)
     {
      ClearParsedBindNames(theEnv);
      SetParsedBindNames(theEnv,oldBindList);
      PrintErrorID(theEnv,"INSQYPSR",2,false);
      WriteString(theEnv,STDERR,"Binds are not allowed in instance-set query in function '");
      WriteString(theEnv,STDERR,ExpressionFunctionCallName(top)->contents);
      WriteString(theEnv,STDERR,"'.\n");
      ReturnExpression(theEnv,top);
      return false;
     }
   SetParsedBindNames(theEnv,oldBindList);
   return true;
  }

/*************************************************************
  NAME         : ParseQueryActionExpression
  DESCRIPTION  : Parses the action-expression for a query
  INPUTS       : 1) The top node of the query expression
                 2) The logical name of the input
                 3) List of query parameters
  RETURNS      : True if all OK, false otherwise
  SIDE EFFECTS : Entire query-expression deleted on errors
                 Nodes allocated for new expression
                 Action shoved in front of class-restrictions
                    and in back of test-expression on query
                    argument list
  NOTES        : Expects top != NULL && top->argList != NULL
 *************************************************************/
static bool ParseQueryActionExpression(
  Environment *theEnv,
  Expression *top,
  const char *readSource,
  Expression *insQuerySetVars,
  struct token *queryInputToken)
  {
   Expression *qaction, *tmpInsSetVars;
   struct BindInfo *oldBindList,*newBindList,*prev;

   oldBindList = GetParsedBindNames(theEnv);
   SetParsedBindNames(theEnv,NULL);
   ExpressionData(theEnv)->BreakContext = true;
   ExpressionData(theEnv)->ReturnContext = ExpressionData(theEnv)->svContexts->rtn;

   qaction = GroupActions(theEnv,readSource,queryInputToken,true,NULL,false);
   PPBackup(theEnv);
   PPBackup(theEnv);
   SavePPBuffer(theEnv,queryInputToken->printForm);

   ExpressionData(theEnv)->BreakContext = false;
   if (qaction == NULL)
     {
      ClearParsedBindNames(theEnv);
      SetParsedBindNames(theEnv,oldBindList);
      SyntaxErrorMessage(theEnv,"instance-set query function");
      ReturnExpression(theEnv,top);
      return false;
     }
   qaction->nextArg = top->argList->nextArg;
   top->argList->nextArg = qaction;
   newBindList = GetParsedBindNames(theEnv);
   prev = NULL;
   while (newBindList != NULL)
     {
      tmpInsSetVars = insQuerySetVars;
      while (tmpInsSetVars != NULL)
        {
         if (tmpInsSetVars->value == (void *) newBindList->name)
           {
            ClearParsedBindNames(theEnv);
            SetParsedBindNames(theEnv,oldBindList);
            PrintErrorID(theEnv,"INSQYPSR",3,false);
            WriteString(theEnv,STDERR,"Cannot rebind instance-set member variable ?");
            WriteString(theEnv,STDERR,tmpInsSetVars->lexemeValue->contents);
            WriteString(theEnv,STDERR," in function '");
            WriteString(theEnv,STDERR,ExpressionFunctionCallName(top)->contents);
            WriteString(theEnv,STDERR,"'.\n");
            ReturnExpression(theEnv,top);
            return false;
           }
         tmpInsSetVars = tmpInsSetVars->nextArg;
        }
      prev = newBindList;
      newBindList = newBindList->next;
     }
   if (prev == NULL)
     SetParsedBindNames(theEnv,oldBindList);
   else
     prev->next = oldBindList;
   return true;
  }

/***********************************************************************************
  NAME         : ReplaceInstanceVariables
  DESCRIPTION  : Replaces all references to instance-variables within an
                   instance query-function with function calls to query-instance
                   (which references the instance array at run-time)
  INPUTS       : 1) The instance-variable list
                 2) A boolean expression containing variable references
                 3) A flag indicating whether to allow slot references of the type
                    <instance-query-variable>:<slot-name> for direct slot access
                    or not
                 4) Nesting depth of query functions
  RETURNS      : Nothing useful
  SIDE EFFECTS : If a SF_VARIABLE node is found and is on the list of instance
                   variables, it is replaced with a query-instance function call.
  NOTES        : Other SF_VARIABLE(S) are left alone for replacement by other
                   parsers.  This implies that a user may use defgeneric,
                   defrule, and defmessage-handler variables within a query-function
                   where they do not conflict with instance-variable names.
 ***********************************************************************************/
static bool ReplaceInstanceVariables(
  Environment *theEnv,
  Expression *vlist,
  Expression *bexp,
  bool sdirect,
  int ndepth)
  {
   Expression *eptr;
   struct functionDefinition *rindx_func,*rslot_func;
   int posn;

   rindx_func = FindFunction(theEnv,"(query-instance)");
   rslot_func = FindFunction(theEnv,"(query-instance-slot)");
   while (bexp != NULL)
     {
      if (bexp->type == SF_VARIABLE)
        {
         eptr = vlist;
         posn = 0;
         while ((eptr != NULL) ? (eptr->value != bexp->value) : false)
           {
            eptr = eptr->nextArg;
            posn++;
           }
         if (eptr != NULL)
           {
            bexp->type = FCALL;
            bexp->value = rindx_func;
            eptr = GenConstant(theEnv,INTEGER_TYPE,CreateInteger(theEnv,ndepth));
            eptr->nextArg = GenConstant(theEnv,INTEGER_TYPE,CreateInteger(theEnv,posn));
            bexp->argList = eptr;
           }
         else if (sdirect == true)
           {
            if (ReplaceSlotReference(theEnv,vlist,bexp,rslot_func,ndepth))
              { return true; }
           }
        }
      if (bexp->argList != NULL)
        {
         if (IsQueryFunction(bexp))
           {
            if (ReplaceInstanceVariables(theEnv,vlist,bexp->argList,sdirect,ndepth+1))
              { return true; }
           }
         else
           {
            if (ReplaceInstanceVariables(theEnv,vlist,bexp->argList,sdirect,ndepth))
              { return true; }
           }
        }
      bexp = bexp->nextArg;
     }
     
   return false;
  }

/*************************************************************************
  NAME         : ReplaceSlotReference
  DESCRIPTION  : Replaces instance-set query function variable
                   references of the form: <instance-variable>:<slot-name>
                   with function calls to get these instance-slots at run
                   time
  INPUTS       : 1) The instance-set variable list
                 2) The expression containing the variable
                 3) The address of the instance slot access function
                 4) Nesting depth of query functions
  RETURNS      : Nothing useful
  SIDE EFFECTS : If the variable is a slot reference, then it is replaced
                   with the appropriate function-call.
  NOTES        : None
 *************************************************************************/
static bool ReplaceSlotReference(
  Environment *theEnv,
  Expression *vlist,
  Expression *theExp,
  struct functionDefinition *func,
  int ndepth)
  {
   size_t len;
   int posn;
   bool oldpp;
   size_t i;
   const char *str;
   Expression *eptr;
   struct token itkn;

   str = theExp->lexemeValue->contents;
   len =  strlen(str);
   if (len < 3)
     { return false; }
     
   for (i = len-2 ; i >= 1 ; i--)
     {
      if ((str[i] == INSTANCE_SLOT_REF) ? (i >= 1) : false)
        {
         eptr = vlist;
         posn = 0;
         while (eptr && ((i != strlen(eptr->lexemeValue->contents)) ||
                         strncmp(eptr->lexemeValue->contents,str,
                                 (STD_SIZE) i)))
           {
            eptr = eptr->nextArg;
            posn++;
           }
         if (eptr != NULL)
           {
            OpenStringSource(theEnv,"query-var",str+i+1,0);
            oldpp = GetPPBufferStatus(theEnv);
            SetPPBufferStatus(theEnv,false);
            GetToken(theEnv,"query-var",&itkn);
            SetPPBufferStatus(theEnv,oldpp);
            CloseStringSource(theEnv,"query-var");
            
            if (itkn.tknType != SYMBOL_TOKEN)
              {
               InvalidVarSlotErrorMessage(theEnv,str);
               SetEvaluationError(theEnv,true);
               return true;
              }
              
            theExp->type = FCALL;
            theExp->value = func;
            theExp->argList = GenConstant(theEnv,INTEGER_TYPE,CreateInteger(theEnv,ndepth));
            theExp->argList->nextArg = GenConstant(theEnv,INTEGER_TYPE,CreateInteger(theEnv,posn));
            theExp->argList->nextArg->nextArg = GenConstant(theEnv,TokenTypeToType(itkn.tknType),itkn.value);
            theExp->argList->nextArg->nextArg->nextArg = GenConstant(theEnv,SYMBOL_TYPE,CreateSymbol(theEnv,str));
            return false;
           }
        }
     }
     
   return false;
  }

/********************************************************************
  NAME         : IsQueryFunction
  DESCRIPTION  : Determines if an expression is a query function call
  INPUTS       : The expression
  RETURNS      : True if query function call, false otherwise
  SIDE EFFECTS : None
  NOTES        : None
 ********************************************************************/
static bool IsQueryFunction(
  Expression *theExp)
  {
   UserDefinedFunction *theFunction;
   
   if (theExp->type != FCALL)
     { return false; }
     
   theFunction = theExp->functionValue->functionPointer;

   if ((theFunction == AnyInstances) ||
       (theFunction == QueryFindInstance) ||
       (theFunction == QueryFindAllInstances) ||
       (theFunction == QueryDoForInstance) ||
       (theFunction == QueryDoForAllInstances) ||
       (theFunction == DelayedQueryDoForAllInstances))
     { return true; }

   return false;
  }

#endif
