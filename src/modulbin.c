   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 7.00  01/23/24             */
   /*                                                     */
   /*             DEFMODULE BSAVE/BLOAD MODULE            */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements the binary save/load feature for the  */
/*    defmodule construct.                                   */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*      7.00: Construct hashing for quick lookup.            */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME)

#include <stdio.h>

#include "bload.h"
#include "bsave.h"
#include "constrct.h"
#include "cstrcbin.h"
#include "envrnmnt.h"
#include "memalloc.h"
#include "moduldef.h"

#include "modulbin.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if BLOAD_AND_BSAVE
   static void                    BsaveFind(Environment *);
   static void                    BsaveStorage(Environment *,FILE *);
   static void                    BsaveBinaryItem(Environment *,FILE *);
#endif
   static void                    BloadStorage(Environment *);
   static void                    BloadBinaryItem(Environment *);
   static void                    UpdateDefmodule(Environment *,void *,unsigned long);
   static void                    UpdatePortItem(Environment *,void *,unsigned long);
   static void                    ClearBload(Environment *);

/*********************************************/
/* DefmoduleBinarySetup: Installs the binary */
/*   save/load feature for defmodules.       */
/*********************************************/
void DefmoduleBinarySetup(
  Environment *theEnv)
  {
   AddBeforeBloadFunction(theEnv,"defmodule",RemoveAllDefmodules,2000,NULL);

#if BLOAD_AND_BSAVE
   AddBinaryItem(theEnv,"defmodule",0,BsaveFind,NULL,
                             BsaveStorage,BsaveBinaryItem,
                             BloadStorage,BloadBinaryItem,
                             ClearBload);
#endif

   AddAbortBloadFunction(theEnv,"defmodule",CreateMainModule,0,NULL);

#if (BLOAD || BLOAD_ONLY)
   AddBinaryItem(theEnv,"defmodule",0,NULL,NULL,NULL,NULL,
                             BloadStorage,BloadBinaryItem,
                             ClearBload);
#endif
  }

/**************************************************************/
/* UpdateDefmoduleItemHeader: Updates the values in defmodule */
/*   item headers for the loaded binary image.                */
/**************************************************************/
void UpdateDefmoduleItemHeader(
  Environment *theEnv,
  struct bsaveDefmoduleItemHeader *theBsaveHeader,
  struct defmoduleItemHeader *theHeader,
  size_t itemSize,
  void *itemArray)
  {
   size_t firstOffset, lastOffset;

   theHeader->theModule = ModulePointer(theBsaveHeader->theModule);
   if (theBsaveHeader->firstItem == ULONG_MAX)
     {
      theHeader->firstItem = NULL;
      theHeader->lastItem = NULL;
     }
   else
     {
      firstOffset = itemSize * theBsaveHeader->firstItem;
      lastOffset = itemSize * theBsaveHeader->lastItem;
      theHeader->firstItem =
        (ConstructHeader *) &((char *) itemArray)[firstOffset];
      theHeader->lastItem =
        (ConstructHeader *) &((char *) itemArray)[lastOffset];
     }
  }

/****************************************************************/
/* UpdateDefmoduleItemHeaderHM: Updates the values in defmodule */
/*   item headers for the loaded binary image.                  */
/****************************************************************/
void UpdateDefmoduleItemHeaderHM(
  Environment *theEnv,
  struct bsaveDefmoduleItemHeader *theBsaveHeader,
  struct defmoduleItemHeaderHM *theHeader,
  size_t itemSize,
  void *itemArray)
  {
   size_t firstOffset, lastOffset;

   theHeader->theModule = ModulePointer(theBsaveHeader->theModule);
   if (theBsaveHeader->firstItem == ULONG_MAX)
     {
      theHeader->firstItem = NULL;
      theHeader->lastItem = NULL;
     }
   else
     {
      firstOffset = itemSize * theBsaveHeader->firstItem;
      lastOffset = itemSize * theBsaveHeader->lastItem;
      theHeader->firstItem =
        (ConstructHeader *) &((char *) itemArray)[firstOffset];
      theHeader->lastItem =
        (ConstructHeader *) &((char *) itemArray)[lastOffset];
     }
  }

#if BLOAD_AND_BSAVE

/*********************************************************/
/* AssignBsaveDefmdlItemHdrVals: Assigns the appropriate */
/*   values to a bsave defmodule item header record.     */
/*********************************************************/
void AssignBsaveDefmdlItemHdrVals(
  struct bsaveDefmoduleItemHeader *theBsaveHeader,
  struct defmoduleItemHeader *theHeader)
  {
   theBsaveHeader->theModule = theHeader->theModule->header.bsaveID;
   if (theHeader->firstItem == NULL)
     {
      theBsaveHeader->firstItem = ULONG_MAX;
      theBsaveHeader->lastItem = ULONG_MAX;
     }
   else
     {
      theBsaveHeader->firstItem = theHeader->firstItem->bsaveID;
      theBsaveHeader->lastItem = theHeader->lastItem->bsaveID;
     }
  }

/***********************************************************/
/* AssignBsaveDefmdlItemHdrHMVals: Assigns the appropriate */
/*   values to a bsave defmodule item header record.       */
/***********************************************************/
void AssignBsaveDefmdlItemHdrHMVals(
  struct bsaveDefmoduleItemHeader *theBsaveHeader,
  struct defmoduleItemHeaderHM *theHeader)
  {
   theBsaveHeader->theModule = theHeader->theModule->header.bsaveID;
   
   theBsaveHeader->itemCount = theHeader->itemCount;
   if (theBsaveHeader->itemCount > 3)
     {
      theBsaveHeader->itemCount = theBsaveHeader->itemCount * 5 / 4;
      while (! IsPrime(theBsaveHeader->itemCount))
        { theBsaveHeader->itemCount++; }
     }

   if (theHeader->firstItem == NULL)
     {
      theBsaveHeader->firstItem = ULONG_MAX;
      theBsaveHeader->lastItem = ULONG_MAX;
     }
   else
     {
      theBsaveHeader->firstItem = theHeader->firstItem->bsaveID;
      theBsaveHeader->lastItem = theHeader->lastItem->bsaveID;
     }
  }

/**********************************************************/
/* BsaveFind: Counts the number of data structures which  */
/*   must be saved in the binary image for the defmodules */
/*   in the current environment.                          */
/**********************************************************/
static void BsaveFind(
  Environment *theEnv)
  {
   Defmodule *defmodulePtr;
   struct portItem *theList;

   /*=======================================================*/
   /* If a binary image is already loaded, then temporarily */
   /* save the count values since these will be overwritten */
   /* in the process of saving the binary image.            */
   /*=======================================================*/

   SaveBloadCount(theEnv,DefmoduleData(theEnv)->BNumberOfDefmodules);
   SaveBloadCount(theEnv,DefmoduleData(theEnv)->NumberOfPortItems);

   /*==========================================*/
   /* Set the count of defmodule and defmodule */
   /* port items data structures to zero.      */
   /*==========================================*/

   DefmoduleData(theEnv)->BNumberOfDefmodules = 0;
   DefmoduleData(theEnv)->NumberOfPortItems = 0;

   /*===========================*/
   /* Loop through each module. */
   /*===========================*/

   for (defmodulePtr = GetNextDefmodule(theEnv,NULL);
        defmodulePtr != NULL;
        defmodulePtr = GetNextDefmodule(theEnv,defmodulePtr))
     {
      /*==============================================*/
      /* Increment the number of modules encountered. */
      /*==============================================*/

      DefmoduleData(theEnv)->BNumberOfDefmodules++;

      /*===========================*/
      /* Mark the defmodule's name */
      /* as being a needed symbol. */
      /*===========================*/

      MarkConstructHeaderNeededItems(&defmodulePtr->header,defmodulePtr->header.bsaveID);
         
      /*==============================================*/
      /* Loop through each of the port items in the   */
      /* defmodule's import list incrementing the     */
      /* number of port items encountered and marking */
      /* needed symbols.                              */
      /*==============================================*/

      for (theList = defmodulePtr->importList;
           theList != NULL;
           theList = theList->next)
        {
         DefmoduleData(theEnv)->NumberOfPortItems++;
         if (theList->moduleName != NULL)
           { theList->moduleName->neededSymbol = true; }
         if (theList->constructType != NULL)
           { theList->constructType->neededSymbol = true; }
         if (theList->constructName != NULL)
           { theList->constructName->neededSymbol = true; }
        }

      /*==============================================*/
      /* Loop through each of the port items in the   */
      /* defmodule's export list incrementing the     */
      /* number of port items encountered and marking */
      /* needed symbols.                              */
      /*==============================================*/

      for (theList = defmodulePtr->exportList;
           theList != NULL;
           theList = theList->next)
        {
         DefmoduleData(theEnv)->NumberOfPortItems++;
         if (theList->moduleName != NULL)
           { theList->moduleName->neededSymbol = true; }
         if (theList->constructType != NULL)
           { theList->constructType->neededSymbol = true; }
         if (theList->constructName != NULL)
           { theList->constructName->neededSymbol = true; }
        }
     }
  }

/*********************************************************/
/* BsaveStorage: Writes out the storage requirements for */
/*    all defmodule structures to the binary file.       */
/*********************************************************/
static void BsaveStorage(
  Environment *theEnv,
  FILE *fp)
  {
   size_t space;

   space = sizeof(long) * 2;
   GenWrite(&space,sizeof(size_t),fp);
   GenWrite(&DefmoduleData(theEnv)->BNumberOfDefmodules,sizeof(long),fp);
   GenWrite(&DefmoduleData(theEnv)->NumberOfPortItems,sizeof(long),fp);
  }

/*********************************************/
/* BsaveBinaryItem: Writes out all defmodule */
/*   structures to the binary file.          */
/*********************************************/
static void BsaveBinaryItem(
  Environment *theEnv,
  FILE *fp)
  {
   size_t space;
   Defmodule *defmodulePtr;
   struct bsaveDefmodule newDefmodule;
   struct bsavePortItem newPortItem;
   struct portItem *theList;

   /*=========================================================*/
   /* Write out the amount of space taken up by the defmodule */
   /* and port item data structures in the binary image.      */
   /*=========================================================*/

   space = DefmoduleData(theEnv)->BNumberOfDefmodules * sizeof(struct bsaveDefmodule);
   space += DefmoduleData(theEnv)->NumberOfPortItems * sizeof(struct bsavePortItem);
   GenWrite(&space,sizeof(size_t),fp);

   /*==========================================*/
   /* Write out each defmodule data structure. */
   /*==========================================*/

   DefmoduleData(theEnv)->BNumberOfDefmodules = 0;
   DefmoduleData(theEnv)->NumberOfPortItems = 0;
   for (defmodulePtr = GetNextDefmodule(theEnv,NULL);
        defmodulePtr != NULL;
        defmodulePtr = GetNextDefmodule(theEnv,defmodulePtr))
     {
      AssignBsaveConstructHeaderVals(&newDefmodule.header,&defmodulePtr->header);

      DefmoduleData(theEnv)->BNumberOfDefmodules++;
      
      if (defmodulePtr->importList == NULL)
        { newDefmodule.importList = ULONG_MAX; }
      else
        {
         newDefmodule.importList = DefmoduleData(theEnv)->NumberOfPortItems;
         for (theList = defmodulePtr->importList;
              theList != NULL;
              theList = theList->next)
           { DefmoduleData(theEnv)->NumberOfPortItems++; }
        }

      if (defmodulePtr->exportList == NULL)
        { newDefmodule.exportList = ULONG_MAX; }
      else
        {
         newDefmodule.exportList = DefmoduleData(theEnv)->NumberOfPortItems;
         for (theList = defmodulePtr->exportList;
              theList != NULL;
              theList = theList->next)
           { DefmoduleData(theEnv)->NumberOfPortItems++; }
        }

      newDefmodule.bsaveID = defmodulePtr->header.bsaveID;
      GenWrite(&newDefmodule,sizeof(struct bsaveDefmodule),fp);
     }

   /*==========================================*/
   /* Write out each port item data structure. */
   /*==========================================*/

   DefmoduleData(theEnv)->NumberOfPortItems = 0;
   defmodulePtr = GetNextDefmodule(theEnv,NULL);
   while (defmodulePtr != NULL)
     {
      for (theList = defmodulePtr->importList;
           theList != NULL;
           theList = theList->next)
        {
         DefmoduleData(theEnv)->NumberOfPortItems++;
         if (theList->moduleName == NULL) newPortItem.moduleName = ULONG_MAX;
         else newPortItem.moduleName = theList->moduleName->bucket;

         if (theList->constructType == NULL) newPortItem.constructType = ULONG_MAX;
         else newPortItem.constructType = theList->constructType->bucket;

         if (theList->constructName == NULL) newPortItem.constructName = ULONG_MAX;
         else newPortItem.constructName = theList->constructName->bucket;

         if (theList->next == NULL) newPortItem.next = ULONG_MAX;
         else newPortItem.next = DefmoduleData(theEnv)->NumberOfPortItems;

         GenWrite(&newPortItem,sizeof(struct bsavePortItem),fp);
        }

      for (theList = defmodulePtr->exportList;
           theList != NULL;
           theList = theList->next)
        {
         DefmoduleData(theEnv)->NumberOfPortItems++;
         if (theList->moduleName == NULL) newPortItem.moduleName = ULONG_MAX;
         else newPortItem.moduleName = theList->moduleName->bucket;

         if (theList->constructType == NULL) newPortItem.constructType = ULONG_MAX;
         else newPortItem.constructType = theList->constructType->bucket;

         if (theList->constructName == NULL) newPortItem.constructName = ULONG_MAX;
         else newPortItem.constructName = theList->constructName->bucket;

         if (theList->next == NULL) newPortItem.next = ULONG_MAX;
         else newPortItem.next = DefmoduleData(theEnv)->NumberOfPortItems;

         GenWrite(&newPortItem,sizeof(struct bsavePortItem),fp);
        }

      defmodulePtr = GetNextDefmodule(theEnv,defmodulePtr);
     }

   /*=============================================================*/
   /* If a binary image was already loaded when the bsave command */
   /* was issued, then restore the counts indicating the number   */
   /* of defmodule and port items in the binary image (these were */
   /* overwritten by the binary save).                            */
   /*=============================================================*/

   RestoreBloadCount(theEnv,&DefmoduleData(theEnv)->BNumberOfDefmodules);
   RestoreBloadCount(theEnv,&DefmoduleData(theEnv)->NumberOfPortItems);
  }

#endif /* BLOAD_AND_BSAVE */

/**********************************************************/
/* BloadStorage: Allocates storage requirements for the   */
/*   defmodules and port items used by this binary image. */
/**********************************************************/
static void BloadStorage(
  Environment *theEnv)
  {
   size_t space;

   /*=======================================*/
   /* Determine the number of defmodule and */
   /* port item data structures to be read. */
   /*=======================================*/

   GenReadBinary(theEnv,&space,sizeof(size_t));
   GenReadBinary(theEnv,&DefmoduleData(theEnv)->BNumberOfDefmodules,sizeof(unsigned long));
   GenReadBinary(theEnv,&DefmoduleData(theEnv)->NumberOfPortItems,sizeof(unsigned long));

   /*================================*/
   /* Allocate the space needed for  */
   /* the defmodule data structures. */
   /*================================*/

   if (DefmoduleData(theEnv)->BNumberOfDefmodules == 0)
     {
      DefmoduleData(theEnv)->DefmoduleArray = NULL;
      return;
     }

   space = (DefmoduleData(theEnv)->BNumberOfDefmodules * sizeof(Defmodule));
   DefmoduleData(theEnv)->DefmoduleArray = (Defmodule *) genalloc(theEnv,space);

   /*================================*/
   /* Allocate the space needed for  */
   /* the port item data structures. */
   /*================================*/

   if (DefmoduleData(theEnv)->NumberOfPortItems == 0)
     {
      DefmoduleData(theEnv)->PortItemArray = NULL;
      return;
     }

   space = (DefmoduleData(theEnv)->NumberOfPortItems * sizeof(struct portItem));
   DefmoduleData(theEnv)->PortItemArray = (struct portItem *) genalloc(theEnv,space);
  }

/********************************************/
/* BloadBinaryItem: Loads and refreshes the */
/*   defmodules used by this binary image.  */
/********************************************/
static void BloadBinaryItem(
  Environment *theEnv)
  {
   size_t space;

   GenReadBinary(theEnv,&space,sizeof(size_t));
   if (DefmoduleData(theEnv)->BNumberOfDefmodules == 0) return;

   BloadandRefresh(theEnv,DefmoduleData(theEnv)->BNumberOfDefmodules,sizeof(struct bsaveDefmodule),UpdateDefmodule);
   BloadandRefresh(theEnv,DefmoduleData(theEnv)->NumberOfPortItems,sizeof(struct bsavePortItem),UpdatePortItem);

   SetListOfDefmodules(theEnv,DefmoduleData(theEnv)->DefmoduleArray);
   SetCurrentModule(theEnv,GetNextDefmodule(theEnv,NULL));
  }

/******************************************/
/* UpdateDefmodule: Bload refresh routine */
/*   for defmodule data structure.        */
/******************************************/
static void UpdateDefmodule(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   struct bsaveDefmodule *bdp;
   struct moduleItem *theItem;
   unsigned int i;

   bdp = (struct bsaveDefmodule *) buf;
   
   UpdateConstructHeader(theEnv,&bdp->header,&DefmoduleData(theEnv)->DefmoduleArray[obji].header,DEFMODULE,
                         0,NULL,sizeof(Defmodule),DefmoduleData(theEnv)->DefmoduleArray);

   if (GetNumberOfModuleItems(theEnv) == 0)
     { DefmoduleData(theEnv)->DefmoduleArray[obji].itemsArray = NULL; }
   else
     {
      DefmoduleData(theEnv)->DefmoduleArray[obji].itemsArray =
         (struct defmoduleItemHeader **) gm2(theEnv,sizeof(void *) * GetNumberOfModuleItems(theEnv));
     }

   for (i = 0, theItem = GetListOfModuleItems(theEnv);
        (i < GetNumberOfModuleItems(theEnv)) && (theItem != NULL) ;
        i++, theItem = theItem->next)
     {
      if (theItem->bloadModuleReference == NULL)
        { DefmoduleData(theEnv)->DefmoduleArray[obji].itemsArray[i] = NULL; }
      else
        {
         DefmoduleData(theEnv)->DefmoduleArray[obji].itemsArray[i] =
             (struct defmoduleItemHeader *)
             (*theItem->bloadModuleReference)(theEnv,obji);
        }
     }

   DefmoduleData(theEnv)->DefmoduleArray[obji].header.ppForm = NULL;

   if (bdp->importList != ULONG_MAX)
     { DefmoduleData(theEnv)->DefmoduleArray[obji].importList = (struct portItem *) &DefmoduleData(theEnv)->PortItemArray[bdp->importList]; }
   else
     { DefmoduleData(theEnv)->DefmoduleArray[obji].importList = NULL; }

   if (bdp->exportList != ULONG_MAX)
     { DefmoduleData(theEnv)->DefmoduleArray[obji].exportList = (struct portItem *) &DefmoduleData(theEnv)->PortItemArray[bdp->exportList]; }
   else
     { DefmoduleData(theEnv)->DefmoduleArray[obji].exportList = NULL; }
   DefmoduleData(theEnv)->DefmoduleArray[obji].header.bsaveID = bdp->bsaveID;
  }

/*****************************************/
/* UpdatePortItem: Bload refresh routine */
/*   for port item data structure.       */
/*****************************************/
static void UpdatePortItem(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   struct bsavePortItem *bdp;

   bdp = (struct bsavePortItem *) buf;

   if (bdp->moduleName != ULONG_MAX)
     {
      DefmoduleData(theEnv)->PortItemArray[obji].moduleName = SymbolPointer(bdp->moduleName);
      IncrementLexemeCount(DefmoduleData(theEnv)->PortItemArray[obji].moduleName);
     }
   else
     { DefmoduleData(theEnv)->PortItemArray[obji].moduleName = NULL; }

   if (bdp->constructType != ULONG_MAX)
     {
      DefmoduleData(theEnv)->PortItemArray[obji].constructType = SymbolPointer(bdp->constructType);
      IncrementLexemeCount(DefmoduleData(theEnv)->PortItemArray[obji].constructType);
     }
   else
     { DefmoduleData(theEnv)->PortItemArray[obji].constructType = NULL; }

   if (bdp->constructName != ULONG_MAX)
     {
      DefmoduleData(theEnv)->PortItemArray[obji].constructName = SymbolPointer(bdp->constructName);
      IncrementLexemeCount(DefmoduleData(theEnv)->PortItemArray[obji].constructName);
     }
   else
     { DefmoduleData(theEnv)->PortItemArray[obji].constructName = NULL; }

   if (bdp->next != ULONG_MAX)
     { DefmoduleData(theEnv)->PortItemArray[obji].next = (struct portItem *) &DefmoduleData(theEnv)->PortItemArray[bdp->next]; }
   else
     { DefmoduleData(theEnv)->PortItemArray[obji].next = NULL; }
  }

/***************************************/
/* ClearBload: Defmodule clear routine */
/*   when a binary load is in effect.  */
/***************************************/
static void ClearBload(
  Environment *theEnv)
  {
   unsigned long i;
   size_t space;
   struct portItem *theList;

   /*===========================*/
   /* Decrement in use counters */
   /* used by the binary image. */
   /*===========================*/

   for (i = 0; i < DefmoduleData(theEnv)->BNumberOfDefmodules; i++)
     {
      ReleaseLexeme(theEnv,DefmoduleData(theEnv)->DefmoduleArray[i].header.name);
      for (theList = DefmoduleData(theEnv)->DefmoduleArray[i].importList;
           theList != NULL;
           theList = theList->next)
        {
         if (theList->moduleName != NULL) ReleaseLexeme(theEnv,theList->moduleName);
         if (theList->constructType != NULL) ReleaseLexeme(theEnv,theList->constructType);
         if (theList->constructName != NULL) ReleaseLexeme(theEnv,theList->constructName);
        }

      for (theList = DefmoduleData(theEnv)->DefmoduleArray[i].exportList;
           theList != NULL;
           theList = theList->next)
        {
         if (theList->moduleName != NULL) ReleaseLexeme(theEnv,theList->moduleName);
         if (theList->constructType != NULL) ReleaseLexeme(theEnv,theList->constructType);
         if (theList->constructName != NULL) ReleaseLexeme(theEnv,theList->constructName);
        }

      rm(theEnv,DefmoduleData(theEnv)->DefmoduleArray[i].itemsArray,sizeof(void *) * GetNumberOfModuleItems(theEnv));
     }

   /*===========================*/
   /* Deallocate the hash maps. */
   /*===========================*/

   for (i = 0; i < DefmoduleData(theEnv)->BNumberOfDefmodules; i++)
     { ClearDefmoduleHashMap(theEnv,&DefmoduleData(theEnv)->hashMap); }

   /*================================*/
   /* Deallocate the space used for  */
   /* the defmodule data structures. */
   /*================================*/

   space = DefmoduleData(theEnv)->BNumberOfDefmodules * sizeof(Defmodule);
   if (space != 0) genfree(theEnv,DefmoduleData(theEnv)->DefmoduleArray,space);
   DefmoduleData(theEnv)->BNumberOfDefmodules = 0;

   /*================================*/
   /* Deallocate the space used for  */
   /* the port item data structures. */
   /*================================*/

   space = DefmoduleData(theEnv)->NumberOfPortItems * sizeof(struct portItem);
   if (space != 0) genfree(theEnv,DefmoduleData(theEnv)->PortItemArray,space);
   DefmoduleData(theEnv)->NumberOfPortItems = 0;

   /*===========================*/
   /* Reset module information. */
   /*===========================*/

   SetListOfDefmodules(theEnv,NULL);
   CreateMainModule(theEnv,NULL);
   DefmoduleData(theEnv)->MainModuleRedefinable = true;
  }

#endif /*  (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME) */


