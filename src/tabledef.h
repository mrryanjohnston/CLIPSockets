   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 7.00  02/17/24            */
   /*                                                     */
   /*                DEFTABLE HEADER FILE                 */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
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

#ifndef _H_tabledef

#pragma once

#define _H_tabledef

typedef struct deftable Deftable;

#include "constrct.h"
#include "cstrccom.h"

#define DEFTABLE_DATA 65

typedef struct table_query_core
  {
   Expression *query;
   Expression *action;
   Deftable *table;
   CLIPSLexeme *rowVar;
   unsigned int row;
   UDFValue *result;
  } TABLE_QUERY_CORE;

typedef struct table_query_stack
  {
   TABLE_QUERY_CORE *core;
   struct table_query_stack *nxt;
  } TABLE_QUERY_STACK;

struct deftableData
  {
   Construct *DeftableConstruct;
   unsigned DeftableModuleIndex;
   struct entityRecord DeftablePtrRecord;
#if CONSTRUCT_COMPILER && (! RUN_TIME)
   struct CodeGeneratorItem *DeftableCodeItem;
#endif
   TABLE_QUERY_CORE *QueryCore;
   TABLE_QUERY_STACK *QueryCoreStack;
   bool AbortQuery;
  };

struct rcHashTableEntry
  {
   Expression *item;
   unsigned short itemIndex;
   size_t hashValue;
   struct rcHashTableEntry *next;
  };

struct deftable
  {
   ConstructHeader header;
   struct expr *columns;
   struct expr *rows;
   size_t columnCount;
   size_t rowCount;
   size_t columnHashTableSize;
   size_t rowHashTableSize;
   struct rcHashTableEntry **columnHashTable;
   struct rcHashTableEntry **rowHashTable;
   long busyCount;
  };

struct deftableModule
  {
   struct defmoduleItemHeaderHM header;
  };

#define DeftableData(theEnv) ((struct deftableData *) GetEnvironmentData(theEnv,DEFTABLE_DATA))

   void                           InitializeDeftable(Environment *);
   Deftable                      *FindDeftable(Environment *,const char *);
   Deftable                      *FindDeftableInModule(Environment *,const char *);
   Deftable                      *GetNextDeftable(Environment *,Deftable *);
   bool                           DeftableIsDeletable(Deftable *);
   struct deftableModule         *GetDeftableModuleItem(Environment *,Defmodule *);
   const char                    *DeftableModule(Deftable *);
   const char                    *DeftableName(Deftable *);
   const char                    *DeftablePPForm(Deftable *);
   ConstructHeader               *FindNamedConstructInModuleHT(Environment *,const char *,Construct *);
   struct rcHashTableEntry      **AllocateRCHT(Environment *,size_t);
   void                           AddEntryToRCHT(Environment *,struct rcHashTableEntry **,size_t,struct expr *,unsigned short);
   void                           FreeRCHT(Environment *,struct rcHashTableEntry **,size_t);
   void                           DecrementDeftableBusyCount(Environment *,Deftable *);
   void                           IncrementDeftableBusyCount(Environment *,Deftable *);
   size_t                         TableKeyHashFromExpression(Environment *,Expression *);
   size_t                         TableKeyHashTypeValue(Environment *,unsigned short,void *);
   bool                           TableKeysMatchEE(Expression *,Expression *);
   bool                           TableKeysMatchTypeValue(Expression *,unsigned short,void *);
   void                           PrintKey(Environment *,const char *,Expression *);
   struct rcHashTableEntry       *FindTableEntryForRow(Environment *,Deftable *,unsigned short,void *);
   struct rcHashTableEntry       *FindTableEntryForColumn(Environment *,Deftable *,unsigned short,void *);
   struct rcHashTableEntry       *FindTableEntryTypeValue(Environment *,struct rcHashTableEntry **,size_t,
                                                          unsigned short,void *);
   void                           DebugHashTable(struct rcHashTableEntry **,size_t);
  
#if RUN_TIME
   void                           DeftableRunTimeInitialize(Environment *);
#endif

#endif /* _H_tabledef */
