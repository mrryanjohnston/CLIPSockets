   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 7.00  01/18/24            */
   /*                                                     */
   /*                DEFGLOBAL HEADER FILE                */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
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
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Corrected code to remove run-time program      */
/*            compiler warning.                              */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Changed garbage collection algorithm.          */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Fixed linkage issue when BLOAD_ONLY compiler   */
/*            flag is set to 1.                              */
/*                                                           */
/*            Changed find construct functionality so that   */
/*            imported modules are search when locating a    */
/*            named construct.                               */
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
/*      7.00: Construct hashing for quick lookup.            */
/*                                                           */
/*************************************************************/

#ifndef _H_globldef

#pragma once

#define _H_globldef

typedef struct defglobal Defglobal;

#include "conscomp.h"
#include "constrct.h"
#include "cstrccom.h"
#include "evaluatn.h"
#include "expressn.h"
#include "moduldef.h"
#include "symbol.h"

#define DEFGLOBAL_DATA 1

struct defglobalData
  {
   Construct *DefglobalConstruct;
   unsigned DefglobalModuleIndex;
   bool ChangeToGlobals;
#if DEBUGGING_FUNCTIONS
   bool WatchGlobals;
#endif
   bool ResetGlobals;
   struct entityRecord GlobalInfo;
   struct entityRecord DefglobalPtrRecord;
   long LastModuleIndex;
   Defmodule *TheDefmodule;
#if CONSTRUCT_COMPILER && (! RUN_TIME)
   struct CodeGeneratorItem *DefglobalCodeItem;
#endif
  };

struct defglobal
  {
   ConstructHeader header;
   unsigned int watch   : 1;
   unsigned int inScope : 1;
   long busyCount;
   CLIPSValue current;
   struct expr *initial;
  };

struct defglobalModule
  {
   struct defmoduleItemHeaderHM header;
  };

#define DefglobalData(theEnv) ((struct defglobalData *) GetEnvironmentData(theEnv,DEFGLOBAL_DATA))

   void                           InitializeDefglobals(Environment *);
   Defglobal                     *FindDefglobal(Environment *,const char *);
   Defglobal                     *FindDefglobalInModule(Environment *,const char *);
   Defglobal                     *GetNextDefglobal(Environment *,Defglobal *);
   void                           CreateInitialFactDefglobal(void);
   bool                           DefglobalIsDeletable(Defglobal *);
   struct defglobalModule        *GetDefglobalModuleItem(Environment *,Defmodule *);
   void                           QSetDefglobalValue(Environment *,Defglobal *,UDFValue *,bool);
   void                           DefglobalValueForm(Defglobal *,StringBuilder *);
   bool                           GetGlobalsChanged(Environment *);
   void                           SetGlobalsChanged(Environment *,bool);
   void                           DefglobalGetValue(Defglobal *,CLIPSValue *);
   void                           DefglobalSetValue(Defglobal *,CLIPSValue *);
   void                           DefglobalSetInteger(Defglobal *,long long);
   void                           DefglobalSetFloat(Defglobal *,double);
   void                           DefglobalSetSymbol(Defglobal *,const char *);
   void                           DefglobalSetString(Defglobal *,const char *);
   void                           DefglobalSetInstanceName(Defglobal *,const char *);
   void                           DefglobalSetCLIPSInteger(Defglobal *,CLIPSInteger *);
   void                           DefglobalSetCLIPSFloat(Defglobal *,CLIPSFloat *);
   void                           DefglobalSetCLIPSLexeme(Defglobal *,CLIPSLexeme *);
   void                           DefglobalSetFact(Defglobal *,Fact *);
   void                           DefglobalSetInstance(Defglobal *,Instance *);
   void                           DefglobalSetMultifield(Defglobal *,Multifield *);
   void                           DefglobalSetCLIPSExternalAddress(Defglobal *,CLIPSExternalAddress *);
   void                           UpdateDefglobalScope(Environment *);
   Defglobal                     *GetNextDefglobalInScope(Environment *,Defglobal *);
   bool                           QGetDefglobalUDFValue(Environment *,Defglobal *,UDFValue *);
   const char                    *DefglobalModule(Defglobal *);
   const char                    *DefglobalName(Defglobal *);
   const char                    *DefglobalPPForm(Defglobal *);
   Defglobal                     *LookupDefglobal(Environment *,CLIPSLexeme *);
#if RUN_TIME
   void                           DefglobalRunTimeInitialize(Environment *);
#endif

#endif /* _H_globldef */


