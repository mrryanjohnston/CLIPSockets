   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 7.00  02/06/24             */
   /*                                                     */
   /*             DEFTABLE BSAVE/BLOAD MODULE             */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements the binary save/load feature for the  */
/*    deffacts construct.                                    */
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

#if DEFTABLE_CONSTRUCT && (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME)

#include <stdio.h>

#include "bload.h"
#include "bsave.h"
#include "envrnmnt.h"
#include "memalloc.h"
#include "moduldef.h"
#include "tabledef.h"

#include "tablebin.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if BLOAD_AND_BSAVE
   static void                    BsaveFind(Environment *);
   static void                    BsaveExpressions(Environment *,FILE *);
   static void                    BsaveStorage(Environment *,FILE *);
   static void                    BsaveBinaryItem(Environment *,FILE *);
#endif
   static void                    BloadStorage(Environment *);
   static void                    BloadBinaryItem(Environment *);
   static void                    UpdateDeftableModule(Environment *,void *,unsigned long);
   static void                    UpdateDeftable(Environment *,void *,unsigned long);
   static void                    ClearBload(Environment *);
   static void                    DeallocateDeftableBloadData(Environment *);

/********************************************/
/* DeftableBinarySetup: Installs the binary */
/*   save/load feature for deftable.        */
/********************************************/
void DeftableBinarySetup(
  Environment *theEnv)
  {
   AllocateEnvironmentData(theEnv,DFTBLBIN_DATA,sizeof(struct deftableBinaryData),DeallocateDeftableBloadData);
#if BLOAD_AND_BSAVE
   AddBinaryItem(theEnv,"deftable",0,BsaveFind,BsaveExpressions,
                             BsaveStorage,BsaveBinaryItem,
                             BloadStorage,BloadBinaryItem,
                             ClearBload);
#endif

#if (BLOAD || BLOAD_ONLY)
   AddBinaryItem(theEnv,"deftable",0,NULL,NULL,NULL,NULL,
                             BloadStorage,BloadBinaryItem,
                             ClearBload);
#endif
  }

/********************************************************/
/* DeallocateDeftableBloadData: Deallocates environment */
/*    data for the deftable bsave functionality.        */
/********************************************************/
static void DeallocateDeftableBloadData(
  Environment *theEnv)
  {
   size_t space;

   space = DeftableBinaryData(theEnv)->NumberOfDeftables * sizeof(Deftable);
   if (space != 0) genfree(theEnv,DeftableBinaryData(theEnv)->DeftableArray,space);

   space = DeftableBinaryData(theEnv)->NumberOfDeftableModules * sizeof(struct deftableModule);
   if (space != 0) genfree(theEnv,DeftableBinaryData(theEnv)->ModuleArray,space);
  }

#if BLOAD_AND_BSAVE

/*********************************************************/
/* BsaveFind: Counts the number of data structures which */
/*   must be saved in the binary image for the deftables */
/*   in the current environment.                         */
/*********************************************************/
static void BsaveFind(
  Environment *theEnv)
  {
   Deftable *theDeftable;
   Defmodule *theModule;

   /*=======================================================*/
   /* If a binary image is already loaded, then temporarily */
   /* save the count values since these will be overwritten */
   /* in the process of saving the binary image.            */
   /*=======================================================*/

   SaveBloadCount(theEnv,DeftableBinaryData(theEnv)->NumberOfDeftableModules);
   SaveBloadCount(theEnv,DeftableBinaryData(theEnv)->NumberOfDeftables);

   /*=========================================*/
   /* Set the count of deftables and deftable */
   /* module data structures to zero.         */
   /*=========================================*/

   DeftableBinaryData(theEnv)->NumberOfDeftables = 0;
   DeftableBinaryData(theEnv)->NumberOfDeftableModules = 0;

   /*===========================*/
   /* Loop through each module. */
   /*===========================*/

   for (theModule = GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = GetNextDefmodule(theEnv,theModule))
     {
      /*===============================================*/
      /* Set the current module to the module being    */
      /* examined and increment the number of deftable */
      /* modules encountered.                          */
      /*===============================================*/

      SetCurrentModule(theEnv,theModule);
      DeftableBinaryData(theEnv)->NumberOfDeftableModules++;

      /*===================================================*/
      /* Loop through each deftable in the current module. */
      /*===================================================*/

      for (theDeftable = GetNextDeftable(theEnv,NULL);
           theDeftable != NULL;
           theDeftable = GetNextDeftable(theEnv,theDeftable))
        {
         /*======================================================*/
         /* Initialize the construct header for the binary save. */
         /*======================================================*/

         MarkConstructHeaderNeededItems(&theDeftable->header,DeftableBinaryData(theEnv)->NumberOfDeftables++);

         /*==================================================*/
         /* Count the number of expressions contained in the */
         /* columns and rows of the deftable and mark any    */
         /* atomic values contained there as in use.         */
         /*==================================================*/

         ExpressionData(theEnv)->ExpressionCount += ExpressionSize(theDeftable->columns);
         MarkNeededItems(theEnv,theDeftable->columns);

         ExpressionData(theEnv)->ExpressionCount += ExpressionSize(theDeftable->rows);
         MarkNeededItems(theEnv,theDeftable->rows);
        }
     }
  }

/************************************************/
/* BsaveExpressions: Saves the expressions used */
/*   by deffacts to the binary save file.       */
/************************************************/
static void BsaveExpressions(
  Environment *theEnv,
  FILE *fp)
  {
   Deftable *theDeftable;
   Defmodule *theModule;

   /*===========================*/
   /* Loop through each module. */
   /*===========================*/

   for (theModule = GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = GetNextDefmodule(theEnv,theModule))
     {
      /*======================================================*/
      /* Set the current module to the module being examined. */
      /*======================================================*/

      SetCurrentModule(theEnv,theModule);

      /*==================================================*/
      /* Loop through each deftable in the current module */
      /* and save the column and row expressions.         */
      /*==================================================*/

      for (theDeftable = GetNextDeftable(theEnv,NULL);
           theDeftable != NULL;
           theDeftable = GetNextDeftable(theEnv,theDeftable))
        {
         BsaveExpression(theEnv,theDeftable->columns,fp);
         BsaveExpression(theEnv,theDeftable->rows,fp);
        }
     }
  }

/******************************************************/
/* BsaveStorage: Writes out the storage requirements  */
/*    for all deftable structures to the binary file. */
/******************************************************/
static void BsaveStorage(
  Environment *theEnv,
  FILE *fp)
  {
   size_t space;

   /*=================================================================*/
   /* Only two data structures are saved as part of a deftable binary */
   /* image: the deftable data structure and the deftableModule data  */
   /* structure. The assertion list expressions are not save with the */
   /* deffacts portion of the binary image.                           */
   /*=================================================================*/

   space = sizeof(long) * 2;
   GenWrite(&space,sizeof(size_t),fp);
   GenWrite(&DeftableBinaryData(theEnv)->NumberOfDeftables,sizeof(long),fp);
   GenWrite(&DeftableBinaryData(theEnv)->NumberOfDeftableModules,sizeof(long),fp);
  }

/********************************************/
/* BsaveBinaryItem: Writes out all deffacts */
/*   structures to the binary file.         */
/********************************************/
static void BsaveBinaryItem(
  Environment *theEnv,
  FILE *fp)
  {
   size_t space;
   Deftable *theDeftable;
   struct bsaveDeftable newDeftable;
   Defmodule *theModule;
   struct bsaveDeftableModule tempDeftableModule;
   struct deftableModule *theModuleItem;

   /*=========================================================*/
   /* Write out the amount of space taken up by the deftable  */
   /* and deftableModule data structures in the binary image. */
   /*=========================================================*/

   space = DeftableBinaryData(theEnv)->NumberOfDeftables * sizeof(struct bsaveDeftable) +
           (DeftableBinaryData(theEnv)->NumberOfDeftableModules * sizeof(struct bsaveDeftableModule));
   GenWrite(&space,sizeof(size_t),fp);

   /*================================================*/
   /* Write out each deftable module data structure. */
   /*================================================*/

   DeftableBinaryData(theEnv)->NumberOfDeftables = 0;
   for (theModule = GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = GetNextDefmodule(theEnv,theModule))
     {
      SetCurrentModule(theEnv,theModule);

      theModuleItem = (struct deftableModule *) GetModuleItem(theEnv,NULL,DeftableData(theEnv)->DeftableModuleIndex);
      AssignBsaveDefmdlItemHdrHMVals(&tempDeftableModule.header,&theModuleItem->header);
      GenWrite(&tempDeftableModule,sizeof(struct bsaveDeftableModule),fp);
     }

   /*==========================*/
   /* Write out each deftable. */
   /*==========================*/

   for (theModule = GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = GetNextDefmodule(theEnv,theModule))
     {
      SetCurrentModule(theEnv,theModule);

      for (theDeftable = GetNextDeftable(theEnv,NULL);
           theDeftable != NULL;
           theDeftable = GetNextDeftable(theEnv,theDeftable))
        {
         AssignBsaveConstructHeaderVals(&newDeftable.header,&theDeftable->header);

         if (theDeftable->columns != NULL)
           {
            newDeftable.columns = ExpressionData(theEnv)->ExpressionCount;
            ExpressionData(theEnv)->ExpressionCount += ExpressionSize(theDeftable->columns);
           }
         else
           { newDeftable.columns = ULONG_MAX; }

         if (theDeftable->rows != NULL)
           {
            newDeftable.rows = ExpressionData(theEnv)->ExpressionCount;
            ExpressionData(theEnv)->ExpressionCount += ExpressionSize(theDeftable->rows);
           }
         else
           { newDeftable.rows = ULONG_MAX; }

         newDeftable.columnCount = theDeftable->columnCount;
         newDeftable.rowCount = theDeftable->rowCount;
         newDeftable.columnHashTableSize = theDeftable->columnHashTableSize;
         newDeftable.rowHashTableSize = theDeftable->rowHashTableSize;
         
         GenWrite(&newDeftable,sizeof(struct bsaveDeftable),fp);
        }
     }

   /*==============================================================*/
   /* If a binary image was already loaded when the bsave command  */
   /* was issued, then restore the counts indicating the number    */
   /* of deftables and deftable modules in the binary image (these */
   /* were overwritten by the binary save).                        */
   /*==============================================================*/

   RestoreBloadCount(theEnv,&DeftableBinaryData(theEnv)->NumberOfDeftableModules);
   RestoreBloadCount(theEnv,&DeftableBinaryData(theEnv)->NumberOfDeftables);
  }

#endif /* BLOAD_AND_BSAVE */

/****************************************************/
/* BloadStorage: Allocates storage requirements for */
/*   the deffacts used by this binary image.        */
/****************************************************/
static void BloadStorage(
  Environment *theEnv)
  {
   size_t space;

   /*=====================================================*/
   /* Determine the number of deftable and deftableModule */
   /* data structures to be read.                         */
   /*=====================================================*/

   GenReadBinary(theEnv,&space,sizeof(size_t));
   GenReadBinary(theEnv,&DeftableBinaryData(theEnv)->NumberOfDeftables,sizeof(long));
   GenReadBinary(theEnv,&DeftableBinaryData(theEnv)->NumberOfDeftableModules,sizeof(long));

   /*===================================*/
   /* Allocate the space needed for the */
   /* deftableModule data structures.   */
   /*===================================*/

   if (DeftableBinaryData(theEnv)->NumberOfDeftableModules == 0)
     {
      DeftableBinaryData(theEnv)->DeftableArray = NULL;
      DeftableBinaryData(theEnv)->ModuleArray = NULL;
      return;
     }

   space = DeftableBinaryData(theEnv)->NumberOfDeftableModules * sizeof(struct deftableModule);
   DeftableBinaryData(theEnv)->ModuleArray = (struct deftableModule *) genalloc(theEnv,space);

   /*===============================*/
   /* Allocate the space needed for */
   /* the deftable data structures. */
   /*===============================*/

   if (DeftableBinaryData(theEnv)->NumberOfDeftables == 0)
     {
      DeftableBinaryData(theEnv)->DeftableArray = NULL;
      return;
     }

   space = (DeftableBinaryData(theEnv)->NumberOfDeftables * sizeof(Deftable));
   DeftableBinaryData(theEnv)->DeftableArray = (Deftable *) genalloc(theEnv,space);
  }

/*****************************************************/
/* BloadBinaryItem: Loads and refreshes the deftable */
/*   constructs used by this binary image.           */
/*****************************************************/
static void BloadBinaryItem(
  Environment *theEnv)
  {
   size_t space;

   /*======================================================*/
   /* Read in the amount of space used by the binary image */
   /* (this is used to skip the construct in the event it  */
   /* is not available in the version being run).          */
   /*======================================================*/

   GenReadBinary(theEnv,&space,sizeof(size_t));

   /*============================================*/
   /* Read in the deftableModule data structures */
   /* and refresh the pointers.                  */
   /*============================================*/

   BloadandRefresh(theEnv,DeftableBinaryData(theEnv)->NumberOfDeftableModules,
                   sizeof(struct bsaveDeftableModule),UpdateDeftableModule);

   /*======================================*/
   /* Read in the deftable data structures */
   /* and refresh the pointers.            */
   /*======================================*/

   BloadandRefresh(theEnv,DeftableBinaryData(theEnv)->NumberOfDeftables,
                   sizeof(struct bsaveDeftable),UpdateDeftable);
  }

/***************************************************/
/* UpdateDeftableModule: Bload refresh routine for */
/*   deftable module data structures.              */
/***************************************************/
static void UpdateDeftableModule(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   struct bsaveDeftableModule *bdmPtr;

   bdmPtr = (struct bsaveDeftableModule *) buf;
   UpdateDefmoduleItemHeaderHM(theEnv,&bdmPtr->header,&DeftableBinaryData(theEnv)->ModuleArray[obji].header,
                               sizeof(Deftable),DeftableBinaryData(theEnv)->DeftableArray);
                               
   AssignHashMapSize(theEnv,&DeftableBinaryData(theEnv)->ModuleArray[obji].header,bdmPtr->header.itemCount);
  }

/*****************************************/
/* UpdateDeftable: Bload refresh routine */
/*   for deftable data structures.       */
/*****************************************/
static void UpdateDeftable(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   struct bsaveDeftable *bdp;
   struct expr *rc;
   unsigned short index = 0;
   struct rcHashTableEntry **theHT;
   size_t htSize;
   unsigned int r;

   bdp = (struct bsaveDeftable *) buf;
   UpdateConstructHeader(theEnv,&bdp->header,&DeftableBinaryData(theEnv)->DeftableArray[obji].header,
                         DEFTABLE,sizeof(struct deftableModule),DeftableBinaryData(theEnv)->ModuleArray,
                         sizeof(Deftable),DeftableBinaryData(theEnv)->DeftableArray);
   DeftableBinaryData(theEnv)->DeftableArray[obji].columns = ExpressionPointer(bdp->columns);
   DeftableBinaryData(theEnv)->DeftableArray[obji].rows = ExpressionPointer(bdp->rows);
   
   AddConstructToHashMap(theEnv,&DeftableBinaryData(theEnv)->DeftableArray[obji].header,
                         DeftableBinaryData(theEnv)->DeftableArray[obji].header.whichModule);

   DeftableBinaryData(theEnv)->DeftableArray[obji].columnCount = bdp->columnCount;
   DeftableBinaryData(theEnv)->DeftableArray[obji].rowCount = bdp->rowCount;

   htSize = bdp->columnHashTableSize;
   DeftableBinaryData(theEnv)->DeftableArray[obji].columnHashTableSize = htSize;
   theHT = AllocateRCHT(theEnv,bdp->columnHashTableSize);
   DeftableBinaryData(theEnv)->DeftableArray[obji].columnHashTable = theHT;
   
   for (rc = DeftableBinaryData(theEnv)->DeftableArray[obji].columns;
        rc != NULL;
        rc = rc->nextArg)
     { AddEntryToRCHT(theEnv,theHT,htSize,rc,index++); }

   htSize = bdp->rowHashTableSize;
   DeftableBinaryData(theEnv)->DeftableArray[obji].rowHashTableSize = htSize;
   theHT = AllocateRCHT(theEnv,bdp->rowHashTableSize);
   DeftableBinaryData(theEnv)->DeftableArray[obji].rowHashTable = theHT;
   
   index = 0;
   rc = DeftableBinaryData(theEnv)->DeftableArray[obji].rows;
   for (r = 0; r < bdp->rowCount; r++)
     {
      AddEntryToRCHT(theEnv,theHT,htSize,rc,index++);
      rc += bdp->columnCount;
     }
  }

/**************************************/
/* ClearBload: Deffacts clear routine */
/*   when a binary load is in effect. */
/**************************************/
static void ClearBload(
  Environment *theEnv)
  {
   unsigned long i;
   size_t space;

   /*=============================================*/
   /* Decrement in use counters for atomic values */
   /* contained in the construct headers.         */
   /*=============================================*/

   for (i = 0; i < DeftableBinaryData(theEnv)->NumberOfDeftables; i++)
     { UnmarkConstructHeader(theEnv,&DeftableBinaryData(theEnv)->DeftableArray[i].header); }

   /*==========================================*/
   /* Deallocate the DeftableModule hash maps. */
   /*==========================================*/

   for (i = 0; i < DeftableBinaryData(theEnv)->NumberOfDeftableModules; i++)
     { ClearDefmoduleHashMap(theEnv,&DeftableBinaryData(theEnv)->ModuleArray[i].header); }

   /*====================================*/
   /* Deallocate the Deftable hash maps. */
   /*====================================*/
   
   for (i = 0; i < DeftableBinaryData(theEnv)->NumberOfDeftables; i++)
     {
      FreeRCHT(theEnv,DeftableBinaryData(theEnv)->DeftableArray[i].columnHashTable,
               DeftableBinaryData(theEnv)->DeftableArray[i].columnHashTableSize);
      FreeRCHT(theEnv,DeftableBinaryData(theEnv)->DeftableArray[i].rowHashTable,
               DeftableBinaryData(theEnv)->DeftableArray[i].rowHashTableSize);
     }

   /*=============================================================*/
   /* Deallocate the space used for the deftable data structures. */
   /*=============================================================*/

   space = DeftableBinaryData(theEnv)->NumberOfDeftables * sizeof(Deftable);
   if (space != 0) genfree(theEnv,DeftableBinaryData(theEnv)->DeftableArray,space);
   DeftableBinaryData(theEnv)->NumberOfDeftables = 0;

   /*====================================================================*/
   /* Deallocate the space used for the deftable module data structures. */
   /*====================================================================*/

   space = DeftableBinaryData(theEnv)->NumberOfDeftableModules * sizeof(struct deftableModule);
   if (space != 0) genfree(theEnv,DeftableBinaryData(theEnv)->ModuleArray,space);
   DeftableBinaryData(theEnv)->NumberOfDeftableModules = 0;
  }

/******************************************************/
/* BloadDeftableModuleReference: Returns the deftable */
/*   module pointer for use with the bload function.  */
/******************************************************/
void *BloadDeftableModuleReference(
  Environment *theEnv,
  unsigned long theIndex)
  {
   return (void *) &DeftableBinaryData(theEnv)->ModuleArray[theIndex];
  }

#endif /* DEFTABLE_CONSTRUCT && (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME) */
