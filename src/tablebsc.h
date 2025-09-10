   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 7.00  03/08/25            */
   /*                                                     */
   /*         DEFTABLE BASIC COMMANDS HEADER FILE         */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements core commands for the deftable        */
/*   construct such as clear, reset, save, undeftable,       */
/*   ppdeftable, list-deftables, and get-deftable-list.      */
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

#ifndef _H_tablebsc

#pragma once

#define _H_tablebsc

#include "tabledef.h"

   void                           DeftableBasicCommands(Environment *);
   void                           UndeftableCommand(Environment *,UDFContext *,UDFValue *);
   bool                           Undeftable(Deftable *,Environment *);
   void                           GetDeftableListFunction(Environment *,UDFContext *,UDFValue *);
   void                           GetDeftableList(Environment *,CLIPSValue *,Defmodule *);
   void                           DeftableModuleFunction(Environment *,UDFContext *,UDFValue *);
   void                           PPDeftableCommand(Environment *,UDFContext *,UDFValue *);
   bool                           PPDeftable(Environment *,const char *,const char *);
   void                           ListDeftablesCommand(Environment *,UDFContext *,UDFValue *);
   void                           ListDeftables(Environment *,const char *,Defmodule *);
   void                           LookupFunction(Environment *,UDFContext *,UDFValue *);
   void                           ContainsKeyFunction(Environment *,UDFContext *,UDFValue *);
   Expression                    *RowParseQueryAction(Environment *,Expression *,const char *);
   void                           GetQueryRow(Environment *,UDFContext *,UDFValue *);
   void                           GetQueryTableRowColumnValue(Environment *,UDFContext *,UDFValue *);
   void                           GetTableValue(Environment *,Deftable *,unsigned int,unsigned int,UDFValue *);
   void                           GetQueryRowColumn(Environment *,UDFContext *,UDFValue *);
   void                           QueryAnyRowp(Environment *,UDFContext *,UDFValue *);
   void                           QueryFindRow(Environment *,UDFContext *,UDFValue *);
   void                           QueryDoForRow(Environment *,UDFContext *,UDFValue *);
   void                           QueryDoForThisRow(Environment *,UDFContext *,UDFValue *);
   void                           QueryDoForAllRows(Environment *,UDFContext *,UDFValue *);
   void                           TableColumnsFunction(Environment *,UDFContext *,UDFValue *);
   void                           TableRowCountFunction(Environment *,UDFContext *,UDFValue *);
   
#endif /* _H_tablebsc */

