   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.41  08/16/22             */
   /*                                                     */
   /*        FACT LOAD/SAVE (ASCII/BINARY) MODULE         */
   /*******************************************************/

/*************************************************************/
/* Purpose:  File load/save routines for facts               */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.32: Fixed load-facts garbage collection issue.     */
/*                                                           */
/*      6.40: New file for save-facts and load-facts.        */
/*                                                           */
/*            Added bsave-facts and bload-facts.             */
/*                                                           */
/*            The save-facts and load-facts functions now    */
/*            return the number of facts saved/loaded.       */
/*                                                           */
/*************************************************************/

/* =========================================
   *****************************************
               EXTERNAL DEFINITIONS
   =========================================
   ***************************************** */

#include <stdlib.h>
#include <string.h>

#include "setup.h"

#if DEFTEMPLATE_CONSTRUCT

#include "argacces.h"
#if BLOAD || BLOAD_AND_BSAVE
#include "bload.h"
#endif
#include "cstrcpsr.h"
#include "factmngr.h"
#include "factrhs.h"
#include "insmngr.h"
#include "memalloc.h"
#include "modulpsr.h"
#include "modulutl.h"
#include "prntutil.h"
#include "router.h"
#include "scanner.h"
#include "strngrtr.h"
#include "symblbin.h"
#include "sysdep.h"
#include "tmpltdef.h"
#include "tmpltutl.h"

#include "factfile.h"

struct bsaveSlotValue
  {
   unsigned long slotName;
   unsigned long valueCount;
  };

struct bsaveSlotValueAtom
  {
   unsigned short type;
   unsigned long value;
  };

#define BINARY_FACTS_PREFIX_ID "\5\6\7BSFCLIPS"
#define BINARY_FACTS_VERSION_ID "V6.40"

#define MAX_BLOCK_SIZE 10240

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static struct expr            *StandardLoadFact(Environment *,const char *,struct token *);
   static Deftemplate           **GetSaveFactsDeftemplateNames(Environment *,const char *,struct expr *,SaveScope,
                                                               unsigned int *,bool *);

   static bool                    VerifyBinaryHeader(Environment *,const char *);

   static long                    MarkFacts(Environment *,SaveScope,Deftemplate **,unsigned int,size_t *);
   static void                    MarkSingleFact(Environment *,Fact *,size_t *);
   static void                    MarkNeededAtom(Environment *,CLIPSValue *,size_t *);
   static void                    WriteBinaryHeader(Environment *,FILE *);
   static long                    SaveBinaryFacts(Environment *,FILE *,SaveScope,Deftemplate **,unsigned int count,size_t *);
   static void                    SaveSingleFactBinary(Environment *theEnv,FILE *,Fact *);
   static void                    SaveAtomBinary(Environment *,CLIPSValue *,FILE *);
   static bool                    LoadSingleBinaryFact(Environment *);
   static Deftemplate            *BloadFactsCreateImpliedDeftemplate(Environment *,CLIPSLexeme *);
   static void                    BinaryLoadFactError(Environment *,Deftemplate *);
   static void                    CreateSlotValue(Environment *,UDFValue *,struct bsaveSlotValueAtom *,unsigned long,bool);
   static void                   *GetBinaryAtomValue(Environment *,struct bsaveSlotValueAtom *);

/********************************************************/
/* FactFileCommandDefinitions: Initializes save-facts,  */
/*   load-facts, bsave-facts, and bload-facts commands. */
/********************************************************/
void FactFileCommandDefinitions(
  Environment *theEnv)
  {
#if (! RUN_TIME)
   AddUDF(theEnv,"save-facts","l",1,UNBOUNDED,"y;sy",SaveFactsCommand,"SaveFactsCommand",NULL);
   AddUDF(theEnv,"load-facts","l",1,1,"sy",LoadFactsCommand,"LoadFactsCommand",NULL);
   
   AddUDF(theEnv,"bsave-facts","l",1,UNBOUNDED,"y;sy",BinarySaveFactsCommand,"BinarySaveFactsCommand",NULL);
   AddUDF(theEnv,"bload-facts","l",1,1,"sy",BinaryLoadFactsCommand,"BinaryLoadFactsCommand",NULL);
#endif
  }

/******************************************/
/* SaveFactsCommand: H/L access routine   */
/*   for the save-facts command.          */
/******************************************/
void SaveFactsCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   const char *fileName;
   unsigned int numArgs;
   SaveScope saveCode = LOCAL_SAVE;
   const char *argument;
   UDFValue theValue;
   struct expr *theList = NULL;
   long factCount = 0;

   /*============================================*/
   /* Check for the correct number of arguments. */
   /*============================================*/

   numArgs = UDFArgumentCount(context);

   /*=================================================*/
   /* Get the file name to which facts will be saved. */
   /*=================================================*/

   if ((fileName = GetFileName(context)) == NULL)
     {
      returnValue->integerValue = CreateInteger(theEnv,-1);
      return;
     }

   /*=============================================================*/
   /* If specified, the second argument to save-facts indicates   */
   /* whether just facts local to the current module or all facts */
   /* visible to the current module will be saved.                */
   /*=============================================================*/

   if (numArgs > 1)
     {
      if (! UDFNextArgument(context,SYMBOL_BIT,&theValue))
        {
         returnValue->integerValue = CreateInteger(theEnv,-1);
         return;
        }

      argument = theValue.lexemeValue->contents;

      if (strcmp(argument,"local") == 0)
        { saveCode = LOCAL_SAVE; }
      else if (strcmp(argument,"visible") == 0)
        { saveCode = VISIBLE_SAVE; }
      else
        {
         ExpectedTypeError1(theEnv,"save-facts",2,"symbol with value local or visible");
         returnValue->integerValue = CreateInteger(theEnv,-1);
         return;
        }
     }

   /*======================================================*/
   /* Subsequent arguments indicate that only those facts  */
   /* associated with the specified deftemplates should be */
   /* saved to the file.                                   */
   /*======================================================*/

   if (numArgs > 2) theList = GetFirstArgument()->nextArg->nextArg;

   /*====================================*/
   /* Call the SaveFacts driver routine. */
   /*====================================*/

   factCount = SaveFactsDriver(theEnv,fileName,saveCode,theList);
   returnValue->integerValue = CreateInteger(theEnv,factCount);
  }

/***********************************************************/
/* SaveFacts: C access routine for the save-facts command. */
/***********************************************************/
long SaveFacts(
  Environment *theEnv,
  const char *fileName,
  SaveScope saveCode)
  {
   return SaveFactsDriver(theEnv,fileName,saveCode,NULL);
  }

/*****************************************************************/
/* SaveFactsDriver: C access routine for the save-facts command. */
/*****************************************************************/
long SaveFactsDriver(
  Environment *theEnv,
  const char *fileName,
  SaveScope saveCode,
  struct expr *theList)
  {
   bool tempValue1, tempValue2, tempValue3;
   Fact *theFact;
   FILE *filePtr;
   Defmodule *theModule;
   Deftemplate **deftemplateArray;
   unsigned int count, i;
   bool printFact, error;
   long factCount = 0;

   /*======================================================*/
   /* Open the file. Use either "fast save" or I/O Router. */
   /*======================================================*/

   if ((filePtr = GenOpen(theEnv,fileName,"w")) == NULL)
     {
      OpenErrorMessage(theEnv,"save-facts",fileName);
      return -1;
     }

   SetFastSave(theEnv,filePtr);

   /*===========================================*/
   /* Set the print flags so that addresses and */
   /* strings are printed properly to the file. */
   /*===========================================*/

   tempValue1 = PrintUtilityData(theEnv)->PreserveEscapedCharacters;
   PrintUtilityData(theEnv)->PreserveEscapedCharacters = true;
   tempValue2 = PrintUtilityData(theEnv)->AddressesToStrings;
   PrintUtilityData(theEnv)->AddressesToStrings = true;
   tempValue3 = PrintUtilityData(theEnv)->InstanceAddressesToNames;
   PrintUtilityData(theEnv)->InstanceAddressesToNames = true;

   /*===================================================*/
   /* Determine the list of specific facts to be saved. */
   /*===================================================*/

   deftemplateArray = GetSaveFactsDeftemplateNames(theEnv,"save-facts",theList,saveCode,&count,&error);

   if (error)
     {
      PrintUtilityData(theEnv)->PreserveEscapedCharacters = tempValue1;
      PrintUtilityData(theEnv)->AddressesToStrings = tempValue2;
      PrintUtilityData(theEnv)->InstanceAddressesToNames = tempValue3;
      GenClose(theEnv,filePtr);
      SetFastSave(theEnv,NULL);
      return -1;
     }

   /*=================*/
   /* Save the facts. */
   /*=================*/

   theModule = GetCurrentModule(theEnv);

   for (theFact = GetNextFactInScope(theEnv,NULL);
        theFact != NULL;
        theFact = GetNextFactInScope(theEnv,theFact))
     {
      /*===========================================================*/
      /* If we're doing a local save and the facts's corresponding */
      /* deftemplate isn't in the current module, then don't save  */
      /* the fact.                                                 */
      /*===========================================================*/

      if ((saveCode == LOCAL_SAVE) &&
          (theFact->whichDeftemplate->header.whichModule->theModule != theModule))
        { printFact = false; }

      /*=====================================================*/
      /* Otherwise, if the list of facts to be printed isn't */
      /* restricted, then set the print flag to true.        */
      /*=====================================================*/

      else if (theList == NULL)
        { printFact = true; }

      /*=======================================================*/
      /* Otherwise see if the fact's corresponding deftemplate */
      /* is in the list of deftemplates whose facts are to be  */
      /* saved. If it's in the list, then set the print flag   */
      /* to true, otherwise set it to false.                   */
      /*=======================================================*/

      else
        {
         printFact = false;
         for (i = 0; i < count; i++)
           {
            if (deftemplateArray[i] == theFact->whichDeftemplate)
              {
               printFact = true;
               break;
              }
           }
        }

      /*===================================*/
      /* If the print flag is set to true, */
      /* then save the fact to the file.   */
      /*===================================*/

      if (printFact)
        {
         factCount++;
         PrintFact(theEnv,(char *) filePtr,theFact,false,false,NULL);
         WriteString(theEnv,(char *) filePtr,"\n");
        }
     }

   /*==========================*/
   /* Restore the print flags. */
   /*==========================*/

   PrintUtilityData(theEnv)->PreserveEscapedCharacters = tempValue1;
   PrintUtilityData(theEnv)->AddressesToStrings = tempValue2;
   PrintUtilityData(theEnv)->InstanceAddressesToNames = tempValue3;

   /*=================*/
   /* Close the file. */
   /*=================*/

   GenClose(theEnv,filePtr);
   SetFastSave(theEnv,NULL);

   /*==================================*/
   /* Free the deftemplate name array. */
   /*==================================*/

   if (theList != NULL)
     { rm(theEnv,deftemplateArray,sizeof(Deftemplate *) * count); }

   /*=========================================*/
   /* Return the fact count to indicate no    */
   /* errors occurred while saving the facts. */
   /*=========================================*/

   return factCount;
  }

/*******************************************************************/
/* GetSaveFactsDeftemplateNames: Retrieves the list of deftemplate */
/*   names for saving specific facts with the save-facts command.  */
/*******************************************************************/
static Deftemplate **GetSaveFactsDeftemplateNames(
  Environment *theEnv,
  const char *functionName,
  struct expr *theList,
  SaveScope saveCode,
  unsigned int *count,
  bool *error)
  {
   struct expr *tempList;
   Deftemplate **deftemplateArray;
   UDFValue tempArg;
   unsigned int i, tempCount;
   Deftemplate *theDeftemplate = NULL;

   /*=============================*/
   /* Initialize the error state. */
   /*=============================*/

   *error = false;

   /*=====================================================*/
   /* If no deftemplate names were specified as arguments */
   /* then the deftemplate name list is empty.            */
   /*=====================================================*/

   if (theList == NULL)
     {
      *count = 0;
      return NULL;
     }

   /*======================================*/
   /* Determine the number of deftemplate  */
   /* names to be stored in the name list. */
   /*======================================*/

   for (tempList = theList, *count = 0;
        tempList != NULL;
        tempList = tempList->nextArg, (*count)++)
     { /* Do Nothing */ }

   /*=========================================*/
   /* Allocate the storage for the name list. */
   /*=========================================*/

   deftemplateArray = (Deftemplate **) gm2(theEnv,sizeof(Deftemplate *) * *count);

   /*=====================================*/
   /* Loop through each of the arguments. */
   /*=====================================*/

   for (tempList = theList, i = 0;
        i < *count;
        tempList = tempList->nextArg, i++)
     {
      /*========================*/
      /* Evaluate the argument. */
      /*========================*/

      EvaluateExpression(theEnv,tempList,&tempArg);

      if (EvaluationData(theEnv)->EvaluationError)
        {
         *error = true;
         rm(theEnv,deftemplateArray,sizeof(Deftemplate *) * *count);
         return NULL;
        }

      /*======================================*/
      /* A deftemplate name must be a symbol. */
      /*======================================*/

      if (tempArg.header->type != SYMBOL_TYPE)
        {
         *error = true;
         ExpectedTypeError1(theEnv,functionName,3+i,"symbol");
         rm(theEnv,deftemplateArray,sizeof(Deftemplate *) * *count);
         return NULL;
        }

      /*===================================================*/
      /* Find the deftemplate. For a local save, look only */
      /* in the current module. For a visible save, look   */
      /* in all visible modules.                           */
      /*===================================================*/

      if (saveCode == LOCAL_SAVE)
        {
         theDeftemplate = FindDeftemplateInModule(theEnv,tempArg.lexemeValue->contents);
         if (theDeftemplate == NULL)
           {
            *error = true;
            ExpectedTypeError1(theEnv,functionName,3+i,"'local deftemplate name'");
            rm(theEnv,deftemplateArray,sizeof(Deftemplate *) * *count);
            return NULL;
           }
        }
      else if (saveCode == VISIBLE_SAVE)
        {
         theDeftemplate = (Deftemplate *)
           FindImportedConstruct(theEnv,"deftemplate",NULL,
                                 tempArg.lexemeValue->contents,
                                 &tempCount,true,NULL);
         if (theDeftemplate == NULL)
           {
            *error = true;
            ExpectedTypeError1(theEnv,functionName,3+i,"'visible deftemplate name'");
            rm(theEnv,deftemplateArray,sizeof(Deftemplate *) * *count);
            return NULL;
           }
        }

      /*==================================*/
      /* Add a pointer to the deftemplate */
      /* to the array being created.      */
      /*==================================*/

      deftemplateArray[i] = theDeftemplate;
     }

   /*===================================*/
   /* Return the array of deftemplates. */
   /*===================================*/

   return deftemplateArray;
  }

/******************************************/
/* LoadFactsCommand: H/L access routine   */
/*   for the load-facts command.          */
/******************************************/
void LoadFactsCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   const char *fileName;
   long factCount;
   
   /*====================================================*/
   /* Get the file name from which facts will be loaded. */
   /*====================================================*/

   if ((fileName = GetFileName(context)) == NULL)
     {
      returnValue->integerValue = CreateInteger(theEnv,-1);
      return;
     }

   /*====================================*/
   /* Call the LoadFacts driver routine. */
   /*====================================*/

   factCount = LoadFacts(theEnv,fileName);
   returnValue->integerValue = CreateInteger(theEnv,factCount);
  }

/***********************************************************/
/* LoadFacts: C access routine for the load-facts command. */
/***********************************************************/
long LoadFacts(
  Environment *theEnv,
  const char *fileName)
  {
   FILE *filePtr;
   struct token theToken;
   struct expr *testPtr;
   UDFValue rv;
   int danglingConstructs;
   GCBlock gcb;
   long factCount = 0;

   /*=====================================*/
   /* If embedded, clear the error flags. */
   /*=====================================*/
   
   if (EvaluationData(theEnv)->CurrentExpression == NULL)
     { ResetErrorFlags(theEnv); }

   /*======================================================*/
   /* Open the file. Use either "fast save" or I/O Router. */
   /*======================================================*/

   if ((filePtr = GenOpen(theEnv,fileName,"r")) == NULL)
     {
      OpenErrorMessage(theEnv,"load-facts",fileName);
      return -1;
     }

   SetFastLoad(theEnv,filePtr);

   /*========================================*/
   /* Set up the frame for tracking garbage. */
   /*========================================*/
   
   GCBlockStart(theEnv,&gcb);

   /*=================*/
   /* Load the facts. */
   /*=================*/
   
   danglingConstructs = ConstructData(theEnv)->DanglingConstructs;

   theToken.tknType = LEFT_PARENTHESIS_TOKEN;
   while (theToken.tknType != STOP_TOKEN)
     {
      testPtr = StandardLoadFact(theEnv,(char *) filePtr,&theToken);
      if (testPtr == NULL) theToken.tknType = STOP_TOKEN;
      else
        {
         factCount++;
         ExpressionInstall(theEnv,testPtr);
         EvaluateExpression(theEnv,testPtr,&rv);
         ExpressionDeinstall(theEnv,testPtr);
        }
      ReturnExpression(theEnv,testPtr);
     }
     
   /*================================*/
   /* Restore the old garbage frame. */
   /*================================*/
   
   GCBlockEnd(theEnv,&gcb);

   /*===============================================*/
   /* If embedded, clean the topmost garbage frame. */
   /*===============================================*/

   if (EvaluationData(theEnv)->CurrentExpression == NULL)
     {
      CleanCurrentGarbageFrame(theEnv,NULL);
      ConstructData(theEnv)->DanglingConstructs = danglingConstructs;
     }

   /*======================*/
   /* Call periodic tasks. */
   /*======================*/
   
   CallPeriodicTasks(theEnv);

   /*=================*/
   /* Close the file. */
   /*=================*/

   SetFastLoad(theEnv,NULL);
   GenClose(theEnv,filePtr);

   /*================================================*/
   /* Return true if no error occurred while loading */
   /* the facts, otherwise return false.             */
   /*================================================*/

   if (EvaluationData(theEnv)->EvaluationError) return -1;
   return factCount;
  }

/******************************************/
/* LoadFactsFromString: C access routine. */
/******************************************/
long LoadFactsFromString(
  Environment *theEnv,
  const char *theString,
  size_t theMax)
  {
   const char *theStrRouter = "*** load-facts-from-string ***";
   struct token theToken;
   struct expr *testPtr;
   UDFValue rv;
   long factCount = 0;
   
   /*=====================================*/
   /* If embedded, clear the error flags. */
   /*=====================================*/
   
   if (EvaluationData(theEnv)->CurrentExpression == NULL)
     { ResetErrorFlags(theEnv); }

   /*==========================*/
   /* Initialize string router */
   /*==========================*/

   if ((theMax == SIZE_MAX) ? (! OpenStringSource(theEnv,theStrRouter,theString,0)) :
                              (! OpenTextSource(theEnv,theStrRouter,theString,0,theMax)))
     { return -1; }

   /*=================*/
   /* Load the facts. */
   /*=================*/

   theToken.tknType = LEFT_PARENTHESIS_TOKEN;
   while (theToken.tknType != STOP_TOKEN)
     {
      testPtr = StandardLoadFact(theEnv,theStrRouter,&theToken);
      if (testPtr == NULL) theToken.tknType = STOP_TOKEN;
      else
        {
         factCount++;
         EvaluateExpression(theEnv,testPtr,&rv);
        }
      ReturnExpression(theEnv,testPtr);
     }

   /*=================*/
   /* Close router.   */
   /*=================*/

   CloseStringSource(theEnv,theStrRouter);

   /*==================================================*/
   /* Return the fact count if no error occurred while */
   /* loading the facts, otherwise return -1.          */
   /*==================================================*/

   if (EvaluationData(theEnv)->EvaluationError) return -1;
   return factCount;
  }

/**************************************************************************/
/* StandardLoadFact: Loads a single fact from the specified logical name. */
/**************************************************************************/
static struct expr *StandardLoadFact(
  Environment *theEnv,
  const char *logicalName,
  struct token *theToken)
  {
   bool error = false;
   struct expr *temp;

   GetToken(theEnv,logicalName,theToken);
   if (theToken->tknType != LEFT_PARENTHESIS_TOKEN) return NULL;

   temp = GenConstant(theEnv,FCALL,FindFunction(theEnv,"assert"));
   temp->argList = GetRHSPattern(theEnv,logicalName,theToken,&error,
                                  true,false,true,RIGHT_PARENTHESIS_TOKEN);

   if (error == true)
     {
      WriteString(theEnv,STDERR,"Function load-facts encountered an error\n");
      SetEvaluationError(theEnv,true);
      ReturnExpression(theEnv,temp);
      return NULL;
     }

   if (ExpressionContainsVariables(temp,true))
     {
      ReturnExpression(theEnv,temp);
      return NULL;
     }

   return(temp);
  }

/**********************************************/
/* BinaryLoadFactsCommand: H/L access routine */
/*   for the bload-facts command.             */
/**********************************************/
void BinaryLoadFactsCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   const char *fileName;
   long factCount;

   /*====================================================*/
   /* Get the file name from which facts will be loaded. */
   /*====================================================*/

   if ((fileName = GetFileName(context)) == NULL)
     {
      returnValue->integerValue = CreateInteger(theEnv,-1);
      return;
     }

   /*==========================================*/
   /* Call the BinaryLoadFacts driver routine. */
   /*==========================================*/

   factCount = BinaryLoadFacts(theEnv,fileName);
   returnValue->integerValue = CreateInteger(theEnv,factCount);
  }

/******************************************************************/
/* BinaryLoadFacts: C access routine for the bload-facts command. */
/******************************************************************/
long BinaryLoadFacts(
  Environment *theEnv,
  const char *fileName)
  {
   GCBlock gcb;
   long i;
   long factCount;

   /*=====================================*/
   /* If embedded, clear the error flags. */
   /*=====================================*/
   
   if (EvaluationData(theEnv)->CurrentExpression == NULL)
     { ResetErrorFlags(theEnv); }

   /*======================================================*/
   /* Open the file. Use either "fast save" or I/O Router. */
   /*======================================================*/

   if (GenOpenReadBinary(theEnv,"bload-facts",fileName) == false)
     {
      OpenErrorMessage(theEnv,"bload-facts",fileName);
      return -1;
     }

   /*======================================*/
   /* Check the binary header to determine */
   /* if this is a binary fact file.       */
   /*======================================*/
   
   if (VerifyBinaryHeader(theEnv,fileName) == false)
     {
      GenCloseBinary(theEnv);
      return -1;
     }

   /*========================================*/
   /* Set up the frame for tracking garbage. */
   /*========================================*/
   
   GCBlockStart(theEnv,&gcb);

   /*=================*/
   /* Load the facts. */
   /*=================*/

   ReadNeededAtomicValues(theEnv);
   
   UtilityData(theEnv)->BinaryFileOffset = 0L;

   GenReadBinary(theEnv,&UtilityData(theEnv)->BinaryFileSize,sizeof(size_t));
   GenReadBinary(theEnv,&factCount,sizeof(long));

   for (i = 0; i < factCount; i++)
     {
      if (LoadSingleBinaryFact(theEnv) == false)
        {
         SetEvaluationError(theEnv,true);
         break;
        }
     }

   FreeReadBuffer(theEnv);
   
   FreeAtomicValueStorage(theEnv);

   /*=================*/
   /* Close the file. */
   /*=================*/

   GenCloseBinary(theEnv);

   /*================================*/
   /* Restore the old garbage frame. */
   /*================================*/
   
   GCBlockEnd(theEnv,&gcb);

   /*===============================================*/
   /* If embedded, clean the topmost garbage frame. */
   /*===============================================*/

   if (EvaluationData(theEnv)->CurrentExpression == NULL)
     { CleanCurrentGarbageFrame(theEnv,NULL); }

   /*======================*/
   /* Call periodic tasks. */
   /*======================*/
   
   CallPeriodicTasks(theEnv);

   /*===============================================*/
   /* Return the fact count if no error occurred    */
   /* while loading the facts, otherwise return -1. */
   /*===============================================*/

   if (EvaluationData(theEnv)->EvaluationError) return -1;
   
   return factCount;
  }

/***********************/
/* VerifyBinaryHeader: */
/***********************/
static bool VerifyBinaryHeader(
  Environment *theEnv,
  const char *theFile)
  {
   char buf[20];

   GenReadBinary(theEnv,buf,(strlen(BINARY_FACTS_PREFIX_ID) + 1));
   if (strcmp(buf,BINARY_FACTS_PREFIX_ID) != 0)
     {
      PrintErrorID(theEnv,"FACTFILE",1,false);
      WriteString(theEnv,STDERR,"File '");
      WriteString(theEnv,STDERR,theFile);
      WriteString(theEnv,STDERR,"' is not a binary facts file.\n");
      return false;
     }
     
   GenReadBinary(theEnv,buf,(strlen(BINARY_FACTS_VERSION_ID) + 1));
   if (strcmp(buf,BINARY_FACTS_VERSION_ID) != 0)
     {
      PrintErrorID(theEnv,"FACTFILE",2,false);
      WriteString(theEnv,STDERR,"File '");
      WriteString(theEnv,STDERR,theFile);
      WriteString(theEnv,STDERR,"' is not a compatible binary facts file.\n");
      return false;
     }
     
   return true;
  }

/************************/
/* LoadSingleBinaryFact */
/************************/
static bool LoadSingleBinaryFact(
  Environment *theEnv)
  {
   CLIPSLexeme *deftemplateName;
   unsigned short slotCount;
   Deftemplate *theDeftemplate;
   Fact *newFact;
   struct bsaveSlotValue *bsArray;
   struct bsaveSlotValueAtom *bsaArray = NULL;
   unsigned long nameIndex;
   unsigned long totalValueCount;
   long i, j;
   unsigned int count;
   TemplateSlot *sp;
   UDFValue slotValue;
   bool implied, isMultislot;
   bool success = true;
   
   /*===========================*/
   /* Get the deftemplate name. */
   /*===========================*/
   
   BufferedRead(theEnv,&nameIndex,sizeof(unsigned long));
   deftemplateName = SymbolPointer(nameIndex);

   /*===========================*/
   /* Get the deftemplate type. */
   /*===========================*/

   BufferedRead(theEnv,&implied,sizeof(bool));

   /*=====================*/
   /* Get the slot count. */
   /*=====================*/
   
   BufferedRead(theEnv,&slotCount,sizeof(unsigned short));

   /*==================================*/
   /* Make sure the deftemplate exists */
   /* and check the slot count.        */
   /*==================================*/
   
   theDeftemplate = (Deftemplate *)
                    FindImportedConstruct(theEnv,"deftemplate",NULL,deftemplateName->contents,
                                          &count,true,NULL);

   if (theDeftemplate == NULL)
     {
      if (implied)
        {
         theDeftemplate = BloadFactsCreateImpliedDeftemplate(theEnv,deftemplateName);
         if (theDeftemplate == NULL)
           { return false; }
        }
      else
        {
         CantFindItemInFunctionErrorMessage(theEnv,"deftemplate",deftemplateName->contents,"bload-facts",true);
         return false;
        }
     }

   if ((implied && (slotCount != 1)) ||
       ((! implied) && (theDeftemplate->numberOfSlots != slotCount)))
     {
      BinaryLoadFactError(theEnv,theDeftemplate);
      return false;
     }

   /*==================================*/
   /* Create the new unitialized fact. */
   /*==================================*/
   
   newFact = CreateFactBySize(theEnv,slotCount);
   newFact->whichDeftemplate = theDeftemplate;

   if (slotCount == 0)
     {
      Assert(newFact);
      return true;
     }
   
   sp = theDeftemplate->slotList;

   /*====================================*/
   /* Read all slot information and slot */
   /* value atoms into big arrays.       */
   /*====================================*/
   
   bsArray = (struct bsaveSlotValue *) gm2(theEnv,(sizeof(struct bsaveSlotValue) * slotCount));
   BufferedRead(theEnv,bsArray,(sizeof(struct bsaveSlotValue) * slotCount));

   BufferedRead(theEnv,&totalValueCount,sizeof(unsigned long));

   if (totalValueCount != 0L)
     {
      bsaArray = (struct bsaveSlotValueAtom *)
                  gm2(theEnv,(totalValueCount * sizeof(struct bsaveSlotValueAtom)));
      BufferedRead(theEnv,bsaArray,(totalValueCount * sizeof(struct bsaveSlotValueAtom)));
     }

   /*==================================*/
   /* Insert the values for the slots. */
   /*==================================*/
   
   for (i = 0 , j = 0L ; i < slotCount ; i++)
     {
      /*=======================================================*/
      /* Here is another check for the validity of the binary  */
      /* file: the order of the slots in the file should match */
      /* the order in the deftemplate definition.              */
      /*=======================================================*/
      
      if (implied)
        {
         if (bsArray[i].slotName != ULONG_MAX)
           {
            BinaryLoadFactError(theEnv,theDeftemplate);
            ReturnFact(theEnv,newFact);
            success = false;
            break;
           }
        }
      else
        {
         if (sp->slotName != SymbolPointer(bsArray[i].slotName))
           {
            BinaryLoadFactError(theEnv,theDeftemplate);
            ReturnFact(theEnv,newFact);
            success = false;
            break;
           }
        }

      isMultislot = implied || (sp->multislot == true);
        
      CreateSlotValue(theEnv,&slotValue,(struct bsaveSlotValueAtom *) &bsaArray[j],
                      bsArray[i].valueCount,isMultislot);

      newFact->theProposition.contents[i].value = slotValue.value;

      j += (unsigned long) bsArray[i].valueCount;

      if (! implied)
        { sp = sp->next; }
     }

   rm(theEnv,bsArray,(sizeof(struct bsaveSlotValue) * slotCount));

   if (totalValueCount != 0L)
     { rm(theEnv,bsaArray,(totalValueCount * sizeof(struct bsaveSlotValueAtom))); }

   if (success)
     { Assert(newFact); }
     
   return success;
  }

/**************************************/
/* BloadFactsCreateImpliedDeftemplate */
/**************************************/
static Deftemplate *BloadFactsCreateImpliedDeftemplate(
  Environment *theEnv,
  CLIPSLexeme *deftemplateName)
  {
#if (! BLOAD_ONLY) && (! RUN_TIME)

#if BLOAD || BLOAD_AND_BSAVE
   if (Bloaded(theEnv))
     {
      CantFindItemInFunctionErrorMessage(theEnv,"deftemplate",deftemplateName->contents,"bload-facts",true);
      return NULL;
     }
#endif
#if DEFMODULE_CONSTRUCT
   if (FindImportExportConflict(theEnv,"deftemplate",GetCurrentModule(theEnv),deftemplateName->contents))
     {
      ImportExportConflictMessage(theEnv,"implied deftemplate",deftemplateName->contents,NULL,NULL);
      return NULL;
     }
#endif
   return CreateImpliedDeftemplate(theEnv,deftemplateName,true);

#else
   CantFindItemInFunctionErrorMessage(theEnv,"deftemplate",deftemplateName->contents,"bload-facts",true);
   return NULL;
#endif
  }

/***************************************************
  NAME         : CreateSlotValue
  DESCRIPTION  : Creates a data object value from
                 the binary slot value atom data
  INPUTS       : 1) A data object buffer
                 2) The slot value atoms array
                 3) The number of values to put
                    in the data object
  RETURNS      : Nothing useful
  SIDE EFFECTS : Data object initialized
                 (if more than one value, a
                 multifield is created)
  NOTES        : None
 ***************************************************/
static void CreateSlotValue(
  Environment *theEnv,
  UDFValue *returnValue,
  struct bsaveSlotValueAtom *bsaValues,
  unsigned long valueCount,
  bool isMultislot)
  {
   unsigned i;

   if (isMultislot)
     {
      returnValue->value = CreateUnmanagedMultifield(theEnv,valueCount);
      returnValue->begin = 0;
      returnValue->range = valueCount;
      for (i = 0 ; i < valueCount ; i++)
        { returnValue->multifieldValue->contents[i].value = GetBinaryAtomValue(theEnv,&bsaValues[i]); }
     }
   else
     { returnValue->value = GetBinaryAtomValue(theEnv,&bsaValues[0]); }
  }

/**********************/
/* GetBinaryAtomValue */
/**********************/
static void *GetBinaryAtomValue(
  Environment *theEnv,
  struct bsaveSlotValueAtom *ba)
  {
   switch (ba->type)
     {
      case SYMBOL_TYPE:
      case STRING_TYPE:
      case INSTANCE_NAME_TYPE:
         return((void *) SymbolPointer(ba->value));
         
      case FLOAT_TYPE:
         return((void *) FloatPointer(ba->value));
         
      case INTEGER_TYPE:
         return((void *) IntegerPointer(ba->value));
         
      case FACT_ADDRESS_TYPE:
#if DEFTEMPLATE_CONSTRUCT && DEFRULE_CONSTRUCT
         return((void *) &FactData(theEnv)->DummyFact);
#else
         return NULL;
#endif

      case EXTERNAL_ADDRESS_TYPE:
        return CreateExternalAddress(theEnv,NULL,0);

      default:
        {
         SystemError(theEnv,"INSFILE",1);
         ExitRouter(theEnv,EXIT_FAILURE);
        }
     }
   return NULL;
  }

/***********************/
/* BinaryLoadFactError */
/***********************/
static void BinaryLoadFactError(
  Environment *theEnv,
  Deftemplate *theDeftemplate)
  {
   PrintErrorID(theEnv,"FACTFILE",3,false);
   WriteString(theEnv,STDERR,"Function 'bload-facts' is unable to load fact of deftemplate '");
   WriteString(theEnv,STDERR,theDeftemplate->header.name->contents);
   WriteString(theEnv,STDERR,"'\n");
  }

/**********************************************/
/* BinarySaveFactsCommand: H/L access routine */
/*   for the bsave-facts command.             */
/**********************************************/
void BinarySaveFactsCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   const char *fileName;
   unsigned int numArgs;
   SaveScope saveCode = LOCAL_SAVE;
   const char *argument;
   UDFValue theValue;
   struct expr *theList = NULL;
   long factCount;

   /*============================================*/
   /* Check for the correct number of arguments. */
   /*============================================*/

   numArgs = UDFArgumentCount(context);

   /*=================================================*/
   /* Get the file name to which facts will be saved. */
   /*=================================================*/

   if ((fileName = GetFileName(context)) == NULL)
     {
      returnValue->integerValue = CreateInteger(theEnv,-1);
      return;
     }

   /*=============================================================*/
   /* If specified, the second argument to save-facts indicates   */
   /* whether just facts local to the current module or all facts */
   /* visible to the current module will be saved.                */
   /*=============================================================*/

   if (numArgs > 1)
     {
      if (! UDFNextArgument(context,SYMBOL_BIT,&theValue))
        {
         returnValue->integerValue = CreateInteger(theEnv,-1);
         return;
        }

      argument = theValue.lexemeValue->contents;

      if (strcmp(argument,"local") == 0)
        { saveCode = LOCAL_SAVE; }
      else if (strcmp(argument,"visible") == 0)
        { saveCode = VISIBLE_SAVE; }
      else
        {
         ExpectedTypeError1(theEnv,"bsave-facts",2,"symbol with value local or visible");
         returnValue->integerValue = CreateInteger(theEnv,-1);
         return;
        }
     }

   /*======================================================*/
   /* Subsequent arguments indicate that only those facts  */
   /* associated with the specified deftemplates should be */
   /* saved to the file.                                   */
   /*======================================================*/

   if (numArgs > 2) theList = GetFirstArgument()->nextArg->nextArg;

   /*==========================================*/
   /* Call the BinarySaveFacts driver routine. */
   /*==========================================*/

   factCount = BinarySaveFactsDriver(theEnv,fileName,saveCode,theList);
   returnValue->integerValue = CreateInteger(theEnv,factCount);
  }

/******************************************************************/
/* BinarySaveFacts: C access routine for the bsave-facts command. */
/******************************************************************/
long BinarySaveFacts(
  Environment *theEnv,
  const char *file,
  SaveScope saveCode)
  {
   return BinarySaveFactsDriver(theEnv,file,saveCode,NULL);
  }

/**************************/
/* BinarySaveFactsDriver: */
/**************************/
long BinarySaveFactsDriver(
  Environment *theEnv,
  const char *fileName,
  SaveScope saveCode,
  Expression *theList)
  {
   FILE *filePtr;
   Deftemplate **deftemplateArray;
   unsigned int templateCount;
   bool error;
   size_t neededSpace = 0;
   long factCount;

   /*=====================================*/
   /* If embedded, clear the error flags. */
   /*=====================================*/

   if (EvaluationData(theEnv)->CurrentExpression == NULL)
     { ResetErrorFlags(theEnv); }
     
   /*======================================================*/
   /* Open the file. Use either "fast save" or I/O Router. */
   /*======================================================*/

   if ((filePtr = GenOpen(theEnv,fileName,"wb")) == NULL)
     {
      OpenErrorMessage(theEnv,"bsave-facts",fileName);
      return -1;
     }

   /*===================================================*/
   /* Determine the list of specific facts to be saved. */
   /*===================================================*/

   deftemplateArray = GetSaveFactsDeftemplateNames(theEnv,"bsave-facts",theList,
                                                   saveCode,&templateCount,&error);

   if (error)
     {
      GenClose(theEnv,filePtr);
      return -1;
     }

   InitAtomicValueNeededFlags(theEnv);
   
   factCount = MarkFacts(theEnv,saveCode,deftemplateArray,templateCount,&neededSpace);

   WriteBinaryHeader(theEnv,filePtr);
   WriteNeededAtomicValues(theEnv,filePtr);

   fwrite(&neededSpace,sizeof(size_t),1,filePtr);
   fwrite(&factCount,sizeof(unsigned long),1,filePtr);

   SetAtomicValueIndices(theEnv,false);
   
   SaveBinaryFacts(theEnv,filePtr,saveCode,deftemplateArray,templateCount,&neededSpace);

   RestoreAtomicValueBuckets(theEnv);
   
   /*=================*/
   /* Close the file. */
   /*=================*/

   GenClose(theEnv,filePtr);

   /*==================================*/
   /* Free the deftemplate name array. */
   /*==================================*/

   if (theList != NULL)
     { rm(theEnv,deftemplateArray,sizeof(Deftemplate *) * templateCount); }

   /*=========================================*/
   /* Return the fact count to indicate no    */
   /* errors occurred while saving the facts. */
   /*=========================================*/

   return factCount;
  }

/*************/
/* MarkFacts */
/*************/
static long MarkFacts(
  Environment *theEnv,
  SaveScope saveCode,
  Deftemplate **deftemplateArray,
  unsigned int count,
  size_t *neededSpace)
  {
   Fact *theFact;
   Defmodule *theModule;
   unsigned int i;
   bool markFact;
   long factCount = 0;

   /*=================*/
   /* Save the facts. */
   /*=================*/

   theModule = GetCurrentModule(theEnv);

   for (theFact = GetNextFactInScope(theEnv,NULL);
        theFact != NULL;
        theFact = GetNextFactInScope(theEnv,theFact))
     {
      /*===========================================================*/
      /* If we're doing a local save and the facts's corresponding */
      /* deftemplate isn't in the current module, then don't save  */
      /* the fact.                                                 */
      /*===========================================================*/

      if ((saveCode == LOCAL_SAVE) &&
          (theFact->whichDeftemplate->header.whichModule->theModule != theModule))
        { markFact = false; }

      /*===================================================*/
      /* Otherwise, if the list of facts to be saved isn't */
      /* restricted, then set the mark flag to true.       */
      /*===================================================*/

      else if (deftemplateArray == NULL)
        { markFact = true; }

      /*=======================================================*/
      /* Otherwise see if the fact's corresponding deftemplate */
      /* is in the list of deftemplates whose facts are to be  */
      /* saved. If it's in the list, then set the mark flag    */
      /* to true, otherwise set it to false.                   */
      /*=======================================================*/

      else
        {
         markFact = false;
         for (i = 0; i < count; i++)
           {
            if (deftemplateArray[i] == theFact->whichDeftemplate)
              {
               markFact = true;
               break;
              }
           }
        }

      /*============================*/
      /* If the mark flag is set to */
      /* true, then mark the fact.  */
      /*============================*/

      if (markFact)
        {
         factCount++;
         MarkSingleFact(theEnv,theFact,neededSpace);
        }
     }
     
   return factCount;
  }

/******************/
/* MarkSingleFact */
/******************/
static void MarkSingleFact(
  Environment *theEnv,
  Fact *theFact,
  size_t *neededSpace)
  {
   TemplateSlot *sp;
   unsigned short i;
   size_t j;
   
   *neededSpace += sizeof(unsigned long) + // Deftemplate name
                   sizeof(bool);           // Deftemplate type
                   
   theFact->whichDeftemplate->header.name->neededSymbol = true;
   
   if (theFact->whichDeftemplate->implied)
     {
      *neededSpace += (sizeof(unsigned short) + // Number of slots
                       sizeof(struct bsaveSlotValue) +
                       sizeof(unsigned long)); // Number of atoms

      CLIPSValue *theValue = &theFact->theProposition.contents[0];

      for (j = 0 ; j < theValue->multifieldValue->length ; j++)
        { MarkNeededAtom(theEnv,&theValue->multifieldValue->contents[j],neededSpace); }
     }
   else
     {
      *neededSpace += (sizeof(unsigned short) + // Number of slots
                       (sizeof(struct bsaveSlotValue) * theFact->whichDeftemplate->numberOfSlots) +
                       sizeof(unsigned long)); // Number of atoms
      
      sp = theFact->whichDeftemplate->slotList;
      for (i = 0 ; i < theFact->whichDeftemplate->numberOfSlots ; i++)
        {
         CLIPSValue *theValue = &theFact->theProposition.contents[i];
      
         sp->slotName->neededSymbol = true;

         if (theValue->header->type == MULTIFIELD_TYPE)
           {
            for (j = 0 ; j < theValue->multifieldValue->length ; j++)
              { MarkNeededAtom(theEnv,&theValue->multifieldValue->contents[j],neededSpace); }
           }
         else
           { MarkNeededAtom(theEnv,theValue,neededSpace); }
         sp = sp->next;
        }
     }
  }

/******************/
/* MarkNeededAtom */
/******************/
static void MarkNeededAtom(
  Environment *theEnv,
  CLIPSValue *theValue,
  size_t *neededSpace)
  {
   *neededSpace += sizeof(struct bsaveSlotValueAtom);

   /* =====================================
      Assumes slot value atoms  can only be
      floats, integers, symbols, strings,
      instance-names, instance-addresses,
      fact-addresses or external-addresses
      ===================================== */

   switch (theValue->header->type)
     {
      case SYMBOL_TYPE:
      case STRING_TYPE:
      case INSTANCE_NAME_TYPE:
         theValue->lexemeValue->neededSymbol = true;
         break;
      case FLOAT_TYPE:
         theValue->floatValue->neededFloat = true;
         break;
      case INTEGER_TYPE:
         theValue->integerValue->neededInteger = true;
         break;
#if OBJECT_SYSTEM
      case INSTANCE_ADDRESS_TYPE:
         GetFullInstanceName(theEnv,theValue->instanceValue)->neededSymbol = true;
         break;
#endif
     }
  }

/*********************/
/* WriteBinaryHeader */
/*********************/
static void WriteBinaryHeader(
  Environment *theEnv,
  FILE *filePtr)
  {
   fwrite(BINARY_FACTS_PREFIX_ID,
          (STD_SIZE) (strlen(BINARY_FACTS_PREFIX_ID) + 1),1,filePtr);
   fwrite(BINARY_FACTS_VERSION_ID,
          (STD_SIZE) (strlen(BINARY_FACTS_VERSION_ID) + 1),1,filePtr);
  }

/*******************/
/* SaveBinaryFacts */
/*******************/
static long SaveBinaryFacts(
  Environment *theEnv,
  FILE *filePtr,
  SaveScope saveCode,
  Deftemplate **deftemplateArray,
  unsigned int count,
  size_t *neededSpace)
  {
   Fact *theFact;
   Defmodule *theModule;
   unsigned int i;
   bool saveFact;
   long factCount = 0;

   /*=================*/
   /* Save the facts. */
   /*=================*/

   theModule = GetCurrentModule(theEnv);

   for (theFact = GetNextFactInScope(theEnv,NULL);
        theFact != NULL;
        theFact = GetNextFactInScope(theEnv,theFact))
     {
      /*===========================================================*/
      /* If we're doing a local save and the facts's corresponding */
      /* deftemplate isn't in the current module, then don't save  */
      /* the fact.                                                 */
      /*===========================================================*/

      if ((saveCode == LOCAL_SAVE) &&
          (theFact->whichDeftemplate->header.whichModule->theModule != theModule))
        { saveFact = false; }

      /*===================================================*/
      /* Otherwise, if the list of facts to be saved isn't */
      /* restricted, then set the mark flag to true.       */
      /*===================================================*/

      else if (deftemplateArray == NULL)
        { saveFact = true; }

      /*=======================================================*/
      /* Otherwise see if the fact's corresponding deftemplate */
      /* is in the list of deftemplates whose facts are to be  */
      /* saved. If it's in the list, then set the mark flag    */
      /* to true, otherwise set it to false.                   */
      /*=======================================================*/

      else
        {
         saveFact = false;
         for (i = 0; i < count; i++)
           {
            if (deftemplateArray[i] == theFact->whichDeftemplate)
              {
               saveFact = true;
               break;
              }
           }
        }

      /*============================*/
      /* If the mark flag is set to */
      /* true, then mark the fact.  */
      /*============================*/

      if (saveFact)
        {
         factCount++;
         SaveSingleFactBinary(theEnv,filePtr,theFact);
        }
     }
     
   return factCount;
  }

/************************/
/* SaveSingleFactBinary */
/************************/
static void SaveSingleFactBinary(
  Environment *theEnv,
  FILE *filePtr,
  Fact *theFact)
  {
   unsigned long nameIndex;
   unsigned short i;
   size_t j;
   struct bsaveSlotValue bs;
   unsigned long totalValueCount = 0;
   size_t slotLen;
   unsigned short slotCount;
   bool implied;
   
   /*=================================*/
   /* Write out the deftemplate name. */
   /*=================================*/
   
   nameIndex = theFact->whichDeftemplate->header.name->bucket;
   fwrite(&nameIndex,sizeof(unsigned long),1,filePtr);

   /*================================*/
   /* Save out the deftemplate type. */
   /*================================*/
   
   implied = theFact->whichDeftemplate->implied;
   fwrite(&implied,sizeof(bool),1,filePtr);
   
   /*================================*/
   /* Write out the number of slots. */
   /*================================*/

   if (theFact->whichDeftemplate->implied)
     { slotCount = 1; }
   else
     { slotCount = theFact->whichDeftemplate->numberOfSlots; }
     
   fwrite(&slotCount,sizeof(unsigned short),1,filePtr);

   /*============================================*/
   /* Write out the slot names and value counts. */
   /*============================================*/
   
   if (theFact->whichDeftemplate->implied)
     {
      CLIPSValue *theValue = &theFact->theProposition.contents[0];
      
      bs.slotName = ULONG_MAX;
      bs.valueCount = (unsigned long) theValue->multifieldValue->length;
      fwrite(&bs,sizeof(struct bsaveSlotValue),1,filePtr);
      totalValueCount += bs.valueCount;
     }
   else
     {
      TemplateSlot *sp = theFact->whichDeftemplate->slotList;
      for (i = 0 ; i < slotCount; i++)
        {
         CLIPSValue *theValue = &theFact->theProposition.contents[i];
            
         /*==================================================*/
         /* Write out the number of atoms in the slot value. */
         /*==================================================*/
      
         bs.slotName = sp->slotName->bucket;
         bs.valueCount = (unsigned long) (sp->multislot ? theValue->multifieldValue->length : 1);
         fwrite(&bs,sizeof(struct bsaveSlotValue),1,filePtr);
         totalValueCount += bs.valueCount;
         sp = sp->next;
        }
     }

   /*====================================*/
   /* Write out the number of slot value */
   /* atoms for the whole fact.          */
   /*====================================*/
   
   if (slotCount != 0)
     { fwrite(&totalValueCount,sizeof(unsigned long),1,filePtr); }

   /*=================================*/
   /* Write out the slot value atoms. */
   /*=================================*/

   if (theFact->whichDeftemplate->implied)
     {
      CLIPSValue *theValue = &theFact->theProposition.contents[0];
      
      for (j = 0 ; j < theValue->multifieldValue->length ; j++)
        { SaveAtomBinary(theEnv,&theValue->multifieldValue->contents[j],filePtr); }
     }
   else
     {
      TemplateSlot *sp = theFact->whichDeftemplate->slotList;
      for (i = 0 ; i < slotCount ; i++)
        {
         CLIPSValue *theValue = &theFact->theProposition.contents[i];

         slotLen = sp->multislot ? theValue->multifieldValue->length : 1;

         /*============================================*/
         /* Write out the type and index of each atom. */
         /*============================================*/
      
         if (sp->multislot)
           {
            for (j = 0 ; j < slotLen ; j++)
              { SaveAtomBinary(theEnv,&theValue->multifieldValue->contents[j],filePtr); }
           }
         else
           { SaveAtomBinary(theEnv,theValue,filePtr); }
           
         sp = sp->next;
        }
     }
  }

/******************/
/* SaveAtomBinary */
/******************/
static void SaveAtomBinary(
  Environment *theEnv,
  CLIPSValue *theValue,
  FILE *bsaveFP)
  {
   struct bsaveSlotValueAtom bsa;

   /* =====================================
      Assumes slot value atoms  can only be
      floats, integers, symbols, strings,
      instance-names, instance-addresses,
      fact-addresses or external-addresses
      ===================================== */

   bsa.type = theValue->header->type;
   switch (theValue->header->type)
     {
      case SYMBOL_TYPE:
      case STRING_TYPE:
      case INSTANCE_NAME_TYPE:
         bsa.value = theValue->lexemeValue->bucket;
         break;
      case FLOAT_TYPE:
         bsa.value = theValue->floatValue->bucket;
         break;
      case INTEGER_TYPE:
         bsa.value = theValue->integerValue->bucket;
         break;
#if OBJECT_SYSTEM
      case INSTANCE_ADDRESS_TYPE:
         bsa.type = INSTANCE_NAME_TYPE;
         bsa.value = GetFullInstanceName(theEnv,theValue->instanceValue)->bucket;
         break;
#endif
      default:
         bsa.value = ULONG_MAX;
     }
     
   fwrite(&bsa,sizeof(struct bsaveSlotValueAtom),1,bsaveFP);
  }


#endif /* DEFTEMPLATE_CONSTRUCT */
