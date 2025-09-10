   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 7.00  06/28/24             */
   /*                                                     */
   /*                                                     */
   /*******************************************************/

/*************************************************************/
/* Purpose: Generic Function Construct Compiler Code         */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Added pragmas to remove unused parameter       */
/*            warnings.                                      */
/*                                                           */
/*      6.30: Added support for path name argument to        */
/*            constructs-to-c.                               */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*      7.00: Generic function support for deftemplates.     */
/*                                                           */
/*************************************************************/

/* =========================================
   *****************************************
               EXTERNAL DEFINITIONS
   =========================================
   ***************************************** */
#include "setup.h"

#if DEFGENERIC_CONSTRUCT && CONSTRUCT_COMPILER && (! RUN_TIME)

#include "conscomp.h"
#include "envrnmnt.h"
#include "genrccom.h"
#if DEFRULE_CONSTRUCT
#include "network.h"
#endif
#if DEFTEMPLATE_CONSTRUCT
#include "tmpltcmp.h"
#endif
#if OBJECT_SYSTEM
#include "objcmp.h"
#endif

#include "genrccmp.h"

/* =========================================
   *****************************************
                   CONSTANTS
   =========================================
   ***************************************** */
#define MODULEI      0
#define GENERICI     1
#define METHODI      2
#define RESTRICTIONI 3
#define TYPEI        4

#define SAVE_ITEMS   5

/* =========================================
   *****************************************
               MACROS AND TYPES
   =========================================
   ***************************************** */
#define MethodPrefix()      ArbitraryPrefix(DefgenericData(theEnv)->DefgenericCodeItem,2)
#define RestrictionPrefix() ArbitraryPrefix(DefgenericData(theEnv)->DefgenericCodeItem,3)
#define TypePrefix()        ArbitraryPrefix(DefgenericData(theEnv)->DefgenericCodeItem,4)

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static void                    ReadyDefgenericsForCode(Environment *);
   static bool                    DefgenericsToCode(Environment *,const char *,const char *,char *,
                                                    unsigned int,FILE *,unsigned int,unsigned int);
   static void                    CloseDefgenericFiles(Environment *,FILE *[SAVE_ITEMS],bool [SAVE_ITEMS],
                                                       struct CodeGeneratorFile [SAVE_ITEMS],unsigned int);
   static void                    DefgenericModuleToCode(Environment *,FILE *,Defmodule *,unsigned int,unsigned int);
   static void                    SingleDefgenericToCode(Environment *,FILE *,unsigned int,unsigned int,
                                                         Defgeneric *,unsigned int,unsigned int,unsigned int);
   static void                    MethodToCode(Environment *,FILE *,unsigned int,unsigned int,Defmethod *,
                                               unsigned int,unsigned int,unsigned int);
   static void                    RestrictionToCode(Environment *,FILE *,unsigned int,RESTRICTION *,
                                                    unsigned int,unsigned int);
   static void                    TypeToCode(Environment *,FILE *,unsigned int,struct restrictionType *,unsigned int);
   static void                    InitDefgenericsCode(Environment *,FILE *,unsigned int,unsigned int);

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/***************************************************
  NAME         : SetupGenericsCompiler
  DESCRIPTION  : Initializes the construct compiler
                   item for generic functions
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Code generator item initialized
  NOTES        : None
 ***************************************************/
void SetupGenericsCompiler(
  Environment *theEnv)
  {
   DefgenericData(theEnv)->DefgenericCodeItem = AddCodeGeneratorItem(theEnv,"generics",0,ReadyDefgenericsForCode,
                                             InitDefgenericsCode,DefgenericsToCode,5);
  }

/***************************************************
  NAME         : PrintGenericFunctionReference
  DESCRIPTION  : Prints a reference to the run-time
                 generic array for the construct
                 compiler
  INPUTS       : 1) The file output destination
                 2) A pointer to the generic
                 3) The id of the run-time image
                 4) The maximum number of indices
                    in any array
  RETURNS      : Nothing useful
  SIDE EFFECTS : Reference printed
  NOTES        : None
 ***************************************************/
void PrintGenericFunctionReference(
  Environment *theEnv,
  FILE *fp,
  Defgeneric *gfunc,
  unsigned imageID,
  unsigned maxIndices)
  {
   if (gfunc == NULL)
     fprintf(fp,"NULL");
   else
     fprintf(fp,"&%s%d_%lu[%lul]",ConstructPrefix(DefgenericData(theEnv)->DefgenericCodeItem),imageID,
                                (gfunc->header.bsaveID / maxIndices) + 1,
                                (gfunc->header.bsaveID % maxIndices));
  }

/****************************************************
  NAME         : DefgenericCModuleReference
  DESCRIPTION  : Prints out a reference to a
                 defgeneric module
  INPUTS       : 1) The output file
                 2) The id of the module item
                 3) The id of the image
                 4) The maximum number of elements
                    allowed in an array
  RETURNS      : Nothing useful
  SIDE EFFECTS : Defgeneric module reference printed
  NOTES        : None
 ****************************************************/
void DefgenericCModuleReference(
  Environment *theEnv,
  FILE *theFile,
  unsigned long count,
  unsigned int imageID,
  unsigned int maxIndices)
  {
   fprintf(theFile,"MIHS &%s%u_%lu[%lu]",
                      ModulePrefix(DefgenericData(theEnv)->DefgenericCodeItem),
                      imageID,
                      (count / maxIndices) + 1,
                      (count % maxIndices));
  }

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/***************************************************
  NAME         : ReadyDefgenericsForCode
  DESCRIPTION  : Sets index of generic-functions
                   for use in compiled expressions
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : BsaveIndices set
  NOTES        : None
 ***************************************************/
static void ReadyDefgenericsForCode(
  Environment *theEnv)
  {
   MarkConstructBsaveIDs(theEnv,DefgenericData(theEnv)->DefgenericModuleIndex);
  }

/**************************************************/
/* InitDefgenericsCode: Writes out initialization */
/*   code for defgenerics for a run-time module.  */
/**************************************************/
static void InitDefgenericsCode(
  Environment *theEnv,
  FILE *initFP,
  unsigned int imageID,
  unsigned int maxIndices)
  {
#if MAC_XCD
#pragma unused(maxIndices)
#pragma unused(imageID)
#pragma unused(theEnv)
#endif
   fprintf(initFP,"   DefgenericRunTimeInitialize(theEnv);\n");
  }

/*******************************************************
  NAME         : DefgenericsToCode
  DESCRIPTION  : Writes out static array code for
                   generic functions, methods, etc.
  INPUTS       : 1) The base name of the construct set
                 2) The base id for this construct
                 3) The file pointer for the header file
                 4) The base id for the construct set
                 5) The max number of indices allowed
                    in an array
  RETURNS      : False on errors,
                 true if generic functions written
  SIDE EFFECTS : Code written to files
  NOTES        : None
 *******************************************************/
static bool DefgenericsToCode(
  Environment *theEnv,
  const char *fileName,
  const char *pathName,
  char *fileNameBuffer,
  unsigned int fileID,
  FILE *headerFP,
  unsigned int imageID,
  unsigned int maxIndices)
  {
   unsigned int fileCount = 1;
   Defmodule *theModule;
   Defgeneric *theDefgeneric;
   Defmethod *theMethod;
   RESTRICTION *theRestriction;
   short i,j,k;
   unsigned int moduleCount = 0;
   unsigned int itemArrayCounts[SAVE_ITEMS];
   unsigned int itemArrayVersions[SAVE_ITEMS];
   FILE *itemFiles[SAVE_ITEMS];
   bool itemReopenFlags[SAVE_ITEMS];
   struct CodeGeneratorFile itemCodeFiles[SAVE_ITEMS];

   for (i = 0 ; i < SAVE_ITEMS ; i++)
     {
      itemArrayCounts[i] = 0;
      itemArrayVersions[i] = 1;
      itemFiles[i] = NULL;
      itemReopenFlags[i] = false;
      itemCodeFiles[i].filePrefix = NULL;
      itemCodeFiles[i].pathName = pathName;
      itemCodeFiles[i].fileNameBuffer = fileNameBuffer;
     }

   /* ===========================================
      Include the appropriate generic header file
      =========================================== */
   fprintf(headerFP,"#include \"genrcfun.h\"\n");

   /* =============================================================
      Loop through all the modules and all the defgenerics writing
      their C code representation to the file as they are traversed
      ============================================================= */
   theModule = GetNextDefmodule(theEnv,NULL);

   while (theModule != NULL)
     {
      SetCurrentModule(theEnv,theModule);

      itemFiles[MODULEI] =
         OpenFileIfNeeded(theEnv,itemFiles[MODULEI],fileName,pathName,fileNameBuffer,fileID,imageID,&fileCount,
                          itemArrayVersions[MODULEI],headerFP,
                          "DEFGENERIC_MODULE",ModulePrefix(DefgenericData(theEnv)->DefgenericCodeItem),
                          itemReopenFlags[MODULEI],&itemCodeFiles[MODULEI]);
      if (itemFiles[MODULEI] == NULL)
        goto GenericCodeError;

      DefgenericModuleToCode(theEnv,itemFiles[MODULEI],theModule,imageID,maxIndices);
      itemFiles[MODULEI] =
          CloseFileIfNeeded(theEnv,itemFiles[MODULEI],&itemArrayCounts[MODULEI],
                            &itemArrayVersions[MODULEI],maxIndices,
                            &itemReopenFlags[MODULEI],&itemCodeFiles[MODULEI]);

      theDefgeneric = GetNextDefgeneric(theEnv,NULL);

      while (theDefgeneric != NULL)
        {
         itemFiles[GENERICI] =
            OpenFileIfNeeded(theEnv,itemFiles[GENERICI],fileName,pathName,fileNameBuffer,fileID,imageID,&fileCount,
                             itemArrayVersions[GENERICI],headerFP,
                             "Defgeneric",ConstructPrefix(DefgenericData(theEnv)->DefgenericCodeItem),
                             itemReopenFlags[GENERICI],&itemCodeFiles[GENERICI]);
         if (itemFiles[GENERICI] == NULL)
           goto GenericCodeError;

         SingleDefgenericToCode(theEnv,itemFiles[GENERICI],imageID,maxIndices,theDefgeneric,
                                moduleCount,itemArrayVersions[METHODI],
                                itemArrayCounts[METHODI]);
         itemArrayCounts[GENERICI]++;
         itemFiles[GENERICI] =
           CloseFileIfNeeded(theEnv,itemFiles[GENERICI],&itemArrayCounts[GENERICI],
                             &itemArrayVersions[GENERICI],maxIndices,
                             &itemReopenFlags[GENERICI],&itemCodeFiles[GENERICI]);
         if (theDefgeneric->mcnt > 0)
           {

            /* ===========================================
               Make sure that all methods for a particular
               generic function go into the same array
               =========================================== */
            itemFiles[METHODI] =
                OpenFileIfNeeded(theEnv,itemFiles[METHODI],fileName,pathName,fileNameBuffer,fileID,imageID,&fileCount,
                                 itemArrayVersions[METHODI],headerFP,
                                 "Defmethod",MethodPrefix(),
                                 itemReopenFlags[METHODI],&itemCodeFiles[METHODI]);
            if (itemFiles[METHODI] == NULL)
              goto GenericCodeError;

            for (i = 0 ; i < theDefgeneric->mcnt ; i++)
              {
               theMethod = &theDefgeneric->methods[i];
               if (i > 0)
                 fprintf(itemFiles[METHODI],",\n");
               MethodToCode(theEnv,itemFiles[METHODI],imageID,maxIndices,theMethod,moduleCount,
                            itemArrayVersions[RESTRICTIONI],itemArrayCounts[RESTRICTIONI]);
               if (theMethod->restrictionCount > 0)
                 {
                  /* ========================================
                     Make sure that all restrictions for a
                     particular method go into the same array
                     ======================================== */
                  itemFiles[RESTRICTIONI] =
                     OpenFileIfNeeded(theEnv,itemFiles[RESTRICTIONI],fileName,pathName,fileNameBuffer,fileID,
                                      imageID,&fileCount,
                                      itemArrayVersions[RESTRICTIONI],headerFP,
                                      "RESTRICTION",RestrictionPrefix(),
                                      itemReopenFlags[RESTRICTIONI],&itemCodeFiles[RESTRICTIONI]);
                  if (itemFiles[RESTRICTIONI] == NULL)
                    goto GenericCodeError;
                  for (j = 0 ; j < theMethod->restrictionCount ; j++)
                    {
                     theRestriction = &theMethod->restrictions[j];
                     if (j > 0)
                       fprintf(itemFiles[RESTRICTIONI],",\n");
                     RestrictionToCode(theEnv,itemFiles[RESTRICTIONI],imageID,theRestriction,
                                       itemArrayVersions[TYPEI],itemArrayCounts[TYPEI]);

                     if (theRestriction->tcnt > 0)
                       {
                        /* =========================================
                           Make sure that all types for a particular
                           restriction go into the same array
                           ========================================= */
                        itemFiles[TYPEI] =
                           OpenFileIfNeeded(theEnv,itemFiles[TYPEI],fileName,pathName,fileNameBuffer,fileID,
                                            imageID,&fileCount,
                                            itemArrayVersions[TYPEI],headerFP,
                                            "RESTRICTION_TYPE",TypePrefix(),
                                            itemReopenFlags[TYPEI],&itemCodeFiles[TYPEI]);
                        if (itemFiles[TYPEI] == NULL)
                          goto GenericCodeError;
                        for (k = 0 ; k < theRestriction->tcnt ; k++)
                          {
                           if (k > 0)
                             fprintf(itemFiles[TYPEI],",\n");
                           TypeToCode(theEnv,itemFiles[TYPEI],imageID,
                                      &theRestriction->types[k],maxIndices);
                          }
                        itemArrayCounts[TYPEI] += theRestriction->tcnt;
                        itemFiles[TYPEI] =
                           CloseFileIfNeeded(theEnv,itemFiles[TYPEI],&itemArrayCounts[TYPEI],
                                             &itemArrayVersions[TYPEI],maxIndices,
                                             &itemReopenFlags[TYPEI],&itemCodeFiles[TYPEI]);
                       }
                    }
                  itemArrayCounts[RESTRICTIONI] += theMethod->restrictionCount;
                  itemFiles[RESTRICTIONI] =
                     CloseFileIfNeeded(theEnv,itemFiles[RESTRICTIONI],&itemArrayCounts[RESTRICTIONI],
                                       &itemArrayVersions[RESTRICTIONI],maxIndices,
                                       &itemReopenFlags[RESTRICTIONI],&itemCodeFiles[RESTRICTIONI]);
                 }
              }
            itemArrayCounts[METHODI] += theDefgeneric->mcnt;
            itemFiles[METHODI] =
               CloseFileIfNeeded(theEnv,itemFiles[METHODI],&itemArrayCounts[METHODI],
                                 &itemArrayVersions[METHODI],maxIndices,
                                 &itemReopenFlags[METHODI],&itemCodeFiles[METHODI]);
           }
         theDefgeneric = GetNextDefgeneric(theEnv,theDefgeneric);
        }

      theModule = GetNextDefmodule(theEnv,theModule);
      moduleCount++;
      itemArrayCounts[MODULEI]++;
     }
   CloseDefgenericFiles(theEnv,itemFiles,itemReopenFlags,itemCodeFiles,maxIndices);
   return true;

GenericCodeError:
   CloseDefgenericFiles(theEnv,itemFiles,itemReopenFlags,itemCodeFiles,maxIndices);
   return false;
  }

/******************************************************
  NAME         : CloseDefgenericFiles
  DESCRIPTION  : Closes construct compiler files
                  for defgeneric structures
  INPUTS       : 1) An array containing all the
                    pertinent file pointers
                 2) An array containing all the
                    pertinent file reopen flags
                 3) An array containing all the
                    pertinent file name/id/version info
                 4) The maximum number of indices
                    allowed in an array
  RETURNS      : Nothing useful
  SIDE EFFECTS : Files closed
  NOTES        : None
 *****************************************************/
static void CloseDefgenericFiles(
  Environment *theEnv,
  FILE *itemFiles[SAVE_ITEMS],
  bool itemReopenFlags[SAVE_ITEMS],
  struct CodeGeneratorFile itemCodeFiles[SAVE_ITEMS],
  unsigned int maxIndices)
  {
   unsigned int count = maxIndices;
   unsigned int arrayVersion = 0;
   unsigned int i;

   for (i = 0 ; i < SAVE_ITEMS ; i++)
     {
      count = maxIndices;
      itemFiles[i] = CloseFileIfNeeded(theEnv,itemFiles[i],&count,&arrayVersion,
                                       maxIndices,&itemReopenFlags[i],
                                       &itemCodeFiles[i]);
     }
  }

/***************************************************
  NAME         : DefgenericModuleToCode
  DESCRIPTION  : Writes out the C values for a
                 defgeneric module item
  INPUTS       : 1) The output file
                 2) The module for the defgenerics
                 3) The compile image id
                 4) The maximum number of elements
                    in an array
  RETURNS      : Nothing useful
  SIDE EFFECTS : Defgeneric module item written
  NOTES        : None
 ***************************************************/
static void DefgenericModuleToCode(
  Environment *theEnv,
  FILE *theFile,
  Defmodule *theModule,
  unsigned int imageID,
  unsigned int maxIndices)
  {
   fprintf(theFile,"{");
   ConstructModuleToCode(theEnv,theFile,theModule,imageID,maxIndices,
                         DefgenericData(theEnv)->DefgenericModuleIndex,ConstructPrefix(DefgenericData(theEnv)->DefgenericCodeItem));
   fprintf(theFile,"}");
  }

/****************************************************************
  NAME         : SingleDefgenericToCode
  DESCRIPTION  : Writes out a single defgeneric's
                 data to the file
  INPUTS       : 1)  The output file
                 2)  The compile image id
                 3)  The maximum number of
                     elements in an array
                 4)  The defgeneric
                 5)  The module index
                 6)  The partition holding the
                     generic methods
                 7) The relative index of the
                    generics methods in the partition
  RETURNS      : Nothing useful
  SIDE EFFECTS : Defgeneric data written
  NOTES        : None
 ***************************************************************/
static void SingleDefgenericToCode(
  Environment *theEnv,
  FILE *theFile,
  unsigned int imageID,
  unsigned int maxIndices,
  Defgeneric *theDefgeneric,
  unsigned int moduleCount,
  unsigned int methodArrayVersion,
  unsigned int methodArrayCount)
  {
   /* ==================
      Defgeneric Header
      ================== */
   fprintf(theFile,"{");
   ConstructHeaderToCode(theEnv,theFile,&theDefgeneric->header,imageID,maxIndices,moduleCount,
                         ModulePrefix(DefgenericData(theEnv)->DefgenericCodeItem),
                         ConstructPrefix(DefgenericData(theEnv)->DefgenericCodeItem));

   /* =========================
      Defgeneric specific data
      ========================= */
   fprintf(theFile,",0,0,");
   if (theDefgeneric->methods == NULL)
     fprintf(theFile,"NULL");
   else
     {
      fprintf(theFile,"&%s%d_%d[%d]",MethodPrefix(),imageID,
                      methodArrayVersion,methodArrayCount);
     }
   fprintf(theFile,",%hd,0}",theDefgeneric->mcnt);
  }

/****************************************************************
  NAME         : MethodToCode
  DESCRIPTION  : Writes out a single method's
                 data to the file
  INPUTS       : 1)  The output file
                 2)  The compile image id
                 3)  The method
                 4)  The partition holding the
                     method restrictions
                 5) The relative index of the
                    method restrictions in the partition
  RETURNS      : Nothing useful
  SIDE EFFECTS : Method data written
  NOTES        : None
 ***************************************************************/
static void MethodToCode(
  Environment *theEnv,
  FILE *theFile,
  unsigned int imageID,
  unsigned int maxIndices,
  Defmethod *theMethod,
  unsigned int moduleCount,
  unsigned int restrictionArrayVersion,
  unsigned int restrictionArrayCount)
  {
   fprintf(theFile,"{");
   ConstructHeaderToCode(theEnv,theFile,&theMethod->header,imageID,maxIndices,moduleCount,
                         ModulePrefix(DefgenericData(theEnv)->DefgenericCodeItem),
                         "");

   fprintf(theFile,",%u,0,%hd,%hu,%hu,%hd,%u,0,",
                   theMethod->index,theMethod->restrictionCount,
                   theMethod->minRestrictions,theMethod->maxRestrictions,
                   theMethod->localVarCount,theMethod->system);
   if (theMethod->restrictions == NULL)
     fprintf(theFile,"NULL,");
   else
     fprintf(theFile,"&%s%d_%d[%d],",RestrictionPrefix(),imageID,
                                     restrictionArrayVersion,restrictionArrayCount);
   ExpressionToCode(theEnv,theFile,theMethod->actions);
   fprintf(theFile,"}");
  }

/****************************************************************
  NAME         : RestrictionToCode
  DESCRIPTION  : Writes out a single restriction's
                 data to the file
  INPUTS       : 1)  The output file
                 2)  The compile image id
                 3)  The restriction
                 4)  The partition holding the
                     restriction types
                 5) The relative index of the
                    restriction types in the partition
  RETURNS      : Nothing useful
  SIDE EFFECTS : Restriction data written
  NOTES        : None
 ***************************************************************/
static void RestrictionToCode(
  Environment *theEnv,
  FILE *theFile,
  unsigned int imageID,
  RESTRICTION *theRestriction,
  unsigned int typeArrayVersion,
  unsigned int typeArrayCount)
  {
   fprintf(theFile,"{");
   if (theRestriction->types == NULL)
     fprintf(theFile,"NULL,");
   else
     fprintf(theFile,"&%s%d_%d[%d],",TypePrefix(),imageID,
                                     typeArrayVersion,typeArrayCount);
   ExpressionToCode(theEnv,theFile,theRestriction->query);
   fprintf(theFile,",%hd}",theRestriction->tcnt);
  }

/****************************************************************
  NAME         : TypeToCode
  DESCRIPTION  : Writes out a single type's
                 data to the file
  INPUTS       : 1)  The output file
                 2)  The compile image id
                 3)  The type
  RETURNS      : Nothing useful
  SIDE EFFECTS : Type data written
  NOTES        : None
 ***************************************************************/
static void TypeToCode(
  Environment *theEnv,
  FILE *theFile,
  unsigned int imageID,
  struct restrictionType *theType,
  unsigned int maxIndices)
  {
   fprintf(theFile,"{%d,",theType->type);
   
   switch (theType->type)
     {
#if OBJECT_SYSTEM
      case DEFCLASS_PTR:
        PrintClassReference(theEnv,theFile,theType->theClass,imageID,maxIndices);
        break;
#else
#if MAC_XCD
#pragma unused(imageID)
#pragma unused(maxIndices)
#endif
      case INTEGER_TYPE:
        PrintIntegerReference(theEnv,theFile,theType->theInteger);
        break;
#endif
#if DEFTEMPLATE_CONSTRUCT
      case DEFTEMPLATE_PTR:
        DeftemplateCConstructReference(theEnv,theFile,theType->theTemplate,imageID,maxIndices);
        break;
#endif
     }

   fprintf(theFile,"}");
  }

#endif
