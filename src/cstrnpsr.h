   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 7.00  12/02/23            */
   /*                                                     */
   /*             CONSTRAINT PARSER HEADER FILE           */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides functions for parsing constraint        */
/*   declarations.                                           */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Changed name of variable exp to theExp         */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Added allowed-classes slot facet.              */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Used gensprintf instead of sprintf.            */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*      6.40: Removed LOCALE definition.                     */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*************************************************************/

#ifndef _H_cstrnpsr

#pragma once

#define _H_cstrnpsr

#include "constrnt.h"

struct constraintParseRecord
  {
   unsigned int type : 1;
   unsigned int range : 1;
   unsigned int allowedSymbols : 1;
   unsigned int allowedStrings : 1;
   unsigned int allowedLexemes : 1;
   unsigned int allowedFloats : 1;
   unsigned int allowedIntegers : 1;
   unsigned int allowedNumbers : 1;
   unsigned int allowedValues : 1;
   unsigned int allowedClasses : 1;
   unsigned int allowedInstanceNames : 1;
   unsigned int cardinality : 1;
  };

typedef struct constraintParseRecord CONSTRAINT_PARSE_RECORD;

   bool                           CheckConstraintParseConflicts(Environment *,CONSTRAINT_RECORD *);
   void                           AttributeConflictErrorMessage(Environment *,const char *,const char *);
#if (! RUN_TIME) && (! BLOAD_ONLY)
   void                           InitializeConstraintParseRecord(CONSTRAINT_PARSE_RECORD *);
   bool                           StandardConstraint(const char *);
   bool                           ParseStandardConstraint(Environment *,const char *,const char *,
                                                                 CONSTRAINT_RECORD *,
                                                                 CONSTRAINT_PARSE_RECORD *,
                                                                 bool);
   void                           OverlayConstraint(Environment *,CONSTRAINT_PARSE_RECORD *,
                                                           CONSTRAINT_RECORD *,CONSTRAINT_RECORD *);
   void                           OverlayConstraintParseRecord(CONSTRAINT_PARSE_RECORD *,
                                                                      CONSTRAINT_PARSE_RECORD *);
   void                           NoConjunctiveUseError(Environment *,const char *,const char *);
#endif /* (! RUN_TIME) && (! BLOAD_ONLY) */

#endif /* _H_cstrnpsr */



