   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 7.00  12/24/23            */
   /*                                                     */
   /*               CLASS PARSER HEADER FILE              */
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
/*      6.24: Added allowed-classes slot facet.               */
/*                                                            */
/*            Converted INSTANCE_PATTERN_MATCHING to          */
/*            DEFRULE_CONSTRUCT.                              */
/*                                                            */
/*            Renamed BOOLEAN macro type to intBool.          */
/*                                                            */
/*      6.30: Added support to allow CreateClassScopeMap to   */
/*            be used by other functions.                     */
/*                                                            */
/*            Changed integer type/precision.                 */
/*                                                            */
/*            GetConstructNameAndComment API change.          */
/*                                                            */
/*            Added const qualifiers to remove C++            */
/*            deprecation warnings.                           */
/*                                                            */
/*            Converted API macros to function calls.         */
/*                                                            */
/*      6.40: Removed LOCALE definition.                     */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            Eval support for run time and bload only.      */
/*                                                           */
/*************************************************************/

#ifndef _H_classpsr

#pragma once

#define _H_classpsr

#if OBJECT_SYSTEM

#if (! BLOAD_ONLY) && (! RUN_TIME)
   bool                    ParseDefclass(Environment *,const char *);

#endif

#if DEFMODULE_CONSTRUCT
   CLIPSBitMap            *CreateClassScopeMap(Environment *,Defclass *);
#endif

#endif /* OBJECT_SYSTEM */

#endif /* _H_classpsr */



