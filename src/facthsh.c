   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 7.00  01/29/25             */
   /*                                                     */
   /*                 FACT HASHING MODULE                 */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides routines for maintaining a fact hash    */
/*   table so that duplication of facts can quickly be       */
/*   determined.                                             */
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
/*      6.40: Pragma once and other inclusion changes.       */
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
/*      6.41: Used gensnprintf in place of gensprintf and.   */
/*            sprintf.                                       */
/*                                                           */
/*      6.42: Fixed GC bug by including garbage fact and     */
/*            instances in the GC frame.                     */
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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "setup.h"

#if DEFTEMPLATE_CONSTRUCT

#include "constant.h"
#include "envrnmnt.h"
#include "factmngr.h"
#include "memalloc.h"
#include "multifld.h"
#include "prntutil.h"
#include "router.h"
#include "sysdep.h"
#include "tmpltfun.h"
#include "utility.h"

#if DEFRULE_CONSTRUCT
#include "engine.h"
#include "lgcldpnd.h"
#endif

#include "facthsh.h"

#define NAMED_FACT_INITIAL_HASH_TABLE_SIZE 17
#define NAMED_FACT_HASH_TABLE_LOAD_FACTOR 2

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static struct factHashEntry  **CreateFactHashTable(Environment *,size_t);
   static void                    ResizeFactHashTable(Environment *);
   static void                    ResetFactHashTable(Environment *);
   static bool                    FactsEqual(Multifield *,Multifield *,bool);
   static void                    UpdateHashMapSize(Environment *,size_t);
   static void                    RehashValues(struct namedFactHashTableEntry **,size_t,struct namedFactHashTableEntry **,size_t);
  
/************************************************/
/* HashFact: Returns the hash value for a fact. */
/************************************************/
size_t HashFact(
  Fact *theFact)
  {
   size_t count = 0;
   Multifield *theSegment;

   /*============================================*/
   /* Get a hash value for the deftemplate name. */
   /*============================================*/

   count += theFact->whichDeftemplate->header.name->bucket * 73981;

   /*=====================================================*/
   /* Add in the hash value for the rest of the fact. The */
   /* certainty factor is not included in the hash value. */
   /*=====================================================*/

   theSegment = &theFact->theProposition;

   if (theFact->whichDeftemplate->cfd)
     { count += HashValueArray(&theSegment->contents[1],theSegment->length-1,0); }
   else
     { count += HashValueArray(&theSegment->contents[0],theSegment->length,0); }

   /*================================*/
   /* Make sure the hash value falls */
   /* in the appropriate range.      */
   /*================================*/

   theFact->hashValue = (unsigned long) count;

   /*========================*/
   /* Return the hash value. */
   /*========================*/

   return count;
  }

/***********************************************************************/
/* HashValueArrayFact: Returns the hash value for a fact value array. */
/**********************************************************************/
size_t HashValueArrayFact(
  Deftemplate *theDeftemplate,
  CLIPSValue theArray[])
  {
   size_t count = 0;

   /*============================================*/
   /* Get a hash value for the deftemplate name. */
   /*============================================*/

   count += theDeftemplate->header.name->bucket * 73981;

   /*=================================================*/
   /* Add in the hash value for the rest of the fact. */
   /*=================================================*/

   count +=  HashValueArray(theArray,theDeftemplate->numberOfSlots,0);

   /*================================*/
   /* Make sure the hash value falls */
   /* in the appropriate range.      */
   /*================================*/

   return (unsigned long) count;
  }
  
/**********************************************/
/* FactExists: Determines if a specified fact */
/*   already exists in the fact hash table.   */
/**********************************************/
Fact *FactExists(
  Environment *theEnv,
  Fact *theFact,
  size_t hashValue)
  {
   struct factHashEntry *theFactHash;

   hashValue = (hashValue % FactData(theEnv)->FactHashTableSize);

   for (theFactHash = FactData(theEnv)->FactHashTable[hashValue];
        theFactHash != NULL;
        theFactHash = theFactHash->next)
     {
      if (theFact->hashValue != theFactHash->theFact->hashValue)
        { continue; }
        
      if (theFact->goal != theFactHash->theFact->goal)
        { continue; }

      if ((theFact->whichDeftemplate == theFactHash->theFact->whichDeftemplate) ?
          FactsEqual(&theFact->theProposition,
                     &theFactHash->theFact->theProposition,
                     theFact->whichDeftemplate->cfd) : false)
        { return(theFactHash->theFact); }
     }

   return NULL;
  }

/******************************************************/
/* FactsEqual: Determines if two facts are identical. */
/******************************************************/
static bool FactsEqual(
  Multifield *segment1,
  Multifield *segment2,
  bool cfd)
  {
   CLIPSValue *elem1;
   CLIPSValue *elem2;
   size_t length, i = 0;

   length = segment1->length;
   if (length != segment2->length)
     { return false; }

   elem1 = segment1->contents;
   elem2 = segment2->contents;

   /*========================================================*/
   /* When comparing facts with certainty factors, skip past */
   /* the first slot holding the certainty factor value.     */
   /*========================================================*/
   
   if (cfd)
     { i++; }
     
   /*==================================================*/
   /* Compare each field of both facts until the facts */
   /* match completely or the facts mismatch.          */
   /*==================================================*/

   while (i < length)
     {
      if (elem1[i].header->type == MULTIFIELD_TYPE)
        {
         if (MultifieldsEqual(elem1[i].multifieldValue,
                              elem2[i].multifieldValue) == false)
          { return false; }
        }
      else if (elem1[i].value != elem2[i].value)
        { return false; }

      i++;
     }
     
   return true;
  }

/*********************************************************/
/* FactExistsValueArray: Determines if a specified value */
/*   array already exists in the fact hash table.        */
/*********************************************************/
Fact *FactExistsValueArray(
  Environment *theEnv,
  Deftemplate *theDeftemplate,
  CLIPSValue theArray[],
  size_t hashValue)
  {
   struct factHashEntry *theFactHash;
   size_t factHashValue = hashValue;

   hashValue = (hashValue % FactData(theEnv)->FactHashTableSize);

   for (theFactHash = FactData(theEnv)->FactHashTable[hashValue];
        theFactHash != NULL;
        theFactHash = theFactHash->next)
     {
      if (factHashValue != theFactHash->theFact->hashValue)
        { continue; }
        
      if (theFactHash->theFact->goal)
        { continue; }

      if ((theDeftemplate == theFactHash->theFact->whichDeftemplate) ?
          ValueArraysEqual(theArray,theDeftemplate->numberOfSlots,
                           theFactHash->theFact->theProposition.contents,
                           theDeftemplate->numberOfSlots) : false)
        { return(theFactHash->theFact); }
     }

   return NULL;
  }

/************************************************************/
/* AddHashedFact: Adds a fact entry to the fact hash table. */
/************************************************************/
void AddHashedFact(
  Environment *theEnv,
  Fact *theFact,
  size_t hashValue)
  {
   struct factHashEntry *newhash, *temp;

   if ((FactData(theEnv)->NumberOfFacts + FactData(theEnv)->NumberOfGoals) > FactData(theEnv)->FactHashTableSize)
     { ResizeFactHashTable(theEnv); }

   newhash = get_struct(theEnv,factHashEntry);
   newhash->theFact = theFact;

   hashValue = (hashValue % FactData(theEnv)->FactHashTableSize);

   temp = FactData(theEnv)->FactHashTable[hashValue];
   FactData(theEnv)->FactHashTable[hashValue] = newhash;
   newhash->next = temp;
  }

/******************************************/
/* RemoveHashedFact: Removes a fact entry */
/*   from the fact hash table.            */
/******************************************/
bool RemoveHashedFact(
  Environment *theEnv,
  Fact *theFact)
  {
   size_t hashValue;
   struct factHashEntry *hptr, *prev;

   hashValue = HashFact(theFact);
   hashValue = (hashValue % FactData(theEnv)->FactHashTableSize);

   for (hptr = FactData(theEnv)->FactHashTable[hashValue], prev = NULL;
        hptr != NULL;
        hptr = hptr->next)
     {
      if (hptr->theFact == theFact)
        {
         if (prev == NULL)
           {
            FactData(theEnv)->FactHashTable[hashValue] = hptr->next;
            rtn_struct(theEnv,factHashEntry,hptr);
            if ((FactData(theEnv)->NumberOfFacts + FactData(theEnv)->NumberOfGoals) == 1)
              { ResetFactHashTable(theEnv); }
            return true;
           }
         else
           {
            prev->next = hptr->next;
            rtn_struct(theEnv,factHashEntry,hptr);
            if ((FactData(theEnv)->NumberOfFacts + FactData(theEnv)->NumberOfGoals) == 1)
              { ResetFactHashTable(theEnv); }
            return true;
           }
        }
      prev = hptr;
     }

   return false;
  }

/****************************************************/
/* FactWillBeAsserted: Determines if a fact will be */
/*   asserted based on the duplication settings.    */
/****************************************************/
bool FactWillBeAsserted(
  Environment *theEnv,
  Fact *theFact)
  {
   Fact *tempPtr;
   size_t hashValue;

   if (FactData(theEnv)->FactDuplication) return true;

   hashValue = HashFact(theFact);

   tempPtr = FactExists(theEnv,theFact,hashValue);
   if (tempPtr == NULL) return true;

   return false;
  }

/*****************************************************/
/* HandleFactDuplication: Determines if a fact to be */
/*   added to the fact-list is a duplicate entry and */
/*   takes appropriate action based on the current   */
/*   setting of the fact-duplication flag.           */
/*****************************************************/
size_t HandleFactDuplication(
  Environment *theEnv,
  Fact *theFact,
  Fact **duplicate,
  long long reuseIndex,
  short cf,
  CLIPSLexeme *factName,
  bool *error)
  {
   size_t hashValue;
#if CERTAINTY_FACTORS
   CFTransition transition = CF_NO_TRANSITION;
   CLIPSBitMap *theBitMap = NULL;
   short oldCF, newCF;
   bool isCFFact = theFact->whichDeftemplate->cfd;
#endif

   *duplicate = NULL;

   /*================================*/
   /* Compute the fact's hash value. */
   /*================================*/

   hashValue = HashFact(theFact);

   /*=================================================*/
   /* If the fact doesn't have a certainty factor or  */
   /* name, and duplication is allowed, then the fact */
   /* can be asserted regardless of whether it's a    */
   /* duplicate.                                      */
   /*=================================================*/
      
   if ((! isCFFact) &&
       (factName == NULL) &&
       (FactData(theEnv)->FactDuplication))
     { return hashValue; }

   /*=================================*/
   /* See if the fact already exists. */
   /*=================================*/
   
   *duplicate = FactExists(theEnv,theFact,hashValue);
   
   /*================================================*/
   /* If it's not a duplicate and it's a named fact, */
   /* the name must not be in use by another fact.   */
   /*================================================*/
   
   if ((*duplicate == NULL) && (factName != NULL))
     {
      if (LookupFact(theEnv,factName))
        {
         PrintErrorID(theEnv,"FACTMNGR",5,true);
         WriteString(theEnv,STDERR,"A fact named ");
         WriteString(theEnv,STDERR,factName->contents);
         WriteString(theEnv,STDERR," already exists.\n");
         SetEvaluationError(theEnv,true);
         ReturnFact(theEnv,theFact);
         *error = true;
         return hashValue;
        }
        
      AddNamedFactToHashMap(theEnv,theFact,factName);
     }
     
   /*=====================================================*/
   /* If the fact doesn't exist, then it can be asserted. */
   /*=====================================================*/
   
   if (*duplicate == NULL)
     {
#if CERTAINTY_FACTORS
      if (isCFFact &&
          (EngineData(theEnv)->TheLogicalJoin == NULL) &&
          (! EngineData(theEnv)->cfUpdateInProgress))
        { theFact->cfBase = cf; }
#endif
      return hashValue;
     }

   /*==============================================================*/
   /* A duplicate of a CF fact will cause the CF to be recomputed. */
   /*==============================================================*/

#if CERTAINTY_FACTORS
   if (isCFFact)
     {
      theBitMap = CreateCFChangeMap(theEnv,theFact->whichDeftemplate);
      IncrementBitMapReferenceCount(theEnv,theBitMap);

      if (EngineData(theEnv)->TheLogicalJoin == NULL)
        {
         if ((*duplicate)->cfBase == NO_CF_BASE)
           { (*duplicate)->cfBase = cf; }
         else
           { (*duplicate)->cfBase = CombineCFs((*duplicate)->cfBase,cf); }
        }
      else
        { EngineData(theEnv)->certaintyAddition = cf; }
     
      /*========================================*/
      /* Compute the combined certainty factor. */
      /*========================================*/
   
      oldCF = (short) (*duplicate)->theProposition.contents[0].integerValue->contents;
      newCF = (short) theFact->theProposition.contents[0].integerValue->contents;
      newCF = CombineCFs(oldCF,newCF);
      transition = DetermineCFTransition(oldCF,newCF);
     
      switch (transition)
        {
         case CF_NO_TRANSITION:
         case CF_UNCHANGED:
           break;
        
         case CF_UNKNOWN_TO_KNOWN:
         case CF_KNOWN_TO_UNKNOWN:
           HandleCFUpdate(theEnv,*duplicate,theBitMap,newCF,false);
           break;

         case CF_REMAINS_KNOWN_OR_UNKNOWN:
           HandleCFUpdate(theEnv,*duplicate,theBitMap,newCF,true);
           break;
        }
        
      DecrementBitMapReferenceCount(theEnv,theBitMap);
     }
#endif

   if (reuseIndex == 0) // TBD what is this for CF fact
     { ReturnFact(theEnv,theFact); }
   else
     { AddToGarbageFactList(theEnv,theFact); }

#if DEFRULE_CONSTRUCT
   if (! isCFFact)
#if CERTAINTY_FACTORS
     { AddLogicalDependencies(theEnv,(struct patternEntity *) *duplicate,true,NON_CF_FACT); }
#else
     { AddLogicalDependencies(theEnv,(struct patternEntity *) *duplicate,true,0); }
#endif
#endif

   return 0;
  }

#if CERTAINTY_FACTORS

/*****************************************************/
/* CreateCFChangeMap: Creates a change map for a CFD */
/*   deftemplate that includes just the CF slot.     */
/*****************************************************/
CLIPSBitMap *CreateCFChangeMap(
  Environment *theEnv,
  Deftemplate *theDeftemplate)
  {
   char *changeMap;
   CLIPSBitMap *theBitMap = NULL;

   if (! theDeftemplate->cfd)
     { return NULL; }
     
   changeMap = (char *) gm2(theEnv,CountToBitMapSize(theDeftemplate->numberOfSlots));
   ClearBitString((void *) changeMap,CountToBitMapSize(theDeftemplate->numberOfSlots));
   SetBitMap(changeMap,0);
   theBitMap = AddBitMap(theEnv,changeMap,CountToBitMapSize(theDeftemplate->numberOfSlots));
   rm(theEnv,(void *) changeMap,CountToBitMapSize(theDeftemplate->numberOfSlots));
   
   return theBitMap;
  }

/******************/
/* HandleCFUpdate */
/******************/
void HandleCFUpdate(
  Environment *theEnv,
  Fact *theFact,
  CLIPSBitMap *changeMap,
  short newCF,
  bool applyReactivity)
  {
   CLIPSValue *theValueArray;
   size_t i;
   struct dependency *theDependents;

   theValueArray = (CLIPSValue *) gm2(theEnv,sizeof(void *) * theFact->whichDeftemplate->numberOfSlots);
   
   theValueArray[0].integerValue = CreateInteger(theEnv,newCF);
   for (i = 1; i < theFact->theProposition.length; i++)
     { theValueArray[i].value = theFact->theProposition.contents[i].value; }

   /*===================================================*/
   /* Set the flag indicating a certainty factor update */
   /* to disable any certainty factor compuations on a  */
   /* value that has already been computed.             */
   /*===================================================*/
   
   EngineData(theEnv)->cfUpdateInProgress = true;
   
   /*============================*/
   /* Preserve the dependencies. */
   /*============================*/
   
   theDependents = theFact->patternHeader.dependents;
   theFact->patternHeader.dependents = NULL;
   
   /*======================================*/
   /* Process the certainty factor update. */
   /*======================================*/
   
   ReplaceFact(theEnv,theFact,theValueArray,changeMap,applyReactivity);
   
   /*===========================*/
   /* Restore the dependencies. */
   /*===========================*/

   if (theDependents != NULL)
     {
      if (theFact->patternHeader.dependents != NULL)
        { theFact->patternHeader.dependents->next = theDependents; }
      else
        { theFact->patternHeader.dependents = theDependents; }
     }

   /*======================================*/
   /* Certainty factor update is complete. */
   /*======================================*/
   
   EngineData(theEnv)->cfUpdateInProgress = false;

   rm(theEnv,theValueArray,sizeof(void *) * theFact->whichDeftemplate->numberOfSlots);
  }

/**************************/
/* DetermineCFTransition: */
/**************************/
CFTransition DetermineCFTransition(
  short oldCF,
  short newCF)
  {
   if (newCF == oldCF)
     { return CF_UNCHANGED; }
     
   if ((oldCF < CERTAINTY_THRESHOLD) && (newCF >= CERTAINTY_THRESHOLD))
     { return CF_UNKNOWN_TO_KNOWN; }
   else if ((oldCF >= CERTAINTY_THRESHOLD) && (newCF < CERTAINTY_THRESHOLD))
     { return CF_KNOWN_TO_UNKNOWN; }
   
   return CF_REMAINS_KNOWN_OR_UNKNOWN;
  }

/***************/
/* CombineCFs: */
/***************/
short CombineCFs(
  short cf1,
  short cf2)
  {
   int min;
   
   if ((cf1 > 0) && (cf2 > 0))
     { return (short) round((((cf1 + cf2) * 100.0) - (cf1 * cf2)) / 100.0); }
     
   if ((cf1 < 0) && (cf2 < 0))
     { return (short) round((((cf1 + cf2) * 100.0) + (cf1 * cf2)) / 100.0); }
   
   if (cf1 + cf2 == 0)
     { return 0; }

   min = (abs(cf1) > abs(cf2)) ? abs(cf2) : abs(cf1);
   
   return (short) round(((cf1 + cf2) * 100.0) / (100.0 - min));
  }
#endif

/*******************************************/
/* GetFactDuplication: C access routine    */
/*   for the get-fact-duplication command. */
/*******************************************/
bool GetFactDuplication(
  Environment *theEnv)
  {
   return FactData(theEnv)->FactDuplication;
  }

/*******************************************/
/* SetFactDuplication: C access routine    */
/*   for the set-fact-duplication command. */
/*******************************************/
bool SetFactDuplication(
  Environment *theEnv,
  bool value)
  {
   bool ov;

   ov = FactData(theEnv)->FactDuplication;
   FactData(theEnv)->FactDuplication = value;
   return ov;
  }

/**************************************************/
/* InitializeFactHashTable: Initializes the table */
/*   entries in the fact hash table to NULL.      */
/**************************************************/
void InitializeFactHashTable(
   Environment *theEnv)
   {
    FactData(theEnv)->FactHashTable = CreateFactHashTable(theEnv,SIZE_FACT_HASH);
    FactData(theEnv)->FactHashTableSize = SIZE_FACT_HASH;
   }

/*******************************************************************/
/* CreateFactHashTable: Creates and initializes a fact hash table. */
/*******************************************************************/
static struct factHashEntry **CreateFactHashTable(
   Environment *theEnv,
   size_t tableSize)
   {
    unsigned long i;
    struct factHashEntry **theTable;

    theTable = (struct factHashEntry **)
               gm2(theEnv,sizeof (struct factHashEntry *) * tableSize);

    if (theTable == NULL) ExitRouter(theEnv,EXIT_FAILURE);

    for (i = 0; i < tableSize; i++) theTable[i] = NULL;

    return theTable;
   }

/************************/
/* ResizeFactHashTable: */
/************************/
static void ResizeFactHashTable(
   Environment *theEnv)
   {
    unsigned long i, newSize, newLocation;
    struct factHashEntry **theTable, **newTable;
    struct factHashEntry *theEntry, *nextEntry;

    theTable = FactData(theEnv)->FactHashTable;

    newSize = (FactData(theEnv)->FactHashTableSize * 2) + 1;
    newTable = CreateFactHashTable(theEnv,newSize);

    /*========================================*/
    /* Copy the old entries to the new table. */
    /*========================================*/

    for (i = 0; i < FactData(theEnv)->FactHashTableSize; i++)
      {
       theEntry = theTable[i];
       while (theEntry != NULL)
         {
          nextEntry = theEntry->next;

          newLocation = theEntry->theFact->hashValue % newSize;
          theEntry->next = newTable[newLocation];
          newTable[newLocation] = theEntry;

          theEntry = nextEntry;
         }
      }

    /*=====================================================*/
    /* Replace the old hash table with the new hash table. */
    /*=====================================================*/

    rm(theEnv,theTable,sizeof(struct factHashEntry *) * FactData(theEnv)->FactHashTableSize);
    FactData(theEnv)->FactHashTableSize = newSize;
    FactData(theEnv)->FactHashTable = newTable;
   }

/***********************/
/* ResetFactHashTable: */
/***********************/
static void ResetFactHashTable(
   Environment *theEnv)
   {
    struct factHashEntry **newTable;

    /*=============================================*/
    /* Don't reset the table unless the hash table */
    /* has been expanded from its original size.   */
    /*=============================================*/

    if (FactData(theEnv)->FactHashTableSize == SIZE_FACT_HASH)
      { return; }

    /*=======================*/
    /* Create the new table. */
    /*=======================*/

    newTable = CreateFactHashTable(theEnv,SIZE_FACT_HASH);

    /*=====================================================*/
    /* Replace the old hash table with the new hash table. */
    /*=====================================================*/

    rm(theEnv,FactData(theEnv)->FactHashTable,sizeof(struct factHashEntry *) * FactData(theEnv)->FactHashTableSize);
    FactData(theEnv)->FactHashTableSize = SIZE_FACT_HASH;
    FactData(theEnv)->FactHashTable = newTable;
   }

/**************************/
/* AddNamedFactToHashMap: */
/**************************/
void AddNamedFactToHashMap(
  Environment *theEnv,
  Fact *theFact,
  CLIPSLexeme *theName)
  {
   size_t theHashTableSize, spaceNeeded, theHashValue;
   struct namedFactHashTableEntry *theEntry;
   
   if (! theFact->whichDeftemplate->named)
     { return; }
     
   FactData(theEnv)->namedFactCount++;
   
   /*======================================*/
   /* If no hash table exists, create one. */
   /*======================================*/
   
   if (FactData(theEnv)->namedFactHashTableSize == 0)
     {
      theHashTableSize = IncreaseHashSize(FactData(theEnv)->namedFactHashTableSize,NAMED_FACT_INITIAL_HASH_TABLE_SIZE);
      FactData(theEnv)->namedFactHashTableSize = theHashTableSize;
      spaceNeeded = sizeof(struct namedFactHashTableEntry *) * theHashTableSize;
      FactData(theEnv)->namedFactHashTable = (struct namedFactHashTableEntry **) gm2(theEnv,spaceNeeded);
      memset(FactData(theEnv)->namedFactHashTable,0,spaceNeeded);
     }
   else
     { theHashTableSize = FactData(theEnv)->namedFactHashTableSize; }
   
   /*===============================================*/
   /* Add the new entry to the existing hash table. */
   /*===============================================*/

   theEntry = get_struct(theEnv,namedFactHashTableEntry);
   theEntry->item = theFact;
   theEntry->name = theName;
   RetainLexeme(theEnv,theName);
   
   theHashValue = theEntry->name->hv % theHashTableSize;

   theEntry->next = FactData(theEnv)->namedFactHashTable[theHashValue];
   FactData(theEnv)->namedFactHashTable[theHashValue] = theEntry;

   /*=====================================================*/
   /* If the number of entries is within the load factor, */
   /* then the hash table doesn't need to be resized.     */
   /*=====================================================*/
   
   if (FactData(theEnv)->namedFactCount < (theHashTableSize * NAMED_FACT_HASH_TABLE_LOAD_FACTOR))
     { return; }

   /*=============================*/
   /* Create a larger hash table. */
   /*=============================*/
   
   theHashTableSize = IncreaseHashSize(FactData(theEnv)->namedFactHashTableSize,NAMED_FACT_INITIAL_HASH_TABLE_SIZE);
   UpdateHashMapSize(theEnv,theHashTableSize);
  }

/*******************************/
/* RemoveNamedFactFromHashMap: */
/*******************************/
void RemoveNamedFactFromHashMap(
  Environment *theEnv,
  CLIPSLexeme *compareName)
  {
   size_t theHashTableSize, spaceNeeded, theHashValue;
   struct namedFactHashTableEntry *theEntry, *lastEntry = NULL;
   Fact *theFact;
   bool found = false;
   CLIPSLexeme *theName;
   short namePosition;

   if (FactData(theEnv)->namedFactHashTable == NULL)
     { return; }
     
   /*==================================*/
   /* Remove the item from hash table. */
   /*==================================*/
   
   theHashValue = HashSymbol(compareName->contents,FactData(theEnv)->namedFactHashTableSize);
   
   for (theEntry = FactData(theEnv)->namedFactHashTable[theHashValue];
        theEntry != NULL;
        theEntry = theEntry->next)
     {
      theFact = theEntry->item;
      
      if (theFact->whichDeftemplate->cfd)
        { namePosition = 1; }
      else
        { namePosition = 0; }
        
      theName = theFact->theProposition.contents[namePosition].lexemeValue;
      
      if (theName == compareName)
        {
         ReleaseLexeme(theEnv,theName);
         
         if (lastEntry == NULL)
           { FactData(theEnv)->namedFactHashTable[theHashValue] = theEntry->next; }
         else
           { lastEntry->next = theEntry->next; }
           
         FactData(theEnv)->namedFactCount--;
         found = true;
         rtn_struct(theEnv,namedFactHashTableEntry,theEntry);
         break;
        }
        
      lastEntry = theEntry;
     }
   
   if (! found)
     { return; }

   /*=====================================================*/
   /* If the number of entries is within the load factor, */
   /* then the hash table doesn't need to be resized.     */
   /*=====================================================*/

   if (FactData(theEnv)->namedFactCount > (FactData(theEnv)->namedFactHashTableSize / NAMED_FACT_HASH_TABLE_LOAD_FACTOR))
     { return; }

   /*=============================================*/
   /* If no items remain, release the hash table. */
   /*=============================================*/

   if (FactData(theEnv)->namedFactCount == 0)
     {
      spaceNeeded = sizeof(struct namedFactHashTableEntry *) * FactData(theEnv)->namedFactHashTableSize;
      rm(theEnv,FactData(theEnv)->namedFactHashTable,spaceNeeded);
      FactData(theEnv)->namedFactHashTableSize = 0;
      FactData(theEnv)->namedFactHashTable = NULL;
      return;
     }
  
   /*==============================*/
   /* Create a smaller hash table. */
   /*==============================*/
   
   theHashTableSize = DecreaseHashSize(FactData(theEnv)->namedFactHashTableSize);
   UpdateHashMapSize(theEnv,theHashTableSize);
  }

/*****************************************/
/* LookupFact: Finds a fact by searching */
/*   for it in the hashmap.              */
/*****************************************/
Fact *LookupFact(
  Environment *theEnv,
  CLIPSLexeme *factName)
  {
   size_t theHashValue;
   struct namedFactHashTableEntry *theEntry;
     
   if (FactData(theEnv)->namedFactCount == 0)
     { return NULL; }

   theHashValue = factName->hv % FactData(theEnv)->namedFactHashTableSize;
        
   for (theEntry = FactData(theEnv)->namedFactHashTable[theHashValue];
        theEntry != NULL;
        theEntry = theEntry->next)
     {
      if (theEntry->name == factName)
        { return theEntry->item; }
     }

   return NULL;
  }
  
/**********************/
/* UpdateHashMapSize: */
/**********************/
static void UpdateHashMapSize(
  Environment *theEnv,
  size_t newHashTableSize)
  {
   size_t spaceNeeded;
   struct namedFactHashTableEntry **newHashTable;
   
   /*============================*/
   /* Create the new hash table. */
   /*============================*/
   
   spaceNeeded = sizeof(struct namedFactHashTableEntry *) * newHashTableSize;
   newHashTable = (struct namedFactHashTableEntry **) gm2(theEnv,spaceNeeded);
   memset(newHashTable,0,spaceNeeded);

   /*===================================*/
   /* Copy the values from the old hash */
   /* table to the new hash table.      */
   /*===================================*/
   
   RehashValues(FactData(theEnv)->namedFactHashTable,FactData(theEnv)->namedFactHashTableSize,newHashTable,newHashTableSize);

   /*===========================*/
   /* Delete the old hash table */
   /* and install the new one.  */
   /*===========================*/
   
   spaceNeeded = sizeof(struct namedFactHashTableEntry *) * FactData(theEnv)->namedFactHashTableSize;
   rm(theEnv,FactData(theEnv)->namedFactHashTable,spaceNeeded);
   
   FactData(theEnv)->namedFactHashTable = newHashTable;
   FactData(theEnv)->namedFactHashTableSize = newHashTableSize;
  }
  
/*****************************************/
/* RehashValues: Copies hash values from */
/*    one hash table to another.         */
/*****************************************/
static void RehashValues(
  struct namedFactHashTableEntry **oldHashTable,
  size_t oldHashTableSize,
  struct namedFactHashTableEntry **newHashTable,
  size_t newHashTableSize)
  {
   size_t i, theHashValue;
   struct namedFactHashTableEntry *theEntry;

   for (i = 0; i < oldHashTableSize; i++)
     {
      theEntry = oldHashTable[i];
      while (theEntry != NULL)
        {
         oldHashTable[i] = theEntry->next;
         
         theHashValue = theEntry->name->hv % newHashTableSize;
         theEntry->next = newHashTable[theHashValue];
         newHashTable[theHashValue] = theEntry;
         
         theEntry = oldHashTable[i];
        }
     }
  }

#if DEVELOPER

/****************************************************/
/* ShowFactHashTableCommand: Displays the number of */
/*   entries in each slot of the fact hash table.   */
/****************************************************/
void ShowFactHashTableCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   unsigned long i, goalCount, factCount;
   struct factHashEntry *theEntry;
   char buffer[30];

   for (i = 0; i < FactData(theEnv)->FactHashTableSize; i++)
     {
      for (theEntry =  FactData(theEnv)->FactHashTable[i], factCount = 0, goalCount = 0;
           theEntry != NULL;
           theEntry = theEntry->next)
        {
         if (theEntry->theFact->goal)
           { goalCount++; }
         else
           { factCount++; }
        }

      if ((factCount != 0) || (goalCount != 0))
        {
         snprintf(buffer,sizeof(buffer),"%5lu: f %4lu   g %4lu\n",i,factCount,goalCount);
         WriteString(theEnv,STDOUT,buffer);
        }
     }
  }

#endif /* DEVELOPER */

#endif /* DEFTEMPLATE_CONSTRUCT */

