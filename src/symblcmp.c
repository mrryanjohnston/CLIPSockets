   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 7.00  09/18/24             */
   /*                                                     */
   /*           SYMBOL_TYPE CONSTRUCT COMPILER MODULE          */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements the constructs-to-c feature for       */
/*   atomic data values: symbols, integers, floats, and      */
/*   bit maps.                                               */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*      Barry Cameron                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Added environment parameter to GenClose.       */
/*                                                           */
/*            Corrected code to remove compiler warnings.    */
/*                                                           */
/*      6.30: Added support for path name argument to        */
/*            constructs-to-c.                               */
/*                                                           */
/*            Support for long long integers.                */
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
/*      7.00: Hash value stored with lexemes.                */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if CONSTRUCT_COMPILER && (! RUN_TIME)

#include <stdio.h>
#include <string.h>

#include "argacces.h"
#include "constant.h"
#include "conscomp.h"
#include "constrct.h"
#include "cstrncmp.h"
#include "cstrccom.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "memalloc.h"
#include "prntutil.h"
#include "router.h"
#include "symbol.h"
#include "sysdep.h"
#include "utility.h"

#include "symblcmp.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static unsigned int                SymbolHashNodesToCode(Environment *,const char *,const char *,char *,unsigned int);
   static unsigned int                BitMapHashNodesToCode(Environment *,const char *,const char *,char *,unsigned int);
   static unsigned int                BitMapValuesToCode(Environment *,const char *,const char *, char *,unsigned int);
   static unsigned int                FloatHashNodesToCode(Environment *,const char *,const char *,char *,unsigned int);
   static unsigned int                IntegerHashNodesToCode(Environment *,const char *,const char *,char *,unsigned int);
   static int                         HashTablesToCode(Environment *,const char *,const char *,char *);
   static void                        PrintCString(FILE *,const char *);

/**************************************************************/
/* AtomicValuesToCode: Driver routine for generating the code */
/*  used by the symbol, integer, float, and bit map tables.   */
/**************************************************************/
void AtomicValuesToCode(
  Environment *theEnv,
  const char *fileName,
  const char *pathName,
  char *fileNameBuffer)
  {
   unsigned int version;

   SetAtomicValueIndices(theEnv,true);

   HashTablesToCode(theEnv,fileName,pathName,fileNameBuffer);

   version = SymbolHashNodesToCode(theEnv,fileName,pathName,fileNameBuffer,5);
   version = FloatHashNodesToCode(theEnv,fileName,pathName,fileNameBuffer,version);
   version = IntegerHashNodesToCode(theEnv,fileName,pathName,fileNameBuffer,version);
   version = BitMapHashNodesToCode(theEnv,fileName,pathName,fileNameBuffer,version);
   BitMapValuesToCode(theEnv,fileName,pathName,fileNameBuffer,version);
  }

/*****************************************************/
/* SymbolHashNodesToCode: Produces the code for the  */
/*   symbol hash table entries for a run-time module */
/*   created using the constructs-to-c function.     */
/*****************************************************/
static unsigned int SymbolHashNodesToCode(
  Environment *theEnv,
  const char *fileName,
  const char *pathName,
  char *fileNameBuffer,
  unsigned int version)
  {
   unsigned long i, j;
   CLIPSLexeme *hashPtr;
   unsigned int count;
   unsigned int numberOfEntries;
   CLIPSLexeme **symbolTable;
   bool newHeader = true;
   int arrayVersion = 1;
   FILE *fp;

   /*====================================*/
   /* Count the total number of entries. */
   /*====================================*/

   symbolTable = GetSymbolTable(theEnv);
   count = numberOfEntries = 0;

   for (i = 0; i < SYMBOL_HASH_SIZE; i++)
     {
      for (hashPtr = symbolTable[i];
           hashPtr != NULL;
           hashPtr = hashPtr->next)
        { numberOfEntries++; }
     }

   if (numberOfEntries == 0) return version;

   for (i = 1; i <= (numberOfEntries / ConstructCompilerData(theEnv)->MaxIndices) + 1 ; i++)
     { fprintf(ConstructCompilerData(theEnv)->HeaderFP,"extern CLIPSLexeme S%d_%ld[];\n",ConstructCompilerData(theEnv)->ImageID,i); }

   /*==================*/
   /* Create the file. */
   /*==================*/

   if ((fp = NewCFile(theEnv,fileName,pathName,fileNameBuffer,1,version,false)) == NULL) return version;

   /*===================*/
   /* List the entries. */
   /*===================*/

   j = 0;

   for (i = 0; i < SYMBOL_HASH_SIZE; i++)
     {
      for (hashPtr = symbolTable[i];
           hashPtr != NULL;
           hashPtr = hashPtr->next)
        {
         if (newHeader)
           {
            fprintf(fp,"CLIPSLexeme S%d_%d[] = {\n",ConstructCompilerData(theEnv)->ImageID,arrayVersion);
            newHeader = false;
           }

         if (hashPtr->header.type == SYMBOL_TYPE)
           { fprintf(fp,"{{SYMBOL_TYPE},"); }
         else if (hashPtr->header.type == STRING_TYPE)
           { fprintf(fp,"{{STRING_TYPE},"); }
         else
           { fprintf(fp,"{{INSTANCE_NAME_TYPE},"); }
           
         if (hashPtr->next == NULL)
           { fprintf(fp,"NULL,"); }
         else
           {
            if ((j + 1) >= (unsigned long) ConstructCompilerData(theEnv)->MaxIndices)
              { fprintf(fp,"&S%d_%d[%d],",ConstructCompilerData(theEnv)->ImageID,arrayVersion + 1,0); }
            else
              { fprintf(fp,"&S%d_%d[%ld],",ConstructCompilerData(theEnv)->ImageID,arrayVersion,j + 1); }
           }

         fprintf(fp,"%ld,1,0,0,%ld,",hashPtr->count + 1,i);
         PrintCString(fp,hashPtr->contents);
         fprintf(fp,",%zuU",hashPtr->hv);

         count++;
         j++;

         if ((count == numberOfEntries) || (j >= ConstructCompilerData(theEnv)->MaxIndices))
           {
            fprintf(fp,"}};\n");
            GenClose(theEnv,fp);
            j = 0;
            arrayVersion++;
            version++;
            if (count < numberOfEntries)
              {
               if ((fp = NewCFile(theEnv,fileName,pathName,fileNameBuffer,1,version,false)) == NULL)
                 { return 0; }
               newHeader = true;
              }
           }
         else
           { fprintf(fp,"},\n"); }
        }
     }

   return version;
  }

/******************************************************/
/* BitMapHashNodesToCode: Produces the code for the   */
/*   bit map hash table entries for a run-time module */
/*   created using the constructs-to-c function.      */
/******************************************************/
static unsigned int BitMapHashNodesToCode(
  Environment *theEnv,
  const char *fileName,
  const char *pathName,
  char *fileNameBuffer,
  unsigned int version)
  {
   unsigned int i, j;
   CLIPSBitMap *hashPtr;
   unsigned int count;
   unsigned int numberOfEntries;
   CLIPSBitMap **bitMapTable;
   bool newHeader = true;
   unsigned int arrayVersion = 1;
   FILE *fp;
   unsigned int longsReqdPartition = 1, longsReqdPartitionCount = 0;

   /*====================================*/
   /* Count the total number of entries. */
   /*====================================*/

   bitMapTable = GetBitMapTable(theEnv);
   count = numberOfEntries = 0;

   for (i = 0; i < BITMAP_HASH_SIZE; i++)
     {
      for (hashPtr = bitMapTable[i];
           hashPtr != NULL;
           hashPtr = hashPtr->next)
        { numberOfEntries++; }
     }

   if (numberOfEntries == 0) return version;

   for (i = 1; i <= (numberOfEntries / ConstructCompilerData(theEnv)->MaxIndices) + 1 ; i++)
     { fprintf(ConstructCompilerData(theEnv)->HeaderFP,"extern struct clipsBitMap B%d_%d[];\n",ConstructCompilerData(theEnv)->ImageID,i); }

   /*==================*/
   /* Create the file. */
   /*==================*/

   if ((fp = NewCFile(theEnv,fileName,pathName,fileNameBuffer,1,version,false)) == NULL) return version;

   /*===================*/
   /* List the entries. */
   /*===================*/

   j = 0;

   for (i = 0; i < BITMAP_HASH_SIZE; i++)
     {
      for (hashPtr = bitMapTable[i];
           hashPtr != NULL;
           hashPtr = hashPtr->next)
        {
         if (newHeader)
           {
            fprintf(fp,"struct clipsBitMap B%d_%d[] = {\n",ConstructCompilerData(theEnv)->ImageID,arrayVersion);
            newHeader = false;
           }
           
         fprintf(fp,"{{BITMAP_TYPE},");
         
         if (hashPtr->next == NULL)
           { fprintf(fp,"NULL,"); }
         else
           {
            if ((j + 1) >= ConstructCompilerData(theEnv)->MaxIndices)
              { fprintf(fp,"&B%d_%d[%d],",ConstructCompilerData(theEnv)->ImageID,arrayVersion + 1,0); }
            else
              { fprintf(fp,"&B%d_%d[%d],",ConstructCompilerData(theEnv)->ImageID,arrayVersion,j + 1); }
           }

         fprintf(fp,"%ld,1,0,0,%d,(char *) &L%d_%d[%d],%d",
                     hashPtr->count + 1,i,
                     ConstructCompilerData(theEnv)->ImageID,longsReqdPartition,longsReqdPartitionCount,
                     hashPtr->size);

         longsReqdPartitionCount += (hashPtr->size / sizeof(unsigned long));
         if ((hashPtr->size % sizeof(unsigned long)) != 0)
           longsReqdPartitionCount++;
         if (longsReqdPartitionCount >= ConstructCompilerData(theEnv)->MaxIndices)
           {
            longsReqdPartitionCount = 0;
            longsReqdPartition++;
           }

         count++;
         j++;

         if ((count == numberOfEntries) || (j >= ConstructCompilerData(theEnv)->MaxIndices))
           {
            fprintf(fp,"}};\n");
            GenClose(theEnv,fp);
            j = 0;
            arrayVersion++;
            version++;
            if (count < numberOfEntries)
              {
               if ((fp = NewCFile(theEnv,fileName,pathName,fileNameBuffer,1,version,false)) == NULL)
                 { return 0; }
               newHeader = true;
              }
           }
         else
           { fprintf(fp,"},\n"); }
        }
     }

   return version;
  }

/*****************************************************/
/* BitMapValuesToCode: Produces the code for the bit */
/*   map strings for a run-time module created using */
/*   the constructs-to-c function.                   */
/*****************************************************/
static unsigned int BitMapValuesToCode(
  Environment *theEnv,
  const char *fileName,
  const char *pathName,
  char *fileNameBuffer,
  unsigned int version)
  {
   unsigned int i, j, k;
   unsigned l;
   CLIPSBitMap *hashPtr;
   unsigned int count;
   unsigned int numberOfEntries;
   CLIPSBitMap **bitMapTable;
   bool newHeader = true;
   unsigned int arrayVersion = 1;
   FILE *fp;
   unsigned long tmpLong;
   unsigned int longsReqd;

   /*====================================*/
   /* Count the total number of entries. */
   /*====================================*/

   bitMapTable = GetBitMapTable(theEnv);
   count = numberOfEntries = 0;

   for (i = 0; i < BITMAP_HASH_SIZE; i++)
     {
      for (hashPtr = bitMapTable[i];
           hashPtr != NULL;
           hashPtr = hashPtr->next)
        {
         numberOfEntries += (hashPtr->size / sizeof(unsigned long));
         if ((hashPtr->size % sizeof(unsigned long)) != 0)
           { numberOfEntries++; }
        }
     }

   if (numberOfEntries == 0) return version;

   for (i = 1; i <= (numberOfEntries / ConstructCompilerData(theEnv)->MaxIndices) + 1 ; i++)
     { fprintf(ConstructCompilerData(theEnv)->HeaderFP,"extern unsigned long L%d_%d[];\n",ConstructCompilerData(theEnv)->ImageID,i); }

   /*==================*/
   /* Create the file. */
   /*==================*/

   if ((fp = NewCFile(theEnv,fileName,pathName,fileNameBuffer,1,version,false)) == NULL) return version;

   /*===================*/
   /* List the entries. */
   /*===================*/

   j = 0;

   for (i = 0; i < BITMAP_HASH_SIZE; i++)
     {
      for (hashPtr = bitMapTable[i];
           hashPtr != NULL;
           hashPtr = hashPtr->next)
        {
         if (newHeader)
           {
            fprintf(fp,"unsigned long L%d_%d[] = {\n",ConstructCompilerData(theEnv)->ImageID,arrayVersion);
            newHeader = false;
           }

         longsReqd = (hashPtr->size / sizeof(unsigned long));
         if ((hashPtr->size % sizeof(unsigned long)) != 0)
           longsReqd++;

         for (k = 0 ; k < longsReqd ; k++)
           {
            if (k > 0)
              fprintf(fp,",");
            tmpLong = 0L;
            for (l = 0 ;
                 ((l < sizeof(unsigned long)) &&
                 (((k * sizeof(unsigned long)) + l) < (size_t) hashPtr->size)) ;
                 l++)
              ((char *) &tmpLong)[l] = hashPtr->contents[(k * sizeof(unsigned long)) + l];
            fprintf(fp,"0x%lxL",tmpLong);
           }

         count += longsReqd;
         j += longsReqd;

         if ((count == numberOfEntries) || (j >= ConstructCompilerData(theEnv)->MaxIndices))
           {
            fprintf(fp,"};\n");
            GenClose(theEnv,fp);
            j = 0;
            arrayVersion++;
            version++;
            if (count < numberOfEntries)
              {
               if ((fp = NewCFile(theEnv,fileName,pathName,fileNameBuffer,1,version,false)) == NULL)
                 { return 0; }
               newHeader = true;
              }
           }
         else
           { fprintf(fp,",\n"); }
        }
     }

   return version;
  }

/****************************************************/
/* FloatHashNodesToCode: Produces the code for the  */
/*   float hash table entries for a run-time module */
/*   created using the constructs-to-c function.    */
/****************************************************/
static unsigned int FloatHashNodesToCode(
  Environment *theEnv,
  const char *fileName,
  const char *pathName,
  char *fileNameBuffer,
  unsigned int version)
  {
   unsigned int i, j;
   CLIPSFloat *hashPtr;
   unsigned int count;
   unsigned int numberOfEntries;
   CLIPSFloat **floatTable;
   bool newHeader = true;
   FILE *fp;
   unsigned int arrayVersion = 1;

   /*====================================*/
   /* Count the total number of entries. */
   /*====================================*/

   floatTable = GetFloatTable(theEnv);
   count = numberOfEntries = 0;

   for (i = 0; i < FLOAT_HASH_SIZE; i++)
     {
      for (hashPtr = floatTable[i];
           hashPtr != NULL;
           hashPtr = hashPtr->next)
        { numberOfEntries++; }
     }

   if (numberOfEntries == 0) return version;

   for (i = 1; i <= (numberOfEntries / ConstructCompilerData(theEnv)->MaxIndices) + 1 ; i++)
     { fprintf(ConstructCompilerData(theEnv)->HeaderFP,"extern CLIPSFloat F%d_%d[];\n",ConstructCompilerData(theEnv)->ImageID,i); }

   /*==================*/
   /* Create the file. */
   /*==================*/

   if ((fp = NewCFile(theEnv,fileName,pathName,fileNameBuffer,1,version,false)) == NULL) return version;

   /*===================*/
   /* List the entries. */
   /*===================*/

   j = 0;

   for (i = 0; i < FLOAT_HASH_SIZE; i++)
     {
      for (hashPtr = floatTable[i];
           hashPtr != NULL;
           hashPtr = hashPtr->next)
        {
         if (newHeader)
           {
            fprintf(fp,"CLIPSFloat F%d_%d[] = {\n",ConstructCompilerData(theEnv)->ImageID,arrayVersion);
            newHeader = false;
           }
           
         fprintf(fp,"{{FLOAT_TYPE},");
         
         if (hashPtr->next == NULL)
           { fprintf(fp,"NULL,"); }
         else
           {
            if ((j + 1) >= ConstructCompilerData(theEnv)->MaxIndices)
              { fprintf(fp,"&F%d_%d[%d],",ConstructCompilerData(theEnv)->ImageID,arrayVersion + 1,0); }
            else
              { fprintf(fp,"&F%d_%d[%d],",ConstructCompilerData(theEnv)->ImageID,arrayVersion,j + 1); }
           }

         fprintf(fp,"%ld,1,0,0,%d,",hashPtr->count + 1,i);
         fprintf(fp,"%s",FloatToString(theEnv,hashPtr->contents));

         count++;
         j++;

         if ((count == numberOfEntries) || (j >= ConstructCompilerData(theEnv)->MaxIndices))
           {
            fprintf(fp,"}};\n");
            GenClose(theEnv,fp);
            j = 0;
            version++;
            arrayVersion++;
            if (count < numberOfEntries)
              {
               if ((fp = NewCFile(theEnv,fileName,pathName,fileNameBuffer,1,version,false)) == NULL)
                 { return 0; }
               newHeader = true;
              }
           }
         else
           { fprintf(fp,"},\n"); }
        }
     }

   return version;
  }

/******************************************************/
/* IntegerHashNodesToCode: Produces the code for the  */
/*   integer hash table entries for a run-time module */
/*   created using the constructs-to-c function.      */
/******************************************************/
static unsigned int IntegerHashNodesToCode(
  Environment *theEnv,
  const char *fileName,
  const char *pathName,
  char *fileNameBuffer,
  unsigned int version)
  {
   unsigned int i, j;
   CLIPSInteger *hashPtr;
   unsigned int count;
   unsigned int numberOfEntries;
   CLIPSInteger **integerTable;
   bool newHeader = true;
   FILE *fp;
   unsigned int arrayVersion = 1;

   /*====================================*/
   /* Count the total number of entries. */
   /*====================================*/

   integerTable = GetIntegerTable(theEnv);
   count = numberOfEntries = 0;

   for (i = 0; i < INTEGER_HASH_SIZE; i++)
     {
      for (hashPtr = integerTable[i];
           hashPtr != NULL;
           hashPtr = hashPtr->next)
        { numberOfEntries++; }
     }

   if (numberOfEntries == 0) return(version);

   for (i = 1; i <= (numberOfEntries / ConstructCompilerData(theEnv)->MaxIndices) + 1 ; i++)
     { fprintf(ConstructCompilerData(theEnv)->HeaderFP,"extern CLIPSInteger I%d_%d[];\n",ConstructCompilerData(theEnv)->ImageID,i); }

   /*==================*/
   /* Create the file. */
   /*==================*/

   if ((fp = NewCFile(theEnv,fileName,pathName,fileNameBuffer,1,version,false)) == NULL) return version;

   /*===================*/
   /* List the entries. */
   /*===================*/

   j = 0;

   for (i = 0; i < INTEGER_HASH_SIZE; i++)
     {
      for (hashPtr = integerTable[i];
           hashPtr != NULL;
           hashPtr = hashPtr->next)
        {
         if (newHeader)
           {
            fprintf(fp,"CLIPSInteger I%d_%d[] = {\n",ConstructCompilerData(theEnv)->ImageID,arrayVersion);
            newHeader = false;
           }
           
         fprintf(fp,"{{INTEGER_TYPE},");

         if (hashPtr->next == NULL)
           { fprintf(fp,"NULL,"); }
         else
           {
            if ((j + 1) >= ConstructCompilerData(theEnv)->MaxIndices)
              { fprintf(fp,"&I%d_%d[%d],",ConstructCompilerData(theEnv)->ImageID,arrayVersion + 1,0); }
            else
              { fprintf(fp,"&I%d_%d[%d],",ConstructCompilerData(theEnv)->ImageID,arrayVersion,j + 1); }
           }

         fprintf(fp,"%ld,1,0,0,%d,",hashPtr->count + 1,i);
         fprintf(fp,"%lldLL",hashPtr->contents);

         count++;
         j++;

         if ((count == numberOfEntries) || (j >= ConstructCompilerData(theEnv)->MaxIndices))
           {
            fprintf(fp,"}};\n");
            GenClose(theEnv,fp);
            j = 0;
            version++;
            arrayVersion++;
            if (count < numberOfEntries)
              {
               if ((fp = NewCFile(theEnv,fileName,pathName,fileNameBuffer,1,version,false)) == NULL)
                 { return 0; }
               newHeader = true;
              }
           }
         else
           { fprintf(fp,"},\n"); }
        }
     }

   return version;
  }

/****************************************************************/
/* HashTablesToCode: Produces the code for the symbol, integer, */
/*   float, and bitmap hash tables for a run-time module        */
/*   created using the constructs-to-c function.                */
/****************************************************************/
static int HashTablesToCode(
  Environment *theEnv,
  const char *fileName,
  const char *pathName,
  char *fileNameBuffer)
  {
   unsigned long i;
   FILE *fp;
   CLIPSLexeme **symbolTable;
   CLIPSFloat **floatTable;
   CLIPSInteger **integerTable;
   CLIPSBitMap **bitMapTable;

   /*======================================*/
   /* Write the code for the symbol table. */
   /*======================================*/

   symbolTable = GetSymbolTable(theEnv);

   if ((fp = NewCFile(theEnv,fileName,pathName,fileNameBuffer,1,1,false)) == NULL)
     { return 0; }

   fprintf(ConstructCompilerData(theEnv)->HeaderFP,"extern CLIPSLexeme *sht%d[];\n",ConstructCompilerData(theEnv)->ImageID);
   fprintf(fp,"CLIPSLexeme *sht%d[%ld] = {\n",ConstructCompilerData(theEnv)->ImageID,SYMBOL_HASH_SIZE);

   for (i = 0; i < SYMBOL_HASH_SIZE; i++)
      {
       PrintSymbolReference(theEnv,fp,symbolTable[i]);

       if (i + 1 != SYMBOL_HASH_SIZE) fprintf(fp,",\n");
      }

    fprintf(fp,"};\n");

    GenClose(theEnv,fp);

   /*=====================================*/
   /* Write the code for the float table. */
   /*=====================================*/

   floatTable = GetFloatTable(theEnv);

   if ((fp = NewCFile(theEnv,fileName,pathName,fileNameBuffer,1,2,false)) == NULL)
     { return 0; }

   fprintf(ConstructCompilerData(theEnv)->HeaderFP,"extern CLIPSFloat *fht%d[];\n",ConstructCompilerData(theEnv)->ImageID);
   fprintf(fp,"CLIPSFloat *fht%d[%d] = {\n",ConstructCompilerData(theEnv)->ImageID,FLOAT_HASH_SIZE);

   for (i = 0; i < FLOAT_HASH_SIZE; i++)
      {
       if (floatTable[i] == NULL) { fprintf(fp,"NULL"); }
       else PrintFloatReference(theEnv,fp,floatTable[i]);

       if (i + 1 != FLOAT_HASH_SIZE) fprintf(fp,",\n");
      }

    fprintf(fp,"};\n");

    GenClose(theEnv,fp);

   /*=======================================*/
   /* Write the code for the integer table. */
   /*=======================================*/

   integerTable = GetIntegerTable(theEnv);

   if ((fp = NewCFile(theEnv,fileName,pathName,fileNameBuffer,1,3,false)) == NULL)
     { return 0; }

   fprintf(ConstructCompilerData(theEnv)->HeaderFP,"extern CLIPSInteger *iht%d[];\n",ConstructCompilerData(theEnv)->ImageID);
   fprintf(fp,"CLIPSInteger *iht%d[%d] = {\n",ConstructCompilerData(theEnv)->ImageID,INTEGER_HASH_SIZE);

   for (i = 0; i < INTEGER_HASH_SIZE; i++)
      {
       if (integerTable[i] == NULL) { fprintf(fp,"NULL"); }
       else PrintIntegerReference(theEnv,fp,integerTable[i]);

       if (i + 1 != INTEGER_HASH_SIZE) fprintf(fp,",\n");
      }

    fprintf(fp,"};\n");

    GenClose(theEnv,fp);

   /*======================================*/
   /* Write the code for the bitmap table. */
   /*======================================*/

   bitMapTable = GetBitMapTable(theEnv);

   if ((fp = NewCFile(theEnv,fileName,pathName,fileNameBuffer,1,4,false)) == NULL)
     { return 0; }

   fprintf(ConstructCompilerData(theEnv)->HeaderFP,"extern struct clipsBitMap *bmht%d[];\n",ConstructCompilerData(theEnv)->ImageID);
   fprintf(fp,"struct clipsBitMap *bmht%d[%d] = {\n",ConstructCompilerData(theEnv)->ImageID,BITMAP_HASH_SIZE);

   for (i = 0; i < BITMAP_HASH_SIZE; i++)
      {
       PrintBitMapReference(theEnv,fp,bitMapTable[i]);

       if (i + 1 != BITMAP_HASH_SIZE) fprintf(fp,",\n");
      }

    fprintf(fp,"};\n");

    GenClose(theEnv,fp);

    return 1;
   }

/*****************************************************/
/* PrintSymbolReference: Prints the C code reference */
/*   address to the specified symbol (also used for  */
/*   strings and instance names).                    */
/*****************************************************/
void PrintSymbolReference(
  Environment *theEnv,
  FILE *theFile,
  CLIPSLexeme *theSymbol)
  {
   if (theSymbol == NULL) fprintf(theFile,"NULL");
   else fprintf(theFile,"&S%u_%u[%u]",
                        ConstructCompilerData(theEnv)->ImageID,
                        (theSymbol->bucket / ConstructCompilerData(theEnv)->MaxIndices) + 1,
                        theSymbol->bucket % ConstructCompilerData(theEnv)->MaxIndices);
  }

/****************************************************/
/* PrintFloatReference: Prints the C code reference */
/*   address to the specified float.                */
/****************************************************/
void PrintFloatReference(
  Environment *theEnv,
  FILE *theFile,
  CLIPSFloat *theFloat)
  {
   fprintf(theFile,"&F%u_%u[%u]",
                   ConstructCompilerData(theEnv)->ImageID,
                   (theFloat->bucket / ConstructCompilerData(theEnv)->MaxIndices) + 1,
                   theFloat->bucket % ConstructCompilerData(theEnv)->MaxIndices);
  }

/******************************************************/
/* PrintIntegerReference: Prints the C code reference */
/*   address to the specified integer.                */
/******************************************************/
void PrintIntegerReference(
  Environment *theEnv,
  FILE *theFile,
  CLIPSInteger *theInteger)
  {
   fprintf(theFile,"&I%u_%u[%u]",
                   ConstructCompilerData(theEnv)->ImageID,
                   (theInteger->bucket / ConstructCompilerData(theEnv)->MaxIndices) + 1,
                   theInteger->bucket % ConstructCompilerData(theEnv)->MaxIndices);
  }

/*****************************************************/
/* PrintBitMapReference: Prints the C code reference */
/*   address to the specified bit map.               */
/*****************************************************/
void PrintBitMapReference(
  Environment *theEnv,
  FILE *theFile,
  CLIPSBitMap *theBitMap)
  {
   if (theBitMap == NULL) fprintf(theFile,"NULL");
   else fprintf(theFile,"&B%u_%u[%u]",
                        ConstructCompilerData(theEnv)->ImageID,
                        (theBitMap->bucket / ConstructCompilerData(theEnv)->MaxIndices) + 1,
                        theBitMap->bucket % ConstructCompilerData(theEnv)->MaxIndices);
  }

/*********************************************************/
/* PrintCString: Prints KB strings in the appropriate    */
/*   format for C (the " and \ characters are preceeded  */
/*   by a \ and carriage returns are replaced with \n).  */
/*********************************************************/
static void PrintCString(
  FILE *theFile,
  const char *str)
  {
   unsigned i;
   size_t slen;

   /*============================================*/
   /* Print the string's opening quotation mark. */
   /*============================================*/

   fprintf(theFile,"\"");

   /*===============================================*/
   /* Convert and write each character to the file. */
   /*===============================================*/

   slen = strlen(str);
   for (i = 0 ; i < slen ; i++)
     {
      /*====================================*/
      /* Preceed " and \ with the \ escape. */
      /*====================================*/

      if ((str[i] == '"') || (str[i] == '\\'))
        {
         fputc('\\',theFile);
         fputc(str[i],theFile);
        }

      /*====================================*/
      /* Replace a carriage return with \n. */
      /*====================================*/

      else if (str[i] == '\n')
        {
         fputc('\\',theFile);
         fputc('n',theFile);
        }

      /*===============================*/
      /* All other characters can be   */
      /* printed without modification. */
      /*===============================*/

      else
        { fputc(str[i],theFile); }
     }

   /*============================================*/
   /* Print the string's closing quotation mark. */
   /*============================================*/

   fprintf(theFile,"\"");
  }

#endif /* CONSTRUCT_COMPILER && (! RUN_TIME) */
