   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 7.00  02/06/24            */
   /*                                                     */
   /*        DEFTABLE CONSTRUCT COMPILER HEADER FILE      */
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

#ifndef _H_tablecmp

#pragma once

#define _H_tablecmp

#include "tabledef.h"

   void                           DeftableCompilerSetup(Environment *);
   void                           DeftableCModuleReference(Environment *,FILE *,unsigned long,unsigned int,unsigned int);
   void                           DeftableCConstructReference(Environment *,FILE *,Deftable *,unsigned int,unsigned int);

#endif /* _H_tablecmp */
