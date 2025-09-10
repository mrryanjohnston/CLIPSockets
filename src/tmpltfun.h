   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 7.00  11/28/24            */
   /*                                                     */
   /*          DEFTEMPLATE FUNCTION HEADER FILE           */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
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
/*      6.24: Added deftemplate-slot-names,                  */
/*            deftemplate-slot-default-value,                */
/*            deftemplate-slot-cardinality,                  */
/*            deftemplate-slot-allowed-values,               */
/*            deftemplate-slot-range,                        */
/*            deftemplate-slot-types,                        */
/*            deftemplate-slot-multip,                       */
/*            deftemplate-slot-singlep,                      */
/*            deftemplate-slot-existp, and                   */
/*            deftemplate-slot-defaultp functions.           */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Support for deftemplate slot facets.           */
/*                                                           */
/*            Added deftemplate-slot-facet-existp and        */
/*            deftemplate-slot-facet-value functions.        */
/*                                                           */
/*            Support for long long integers.                */
/*                                                           */
/*            Used gensprintf instead of sprintf.            */
/*                                                           */
/*            Support for modify callback function.          */
/*                                                           */
/*            Added additional argument to function          */
/*            CheckDeftemplateAndSlotArguments to specify    */
/*            the expected number of arguments.              */
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
/*      6.40: Removed LOCALE definition.                     */
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
/*            Modify command preserves fact id and address.  */
/*                                                           */
/*      7.00: Support for non-reactive fact patterns.        */
/*                                                           */
/*            Support for ?var:slot references to facts in   */
/*            methods and rule actions.                      */
/*                                                           */
/*************************************************************/

#ifndef _H_tmpltfun

#pragma once

#define _H_tmpltfun

#include "expressn.h"
#include "factmngr.h"
#include "symbol.h"
#include "tmpltdef.h"

   struct expr                   *ModifyParse(Environment *,struct expr *,const char *);
   struct expr                   *UpdateParse(Environment *,struct expr *,const char *);
   struct expr                   *DuplicateParse(Environment *,struct expr *,const char *);
   void                           DeftemplateFunctions(Environment *);
   void                           ModifyCommand(Environment *,UDFContext *,UDFValue *);
   void                           UpdateCommand(Environment *,UDFContext *,UDFValue *);
   void                           DuplicateCommand(Environment *,UDFContext *,UDFValue *);
   void                           DeftemplateSlotNamesFunction(Environment *,UDFContext *,UDFValue *);
   void                           DeftemplateSlotNames(Deftemplate *,CLIPSValue *);
   void                           DeftemplateSlotDefaultValueFunction(Environment *,UDFContext *,UDFValue *);
   bool                           DeftemplateSlotDefaultValue(Deftemplate *,const char *,CLIPSValue *);
   void                           DeftemplateSlotCardinalityFunction(Environment *,UDFContext *,UDFValue *);
   bool                           DeftemplateSlotCardinality(Deftemplate *,const char *,CLIPSValue *);
   void                           DeftemplateSlotAllowedValuesFunction(Environment *,UDFContext *,UDFValue *);
   bool                           DeftemplateSlotAllowedValues(Deftemplate *,const char *,CLIPSValue *);
   void                           DeftemplateSlotRangeFunction(Environment *,UDFContext *,UDFValue *);
   bool                           DeftemplateSlotRange(Deftemplate *,const char *,CLIPSValue *);
   void                           DeftemplateSlotTypesFunction(Environment *,UDFContext *,UDFValue *);
   bool                           DeftemplateSlotTypes(Deftemplate *,const char *,CLIPSValue *);
   void                           DeftemplateSlotMultiPFunction(Environment *,UDFContext *,UDFValue *);
   bool                           DeftemplateSlotMultiP(Deftemplate *,const char *);
   void                           DeftemplateSlotSinglePFunction(Environment *,UDFContext *,UDFValue *);
   bool                           DeftemplateSlotSingleP(Deftemplate *,const char *);
   void                           DeftemplateSlotExistPFunction(Environment *,UDFContext *,UDFValue *);
   bool                           DeftemplateSlotExistP(Deftemplate *,const char *);
   void                           DeftemplateSlotDefaultPFunction(Environment *,UDFContext *,UDFValue *);
   DefaultType                    DeftemplateSlotDefaultP(Deftemplate *,const char *);
   void                           DeftemplateSlotFacetExistPFunction(Environment *,UDFContext *,UDFValue *);
   bool                           DeftemplateSlotFacetExistP(Environment *,Deftemplate *,const char *,const char *);
   void                           DeftemplateSlotFacetValueFunction(Environment *,UDFContext *,UDFValue *);
   bool                           DeftemplateSlotFacetValue(Environment *,Deftemplate *,const char *,const char *,UDFValue *);
   Fact                          *ReplaceFact(Environment *,Fact *,CLIPSValue *,CLIPSBitMap *,bool);
   struct patternMatch           *NetworkRetractReplaceFact(Environment *,struct patternMatch *,CLIPSBitMap *,bool);

#endif /* _H_tmpltfun */




