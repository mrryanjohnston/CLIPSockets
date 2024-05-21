   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 7.00  01/23/24             */
   /*                                                     */
   /*                                                     */
   /*******************************************************/

/*************************************************************/
/* Purpose: Binary Load/Save Functions for Definstances      */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Changed integer type/precision.                */
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

/* =========================================
   *****************************************
               EXTERNAL DEFINITIONS
   =========================================
   ***************************************** */
#include "setup.h"

#if DEFINSTANCES_CONSTRUCT && (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE)

#include "bload.h"
#include "bsave.h"
#include "cstrcbin.h"
#include "defins.h"
#include "envrnmnt.h"
#include "memalloc.h"
#include "modulbin.h"

#include "dfinsbin.h"

/* =========================================
   *****************************************
                   CONSTANTS
   =========================================
   ***************************************** */

/* =========================================
   *****************************************
               MACROS AND TYPES
   =========================================
   ***************************************** */
typedef struct bsaveDefinstancesModule
  {
   struct bsaveDefmoduleItemHeader header;
  } BSAVE_DEFINSTANCES_MODULE;

typedef struct bsaveDefinstances
  {
   struct bsaveConstructHeader header;
   unsigned long mkinstance;
  } BSAVE_DEFINSTANCES;

/* =========================================
   *****************************************
      INTERNALLY VISIBLE FUNCTION HEADERS
   =========================================
   ***************************************** */

#if BLOAD_AND_BSAVE
   static void                    BsaveDefinstancesFind(Environment *);
   static void                    MarkDefinstancesItems(Environment *,ConstructHeader *,void *);
   static void                    BsaveDefinstancesExpressions(Environment *,FILE *);
   static void                    BsaveDefinstancesExpression(Environment *,ConstructHeader *,void *);
   static void                    BsaveStorageDefinstances(Environment *,FILE *);
   static void                    BsaveDefinstancesDriver(Environment *,FILE *);
   static void                    BsaveDefinstances(Environment *,ConstructHeader *,void *);
#endif

   static void                    BloadStorageDefinstances(Environment *);
   static void                    BloadDefinstances(Environment *);
   static void                    UpdateDefinstancesModule(Environment *,void *,unsigned long);
   static void                    UpdateDefinstances(Environment *,void *,unsigned long);
   static void                    ClearDefinstancesBload(Environment *);
   static void                    DeallocateDefinstancesBinaryData(Environment *);

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/***********************************************************
  NAME         : SetupDefinstancesBload
  DESCRIPTION  : Initializes data structures and
                   routines for binary loads of definstances
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Routines defined and structures initialized
  NOTES        : None
 ***********************************************************/
void SetupDefinstancesBload(
  Environment *theEnv)
  {
   AllocateEnvironmentData(theEnv,DFINSBIN_DATA,sizeof(struct definstancesBinaryData),DeallocateDefinstancesBinaryData);
#if BLOAD_AND_BSAVE
   AddBinaryItem(theEnv,"definstances",0,BsaveDefinstancesFind,BsaveDefinstancesExpressions,
                             BsaveStorageDefinstances,BsaveDefinstancesDriver,
                             BloadStorageDefinstances,BloadDefinstances,
                             ClearDefinstancesBload);
#else
   AddBinaryItem(theEnv,"definstances",0,NULL,NULL,NULL,NULL,
                             BloadStorageDefinstances,BloadDefinstances,
                             ClearDefinstancesBload);
#endif
  }

/*************************************************************/
/* DeallocateDefinstancesBinaryData: Deallocates environment */
/*    data for the definstances binary functionality.        */
/*************************************************************/
static void DeallocateDefinstancesBinaryData(
  Environment *theEnv)
  {
   size_t space;

#if (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME)
   space = DefinstancesBinaryData(theEnv)->DefinstancesCount * sizeof(struct definstances);
   if (space != 0) genfree(theEnv,DefinstancesBinaryData(theEnv)->DefinstancesArray,space);

   space =  DefinstancesBinaryData(theEnv)->ModuleCount * sizeof(struct definstancesModule);
   if (space != 0) genfree(theEnv,DefinstancesBinaryData(theEnv)->ModuleArray,space);
#endif
  }

/***************************************************
  NAME         : BloadDefinstancesModuleRef
  DESCRIPTION  : Returns a pointer to the
                 appropriate definstances module
  INPUTS       : The index of the module
  RETURNS      : A pointer to the module
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
void *BloadDefinstancesModuleRef(
  Environment *theEnv,
  unsigned long theIndex)
  {
   return ((void *) &DefinstancesBinaryData(theEnv)->ModuleArray[theIndex]);
  }

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

#if BLOAD_AND_BSAVE

/***************************************************************************
  NAME         : BsaveDefinstancesFind
  DESCRIPTION  : For all definstances, this routine marks all
                   the needed symbols.
                 Also, it also counts the number of
                   expression structures needed.
                 Also, counts total number of definstances.
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : ExpressionCount (a global from BSAVE.C) is incremented
                   for every expression needed
                 Symbols are marked in their structures
  NOTES        : Also sets bsaveIndex for each definstances (assumes
                   definstances will be bsaved in order of binary list)
 ***************************************************************************/
static void BsaveDefinstancesFind(
  Environment *theEnv)
  {
   SaveBloadCount(theEnv,DefinstancesBinaryData(theEnv)->ModuleCount);
   SaveBloadCount(theEnv,DefinstancesBinaryData(theEnv)->DefinstancesCount);
   DefinstancesBinaryData(theEnv)->DefinstancesCount = 0L;

   DefinstancesBinaryData(theEnv)->ModuleCount = GetNumberOfDefmodules(theEnv);
    
   DoForAllConstructs(theEnv,MarkDefinstancesItems,
                      DefinstancesData(theEnv)->DefinstancesModuleIndex,
                      false,NULL);
  }


/***************************************************
  NAME         : MarkDefinstancesItems
  DESCRIPTION  : Marks the needed items for
                 a definstances bsave
  INPUTS       : 1) The definstances
                 2) User data buffer (ignored)
  RETURNS      : Nothing useful
  SIDE EFFECTS : Needed items marked
  NOTES        : None
 ***************************************************/
static void MarkDefinstancesItems(
  Environment *theEnv,
  ConstructHeader *theDefinstances,
  void *userBuffer)
  {
#if MAC_XCD
#pragma unused(userBuffer)
#endif

   MarkConstructHeaderNeededItems(theDefinstances,DefinstancesBinaryData(theEnv)->DefinstancesCount++);
   ExpressionData(theEnv)->ExpressionCount += ExpressionSize(((Definstances *) theDefinstances)->mkinstance);
   MarkNeededItems(theEnv,((Definstances *) theDefinstances)->mkinstance);
  }

/***************************************************
  NAME         : BsaveDefinstancesExpressions
  DESCRIPTION  : Writes out all expressions needed
                   by deffunctyions
  INPUTS       : The file pointer of the binary file
  RETURNS      : Nothing useful
  SIDE EFFECTS : File updated
  NOTES        : None
 ***************************************************/
static void BsaveDefinstancesExpressions(
  Environment *theEnv,
  FILE *fp)
  {
   DoForAllConstructs(theEnv,BsaveDefinstancesExpression,DefinstancesData(theEnv)->DefinstancesModuleIndex,
                      false,fp);
  }

/***************************************************
  NAME         : BsaveDefinstancesExpression
  DESCRIPTION  : Saves the needed expressions for
                 a definstances bsave
  INPUTS       : 1) The definstances
                 2) Output data file pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Expressions saved
  NOTES        : None
 ***************************************************/
static void BsaveDefinstancesExpression(
  Environment *theEnv,
  ConstructHeader *theDefinstances,
  void *userBuffer)
  {
   BsaveExpression(theEnv,((Definstances *) theDefinstances)->mkinstance,(FILE *) userBuffer);
  }

/***********************************************************
  NAME         : BsaveStorageDefinstances
  DESCRIPTION  : Writes out number of each type of structure
                   required for definstances
                 Space required for counts (unsigned long)
  INPUTS       : File pointer of binary file
  RETURNS      : Nothing useful
  SIDE EFFECTS : Binary file adjusted
  NOTES        : None
 ***********************************************************/
static void BsaveStorageDefinstances(
  Environment *theEnv,
  FILE *fp)
  {
   size_t space;

   space = sizeof(unsigned long) * 2;
   GenWrite(&space,sizeof(size_t),fp);
   GenWrite(&DefinstancesBinaryData(theEnv)->ModuleCount,sizeof(unsigned long),fp);
   GenWrite(&DefinstancesBinaryData(theEnv)->DefinstancesCount,sizeof(unsigned long),fp);
  }

/*************************************************************************************
  NAME         : BsaveDefinstancesDriver
  DESCRIPTION  : Writes out definstances in binary format
                 Space required (unsigned long)
                 All definstances (sizeof(Definstances) * Number of definstances)
  INPUTS       : File pointer of binary file
  RETURNS      : Nothing useful
  SIDE EFFECTS : Binary file adjusted
  NOTES        : None
 *************************************************************************************/
static void BsaveDefinstancesDriver(
  Environment *theEnv,
  FILE *fp)
  {
   size_t space;
   Defmodule *theModule;
   DEFINSTANCES_MODULE *theModuleItem;
   BSAVE_DEFINSTANCES_MODULE dummy_mitem;

   space = ((sizeof(BSAVE_DEFINSTANCES_MODULE) * DefinstancesBinaryData(theEnv)->ModuleCount) +
            (sizeof(BSAVE_DEFINSTANCES) * DefinstancesBinaryData(theEnv)->DefinstancesCount));
   GenWrite(&space,sizeof(size_t),fp);

   /* =================================
      Write out each definstances module
      ================================= */
   DefinstancesBinaryData(theEnv)->DefinstancesCount = 0L;
   theModule = GetNextDefmodule(theEnv,NULL);
   while (theModule != NULL)
     {
      theModuleItem = (DEFINSTANCES_MODULE *)
                      GetModuleItem(theEnv,theModule,FindModuleItem(theEnv,"definstances")->moduleIndex);
      AssignBsaveDefmdlItemHdrHMVals(&dummy_mitem.header,&theModuleItem->header);
      GenWrite(&dummy_mitem,sizeof(BSAVE_DEFINSTANCES_MODULE),fp);
      theModule = GetNextDefmodule(theEnv,theModule);
     }

   /*==============================*/
   /* Write out each definstances. */
   /*==============================*/

   DoForAllConstructs(theEnv,BsaveDefinstances,DefinstancesData(theEnv)->DefinstancesModuleIndex,
                      false,fp);

   RestoreBloadCount(theEnv,&DefinstancesBinaryData(theEnv)->ModuleCount);
   RestoreBloadCount(theEnv,&DefinstancesBinaryData(theEnv)->DefinstancesCount);
  }

/***************************************************
  NAME         : BsaveDefinstances
  DESCRIPTION  : Bsaves a definstances
  INPUTS       : 1) The definstances
                 2) Output data file pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Definstances saved
  NOTES        : None
 ***************************************************/
static void BsaveDefinstances(
  Environment *theEnv,
  ConstructHeader *theDefinstances,
  void *userBuffer)
  {
   Definstances *dptr = (Definstances *) theDefinstances;
   BSAVE_DEFINSTANCES dummy_df;

   AssignBsaveConstructHeaderVals(&dummy_df.header,&dptr->header);
  if (dptr->mkinstance != NULL)
     {
      dummy_df.mkinstance = ExpressionData(theEnv)->ExpressionCount;
      ExpressionData(theEnv)->ExpressionCount += ExpressionSize(dptr->mkinstance);
     }
   else
    dummy_df.mkinstance = ULONG_MAX;
   GenWrite(&dummy_df,sizeof(BSAVE_DEFINSTANCES),(FILE *) userBuffer);
  }

#endif

/***********************************************************************
  NAME         : BloadStorageDefinstances
  DESCRIPTION  : This routine space required for definstances
                   structures and allocates space for them
  INPUTS       : Nothing
  RETURNS      : Nothing useful
  SIDE EFFECTS : Arrays allocated and set
  NOTES        : This routine makes no attempt to reset any pointers
                   within the structures
 ***********************************************************************/
static void BloadStorageDefinstances(
  Environment *theEnv)
  {
   size_t space;

   GenReadBinary(theEnv,&space,sizeof(size_t));
   if (space == 0L)
     return;
   GenReadBinary(theEnv,&DefinstancesBinaryData(theEnv)->ModuleCount,sizeof(unsigned long));
   GenReadBinary(theEnv,&DefinstancesBinaryData(theEnv)->DefinstancesCount,sizeof(unsigned long));
   if (DefinstancesBinaryData(theEnv)->ModuleCount == 0L)
     {
      DefinstancesBinaryData(theEnv)->ModuleArray = NULL;
      DefinstancesBinaryData(theEnv)->DefinstancesArray = NULL;
      return;
     }

   space = (DefinstancesBinaryData(theEnv)->ModuleCount * sizeof(DEFINSTANCES_MODULE));
   DefinstancesBinaryData(theEnv)->ModuleArray = (DEFINSTANCES_MODULE *) genalloc(theEnv,space);

   if (DefinstancesBinaryData(theEnv)->DefinstancesCount == 0L)
     {
      DefinstancesBinaryData(theEnv)->DefinstancesArray = NULL;
      return;
     }

   space = (DefinstancesBinaryData(theEnv)->DefinstancesCount * sizeof(Definstances));
   DefinstancesBinaryData(theEnv)->DefinstancesArray = (Definstances *) genalloc(theEnv,space);
  }

/*********************************************************************
  NAME         : BloadDefinstances
  DESCRIPTION  : This routine reads definstances information from
                   a binary file
                 This routine moves through the definstances
                   binary array updating pointers
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Pointers reset from array indices
  NOTES        : Assumes all loading is finished
 ********************************************************************/
static void BloadDefinstances(
  Environment *theEnv)
  {
   size_t space;

   GenReadBinary(theEnv,&space,sizeof(size_t));
   BloadandRefresh(theEnv,DefinstancesBinaryData(theEnv)->ModuleCount,sizeof(BSAVE_DEFINSTANCES_MODULE),UpdateDefinstancesModule);
   BloadandRefresh(theEnv,DefinstancesBinaryData(theEnv)->DefinstancesCount,sizeof(BSAVE_DEFINSTANCES),UpdateDefinstances);
  }

/*******************************************************
  NAME         : UpdateDefinstancesModule
  DESCRIPTION  : Updates definstances module with binary
                 load data - sets pointers from
                 offset information
  INPUTS       : 1) A pointer to the bloaded data
                 2) The index of the binary array
                    element to update
  RETURNS      : Nothing useful
  SIDE EFFECTS : Definstances moudle pointers updated
  NOTES        : None
 *******************************************************/
static void UpdateDefinstancesModule(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   BSAVE_DEFINSTANCES_MODULE *bdptr;

   bdptr = (BSAVE_DEFINSTANCES_MODULE *) buf;
   UpdateDefmoduleItemHeaderHM(theEnv,&bdptr->header,&DefinstancesBinaryData(theEnv)->ModuleArray[obji].header,
                               sizeof(Definstances),DefinstancesBinaryData(theEnv)->DefinstancesArray);
                               
   AssignHashMapSize(theEnv,&DefinstancesBinaryData(theEnv)->ModuleArray[obji].header,bdptr->header.itemCount);
  }

/***************************************************
  NAME         : UpdateDefinstances
  DESCRIPTION  : Updates definstances with binary
                 load data - sets pointers from
                 offset information
  INPUTS       : 1) A pointer to the bloaded data
                 2) The index of the binary array
                    element to update
  RETURNS      : Nothing useful
  SIDE EFFECTS : Definstances pointers upadted
  NOTES        : None
 ***************************************************/
static void UpdateDefinstances(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   BSAVE_DEFINSTANCES *bdptr;
   Definstances *dfiptr;

   bdptr = (BSAVE_DEFINSTANCES *) buf;
   dfiptr = (Definstances *) &DefinstancesBinaryData(theEnv)->DefinstancesArray[obji];

   UpdateConstructHeader(theEnv,&bdptr->header,&dfiptr->header,DEFINSTANCES,
                         sizeof(DEFINSTANCES_MODULE),DefinstancesBinaryData(theEnv)->ModuleArray,
                         sizeof(Definstances),DefinstancesBinaryData(theEnv)->DefinstancesArray);
   dfiptr->mkinstance = ExpressionPointer(bdptr->mkinstance);
   dfiptr->busy = 0;

   AddConstructToHashMap(theEnv,&DefinstancesBinaryData(theEnv)->DefinstancesArray[obji].header,
                         DefinstancesBinaryData(theEnv)->DefinstancesArray[obji].header.whichModule);
  }

/***************************************************************
  NAME         : ClearDefinstancesBload
  DESCRIPTION  : Release all binary-loaded definstances
                   structure arrays
                 Resets definstances list to NULL
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Memory cleared
  NOTES        : Definstances name symbol counts decremented
 ***************************************************************/
static void ClearDefinstancesBload(
  Environment *theEnv)
  {
   unsigned long i;
   size_t space;

   space = (sizeof(DEFINSTANCES_MODULE) * DefinstancesBinaryData(theEnv)->ModuleCount);
   if (space == 0L)
     return;
     
   /*==============================================*/
   /* Deallocate the DefinstancesModule hash maps. */
   /*==============================================*/

   for (i = 0; i < DefinstancesBinaryData(theEnv)->DefinstancesCount; i++)
     { ClearDefmoduleHashMap(theEnv,&DefinstancesBinaryData(theEnv)->ModuleArray[i].header); }

   genfree(theEnv,DefinstancesBinaryData(theEnv)->ModuleArray,space);
   DefinstancesBinaryData(theEnv)->ModuleArray = NULL;
   DefinstancesBinaryData(theEnv)->ModuleCount = 0L;

   for (i = 0 ; i < DefinstancesBinaryData(theEnv)->DefinstancesCount ; i++)
     UnmarkConstructHeader(theEnv,&DefinstancesBinaryData(theEnv)->DefinstancesArray[i].header);
   space = (sizeof(Definstances) * DefinstancesBinaryData(theEnv)->DefinstancesCount);
   if (space == 0)
     return;
   genfree(theEnv,DefinstancesBinaryData(theEnv)->DefinstancesArray,space);
   DefinstancesBinaryData(theEnv)->DefinstancesArray = NULL;
   DefinstancesBinaryData(theEnv)->DefinstancesCount = 0L;
  }

#endif

