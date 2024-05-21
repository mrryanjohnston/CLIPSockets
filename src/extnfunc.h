   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 7.00  02/06/24            */
   /*                                                     */
   /*            EXTERNAL FUNCTIONS HEADER FILE           */
   /*******************************************************/

/*************************************************************/
/* Purpose: Routines for adding new user or system defined   */
/*   functions.                                              */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Added support for passing context information  */
/*            to user defined functions.                     */
/*                                                           */
/*            Support for long long integers.                */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Replaced ALLOW_ENVIRONMENT_GLOBALS macros      */
/*            with functions.                                */
/*                                                           */
/*      6.40: Changed restrictions from char * to            */
/*            CLIPSLexeme * to support strings               */
/*            originating from sources that are not          */
/*            statically allocated.                          */
/*                                                           */
/*            Removed LOCALE definition.                     */
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
/*            Callbacks must be environment aware.           */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*************************************************************/

#ifndef _H_extnfunc

#pragma once

#define _H_extnfunc

#include "entities.h"

#include "evaluatn.h"
#include "expressn.h"
#include "symbol.h"
#include "userdata.h"

typedef void UserDefinedFunction(Environment *,UDFContext *,UDFValue *);

struct functionDefinition
  {
   CLIPSLexeme *callFunctionName;
   const char *actualFunctionName;
   unsigned unknownReturnValueType;
   UserDefinedFunction *functionPointer;
   struct expr *(*parser)(Environment *,struct expr *,const char *);
   CLIPSLexeme *restrictions;
   unsigned short minArgs;
   unsigned short maxArgs;
   bool overloadable;
   bool sequenceuseok;
   bool neededFunction;
   unsigned long bsaveIndex;
   struct functionDefinition *next;
   struct userData *usrData;
   void *context;
  };

#define UnknownFunctionType(target) (((struct functionDefinition *) target)->unknownReturnValueType)
#define ExpressionFunctionPointer(target) ((target)->functionValue->functionPointer)
#define ExpressionFunctionCallName(target) ((target)->functionValue->callFunctionName)
#define ExpressionFunctionRealName(target) ((target)->functionValue->actualFunctionName)
#define ExpressionUnknownFunctionType(target) ((target)->functionValue->unknownReturnValueType)

/*==================*/
/* ENVIRONMENT DATA */
/*==================*/

#define EXTERNAL_FUNCTION_DATA 50

struct externalFunctionData
  {
   struct functionDefinition *ListOfFunctions;
   struct FunctionHash **FunctionHashtable;
  };

#define ExternalFunctionData(theEnv) ((struct externalFunctionData *) GetEnvironmentData(theEnv,EXTERNAL_FUNCTION_DATA))

typedef enum
  {
   AUE_NO_ERROR = 0,
   AUE_MIN_EXCEEDS_MAX_ERROR,
   AUE_FUNCTION_NAME_IN_USE_ERROR,
   AUE_INVALID_ARGUMENT_TYPE_ERROR,
   AUE_INVALID_RETURN_TYPE_ERROR
  } AddUDFError;

struct FunctionHash
  {
   struct functionDefinition *fdPtr;
   struct FunctionHash *next;
  };

#define SIZE_FUNCTION_HASH 517

   void                           InitializeExternalFunctionData(Environment *);
   AddUDFError                    AddUDF(Environment *,const char *,const char *,
                                         unsigned short,unsigned short,const char *,
                                         UserDefinedFunction *,
                                         const char *,void *);
   bool                           AddFunctionParser(Environment *,const char *,
                                                           struct expr *(*)( Environment *,struct expr *,const char *));
   bool                           RemoveFunctionParser(Environment *,const char *);
   bool                           FuncSeqOvlFlags(Environment *,const char *,bool,bool);
   struct functionDefinition     *GetFunctionList(Environment *);
   void                           InstallFunctionList(Environment *,struct functionDefinition *);
   struct functionDefinition     *FindFunction(Environment *,const char *);
   unsigned                       GetNthRestriction(Environment *,struct functionDefinition *,unsigned int);
   bool                           RemoveUDF(Environment *,const char *);
   int                            GetMinimumArgs(struct functionDefinition *);
   int                            GetMaximumArgs(struct functionDefinition *);
   unsigned int                   UDFArgumentCount(UDFContext *);
   bool                           UDFNthArgument(UDFContext *,unsigned int,unsigned,UDFValue *);
   void                           UDFInvalidArgumentMessage(UDFContext *,const char *);
   const char                    *UDFContextFunctionName(UDFContext *);
   void                           PrintTypesString(Environment *,const char *,unsigned,bool);
   bool                           UDFFirstArgument(UDFContext *,unsigned,UDFValue *);
   bool                           UDFNextArgument(UDFContext *,unsigned,UDFValue *);
   void                           UDFThrowError(UDFContext *);
   void                          *GetUDFContext(Environment *,const char *);

#define UDFHasNextArgument(context) (context->lastArg != NULL)

#endif /* _H_extnfunc */



