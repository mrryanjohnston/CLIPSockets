   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.50  09/14/23             */
   /*                                                     */
   /*          DEFTEMPLATE BASIC COMMANDS MODULE          */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements core commands for the deftemplate     */
/*   construct such as clear, reset, save, undeftemplate,    */
/*   ppdeftemplate, list-deftemplates, and                   */
/*   get-deftemplate-list.                                   */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Corrected compilation errors for files         */
/*            generated by constructs-to-c. DR0861           */
/*                                                           */
/*            Changed name of variable log to logName        */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Corrected code to remove compiler warnings     */
/*            when ENVIRONMENT_API_ONLY flag is set.         */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            ALLOW_ENVIRONMENT_GLOBALS no longer supported. */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*            Removed initial-fact support.                  */
/*                                                           */
/*            Pretty print functions accept optional logical */
/*            name argument.                                 */
/*                                                           */
/*      6.50: Changed the function name DeftemplateGetWatch  */
/*            to DeftemplateGetWatchFacts.                   */
/*                                                           */
/*            Added functions DeftemplateGetWatchGoals and   */
/*            DeftemplateSetWatchGoals.                      */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if DEFTEMPLATE_CONSTRUCT

#include <stdio.h>
#include <string.h>

#include "argacces.h"
#include "constrct.h"
#include "cstrccom.h"
#include "cstrcpsr.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#include "factrhs.h"
#include "memalloc.h"
#include "multifld.h"
#include "router.h"
#include "scanner.h"
#if BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE
#include "tmpltbin.h"
#endif
#if CONSTRUCT_COMPILER && (! RUN_TIME)
#include "tmpltcmp.h"
#endif
#include "tmpltdef.h"
#include "tmpltpsr.h"
#include "tmpltutl.h"

#include "tmpltbsc.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static void                    SaveDeftemplates(Environment *,Defmodule *,const char *,void *);

/*********************************************************************/
/* DeftemplateBasicCommands: Initializes basic deftemplate commands. */
/*********************************************************************/
void DeftemplateBasicCommands(
  Environment *theEnv)
  {
   AddSaveFunction(theEnv,"deftemplate",SaveDeftemplates,10,NULL);

#if ! RUN_TIME
   AddUDF(theEnv,"get-deftemplate-list","m",0,1,"y",GetDeftemplateListFunction,"GetDeftemplateListFunction",NULL);
   AddUDF(theEnv,"undeftemplate","v",1,1,"y",UndeftemplateCommand,"UndeftemplateCommand",NULL);
   AddUDF(theEnv,"deftemplate-module","y",1,1,"y",DeftemplateModuleFunction,"DeftemplateModuleFunction",NULL);

#if DEBUGGING_FUNCTIONS
   AddUDF(theEnv,"list-deftemplates","v",0,1,"y",ListDeftemplatesCommand,"ListDeftemplatesCommand",NULL);
   AddUDF(theEnv,"ppdeftemplate","vs",1,2,";y;ldsyn",PPDeftemplateCommand,"PPDeftemplateCommand",NULL);
#endif

#if (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE)
   DeftemplateBinarySetup(theEnv);
#endif

#if CONSTRUCT_COMPILER && (! RUN_TIME)
   DeftemplateCompilerSetup(theEnv);
#endif

#endif
  }

/**********************************************/
/* SaveDeftemplates: Deftemplate save routine */
/*   for use with the save command.           */
/**********************************************/
static void SaveDeftemplates(
  Environment *theEnv,
  Defmodule *theModule,
  const char *logicalName,
  void *context)
  {
   SaveConstruct(theEnv,theModule,logicalName,DeftemplateData(theEnv)->DeftemplateConstruct);
  }

/**********************************************/
/* UndeftemplateCommand: H/L access routine   */
/*   for the undeftemplate command.           */
/**********************************************/
void UndeftemplateCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UndefconstructCommand(context,"undeftemplate",DeftemplateData(theEnv)->DeftemplateConstruct);
  }

/************************************/
/* Undeftemplate: C access routine  */
/*   for the undeftemplate command. */
/************************************/
bool Undeftemplate(
  Deftemplate *theDeftemplate,
  Environment *allEnv)
  {
   Environment *theEnv;
   
   if (theDeftemplate == NULL)
     {
      theEnv = allEnv;
      return Undefconstruct(theEnv,NULL,DeftemplateData(theEnv)->DeftemplateConstruct);
     }
   else
     {
      theEnv = theDeftemplate->header.env;
      return Undefconstruct(theEnv,&theDeftemplate->header,DeftemplateData(theEnv)->DeftemplateConstruct);
     }
  }

/****************************************************/
/* GetDeftemplateListFunction: H/L access routine   */
/*   for the get-deftemplate-list function.         */
/****************************************************/
void GetDeftemplateListFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   GetConstructListFunction(context,returnValue,DeftemplateData(theEnv)->DeftemplateConstruct);
  }

/********************************************/
/* GetDeftemplateList: C access routine for */
/*   the get-deftemplate-list function.     */
/********************************************/
void GetDeftemplateList(
  Environment *theEnv,
  CLIPSValue *returnValue,
  Defmodule *theModule)
  {
   UDFValue result;
   
   GetConstructList(theEnv,&result,DeftemplateData(theEnv)->DeftemplateConstruct,theModule);
   NormalizeMultifield(theEnv,&result);
   returnValue->value = result.value;
  }

/***************************************************/
/* DeftemplateModuleFunction: H/L access routine   */
/*   for the deftemplate-module function.          */
/***************************************************/
void DeftemplateModuleFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   returnValue->value = GetConstructModuleCommand(context,"deftemplate-module",DeftemplateData(theEnv)->DeftemplateConstruct);
  }

#if DEBUGGING_FUNCTIONS

/**********************************************/
/* PPDeftemplateCommand: H/L access routine   */
/*   for the ppdeftemplate command.           */
/**********************************************/
void PPDeftemplateCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   PPConstructCommand(context,"ppdeftemplate",DeftemplateData(theEnv)->DeftemplateConstruct,returnValue);
  }

/***************************************/
/* PPDeftemplate: C access routine for */
/*   the ppdeftemplate command.        */
/***************************************/
bool PPDeftemplate(
  Environment *theEnv,
  const char *deftemplateName,
  const char *logicalName)
  {
   return(PPConstruct(theEnv,deftemplateName,logicalName,DeftemplateData(theEnv)->DeftemplateConstruct));
  }

/*************************************************/
/* ListDeftemplatesCommand: H/L access routine   */
/*   for the list-deftemplates command.          */
/*************************************************/
void ListDeftemplatesCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   ListConstructCommand(context,DeftemplateData(theEnv)->DeftemplateConstruct);
  }

/****************************************/
/* ListDeftemplates: C access routine   */
/*   for the list-deftemplates command. */
/****************************************/
void ListDeftemplates(
  Environment *theEnv,
  const char *logicalName,
  Defmodule *theModule)
  {
   ListConstruct(theEnv,DeftemplateData(theEnv)->DeftemplateConstruct,logicalName,theModule);
  }

/*************************************************************/
/* DeftemplateGetWatchFacts: C access routine for retrieving */
/*   the current watch facts value of a deftemplate.         */
/*************************************************************/
bool DeftemplateGetWatchFacts(
  Deftemplate *theTemplate)
  {
   return theTemplate->watchFacts;
  }

/***********************************************************/
/* DeftemplateSetWatchFacts:  C access routine for setting */
/*   the current watch facts value of a deftemplate.       */
/***********************************************************/
void DeftemplateSetWatchFacts(
  Deftemplate *theTemplate,
  bool newState)
  {
   theTemplate->watchFacts = newState;
  }

/*************************************************************/
/* DeftemplateGetWatchGoals: C access routine for retrieving */
/*   the current watch goals value of a deftemplate.         */
/*************************************************************/
bool DeftemplateGetWatchGoals(
  Deftemplate *theTemplate)
  {
   return theTemplate->watchGoals;
  }

/***********************************************************/
/* DeftemplateSetWatchGoals:  C access routine for setting */
/*   the current watch goals value of a deftemplate.       */
/***********************************************************/
void DeftemplateSetWatchGoals(
  Deftemplate *theTemplate,
  bool newState)
  {
   theTemplate->watchGoals = newState;
  }

/**********************************************************/
/* DeftemplateWatchAccess: Access routine for setting the */
/*   watch flag of a deftemplate via the watch command.   */
/**********************************************************/
bool DeftemplateWatchAccess(
  Environment *theEnv,
  int code,
  bool newState,
  Expression *argExprs)
  {
#if MAC_XCD
#pragma unused(code)
#endif

   if (code == 0)
     {
      return ConstructSetWatchAccess(theEnv,DeftemplateData(theEnv)->DeftemplateConstruct,newState,argExprs,
                                     (ConstructGetWatchFunction *) DeftemplateGetWatchFacts,
                                     (ConstructSetWatchFunction *) DeftemplateSetWatchFacts);
     }
   else
     {
      return ConstructSetWatchAccess(theEnv,DeftemplateData(theEnv)->DeftemplateConstruct,newState,argExprs,
                                     (ConstructGetWatchFunction *) DeftemplateGetWatchGoals,
                                     (ConstructSetWatchFunction *) DeftemplateSetWatchGoals);
     }
  }

/*************************************************************************/
/* DeftemplateWatchPrint: Access routine for printing which deftemplates */
/*   have their watch flag set via the list-watch-items command.         */
/*************************************************************************/
bool DeftemplateWatchPrint(
  Environment *theEnv,
  const char *logName,
  int code,
  Expression *argExprs)
  {
#if MAC_XCD
#pragma unused(code)
#endif

   if (code == 0)
     {
      return ConstructPrintWatchAccess(theEnv,DeftemplateData(theEnv)->DeftemplateConstruct,logName,argExprs,
                                       (ConstructGetWatchFunction *) DeftemplateGetWatchFacts,
                                       (ConstructSetWatchFunction *) DeftemplateSetWatchFacts);
     }
   else
     {
      return ConstructPrintWatchAccess(theEnv,DeftemplateData(theEnv)->DeftemplateConstruct,logName,argExprs,
                                       (ConstructGetWatchFunction *) DeftemplateGetWatchGoals,
                                       (ConstructSetWatchFunction *) DeftemplateSetWatchGoals);
     }
  }

#endif /* DEBUGGING_FUNCTIONS */

#endif /* DEFTEMPLATE_CONSTRUCT */


