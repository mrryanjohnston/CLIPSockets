   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 7.00  02/06/24             */
   /*                                                     */
   /*            DEFTABLE CONSTRUCTS-TO-C MODULE          */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements the constructs-to-c feature for the   */
/*    deftable construct.                                    */
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

#if DEFTABLE_CONSTRUCT && CONSTRUCT_COMPILER && (! RUN_TIME)

#include <stdio.h>

#include "conscomp.h"
#include "envrnmnt.h"
#include "tabledef.h"

#include "tablecmp.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static bool                    ConstructToCode(Environment *,const char *,const char *,char *,
                                                  unsigned int,FILE *,unsigned int,unsigned int);
   static void                    DeftablesToCode(Environment *,FILE *,Deftable *,
                                                  unsigned int,unsigned int,unsigned int);
   static void                    DeftableModuleToCode(Environment *,FILE *,Defmodule *,
                                                       unsigned int,unsigned int,unsigned int);
   static void                    CloseDeftableFiles(Environment *,FILE *,FILE *,unsigned int);
   static void                    BeforeDeftablesToCode(Environment *);
   static void                    InitDeftablesCode(Environment *,FILE *,unsigned,unsigned int);

/*************************************************************/
/* DeftableCompilerSetup: Initializes the deftable construct */
/*    for use with the constructs-to-c command.              */
/*************************************************************/
void DeftableCompilerSetup(
  Environment *theEnv)
  {
   DeftableData(theEnv)->DeftableCodeItem =
      AddCodeGeneratorItem(theEnv,"deftable",0,BeforeDeftablesToCode,
                           InitDeftablesCode,ConstructToCode,2);
  }

/*************************************************************/
/* BeforeDeftablesToCode: Assigns each deftable a unique ID  */
/*   which will be used for pointer references when the data */
/*   structures are written to a file as C code              */
/*************************************************************/
static void BeforeDeftablesToCode(
  Environment *theEnv)
  {
   MarkConstructBsaveIDs(theEnv,DeftableData(theEnv)->DeftableModuleIndex);
  }

/************************************************/
/* InitDeftablesCode: Writes out initialization */
/*   code for deftables for a run-time module.  */
/************************************************/
static void InitDeftablesCode(
  Environment *theEnv,
  FILE *initFP,
  unsigned imageID,
  unsigned maxIndices)
  {
#if MAC_XCD
#pragma unused(maxIndices)
#pragma unused(imageID)
#pragma unused(theEnv)
#endif
   fprintf(initFP,"   DeftableRunTimeInitialize(theEnv);\n");
  }

/**********************************************************/
/* ConstructToCode: Produces deftable code for a run-time */
/*   module created using the constructs-to-c function.   */
/**********************************************************/
static bool ConstructToCode(
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
   Deftable *theDeftable;
   unsigned int moduleCount = 0, moduleArrayCount = 0, moduleArrayVersion = 1;
   unsigned int deftableArrayCount = 0, deftableArrayVersion = 1;
   FILE *moduleFile = NULL, *deftableFile = NULL;

   /*===============================================*/
   /* Include the appropriate deftable header file. */
   /*===============================================*/

   fprintf(headerFP,"#include \"tabledef.h\"\n");

   /*==================================================================*/
   /* Loop through all the modules and all the deftables writing their */
   /* C code representation to the file as they are traversed.         */
   /*==================================================================*/

   for (theModule = GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = GetNextDefmodule(theEnv,theModule))
     {
      SetCurrentModule(theEnv,theModule);

      moduleFile = OpenFileIfNeeded(theEnv,moduleFile,fileName,pathName,fileNameBuffer,fileID,imageID,&fileCount,
                                    moduleArrayVersion,headerFP,
                                    "struct deftableModule",ModulePrefix(DeftableData(theEnv)->DeftableCodeItem),
                                    false,NULL);

      if (moduleFile == NULL)
        {
         CloseDeftableFiles(theEnv,moduleFile,deftableFile,maxIndices);
         return false;
        }

      DeftableModuleToCode(theEnv,moduleFile,theModule,imageID,maxIndices,moduleCount);
      moduleFile = CloseFileIfNeeded(theEnv,moduleFile,&moduleArrayCount,&moduleArrayVersion,
                                     maxIndices,NULL,NULL);

      /*===================================================*/
      /* Loop through each of the deftable in this module. */
      /*===================================================*/

      for (theDeftable = GetNextDeftable(theEnv,NULL);
           theDeftable != NULL;
           theDeftable = GetNextDeftable(theEnv,theDeftable))
        {
         deftableFile = OpenFileIfNeeded(theEnv,deftableFile,fileName,pathName,fileNameBuffer,fileID,imageID,&fileCount,
                                         deftableArrayVersion,headerFP,
                                         "Deftable",ConstructPrefix(DeftableData(theEnv)->DeftableCodeItem),
                                         false,NULL);
         if (deftableFile == NULL)
           {
            CloseDeftableFiles(theEnv,moduleFile,deftableFile,maxIndices);
            return false;
           }

         DeftablesToCode(theEnv,deftableFile,theDeftable,imageID,maxIndices,moduleCount);
         deftableArrayCount++;
         deftableFile = CloseFileIfNeeded(theEnv,deftableFile,&deftableArrayCount,
                                          &deftableArrayVersion,maxIndices,NULL,NULL);
        }

      moduleCount++;
      moduleArrayCount++;
     }

   CloseDeftableFiles(theEnv,moduleFile,deftableFile,maxIndices);

   return true;
  }

/*********************************************************/
/* CloseDeftableFiles: Closes all of the C files created */
/*   for deftables. Called when an error occurs or when  */
/*   the deftables have all been written to the files.   */
/*********************************************************/
static void CloseDeftableFiles(
  Environment *theEnv,
  FILE *moduleFile,
  FILE *deftableFile,
  unsigned maxIndices)
  {
   unsigned int count = maxIndices;
   unsigned int arrayVersion = 0;

   if (deftableFile != NULL)
     {
      count = maxIndices;
      CloseFileIfNeeded(theEnv,deftableFile,&count,&arrayVersion,maxIndices,NULL,NULL);
     }

   if (moduleFile != NULL)
     {
      count = maxIndices;
      CloseFileIfNeeded(theEnv,moduleFile,&count,&arrayVersion,maxIndices,NULL,NULL);
     }
  }

/**********************************************************/
/* DeftableModuleToCode: Writes the C code representation */
/*   of a single deftable module to the specified file.   */
/**********************************************************/
static void DeftableModuleToCode(
  Environment *theEnv,
  FILE *theFile,
  Defmodule *theModule,
  unsigned int imageID,
  unsigned int maxIndices,
  unsigned int moduleCount)
  {
#if MAC_XCD
#pragma unused(moduleCount)
#endif

   fprintf(theFile,"{");

   ConstructModuleToCode(theEnv,theFile,theModule,imageID,maxIndices,
                                  DeftableData(theEnv)->DeftableModuleIndex,
                                  ConstructPrefix(DeftableData(theEnv)->DeftableCodeItem));

   fprintf(theFile,"}");
  }

/********************************************************/
/* DeftablesToCode: Writes the C code representation of */
/*   a single deftable construct to the specified file. */
/********************************************************/
static void DeftablesToCode(
  Environment *theEnv,
  FILE *theFile,
  Deftable *theDeftable,
  unsigned int imageID,
  unsigned int maxIndices,
  unsigned int moduleCount)
  {
   /*=================*/
   /* Deftable Header */
   /*=================*/

   fprintf(theFile,"{");

   ConstructHeaderToCode(theEnv,theFile,&theDeftable->header,imageID,maxIndices,
                         moduleCount,ModulePrefix(DeftableData(theEnv)->DeftableCodeItem),
                         ConstructPrefix(DeftableData(theEnv)->DeftableCodeItem));

   /*===================*/
   /* Columns and rows. */
   /*===================*/

   fprintf(theFile,",");

   ExpressionToCode(theEnv,theFile,theDeftable->columns);

   fprintf(theFile,",");

   ExpressionToCode(theEnv,theFile,theDeftable->rows);

   fprintf(theFile,",%ld,%ld,%ld,%ld",theDeftable->columnCount,theDeftable->rowCount,
                                      theDeftable->columnHashTableSize,theDeftable->rowHashTableSize);

   fprintf(theFile,"}");
  }

/**************************************************************/
/* DeftableCModuleReference: Writes the C code representation */
/*   of a reference to a deffacts module data structure.      */
/**************************************************************/
void DeftableCModuleReference(
  Environment *theEnv,
  FILE *theFile,
  unsigned long count,
  unsigned int imageID,
  unsigned int maxIndices)
  {
   fprintf(theFile,"MIHS &%s%u_%lu[%lu]",
                      ModulePrefix(DeftableData(theEnv)->DeftableCodeItem),
                      imageID,
                      (count / maxIndices) + 1,
                      (count % maxIndices));
  }

/*****************************************************************/
/* DeftableCConstructReference: Writes the C code representation */
/*   of a reference to a deftable data structure.                */
/*****************************************************************/
void DeftableCConstructReference(
  Environment *theEnv,
  FILE *theFile,
  Deftable *theDeftable,
  unsigned int imageID,
  unsigned int maxIndices)
  {
   if (theDeftable == NULL)
     { fprintf(theFile,"NULL"); }
   else
     {
      fprintf(theFile,"&%s%u_%lu[%lu]",ConstructPrefix(DeftableData(theEnv)->DeftableCodeItem),
                      imageID,
                      (theDeftable->header.bsaveID / maxIndices) + 1,
                      theDeftable->header.bsaveID % maxIndices);
     }

  }

#endif /* DEFTABLE_CONSTRUCT && CONSTRUCT_COMPILER && (! RUN_TIME) */


