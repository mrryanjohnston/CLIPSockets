   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 7.00  02/21/25             */
   /*                                                     */
   /*                DEFTABLE PARSER MODULE               */
   /*******************************************************/

/*************************************************************/
/* Purpose: Parses a deffacts construct.                     */
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

#if BLOAD || BLOAD_AND_BSAVE
#include "bload.h"
#endif

#include "cstrcpsr.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "memalloc.h"
#include "modulutl.h"
#include "pprint.h"
#include "prntutil.h"
#include "router.h"
#include "tablebsc.h"
#include "tabledef.h"

#include "tablepsr.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if (! RUN_TIME) && (! BLOAD_ONLY)
   static struct expr            *ParseColumns(Environment *,const char *,struct token *,bool *,
                                               unsigned short *,bool *);
   static bool                    ParseRow(Environment *,const char *,struct token *,bool *,
                                           unsigned short,int,Expression **,Expression **);
   static struct expr            *TableTokenToExpression(Environment *,struct token *);
   static void                    DeclarationParse(Environment *,const char *,bool *,bool *);
   static void                    ParseAutoKeys(Environment *,const char *,bool *,bool *);
   static void                    AddColumnToRow(Expression **,Expression **,Expression *);
   static struct expr            *TableValueToExpression(Environment *,unsigned short,void *);
   static struct expr            *CLIPSValueToExpression(Environment *,UDFValue *,bool *,bool);
   static struct expr            *MultifieldValueToExpression(Environment *,UDFValue *,bool *);
   static struct expr            *GlobalVariableToExpression(Environment *,unsigned short,void *,
                                                             bool *,bool);
   static struct expr            *FunctionCallToExpression(Environment *,const char *,struct token *,
                                                           bool *,bool);
#endif

/************************************************************/
/* ParseDeftable: Coordinates all actions necessary for the */
/*   addition of a deftable construct into the current      */
/*   environment. Called when parsing a construct after the */
/*   deftable keyword has been found.                       */
/************************************************************/
bool ParseDeftable(
  Environment *theEnv,
  const char *readSource)
  {
#if (! RUN_TIME) && (! BLOAD_ONLY)
   CLIPSLexeme *deftableName;
   Deftable *newDeftable;
   bool deftableError = false;
   struct token inputToken;
   unsigned short colCount = 0, rowCount = 0, i = 0;
   struct rcHashTableEntry **colHT = NULL, **rowHT = NULL;
   unsigned long colHTSize = 0, rowHTSize = 0;
   struct expr *c, *r;
   struct rcHashTableEntry *rcEntry;
   size_t hv;
   bool autoKeys = false;
   int autoKeyIndex = 0;
   Expression *columns, *headOfRows = NULL, *tailOfRows = NULL;
   Expression *packedRows, *packedColumns;

   /*=========================*/
   /* Parsing initialization. */
   /*=========================*/

   deftableError = false;
   SetPPBufferStatus(theEnv,true);

   FlushPPBuffer(theEnv);
   SetIndentDepth(theEnv,3);
   SavePPBuffer(theEnv,"(deftable ");

   /*==========================================================*/
   /* Deftable can not be added when a binary image is loaded. */
   /*==========================================================*/

#if BLOAD || BLOAD_AND_BSAVE
   if ((Bloaded(theEnv) == true) && (! ConstructData(theEnv)->CheckSyntaxMode))
     {
      CannotLoadWithBloadMessage(theEnv,"deftable");
      return true;
     }
#endif

   /*============================*/
   /* Parse the deftable header. */
   /*============================*/

   deftableName = GetConstructNameAndComment(theEnv,readSource,&inputToken,"deftable",
                                             (FindConstructFunction *) FindDeftableInModule,
                                             (DeleteConstructFunction *) Undeftable,">",true,
                                             true,true,false);
   if (deftableName == NULL) { return true; }

   /*============================*/
   /* Parse the list of columns. */
   /*============================*/

   if (inputToken.tknType != LEFT_PARENTHESIS_TOKEN)
     {
      SyntaxErrorMessage(theEnv,"deftable");
      return true;
     }

   columns = ParseColumns(theEnv,readSource,&inputToken,&deftableError,&colCount,&autoKeys);
   if (deftableError)
     { return true; }
  
   /*=================*/
   /* Parse the rows. */
   /*=================*/

   if (autoKeys)
     { autoKeyIndex = 1; }
   else
     { autoKeyIndex = 0; }
     
   while (ParseRow(theEnv,readSource,&inputToken,&deftableError,colCount,autoKeyIndex,&headOfRows,&tailOfRows))
     {
      rowCount++;
      if (autoKeyIndex) autoKeyIndex++;
     }

   if (deftableError)
     {
      ReturnExpression(theEnv,columns);
      ReturnExpression(theEnv,headOfRows);
      return true;
     }
   
   SavePPBuffer(theEnv,"\n");

   /*===========================================*/
   /* Pack the rows and columns so that values  */
   /* can be retrieved using indices.           */
   /*===========================================*/
   
   packedColumns = PackExpression(theEnv,columns);
   ReturnExpression(theEnv,columns);
   packedRows = PackExpression(theEnv,headOfRows);
   ReturnExpression(theEnv,headOfRows);

   /*=======================================================*/
   /* Create the column hashTable and check for duplicates. */
   /*=======================================================*/

   colHTSize = colCount;
   if (colHTSize > 3)
     {
      colHTSize = colHTSize * 5 / 4;
      while (! IsPrime(colHTSize))
        { colHTSize++; }
     }
     
   colHT = AllocateRCHT(theEnv,colHTSize);
      
   c = packedColumns;
   for (i = 0; i < colCount; i++)
     {
      hv = ItemHashValue(theEnv,c->type,c->value,0);
     
      rcEntry = colHT[hv % colHTSize];
      
      while (rcEntry != NULL)
        {
         if (TableKeysMatchEE(rcEntry->item,c))
           {
            PrintErrorID(theEnv,"DFTBLPSR",4,true);
            WriteString(theEnv,STDERR,"The column value ");
            PrintKey(theEnv,STDERR,c);
            WriteString(theEnv,STDERR," has been used more than once.\n");
            ReturnPackedExpression(theEnv,packedColumns);
            ReturnPackedExpression(theEnv,packedRows);
            FreeRCHT(theEnv,colHT,colHTSize);  
            return true;
           }
           
         rcEntry = rcEntry->next;
        }

      AddEntryToRCHT(theEnv,colHT,colHTSize,c,i);
        
      c = c->nextArg;
     }
     
   /*====================================================*/
   /* Create the row hashTable and check for duplicates. */
   /*====================================================*/

   rowHTSize = rowCount;
   if (colHTSize > 3)
     {
      rowHTSize = rowHTSize * 5 / 4;
      while (! IsPrime(rowHTSize))
        { rowHTSize++; }
     }
     
   rowHT = AllocateRCHT(theEnv,rowHTSize);
   
   r = packedRows;
   
   for (i = 0; i < rowCount; i++)
     {
      rcEntry = FindTableEntryTypeValue(theEnv,rowHT,rowHTSize,r->type,r->value);
      if (rcEntry != NULL)
        {
         PrintErrorID(theEnv,"DFTBLPSR",4,true);
         WriteString(theEnv,STDERR,"The row value ");
         PrintKey(theEnv,STDERR,rcEntry->item);
         WriteString(theEnv,STDERR," has been used more than once.\n");
         ReturnPackedExpression(theEnv,packedColumns);
         ReturnPackedExpression(theEnv,packedRows);
         FreeRCHT(theEnv,colHT,colHTSize);
         FreeRCHT(theEnv,rowHT,rowHTSize);
         return true;
        }

      AddEntryToRCHT(theEnv,rowHT,rowHTSize,r,i);
        
      r += colCount;
     }

   /*==========================================*/
   /* If we're only checking syntax, don't add */
   /* thesuccessfully parsed table to the KB.  */
   /*==========================================*/

   if (ConstructData(theEnv)->CheckSyntaxMode)
     {
      ReturnPackedExpression(theEnv,packedColumns);
      ReturnPackedExpression(theEnv,packedRows);
      FreeRCHT(theEnv,colHT,colHTSize);
      FreeRCHT(theEnv,rowHT,rowHTSize);
      return false;
     }

   /*==========================*/
   /* Create the new deftable. */
   /*==========================*/

   newDeftable = get_struct(theEnv,deftable);
   IncrementLexemeCount(deftableName);
   InitializeConstructHeader(theEnv,"deftable",DEFTABLE,&newDeftable->header,deftableName);

   ExpressionInstall(theEnv,packedColumns);
   newDeftable->columns = packedColumns;
   newDeftable->columnCount = colCount;
   newDeftable->columnHashTable = colHT;
   newDeftable->columnHashTableSize = colHTSize;

   ExpressionInstall(theEnv,packedRows);
   newDeftable->rows = packedRows;
   newDeftable->rowCount = rowCount;
   newDeftable->rowHashTable = rowHT;
   newDeftable->rowHashTableSize = rowHTSize;
   
   /*=======================================================*/
   /* Save the pretty print representation of the deftable. */
   /*=======================================================*/

   if (GetConserveMemory(theEnv) == true)
     { newDeftable->header.ppForm = NULL; }
   else
     { newDeftable->header.ppForm = CopyPPBuffer(theEnv); }

   /*=============================================*/
   /* Add the deftable to the appropriate module. */
   /*=============================================*/

   AddConstructToModule(&newDeftable->header);
   AddConstructToHashMap(theEnv,&newDeftable->header,newDeftable->header.whichModule);

#endif /* (! RUN_TIME) && (! BLOAD_ONLY) */

   /*================================================================*/
   /* Return false to indicate the deftable was successfully parsed. */
   /*================================================================*/

   return false;
  }
  
#if (! RUN_TIME) && (! BLOAD_ONLY)

/*****************/
/* ParseColumns: */
/*****************/
static struct expr *ParseColumns(
  Environment *theEnv,
  const char *readSource,
  struct token *inputToken,
  bool *error,
  unsigned short *colCount,
  bool *autoKeys)
  {
   struct expr *columns = NULL, *lastAdd = NULL, *theColumn;
   
   /************************************************/
   /* The opening left parenthesis of the column   */
   /* definitions  has already been parsed. If the */
   /* first token is the keyword 'declare', then   */
   /* parse a declare statement instead.           */
   /************************************************/
   
   GetToken(theEnv,readSource,inputToken);
   
   if ((inputToken->tknType == SYMBOL_TOKEN) &&
       (strcmp(inputToken->lexemeValue->contents,"declare") == 0))
     {
      DeclarationParse(theEnv,readSource,error,autoKeys);
      if (*error) return NULL;
      
      if (*autoKeys)
        {
         columns = GenConstant(theEnv,SYMBOL_TYPE,CreateSymbol(theEnv,"key"));
         lastAdd = columns;
         (*colCount)++;
        }
         
      GetToken(theEnv,readSource,inputToken);

      if (inputToken->tknType != LEFT_PARENTHESIS_TOKEN)
        {
         SyntaxErrorMessage(theEnv,"deftable");
         *error = true;
         return NULL;
        }
        
      GetToken(theEnv,readSource,inputToken);
     }
 
   /*==========================*/
   /* Parse the column values. */
   /*==========================*/

   while (inputToken->tknType != RIGHT_PARENTHESIS_TOKEN)
     {
      if ((inputToken->tknType == GBL_VARIABLE_TOKEN) ||
          (inputToken->tknType == MF_GBL_VARIABLE_TOKEN))
        {
         theColumn = GlobalVariableToExpression(theEnv,
                                                 TokenTypeToType(inputToken->tknType),
                                                 inputToken->value,error,true);
         if (*error)
           {
            ReturnExpression(theEnv,columns);
            SyntaxErrorMessage(theEnv,"deftable");
            return NULL;
           }
            
         if (theColumn->type != SYMBOL_TYPE)
           {
            PrintErrorID(theEnv,"DFTBLPSR",5,true);
            WriteString(theEnv,STDERR,"Deftable column names must be symbols.\n");
            ReturnExpression(theEnv,theColumn);
            ReturnExpression(theEnv,columns);
            *error = true;
            return NULL;
           }
        }
      else if ((inputToken->tknType == SYMBOL_TOKEN) &&
               (strcmp(inputToken->lexemeValue->contents,"=") == 0))
        {
         theColumn = FunctionCallToExpression(theEnv,readSource,inputToken,error,true);
         if (*error)
           { 
            ReturnExpression(theEnv,columns);
            return NULL;
           }

         if (theColumn->type != SYMBOL_TYPE)
           {
            PrintErrorID(theEnv,"DFTBLPSR",5,true);
            WriteString(theEnv,STDERR,"Deftable column names must be symbols.\n");
            ReturnExpression(theEnv,theColumn);
            ReturnExpression(theEnv,columns);
            *error = true;
            return NULL;
           }
        }
      else if (inputToken->tknType == SYMBOL_TOKEN)
        { theColumn = TableTokenToExpression(theEnv,inputToken); }
      else
        {
         PrintErrorID(theEnv,"DFTBLPSR",5,true);
         WriteString(theEnv,STDERR,"Deftable column names must be symbols.\n");
         ReturnExpression(theEnv,columns);
         *error = true;
         return NULL;
        }
      
      (*colCount)++;
      if (columns == NULL)
        { columns = theColumn; }
      else
        { lastAdd->nextArg = theColumn; }
        
      lastAdd = theColumn;
      
      SavePPBuffer(theEnv," ");
      GetToken(theEnv,readSource,inputToken);
     }
     
   if (columns == NULL)
     {
      PrintErrorID(theEnv,"DFTBLPSR",1,true);
      WriteString(theEnv,STDERR,"A deftable must have at least one column for the key value.\n");
      *error = true;
      return NULL;
     }

   PPBackup(theEnv);
   PPBackup(theEnv);
   SavePPBuffer(theEnv,")");
   
   return columns;
  }

/*************************************/
/* AddColumnToRows: Adds a column to */
/*   the linked collection of rows.  */
/*************************************/
static void AddColumnToRow(
  Expression **head,
  Expression **tail,
  Expression *theColumn)
  {
   if (*head == NULL)
     { *head = theColumn; }
   else
     { (*tail)->argList = theColumn; }
     
   *tail = theColumn;
  }

/***********************************************/
/* ParseRow: Parses a single row in the table. */
/***********************************************/
static bool ParseRow(
  Environment *theEnv,
  const char *readSource,
  struct token *inputToken,
  bool *error,
  unsigned short colCount,
  int autoKeyIndex,
  Expression **headOfRows,
  Expression **tailOfRows)
  {
   unsigned short rowColCount = 0;
   Expression *columnHead, *columnTail, *next;
   
   /*====================================================*/
   /* Check for the closing parenthesis of the deftable. */
   /*====================================================*/
   
   GetToken(theEnv,readSource,inputToken);
   if (inputToken->tknType == RIGHT_PARENTHESIS_TOKEN)
     { return false; }

   if (inputToken->tknType != LEFT_PARENTHESIS_TOKEN)
     {
      SyntaxErrorMessage(theEnv,"deftable");
      *error = true;
      return false;
     }

   /*===================================*/
   /* Place the row on the next line in */
   /* the deftable pretty print text.   */
   /*===================================*/
   
   PPBackup(theEnv);
   PPCRAndIndent(theEnv);
   SavePPBuffer(theEnv,"(");
 
   /*==============================================*/
   /* If auto-keys are being generated, create the */
   /* column for the key and add it to the row.    */
   /*==============================================*/
   
   if (autoKeyIndex != 0)
     {
      rowColCount++;
      columnHead = GenConstant(theEnv,INTEGER_TYPE,CreateInteger(theEnv,autoKeyIndex));
      AddColumnToRow(headOfRows,tailOfRows,columnHead);
     }
  
   /*==================================*/
   /* Continue until the closing right */
   /* parenthesis of the row is found. */
   /*==================================*/
   
   GetToken(theEnv,readSource,inputToken);
   while (inputToken->tknType != RIGHT_PARENTHESIS_TOKEN)
     {
      /*==========================================*/
      /* If the next token is a left parenthesis, */
      /* then a multifield value is being parsed. */
      /*==========================================*/
        
      if (inputToken->tknType == LEFT_PARENTHESIS_TOKEN)
        {
         long long valueCount = 0;
         
         columnHead = get_struct(theEnv,expr);
         columnHead->type = QUANTITY_TYPE;
         columnHead->nextArg = NULL;
         columnHead->argList = NULL;

         columnTail = columnHead;
         
         GetToken(theEnv,readSource,inputToken);

         while (inputToken->tknType != RIGHT_PARENTHESIS_TOKEN)
           {
            next = TableTokenToExpression(theEnv,inputToken);
            valueCount++;
            if (next == NULL)
              {
               SyntaxErrorMessage(theEnv,"deftable");
               ReturnExpression(theEnv,columnHead);
               *error = true;
               return false;
              }
              
            columnTail->nextArg = next;
            columnTail = next;
            
            SavePPBuffer(theEnv," ");
            GetToken(theEnv,readSource,inputToken);
            
            if (inputToken->tknType == RIGHT_PARENTHESIS_TOKEN)
              {
               PPBackup(theEnv);
               PPBackup(theEnv);
               SavePPBuffer(theEnv,")");
              }
           }
           
         columnHead->value = CreateQuantity(theEnv,valueCount);
        }
        
      /*=====================*/
      /* Process a = symbol. */
      /*=====================*/
      
      else if ((inputToken->tknType == SYMBOL_TOKEN) &&
               (strcmp(inputToken->lexemeValue->contents,"=") == 0))
        {
         columnHead = FunctionCallToExpression(theEnv,readSource,inputToken,error,true);
         if (*error)
           { return false; }
        }

      /*============================*/
      /* Process a global variable. */
      /*============================*/
      
      else if ((inputToken->tknType == GBL_VARIABLE_TOKEN) ||
               (inputToken->tknType == MF_GBL_VARIABLE_TOKEN))
        {
         columnHead = GlobalVariableToExpression(theEnv,
                                                 TokenTypeToType(inputToken->tknType),
                                                 inputToken->value,error,true);
                                                 
          if (*error)
            {
             SyntaxErrorMessage(theEnv,"deftable");
             return false;
            }
        }
        
      /*==================================================*/
      /* Otherwise, a single-field value is being parsed. */
      /*==================================================*/
        
      else
       {
        columnHead = TableTokenToExpression(theEnv,inputToken); // TBD use error
        if (columnHead == NULL)
          {
           SyntaxErrorMessage(theEnv,"deftable");
           *error = true;
           return false;
          }
       }
       
     rowColCount++;
     AddColumnToRow(headOfRows,tailOfRows,columnHead);

     /*=====================================================*/
     /* It's an error for the row to have more columns than */
     /* are specified in the table's list of columns.       */
     /*=====================================================*/

     if (rowColCount > colCount)
       {
         PrintErrorID(theEnv,"DFTBLPSR",3,true);
         WriteString(theEnv,STDERR,"Each row for this deftable should contain exactly ");
         if (colCount == 1)
           { WriteString(theEnv,STDERR,"1 value.\n"); }
         else
           {
            WriteInteger(theEnv,STDERR,colCount);
            WriteString(theEnv,STDERR," values.\n");
           }
        
         *error = true;
         return false;
        }

     /*=================================================*/
     /* Get the next value and adjust the pretty print  */
     /* text if the closing right parenthesis is found. */
     /*=================================================*/
     
     SavePPBuffer(theEnv," ");
     GetToken(theEnv,readSource,inputToken);
     if (inputToken->tknType == RIGHT_PARENTHESIS_TOKEN)
       {
        PPBackup(theEnv);
        PPBackup(theEnv);
        SavePPBuffer(theEnv,")");
       }
    }

   /*=====================================================*/
   /* It's an error for the row to have more columns than */
   /* are specified in the table's list of columns.       */
   /*=====================================================*/

   if (rowColCount < colCount)
     {
      PrintErrorID(theEnv,"DFTBLPSR",3,true);
      WriteString(theEnv,STDERR,"Each row for this deftable should contain exactly ");
      if (colCount == 1)
        { WriteString(theEnv,STDERR,"1 value.\n"); }
      else
        {
         WriteInteger(theEnv,STDERR,colCount);
         WriteString(theEnv,STDERR," values.\n");
        }
        
      *error = true;
      return false;
     }

   return true;
  }

/**************************/
/* TableTokenToExpression */
/**************************/
static struct expr *TableTokenToExpression(
   Environment *theEnv,
   struct token *inputToken)
   {
    switch (inputToken->tknType)
      {
       case SYMBOL_TOKEN:
       case STRING_TOKEN:
       case INSTANCE_NAME_TOKEN:
         return GenConstant(theEnv,inputToken->lexemeValue->header.type,
                                   inputToken->lexemeValue);
           
       case FLOAT_TOKEN:
         return GenConstant(theEnv,inputToken->floatValue->header.type,
                                   inputToken->floatValue);
           
       case INTEGER_TOKEN:
         return GenConstant(theEnv,inputToken->integerValue->header.type,
                                   inputToken->integerValue);
           
       default:
         return NULL;
        }
        
    return NULL;
   }

/**************************/
/* CLIPSValueToExpression */
/**************************/
static struct expr *CLIPSValueToExpression(
  Environment *theEnv,
  UDFValue *assignValue,
  bool *error,
  bool createQuantity)
  {
   struct expr *columnHead, *next;
   
   if (assignValue->header->type != MULTIFIELD_TYPE)
     {
      columnHead = TableValueToExpression(theEnv,assignValue->header->type,assignValue->value); // TBD error
      if (columnHead == NULL)
        { *error = true; }
        
      return columnHead;
     }
          
   next = MultifieldValueToExpression(theEnv,assignValue,error);
   if (*error)
     { return NULL; }
   
   if (createQuantity)
     {
      columnHead = get_struct(theEnv,expr);
      columnHead->type = QUANTITY_TYPE;
      columnHead->value = CreateQuantity(theEnv,(long long) assignValue->multifieldValue->length); ;
      columnHead->nextArg = next;
      columnHead->argList = NULL;
      return columnHead;
     }

   return next;
  }

/*******************************/
/* MultifieldValueToExpression */
/*******************************/
static struct expr *MultifieldValueToExpression(
  Environment *theEnv,
  UDFValue *assignValue,
  bool *error)
  {
   struct expr *columnHead, *columnTail, *next;
   size_t i;
   
   columnHead = NULL;
   columnTail = NULL;
   
   for (i = 0; i < assignValue->range; i++)
     {
      next = TableValueToExpression(theEnv,assignValue->multifieldValue->contents[i].header->type,
                                           assignValue->multifieldValue->contents[i].value);

      if (next == NULL)
        {
         ReturnExpression(theEnv,columnHead);
         *error = true;
         return NULL;
        }
                       
      if (columnHead == NULL)
        {
         columnHead = next;
         columnTail = columnHead;
        }
      else
        {
         columnTail->nextArg = next;
         columnTail = next;
        }
     }
   
   return columnHead;
  }

/******************************/
/* GlobalVariableToExpression */
/******************************/
static struct expr *GlobalVariableToExpression(
  Environment *theEnv,
  unsigned short type,
  void *value,
  bool *error,
  bool createQuantity)
  {
   Expression *theExpression, *columnHead = NULL;
   UDFValue assignValue;
   
   /*===============================*/
   /* Evaluate the global variable. */
   /*===============================*/
   
   theExpression = GenConstant(theEnv,type,value);
         
   SetEvaluationError(theEnv,false);
   if (EvaluateExpression(theEnv,theExpression,&assignValue))
     {
      ReturnExpression(theEnv,theExpression);
      *error = true;
      return NULL;
     }
        
   ReturnExpression(theEnv,theExpression);
         
   columnHead = CLIPSValueToExpression(theEnv,&assignValue,error,createQuantity);
         
   if (*error)
     {
      SyntaxErrorMessage(theEnv,"deftable");
      return NULL;
     }
     
   return columnHead;
  }

/****************************/
/* FunctionCallToExpression */
/****************************/
static struct expr *FunctionCallToExpression(
  Environment *theEnv,
  const char *readSource,
  struct token *inputToken,
  bool *error,
  bool createQuantity)
  {
   struct expr *theExpression, *columnHead;
   UDFValue assignValue;

   GetToken(theEnv,readSource,inputToken);
         
   if (inputToken->tknType != LEFT_PARENTHESIS_TOKEN)
     {
      PrintErrorID(theEnv,"DFTBLPSR",2,true);
      WriteString(theEnv,STDERR,"Expected a function call.\n");
      PPBackup(theEnv);
      SavePPBuffer(theEnv," ");
      SavePPBuffer(theEnv,inputToken->printForm);
      *error = true;
      return NULL;
     }
         
   theExpression = Function1Parse(theEnv,readSource);
         
   if (theExpression == NULL)
     {
      *error = true;
      return NULL;
     }
           
   SetEvaluationError(theEnv,false);
   if (EvaluateExpression(theEnv,theExpression,&assignValue))
     {
      ReturnExpression(theEnv,theExpression);
      *error = true;
      return NULL;
     }
           
   ReturnExpression(theEnv,theExpression);
   columnHead = CLIPSValueToExpression(theEnv,&assignValue,error,createQuantity);
         
   if (*error)
     {
      SyntaxErrorMessage(theEnv,"deftable");
      *error = true;
      return NULL;
     }
     
   return columnHead;
  }

/**************************/
/* TableValueToExpression */
/**************************/
static struct expr *TableValueToExpression(
  Environment *theEnv,
  unsigned short type,
  void *value)
  {
   switch (type)
     {
      case SYMBOL_TYPE:
      case STRING_TYPE:
      case INSTANCE_NAME_TYPE:
        return GenConstant(theEnv,type,value);
           
      case FLOAT_TYPE:
        return GenConstant(theEnv,type,value);
           
      case INTEGER_TYPE:
        return GenConstant(theEnv,type,value);
           
      default:
        return NULL;
     }
        
   return NULL;
  }
   
/****************************************************/
/* DeclarationParse: Parses a deftable declaration. */
/****************************************************/
static void DeclarationParse(
  Environment *theEnv,
  const char *readSource,
  bool *error,
  bool *autoKeys)
  {
   struct token theToken;
   bool notDone = true;
   bool autoKeysParsed = false;

   /*===========================*/
   /* Next token must be a '('. */
   /*===========================*/

   SavePPBuffer(theEnv," ");

   GetToken(theEnv,readSource,&theToken);
   if (theToken.tknType != LEFT_PARENTHESIS_TOKEN)
     {
      SyntaxErrorMessage(theEnv,"deftable declare statement");
      *error = true;
      return;
     }

   /*==========================================*/
   /* Continue parsing until there are no more */
   /* valid rule property declarations.        */
   /*==========================================*/

   while (notDone)
     {
      /*=============================================*/
      /* The name of a rule property must be symbol. */
      /*=============================================*/

      GetToken(theEnv,readSource,&theToken);
      if (theToken.tknType != SYMBOL_TOKEN)
        {
         SyntaxErrorMessage(theEnv,"deftable declare statement");
         *error = true;
        }

      /*==============================================*/
      /* Parse a auto-key declaration if encountered. */
      /*==============================================*/

      else if (strcmp(theToken.lexemeValue->contents,"auto-keys") == 0)
        {
         if (autoKeysParsed)
           {
            AlreadyParsedErrorMessage(theEnv,"auto-keys declaration",NULL);
            *error = true;
           }
         else
           {
            ParseAutoKeys(theEnv,readSource,error,autoKeys);
            autoKeysParsed = true;
           }
        }

      /*==========================================*/
      /* Otherwise the symbol does not correspond */
      /* to a valid table property.               */
      /*==========================================*/

      else
        {
         SyntaxErrorMessage(theEnv,"deftable declare statement");
         *error = true;
        }

      /*=====================================*/
      /* Return if an error was encountered. */
      /*=====================================*/

      if (*error)
        { return; }

      /*==============================*/
      /* The auto-keys table property */
      /* is closed with a ')'.        */
      /*==============================*/

      GetToken(theEnv,readSource,&theToken);
      if (theToken.tknType != RIGHT_PARENTHESIS_TOKEN)
        {
         PPBackup(theEnv);
         SavePPBuffer(theEnv," ");
         SavePPBuffer(theEnv,theToken.printForm);
         SyntaxErrorMessage(theEnv,"deftable declare statement");
         *error = true;
         return;
        }

      /*=============================================*/
      /* The declare statement is closed with a ')'. */
      /*=============================================*/

      GetToken(theEnv,readSource,&theToken);
      if (theToken.tknType == RIGHT_PARENTHESIS_TOKEN) notDone = false;
      else if (theToken.tknType != LEFT_PARENTHESIS_TOKEN)
        {
         SyntaxErrorMessage(theEnv,"deftable declare statement");
         *error = true;
         return;
        }
      else
        {
         PPBackup(theEnv);
         SavePPBuffer(theEnv," (");
        }
     }
  }

/*************************************************************/
/* ParseAutoKeys: Parses the rest of a deftable auto-keys    */
/*   declaration once the auto-keys keyword has been parsed. */
/*************************************************************/
static void ParseAutoKeys(
  Environment *theEnv,
  const char *readSource,
  bool *error,
  bool *autoKeys)
  {
   struct token theToken;

   /*========================================*/
   /* The auto-focus value must be a symbol. */
   /*========================================*/

   SavePPBuffer(theEnv," ");

   GetToken(theEnv,readSource,&theToken);
   if (theToken.tknType != SYMBOL_TOKEN)
     {
      SyntaxErrorMessage(theEnv,"auto-keys statement");
      *error = true;
      return;
     }

   /*===================================================*/
   /* The auto-keys value must be either TRUE or FALSE. */
   /*===================================================*/

   if (strcmp(theToken.lexemeValue->contents,"TRUE") == 0)
     { *autoKeys = true; }
   else if (strcmp(theToken.lexemeValue->contents,"FALSE") == 0)
     { *autoKeys = false; }
   else
     {
      SyntaxErrorMessage(theEnv,"auto-keys statement");
      *error = true;
     }
  }

#endif /* (! RUN_TIME) && (! BLOAD_ONLY) */

#endif /* DEFTABLE_CONSTRUCT */
