   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 7.00  02/05/25             */
   /*                                                     */
   /*              STRING_TYPE I/O ROUTER MODULE               */
   /*******************************************************/

/*************************************************************/
/* Purpose: I/O Router routines which allow strings to be    */
/*   used as input and output sources.                       */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Used genstrcpy instead of strcpy.              */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Changed return values for router functions.    */
/*                                                           */
/*************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "setup.h"

#include "constant.h"
#include "envrnmnt.h"
#include "memalloc.h"
#include "prntutil.h"
#include "router.h"
#include "sysdep.h"

#include "strngrtr.h"

#define READ_STRING 0
#define WRITE_STRING 1

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static bool                    QueryStringCallback(Environment *,const char *,void *);
   static void                    WriteStringCallback(Environment *,const char *,const char *,void *);
   static int                     ReadStringCallback(Environment *,const char *,void *);
   static int                     UnreadStringCallback(Environment *,const char *,int,void *);
   static StringRouter           *FindStringRouter(Environment *,const char *);
   static bool                    CreateReadStringSource(Environment *,const char *,const char *,size_t,size_t);
   static void                    DeallocateStringRouterData(Environment *);
   static StringBuilderRouter    *FindStringBuilderRouter(Environment *,const char *);
   static bool                    QueryStringBuilderCallback(Environment *,const char *,void *);
   static void                    WriteStringBuilderCallback(Environment *,const char *,const char *,void *);

/**********************************************************/
/* InitializeStringRouter: Initializes string I/O router. */
/**********************************************************/
void InitializeStringRouter(
  Environment *theEnv)
  {
   AllocateEnvironmentData(theEnv,STRING_ROUTER_DATA,sizeof(struct stringRouterData),DeallocateStringRouterData);

   AddRouter(theEnv,"string",0,QueryStringCallback,WriteStringCallback,ReadStringCallback,UnreadStringCallback,NULL,NULL);
   AddRouter(theEnv,"stringBuilder",0,QueryStringBuilderCallback,WriteStringBuilderCallback,NULL,NULL,NULL,NULL);
  }

/*******************************************/
/* DeallocateStringRouterData: Deallocates */
/*    environment data for string routers. */
/*******************************************/
static void DeallocateStringRouterData(
  Environment *theEnv)
  {
   StringRouter *tmpPtr, *nextPtr;
   StringBuilderRouter *tmpSBPtr, *nextSBPtr;

   tmpPtr = StringRouterData(theEnv)->ListOfStringRouters;
   while (tmpPtr != NULL)
     {
      nextPtr = tmpPtr->next;
      rm(theEnv,(void *) tmpPtr->name,strlen(tmpPtr->name) + 1);
      rtn_struct(theEnv,stringRouter,tmpPtr);
      tmpPtr = nextPtr;
     }

   tmpSBPtr = StringRouterData(theEnv)->ListOfStringBuilderRouters;
   while (tmpSBPtr != NULL)
     {
      nextSBPtr = tmpSBPtr->next;
      rm(theEnv,(void *) tmpSBPtr->name,strlen(tmpSBPtr->name) + 1);
      rtn_struct(theEnv,stringBuilderRouter,tmpSBPtr);
      tmpSBPtr = nextSBPtr;
     }
  }

/*********************************************************************/
/* QueryStringCallback: Find routine for string router logical names. */
/*********************************************************************/
static bool QueryStringCallback(
  Environment *theEnv,
  const char *logicalName,
  void *context)
  {
   struct stringRouter *head;

   head = StringRouterData(theEnv)->ListOfStringRouters;
   while (head != NULL)
     {
      if (strcmp(head->name,logicalName) == 0)
        { return true; }
      head = head->next;
     }

   return false;
  }

/**********************************************************/
/* WriteStringCallback: Print routine for string routers. */
/**********************************************************/
static void WriteStringCallback(
  Environment *theEnv,
  const char *logicalName,
  const char *str,
  void *context)
  {
   struct stringRouter *head;
   size_t length;

   head = FindStringRouter(theEnv,logicalName);
   if (head == NULL)
     {
      SystemError(theEnv,"ROUTER",3);
      ExitRouter(theEnv,EXIT_FAILURE);
      return;
     }

   if (head->readWriteType != WRITE_STRING) return;

   length = strlen(str);
   
   if ((length + head->currentPosition + 1) > head->maximumPosition)
     { return; }

   head->writeString[head->currentPosition] = EOS;
   genstrncat(&head->writeString[head->currentPosition],
              str,length+1);

   head->currentPosition += length;
  }

/********************************************************/
/* ReadStringCallback: Getc routine for string routers. */
/********************************************************/
static int ReadStringCallback(
  Environment *theEnv,
  const char *logicalName,
  void *context)
  {
   struct stringRouter *head;
   int rc;

   head = FindStringRouter(theEnv,logicalName);
   if (head == NULL)
     {
      SystemError(theEnv,"ROUTER",1);
      ExitRouter(theEnv,EXIT_FAILURE);
     }

   if (head->readWriteType != READ_STRING) return(EOF);
   if (head->currentPosition >= head->maximumPosition)
     {
      head->currentPosition++;
      return(EOF);
     }

   rc = (unsigned char) head->readString[head->currentPosition];
   head->currentPosition++;

   return(rc);
  }

/************************************************************/
/* UnreadStringCallback: Ungetc routine for string routers. */
/************************************************************/
static int UnreadStringCallback(
  Environment *theEnv,
  const char *logicalName,
  int ch,
  void *context)
  {
   struct stringRouter *head;
#if MAC_XCD
#pragma unused(ch)
#endif

   head = FindStringRouter(theEnv,logicalName);

   if (head == NULL)
     {
      SystemError(theEnv,"ROUTER",2);
      ExitRouter(theEnv,EXIT_FAILURE);
     }

   if (head->readWriteType != READ_STRING) return 0;
   if (head->currentPosition > 0)
     { head->currentPosition--; }

   return 1;
  }

/************************************************/
/* OpenStringSource: Opens a new string router. */
/************************************************/
bool OpenStringSource(
  Environment *theEnv,
  const char *name,
  const char *str,
  size_t currentPosition)
  {
   size_t maximumPosition;

   if (str == NULL)
     {
      currentPosition = 0;
      maximumPosition = 0;
     }
   else
     { maximumPosition = strlen(str); }

   return(CreateReadStringSource(theEnv,name,str,currentPosition,maximumPosition));
  }

/******************************************************/
/* OpenTextSource: Opens a new string router for text */
/*   (which is not NULL terminated).                  */
/******************************************************/
bool OpenTextSource(
  Environment *theEnv,
  const char *name,
  const char *str,
  size_t currentPosition,
  size_t maximumPosition)
  {
   if (str == NULL)
     {
      currentPosition = 0;
      maximumPosition = 0;
     }

   return CreateReadStringSource(theEnv,name,str,currentPosition,maximumPosition);
  }

/******************************************************************/
/* CreateReadStringSource: Creates a new string router for input. */
/******************************************************************/
static bool CreateReadStringSource(
  Environment *theEnv,
  const char *name,
  const char *str,
  size_t currentPosition,
  size_t maximumPosition)
  {
   struct stringRouter *newStringRouter;
   char *theName;

   if (FindStringRouter(theEnv,name) != NULL) return false;

   newStringRouter = get_struct(theEnv,stringRouter);
   theName = (char *) gm1(theEnv,strlen(name) + 1);
   genstrcpy(theName,name);
   newStringRouter->name = theName;
   newStringRouter->writeString = NULL;
   newStringRouter->readString = str;
   newStringRouter->currentPosition = currentPosition;
   newStringRouter->readWriteType = READ_STRING;
   newStringRouter->maximumPosition = maximumPosition;
   newStringRouter->next = StringRouterData(theEnv)->ListOfStringRouters;
   StringRouterData(theEnv)->ListOfStringRouters = newStringRouter;

   return true;
  }

/**********************************************/
/* CloseStringSource: Closes a string router. */
/**********************************************/
bool CloseStringSource(
  Environment *theEnv,
  const char *name)
  {
   struct stringRouter *head, *last;

   last = NULL;
   head = StringRouterData(theEnv)->ListOfStringRouters;
   while (head != NULL)
     {
      if (strcmp(head->name,name) == 0)
        {
         if (last == NULL)
           {
            StringRouterData(theEnv)->ListOfStringRouters = head->next;
            rm(theEnv,(void *) head->name,strlen(head->name) + 1);
            rtn_struct(theEnv,stringRouter,head);
            return true;
           }
         else
           {
            last->next = head->next;
            rm(theEnv,(void *) head->name,strlen(head->name) + 1);
            rtn_struct(theEnv,stringRouter,head);
            return true;
           }
        }
      last = head;
      head = head->next;
     }

   return false;
  }

/******************************************************************/
/* OpenStringDestination: Opens a new string router for printing. */
/******************************************************************/
bool OpenStringDestination(
  Environment *theEnv,
  const char *name,
  char *str,
  size_t maximumPosition)
  {
   struct stringRouter *newStringRouter;
   char *theName;

   if (FindStringRouter(theEnv,name) != NULL) return false;

   newStringRouter = get_struct(theEnv,stringRouter);
   theName = (char *) gm1(theEnv,strlen(name) + 1);
   genstrcpy(theName,name);
   newStringRouter->name = theName;
   newStringRouter->readString = NULL;
   newStringRouter->writeString = str;
   newStringRouter->currentPosition = 0;
   newStringRouter->readWriteType = WRITE_STRING;
   newStringRouter->maximumPosition = maximumPosition;
   newStringRouter->next = StringRouterData(theEnv)->ListOfStringRouters;
   StringRouterData(theEnv)->ListOfStringRouters = newStringRouter;

   return true;
  }

/***************************************************/
/* CloseStringDestination: Closes a string router. */
/***************************************************/
bool CloseStringDestination(
  Environment *theEnv,
  const char *name)
  {
   return CloseStringSource(theEnv,name);
  }

/*******************************************************************/
/* FindStringRouter: Returns a pointer to the named string router. */
/*******************************************************************/
static struct stringRouter *FindStringRouter(
  Environment *theEnv,
  const char *name)
  {
   struct stringRouter *head;

   head = StringRouterData(theEnv)->ListOfStringRouters;
   while (head != NULL)
     {
      if (strcmp(head->name,name) == 0)
        { return(head); }
      head = head->next;
     }

   return NULL;
  }

/*********************************************/
/* OpenStringBuilderDestination: Opens a new */
/*   StringBuilder router for printing.      */
/*********************************************/
bool OpenStringBuilderDestination(
  Environment *theEnv,
  const char *name,
  StringBuilder *theSB)
  {
   StringBuilderRouter *newStringRouter;
   char *theName;

   if (FindStringBuilderRouter(theEnv,name) != NULL) return false;

   newStringRouter = get_struct(theEnv,stringBuilderRouter);
   theName = (char *) gm1(theEnv,strlen(name) + 1);
   genstrcpy(theName,name);
   newStringRouter->name = theName;
   newStringRouter->SBR = theSB;
   newStringRouter->next = StringRouterData(theEnv)->ListOfStringBuilderRouters;
   StringRouterData(theEnv)->ListOfStringBuilderRouters = newStringRouter;

   return true;
  }

/*****************************************/
/* CloseStringBuilderDestination: Closes */
/*   a StringBuilder router.             */
/*****************************************/
bool CloseStringBuilderDestination(
  Environment *theEnv,
  const char *name)
  {
   StringBuilderRouter *head, *last;

   last = NULL;
   head = StringRouterData(theEnv)->ListOfStringBuilderRouters;
   while (head != NULL)
     {
      if (strcmp(head->name,name) == 0)
        {
         if (last == NULL)
           {
            StringRouterData(theEnv)->ListOfStringBuilderRouters = head->next;
            rm(theEnv,(void *) head->name,strlen(head->name) + 1);
            rtn_struct(theEnv,stringBuilderRouter,head);
            return true;
           }
         else
           {
            last->next = head->next;
            rm(theEnv,(void *) head->name,strlen(head->name) + 1);
            rtn_struct(theEnv,stringBuilderRouter,head);
            return true;
           }
        }
      last = head;
      head = head->next;
     }

   return false;
  }

/**********************************************/
/* FindStringBuilderRouter: Returns a pointer */
/*   to the named StringBuilder router.       */
/**********************************************/
static struct stringBuilderRouter *FindStringBuilderRouter(
  Environment *theEnv,
  const char *name)
  {
   StringBuilderRouter *head;

   head = StringRouterData(theEnv)->ListOfStringBuilderRouters;
   while (head != NULL)
     {
      if (strcmp(head->name,name) == 0)
        { return head; }
      head = head->next;
     }

   return NULL;
  }

/*********************************************/
/* QueryStringBuilderCallback: Query routine */
/*   for stringBuilder router logical names. */
/*********************************************/
static bool QueryStringBuilderCallback(
  Environment *theEnv,
  const char *logicalName,
  void *context)
  {
   StringBuilderRouter *head;

   head = StringRouterData(theEnv)->ListOfStringBuilderRouters;
   while (head != NULL)
     {
      if (strcmp(head->name,logicalName) == 0)
        { return true; }
      head = head->next;
     }

   return false;
  }

/*********************************************/
/* WriteStringBuilderCallback: Print routine */
/*    for stringBuilder routers.             */
/*********************************************/
static void WriteStringBuilderCallback(
  Environment *theEnv,
  const char *logicalName,
  const char *str,
  void *context)
  {
   StringBuilderRouter *head;

   head = FindStringBuilderRouter(theEnv,logicalName);
   if (head == NULL)
     {
      SystemError(theEnv,"ROUTER",3);
      ExitRouter(theEnv,EXIT_FAILURE);
      return;
     }
     
   SBAppend(head->SBR,str);
  }


