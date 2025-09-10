   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 7.00  01/04/25            */
   /*                                                     */
   /*                 FACT HASHING MODULE                 */
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
/*      6.24: Removed LOGICAL_DEPENDENCIES compilation flag. */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Fact hash table is resizable.                  */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Added FactWillBeAsserted.                      */
/*                                                           */
/*            Converted API macros to function calls.        */
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
/*            Assert returns duplicate fact. FALSE is now    */
/*            returned only if an error occurs.              */
/*                                                           */
/*      7.00: Support for data driven backward chaining.     */
/*                                                           */
/*            Support for non-reactive fact patterns.        */
/*                                                           */
/*            Support for certainty factors.                 */
/*                                                           */
/*            Support for named facts.                       */
/*                                                           */
/*************************************************************/

#ifndef _H_facthsh

#pragma once

#define _H_facthsh

#include "entities.h"
#include "tmpltdef.h"

typedef struct factHashEntry FactHashEntry;

struct factHashEntry
  {
   Fact *theFact;
   FactHashEntry *next;
  };

struct namedFactHashTableEntry
  {
   Fact *item;
   //size_t hashValue;
   CLIPSLexeme *name;
   struct namedFactHashTableEntry *next;
  };
  
#define SIZE_FACT_HASH 16231

typedef enum
  {
   CF_NO_TRANSITION,
   CF_UNCHANGED,
   CF_UNKNOWN_TO_KNOWN,
   CF_KNOWN_TO_UNKNOWN,
   CF_REMAINS_KNOWN_OR_UNKNOWN
  } CFTransition;

   void                           AddHashedFact(Environment *,Fact *,size_t);
   bool                           RemoveHashedFact(Environment *,Fact *);
   size_t                         HandleFactDuplication(Environment *,Fact *,Fact **,long long,short,CLIPSLexeme *,bool *);
   bool                           GetFactDuplication(Environment *);
   bool                           SetFactDuplication(Environment *,bool);
   void                           InitializeFactHashTable(Environment *);
   void                           ShowFactHashTableCommand(Environment *,UDFContext *,UDFValue *);
   size_t                         HashFact(Fact *);
   size_t                         HashValueArrayFact(Deftemplate *,CLIPSValue []);
   size_t                         HashFactWithModications(Fact *,CLIPSValue *);
   bool                           FactWillBeAsserted(Environment *,Fact *);
   Fact                          *FactExists(Environment *,Fact *,size_t);
   Fact                          *FactExistsValueArray(Environment *,Deftemplate *,CLIPSValue [],size_t);
#if CERTAINTY_FACTORS
   short                          CombineCFs(short,short);
   CLIPSBitMap                   *CreateCFChangeMap(Environment *,Deftemplate *);
   CFTransition                   DetermineCFTransition(short,short);
   void                           HandleCFUpdate(Environment *,Fact *,CLIPSBitMap *,short,bool);
#endif
   void                           AddNamedFactToHashMap(Environment *,Fact *,CLIPSLexeme *);
   void                           RemoveNamedFactFromHashMap(Environment *,CLIPSLexeme *);
   Fact                          *LookupFact(Environment *,CLIPSLexeme *);
  
#endif /* _H_facthsh */


