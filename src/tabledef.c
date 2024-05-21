   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 7.00  02/17/24             */
   /*                                                     */
   /*              DEFTABLE DEFINITION MODULE             */
   /*******************************************************/

/*************************************************************/
/* Purpose: Defines basic deftable primitive functions such  */
/*   as allocating and deallocating, traversing, and finding */
/*   deftable data structures.                               */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      7.00: Deftable construct added.                      */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if DEFTABLE_CONSTRUCT

#include <stdio.h>

#include "envrnmnt.h"
#include "expressn.h"
#include "memalloc.h"
#include "prntutil.h"
#include "router.h"
#include "symbol.h"
#include "tablebsc.h"
#include "tablepsr.h"

#if BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE
#include "bload.h"
#include "tablebin.h"
#endif

#if CONSTRUCT_COMPILER && (! RUN_TIME)
#include "tablecmp.h"
#endif

#include "tabledef.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static void                   *AllocateModule(Environment *);
   static void                    InitModule(Environment *,void *);
   static void                    ReturnModule(Environment *,void *);
   static void                    ReturnDeftable(Environment *,Deftable *);
   static void                    InitializeDeftableModules(Environment *);
   static void                    DeallocateDeftableData(Environment *);
   static Deftable               *LookupDeftable(Environment *,CLIPSLexeme *);

#if ! RUN_TIME
   static void                    DestroyDeftableAction(Environment *,ConstructHeader *,void *);
#else
   static void                    RuntimeDeftableAction(Environment *,ConstructHeader *,void *);
   static void                    RuntimeDeftableCleanup(Environment *,ConstructHeader *,void *);
   static void                    DeallocateDeftableCTC(Environment *);
#endif

/***********************************************************/
/* InitializeDeftable: Initializes the deftable construct. */
/***********************************************************/
void InitializeDeftable(
  Environment *theEnv)
  {
   struct entityRecord deftablePtrRecord = { "DEFTABLE_PTR", DEFTABLE_PTR,1,0,0,
                                                       NULL,NULL,NULL,
                                                       NULL,
                                                       NULL,
                                                       (EntityBusyCountFunction *) DecrementDeftableBusyCount,
                                                       (EntityBusyCountFunction *) IncrementDeftableBusyCount,
                                                       NULL,NULL,NULL,NULL,NULL };

   AllocateEnvironmentData(theEnv,DEFTABLE_DATA,sizeof(struct deftableData),DeallocateDeftableData);

   memcpy(&DeftableData(theEnv)->DeftablePtrRecord,&deftablePtrRecord,sizeof(struct entityRecord));

   InitializeDeftableModules(theEnv);

   DeftableBasicCommands(theEnv);

   DeftableData(theEnv)->DeftableConstruct =
      AddConstruct(theEnv,"deftable","deftables",ParseDeftable,
                   (FindConstructFunction *) FindDeftable,
                   GetConstructNamePointer,GetConstructPPForm,
                   GetConstructModuleItem,
                   (GetNextConstructFunction *) GetNextDeftable,
                   SetNextConstruct,
                   (IsConstructDeletableFunction *) DeftableIsDeletable,
                   (DeleteConstructFunction *) Undeftable,
                   (FreeConstructFunction *) ReturnDeftable,
                   (LookupConstructFunction *) LookupDeftable);
                   
   InstallPrimitive(theEnv,(EntityRecord *) &DeftableData(theEnv)->DeftablePtrRecord,DEFTABLE_PTR);
  }

/***************************************************/
/* DeallocateDeftableData: Deallocates environment */
/*    data for the deftable construct.             */
/***************************************************/
static void DeallocateDeftableData(
  Environment *theEnv)
  {
#if ! RUN_TIME
   struct deftableModule *theModuleItem;
   Defmodule *theModule;

#if BLOAD || BLOAD_AND_BSAVE
   if (Bloaded(theEnv)) return;
#endif

   DoForAllConstructs(theEnv,
                      DestroyDeftableAction,
                      DeftableData(theEnv)->DeftableModuleIndex,
                      false,NULL);

   for (theModule = GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = GetNextDefmodule(theEnv,theModule))
     {
      theModuleItem = (struct deftableModule *)
                      GetModuleItem(theEnv,theModule,
                                    DeftableData(theEnv)->DeftableModuleIndex);
      rtn_struct(theEnv,deftableModule,theModuleItem);
     }
#else
#if MAC_XCD
#pragma unused(theEnv)
#endif
#endif
  }

#if ! RUN_TIME
/*********************************************************/
/* DestroyDeftableAction: Action used to remove deftable */
/*   as a result of DestroyEnvironment.                  */
/*********************************************************/
static void DestroyDeftableAction(
  Environment *theEnv,
  ConstructHeader *theConstruct,
  void *buffer)
  {
#if MAC_XCD
#pragma unused(buffer)
#endif
#if (! BLOAD_ONLY) && (! RUN_TIME)
   Deftable *theDeftable = (Deftable *) theConstruct;

   if (theDeftable == NULL) return;

   ReturnPackedExpression(theEnv,theDeftable->columns);
   ReturnPackedExpression(theEnv,theDeftable->rows);
   FreeRCHT(theEnv,theDeftable->columnHashTable,theDeftable->columnHashTableSize);
   DestroyConstructHeader(theEnv,&theDeftable->header);

   rtn_struct(theEnv,deftable,theDeftable);
#else
#if MAC_XCD
#pragma unused(theEnv,theConstruct)
#endif
#endif
  }
#endif

#if RUN_TIME

/***********************************************/
/* RuntimeDeftableAction: Action to be applied */
/*   to each deftable construct when a runtime */
/*   initialization occurs.                    */
/***********************************************/
static void RuntimeDeftableAction(
  Environment *theEnv,
  ConstructHeader *theConstruct,
  void *buffer)
  {
#if MAC_XCD
#pragma unused(buffer)
#endif
   struct expr *rc;
   unsigned short index = 0;
   Deftable *theDeftable = (Deftable *) theConstruct;
   unsigned int r;

   theDeftable->header.env = theEnv;
   
   AddConstructToHashMap(theEnv,&theDeftable->header,theDeftable->header.whichModule);
   
   theDeftable->columnHashTable = AllocateRCHT(theEnv,theDeftable->columnHashTableSize);
   
   for (rc = theDeftable->columns;
        rc != NULL;
        rc = rc->nextArg)
     { AddEntryToRCHT(theEnv,theDeftable->columnHashTable,theDeftable->columnHashTableSize,rc,index++); }
     
   theDeftable->rowHashTable = AllocateRCHT(theEnv,theDeftable->rowHashTableSize);

   index = 0;
   rc = theDeftable->rows;
   for (r = 0; r < theDeftable->rowCount; r++)
     {
      AddEntryToRCHT(theEnv,theDeftable->rowHashTable,theDeftable->rowHashTableSize,rc,index++);
      rc += theDeftable->columnCount;
     }
  }

/************************************************/
/* RuntimeDeftableCleanup: Action to be applied */
/*   to each deftable construct when a runtime  */
/*   destruction occurs.                        */
/************************************************/
static void RuntimeDeftableCleanup(
  Environment *theEnv,
  ConstructHeader *theConstruct,
  void *buffer)
  {
#if MAC_XCD
#pragma unused(buffer)
#endif
   Deftable *theDeftable = (Deftable *) theConstruct;

   RemoveConstructFromHashMap(theEnv,&theDeftable->header,theDeftable->header.whichModule);
   
   FreeRCHT(theEnv,theDeftable->columnHashTable,theDeftable->columnHashTableSize);
   FreeRCHT(theEnv,theDeftable->rowHashTable,theDeftable->rowHashTableSize);
  }

/**************************/
/* DeallocateDeftableCTC: */
/**************************/
static void DeallocateDeftableCTC(
   Environment *theEnv)
   {
    DoForAllConstructs(theEnv,RuntimeDeftableCleanup,DeftableData(theEnv)->DeftableModuleIndex,true,NULL);
   }

/******************************/
/* DeftableRunTimeInitialize: */
/******************************/
void DeftableRunTimeInitialize(
  Environment *theEnv)
  {
   DoForAllConstructs(theEnv,RuntimeDeftableAction,DeftableData(theEnv)->DeftableModuleIndex,true,NULL);
   AddEnvironmentCleanupFunction(theEnv,"deftablectc",DeallocateDeftableCTC,0);
  }

#endif

/*******************************************************/
/* InitializeDeftableModules: Initializes the deftable */
/*   construct for use with the defmodule construct.   */
/*******************************************************/
static void InitializeDeftableModules(
  Environment *theEnv)
  {
   DeftableData(theEnv)->DeftableModuleIndex =
      RegisterModuleItem(theEnv,"deftable",
                         AllocateModule,
                         InitModule,
                         ReturnModule,
#if BLOAD_AND_BSAVE || BLOAD || BLOAD_ONLY
                         BloadDeftableModuleReference,
#else
                         NULL,
#endif
#if CONSTRUCT_COMPILER && (! RUN_TIME)
                         DeftableCModuleReference,
#else
                         NULL,
#endif
                         (FindConstructFunction *) FindDeftableInModule);
  }

/************************************************/
/* AllocateModule: Allocates a deftable module. */
/************************************************/
static void *AllocateModule(
  Environment *theEnv)
  {
   return((void *) get_struct(theEnv,deftableModule));
  }

/**********************************************/
/* InitModule: Initializes a deftable module. */
/**********************************************/
static void InitModule(
  Environment *theEnv,
  void *theItem)
  {
   struct deftableModule *theModule = (struct deftableModule *) theItem;
   
   theModule->header.itemCount = 0;
   theModule->header.hashTableSize = 0;
   theModule->header.hashTable = NULL;
  }
  
/************************************************/
/* ReturnModule: Deallocates a deftable module. */
/************************************************/
static void ReturnModule(
  Environment *theEnv,
  void *theItem)
  {
   FreeConstructHeaderModule(theEnv,(struct defmoduleItemHeader *) theItem,DeftableData(theEnv)->DeftableConstruct);
   rtn_struct(theEnv,deftableModule,theItem);
  }

/*************************************************************/
/* GetDeftableModuleItem: Returns a pointer to the defmodule */
/*  item for the specified deftable or defmodule.            */
/*************************************************************/
struct deftableModule *GetDeftableModuleItem(
  Environment *theEnv,
  Defmodule *theModule)
  {
   return((struct deftableModule *) GetConstructModuleItemByIndex(theEnv,theModule,DeftableData(theEnv)->DeftableModuleIndex));
  }

/*************************************************/
/* FindDeftable: Searches for a deftable in the  */
/*   list of deftables. Returns a pointer to the */
/*   deftable if found, otherwise NULL.          */
/*************************************************/
Deftable *FindDeftable(
  Environment *theEnv,
  const char *deftableName)
  {
   return (Deftable *) FindNamedConstructInModuleOrImports(theEnv,deftableName,DeftableData(theEnv)->DeftableConstruct);
  }

/*************************************************/
/* FindDeftableInModule: Searches for a deftable */
/*   in the list of deftables. Returns a pointer */
/*   to the deftable if found, otherwise NULL.   */
/*************************************************/
Deftable *FindDeftableInModule(
  Environment *theEnv,
  const char *deftableName)
  {
   return (Deftable *) FindNamedConstructInModule(theEnv,deftableName,DeftableData(theEnv)->DeftableConstruct);
  }

/**********************************************************/
/* GetNextDeftable: If passed a NULL pointer, returns     */
/*   the first deftable in the ListOfDeftables. Otherwise */
/*   returns the next deftables following the deftable    */
/*   passed as an argument.                               */
/**********************************************************/
Deftable *GetNextDeftable(
  Environment *theEnv,
  Deftable *deftablePtr)
  {
   return (Deftable *) GetNextConstructItem(theEnv,&deftablePtr->header,DeftableData(theEnv)->DeftableModuleIndex);
  }

/*******************************************************/
/* DeftableIsDeletable: Returns true if a particular   */
/*   deftable can be deleted, otherwise returns false. */
/*******************************************************/
bool DeftableIsDeletable(
  Deftable *theDeftable)
  {
   Environment *theEnv = theDeftable->header.env;

   if (! ConstructsDeletable(theEnv))
     { return false; }

   return true;
  }

/***********************************************************/
/* ReturnDeftable: Returns the data structures associated  */
/*   with a deftable construct to the pool of free memory. */
/***********************************************************/
static void ReturnDeftable(
  Environment *theEnv,
  Deftable *theDeftable)
  {
#if (! BLOAD_ONLY) && (! RUN_TIME)
   if (theDeftable == NULL) return;

   RemoveConstructFromHashMap(theEnv,&theDeftable->header,theDeftable->header.whichModule);

   ExpressionDeinstall(theEnv,theDeftable->columns);
   ReturnPackedExpression(theEnv,theDeftable->columns);

   ExpressionDeinstall(theEnv,theDeftable->rows);
   ReturnPackedExpression(theEnv,theDeftable->rows);
   FreeRCHT(theEnv,theDeftable->columnHashTable,theDeftable->columnHashTableSize);
   FreeRCHT(theEnv,theDeftable->rowHashTable,theDeftable->rowHashTableSize);

   DeinstallConstructHeader(theEnv,&theDeftable->header);

   rtn_struct(theEnv,deftable,theDeftable);
#endif
  }

/***************************************/
/* LookupDeftable: Finds a deftable by */
/*   searching for it in the hashmap.  */
/***************************************/
static Deftable *LookupDeftable(
  Environment *theEnv,
  CLIPSLexeme *deftableName)
  {
   struct defmoduleItemHeaderHM *theModuleItem;
   size_t theHashValue;
   struct itemHashTableEntry *theItem;
   
   theModuleItem = (struct defmoduleItemHeaderHM *)
                   GetModuleItem(theEnv,NULL,DeftableData(theEnv)->DeftableModuleIndex);
                   
   if (theModuleItem->itemCount == 0)
     { return NULL; }

   theHashValue = HashSymbol(deftableName->contents,theModuleItem->hashTableSize);

   for (theItem = theModuleItem->hashTable[theHashValue];
        theItem != NULL;
        theItem = theItem->next)
     {
      if (theItem->item->name == deftableName)
        { return (Deftable *) theItem->item; }
     }

   return NULL;
  }

/****************************************************/
/* AllocateRCHT: Allocates a row/column hash table. */
/****************************************************/
struct rcHashTableEntry **AllocateRCHT(
  Environment *theEnv,
  size_t htSize)
  {
   size_t spaceNeeded;
   struct rcHashTableEntry **theHT;

   if (htSize == 0)
     { return NULL; }
     
   spaceNeeded = sizeof(struct rcHashTableEntry *) * htSize;
   theHT = (struct rcHashTableEntry **) gm2(theEnv,spaceNeeded);
   memset(theHT,0,spaceNeeded);
   
   return theHT;
  }

/******************/
/* AddEntryToRCHT */
/******************/
void AddEntryToRCHT(
  Environment *theEnv,
  struct rcHashTableEntry **theHT,
  size_t htSize,
  struct expr *item,
  unsigned short itemIndex)
  {
   struct rcHashTableEntry *rcEntry;
   size_t hv;

   hv = TableKeyHashFromExpression(theEnv,item);
   hv = hv % htSize;

   rcEntry = get_struct(theEnv,rcHashTableEntry);
   rcEntry->next = theHT[hv];
   rcEntry->item = item;
   rcEntry->itemIndex = itemIndex;
   theHT[hv] = rcEntry;
  }
  
/**********************************/
/* TableKeyHashHashFromExpression */
/**********************************/
size_t TableKeyHashFromExpression(
  Environment *theEnv,
  Expression *theKey)
  {
   size_t hv;

   if (theKey->type != QUANTITY_TYPE)
     { return ItemHashValue(theEnv,theKey->type,theKey->value,0); }
     
   hv = 0;
   
   theKey = theKey->nextArg;
   
   while (theKey != NULL)
     {
      hv += ItemHashValue(theEnv,theKey->type,theKey->value,0);
      theKey = theKey->nextArg;
     }
     
   return hv;
  }

/*****************************/
/* TableKeyHashHashTypeValue */
/*****************************/
size_t TableKeyHashTypeValue(
  Environment *theEnv,
  unsigned short type,
  void *value)
  {
   size_t hv, i;
   Multifield *theMultifield;

   if (type != MULTIFIELD_TYPE)
     { return ItemHashValue(theEnv,type,value,0); }
     
   hv = 0;
   
   theMultifield = (Multifield *) value;
   
   for (i = 0; i < theMultifield->length; i++)
     { hv += ItemHashValue(theEnv,theMultifield->contents[i].header->type,theMultifield->contents[i].value,0); }
     
   return hv;
  }

/***********************************************************/
/* TableKeysMatchEE: Determines if two keys for retrieving */
/*   column and row indices are the same.                  */
/***********************************************************/
bool TableKeysMatchEE(
  Expression *key1,
  Expression *key2)
  {
   /*==================================*/
   /* Compare the values in one key to */
   /* the other to see if they match.  */
   /*==================================*/
   
   while ((key1 != NULL) && (key2 != NULL))
     {
      if ((key1->type != key2->type) ||
          (key1->value != key2->value))
        { return false; }
        
      key1 = key1->nextArg;
      key2 = key2->nextArg;
     }
     
   /*====================================*/
   /* If the keys are equal, both should */
   /* have no values left to check.      */
   /*====================================*/
   
   if (key1 != key2)
     { return false; }
   else
     { return true; }
  }

/**********************************************************/
/* TableKeysMatchEV: Determines if two key for retrieving */
/*   column and row indices are the same.                 */
/**********************************************************/
bool TableKeysMatchTypeValue(
  Expression *key,
  unsigned short type,
  void *value)
  {
   Multifield *theMultifield;
   size_t i;
    
   /*===========================================*/
   /* Determine a match for a single-field key. */
   /*===========================================*/
   
   if (key->type != QUANTITY_TYPE)
     {
      if ((key->type == type) &&
          (key->value == value))
        { return true; }
      else
        { return false; }
     }

   /*=========================================*/
   /* Determine a match for a multifield key. */
   /*=========================================*/

   if (type != MULTIFIELD_TYPE)
     { return false; }

   theMultifield = (Multifield *) value;
   
   if (key->integerValue->contents != (long long) theMultifield->length)
     { return false; }
       
   i = 0;
   key = key->nextArg;
   while (key != NULL)
     {
      if ((key->type != theMultifield->contents[i].header->type) ||
          (key->value != theMultifield->contents[i].value))
        { return false; }
        
      key = key->nextArg;
      i++;
     }
     
   return true;
  }

/************************/
/* FindTableEntryForRow */
/************************/
struct rcHashTableEntry *FindTableEntryForRow(
  Environment *theEnv,
  Deftable *theDeftable,
  unsigned short type,
  void *value)
  {
   return FindTableEntryTypeValue(theEnv,theDeftable->rowHashTable,theDeftable->rowHashTableSize,type,value);
  }

/***************************/
/* FindTableEntryForColumn */
/***************************/
struct rcHashTableEntry *FindTableEntryForColumn(
  Environment *theEnv,
  Deftable *theDeftable,
  unsigned short type,
  void *value)
  {
   return FindTableEntryTypeValue(theEnv,theDeftable->columnHashTable,theDeftable->columnHashTableSize,type,value);
  }

/***************************/
/* FindTableEntryTypeValue */
/***************************/
struct rcHashTableEntry *FindTableEntryTypeValue(
  Environment *theEnv,
  struct rcHashTableEntry **hashTable,
  size_t hashTableSize,
  unsigned short type,
  void *value)
  {
   size_t hv;
   struct rcHashTableEntry *entry;

   hv = TableKeyHashTypeValue(theEnv,type,value);
   hv = hv % hashTableSize;
  
   entry = hashTable[hv];

   while (entry != NULL)
     {
      if (TableKeysMatchTypeValue(entry->item,type,value))
        { return entry; }

      entry = entry->next;
     }
     
   return NULL;
  }

/************/
/* PrintKey */
/************/
void PrintKey(
  Environment *theEnv,
  const char *logicalName,
  Expression *theKey)
  {
   if (theKey->type == QUANTITY_TYPE)
     {
      WriteString(theEnv,logicalName,"(");
      
      for (theKey = theKey->nextArg; theKey != NULL; theKey = theKey->nextArg)
        {
         PrintAtom(theEnv,logicalName,theKey->type,theKey->value);
         if (theKey->nextArg)
           { WriteString(theEnv,logicalName," "); }
        }
      
      WriteString(theEnv,logicalName,")");
     }
   else
     { PrintAtom(theEnv,logicalName,theKey->type,theKey->value); }
  }

/****************************************/
/* FreeRCHT: Frees the data structures  */
/*   used for a row/columnn hash table. */
/****************************************/
void FreeRCHT(
  Environment *theEnv,
  struct rcHashTableEntry **theHT,
  size_t htSize)
  {
   size_t spaceNeeded, i;
   struct rcHashTableEntry *rcEntry, *nextEntry;
   
   if (htSize == 0)
     { return; }
     
   spaceNeeded = sizeof(struct rcHashTableEntry *) * htSize;
   
   for (i = 0; i < htSize; i++)
     {
      rcEntry = theHT[i];
      while (rcEntry != NULL)
        {
         nextEntry = rcEntry->next;
         rtn_struct(theEnv,rcHashTableEntry,rcEntry);
         rcEntry = nextEntry;
        }
     }
     
   rm(theEnv,theHT,spaceNeeded);
  }

/******************/
/* DebugHashTable */
/******************/
void DebugHashTable(
  struct rcHashTableEntry **theHT,
  size_t htSize)
  {
   size_t i;
   struct rcHashTableEntry *theEntry;
   
   for (i = 0; i < htSize; i++)
     {
      theEntry = theHT[i];
      while (theEntry != NULL)
        { theEntry = theEntry->next; }
     }
  }

/**********************************************/
/* DecrementDeftableBusyCount: Decrements the */
/*   busy count of a deftable data structure. */
/**********************************************/
void DecrementDeftableBusyCount(
  Environment *theEnv,
  Deftable *theTable)
  {
   if (! ConstructData(theEnv)->ClearInProgress) theTable->busyCount--;
  }

/*************************************************/
/* IncrementDeftableBusyCount: Increments the */
/*   busy count of a deftemplate data structure. */
/*************************************************/
void IncrementDeftableBusyCount(
  Environment *theEnv,
  Deftable *theTable)
  {
#if MAC_XCD
#pragma unused(theEnv)
#endif

   theTable->busyCount++;
  }

/*##################################*/
/* Additional Environment Functions */
/*##################################*/

const char *DeftableModule(
  Deftable *theDeftable)
  {
   return GetConstructModuleName(&theDeftable->header);
  }

const char *DeftableName(
  Deftable *theDeftable)
  {
   return GetConstructNameString(&theDeftable->header);
  }

const char *DeftablePPForm(
  Deftable *theDeftable)
  {
   return GetConstructPPForm(&theDeftable->header);
  }

#endif /* DEFTABLE_CONSTRUCT */
