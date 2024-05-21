   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 7.00  01/23/24             */
   /*                                                     */
   /*                                                     */
   /*******************************************************/

/*************************************************************/
/* Purpose: Binary Load/Save Functions for Deffunctions      */
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

#if DEFFUNCTION_CONSTRUCT && (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE)

#include "bload.h"
#include "bsave.h"
#include "cstrcbin.h"
#include "cstrccom.h"
#include "envrnmnt.h"
#include "memalloc.h"
#include "modulbin.h"

#include "dffnxbin.h"

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
typedef struct bsaveDeffunctionModule
  {
   struct bsaveDefmoduleItemHeader header;
  } BSAVE_DEFFUNCTION_MODULE;

typedef struct bsaveDeffunctionStruct
  {
   struct bsaveConstructHeader header;
   unsigned short minNumberOfParameters;
   unsigned short maxNumberOfParameters;
   unsigned short numberOfLocalVars;
   unsigned long name;
   unsigned long code;
  } BSAVE_DEFFUNCTION;

/****************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS  */
/****************************************/

#if BLOAD_AND_BSAVE
   static void                    BsaveDeffunctionFind(Environment *);
   static void                    MarkDeffunctionItems(Environment *,ConstructHeader *,void *);
   static void                    BsaveDeffunctionExpressions(Environment *,FILE *);
   static void                    BsaveDeffunctionExpression(Environment *,ConstructHeader *,void *);
   static void                    BsaveStorageDeffunctions(Environment *,FILE *);
   static void                    BsaveDeffunctions(Environment *,FILE *);
   static void                    BsaveDeffunction(Environment *,ConstructHeader *,void *);
#endif

   static void                    BloadStorageDeffunctions(Environment *);
   static void                    BloadDeffunctions(Environment *);
   static void                    UpdateDeffunctionModule(Environment *,void *,unsigned long);
   static void                    UpdateDeffunction(Environment *,void *,unsigned long);
   static void                    ClearDeffunctionBload(Environment *);
   static void                    DeallocateDeffunctionBloadData(Environment *);

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/***********************************************************
  NAME         : SetupDeffunctionsBload
  DESCRIPTION  : Initializes data structures and
                   routines for binary loads of deffunctions
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Routines defined and structures initialized
  NOTES        : None
 ***********************************************************/
void SetupDeffunctionsBload(
  Environment *theEnv)
  {
   AllocateEnvironmentData(theEnv,DFFNXBIN_DATA,sizeof(struct deffunctionBinaryData),DeallocateDeffunctionBloadData);
#if BLOAD_AND_BSAVE
   AddBinaryItem(theEnv,"deffunctions",0,BsaveDeffunctionFind,BsaveDeffunctionExpressions,
                             BsaveStorageDeffunctions,BsaveDeffunctions,
                             BloadStorageDeffunctions,BloadDeffunctions,
                             ClearDeffunctionBload);
#else
   AddBinaryItem(theEnv,"deffunctions",0,NULL,NULL,NULL,NULL,
                             BloadStorageDeffunctions,BloadDeffunctions,
                             ClearDeffunctionBload);
#endif
  }

/***********************************************************/
/* DeallocateDeffunctionBloadData: Deallocates environment */
/*    data for the deffunction bsave functionality.        */
/***********************************************************/
static void DeallocateDeffunctionBloadData(
  Environment *theEnv)
  {
   size_t space;

#if (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME)
   space = DeffunctionBinaryData(theEnv)->DeffunctionCount * sizeof(Deffunction);
   if (space != 0) genfree(theEnv,DeffunctionBinaryData(theEnv)->DeffunctionArray,space);

   space =  DeffunctionBinaryData(theEnv)->ModuleCount * sizeof(struct deffunctionModuleData);
   if (space != 0) genfree(theEnv,DeffunctionBinaryData(theEnv)->ModuleArray,space);
#endif
  }

/***************************************************
  NAME         : BloadDeffunctionModuleReference
  DESCRIPTION  : Returns a pointer to the
                 appropriate deffunction module
  INPUTS       : The index of the module
  RETURNS      : A pointer to the module
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
void *BloadDeffunctionModuleReference(
  Environment *theEnv,
  unsigned long theIndex)
  {
   return (void *) &DeffunctionBinaryData(theEnv)->ModuleArray[theIndex];
  }

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

#if BLOAD_AND_BSAVE

/***************************************************************************
  NAME         : BsaveDeffunctionFind
  DESCRIPTION  : For all deffunctions, this routine marks all
                   the needed symbols.
                 Also, it also counts the number of
                   expression structures needed.
                 Also, counts total number of deffunctions.
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : ExpressionCount (a global from BSAVE.C) is incremented
                   for every expression needed
                 Symbols are marked in their structures
  NOTES        : Also sets bsaveIndex for each deffunction (assumes
                   deffunctions will be bsaved in order of binary list)
 ***************************************************************************/
static void BsaveDeffunctionFind(
  Environment *theEnv)
  {
   SaveBloadCount(theEnv,DeffunctionBinaryData(theEnv)->ModuleCount);
   SaveBloadCount(theEnv,DeffunctionBinaryData(theEnv)->DeffunctionCount);
   DeffunctionBinaryData(theEnv)->DeffunctionCount = 0L;

   DeffunctionBinaryData(theEnv)->ModuleCount = GetNumberOfDefmodules(theEnv);
   
   DoForAllConstructs(theEnv,MarkDeffunctionItems,
                      DeffunctionData(theEnv)->DeffunctionModuleIndex,
                      false,NULL);
  }

/***************************************************
  NAME         : MarkDeffunctionItems
  DESCRIPTION  : Marks the needed items for
                 a deffunction bsave
  INPUTS       : 1) The deffunction
                 2) User data buffer (ignored)
  RETURNS      : Nothing useful
  SIDE EFFECTS : Needed items marked
  NOTES        : None
 ***************************************************/
static void MarkDeffunctionItems(
  Environment *theEnv,
  ConstructHeader *theDeffunction,
  void *userBuffer)
  {
#if MAC_XCD
#pragma unused(userBuffer)
#endif

   MarkConstructHeaderNeededItems(theDeffunction,DeffunctionBinaryData(theEnv)->DeffunctionCount++);
   ExpressionData(theEnv)->ExpressionCount += ExpressionSize(((Deffunction *) theDeffunction)->code);
   MarkNeededItems(theEnv,((Deffunction *) theDeffunction)->code);
  }

/***************************************************
  NAME         : BsaveDeffunctionExpressions
  DESCRIPTION  : Writes out all expressions needed
                   by deffunctyions
  INPUTS       : The file pointer of the binary file
  RETURNS      : Nothing useful
  SIDE EFFECTS : File updated
  NOTES        : None
 ***************************************************/
static void BsaveDeffunctionExpressions(
  Environment *theEnv,
  FILE *fp)
  {
   DoForAllConstructs(theEnv,BsaveDeffunctionExpression,DeffunctionData(theEnv)->DeffunctionModuleIndex,
                      false,fp);
  }

/***************************************************
  NAME         : BsaveDeffunctionExpression
  DESCRIPTION  : Saves the needed expressions for
                 a deffunction bsave
  INPUTS       : 1) The deffunction
                 2) Output data file pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Expressions saved
  NOTES        : None
 ***************************************************/
static void BsaveDeffunctionExpression(
  Environment *theEnv,
  ConstructHeader *theDeffunction,
  void *userBuffer)
  {
   BsaveExpression(theEnv,((Deffunction *) theDeffunction)->code,(FILE *) userBuffer);
  }

/***********************************************************
  NAME         : BsaveStorageDeffunctions
  DESCRIPTION  : Writes out number of each type of structure
                   required for deffunctions
                 Space required for counts (unsigned long)
  INPUTS       : File pointer of binary file
  RETURNS      : Nothing useful
  SIDE EFFECTS : Binary file adjusted
  NOTES        : None
 ***********************************************************/
static void BsaveStorageDeffunctions(
  Environment *theEnv,
  FILE *fp)
  {
   size_t space;

   space = sizeof(unsigned long) * 2;
   GenWrite(&space,sizeof(size_t),fp);
   GenWrite(&DeffunctionBinaryData(theEnv)->ModuleCount,sizeof(unsigned long),fp);
   GenWrite(&DeffunctionBinaryData(theEnv)->DeffunctionCount,sizeof(unsigned long),fp);
  }

/*************************************************************************************
  NAME         : BsaveDeffunctions
  DESCRIPTION  : Writes out deffunction in binary format
                 Space required (unsigned long)
                 All deffunctions (sizeof(Deffunction) * Number of deffunctions)
  INPUTS       : File pointer of binary file
  RETURNS      : Nothing useful
  SIDE EFFECTS : Binary file adjusted
  NOTES        : None
 *************************************************************************************/
static void BsaveDeffunctions(
  Environment *theEnv,
  FILE *fp)
  {
   size_t space;
   Defmodule *theModule;
   DeffunctionModuleData *theModuleItem;
   BSAVE_DEFFUNCTION_MODULE dummy_mitem;

   space = ((sizeof(BSAVE_DEFFUNCTION_MODULE) * DeffunctionBinaryData(theEnv)->ModuleCount) +
            (sizeof(BSAVE_DEFFUNCTION) * DeffunctionBinaryData(theEnv)->DeffunctionCount));
   GenWrite(&space,sizeof(size_t),fp);

   /* =================================
      Write out each deffunction module
      ================================= */
   DeffunctionBinaryData(theEnv)->DeffunctionCount = 0L;
   theModule = GetNextDefmodule(theEnv,NULL);
   while (theModule != NULL)
     {
      theModuleItem = (DeffunctionModuleData *)
                      GetModuleItem(theEnv,theModule,FindModuleItem(theEnv,"deffunction")->moduleIndex);
      AssignBsaveDefmdlItemHdrHMVals(&dummy_mitem.header,&theModuleItem->header);
      GenWrite(&dummy_mitem,sizeof(BSAVE_DEFFUNCTION_MODULE),fp);
      theModule = GetNextDefmodule(theEnv,theModule);
     }

   /* ==========================
      Write out each deffunction
      ========================== */
   DoForAllConstructs(theEnv,BsaveDeffunction,DeffunctionData(theEnv)->DeffunctionModuleIndex,
                      false,fp);

   RestoreBloadCount(theEnv,&DeffunctionBinaryData(theEnv)->ModuleCount);
   RestoreBloadCount(theEnv,&DeffunctionBinaryData(theEnv)->DeffunctionCount);
  }

/***************************************************
  NAME         : BsaveDeffunction
  DESCRIPTION  : Bsaves a deffunction
  INPUTS       : 1) The deffunction
                 2) Output data file pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Deffunction saved
  NOTES        : None
 ***************************************************/
static void BsaveDeffunction(
  Environment *theEnv,
  ConstructHeader *theDeffunction,
  void *userBuffer)
  {
   Deffunction *dptr = (Deffunction *) theDeffunction;
   BSAVE_DEFFUNCTION dummy_df;

   AssignBsaveConstructHeaderVals(&dummy_df.header,&dptr->header);
   dummy_df.minNumberOfParameters = dptr->minNumberOfParameters;
   dummy_df.maxNumberOfParameters = dptr->maxNumberOfParameters;
   dummy_df.numberOfLocalVars = dptr->numberOfLocalVars;
   if (dptr->code != NULL)
     {
      dummy_df.code = ExpressionData(theEnv)->ExpressionCount;
      ExpressionData(theEnv)->ExpressionCount += ExpressionSize(dptr->code);
     }
   else
     dummy_df.code = ULONG_MAX;
   GenWrite(&dummy_df,sizeof(BSAVE_DEFFUNCTION),(FILE *) userBuffer);
  }

#endif

/***********************************************************************
  NAME         : BloadStorageDeffunctions
  DESCRIPTION  : This routine space required for deffunction
                   structures and allocates space for them
  INPUTS       : Nothing
  RETURNS      : Nothing useful
  SIDE EFFECTS : Arrays allocated and set
  NOTES        : This routine makes no attempt to reset any pointers
                   within the structures
 ***********************************************************************/
static void BloadStorageDeffunctions(
  Environment *theEnv)
  {
   size_t space;

   GenReadBinary(theEnv,&space,sizeof(size_t));
   if (space == 0L)
     return;
   GenReadBinary(theEnv,&DeffunctionBinaryData(theEnv)->ModuleCount,sizeof(unsigned long));
   GenReadBinary(theEnv,&DeffunctionBinaryData(theEnv)->DeffunctionCount,sizeof(unsigned long));
   if (DeffunctionBinaryData(theEnv)->ModuleCount == 0L)
     {
      DeffunctionBinaryData(theEnv)->ModuleArray = NULL;
      DeffunctionBinaryData(theEnv)->DeffunctionArray = NULL;
      return;
     }

   space = (DeffunctionBinaryData(theEnv)->ModuleCount * sizeof(DeffunctionModuleData));
   DeffunctionBinaryData(theEnv)->ModuleArray = (DeffunctionModuleData *) genalloc(theEnv,space);

   if (DeffunctionBinaryData(theEnv)->DeffunctionCount == 0L)
     {
      DeffunctionBinaryData(theEnv)->DeffunctionArray = NULL;
      return;
     }

   space = (DeffunctionBinaryData(theEnv)->DeffunctionCount * sizeof(Deffunction));
   DeffunctionBinaryData(theEnv)->DeffunctionArray = (Deffunction *) genalloc(theEnv,space);
  }

/*********************************************************************
  NAME         : BloadDeffunctions
  DESCRIPTION  : This routine reads deffunction information from
                   a binary file
                 This routine moves through the deffunction
                   binary array updating pointers
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Pointers reset from array indices
  NOTES        : Assumes all loading is finished
 ********************************************************************/
static void BloadDeffunctions(
  Environment *theEnv)
  {
   size_t space;

   GenReadBinary(theEnv,&space,sizeof(size_t));
   BloadandRefresh(theEnv,DeffunctionBinaryData(theEnv)->ModuleCount,sizeof(BSAVE_DEFFUNCTION_MODULE),UpdateDeffunctionModule);
   BloadandRefresh(theEnv,DeffunctionBinaryData(theEnv)->DeffunctionCount,sizeof(BSAVE_DEFFUNCTION),UpdateDeffunction);
  }

/*******************************************************
  NAME         : UpdateDeffunctionModule
  DESCRIPTION  : Updates deffunction module with binary
                 load data - sets pointers from
                 offset information
  INPUTS       : 1) A pointer to the bloaded data
                 2) The index of the binary array
                    element to update
  RETURNS      : Nothing useful
  SIDE EFFECTS : Deffunction moudle pointers updated
  NOTES        : None
 *******************************************************/
static void UpdateDeffunctionModule(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   BSAVE_DEFFUNCTION_MODULE *bdptr;

   bdptr = (BSAVE_DEFFUNCTION_MODULE *) buf;
   UpdateDefmoduleItemHeaderHM(theEnv,&bdptr->header,&DeffunctionBinaryData(theEnv)->ModuleArray[obji].header,
                             sizeof(Deffunction),DeffunctionBinaryData(theEnv)->DeffunctionArray);
                             
   AssignHashMapSize(theEnv,&DeffunctionBinaryData(theEnv)->ModuleArray[obji].header,bdptr->header.itemCount);
  }

/***************************************************
  NAME         : UpdateDeffunction
  DESCRIPTION  : Updates deffunction with binary
                 load data - sets pointers from
                 offset information
  INPUTS       : 1) A pointer to the bloaded data
                 2) The index of the binary array
                    element to update
  RETURNS      : Nothing useful
  SIDE EFFECTS : Deffunction pointers upadted
  NOTES        : None
 ***************************************************/
static void UpdateDeffunction(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   BSAVE_DEFFUNCTION *bdptr;
   Deffunction *dptr;

   bdptr = (BSAVE_DEFFUNCTION *) buf;
   dptr = &DeffunctionBinaryData(theEnv)->DeffunctionArray[obji];

   UpdateConstructHeader(theEnv,&bdptr->header,&dptr->header,DEFFUNCTION,
                         sizeof(DeffunctionModuleData),DeffunctionBinaryData(theEnv)->ModuleArray,
                         sizeof(Deffunction),DeffunctionBinaryData(theEnv)->DeffunctionArray);

   dptr->code = ExpressionPointer(bdptr->code);
   dptr->busy = 0;
   dptr->executing = 0;
#if DEBUGGING_FUNCTIONS
   dptr->trace = DeffunctionData(theEnv)->WatchDeffunctions;
#endif
   dptr->minNumberOfParameters = bdptr->minNumberOfParameters;
   dptr->maxNumberOfParameters = bdptr->maxNumberOfParameters;
   dptr->numberOfLocalVars = bdptr->numberOfLocalVars;
   
   AddConstructToHashMap(theEnv,&DeffunctionBinaryData(theEnv)->DeffunctionArray[obji].header,
                         DeffunctionBinaryData(theEnv)->DeffunctionArray[obji].header.whichModule);
  }

/***************************************************************
  NAME         : ClearDeffunctionBload
  DESCRIPTION  : Release all binary-loaded deffunction
                   structure arrays
                 Resets deffunction list to NULL
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Memory cleared
  NOTES        : Deffunction name symbol counts decremented
 ***************************************************************/
static void ClearDeffunctionBload(
  Environment *theEnv)
  {
   unsigned long i;
   size_t space;

   space = (sizeof(DeffunctionModuleData) * DeffunctionBinaryData(theEnv)->ModuleCount);
   if (space == 0L)
     return;
     
   for (i = 0; i < DeffunctionBinaryData(theEnv)->ModuleCount; i++)
     { ClearDefmoduleHashMap(theEnv,&DeffunctionBinaryData(theEnv)->ModuleArray[i].header); }

   genfree(theEnv,DeffunctionBinaryData(theEnv)->ModuleArray,space);
   DeffunctionBinaryData(theEnv)->ModuleArray = NULL;
   DeffunctionBinaryData(theEnv)->ModuleCount = 0L;

   for (i = 0 ; i < DeffunctionBinaryData(theEnv)->DeffunctionCount ; i++)
     UnmarkConstructHeader(theEnv,&DeffunctionBinaryData(theEnv)->DeffunctionArray[i].header);
   space = (sizeof(Deffunction) * DeffunctionBinaryData(theEnv)->DeffunctionCount);
   if (space == 0)
     return;
   genfree(theEnv,DeffunctionBinaryData(theEnv)->DeffunctionArray,space);
   DeffunctionBinaryData(theEnv)->DeffunctionArray = NULL;
   DeffunctionBinaryData(theEnv)->DeffunctionCount = 0L;
  }

#endif

