   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 7.00  07/03/24            */
   /*                                                     */
   /*                                                     */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
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
/*            Changed name of variable log to logName        */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Added pragmas to remove compilation warnings.  */
/*                                                           */
/*      6.30: Updated ENTITY_RECORD definitions to include   */
/*            additional NULL initializers.                  */
/*                                                           */
/*            Added ReleaseProcParameters call.              */
/*                                                           */
/*            Added tracked memory calls.                    */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*      6.40: Removed LOCALE definition.                     */
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
/*      7.00: Support for ?var:slot references to facts in   */
/*            methods and rule actions.                      */
/*                                                           */
/*************************************************************/

#ifndef _H_prccode

#pragma once

#define _H_prccode

#include "expressn.h"
#include "evaluatn.h"
#include "moduldef.h"
#include "scanner.h"
#include "symbol.h"

typedef struct ProcParamStack
  {
   UDFValue *ParamArray;

#if DEFGENERIC_CONSTRUCT
   Expression *ParamExpressions;
#endif

   unsigned int ParamArraySize;
   UDFValue *WildcardValue;
   void (*UnboundErrFunc)(Environment *,const char *);
   struct ProcParamStack *nxt;
  } PROC_PARAM_STACK;

#define PROCEDURAL_PRIMITIVE_DATA 37

struct proceduralPrimitiveData
  {
   Multifield *NoParamValue;
   UDFValue *ProcParamArray;
   unsigned int ProcParamArraySize;
   Expression *CurrentProcActions;
#if DEFGENERIC_CONSTRUCT
   Expression *ProcParamExpressions;
#endif
   PROC_PARAM_STACK *pstack;
   UDFValue *WildcardValue;
   UDFValue *LocalVarArray;
   void (*ProcUnboundErrFunc)(Environment *,const char *);
   EntityRecord ProcParameterInfo;
   EntityRecord ProcWildInfo;
   EntityRecord ProcGetInfo;
   EntityRecord ProcBindInfo;
#if ! DEFFUNCTION_CONSTRUCT
   EntityRecord DeffunctionEntityRecord;
#endif
#if ! DEFGENERIC_CONSTRUCT
   EntityRecord GenericEntityRecord;
#endif
   unsigned int Oldindex;
  };

#define ProceduralPrimitiveData(theEnv) ((struct proceduralPrimitiveData *) GetEnvironmentData(theEnv,PROCEDURAL_PRIMITIVE_DATA))

   void                           InstallProcedurePrimitives(Environment *);

#if (! BLOAD_ONLY) && (! RUN_TIME)

#if DEFFUNCTION_CONSTRUCT || OBJECT_SYSTEM
   Expression                    *ParseProcParameters(Environment *,const char *,struct token *,Expression *,
                                                             CLIPSLexeme **,unsigned short *,unsigned short *,bool *,
                                                             bool (*)(Environment *,const char *));
#endif
   Expression                    *ParseProcActions(Environment *,const char *,const char *,struct token *,Expression *,CLIPSLexeme *,
                                                          int (*)(Environment *,Expression *,void *),
                                                          int (*)(Environment *,Expression *,void *),
                                                          unsigned short *,void *);
   int                            ReplaceProcVars(Environment *,const char *,Expression *,Expression *,CLIPSLexeme *,
                                                         int (*)(Environment *,Expression *,void *),void *);
#if DEFGENERIC_CONSTRUCT
   Expression                    *GenProcWildcardReference(Environment *,int);
#endif
#endif

   void                           PushProcParameters(Environment *,Expression *,unsigned int,const char *,const char *,void (*)(Environment *,const char *));
   void                           PopProcParameters(Environment *);

#if DEFGENERIC_CONSTRUCT
   Expression                    *GetProcParamExpressions(Environment *);
#endif

   void                           EvaluateProcActions(Environment *,Defmodule *,Expression *,unsigned short,
                                                      UDFValue *,void (*)(Environment *,const char *));
   void                           PrintProcParamArray(Environment *,const char *);
   void                           GrabProcWildargs(Environment *,UDFValue *,unsigned int);
   
   
   bool                           ReplaceProcBinds(Environment *,Expression *,
                                                   int (*)(Environment *,Expression *,void *),void *);
   int                            BindSlotReferenceDefault(Environment *,Expression *,void *);

#endif /* _H_prccode */

