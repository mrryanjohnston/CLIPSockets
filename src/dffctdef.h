   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 7.00  01/17/24            */
   /*                                                     */
   /*                DEFFACTS HEADER FILE                 */
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
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
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
/*      7.00: Construct hashing for quick lookup.            */
/*                                                           */
/*************************************************************/

#ifndef _H_dffctdef

#pragma once

#define _H_dffctdef

typedef struct deffacts Deffacts;

#include "constrct.h"
#include "conscomp.h"
#include "cstrccom.h"
#include "evaluatn.h"
#include "expressn.h"
#include "moduldef.h"
#include "symbol.h"

#define DEFFACTS_DATA 0

struct deffactsData
  {
   Construct *DeffactsConstruct;
   unsigned DeffactsModuleIndex;
#if CONSTRUCT_COMPILER && (! RUN_TIME)
   struct CodeGeneratorItem *DeffactsCodeItem;
#endif
  };

struct deffacts
  {
   ConstructHeader header;
   struct expr *assertList;
  };

struct deffactsModule
  {
   struct defmoduleItemHeaderHM header;
  };

#define DeffactsData(theEnv) ((struct deffactsData *) GetEnvironmentData(theEnv,DEFFACTS_DATA))

   void                           InitializeDeffacts(Environment *);
   Deffacts                      *FindDeffacts(Environment *,const char *);
   Deffacts                      *FindDeffactsInModule(Environment *,const char *);
   Deffacts                      *GetNextDeffacts(Environment *,Deffacts *);
   void                           CreateInitialFactDeffacts(void);
   bool                           DeffactsIsDeletable(Deffacts *);
   struct deffactsModule         *GetDeffactsModuleItem(Environment *,Defmodule *);
   const char                    *DeffactsModule(Deffacts *);
   const char                    *DeffactsName(Deffacts *);
   const char                    *DeffactsPPForm(Deffacts *);
#if RUN_TIME
   void                           DeffactsRunTimeInitialize(Environment *);
#endif

#endif /* _H_dffctdef */


