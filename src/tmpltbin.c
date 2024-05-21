   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 7.00  01/23/24             */
   /*                                                     */
   /*           DEFTEMPLATE BSAVE/BLOAD MODULE            */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements the binary save/load feature for the  */
/*    deftemplate construct.                                 */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Added support for templates maintaining their  */
/*            own list of facts.                             */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            Support for deftemplate slot facets.           */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            Removed initial-fact support.                  */
/*                                                           */
/*      7.00: Data driven backward chaining.                 */
/*                                                           */
/*            Deftemplate inheritance.                       */
/*                                                           */
/*            Support for non-reactive fact patterns.        */
/*                                                           */
/*            Construct hashing for quick lookup.            */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if DEFTEMPLATE_CONSTRUCT && (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME)

#include <stdio.h>

#include "bload.h"
#include "bsave.h"
#include "cstrnbin.h"
#include "envrnmnt.h"
#include "factbin.h"
#include "factmngr.h"
#include "memalloc.h"
#include "tmpltdef.h"
#include "tmpltpsr.h"
#include "tmpltutl.h"

#include "tmpltbin.h"

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
   static void                    UpdateDeftemplateModule(Environment *,void *,unsigned long);
   static void                    UpdateDeftemplate(Environment *,void *,unsigned long);
   static void                    UpdateDeftemplateSlot(Environment *,void *,unsigned long);
   static void                    ClearBload(Environment *);
   static void                    DeallocateDeftemplateBloadData(Environment *);

/***********************************************/
/* DeftemplateBinarySetup: Installs the binary */
/*   save/load feature for deftemplates.       */
/***********************************************/
void DeftemplateBinarySetup(
  Environment *theEnv)
  {
   AllocateEnvironmentData(theEnv,TMPLTBIN_DATA,sizeof(struct deftemplateBinaryData),DeallocateDeftemplateBloadData);
#if BLOAD_AND_BSAVE
   AddBinaryItem(theEnv,"deftemplate",0,BsaveFind,NULL,
                             BsaveStorage,BsaveBinaryItem,
                             BloadStorage,BloadBinaryItem,
                             ClearBload);
#endif
#if (BLOAD || BLOAD_ONLY)
   AddBinaryItem(theEnv,"deftemplate",0,NULL,NULL,NULL,NULL,
                             BloadStorage,BloadBinaryItem,
                             ClearBload);
#endif
  }

/***********************************************************/
/* DeallocateDeftemplateBloadData: Deallocates environment */
/*    data for the deftemplate bsave functionality.        */
/***********************************************************/
static void DeallocateDeftemplateBloadData(
  Environment *theEnv)
  {
   size_t space;

   space =  DeftemplateBinaryData(theEnv)->NumberOfTemplateModules * sizeof(struct deftemplateModule);
   if (space != 0) genfree(theEnv,DeftemplateBinaryData(theEnv)->ModuleArray,space);

   space = DeftemplateBinaryData(theEnv)->NumberOfDeftemplates * sizeof(Deftemplate);
   if (space != 0) genfree(theEnv,DeftemplateBinaryData(theEnv)->DeftemplateArray,space);

   space =  DeftemplateBinaryData(theEnv)->NumberOfTemplateSlots * sizeof(struct templateSlot);
   if (space != 0) genfree(theEnv,DeftemplateBinaryData(theEnv)->SlotArray,space);
  }

#if BLOAD_AND_BSAVE

/**************************************************************/
/* BsaveFind: Counts the number of data structures which must */
/*   be saved in the binary image for the deftemplates in the */
/*   current environment.                                     */
/**************************************************************/
static void BsaveFind(
  Environment *theEnv)
  {
   Deftemplate *theDeftemplate;
   struct templateSlot *theSlot;
   Defmodule *theModule;

   /*=======================================================*/
   /* If a binary image is already loaded, then temporarily */
   /* save the count values since these will be overwritten */
   /* in the process of saving the binary image.            */
   /*=======================================================*/

   SaveBloadCount(theEnv,DeftemplateBinaryData(theEnv)->NumberOfDeftemplates);
   SaveBloadCount(theEnv,DeftemplateBinaryData(theEnv)->NumberOfTemplateSlots);
   SaveBloadCount(theEnv,DeftemplateBinaryData(theEnv)->NumberOfTemplateModules);

   /*==================================================*/
   /* Set the count of deftemplates, deftemplate slots */
   /* and deftemplate module data structures to zero.  */
   /*==================================================*/

   DeftemplateBinaryData(theEnv)->NumberOfDeftemplates = 0;
   DeftemplateBinaryData(theEnv)->NumberOfTemplateSlots = 0;
   DeftemplateBinaryData(theEnv)->NumberOfTemplateModules = 0;

   /*===========================*/
   /* Loop through each module. */
   /*===========================*/

   for (theModule = GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = GetNextDefmodule(theEnv,theModule))
     {
      /*============================================*/
      /* Set the current module to the module being */
      /* examined and increment the number of       */
      /* deftemplate modules encountered.           */
      /*============================================*/

      SetCurrentModule(theEnv,theModule);
      DeftemplateBinaryData(theEnv)->NumberOfTemplateModules++;

      /*======================================================*/
      /* Loop through each deftemplate in the current module. */
      /*======================================================*/

      for (theDeftemplate = GetNextDeftemplate(theEnv,NULL);
           theDeftemplate != NULL;
           theDeftemplate = GetNextDeftemplate(theEnv,theDeftemplate))
        {
         /*======================================================*/
         /* Initialize the construct header for the binary save. */
         /*======================================================*/

         MarkConstructHeaderNeededItems(&theDeftemplate->header,
                                        DeftemplateBinaryData(theEnv)->NumberOfDeftemplates++);

         /*=============================================================*/
         /* Loop through each slot in the deftemplate, incrementing the */
         /* slot count and marking the slot names as needed symbols.    */
         /*=============================================================*/

         for (theSlot = theDeftemplate->slotList;
              theSlot != NULL;
              theSlot = theSlot->next)
           {
            DeftemplateBinaryData(theEnv)->NumberOfTemplateSlots++;
            theSlot->slotName->neededSymbol = true;
           }
        }

     }
  }

/*********************************************************/
/* BsaveStorage: Writes out the storage requirements for */
/*    all deftemplate structures to the binary file.     */
/*********************************************************/
static void BsaveStorage(
  Environment *theEnv,
  FILE *fp)
  {
   size_t space;

   /*========================================================================*/
   /* Three data structures are saved as part of a deftemplate binary image: */
   /* the deftemplate data structure, the deftemplateModule data structure,  */
   /* and the templateSlot data structure. The data structures associated    */
   /* with default values and constraints are not save with the deftemplate  */
   /* portion of the binary image.                                           */
   /*========================================================================*/

   space = sizeof(long) * 3;
   GenWrite(&space,sizeof(size_t),fp);
   GenWrite(&DeftemplateBinaryData(theEnv)->NumberOfDeftemplates,sizeof(long),fp);
   GenWrite(&DeftemplateBinaryData(theEnv)->NumberOfTemplateSlots,sizeof(long),fp);
   GenWrite(&DeftemplateBinaryData(theEnv)->NumberOfTemplateModules,sizeof(long),fp);
  }

/***********************************************/
/* BsaveBinaryItem: Writes out all deftemplate */
/*   structures to the binary file.            */
/***********************************************/
static void BsaveBinaryItem(
  Environment *theEnv,
  FILE *fp)
  {
   size_t space;
   Deftemplate *theDeftemplate;
   struct bsaveDeftemplate tempDeftemplate;
   struct templateSlot *theSlot;
   struct bsaveTemplateSlot tempTemplateSlot;
   struct bsaveDeftemplateModule tempTemplateModule;
   Defmodule *theModule;
   struct deftemplateModule *theModuleItem;

   /*============================================================*/
   /* Write out the amount of space taken up by the deftemplate, */
   /* deftemplateModule, and templateSlot data structures in the */
   /* binary image.                                              */
   /*============================================================*/

   space = (DeftemplateBinaryData(theEnv)->NumberOfDeftemplates * sizeof(struct bsaveDeftemplate)) +
           (DeftemplateBinaryData(theEnv)->NumberOfTemplateSlots * sizeof(struct bsaveTemplateSlot)) +
           (DeftemplateBinaryData(theEnv)->NumberOfTemplateModules * sizeof(struct bsaveDeftemplateModule));
   GenWrite(&space,sizeof(size_t),fp);

   /*===================================================*/
   /* Write out each deftemplate module data structure. */
   /*===================================================*/

   DeftemplateBinaryData(theEnv)->NumberOfDeftemplates = 0;
   for (theModule = GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = GetNextDefmodule(theEnv,theModule))
     {
      SetCurrentModule(theEnv,theModule);

      theModuleItem = (struct deftemplateModule *)
                      GetModuleItem(theEnv,NULL,FindModuleItem(theEnv,"deftemplate")->moduleIndex);
      AssignBsaveDefmdlItemHdrHMVals(&tempTemplateModule.header,&theModuleItem->header);
      GenWrite(&tempTemplateModule,sizeof(struct bsaveDeftemplateModule),fp);
     }

   /*============================================*/
   /* Write out each deftemplate data structure. */
   /*============================================*/

   DeftemplateBinaryData(theEnv)->NumberOfTemplateSlots = 0;
   for (theModule = GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = GetNextDefmodule(theEnv,theModule))
     {
      SetCurrentModule(theEnv,theModule);

      for (theDeftemplate = GetNextDeftemplate(theEnv,NULL);
           theDeftemplate != NULL;
           theDeftemplate = GetNextDeftemplate(theEnv,theDeftemplate))
        {
         AssignBsaveConstructHeaderVals(&tempDeftemplate.header,&theDeftemplate->header);
         tempDeftemplate.parent = BsaveDeftemplateIndex(theDeftemplate->parent);
         tempDeftemplate.child = BsaveDeftemplateIndex(theDeftemplate->child);
         tempDeftemplate.sibling = BsaveDeftemplateIndex(theDeftemplate->sibling);
         tempDeftemplate.implied = theDeftemplate->implied;
         tempDeftemplate.numberOfSlots = theDeftemplate->numberOfSlots;
         tempDeftemplate.patternNetwork = BsaveFactPatternIndex(theDeftemplate->patternNetwork);
         tempDeftemplate.goalNetwork = BsaveFactPatternIndex(theDeftemplate->goalNetwork);

         if (theDeftemplate->slotList != NULL)
           { tempDeftemplate.slotList = DeftemplateBinaryData(theEnv)->NumberOfTemplateSlots; }
         else
           { tempDeftemplate.slotList = ULONG_MAX; }

         GenWrite(&tempDeftemplate,sizeof(struct bsaveDeftemplate),fp);

         DeftemplateBinaryData(theEnv)->NumberOfTemplateSlots += theDeftemplate->numberOfSlots;
        }
     }

   /*=============================================*/
   /* Write out each templateSlot data structure. */
   /*=============================================*/

   for (theModule = GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = GetNextDefmodule(theEnv,theModule))
     {
      SetCurrentModule(theEnv,theModule);

      for (theDeftemplate = GetNextDeftemplate(theEnv,NULL);
           theDeftemplate != NULL;
           theDeftemplate = GetNextDeftemplate(theEnv,theDeftemplate))
        {
         for (theSlot = theDeftemplate->slotList;
              theSlot != NULL;
              theSlot = theSlot->next)
           {
            tempTemplateSlot.constraints = ConstraintIndex(theSlot->constraints);
            tempTemplateSlot.slotName = theSlot->slotName->bucket;
            tempTemplateSlot.multislot = theSlot->multislot;
            tempTemplateSlot.noDefault = theSlot->noDefault;
            tempTemplateSlot.defaultPresent = theSlot->defaultPresent;
            tempTemplateSlot.defaultDynamic = theSlot->defaultDynamic;
            tempTemplateSlot.reactive = theSlot->reactive;
            tempTemplateSlot.defaultList = HashedExpressionIndex(theEnv,theSlot->defaultList);
            tempTemplateSlot.facetList = HashedExpressionIndex(theEnv,theSlot->facetList);

            if (theSlot->next != NULL) tempTemplateSlot.next = 0L;
            else tempTemplateSlot.next = ULONG_MAX;

            GenWrite(&tempTemplateSlot,sizeof(struct bsaveTemplateSlot),fp);
           }
        }
     }

   /*=============================================================*/
   /* If a binary image was already loaded when the bsave command */
   /* was issued, then restore the counts indicating the number   */
   /* of deftemplates, deftemplate modules, and deftemplate slots */
   /* in the binary image (these were overwritten by the binary   */
   /* save).                                                      */
   /*=============================================================*/

   RestoreBloadCount(theEnv,&DeftemplateBinaryData(theEnv)->NumberOfDeftemplates);
   RestoreBloadCount(theEnv,&DeftemplateBinaryData(theEnv)->NumberOfTemplateSlots);
   RestoreBloadCount(theEnv,&DeftemplateBinaryData(theEnv)->NumberOfTemplateModules);
  }

#endif /* BLOAD_AND_BSAVE */

/****************************************************/
/* BloadStorage: Allocates storage requirements for */
/*   the deftemplates used by this binary image.    */
/****************************************************/
static void BloadStorage(
  Environment *theEnv)
  {
   size_t space;

   /*=========================================================*/
   /* Determine the number of deftemplate, deftemplateModule, */
   /* and templateSlot data structures to be read.            */
   /*=========================================================*/

   GenReadBinary(theEnv,&space,sizeof(size_t));
   GenReadBinary(theEnv,&DeftemplateBinaryData(theEnv)->NumberOfDeftemplates,sizeof(long));
   GenReadBinary(theEnv,&DeftemplateBinaryData(theEnv)->NumberOfTemplateSlots,sizeof(long));
   GenReadBinary(theEnv,&DeftemplateBinaryData(theEnv)->NumberOfTemplateModules,sizeof(long));

   /*====================================*/
   /* Allocate the space needed for the  */
   /* deftemplateModule data structures. */
   /*====================================*/

   if (DeftemplateBinaryData(theEnv)->NumberOfTemplateModules == 0)
     {
      DeftemplateBinaryData(theEnv)->DeftemplateArray = NULL;
      DeftemplateBinaryData(theEnv)->SlotArray = NULL;
      DeftemplateBinaryData(theEnv)->ModuleArray = NULL;
      return;
     }

   space = DeftemplateBinaryData(theEnv)->NumberOfTemplateModules * sizeof(struct deftemplateModule);
   DeftemplateBinaryData(theEnv)->ModuleArray = (struct deftemplateModule *) genalloc(theEnv,space);

   /*===================================*/
   /* Allocate the space needed for the */
   /* deftemplate data structures.      */
   /*===================================*/

   if (DeftemplateBinaryData(theEnv)->NumberOfDeftemplates == 0)
     {
      DeftemplateBinaryData(theEnv)->DeftemplateArray = NULL;
      DeftemplateBinaryData(theEnv)->SlotArray = NULL;
      return;
     }

   space = DeftemplateBinaryData(theEnv)->NumberOfDeftemplates * sizeof(Deftemplate);
   DeftemplateBinaryData(theEnv)->DeftemplateArray = (Deftemplate *) genalloc(theEnv,space);

   /*===================================*/
   /* Allocate the space needed for the */
   /* templateSlot data structures.     */
   /*===================================*/

   if (DeftemplateBinaryData(theEnv)->NumberOfTemplateSlots == 0)
     {
      DeftemplateBinaryData(theEnv)->SlotArray = NULL;
      return;
     }

   space =  DeftemplateBinaryData(theEnv)->NumberOfTemplateSlots * sizeof(struct templateSlot);
   DeftemplateBinaryData(theEnv)->SlotArray = (struct templateSlot *) genalloc(theEnv,space);
  }

/********************************************************/
/* BloadBinaryItem: Loads and refreshes the deftemplate */
/*   constructs used by this binary image.              */
/********************************************************/
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

   /*===============================================*/
   /* Read in the deftemplateModule data structures */
   /* and refresh the pointers.                     */
   /*===============================================*/

   BloadandRefresh(theEnv,DeftemplateBinaryData(theEnv)->NumberOfTemplateModules,sizeof(struct bsaveDeftemplateModule),
                   UpdateDeftemplateModule);

   /*===============================================*/
   /* Read in the deftemplateModule data structures */
   /* and refresh the pointers.                     */
   /*===============================================*/

   BloadandRefresh(theEnv,DeftemplateBinaryData(theEnv)->NumberOfDeftemplates,sizeof(struct bsaveDeftemplate),
                   UpdateDeftemplate);

   /*==========================================*/
   /* Read in the templateSlot data structures */
   /* and refresh the pointers.                */
   /*==========================================*/

   BloadandRefresh(theEnv,DeftemplateBinaryData(theEnv)->NumberOfTemplateSlots,sizeof(struct bsaveTemplateSlot),
                   UpdateDeftemplateSlot);
  }

/**************************************************/
/* UpdateDeftemplateModule: Bload refresh routine */
/*   for deftemplateModule data structures.       */
/**************************************************/
static void UpdateDeftemplateModule(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   struct bsaveDeftemplateModule *bdmPtr;

   bdmPtr = (struct bsaveDeftemplateModule *) buf;
   UpdateDefmoduleItemHeaderHM(theEnv,&bdmPtr->header,&DeftemplateBinaryData(theEnv)->ModuleArray[obji].header,
                               sizeof(Deftemplate),
                               (void *) DeftemplateBinaryData(theEnv)->DeftemplateArray);
                               
   AssignHashMapSize(theEnv,&DeftemplateBinaryData(theEnv)->ModuleArray[obji].header,bdmPtr->header.itemCount);
  }

/********************************************/
/* UpdateDeftemplate: Bload refresh routine */
/*   for deftemplate data structures.       */
/********************************************/
static void UpdateDeftemplate(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   Deftemplate *theDeftemplate;
   struct bsaveDeftemplate *bdtPtr;

   bdtPtr = (struct bsaveDeftemplate *) buf;
   theDeftemplate = &DeftemplateBinaryData(theEnv)->DeftemplateArray[obji];

   UpdateConstructHeader(theEnv,&bdtPtr->header,&theDeftemplate->header,DEFTEMPLATE,
                         sizeof(struct deftemplateModule),DeftemplateBinaryData(theEnv)->ModuleArray,
                         sizeof(Deftemplate),DeftemplateBinaryData(theEnv)->DeftemplateArray);
                         
   theDeftemplate->parent = BloadDeftemplatePointer(bdtPtr->parent);
   theDeftemplate->child = BloadDeftemplatePointer(bdtPtr->child);
   theDeftemplate->sibling = BloadDeftemplatePointer(bdtPtr->sibling);

   if (bdtPtr->slotList != ULONG_MAX)
     { theDeftemplate->slotList = (struct templateSlot *) &DeftemplateBinaryData(theEnv)->SlotArray[bdtPtr->slotList]; }
   else
     { theDeftemplate->slotList = NULL; }

   if (bdtPtr->patternNetwork != ULONG_MAX)
     { theDeftemplate->patternNetwork = (struct factPatternNode *) BloadFactPatternPointer(bdtPtr->patternNetwork); }
   else
     { theDeftemplate->patternNetwork = NULL; }

   if (bdtPtr->goalNetwork != ULONG_MAX)
     { theDeftemplate->goalNetwork = (struct factPatternNode *) BloadFactPatternPointer(bdtPtr->goalNetwork); }
   else
     { theDeftemplate->goalNetwork = NULL; }

   theDeftemplate->implied = bdtPtr->implied;
#if DEBUGGING_FUNCTIONS
   theDeftemplate->watchFacts = FactData(theEnv)->WatchFacts;
   theDeftemplate->watchGoals = FactData(theEnv)->WatchGoals;
#endif
   theDeftemplate->inScope = false;
   theDeftemplate->numberOfSlots = bdtPtr->numberOfSlots;
   theDeftemplate->factList = NULL;
   theDeftemplate->lastFact = NULL;
   
   AddConstructToHashMap(theEnv,&DeftemplateBinaryData(theEnv)->DeftemplateArray[obji].header,
                         DeftemplateBinaryData(theEnv)->DeftemplateArray[obji].header.whichModule);
  }

/************************************************/
/* UpdateDeftemplateSlot: Bload refresh routine */
/*   for templateSlot data structures.          */
/************************************************/
static void UpdateDeftemplateSlot(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   struct templateSlot *theSlot;
   struct bsaveTemplateSlot *btsPtr;

   btsPtr = (struct bsaveTemplateSlot *) buf;
   theSlot = (struct templateSlot *) &DeftemplateBinaryData(theEnv)->SlotArray[obji];

   theSlot->slotName = SymbolPointer(btsPtr->slotName);
   IncrementLexemeCount(theSlot->slotName);
   theSlot->defaultList = HashedExpressionPointer(btsPtr->defaultList);
   theSlot->facetList = HashedExpressionPointer(btsPtr->facetList);
   theSlot->constraints = ConstraintPointer(btsPtr->constraints);

   theSlot->multislot = btsPtr->multislot;
   theSlot->noDefault = btsPtr->noDefault;
   theSlot->defaultPresent = btsPtr->defaultPresent;
   theSlot->defaultDynamic = btsPtr->defaultDynamic;
   theSlot->reactive = btsPtr->reactive;

   if (btsPtr->next != ULONG_MAX)
     { theSlot->next = (struct templateSlot *) &DeftemplateBinaryData(theEnv)->SlotArray[obji + 1]; }
   else
     { theSlot->next = NULL; }
  }

/*****************************************/
/* ClearBload: Deftemplate clear routine */
/*   when a binary load is in effect.    */
/*****************************************/
static void ClearBload(
  Environment *theEnv)
  {
   size_t space;
   unsigned long i;

   /*=============================================*/
   /* Decrement in use counters for atomic values */
   /* contained in the construct headers.         */
   /*=============================================*/

   for (i = 0; i < DeftemplateBinaryData(theEnv)->NumberOfDeftemplates; i++)
     { UnmarkConstructHeader(theEnv,&DeftemplateBinaryData(theEnv)->DeftemplateArray[i].header); }
     
   /*=============================================*/
   /* Deallocate the DeftemplateModule hash maps. */
   /*=============================================*/

   for (i = 0; i < DeftemplateBinaryData(theEnv)->NumberOfTemplateModules; i++)
     { ClearDefmoduleHashMap(theEnv,&DeftemplateBinaryData(theEnv)->ModuleArray[i].header); }

   /*=======================================*/
   /* Decrement in use counters for symbols */
   /* used as slot names.                   */
   /*=======================================*/

   for (i = 0; i < DeftemplateBinaryData(theEnv)->NumberOfTemplateSlots; i++)
     { ReleaseLexeme(theEnv,DeftemplateBinaryData(theEnv)->SlotArray[i].slotName); }

   /*======================================================================*/
   /* Deallocate the space used for the deftemplateModule data structures. */
   /*======================================================================*/

   space =  DeftemplateBinaryData(theEnv)->NumberOfTemplateModules * sizeof(struct deftemplateModule);
   if (space != 0) genfree(theEnv,DeftemplateBinaryData(theEnv)->ModuleArray,space);
   DeftemplateBinaryData(theEnv)->NumberOfTemplateModules = 0;

   /*================================================================*/
   /* Deallocate the space used for the deftemplate data structures. */
   /*================================================================*/

   space = DeftemplateBinaryData(theEnv)->NumberOfDeftemplates * sizeof(Deftemplate);
   if (space != 0) genfree(theEnv,DeftemplateBinaryData(theEnv)->DeftemplateArray,space);
   DeftemplateBinaryData(theEnv)->NumberOfDeftemplates = 0;

   /*=================================================================*/
   /* Deallocate the space used for the templateSlot data structures. */
   /*=================================================================*/

   space =  DeftemplateBinaryData(theEnv)->NumberOfTemplateSlots * sizeof(struct templateSlot);
   if (space != 0) genfree(theEnv,DeftemplateBinaryData(theEnv)->SlotArray,space);
   DeftemplateBinaryData(theEnv)->NumberOfTemplateSlots = 0;
  }

/************************************************************/
/* BloadDeftemplateModuleReference: Returns the deftemplate */
/*   module pointer for use with the bload function.        */
/************************************************************/
void *BloadDeftemplateModuleReference(
  Environment *theEnv,
  unsigned long theIndex)
  {
   return ((void *) &DeftemplateBinaryData(theEnv)->ModuleArray[theIndex]);
  }

#endif /* DEFTEMPLATE_CONSTRUCT && (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME) */


