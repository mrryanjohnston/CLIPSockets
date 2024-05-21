   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 7.00  01/23/24            */
   /*                                                     */
   /*           DEFMODULE BSAVE/BLOAD HEADER FILE         */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*      6.40: Removed LOCALE definition.                     */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*      7.00: Construct hashing for quick lookup.            */
/*                                                           */
/*************************************************************/

#ifndef _H_modulbin

#pragma once

#define _H_modulbin

#include "moduldef.h"
#include "modulbin.h"
#include "cstrcbin.h"

#if (! RUN_TIME)

struct bsaveDefmodule
  {
   struct bsaveConstructHeader header;
   unsigned long importList;
   unsigned long exportList;
   unsigned long bsaveID;
  };

struct bsaveDefmoduleItemHeader
  {
   unsigned long theModule;
   unsigned long firstItem;
   unsigned long lastItem;
   unsigned long itemCount;
  };

struct bsavePortItem
  {
   unsigned long moduleName;
   unsigned long constructType;
   unsigned long constructName;
   unsigned long next;
  };

#define ModulePointer(i) ((Defmodule *) (&DefmoduleData(theEnv)->DefmoduleArray[i]))

   void                           DefmoduleBinarySetup(Environment *);
   void                           UpdateDefmoduleItemHeader
                                                 (Environment *,struct bsaveDefmoduleItemHeader *,
                                                  struct defmoduleItemHeader *,size_t,void *);
   void                           UpdateDefmoduleItemHeaderHM
                                                 (Environment *,struct bsaveDefmoduleItemHeader *,
                                                  struct defmoduleItemHeaderHM *,size_t,void *);

#if BLOAD_AND_BSAVE
   void                           AssignBsaveDefmdlItemHdrVals
                                                 (struct bsaveDefmoduleItemHeader *,
                                                  struct defmoduleItemHeader *);
   void                           AssignBsaveDefmdlItemHdrHMVals
                                                 (struct bsaveDefmoduleItemHeader *,
                                                  struct defmoduleItemHeaderHM *);
#endif

#endif /* RUN_TIME */

#endif /* _H_modulbin */



