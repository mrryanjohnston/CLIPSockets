   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 7.00  01/07/24            */
   /*                                                     */
   /*               FACT MATCH HEADER FILE                */
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
/*      6.24: Removed INCREMENTAL_RESET compilation flag.    */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Added support for hashed alpha memories.       */
/*                                                           */
/*            Fix for DR0880. 2008-01-24                     */
/*                                                           */
/*            Added support for hashed comparisons to        */
/*            constants.                                     */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*      6.40: Removed LOCALE definition.                     */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*      7.00: Support for non-reactive fact patterns.        */
/*                                                           */
/*************************************************************/

#ifndef _H_factmch

#pragma once

#define _H_factmch

#include "evaluatn.h"
#include "factbld.h"
#include "factmngr.h"

   void                           FactPatternMatch(Environment *,Fact *,
                                                   struct factPatternNode *,size_t,size_t,
                                                   struct multifieldMarker *,
                                                   struct multifieldMarker *,CLIPSBitMap *,bool);
   void                           MarkFactPatternForIncrementalReset(Environment *,struct patternNodeHeader *,bool);
   void                           FactsIncrementalReset(Environment *);
   bool                           NodeActivatedByChanges(Environment *,struct factPatternNode *,CLIPSBitMap *,bool);

#endif /* _H_factmch */






