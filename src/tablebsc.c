   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 7.00  03/08/25             */
   /*                                                     */
   /*         DEFTABLE BASIC COMMANDS HEADER FILE         */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements core commands for the deftable        */
/*   construct such as clear, save, undeftable, ppdeftable,  */
/*   list-deftables, and get-deftable-list.                  */
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
#include <string.h>

#include "argacces.h"
#include "constrct.h"
#include "cstrccom.h"
#include "cstrcpsr.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "extnfunc.h"
#include "memalloc.h"
#include "modulutl.h"
#include "multifld.h"
#include "pprint.h"
#include "prcdrfun.h"
#include "prcdrpsr.h"
#include "prntutil.h"
#include "router.h"
#include "scanner.h"
#include "strngrtr.h"
#include "tabledef.h"

#if BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE
#include "tablebin.h"
#endif

#if CONSTRUCT_COMPILER && (! RUN_TIME)
#include "tablecmp.h"
#endif

#include "tablebsc.h"

#define TABLE_COLUMN_REF ':'

typedef enum
  {
   QTT_ANY_ROWP = 0,
   QTT_FIND_ROW,
   QTT_DO_FOR_ROW,
   QTT_DO_FOR_ALL_ROWS,
   QTT_DO_FOR_THIS_ROW
  } QueryTableType;


/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static void                    SaveDeftable(Environment *,Defmodule *,const char *,void *);
   static Expression             *ParseQueryDoFor(Environment *,Expression *,const char *,bool,bool);
   static bool                    ReplaceTableNameWithReference(Environment *,Expression *,const char *);
   static Expression             *ParseQueryTestExpression(Environment *,const char *,const char *);
   static Expression             *ParseQueryActionExpression(Environment *,const char *,CLIPSLexeme *,const char *,
                                                             struct token *,bool *);
   static bool                    ReplaceTableVariables(Environment *,CLIPSLexeme *,Deftable *,Expression *,int);
   static bool                    IsQueryFunction(Expression *);
   static bool                    ReplaceColumnReference(Environment *,CLIPSLexeme *,Deftable *,Expression *,
                                                         struct functionDefinition *,int);
   static TABLE_QUERY_CORE       *FindQueryCore(Environment *,long long);
   static void                    PushQueryCore(Environment *);
   static void                    PopQueryCore(Environment *);
   static void                    CheckAllRows(Environment *,TABLE_QUERY_CORE *,QueryTableType);
   static void                    QueryDriver(Environment *,UDFContext *,UDFValue *,bool,QueryTableType,const char *);
   static void                    CheckOneRow(Environment *,TABLE_QUERY_CORE *,unsigned short,void *,const char *);
   static void                    NoRowForKeyError(Environment *,unsigned short,void *,const char *);
   static bool                    CheckForBindingRowVar(Environment *,CLIPSLexeme *,Expression *,
                                                        struct functionDefinition *,const char *);
   static CLIPSLexeme            *SplitVariable(Environment *,CLIPSLexeme *,CLIPSLexeme *);
   static void                    RebindTableMemberVariableError(Environment *,const char *,const char *);
   static void                    InvalidColumnError(Environment *,const char *,const char *,const char *);

/***************************************************************/
/* DeftableBasicCommands: Initializes basic deftable commands. */
/***************************************************************/
void DeftableBasicCommands(
  Environment *theEnv)
  {
   AddSaveFunction(theEnv,"deftable",SaveDeftable,0,NULL);

#if ! RUN_TIME
   AddUDF(theEnv,"contains-key","b",2,2,";y;synldm",ContainsKeyFunction,"ContainsKeyFunction",NULL);
   AddUDF(theEnv,"table-columns","m",1,1,"y",TableColumnsFunction,"TableColumnsFunction",NULL);
   AddUDF(theEnv,"table-row-count","l",1,1,"y",TableRowCountFunction,"TableRowCountFunction",NULL);
   AddUDF(theEnv,"lookup","*",3,3,";y;synldm;y",LookupFunction,"LookupFunction",NULL);

   AddUDF(theEnv,"get-deftable-list","m",0,1,"y",GetDeftableListFunction,"GetDeftableListFunction",NULL);
   AddUDF(theEnv,"undeftable","v",1,1,"y",UndeftableCommand,"UndeftableCommand",NULL);
   AddUDF(theEnv,"deftable-module","y",1,1,"y",DeftableModuleFunction,"DeftableModuleFunction",NULL);

   AddUDF(theEnv,"any-rowp","*",0,UNBOUNDED,NULL,QueryAnyRowp,"QueryAnyRowp",NULL);
   AddUDF(theEnv,"find-row","*",0,UNBOUNDED,NULL,QueryFindRow,"QueryFindRow",NULL);
   AddUDF(theEnv,"do-for-all-rows","*",0,UNBOUNDED,NULL,QueryDoForAllRows,"QueryDoForAllRows",NULL);
   AddUDF(theEnv,"do-for-row","*",0,UNBOUNDED,NULL,QueryDoForRow,"QueryDoForRow",NULL);
   AddUDF(theEnv,"do-for-this-row","*",0,UNBOUNDED,NULL,QueryDoForThisRow,"QueryDoForThisRow",NULL);
   
   AddUDF(theEnv,"(query-row)","f",0,UNBOUNDED,NULL,GetQueryRow,"GetQueryRow",NULL);
   AddUDF(theEnv,"(query-row-column)","*",0,UNBOUNDED,NULL,GetQueryRowColumn,"GetQueryRowColumn",NULL);

#if DEBUGGING_FUNCTIONS
   AddUDF(theEnv,"list-deftables","v",0,1,"y",ListDeftablesCommand,"ListDeftablesCommand",NULL);
   AddUDF(theEnv,"ppdeftable","vs",1,2,";y;ldsyn",PPDeftableCommand,"PPDeftableCommand",NULL);
#endif

#endif

   AddFunctionParser(theEnv,"any-rowp",RowParseQueryAction);
   AddFunctionParser(theEnv,"find-row",RowParseQueryAction);
   AddFunctionParser(theEnv,"do-for-all-rows",RowParseQueryAction);
   AddFunctionParser(theEnv,"do-for-row",RowParseQueryAction);
   AddFunctionParser(theEnv,"do-for-this-row",RowParseQueryAction);

#if (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE)
   DeftableBinarySetup(theEnv);
#endif

#if CONSTRUCT_COMPILER && (! RUN_TIME)
   DeftableCompilerSetup(theEnv);
#endif

  }

/***************************************/
/* SaveDeftable: Deftable save routine */
/*   for use with the save command.    */
/***************************************/
static void SaveDeftable(
  Environment *theEnv,
  Defmodule *theModule,
  const char *logicalName,
  void *context)
  {
   SaveConstruct(theEnv,theModule,logicalName,DeftableData(theEnv)->DeftableConstruct);
  }

/**************************************/
/* LookupFunction: H/L access routine */
/*   for the lookup command.          */
/**************************************/
void LookupFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue theArg;
   Deftable *theDeftable;
   const char *name;
   unsigned int colNum, rowNum;
   struct rcHashTableEntry *colEntry, *rowEntry;
   
   /*==================================*/
   /* Verify that the deftable exists. */
   /*==================================*/

   if (! UDFNthArgument(context,1,SYMBOL_BIT,&theArg))
     { return; }

   name = theArg.lexemeValue->contents;

   theDeftable = FindDeftable(theEnv,name);

   if (theDeftable == NULL)
     {
      CantFindItemErrorMessage(theEnv,"deftable",name,true);
      SetEvaluationError(theEnv,true);
      return;
     }

   /*=============================*/
   /* Verify that the row exists. */
   /*=============================*/

   if (! UDFNthArgument(context,2,NUMBER_BITS | LEXEME_BITS | INSTANCE_NAME_BIT | MULTIFIELD_BIT,&theArg))
     { return; }

   if (theDeftable->rows == NULL)
     {
      NoRowForKeyError(theEnv,theArg.header->type,theArg.value,"lookup");
      return;
     }

   rowEntry = FindTableEntryForRow(theEnv,theDeftable,theArg.header->type,theArg.value);

   if (rowEntry == NULL)
     {
      NoRowForKeyError(theEnv,theArg.header->type,theArg.value,"lookup");
      return;
     }

   rowNum = rowEntry->itemIndex;
     
   /*================================*/
   /* Verify that the column exists. */
   /*================================*/

   if (! UDFNthArgument(context,3,NUMBER_BITS | LEXEME_BITS | INSTANCE_NAME_BIT,&theArg))
     { return; }
     
   colEntry = FindTableEntryForColumn(theEnv,theDeftable,theArg.header->type,theArg.value);

   if (colEntry == NULL)
     {
      PrintErrorID(theEnv,"DFTBLBSC",4,false);
      WriteString(theEnv,STDERR,"Unable to find column ");
      PrintAtom(theEnv,STDERR,theArg.header->type,theArg.value);
      WriteString(theEnv,STDERR," in function lookup.\n");
      SetEvaluationError(theEnv,true);
      return;
     }

   colNum = colEntry->itemIndex;

   /*========================================*/
   /* Retrieve the row value for the column. */
   /*========================================*/
   
   GetTableValue(theEnv,theDeftable,rowNum,colNum,returnValue);
  }

/*****************/
/* GetTableValue */
/*****************/
void GetTableValue(
  Environment *theEnv,
  Deftable *theDeftable,
  unsigned int rowNum,
  unsigned int colNum,
  UDFValue *returnValue)
  {
   size_t index;
   struct expr *value;
   size_t mfSize;
   struct multifield *theMultifield;

   index = (rowNum * theDeftable->columnCount) + colNum;
   value = &theDeftable->rows[index];

   if (value->type == QUANTITY_TYPE)
     {
      mfSize = (size_t) value->integerValue->contents;
      theMultifield = CreateMultifield(theEnv,mfSize);
      
      for (value = value->nextArg, index = 0;
           value != NULL;
           value = value->nextArg, index++)
        { theMultifield->contents[index].value = value->value; }
        
      returnValue->begin = 0;
      returnValue->range = mfSize;
      returnValue->value = theMultifield;
     }
   else
     { returnValue->value = value->value; }
  }

/*******************************************/
/* ContainsKeyFunction: H/L access routine */
/*   for the contains-key function.        */
/*******************************************/
void ContainsKeyFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue theArg;
   Deftable *theDeftable;
   const char *name;
   struct rcHashTableEntry *rowEntry;
   
   returnValue->lexemeValue = FalseSymbol(theEnv);
   
   /*==================================*/
   /* Verify that the deftable exists. */
   /*==================================*/

   if (! UDFNthArgument(context,1,SYMBOL_BIT,&theArg))
     { return; }

   name = theArg.lexemeValue->contents;

   theDeftable = FindDeftable(theEnv,name);

   if (theDeftable == NULL)
     {
      CantFindItemErrorMessage(theEnv,"deftable",name,true);
      SetEvaluationError(theEnv,true);
      return;
     }

   /*==============================*/
   /* Determine if the row exists. */
   /*==============================*/

   if (! UDFNthArgument(context,2,NUMBER_BITS | LEXEME_BITS | INSTANCE_NAME_BIT | MULTIFIELD_BIT,&theArg))
     { return; }

   if (theDeftable->rows == NULL)
     {
      NoRowForKeyError(theEnv,theArg.header->type,theArg.value,"lookup");
      return;
     }
     
   rowEntry = FindTableEntryForRow(theEnv,theDeftable,theArg.header->type,theArg.value);

   if (rowEntry != NULL)
     { returnValue->lexemeValue = TrueSymbol(theEnv); }
  }
  
  
/*******************************************/
/* UndeftableCommand: H/L access routine   */
/*   for the undeftable command.           */
/*******************************************/
void UndeftableCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UndefconstructCommand(context,"undeftable",DeftableData(theEnv)->DeftableConstruct);
  }

/*********************************/
/* Undeftable: C access routine  */
/*   for the undeftable command. */
/*********************************/
bool Undeftable(
  Deftable *theDeftable,
  Environment *allEnv)
  {
   Environment *theEnv;
   
   if (theDeftable == NULL)
     {
      theEnv = allEnv;
      return Undefconstruct(theEnv,NULL,DeftableData(theEnv)->DeftableConstruct);
     }
   else
     {
      theEnv = theDeftable->header.env;
      return Undefconstruct(theEnv,&theDeftable->header,DeftableData(theEnv)->DeftableConstruct);
     }
  }

/*************************************************/
/* GetDeftableListFunction: H/L access routine   */
/*   for the get-deftable-list function.         */
/*************************************************/
void GetDeftableListFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   GetConstructListFunction(context,returnValue,DeftableData(theEnv)->DeftableConstruct);
  }

/*****************************************/
/* GetDeftableList: C access routine     */
/*   for the get-deftable-list function. */
/*****************************************/
void GetDeftableList(
  Environment *theEnv,
  CLIPSValue *returnValue,
  Defmodule *theModule)
  {
   UDFValue result;
   
   GetConstructList(theEnv,&result,DeftableData(theEnv)->DeftableConstruct,theModule);
   NormalizeMultifield(theEnv,&result);
   returnValue->value = result.value;
  }

/************************************************/
/* DeftableModuleFunction: H/L access routine   */
/*   for the deftable-module function.          */
/************************************************/
void DeftableModuleFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   returnValue->value = GetConstructModuleCommand(context,"deftable-module",DeftableData(theEnv)->DeftableConstruct);
  }

#if DEBUGGING_FUNCTIONS

/*******************************************/
/* PPDeftableCommand: H/L access routine   */
/*   for the ppdeftable command.           */
/*******************************************/
void PPDeftableCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   PPConstructCommand(context,"ppdeftable",DeftableData(theEnv)->DeftableConstruct,returnValue);
  }

/************************************/
/* PPDeftable: C access routine for */
/*   the ppdeftable command.        */
/************************************/
bool PPDeftable(
  Environment *theEnv,
  const char *deftableName,
  const char *logicalName)
  {
   return(PPConstruct(theEnv,deftableName,logicalName,DeftableData(theEnv)->DeftableConstruct));
  }

/**********************************************/
/* ListDeftablesCommand: H/L access routine   */
/*   for the list-deftables command.          */
/**********************************************/
void ListDeftablesCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   ListConstructCommand(context,DeftableData(theEnv)->DeftableConstruct);
  }

/*************************************/
/* ListDeftables: C access routine   */
/*   for the list-deftables command. */
/*************************************/
void ListDeftables(
  Environment *theEnv,
  const char *logicalName,
  Defmodule *theModule)
  {
   ListConstruct(theEnv,DeftableData(theEnv)->DeftableConstruct,logicalName,theModule);
  }

#endif /* DEBUGGING_FUNCTIONS */

/********************************************/
/* TableColumnsFunction: H/L access routine */
/*   for the table-columns function.        */
/********************************************/
void TableColumnsFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue theArg;
   Deftable *theDeftable;
   const char *name;
   Multifield *theList;
   unsigned long i;
   
   /*==================================*/
   /* Verify that the deftable exists. */
   /*==================================*/

   if (! UDFNthArgument(context,1,SYMBOL_BIT,&theArg))
     { return; }

   name = theArg.lexemeValue->contents;

   theDeftable = FindDeftable(theEnv,name);

   if (theDeftable == NULL)
     {
      CantFindItemErrorMessage(theEnv,"deftable",name,true);
      SetEvaluationError(theEnv,true);
      return;
     }
     
   returnValue->begin = 0;
   returnValue->range = theDeftable->columnCount;
   theList = CreateMultifield(theEnv,theDeftable->columnCount);
   for (i = 0; i < theDeftable->columnCount; i++)
     { theList->contents[i].value = theDeftable->columns[i].value; }

   returnValue->value = theList;
  }

/*********************************************/
/* TableRowCountFunction: H/L access routine */
/*   for the table-row-count function.       */
/*********************************************/
void TableRowCountFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue theArg;
   Deftable *theDeftable;
   const char *name;
   
   /*==================================*/
   /* Verify that the deftable exists. */
   /*==================================*/

   if (! UDFNthArgument(context,1,SYMBOL_BIT,&theArg))
     { return; }

   name = theArg.lexemeValue->contents;

   theDeftable = FindDeftable(theEnv,name);

   if (theDeftable == NULL)
     {
      CantFindItemErrorMessage(theEnv,"deftable",name,true);
      SetEvaluationError(theEnv,true);
      return;
     }
     
   returnValue->integerValue = CreateInteger(theEnv,(long long) theDeftable->rowCount);
  }
  
/****************/
/* QueryAnyRowp */
/****************/
void QueryAnyRowp(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   QueryDriver(theEnv,context,returnValue,false,QTT_ANY_ROWP,"any-rowp");
  }

/****************/
/* QueryFindRow */
/****************/
void QueryFindRow(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   QueryDriver(theEnv,context,returnValue,false,QTT_FIND_ROW,"find-row");
  }
  
/*****************/
/* QueryDoForRow */
/****************/
void QueryDoForRow(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   QueryDriver(theEnv,context,returnValue,true,QTT_DO_FOR_ROW,"do-for-row");
  }

/*********************/
/* QueryDoForAllRows */
/*********************/
void QueryDoForAllRows(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   QueryDriver(theEnv,context,returnValue,true,QTT_DO_FOR_ALL_ROWS,"do-for-all-rows");
  }

/*********************/
/* QueryDoForThisRow */
/*********************/
void QueryDoForThisRow(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   QueryDriver(theEnv,context,returnValue,true,QTT_DO_FOR_THIS_ROW,"do-for-this-row");
  }
  
/***************/
/* QueryDriver */
/***************/
static void QueryDriver(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue,
  bool hasActions,
  QueryTableType theQueryType,
  const char *functionName)
  {
   Deftable *theDeftable;
   Expression *tableExp, *rowExp;
   Expression *queryExp, *actionExp;
   UDFValue temp, rowKey;
   unsigned int count;
   
   returnValue->lexemeValue = FalseSymbol(theEnv);
   
   tableExp = GetFirstArgument()->nextArg;
   
   if (tableExp->type == DEFTABLE_PTR)
     { theDeftable = (Deftable *) tableExp->value; }
   else
     {
      if (EvaluateExpression(theEnv,tableExp,&temp))
        { return; }
        
      if (temp.header->type == SYMBOL_TYPE)
        {
         theDeftable = (Deftable *)
                       FindImportedConstruct(theEnv,"deftable",NULL,temp.lexemeValue->contents,&count,true,NULL);
         if (theDeftable == NULL)
           {
            CantFindItemInFunctionErrorMessage(theEnv,"deftable",temp.lexemeValue->contents,"do-for-all-rows",true);
            return;
           }
        }
      else if (tableExp->type == DEFTABLE_PTR)
        { theDeftable = (Deftable *) temp.value; }
      else
        {
         PrintErrorID(theEnv,"DFTBLBSC",2,false);
         WriteString(theEnv,STDERR,"The value ");
         WriteString(theEnv,STDERR,DataObjectToString(theEnv,&temp));
         WriteString(theEnv,STDERR," is not a valid table name in function ");
         WriteString(theEnv,STDERR,functionName);
         WriteString(theEnv,STDERR,".\n");
         return;
        }
     }
   
   if (theQueryType == QTT_DO_FOR_THIS_ROW)
     {
      rowExp = tableExp->nextArg;
      
      if (EvaluateExpression(theEnv,rowExp,&rowKey))
        { return; }
        
      if ((rowKey.header->type != SYMBOL_TYPE) &&
          (rowKey.header->type != STRING_TYPE) &&
          (rowKey.header->type != INSTANCE_NAME_TYPE) &&
          (rowKey.header->type != FLOAT_TYPE) &&
          (rowKey.header->type != INTEGER_TYPE) &&
          (rowKey.header->type != MULTIFIELD_TYPE))
        {
         PrintErrorID(theEnv,"DFTBLBSC",3,false);
         WriteString(theEnv,STDERR,"The value ");
         WriteString(theEnv,STDERR,DataObjectToString(theEnv,&rowKey));
         WriteString(theEnv,STDERR," is not a valid key in function ");
         WriteString(theEnv,STDERR,functionName);
         WriteString(theEnv,STDERR,".\n");
         return;
        }
        
      queryExp = rowExp->nextArg;
     }
   else
     { queryExp = tableExp->nextArg; }
   
   actionExp = queryExp->nextArg;
     
   PushQueryCore(theEnv);

   DeftableData(theEnv)->QueryCore = get_struct(theEnv,table_query_core);
   DeftableData(theEnv)->QueryCore->rowVar = GetFirstArgument()->lexemeValue;
   DeftableData(theEnv)->QueryCore->table = theDeftable;
   DeftableData(theEnv)->QueryCore->query = queryExp;
   DeftableData(theEnv)->QueryCore->action = actionExp;
   DeftableData(theEnv)->QueryCore->result = returnValue;
      
   RetainUDFV(theEnv,DeftableData(theEnv)->QueryCore->result);

   if (theQueryType == QTT_DO_FOR_THIS_ROW)
     { CheckOneRow(theEnv,DeftableData(theEnv)->QueryCore,rowKey.header->type,rowKey.value,functionName); }
   else
     { CheckAllRows(theEnv,DeftableData(theEnv)->QueryCore,theQueryType); }

   ReleaseUDFV(theEnv,DeftableData(theEnv)->QueryCore->result);

   DeftableData(theEnv)->AbortQuery = false;
   ProcedureFunctionData(theEnv)->BreakFlag = false;
   
   rtn_struct(theEnv,table_query_core,DeftableData(theEnv)->QueryCore);
   PopQueryCore(theEnv);
  }
  
/****************/
/* CheckAllRows */
/****************/
static void CheckAllRows(
  Environment *theEnv,
  TABLE_QUERY_CORE *core,
  QueryTableType theQueryType)
  {
   unsigned int r;
   Deftable *theDeftable;
   UDFValue temp;

   theDeftable = core->table;
   
   DeftableData(theEnv)->AbortQuery = true;
   for (r = 0; r < theDeftable->rowCount; r++)
     {
      DeftableData(theEnv)->AbortQuery = false;

      core->row = r;
      
      EvaluateExpression(theEnv,core->query,&temp);
      
      if (temp.value != FalseSymbol(theEnv))
        {
         switch (theQueryType)
           {
            case QTT_ANY_ROWP:
              ReleaseUDFV(theEnv,DeftableData(theEnv)->QueryCore->result);
              DeftableData(theEnv)->QueryCore->result->lexemeValue = TrueSymbol(theEnv);
              RetainUDFV(theEnv,DeftableData(theEnv)->QueryCore->result);
              return;
              
            case QTT_FIND_ROW:
              ReleaseUDFV(theEnv,DeftableData(theEnv)->QueryCore->result);
              GetTableValue(theEnv,theDeftable,r,0,DeftableData(theEnv)->QueryCore->result);
              RetainUDFV(theEnv,DeftableData(theEnv)->QueryCore->result);
              return;

            case QTT_DO_FOR_ROW:
              ReleaseUDFV(theEnv,DeftableData(theEnv)->QueryCore->result);
              EvaluateExpression(theEnv,core->action,DeftableData(theEnv)->QueryCore->result);
              RetainUDFV(theEnv,DeftableData(theEnv)->QueryCore->result);
              return;
            
            case QTT_DO_FOR_ALL_ROWS:
              ReleaseUDFV(theEnv,DeftableData(theEnv)->QueryCore->result);
              EvaluateExpression(theEnv,core->action,DeftableData(theEnv)->QueryCore->result);
              RetainUDFV(theEnv,DeftableData(theEnv)->QueryCore->result);
              break;
              
            case QTT_DO_FOR_THIS_ROW:
              /* Handled by CheckOneRow. */
              break;
           }
        }

      if ((EvaluationData(theEnv)->HaltExecution == true) ||
          (DeftableData(theEnv)->AbortQuery == true))
        { return; }
     }
  }
  
/***************/
/* CheckOneRow */
/***************/
static void CheckOneRow(
  Environment *theEnv,
  TABLE_QUERY_CORE *core,
  unsigned short theType,
  void *theValue,
  const char *functionName)
  {
   Deftable *theDeftable;
   UDFValue temp;
   struct rcHashTableEntry *theRow;

   theDeftable = core->table;
      
   theRow = FindTableEntryForRow(theEnv,theDeftable,theType,theValue);
   if (theRow == NULL)
     {
      NoRowForKeyError(theEnv,theType,theValue,functionName);
      return;
     }

   DeftableData(theEnv)->AbortQuery = false;

   core->row = theRow->itemIndex;
      
   EvaluateExpression(theEnv,core->query,&temp);
      
   if (temp.value != FalseSymbol(theEnv))
     {
      ReleaseUDFV(theEnv,DeftableData(theEnv)->QueryCore->result);
      EvaluateExpression(theEnv,core->action,DeftableData(theEnv)->QueryCore->result);
      RetainUDFV(theEnv,DeftableData(theEnv)->QueryCore->result);
      return;
     }
      
   if ((EvaluationData(theEnv)->HaltExecution == true) ||
       (DeftableData(theEnv)->AbortQuery == true))
     { return; }
  }
  
/***********************/
/* RowParseQueryAction */
/***********************/
Expression *RowParseQueryAction(
  Environment *theEnv,
  Expression *top,
  const char *readSource)
  {
   if (top->functionValue->functionPointer == QueryDoForThisRow)
     { return ParseQueryDoFor(theEnv,top,readSource,true,true); }
   else if ((top->functionValue->functionPointer == QueryAnyRowp) ||
            (top->functionValue->functionPointer == QueryFindRow))
     { return ParseQueryDoFor(theEnv,top,readSource,false,false); }

   return ParseQueryDoFor(theEnv,top,readSource,false,true);
  }

/*******************/
/* ParseQueryDoFor */
/*******************/
static Expression *ParseQueryDoFor(
  Environment *theEnv,
  Expression *top,
  const char *readSource,
  bool hasRowArgument,
  bool hasActions)
  {
   Expression *tableExp, *testExp, *actionExp, *rowExp, *endExp;
   bool error = false;
   const char *functionName;
   struct token inputToken;
   Deftable *theDeftable = NULL;
   struct rcHashTableEntry *theRow;

   functionName = ExpressionFunctionCallName(top)->contents;

   SavePPBuffer(theEnv," ");

   /*==========================================*/
   /* The restriction begins with the variable */
   /* that will contain the queried table.     */
   /*==========================================*/
   
   GetToken(theEnv,readSource,&inputToken);

   if (inputToken.tknType != LEFT_PARENTHESIS_TOKEN)
     {
      PrintErrorID(theEnv,"TABLEBSC",5,true);
      WriteString(theEnv,STDERR,"Expected a '(' to begin the table restriction for function ");
      WriteString(theEnv,STDERR,functionName);
      WriteString(theEnv,STDERR,".\n");
      ReturnExpression(theEnv,top);
      return NULL;
     }

   GetToken(theEnv,readSource,&inputToken);
   if (inputToken.tknType != SF_VARIABLE_TOKEN)
     {
      PrintErrorID(theEnv,"TABLEBSC",6,true);
      WriteString(theEnv,STDERR,"Expected a variable after the opening '(' of the table restriction for function ");
      WriteString(theEnv,STDERR,functionName);
      WriteString(theEnv,STDERR,".\n");
      ReturnExpression(theEnv,top);
      return NULL;
     }

   top->argList = GenConstant(theEnv,SYMBOL_TYPE,inputToken.value);

   /*===========================================================*/
   /* An expression follows the variable. This will either be a */
   /* symbol that can resolved while parsing or it will be an   */
   /* expression that will be evaluated and checked at runtime. */
   /*===========================================================*/
   
   SavePPBuffer(theEnv," ");

   tableExp = ArgumentParse(theEnv,readSource,&error);

   if (error)
     {
      ReturnExpression(theEnv,tableExp);
      ReturnExpression(theEnv,top);
      return NULL;
     }

   if (tableExp == NULL)
     {
      PrintErrorID(theEnv,"TABLEBSC",7,true);
      WriteString(theEnv,STDERR,"Expected an expression after the row variable in the table restriction for function ");
      WriteString(theEnv,STDERR,functionName);
      WriteString(theEnv,STDERR,".\n");

      ReturnExpression(theEnv,top);
      return NULL;
     }

   if (ReplaceTableNameWithReference(theEnv,tableExp,functionName) == false)
     {
      ReturnExpression(theEnv,tableExp);
      ReturnExpression(theEnv,top);
      return NULL;
     }

   if (tableExp->type == DEFTABLE_PTR)
     { theDeftable = (Deftable *) tableExp->constructValue; }

   top->argList->nextArg = tableExp;
   endExp = tableExp;

   /*==================================*/
   /* The do-for-this-row function has */
   /* an additional row argument.      */
   /*==================================*/
   
   if (hasRowArgument)
     {
      SavePPBuffer(theEnv," ");

      rowExp = ArgumentParse(theEnv,readSource,&error);

      if (error)
        {
         SyntaxErrorMessage(theEnv,"table query function");
         ReturnExpression(theEnv,top);
         return NULL;
        }
        
      if (rowExp == NULL)
        {
         PrintErrorID(theEnv,"TABLEBSC",8,false);
         WriteString(theEnv,STDERR,"Expected a row expression to follow the table restriction.\n");
         ReturnExpression(theEnv,top);
         return NULL;
        }
      
      if ((rowExp->type == SYMBOL_TYPE) ||
          (rowExp->type == STRING_TYPE) ||
          (rowExp->type == INSTANCE_NAME_TYPE) ||
          (rowExp->type == FLOAT_TYPE) ||
          (rowExp->type == INTEGER_TYPE))
        {
         theRow = FindTableEntryForRow(theEnv,theDeftable,rowExp->type,rowExp->value);
         if (theRow == NULL)
           {
            NoRowForKeyError(theEnv,rowExp->type,rowExp->value,functionName);
            ReturnExpression(theEnv,top);
            ReturnExpression(theEnv,rowExp);
            return NULL;
           }
        }

      endExp->nextArg = rowExp;
      endExp = rowExp;
     }

   /*=========================*/
   /* Closing of restriction. */
   /*=========================*/
   
   GetToken(theEnv,readSource,&inputToken);

   if (inputToken.tknType != RIGHT_PARENTHESIS_TOKEN)
     {
      PPBackup(theEnv);
      SavePPBuffer(theEnv," ");
      SavePPBuffer(theEnv,inputToken.printForm);
      PrintErrorID(theEnv,"TABLEBSC",9,true);
      WriteString(theEnv,STDERR,"Expected a ')' to end the table restriction for function ");
      WriteString(theEnv,STDERR,functionName);
      WriteString(theEnv,STDERR,".\n");
      ReturnExpression(theEnv,top);
      return NULL;
     }
     
   IncrementIndentDepth(theEnv,3);
   PPCRAndIndent(theEnv);
   
   /*============================*/
   /* Parse the test expression. */
   /*============================*/
   
   if ((testExp = ParseQueryTestExpression(theEnv,functionName,readSource)) == NULL)
     {
      DecrementIndentDepth(theEnv,3);
      ReturnExpression(theEnv,top);
      return NULL;
     }

   PPCRAndIndent(theEnv);

   /*====================*/
   /* Parse the actions. */
   /*====================*/
   
   if (hasActions == false)
     {
      GetToken(theEnv,readSource,&inputToken);
      actionExp = NULL;
     }
   else
     {
      actionExp = ParseQueryActionExpression(theEnv,functionName,top->argList->lexemeValue,readSource,&inputToken,&error);
      DecrementIndentDepth(theEnv,3);
   
      if (error)
        {
         ReturnExpression(theEnv,testExp);
         ReturnExpression(theEnv,top);
         return NULL;
        }

      if (CheckForBindingRowVar(theEnv,top->argList->lexemeValue,actionExp,FindFunction(theEnv,"bind"),functionName))
        {
         ReturnExpression(theEnv,actionExp);
         ReturnExpression(theEnv,testExp);
         ReturnExpression(theEnv,top);
         return NULL;
        }
     }
     
   /*======================*/
   /* Closing of function. */
   /*======================*/

   if (inputToken.tknType != RIGHT_PARENTHESIS_TOKEN)
     {
      ReturnExpression(theEnv,actionExp);
      ReturnExpression(theEnv,testExp);
      ReturnExpression(theEnv,top);
      return NULL;
     }

   if (ReplaceTableVariables(theEnv,top->argList->lexemeValue,theDeftable,testExp,0))
     {
      ReturnExpression(theEnv,actionExp);
      ReturnExpression(theEnv,testExp);
      ReturnExpression(theEnv,top);
      return NULL;
     }

   if (ReplaceTableVariables(theEnv,top->argList->lexemeValue,theDeftable,actionExp,0))
     {
      ReturnExpression(theEnv,actionExp);
      ReturnExpression(theEnv,testExp);
      ReturnExpression(theEnv,top);
      return NULL;
     }

   endExp->nextArg = testExp;
   testExp->nextArg = actionExp;

   return top;
  }

/****************************/
/* ParseQueryTestExpression */
/****************************/
static Expression *ParseQueryTestExpression(
  Environment *theEnv,
  const char *functionName,
  const char *readSource)
  {
   Expression *qtest;
   bool error;
   struct BindInfo *oldBindList;

   error = false;
   oldBindList = GetParsedBindNames(theEnv);
   SetParsedBindNames(theEnv,NULL);

   qtest = ArgumentParse(theEnv,readSource,&error);

   if (error == true)
     {
      ClearParsedBindNames(theEnv);
      SetParsedBindNames(theEnv,oldBindList);
      return NULL;
     }

   if (qtest == NULL)
     {
      ClearParsedBindNames(theEnv);
      SetParsedBindNames(theEnv,oldBindList);
      PrintErrorID(theEnv,"TABLEBSC",10,false);
      WriteString(theEnv,STDERR,"Expected a query expression to follow the table restriction.\n");
      return NULL;
     }
     
   if (ParsedBindNamesEmpty(theEnv) == false)
     {
      ReturnExpression(theEnv,qtest);
      ClearParsedBindNames(theEnv);
      SetParsedBindNames(theEnv,oldBindList);
      PrintErrorID(theEnv,"TABLEBSC",11,false);
      WriteString(theEnv,STDERR,"Binds are not allowed in table query in function ");
      WriteString(theEnv,STDERR,functionName);
      WriteString(theEnv,STDERR,".\n");
      return NULL;
     }

   SetParsedBindNames(theEnv,oldBindList);

   return qtest;
  }

/******************************/
/* ParseQueryActionExpression */
/******************************/
static Expression *ParseQueryActionExpression(
  Environment *theEnv,
  const char *functionName,
  CLIPSLexeme *variable,
  const char *readSource,
  struct token *queryInputToken,
  bool *error)
  {
   Expression *qaction;
   struct BindInfo *oldBindList, *newBindList, *prev;

   oldBindList = GetParsedBindNames(theEnv);
   SetParsedBindNames(theEnv,NULL);

   ExpressionData(theEnv)->BreakContext = true;
   ExpressionData(theEnv)->ReturnContext = ExpressionData(theEnv)->svContexts->rtn;

   qaction = GroupActions(theEnv,readSource,queryInputToken,true,NULL,false);

   PPBackup(theEnv);
   PPBackup(theEnv);
   SavePPBuffer(theEnv,queryInputToken->printForm);

   ExpressionData(theEnv)->BreakContext = false;

   if (qaction == NULL)
     {
      ClearParsedBindNames(theEnv);
      SetParsedBindNames(theEnv,oldBindList);
      SyntaxErrorMessage(theEnv,"table query function");
      *error = true;
      return NULL;
     }

   newBindList = GetParsedBindNames(theEnv);
   prev = NULL;
   while (newBindList != NULL)
     {
      if (variable == (void *) newBindList->name)
        {
         ClearParsedBindNames(theEnv);
         SetParsedBindNames(theEnv,oldBindList);
         RebindTableMemberVariableError(theEnv,variable->contents,functionName);
         ReturnExpression(theEnv,qaction);
         *error = true;
         return NULL;
        }
        
      prev = newBindList;
      newBindList = newBindList->next;
     }

   if (prev == NULL)
     { SetParsedBindNames(theEnv,oldBindList); }
   else
     { prev->next = oldBindList; }

   return qaction;
  }
  
/*********************************/
/* ReplaceTableNameWithReference */
/*********************************/
static bool ReplaceTableNameWithReference(
  Environment *theEnv,
  Expression *theExp,
  const char *functionName)
  {
   const char *theTableName;
   Deftable *theDeftable;
   unsigned int count;

   if (theExp->type == SYMBOL_TYPE)
     {
      theTableName = theExp->lexemeValue->contents;

      theDeftable = (Deftable *)
                    FindImportedConstruct(theEnv,"deftable",NULL,theTableName,
                                          &count,true,NULL);

      if (theDeftable == NULL)
        {
         CantFindItemErrorMessage(theEnv,"deftable",theTableName,true);
         return false;
        }

      if (count > 1)
        {
         AmbiguousReferenceErrorMessage(theEnv,"deftable",theTableName);
         return false;
        }

      theExp->type = DEFTABLE_PTR;
      theExp->value = theDeftable;

#if (! RUN_TIME) && (! BLOAD_ONLY)
      if (! ConstructData(theEnv)->ParsingConstruct)
        { ConstructData(theEnv)->DanglingConstructs++; }
#endif
     }
   else if ((theExp->type == INTEGER_TYPE) ||
            (theExp->type == FLOAT_TYPE) ||
            (theExp->type == STRING_TYPE) ||
            (theExp->type == INSTANCE_NAME_TYPE))
     {
      PrintErrorID(theEnv,"TABLEBSC",13,false);
      WriteString(theEnv,STDERR,"The table name in the restriction must be a symbol in function ");
      WriteString(theEnv,STDERR,functionName);
      WriteString(theEnv,STDERR,".\n");

      return false;
     }

   return true;
  }

/*************************/
/* ReplaceTableVariables */
/*************************/
static bool ReplaceTableVariables(
  Environment *theEnv,
  CLIPSLexeme *tableVariable,
  Deftable *theDeftable,
  Expression *bexp,
  int ndepth)
  {
   Expression *eptr;
   struct functionDefinition *rindx_func, *rslot_func;

   rindx_func = FindFunction(theEnv,"(query-row)");
   rslot_func = FindFunction(theEnv,"(query-row-column)");
   
   while (bexp != NULL)
     {
      if (bexp->type == SF_VARIABLE)
        {
         if (tableVariable == bexp->value)
           {
            bexp->type = FCALL;
            bexp->value = rindx_func;
            eptr = GenConstant(theEnv,INTEGER_TYPE,CreateInteger(theEnv,ndepth));
            bexp->argList = eptr;
           }
         else
           {
            if (ReplaceColumnReference(theEnv,tableVariable,theDeftable,bexp,rslot_func,ndepth))
              { return true; }
           }
        }
        
      if (bexp->argList != NULL)
        {
         if (IsQueryFunction(bexp))
           {
            if (ReplaceTableVariables(theEnv,tableVariable,theDeftable,bexp->argList,ndepth+1))
              { return true; }
           }
         else
           {
            if (ReplaceTableVariables(theEnv,tableVariable,theDeftable,bexp->argList,ndepth))
              { return true; }
           }
        }
      bexp = bexp->nextArg;
     }
     
   return false;
  }
  
/*******************/
/* IsQueryFunction */
/*******************/
static bool IsQueryFunction(
  Expression *theExp)
  {
   UserDefinedFunction *theFunction;

   if (theExp->type != FCALL)
     { return false; }
     
   theFunction = theExp->functionValue->functionPointer;
   
   if ((theFunction == QueryDoForThisRow) ||
       (theFunction == QueryDoForRow) ||
       (theFunction == QueryDoForAllRows))
     { return true; }

   return false;
  }

/**************************/
/* ReplaceColumnReference */
/**************************/
static bool ReplaceColumnReference(
  Environment *theEnv,
  CLIPSLexeme *tableVariable,
  Deftable *theDeftable,
  Expression *theExp,
  struct functionDefinition *func,
  int ndepth)
  {
   CLIPSLexeme *columnName, *variableColumn;
   struct rcHashTableEntry *colEntry;
   
   columnName = SplitVariable(theEnv,theExp->lexemeValue,tableVariable);
   if (columnName == NULL)
     { return false; }
     
   if (theDeftable != NULL)
     {
      colEntry = FindTableEntryForColumn(theEnv,theDeftable,SYMBOL_TYPE,columnName);
      if (colEntry == NULL)
        {
         InvalidColumnError(theEnv,theDeftable->header.name->contents,tableVariable->contents,columnName->contents);
         return true;
        }
     }
     
   // TBD Issue Warning
   // TBD Check to see if the table is defined and then
   // TBD Check that the column name exists
   
   variableColumn = theExp->lexemeValue;
   
   theExp->type = FCALL;
   theExp->value = func;
   theExp->argList = GenConstant(theEnv,INTEGER_TYPE,CreateInteger(theEnv,ndepth));
   theExp->argList->nextArg = GenConstant(theEnv,SYMBOL_TYPE,columnName);
   theExp->argList->nextArg->nextArg = GenConstant(theEnv,SYMBOL_TYPE,variableColumn); // TBD Is this arg needed?
   return false;
  }
  
/***************************************************************/
/* SplitVariable: Determine if a variable includes a colon     */
/*   character between a query variable and an attribute name. */
/*   If so, returns the attribute name, otherwise NULL.        */
/***************************************************************/
CLIPSLexeme *SplitVariable(
  Environment *theEnv,
  CLIPSLexeme *theVariable,
  CLIPSLexeme *rowVariable)
  {
   size_t len;
   const char *pos;
   
   pos = strchr(theVariable->contents,':');
   if (pos == NULL)
     { return NULL; }
     
   len = (size_t) (pos - theVariable->contents);
   
   if (strncmp(rowVariable->contents,theVariable->contents,len))
     { return NULL; }
   
   return CreateSymbol(theEnv,&theVariable->contents[len+1]);
  }
  
/*************************/
/* CheckForBindingRowVar */
/*************************/
static bool CheckForBindingRowVar(
  Environment *theEnv,
  CLIPSLexeme *rowVariable,
  Expression *theExpression,
  struct functionDefinition *bindFunction,
  const char *functionName)
  {
   CLIPSLexeme *bindVariable;
   
   while (theExpression != NULL)
     {
      if ((theExpression->type == FCALL) &&
          (theExpression->functionValue == bindFunction))
        {
         bindVariable = theExpression->argList->lexemeValue;
         if (SplitVariable(theEnv,bindVariable,rowVariable) != NULL)
           {
            RebindTableMemberVariableError(theEnv,bindVariable->contents,functionName);
            return true;
           }
        }

      if (CheckForBindingRowVar(theEnv,rowVariable,theExpression->argList,bindFunction,functionName))
        { return true; }
        
      theExpression = theExpression->nextArg;
     }
     
   return false;
  }
    
/**********************/
/* InvalidColumnError */
/**********************/
static void InvalidColumnError(
  Environment *theEnv,
  const char *table,
  const char *rowVariable,
  const char *column)
  {
   PrintErrorID(theEnv,"TABLEBSC",14,false);
   WriteString(theEnv,STDERR,"The table/column reference ?");
   WriteString(theEnv,STDERR,rowVariable);
   WriteString(theEnv,STDERR,":");
   WriteString(theEnv,STDERR,column);
   WriteString(theEnv,STDERR," is invalid because the table ");
   WriteString(theEnv,STDERR,table);
   WriteString(theEnv,STDERR," does not contain the specified column.\n");
   SetEvaluationError(theEnv,true);
  }

/********************************/
/* InvalidVarColumnErrorMessage */
/********************************/
/*
void InvalidVarColumnErrorMessage(
  Environment *theEnv,
  const char *varColumn)
  {
   PrintErrorID(theEnv,"TABLEBSC",14,false);
   
   WriteString(theEnv,STDERR,"The variable/column reference ?");
   WriteString(theEnv,STDERR,varColumn);
   WriteString(theEnv,STDERR," is invalid because column names must be symbols.\n");
  }
*/
/***************/
/* GetQueryRow */
/***************/
void GetQueryRow(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   TABLE_QUERY_CORE *core;

   core = FindQueryCore(theEnv,GetFirstArgument()->integerValue->contents);

   GetTableValue(theEnv,core->table,core->row,0,returnValue);
  }

/*********************/
/* GetQueryRowColumn */
/*********************/
void GetQueryRowColumn(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   TABLE_QUERY_CORE *core;
   Deftable *theDeftable;
   struct rcHashTableEntry *colEntry;
   CLIPSLexeme *columnName;

   returnValue->lexemeValue = FalseSymbol(theEnv);

   core = FindQueryCore(theEnv,GetFirstArgument()->integerValue->contents);
   theDeftable = core->table;
   columnName = GetFirstArgument()->nextArg->lexemeValue;

   /*=============================*/
   /* Get the column information. */
   /*=============================*/

   colEntry = FindTableEntryForColumn(theEnv,theDeftable,GetFirstArgument()->nextArg->type,
                                                 GetFirstArgument()->nextArg->value);

   if (colEntry == NULL)
     {
      InvalidColumnError(theEnv,theDeftable->header.name->contents,core->rowVar->contents,columnName->contents);
      return;
     }
     
   /*====================================*/
   /* Retrieve the value from the table. */
   /*====================================*/
   
   GetTableValue(theEnv,theDeftable,core->row,colEntry->itemIndex,returnValue);
  }
  
/****************************************************/
/* FindQueryCore: Looks up a QueryCore Stack Frame. */
/****************************************************/
static TABLE_QUERY_CORE *FindQueryCore(
  Environment *theEnv,
  long long depth)
  {
   TABLE_QUERY_STACK *qptr;

   if (depth == 0)
     { return DeftableData(theEnv)->QueryCore; }
     
   qptr = DeftableData(theEnv)->QueryCoreStack;
   while (depth > 1)
     {
      qptr = qptr->nxt;
      depth--;
     }
     
   return qptr->core;
  }
  
/***********************************************************/
/* PushQueryCore: Pushes the current QueryCore onto stack. */
/***********************************************************/
static void PushQueryCore(
  Environment *theEnv)
  {
   TABLE_QUERY_STACK *qptr;

   qptr = get_struct(theEnv,table_query_stack);
   qptr->core = DeftableData(theEnv)->QueryCore;
   qptr->nxt = DeftableData(theEnv)->QueryCoreStack;
   DeftableData(theEnv)->QueryCoreStack = qptr;
  }

/*********************************************/
/* PopQueryCore: Pops top of QueryCore stack */
/*   and restores the prior QueryCore.       */
/*********************************************/
static void PopQueryCore(
  Environment *theEnv)
  {
   TABLE_QUERY_STACK *qptr;

   DeftableData(theEnv)->QueryCore = DeftableData(theEnv)->QueryCoreStack->core;
   qptr = DeftableData(theEnv)->QueryCoreStack;
   DeftableData(theEnv)->QueryCoreStack = DeftableData(theEnv)->QueryCoreStack->nxt;
   rtn_struct(theEnv,table_query_stack,qptr);
  }
  
/********************/
/* NoRowForKeyError */
/********************/
static void NoRowForKeyError(
  Environment *theEnv,
  unsigned short theType,
  void *theValue,
  const char *functionName)
  {
   Multifield *theMultifield;
   
   PrintErrorID(theEnv,"DFTBLBSC",1,false);
   WriteString(theEnv,STDERR,"Unable to find row for key ");
   if (theType == MULTIFIELD_TYPE)
     {
      theMultifield = (Multifield *) theValue;
      PrintMultifieldDriver(theEnv,STDERR,theMultifield,
                            0,theMultifield->length,true);
     }
   else
     { PrintAtom(theEnv,STDERR,theType,theValue); }
   WriteString(theEnv,STDERR," in function ");
   WriteString(theEnv,STDERR,functionName);
   WriteString(theEnv,STDERR,".\n");
   SetEvaluationError(theEnv,true);
  }

/**********************************/
/* RebindTableMemberVariableError */
/**********************************/
static void RebindTableMemberVariableError(
  Environment *theEnv,
  const char *theVariable,
  const char *functionName)
  {
   PrintErrorID(theEnv,"TABLEBSC",12,false);
   WriteString(theEnv,STDERR,"Cannot rebind table member variable ?");
   WriteString(theEnv,STDERR,theVariable);
   WriteString(theEnv,STDERR," in function ");
   WriteString(theEnv,STDERR,functionName);
   WriteString(theEnv,STDERR,".\n");
  }

#endif /* DEFTABLE_CONSTRUCT */
