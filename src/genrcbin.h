   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 7.00  06/28/24            */
   /*                                                     */
   /*                                                     */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*      6.40: Removed LOCALE definition.                     */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*      7.00: Generic function support for deftemplates.     */
/*                                                           */
/*************************************************************/

#ifndef _H_genrcbin

#pragma once

#define _H_genrcbin

#include "genrcfun.h"

#define GENRCBIN_DATA 28

struct defgenericBinaryData
  {
   Defgeneric *DefgenericArray;
   unsigned long ModuleCount;
   unsigned long GenericCount;
   unsigned long MethodCount;
   unsigned long RestrictionCount;
   unsigned long TypeCount;
   DEFGENERIC_MODULE *ModuleArray;
   Defmethod *MethodArray;
   RESTRICTION *RestrictionArray;
   RESTRICTION_TYPE *TypeArray;
  };

#define DefgenericBinaryData(theEnv) ((struct defgenericBinaryData *) GetEnvironmentData(theEnv,GENRCBIN_DATA))

#define GenericPointer(i) (((i) == ULONG_MAX) ? NULL : &DefgenericBinaryData(theEnv)->DefgenericArray[i])

   void                           SetupGenericsBload(Environment *);
   void                          *BloadDefgenericModuleReference(Environment *,unsigned long);

#endif /* _H_genrcbin */




