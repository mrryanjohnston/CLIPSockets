   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 7.00  10/13/23             */
   /*                                                     */
   /*           FACT LHS PATTERN PARSING MODULE           */
   /*******************************************************/

/*************************************************************/
/* Purpose: Contains routines for integration of ordered and */
/*   deftemplate fact patterns with the defrule LHS pattern  */
/*   parser including routines for recognizing fact          */
/*   patterns, parsing ordered fact patterns, and initiating */
/*   the parsing of deftemplate fact patterns.               */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Initialize the exists member.                  */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            Removed initial-fact support.                  */
/*                                                           */
/*      7.00: Support for data driven backward chaining.     */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if DEFTEMPLATE_CONSTRUCT && DEFRULE_CONSTRUCT && (! RUN_TIME) && (! BLOAD_ONLY)

#include <stdio.h>
#include <string.h>

#include "cstrcpsr.h"
#include "envrnmnt.h"
#include "modulpsr.h"
#include "modulutl.h"
#include "pattern.h"
#include "pprint.h"
#include "prntutil.h"
#include "reorder.h"
#include "router.h"
#include "tmpltdef.h"
#include "tmpltlhs.h"
#include "tmpltpsr.h"
#include "tmpltutl.h"

#include "factlhs.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static void                    PropagateGoalFlags(struct lhsParseNode *,bool,bool);
  
/***********************************************/
/* SequenceRestrictionParse: Parses an ordered */
/*   fact pattern conditional element.         */
/*                                             */
/*   <ordered-fact-pattern-CE>                 */
/*             ::= (<symbol> <constraint>+)    */
/***********************************************/
struct lhsParseNode *SequenceRestrictionParse(
  Environment *theEnv,
  const char *readSource,
  struct token *theToken)
  {
   struct lhsParseNode *topNode;
   struct lhsParseNode *nextField;

   /*================================================*/
   /* Create the pattern node for the relation name. */
   /*================================================*/

   topNode = GetLHSParseNode(theEnv);
   topNode->pnType = SF_WILDCARD_NODE;
   topNode->negated = false;
   topNode->exists = false;
   topNode->index = NO_INDEX;
   topNode->slotNumber = 1;
   topNode->bottom = GetLHSParseNode(theEnv);
   topNode->bottom->pnType = SYMBOL_NODE;
   topNode->bottom->negated = false;
   topNode->bottom->exists = false;
   topNode->bottom->value = theToken->value;

   /*======================================================*/
   /* Connective constraints cannot be used in conjunction */
   /* with the first field of a pattern.                   */
   /*======================================================*/

   SavePPBuffer(theEnv," ");
   GetToken(theEnv,readSource,theToken);
   if ((theToken->tknType == OR_CONSTRAINT_TOKEN) ||
       (theToken->tknType == AND_CONSTRAINT_TOKEN))
     {
      ReturnLHSParseNodes(theEnv,topNode);
      SyntaxErrorMessage(theEnv,"the first field of a pattern");
      return NULL;
     }

   /*============================================================*/
   /* Treat the remaining constraints of an ordered fact pattern */
   /* as if they were contained in a multifield slot.            */
   /*============================================================*/

   nextField = RestrictionParse(theEnv,readSource,theToken,true,NULL,1,NULL,1);
   if (nextField == NULL)
     {
      ReturnLHSParseNodes(theEnv,topNode);
      return NULL;
     }
   topNode->right = nextField;

   /*================================================*/
   /* The pattern must end with a right parenthesis. */
   /*================================================*/

   if (theToken->tknType != RIGHT_PARENTHESIS_TOKEN)
     {
      PPBackup(theEnv);
      SavePPBuffer(theEnv," ");
      SavePPBuffer(theEnv,theToken->printForm);
      SyntaxErrorMessage(theEnv,"fact patterns");
      ReturnLHSParseNodes(theEnv,topNode);
      return NULL;
     }

   /*====================================*/
   /* Fix the pretty print output if the */
   /* slot contained no restrictions.    */
   /*====================================*/

   if (nextField->bottom == NULL)
     {
      PPBackup(theEnv);
      PPBackup(theEnv);
      SavePPBuffer(theEnv,")");
     }

   /*===================================*/
   /* If no errors, return the pattern. */
   /*===================================*/

   return(topNode);
  }

/**********************************************************************/
/* FactPatternParserFind: This function is the pattern find function  */
/*   for facts. It tells the pattern parsing code that the specified  */
/*   pattern can be parsed as a fact pattern. By default, any pattern */
/*   beginning with a symbol can be parsed as a fact pattern. Since   */
/*   all patterns begin with a symbol, it follows that all patterns   */
/*   can be parsed as a fact pattern.                                 */
/**********************************************************************/
bool FactPatternParserFind(
  CLIPSLexeme *theRelation)
  {
#if MAC_XCD
#pragma unused(theRelation)
#endif
   return true;
  }

/******************************************************/
/* FactPatternParse: This function is called to parse */
/*  both deftemplate and ordered fact patterns.       */
/******************************************************/
struct lhsParseNode *FactPatternParse(
  Environment *theEnv,
  const char *readSource,
  struct token *theToken)
  {
   Deftemplate *theDeftemplate;
   unsigned int count;
   bool goalPattern = false;
   bool explicitPattern = false;
   struct lhsParseNode *rv;

   if (strcmp(theToken->lexemeValue->contents,"goal") == 0)
     { goalPattern = true; }
   else if (strcmp(theToken->lexemeValue->contents,"explicit") == 0)
     { explicitPattern = true; }

   if (goalPattern || explicitPattern)
     {
      SavePPBuffer(theEnv," ");
      GetToken(theEnv,readSource,theToken);
      
      if (theToken->tknType != LEFT_PARENTHESIS_TOKEN)
        {
         PrintErrorID(theEnv,"FACTLHS",1,false);
         if (goalPattern)
           { WriteString(theEnv,STDERR,"Expected a deftemplate pattern after the goal keyword.\n"); }
         else
           { WriteString(theEnv,STDERR,"Expected a deftemplate pattern after the explicit keyword.\n"); }
         return NULL;
        }

      GetToken(theEnv,readSource,theToken);

      if (theToken->tknType != SYMBOL_TOKEN)
        {
         PrintErrorID(theEnv,"FACTLHS",2,false);
         WriteString(theEnv,STDERR,"Expected a deftemplate name.\n");
         return NULL;
        }

      if (ReservedPatternSymbol(theEnv,theToken->lexemeValue->contents,NULL))
        {
         ReservedPatternSymbolErrorMsg(theEnv,theToken->lexemeValue->contents,"a deftemplate name");
         return NULL;
        }
     }
     
   /*=========================================*/
   /* A module separator can not be included  */
   /* as part of the pattern's relation name. */
   /*=========================================*/

   if (FindModuleSeparator(theToken->lexemeValue->contents))
     {
      IllegalModuleSpecifierMessage(theEnv);
      return NULL;
     }

   /*=========================================================*/
   /* Find the deftemplate associated with the relation name. */
   /*=========================================================*/

   theDeftemplate = (Deftemplate *)
                    FindImportedConstruct(theEnv,"deftemplate",NULL,theToken->lexemeValue->contents,
                                          &count,true,NULL);

   if (count > 1)
     {
      AmbiguousReferenceErrorMessage(theEnv,"deftemplate",theToken->lexemeValue->contents);
      return NULL;
     }

   /*======================================================*/
   /* If no deftemplate exists with the specified relation */
   /* name, then create an implied deftemplate.            */
   /*======================================================*/

   if (theDeftemplate == NULL)
     {
#if DEFMODULE_CONSTRUCT
      if (FindImportExportConflict(theEnv,"deftemplate",GetCurrentModule(theEnv),theToken->lexemeValue->contents))
        {
         ImportExportConflictMessage(theEnv,"implied deftemplate",theToken->lexemeValue->contents,NULL,NULL);
         return NULL;
        }
#endif /* DEFMODULE_CONSTRUCT */

      if (! ConstructData(theEnv)->CheckSyntaxMode)
        { theDeftemplate = CreateImpliedDeftemplate(theEnv,theToken->lexemeValue,true); }
      else
        { theDeftemplate = NULL; }
     }

   /*=================================================*/
   /* If an explicit deftemplate exists, then parse   */
   /* the pattern as a deftemplate pattern, otherwise */
   /* parse as an ordered fact pattern.               */
   /*=================================================*/

   if ((theDeftemplate != NULL) && (theDeftemplate->implied == false))
     { rv = DeftemplateLHSParse(theEnv,readSource,theDeftemplate); }
   else
     { rv = SequenceRestrictionParse(theEnv,readSource,theToken); }

   /*===================================*/
   /* Check for the closing parenthesis */
   /* of the goal conditional element.  */
   /*===================================*/
   
   if ((goalPattern || explicitPattern) && (rv != NULL))
     {
      GetToken(theEnv,readSource,theToken);
      if (theToken->tknType != RIGHT_PARENTHESIS_TOKEN)
        {
         ReturnLHSParseNodes(theEnv,rv);
         PrintErrorID(theEnv,"FACTLHS",4,false);
         if (goalPattern)
           { WriteString(theEnv,STDERR,"Expected a ')' to close the goal conditional element.\n"); }
         else
           { WriteString(theEnv,STDERR,"Expected a ')' to close the explicit conditional element.\n"); }
         return NULL;
        }
        
      PropagateGoalFlags(rv,goalPattern,explicitPattern);
     }
        
   return rv;
  }

/***************************************************/
/* PropagateGoalFlags: Recursively sets the goalCE */
/*   flag for each component of a pattern.         */
/***************************************************/
static void PropagateGoalFlags(
  struct lhsParseNode *theField,
  bool goalPattern,
  bool explicitPattern)
  {
   while (theField != NULL)
     {
      theField->goalCE = goalPattern;
      theField->explicitCE = explicitPattern;

      PropagateGoalFlags(theField->right,goalPattern,explicitPattern);
      PropagateGoalFlags(theField->expression,goalPattern,explicitPattern);

      theField = theField->bottom;
     }
  }

#endif /* DEFTEMPLATE_CONSTRUCT && DEFRULE_CONSTRUCT && (! RUN_TIME) && (! BLOAD_ONLY) */
