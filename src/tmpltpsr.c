   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 7.00  08/08/24             */
   /*                                                     */
   /*              DEFTEMPLATE PARSER MODULE              */
   /*******************************************************/

/*************************************************************/
/* Purpose: Parses the deftemplate construct.                */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Added support for templates maintaining their  */
/*            own list of facts.                             */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            GetConstructNameAndComment API change.         */
/*                                                           */
/*            Support for deftemplate slot facets.           */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Changed find construct functionality so that   */
/*            imported modules are search when locating a    */
/*            named construct.                               */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            Static constraint checking is always enabled.  */
/*                                                           */
/*      7.00: Data driven backward chaining.                 */
/*                                                           */
/*            Deftemplate inheritance.                       */
/*                                                           */
/*            Support for non-reactive fact patterns.        */
/*                                                           */
/*            Construct hashing for quick lookup.            */
/*                                                           */
/*            Support for certainty factors.                 */
/*                                                           */
/*            Support for named facts.                       */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if DEFTEMPLATE_CONSTRUCT

#include <stdio.h>
#include <string.h>

#if BLOAD || BLOAD_AND_BSAVE
#include "bload.h"
#endif
#include "constant.h"
#include "constrct.h"
#include "cstrcpsr.h"
#include "cstrnchk.h"
#include "cstrnpsr.h"
#include "cstrnutl.h"
#include "default.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "factmngr.h"
#include "memalloc.h"
#include "modulutl.h"
#include "pattern.h"
#include "pprint.h"
#include "prntutil.h"
#include "router.h"
#include "scanner.h"
#include "symbol.h"
#include "tmpltbsc.h"
#include "tmpltdef.h"
#include "watch.h"

#include "tmpltpsr.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if (! RUN_TIME) && (! BLOAD_ONLY)
   static struct templateSlot    *SlotDeclarations(Environment *,const char *,struct token *,Deftemplate **,bool *,bool *);
   static struct templateSlot    *ParseSlot(Environment *,const char *,struct token *,struct templateSlot *,Deftemplate *,bool,bool);
   static struct templateSlot    *DefinedSlots(Environment *,const char *,CLIPSLexeme *,bool,struct token *,struct templateSlot *);
   static bool                    ParseFacetAttribute(Environment *,const char *,struct templateSlot *,bool);
   static bool                    InheritedAllowedCheck(Environment *,const char *,struct templateSlot *,struct templateSlot *,const char *,unsigned);
   static bool                    InheritedAllowedClassesCheck(Environment *,const char *,struct templateSlot *,struct templateSlot *,const char *);
   static void                    AssignInheritedAttributes(Environment *,struct templateSlot *,struct templateSlot *);
   static struct templateSlot    *CreateSlot(Environment *);
   static struct templateSlot    *CopySlot(Environment *,struct templateSlot *);
   static bool                    ParseSimpleQualifier(Environment *,const char *,const char *,const char *,const char *,bool *);
   static bool                    ParseIsA(Environment *,const char *,struct token *,Deftemplate **,bool *,bool *);
   static bool                    ParseBaseDeftemplates(Environment *,const char *,struct token *,Deftemplate **,bool *,bool *);
  
#if CERTAINTY_FACTORS
   static struct templateSlot    *CreateCFSlot(Environment *);
#endif
   static struct templateSlot    *CreateNameSlot(Environment *);
#endif

/*******************************************************/
/* ParseDeftemplate: Parses the deftemplate construct. */
/*******************************************************/
bool ParseDeftemplate(
  Environment *theEnv,
  const char *readSource)
  {
#if (! RUN_TIME) && (! BLOAD_ONLY)
   CLIPSLexeme *deftemplateName;
   Deftemplate *newDeftemplate, *parent = NULL;
   struct templateSlot *slots;
   struct token inputToken;
   bool cfd = false, named = false;

   /*================================================*/
   /* Initialize pretty print and error information. */
   /*================================================*/

   DeftemplateData(theEnv)->DeftemplateError = false;
   SetPPBufferStatus(theEnv,true);
   FlushPPBuffer(theEnv);
   SavePPBuffer(theEnv,"(deftemplate ");

   /*==============================================================*/
   /* Deftemplates can not be added when a binary image is loaded. */
   /*==============================================================*/

#if BLOAD || BLOAD_AND_BSAVE
   if ((Bloaded(theEnv) == true) && (! ConstructData(theEnv)->CheckSyntaxMode))
     {
      CannotLoadWithBloadMessage(theEnv,"deftemplate");
      return true;
     }
#endif

   /*=======================================================*/
   /* Parse the name and comment fields of the deftemplate. */
   /*=======================================================*/

#if DEBUGGING_FUNCTIONS
   DeftemplateData(theEnv)->DeletedTemplateDebugFlags = 0;
#endif

   deftemplateName = GetConstructNameAndComment(theEnv,readSource,&inputToken,"deftemplate",
                                                (FindConstructFunction *) FindDeftemplateInModule,
                                                (DeleteConstructFunction *) Undeftemplate,"%",
                                                true,true,true,false);

   if (deftemplateName == NULL) return true;

   if (ReservedPatternSymbol(theEnv,deftemplateName->contents,"deftemplate"))
     {
      ReservedPatternSymbolErrorMsg(theEnv,deftemplateName->contents,"a deftemplate name");
      return true;
     }

   /*===========================================*/
   /* Parse the slot fields of the deftemplate. */
   /*===========================================*/

   slots = SlotDeclarations(theEnv,readSource,&inputToken,&parent,&cfd,&named);
   if (DeftemplateData(theEnv)->DeftemplateError == true) return true;

   /*==============================================*/
   /* If we're only checking syntax, don't add the */
   /* successfully parsed deftemplate to the KB.   */
   /*==============================================*/

   if (ConstructData(theEnv)->CheckSyntaxMode)
     {
      ReturnSlots(theEnv,slots);
      return false;
     }

   /*=====================================*/
   /* Create a new deftemplate structure. */
   /*=====================================*/

   newDeftemplate = get_struct(theEnv,deftemplate);
   newDeftemplate->header.name =  deftemplateName;
   newDeftemplate->header.next = NULL;
   newDeftemplate->header.usrData = NULL;
   newDeftemplate->header.constructType = DEFTEMPLATE;
   newDeftemplate->header.env = theEnv;
   
   newDeftemplate->parent = parent;
   newDeftemplate->child = NULL;
   if (parent == NULL)
     { newDeftemplate->sibling = NULL; }
   else
     {
      newDeftemplate->sibling = parent->child;
      parent->child = newDeftemplate;
      IncrementDeftemplateBusyCount(theEnv,parent);
     }
     
   newDeftemplate->slotList = slots;
   newDeftemplate->implied = false;
   newDeftemplate->numberOfSlots = 0;
   newDeftemplate->busyCount = 0;
   newDeftemplate->watchFacts = 0;
   newDeftemplate->watchGoals = 0;
   newDeftemplate->inScope = true;
   newDeftemplate->cfd = cfd;
   newDeftemplate->named = named;
   newDeftemplate->patternNetwork = NULL;
   newDeftemplate->goalNetwork = NULL;
   newDeftemplate->factList = NULL;
   newDeftemplate->lastFact = NULL;
   newDeftemplate->header.whichModule = (struct defmoduleItemHeader *)
                                        GetModuleItem(theEnv,NULL,DeftemplateData(theEnv)->DeftemplateModuleIndex);

   /*================================*/
   /* Determine the number of slots. */
   /*================================*/

   while (slots != NULL)
     {
      newDeftemplate->numberOfSlots++;
      slots = slots->next;
     }

   /*====================================*/
   /* Store pretty print representation. */
   /*====================================*/

   if (GetConserveMemory(theEnv) == true)
     { newDeftemplate->header.ppForm = NULL; }
   else
     { newDeftemplate->header.ppForm = CopyPPBuffer(theEnv); }

   /*=======================================================================*/
   /* If a template is redefined, then we want to restore its watch status. */
   /*=======================================================================*/

#if DEBUGGING_FUNCTIONS
   if ((BitwiseTest(DeftemplateData(theEnv)->DeletedTemplateDebugFlags,0)) ||
       (GetWatchItem(theEnv,"facts") == 1))
     { DeftemplateSetWatchFacts(newDeftemplate,true); }
     
   if ((BitwiseTest(DeftemplateData(theEnv)->DeletedTemplateDebugFlags,1)) ||
       (GetWatchItem(theEnv,"goals") == 1))
     { DeftemplateSetWatchGoals(newDeftemplate,true); }
#endif

   /*==============================================*/
   /* Add deftemplate to the list of deftemplates. */
   /*==============================================*/

   AddConstructToModule(&newDeftemplate->header);
   AddConstructToHashMap(theEnv,&newDeftemplate->header,newDeftemplate->header.whichModule);

   InstallDeftemplate(theEnv,newDeftemplate);

#else
#if MAC_XCD
#pragma unused(theEnv)
#endif
#endif

   return false;
  }

#if (! RUN_TIME) && (! BLOAD_ONLY)

/**************************************************************/
/* InstallDeftemplate: Increments all occurrences in the hash */
/*   table of symbols found in an deftemplate and adds it to  */
/*   the hash table.                                          */
/**************************************************************/
void InstallDeftemplate(
  Environment *theEnv,
  Deftemplate *theDeftemplate)
  {
   struct templateSlot *slotPtr;
   struct expr *tempExpr;

   IncrementLexemeCount(theDeftemplate->header.name);

   for (slotPtr = theDeftemplate->slotList;
        slotPtr != NULL;
        slotPtr = slotPtr->next)
     {
      IncrementLexemeCount(slotPtr->slotName);
      tempExpr = AddHashedExpression(theEnv,slotPtr->defaultList);
      ReturnExpression(theEnv,slotPtr->defaultList);
      slotPtr->defaultList = tempExpr;
      tempExpr = AddHashedExpression(theEnv,slotPtr->facetList);
      ReturnExpression(theEnv,slotPtr->facetList);
      slotPtr->facetList = tempExpr;
      slotPtr->constraints = AddConstraint(theEnv,slotPtr->constraints);
     }
  }

/********************************************************************/
/* SlotDeclarations: Parses the slot declarations of a deftemplate. */
/********************************************************************/
static struct templateSlot *SlotDeclarations(
  Environment *theEnv,
  const char *readSource,
  struct token *inputToken,
  Deftemplate **parent,
  bool *cfd,
  bool *nd)
  {
   struct templateSlot *newSlot, *slotList = NULL, *lastSlot = NULL;
   struct templateSlot *parentSlot, *childSlot, *updatedSlotList, *lastChildSlot;

   while (inputToken->tknType != RIGHT_PARENTHESIS_TOKEN)
     {
      /*====================================================*/
      /* Slots begin with a '(' followed by a slot keyword. */
      /*====================================================*/

      if (inputToken->tknType != LEFT_PARENTHESIS_TOKEN)
        {
         SyntaxErrorMessage(theEnv,"deftemplate");
         ReturnSlots(theEnv,slotList);
         DeftemplateData(theEnv)->DeftemplateError = true;
         return NULL;
        }

      GetToken(theEnv,readSource,inputToken);
      if (inputToken->tknType != SYMBOL_TOKEN)
        {
         SyntaxErrorMessage(theEnv,"deftemplate");
         ReturnSlots(theEnv,slotList);
         DeftemplateData(theEnv)->DeftemplateError = true;
         return NULL;
        }

      /*=====================*/
      /* Handle is-a keyword */
      /*=====================*/
      
      if (strcmp(inputToken->lexemeValue->contents,"is-a") == 0)
        {
         if (slotList != NULL)
           {
            PrintErrorID(theEnv,"TMPLTPSR",1,true);
            WriteString(theEnv,STDERR,"The is-a declaration must be specified before any slot declarations.\n");
            ReturnSlots(theEnv,slotList);
            DeftemplateData(theEnv)->DeftemplateError = true;
            return NULL;
           }

         if (! ParseIsA(theEnv,readSource,inputToken,parent,cfd,nd))
           {
            ReturnSlots(theEnv,slotList);
            return NULL;
           }

         GetToken(theEnv,readSource,inputToken);
         if (inputToken->tknType != RIGHT_PARENTHESIS_TOKEN)
           {
            PPBackup(theEnv);
            SavePPBuffer(theEnv,"\n   ");
            SavePPBuffer(theEnv,inputToken->printForm);
           }
         continue;
        }
        
      /*=================*/
      /* Parse the slot. */
      /*=================*/

      newSlot = ParseSlot(theEnv,readSource,inputToken,slotList,*parent,*cfd,*nd);
      if (DeftemplateData(theEnv)->DeftemplateError == true)
        {
         ReturnSlots(theEnv,newSlot);
         ReturnSlots(theEnv,slotList);
         return NULL;
        }

      /*===========================================*/
      /* Attach the new slot to the list of slots. */
      /*===========================================*/

      if (newSlot != NULL)
        {
         if (lastSlot == NULL)
           { slotList = newSlot; }
         else
           { lastSlot->next = newSlot; }
         lastSlot = newSlot;
        }

      /*================================*/
      /* Check for closing parenthesis. */
      /*================================*/

      GetToken(theEnv,readSource,inputToken);
      if (inputToken->tknType != RIGHT_PARENTHESIS_TOKEN)
        {
         PPBackup(theEnv);
         SavePPBuffer(theEnv,"\n   ");
         SavePPBuffer(theEnv,inputToken->printForm);
        }
     }

  SavePPBuffer(theEnv,"\n");

  /*=========================================*/
  /* If there are no inherited slots, return */
  /* the list of slots just parsed.          */
  /*=========================================*/
  
  if (*parent == NULL)
    {
     if (*nd)
       {
        newSlot = CreateNameSlot(theEnv);
        newSlot->next = slotList;
        slotList = newSlot;
       }
       
#if CERTAINTY_FACTORS
     if (*cfd)
       {
        newSlot = CreateCFSlot(theEnv);
        newSlot->next = slotList;
        slotList = newSlot;
       }
#endif
     return slotList;
    }
  
  /*===============================================*/
  /* If slots are inherited, preserve the ordering */
  /* of the inherited slots and then add new slots */
  /* to the end of the list in the order parsed.   */
  /*===============================================*/

  updatedSlotList = NULL;
  lastSlot = NULL;
  
  for (parentSlot = (*parent)->slotList;
       parentSlot != NULL;
       parentSlot = parentSlot->next)
    {
     lastChildSlot = NULL;
     
     /*============================================*/
     /* Determine if the parent slot was redefined */
     /* in the child deftemplate.                  */
     /*============================================*/
     
     for (childSlot = slotList;
          childSlot != NULL;
          childSlot = childSlot->next)
       {
        if (childSlot->slotName == parentSlot->slotName)
          { break; }
        lastChildSlot = childSlot;
       }

     /*==================================*/
     /* If found, detach the child slot. */
     /*==================================*/
     
     if (childSlot != NULL)
       {
        if (lastChildSlot == NULL)
          { slotList = childSlot->next; }
        else
          { lastChildSlot->next = childSlot->next; }
          
        childSlot->next = NULL;
       }
       
     /*==============================================*/
     /* Otherwise, create a copy of the parent slot. */
     /*==============================================*/
     
     else
       { childSlot = CopySlot(theEnv,parentSlot); }

     /*=========================================*/
     /* Add the child slot to the updated list. */
     /*=========================================*/
     
     if (lastSlot == NULL)
       { updatedSlotList = childSlot; }
     else
       { lastSlot->next = childSlot; }

     lastSlot = childSlot;
    }

  /*============================================*/
  /* Add any remaining new slots from the child */
  /* deftemplate to updated slot list.          */
  /*============================================*/
  
  if (lastSlot == NULL)
    { updatedSlotList = slotList; }
  else
    { lastSlot->next = slotList; }

  return updatedSlotList;
 }

/***********************************************************/
/* ParseIsA: Parses the is-a declaration of a deftemplate. */
/***********************************************************/
static bool ParseIsA(
  Environment *theEnv,
  const char *readSource,
  struct token *inputToken,
  Deftemplate **parent,
  bool *cfd,
  bool *nd)
  {
   SavePPBuffer(theEnv," ");
         
   GetToken(theEnv,readSource,inputToken);
   if (inputToken->tknType != SYMBOL_TOKEN)
     {
      PrintErrorID(theEnv,"TMPLTPSR",2,true);
      WriteString(theEnv,STDERR,"The is-a declaration expected a valid deftemplate name.\n");
      DeftemplateData(theEnv)->DeftemplateError = true;
      return false;
     }

   /*=============================================================*/
   /* Determine if inheriting from one or more base deftemplates. */
   /*=============================================================*/

   if (strcmp(inputToken->lexemeValue->contents,TEMPLATE_ND_STRING) == 0)
     { return ParseBaseDeftemplates(theEnv,readSource,inputToken,parent,cfd,nd); }
#if CERTAINTY_FACTORS
   else if (strcmp(inputToken->lexemeValue->contents,TEMPLATE_CFD_STRING) == 0)
     { return ParseBaseDeftemplates(theEnv,readSource,inputToken,parent,cfd,nd); }
#endif

   /*==========================================*/
   /* Otherwise, the deftemplate inherits from */
   /* a previously defined deftemplate.        */
   /*==========================================*/
   
   *parent = FindDeftemplate(theEnv,inputToken->lexemeValue->contents);
   if (*parent == NULL)
     {
      CantFindItemErrorMessage(theEnv,"deftemplate",inputToken->lexemeValue->contents,true);
      DeftemplateData(theEnv)->DeftemplateError = true;
      return false;
     }

   if ((*parent)->implied)
     {
      PrintErrorID(theEnv,"TMPLTPSR",9,true);
      WriteString(theEnv,STDERR,"The is-a declaration cannot be used with implied deftemplates.\n");
      DeftemplateData(theEnv)->DeftemplateError = true;
      return false;
     }

   if ((*parent)->named)
     { *nd = true; }

#if CERTAINTY_FACTORS
   if ((*parent)->cfd)
     { *cfd = true; }
#endif

   GetToken(theEnv,readSource,inputToken);
   if (inputToken->tknType != RIGHT_PARENTHESIS_TOKEN)
     {
      PPBackup(theEnv);
      SavePPBuffer(theEnv," ");
      SavePPBuffer(theEnv,inputToken->printForm);
      PrintErrorID(theEnv,"TMPLTPSR",3,true);
      WriteString(theEnv,STDERR,"Expected a ')' to close the is-a declaration.\n");
      DeftemplateData(theEnv)->DeftemplateError = true;
      return false;
     }

   return true;
  }

/**************************/
/* ParseBaseDeftemplates: */
/**************************/
static bool ParseBaseDeftemplates(
  Environment *theEnv,
  const char *readSource,
  struct token *inputToken,
  Deftemplate **parent,
  bool *cfd,
  bool *nd)
  {
   while (inputToken->tknType != RIGHT_PARENTHESIS_TOKEN)
     {
      if (strcmp(inputToken->lexemeValue->contents,TEMPLATE_ND_STRING) == 0)
        {
         if (*nd)
           {
            PrintErrorID(theEnv,"TMPLTPSR",12,true);
            WriteString(theEnv,STDERR,"The ND base deftemplate has already been specified.\n");
            DeftemplateData(theEnv)->DeftemplateError = true;
            return false;
           }
         *nd = true;
        }
#if CERTAINTY_FACTORS
      else if (strcmp(inputToken->lexemeValue->contents,TEMPLATE_CFD_STRING) == 0)
        {
         if (*cfd)
           {
            PrintErrorID(theEnv,"TMPLTPSR",12,true);
            WriteString(theEnv,STDERR,"The CFD base deftemplate has already been specified.\n");
            DeftemplateData(theEnv)->DeftemplateError = true;
            return false;
           }
         *cfd = true;
        }
#endif
      else
        {
         PrintErrorID(theEnv,"TMPLTPSR",13,true);
         WriteString(theEnv,STDERR,"Expected a valid base deftemplate or right parenthesis.\n");
         DeftemplateData(theEnv)->DeftemplateError = true;
         return false;
        }
        
      GetToken(theEnv,readSource,inputToken);
      if (inputToken->tknType != RIGHT_PARENTHESIS_TOKEN)
        {
         PPBackup(theEnv);
         SavePPBuffer(theEnv," ");
         SavePPBuffer(theEnv,inputToken->printForm);
        }
     }

   return true;
  }

/*****************************************************/
/* ParseSlot: Parses a single slot of a deftemplate. */
/*****************************************************/
static struct templateSlot *ParseSlot(
  Environment *theEnv,
  const char *readSource,
  struct token *inputToken,
  struct templateSlot *slotList,
  Deftemplate *parent,
  bool cfd,
  bool nd)
  {
   bool parsingMultislot;
   CLIPSLexeme *slotName;
   struct templateSlot *newSlot;
   ConstraintViolationType rv;
   struct templateSlot *parentSlot = NULL;
   struct templateSlot *tempSlot;

   /*=====================================================*/
   /* Slots must  begin with keyword field or multifield. */
   /*=====================================================*/

   if ((strcmp(inputToken->lexemeValue->contents,"field") != 0) &&
       (strcmp(inputToken->lexemeValue->contents,"multifield") != 0) &&
       (strcmp(inputToken->lexemeValue->contents,"slot") != 0) &&
       (strcmp(inputToken->lexemeValue->contents,"multislot") != 0))
     {
      SyntaxErrorMessage(theEnv,"deftemplate");
      DeftemplateData(theEnv)->DeftemplateError = true;
      return NULL;
     }

   /*===============================================*/
   /* Determine if multifield slot is being parsed. */
   /*===============================================*/

   if ((strcmp(inputToken->lexemeValue->contents,"multifield") == 0) ||
       (strcmp(inputToken->lexemeValue->contents,"multislot") == 0))
     { parsingMultislot = true; }
   else
     { parsingMultislot = false; }

   /*========================================*/
   /* The name of the slot must be a symbol. */
   /*========================================*/

   SavePPBuffer(theEnv," ");
   GetToken(theEnv,readSource,inputToken);
   if (inputToken->tknType != SYMBOL_TOKEN)
     {
      SyntaxErrorMessage(theEnv,"deftemplate");
      DeftemplateData(theEnv)->DeftemplateError = true;
      return NULL;
     }

   slotName = inputToken->lexemeValue;
   
   /*=============================================================*/
   /* The CF slot of a certainty factor fact cannot be redefined. */
   /*=============================================================*/

#if CERTAINTY_FACTORS
   if (cfd && (strcmp(slotName->contents,TEMPLATE_CF_STRING) == 0))
     {
      PrintErrorID(theEnv,"TMPLTPSR",12,true);
      WriteString(theEnv,STDERR,"The '");
      WriteString(theEnv,STDERR,TEMPLATE_CF_STRING);
      WriteString(theEnv,STDERR,"' slot of a certainty factor deftemplate cannot be redefined.\n");
      DeftemplateData(theEnv)->DeftemplateError = true;
      return NULL;
     }
#endif

   /*====================================================*/
   /* The name slot of a named fact cannot be redefined. */
   /*====================================================*/

#if CERTAINTY_FACTORS
   if (nd && (strcmp(slotName->contents,TEMPLATE_NAME_STRING) == 0))
     {
      PrintErrorID(theEnv,"TMPLTPSR",12,true);
      WriteString(theEnv,STDERR,"The '");
      WriteString(theEnv,STDERR,TEMPLATE_NAME_STRING);
      WriteString(theEnv,STDERR,"' slot of a named deftemplate cannot be redefined.\n");
      DeftemplateData(theEnv)->DeftemplateError = true;
      return NULL;
     }
#endif

   /*================================================*/
   /* Determine if the slot has already been parsed. */
   /*================================================*/

   while (slotList != NULL)
     {
      if (slotList->slotName == slotName)
        {
         AlreadyParsedErrorMessage(theEnv,"slot ",slotList->slotName->contents);
         DeftemplateData(theEnv)->DeftemplateError = true;
         return NULL;
        }

      slotList = slotList->next;
     }

   /*====================================*/
   /* Locate the parent slot if present. */
   /*====================================*/
   
   if (parent != NULL)
     {
      for (tempSlot = parent->slotList;
           tempSlot != NULL;
           tempSlot = tempSlot->next)
        {
         if (tempSlot->slotName == slotName)
           {
            parentSlot = tempSlot;
            break;
           }
        }
     }
     
   /*=======================================*/
   /* An inherited slot may not change from */
   /* slot to multislot or vice versa.      */
   /*=======================================*/
   
   if (parentSlot != NULL)
     {
      if (parsingMultislot && (! parentSlot->multislot))
        {
         PrintErrorID(theEnv,"TMPLTPSR",4,true);
         WriteString(theEnv,STDERR,"The inherited slot '");
         WriteString(theEnv,STDERR,slotName->contents);
         WriteString(theEnv,STDERR,"' cannot be changed from slot to multislot.\n");
         DeftemplateData(theEnv)->DeftemplateError = true;
         return NULL;
        }
      else if ((! parsingMultislot) && parentSlot->multislot)
        {
         PrintErrorID(theEnv,"TMPLTPSR",4,true);
         WriteString(theEnv,STDERR,"The inherited slot '");
         WriteString(theEnv,STDERR,slotName->contents);
         WriteString(theEnv,STDERR,"' cannot be changed from multislot to slot.\n");
         DeftemplateData(theEnv)->DeftemplateError = true;
         return NULL;
        }
     }
   
   /*===================================*/
   /* Parse the attributes of the slot. */
   /*===================================*/

   newSlot = DefinedSlots(theEnv,readSource,slotName,parsingMultislot,inputToken,parentSlot);
   if (newSlot == NULL)
     {
      DeftemplateData(theEnv)->DeftemplateError = true;
      return NULL;
     }

   /*=========================================================*/
   /* Inherit unspecified values from the parent deftemplate. */
   /*=========================================================*/

   AssignInheritedAttributes(theEnv,newSlot,parentSlot);

   /*=================================*/
   /* Check for slot conflict errors. */
   /*=================================*/

   if (CheckConstraintParseConflicts(theEnv,newSlot->constraints) == false)
     {
      ReturnSlots(theEnv,newSlot);
      DeftemplateData(theEnv)->DeftemplateError = true;
      return NULL;
     }

   if ((newSlot->defaultPresent) || (newSlot->defaultDynamic))
     { rv = ConstraintCheckExpressionChain(theEnv,newSlot->defaultList,newSlot->constraints); }
   else
     { rv = NO_VIOLATION; }

   if (rv != NO_VIOLATION)
     {
      const char *temp;
      if (newSlot->defaultDynamic) temp = "the default-dynamic attribute";
      else temp = "the default attribute";
      ConstraintViolationErrorMessage(theEnv,"An expression",temp,false,0,
                                      newSlot->slotName,0,rv,newSlot->constraints,true);
      ReturnSlots(theEnv,newSlot);
      DeftemplateData(theEnv)->DeftemplateError = true;
      return NULL;
     }

   /*==================*/
   /* Return the slot. */
   /*==================*/

   return(newSlot);
  }

/***************************************************/
/* CreateCFSlot: Creates the certainty factor slot */
/*   for a certainty factor deftemplate.           */
/***************************************************/
#if CERTAINTY_FACTORS
static struct templateSlot *CreateCFSlot(
  Environment *theEnv)
  {
   struct templateSlot *newSlot;
   
   newSlot = CreateSlot(theEnv);
   newSlot->slotName = CreateSymbol(theEnv,TEMPLATE_CF_STRING);
   newSlot->constraints->anyAllowed = false;
   newSlot->constraints->integersAllowed = true;
   newSlot->defaultPresent = true;
   newSlot->defaultList = GenConstant(theEnv,INTEGER_TYPE,CreateInteger(theEnv,100));
   ReturnExpression(theEnv,newSlot->constraints->minValue);
   newSlot->constraints->minValue = GenConstant(theEnv,INTEGER_TYPE,CreateInteger(theEnv,-100));
   ReturnExpression(theEnv,newSlot->constraints->maxValue);
   newSlot->constraints->maxValue = GenConstant(theEnv,INTEGER_TYPE,CreateInteger(theEnv,100));

   return newSlot;
  }
#endif

/*****************************************/
/* CreateNameSlot: Creates the name slot */
/*   for named deftemplates.             */
/*****************************************/
static struct templateSlot *CreateNameSlot(
  Environment *theEnv)
  {
   struct templateSlot *newSlot;
   
   newSlot = CreateSlot(theEnv);
   newSlot->slotName = CreateSymbol(theEnv,TEMPLATE_NAME_STRING);
   newSlot->constraints->anyAllowed = false;
   newSlot->constraints->symbolsAllowed = true;
   newSlot->defaultPresent = true;
   newSlot->defaultList = GenConstant(theEnv,FCALL,FindFunction(theEnv,"(genfactname)"));

   return newSlot;
  }

/**************************************************************/
/* DefinedSlots: Parses a field or multifield slot attribute. */
/**************************************************************/
static struct templateSlot *DefinedSlots(
  Environment *theEnv,
  const char *readSource,
  CLIPSLexeme *slotName,
  bool multifieldSlot,
  struct token *inputToken,
  struct templateSlot *parentSlot)
  {
   struct templateSlot *newSlot;
   struct expr *defaultList;
   bool defaultFound = false, patternMatchFound = false;
   bool reactive = true;
   bool noneSpecified, deriveSpecified;
   CONSTRAINT_PARSE_RECORD parsedConstraints;
   const char *constraintName;

   /*===========================*/
   /* Build the slot container. */
   /*===========================*/

   newSlot = CreateSlot(theEnv);
   newSlot->slotName = slotName;
   newSlot->constraints->multifieldsAllowed = multifieldSlot;
   newSlot->multislot = multifieldSlot;

   /*==================================*/
   /* Inherit the parent's reactivity. */
   /*==================================*/
   
   if (parentSlot != NULL)
     { newSlot->reactive = parentSlot->reactive; }

   /*========================================*/
   /* Parse the primitive slot if it exists. */
   /*========================================*/

   InitializeConstraintParseRecord(&parsedConstraints);
   GetToken(theEnv,readSource,inputToken);

   while (inputToken->tknType != RIGHT_PARENTHESIS_TOKEN)
     {
      PPBackup(theEnv);
      SavePPBuffer(theEnv," ");
      SavePPBuffer(theEnv,inputToken->printForm);

      /*================================================*/
      /* Slot attributes begin with a left parenthesis. */
      /*================================================*/

      if (inputToken->tknType != LEFT_PARENTHESIS_TOKEN)
        {
         SyntaxErrorMessage(theEnv,"deftemplate");
         ReturnSlots(theEnv,newSlot);
         DeftemplateData(theEnv)->DeftemplateError = true;
         return NULL;
        }

      /*=============================================*/
      /* The name of the attribute must be a symbol. */
      /*=============================================*/

      GetToken(theEnv,readSource,inputToken);
      if (inputToken->tknType != SYMBOL_TOKEN)
        {
         SyntaxErrorMessage(theEnv,"deftemplate");
         ReturnSlots(theEnv,newSlot);
         DeftemplateData(theEnv)->DeftemplateError = true;
         return NULL;
        }
      constraintName = inputToken->lexemeValue->contents;

      /*================================================================*/
      /* Determine if the attribute is one of the standard constraints. */
      /*================================================================*/

      if (StandardConstraint(constraintName))
        {
         if (ParseStandardConstraint(theEnv,readSource,(constraintName),
                                     newSlot->constraints,&parsedConstraints,
                                     multifieldSlot) == false)
           {
            DeftemplateData(theEnv)->DeftemplateError = true;
            ReturnSlots(theEnv,newSlot);
            return NULL;
           }
        }

      /*=================================================*/
      /* else if the attribute is the default attribute, */
      /* then get the default list for this slot.        */
      /*=================================================*/

      else if ((strcmp(constraintName,"default") == 0) ||
               (strcmp(constraintName,"default-dynamic") == 0))
        {
         /*======================================================*/
         /* Check to see if the default has already been parsed. */
         /*======================================================*/

         if (defaultFound)
           {
            AlreadyParsedErrorMessage(theEnv,"default attribute",NULL);
            DeftemplateData(theEnv)->DeftemplateError = true;
            ReturnSlots(theEnv,newSlot);
            return NULL;
           }

         newSlot->noDefault = false;

         /*=====================================================*/
         /* Determine whether the default is dynamic or static. */
         /*=====================================================*/

         if (strcmp(constraintName,"default") == 0)
           {
            newSlot->defaultPresent = true;
            newSlot->defaultDynamic = false;
           }
         else
           {
            newSlot->defaultPresent = false;
            newSlot->defaultDynamic = true;
           }

         /*===================================*/
         /* Parse the list of default values. */
         /*===================================*/

         defaultList = ParseDefault(theEnv,readSource,multifieldSlot,newSlot->defaultDynamic,
                                  true,&noneSpecified,&deriveSpecified,&DeftemplateData(theEnv)->DeftemplateError);
         if (DeftemplateData(theEnv)->DeftemplateError == true)
           {
            ReturnSlots(theEnv,newSlot);
            return NULL;
           }

         /*==================================*/
         /* Store the default with the slot. */
         /*==================================*/

         defaultFound = true;
         if (deriveSpecified) newSlot->defaultPresent = false;
         else if (noneSpecified)
           {
            newSlot->noDefault = true;
            newSlot->defaultPresent = false;
           }
         newSlot->defaultList = defaultList;
        }

      /*===============================================*/
      /* else if the attribute is the facet attribute. */
      /*===============================================*/

      else if (strcmp(constraintName,"facet") == 0)
        {
         if (! ParseFacetAttribute(theEnv,readSource,newSlot,false))
           {
            ReturnSlots(theEnv,newSlot);
            DeftemplateData(theEnv)->DeftemplateError = true;
            return NULL;
           }
        }

      else if (strcmp(constraintName,"multifacet") == 0)
        {
         if (! ParseFacetAttribute(theEnv,readSource,newSlot,true))
           {
            ReturnSlots(theEnv,newSlot);
            DeftemplateData(theEnv)->DeftemplateError = true;
            return NULL;
           }
        }

      /*=======================================================*/
      /* else if the attribute is the pattern-match attribute. */
      /*=======================================================*/

      else if (strcmp(constraintName,"pattern-match") == 0)
        {
         /*====================================*/
         /* Check to see if the pattern-match  */
         /* attribute has already been parsed. */
         /*====================================*/

         if (patternMatchFound)
           {
            AlreadyParsedErrorMessage(theEnv,"pattern-match attribute",NULL);
            DeftemplateData(theEnv)->DeftemplateError = true;
            ReturnSlots(theEnv,newSlot);
            return NULL;
           }
           
         if (! ParseSimpleQualifier(theEnv,readSource,"pattern-match","non-reactive","reactive",&reactive))
           {
            DeftemplateData(theEnv)->DeftemplateError = true;
            ReturnSlots(theEnv,newSlot);
            return NULL;
           }
           
         newSlot->reactive = reactive;
         patternMatchFound = true;
        }
        
      /*============================================*/
      /* Otherwise the attribute is an invalid one. */
      /*============================================*/

      else
        {
         SyntaxErrorMessage(theEnv,"slot attributes");
         ReturnSlots(theEnv,newSlot);
         DeftemplateData(theEnv)->DeftemplateError = true;
         return NULL;
        }

      /*==============================================*/
      /* The cardinality slot cannot be less specific */
      /* than the parent cardinality slot.            */
      /*==============================================*/
      
      if ((parentSlot != NULL) && (strcmp(constraintName,"cardinality") == 0))
        {
         if (parentSlot->constraints->minFields->value != SymbolData(theEnv)->Zero)
           {
            if (newSlot->constraints->minFields->integerValue->contents <
                parentSlot->constraints->minFields->integerValue->contents)
              {
               PrintErrorID(theEnv,"TMPLTPSR",5,true);
               WriteString(theEnv,STDERR,"The minimum cardinality for the slot '");
               WriteString(theEnv,STDERR,slotName->contents);
               WriteString(theEnv,STDERR,"' cannot be less than the inherited minimum cardinality.\n");
               ReturnSlots(theEnv,newSlot);
               DeftemplateData(theEnv)->DeftemplateError = true;
               return NULL;
              }
           }

         if (parentSlot->constraints->maxFields->value != SymbolData(theEnv)->PositiveInfinity)
           {
            if ((newSlot->constraints->maxFields->value == SymbolData(theEnv)->PositiveInfinity) || ((newSlot->constraints->maxFields->integerValue->contents >
                  parentSlot->constraints->maxFields->integerValue->contents)))
              {
               PrintErrorID(theEnv,"TMPLTPSR",5,true);
               WriteString(theEnv,STDERR,"The maximum cardinality for the slot '");
               WriteString(theEnv,STDERR,slotName->contents);
               WriteString(theEnv,STDERR,"' cannot be more than the inherited maximum cardinality.\n");
               ReturnSlots(theEnv,newSlot);
               DeftemplateData(theEnv)->DeftemplateError = true;
               return NULL;
              }
           }
        }
        
      /*========================================*/
      /* The range slot cannot be less specific */
      /* than the parent range slot.            */
      /*========================================*/
      
      if ((parentSlot != NULL) && (strcmp(constraintName,"range") == 0))
        {
         if (parentSlot->constraints->minValue->value != SymbolData(theEnv)->NegativeInfinity)
           {
            if ((newSlot->constraints->minValue->value == SymbolData(theEnv)->NegativeInfinity) || ((newSlot->constraints->minValue->integerValue->contents <
                  parentSlot->constraints->minValue->integerValue->contents)))
              {
               PrintErrorID(theEnv,"TMPLTPSR",6,true);
               WriteString(theEnv,STDERR,"The minimum range for the slot '");
               WriteString(theEnv,STDERR,slotName->contents);
               WriteString(theEnv,STDERR,"' cannot be less than the inherited minimum range.\n");
               ReturnSlots(theEnv,newSlot);
               DeftemplateData(theEnv)->DeftemplateError = true;
               return NULL;
              }
           }

         if (parentSlot->constraints->maxValue->value != SymbolData(theEnv)->PositiveInfinity)
           {
            if ((newSlot->constraints->maxValue->value == SymbolData(theEnv)->PositiveInfinity) || ((newSlot->constraints->maxValue->integerValue->contents >
                  parentSlot->constraints->maxValue->integerValue->contents)))
              {
               PrintErrorID(theEnv,"TMPLTPSR",6,true);
               WriteString(theEnv,STDERR,"The maximum range for the slot '");
               WriteString(theEnv,STDERR,slotName->contents);
               WriteString(theEnv,STDERR,"' cannot be more than the inherited maximum range.\n");
               ReturnSlots(theEnv,newSlot);
               DeftemplateData(theEnv)->DeftemplateError = true;
               return NULL;
              }
           }
        }
        
      /*=======================================*/
      /* The type slot cannot be less specific */
      /* than the parent type slot.            */
      /*=======================================*/
      
      if ((parentSlot != NULL) &&
          (! parentSlot->constraints->anyAllowed) &&
          (strcmp(constraintName,"type") == 0))
        {
         if ((newSlot->constraints->anyAllowed) ||
             (newSlot->constraints->symbolsAllowed && (! parentSlot->constraints->symbolsAllowed)) ||
             (newSlot->constraints->stringsAllowed && (! parentSlot->constraints->stringsAllowed)) ||
             (newSlot->constraints->floatsAllowed && (! parentSlot->constraints->floatsAllowed)) ||
             (newSlot->constraints->integersAllowed && (! parentSlot->constraints->integersAllowed)) ||
             (newSlot->constraints->instanceNamesAllowed && (! parentSlot->constraints->instanceNamesAllowed)) ||
             (newSlot->constraints->instanceAddressesAllowed && (! parentSlot->constraints->instanceAddressesAllowed)) ||
             (newSlot->constraints->externalAddressesAllowed && (! parentSlot->constraints->externalAddressesAllowed)) ||
             (newSlot->constraints->factAddressesAllowed && (! parentSlot->constraints->factAddressesAllowed)) ||
             (newSlot->constraints->voidAllowed && (! parentSlot->constraints->voidAllowed)))
           {
            PrintErrorID(theEnv,"TMPLTPSR",7,true);
            WriteString(theEnv,STDERR,"The allowed types for the slot '");
            WriteString(theEnv,STDERR,slotName->contents);
            WriteString(theEnv,STDERR,"' can only include inherited allowed types.\n");
            ReturnSlots(theEnv,newSlot);
            DeftemplateData(theEnv)->DeftemplateError = true;
            return NULL;
           }
        }

      /*==============================================*/
      /* The allowed-... slot cannot be less specific */
      /* than the parent allowed-... slot.            */
      /*==============================================*/

      if ((parentSlot != NULL) &&
          parentSlot->constraints->anyRestriction &&
          (strcmp(constraintName,"allowed-classes") == 0))
        {
         NoConjunctiveUseError(theEnv,"allowed-classes","inherited allowed-values");
         ReturnSlots(theEnv,newSlot);
         DeftemplateData(theEnv)->DeftemplateError = true;
         return NULL;
        }

      if ((parentSlot != NULL) &&
          parentSlot->constraints->classRestriction &&
          (strcmp(constraintName,"allowed-values") == 0))
        {
         NoConjunctiveUseError(theEnv,"allowed-values","inherited allowed-classes");
         ReturnSlots(theEnv,newSlot);
         DeftemplateData(theEnv)->DeftemplateError = true;
         return NULL;
        }

      if ((parentSlot != NULL) &&
          (parentSlot->constraints->symbolRestriction ||
           parentSlot->constraints->anyRestriction) &&
          (strcmp(constraintName,"allowed-symbols") == 0))
        {
         if (InheritedAllowedCheck(theEnv,slotName->contents,parentSlot,newSlot,"allowed-symbols",SYMBOL_BIT))
           { return NULL; }
        }

      if ((parentSlot != NULL) &&
          (parentSlot->constraints->stringRestriction ||
           parentSlot->constraints->anyRestriction) &&
          (strcmp(constraintName,"allowed-strings") == 0))
        {
         if (InheritedAllowedCheck(theEnv,slotName->contents,parentSlot,newSlot,"allowed-strings",STRING_BIT))
           { return NULL; }
        }

      if ((parentSlot != NULL) &&
          (parentSlot->constraints->instanceNameRestriction ||
           parentSlot->constraints->anyRestriction) &&
          (strcmp(constraintName,"allowed-instance-names") == 0))
        {
         if (InheritedAllowedCheck(theEnv,slotName->contents,parentSlot,newSlot,"allowed-instance-names",INSTANCE_NAME_BIT))
           { return NULL; }
        }

      if ((parentSlot != NULL) &&
          (parentSlot->constraints->symbolRestriction ||
           parentSlot->constraints->stringRestriction ||
           parentSlot->constraints->anyRestriction) &&
          (strcmp(constraintName,"allowed-lexemes") == 0))
        {
         if ((parentSlot->constraints->anyRestriction ||
             (parentSlot->constraints->symbolRestriction && parentSlot->constraints->stringRestriction)))
           {
            if (InheritedAllowedCheck(theEnv,slotName->contents,parentSlot,newSlot,"allowed-lexemes",SYMBOL_BIT | STRING_BIT))
              { return NULL; }
           }
         else if (parentSlot->constraints->symbolRestriction)
           {
            if (InheritedAllowedCheck(theEnv,slotName->contents,parentSlot,newSlot,"allowed-lexemes",SYMBOL_BIT))
              { return NULL; }
           }
         else if (parentSlot->constraints->stringRestriction)
           {
            if (InheritedAllowedCheck(theEnv,slotName->contents,parentSlot,newSlot,"allowed-lexemes",STRING_BIT))
              { return NULL; }
           }
        }

      if ((parentSlot != NULL) &&
          (parentSlot->constraints->floatRestriction ||
           parentSlot->constraints->anyRestriction) &&
          (strcmp(constraintName,"allowed-floats") == 0))
        {
         if (InheritedAllowedCheck(theEnv,slotName->contents,parentSlot,newSlot,"allowed-floats",FLOAT_BIT))
           { return NULL; }
        }

      if ((parentSlot != NULL) &&
          (parentSlot->constraints->integerRestriction ||
           parentSlot->constraints->anyRestriction) &&
          (strcmp(constraintName,"allowed-integers") == 0))
        {
         if (InheritedAllowedCheck(theEnv,slotName->contents,parentSlot,newSlot,"allowed-integers",INTEGER_BIT))
           { return NULL; }
        }

      if ((parentSlot != NULL) &&
          (parentSlot->constraints->floatRestriction ||
           parentSlot->constraints->integerRestriction ||
           parentSlot->constraints->anyRestriction) &&
          (strcmp(constraintName,"allowed-numbers") == 0))
        {
         if ((parentSlot->constraints->anyRestriction ||
             (parentSlot->constraints->floatRestriction && parentSlot->constraints->integerRestriction)))
           {
            if (InheritedAllowedCheck(theEnv,slotName->contents,parentSlot,newSlot,"allowed-numbers",INTEGER_BIT | FLOAT_BIT))
              { return NULL; }
           }
         else if (parentSlot->constraints->floatRestriction)
           {
            if (InheritedAllowedCheck(theEnv,slotName->contents,parentSlot,newSlot,"allowed-numbers",FLOAT_BIT))
              { return NULL; }
           }
         else if (parentSlot->constraints->integerRestriction)
           {
            if (InheritedAllowedCheck(theEnv,slotName->contents,parentSlot,newSlot,"allowed-numbers",INTEGER_BIT))
              { return NULL; }
           }
        }

      if ((parentSlot != NULL) &&
          (parentSlot->constraints->floatRestriction ||
           parentSlot->constraints->integerRestriction ||
           parentSlot->constraints->symbolRestriction ||
           parentSlot->constraints->stringRestriction ||
           parentSlot->constraints->instanceNameRestriction ||
           parentSlot->constraints->anyRestriction) &&
          (strcmp(constraintName,"allowed-values") == 0))
        {
         if ((parentSlot->constraints->anyRestriction ||
             (parentSlot->constraints->floatRestriction && parentSlot->constraints->integerRestriction &&
              parentSlot->constraints->symbolRestriction && parentSlot->constraints->stringRestriction &&
              parentSlot->constraints->instanceNameRestriction)))
           {
            if (InheritedAllowedCheck(theEnv,slotName->contents,parentSlot,newSlot,"allowed-values",
                                      INTEGER_BIT | FLOAT_BIT | SYMBOL_BIT | STRING_BIT | INSTANCE_NAME_BIT))
              { return NULL; }
           }
         else
           {
            if (parentSlot->constraints->floatRestriction)
              {
               if (InheritedAllowedCheck(theEnv,slotName->contents,parentSlot,newSlot,"allowed-values",FLOAT_BIT))
                 { return NULL; }
              }
            if (parentSlot->constraints->integerRestriction)
              {
               if (InheritedAllowedCheck(theEnv,slotName->contents,parentSlot,newSlot,"allowed-values",INTEGER_BIT))
                 { return NULL; }
              }
            if (parentSlot->constraints->symbolRestriction)
              {
               if (InheritedAllowedCheck(theEnv,slotName->contents,parentSlot,newSlot,"allowed-values",SYMBOL_BIT))
                 { return NULL; }
              }
            if (parentSlot->constraints->stringRestriction)
              {
               if (InheritedAllowedCheck(theEnv,slotName->contents,parentSlot,newSlot,"allowed-values",STRING_BIT))
                 { return NULL; }
              }
            if (parentSlot->constraints->instanceNameRestriction)
              {
               if (InheritedAllowedCheck(theEnv,slotName->contents,parentSlot,newSlot,"allowed-values",INSTANCE_NAME_BIT))
                 { return NULL; }
              }
           }
        }
        
      if ((parentSlot != NULL) &&
           parentSlot->constraints->classRestriction &&
          (strcmp(constraintName,"allowed-classes") == 0))
        {
         if (InheritedAllowedClassesCheck(theEnv,slotName->contents,parentSlot,newSlot,"allowed-classes"))
           { return NULL; }
        }

      /*===================================*/
      /* Begin parsing the next attribute. */
      /*===================================*/

      GetToken(theEnv,readSource,inputToken);
     }

   /*============================*/
   /* Return the attribute list. */
   /*============================*/

   return newSlot;
  }

/****************************/
/* InheritedAllowedCheck:   */
/****************************/
static bool InheritedAllowedCheck(
  Environment *theEnv,
  const char *slotName,
  struct templateSlot *parentSlot,
  struct templateSlot *newSlot,
  const char *constraint,
  unsigned expectedType)
  {
   struct expr *theValue, *theParentValue;
         
   for (theValue = newSlot->constraints->restrictionList;
        theValue != NULL;
        theValue = theValue->nextArg)
     {
      if (! ValueIsType(theValue->value,expectedType))
        { continue; }
     
      for (theParentValue = parentSlot->constraints->restrictionList;
           theParentValue != NULL;
           theParentValue = theParentValue->nextArg)
        {
         if (theValue->value == theParentValue->value)
           { break; }
        }
              
      if (theParentValue == NULL)
        {
         PrintErrorID(theEnv,"TMPLTPSR",8,true);
         WriteString(theEnv,STDERR,"The value ");
         PrintAtom(theEnv,STDERR,theValue->type,theValue->value);
         WriteString(theEnv,STDERR," in the ");
         WriteString(theEnv,STDERR,constraint);
         WriteString(theEnv,STDERR," for the slot '");
         WriteString(theEnv,STDERR,slotName);
         WriteString(theEnv,STDERR,"' is not in the list of inherited ");
         WriteString(theEnv,STDERR,constraint);
         WriteString(theEnv,STDERR,".\n");
         ReturnSlots(theEnv,newSlot);
         DeftemplateData(theEnv)->DeftemplateError = true;
         return true;
        }
     }
     
   return false;
  }
  
/*********************************/
/* InheritedAllowedClassesCheck: */
/*********************************/
static bool InheritedAllowedClassesCheck(
  Environment *theEnv,
  const char *slotName,
  struct templateSlot *parentSlot,
  struct templateSlot *newSlot,
  const char *constraint)
  {
   struct expr *theValue, *theParentValue;
         
   for (theValue = newSlot->constraints->classList;
        theValue != NULL;
        theValue = theValue->nextArg)
     {
      for (theParentValue = parentSlot->constraints->classList;
           theParentValue != NULL;
           theParentValue = theParentValue->nextArg)
        {
         if (theValue->value == theParentValue->value)
           { break; }
        }
              
      if (theParentValue == NULL)
        {
         PrintErrorID(theEnv,"TMPLTPSR",8,true);
         WriteString(theEnv,STDERR,"The class ");
         PrintAtom(theEnv,STDERR,theValue->type,theValue->value);
         WriteString(theEnv,STDERR," in the ");
         WriteString(theEnv,STDERR,constraint);
         WriteString(theEnv,STDERR," for the slot '");
         WriteString(theEnv,STDERR,slotName);
         WriteString(theEnv,STDERR,"' is not in the list of inherited ");
         WriteString(theEnv,STDERR,constraint);
         WriteString(theEnv,STDERR,".\n");
         ReturnSlots(theEnv,newSlot);
         DeftemplateData(theEnv)->DeftemplateError = true;
         return true;
        }
     }
     
   return false;
  }
  
/***************************************************/
/* ParseFacetAttribute: Parses the type attribute. */
/***************************************************/
static bool ParseFacetAttribute(
  Environment *theEnv,
  const char *readSource,
  struct templateSlot *theSlot,
  bool multifacet)
  {
   struct token inputToken;
   CLIPSLexeme *facetName;
   struct expr *facetPair, *tempFacet, *facetValue = NULL, *lastValue = NULL;

   /*==============================*/
   /* Parse the name of the facet. */
   /*==============================*/

   SavePPBuffer(theEnv," ");
   GetToken(theEnv,readSource,&inputToken);

   /*==================================*/
   /* The facet name must be a symbol. */
   /*==================================*/

   if (inputToken.tknType != SYMBOL_TOKEN)
     {
      if (multifacet) SyntaxErrorMessage(theEnv,"multifacet attribute");
      else SyntaxErrorMessage(theEnv,"facet attribute");
      return false;
     }

   facetName = inputToken.lexemeValue;

   /*===================================*/
   /* Don't allow facets with the same  */
   /* name as a predefined CLIPS facet. */
   /*===================================*/

   /*====================================*/
   /* Has the facet already been parsed? */
   /*====================================*/

   for (tempFacet = theSlot->facetList;
        tempFacet != NULL;
        tempFacet = tempFacet->nextArg)
     {
      if (tempFacet->value == facetName)
        {
         if (multifacet) AlreadyParsedErrorMessage(theEnv,"multifacet ",facetName->contents);
         else AlreadyParsedErrorMessage(theEnv,"facet ",facetName->contents);
         return false;
        }
     }

   /*===============================*/
   /* Parse the value of the facet. */
   /*===============================*/

   SavePPBuffer(theEnv," ");
   GetToken(theEnv,readSource,&inputToken);

   while (inputToken.tknType != RIGHT_PARENTHESIS_TOKEN)
     {
      /*=====================================*/
      /* The facet value must be a constant. */
      /*=====================================*/

      if (! ConstantType(TokenTypeToType(inputToken.tknType)))
        {
         if (multifacet) SyntaxErrorMessage(theEnv,"multifacet attribute");
         else SyntaxErrorMessage(theEnv,"facet attribute");
         ReturnExpression(theEnv,facetValue);
         return false;
        }

      /*======================================*/
      /* Add the value to the list of values. */
      /*======================================*/

      if (lastValue == NULL)
        {
         facetValue = GenConstant(theEnv,TokenTypeToType(inputToken.tknType),inputToken.value);
         lastValue = facetValue;
        }
      else
        {
         lastValue->nextArg = GenConstant(theEnv,TokenTypeToType(inputToken.tknType),inputToken.value);
         lastValue = lastValue->nextArg;
        }

      /*=====================*/
      /* Get the next token. */
      /*=====================*/

      SavePPBuffer(theEnv," ");
      GetToken(theEnv,readSource,&inputToken);

      /*===============================================*/
      /* A facet can't contain more than one constant. */
      /*===============================================*/

      if ((! multifacet) && (inputToken.tknType != RIGHT_PARENTHESIS_TOKEN))
        {
         SyntaxErrorMessage(theEnv,"facet attribute");
         ReturnExpression(theEnv,facetValue);
         return false;
        }
     }

   /*========================================================*/
   /* Remove the space before the closing right parenthesis. */
   /*========================================================*/

   PPBackup(theEnv);
   PPBackup(theEnv);
   SavePPBuffer(theEnv,")");

   /*====================================*/
   /* A facet must contain one constant. */
   /*====================================*/

   if ((! multifacet) && (facetValue == NULL))
     {
      SyntaxErrorMessage(theEnv,"facet attribute");
      return false;
     }

   /*=================================================*/
   /* Add the facet to the list of the slot's facets. */
   /*=================================================*/

   facetPair = GenConstant(theEnv,SYMBOL_TYPE,facetName);

   if (multifacet)
     {
      facetPair->argList = GenConstant(theEnv,FCALL,FindFunction(theEnv,"create$"));
      facetPair->argList->argList = facetValue;
     }
   else
     { facetPair->argList = facetValue; }

   facetPair->nextArg = theSlot->facetList;
   theSlot->facetList = facetPair;

   /*===============================================*/
   /* The facet/multifacet was successfully parsed. */
   /*===============================================*/

   return true;
  }
  
/*************************************************************/
/* ParseSimpleQualifier: Parses the pattern-match attribute. */
/*************************************************************/
static bool ParseSimpleQualifier(
  Environment *theEnv,
  const char *readSource,
  const char *qualifier,
  const char *clearRelation,
  const char *setRelation,
  bool *binaryFlag)
  {
   struct token inputToken;

   /*=================================*/
   /* The qualifier must be a symbol. */
   /*=================================*/
   
   SavePPBuffer(theEnv," ");
   GetToken(theEnv,readSource,&inputToken);
   
   if (inputToken.tknType != SYMBOL_TOKEN)
     {
      PrintErrorID(theEnv,"TMPLTPSR",10,true);
      WriteString(theEnv,STDERR,"Allowed values for the ");
      WriteString(theEnv,STDERR,qualifier);
      WriteString(theEnv,STDERR," attribute are ");
      WriteString(theEnv,STDERR,clearRelation);
      WriteString(theEnv,STDERR," and ");
      WriteString(theEnv,STDERR,setRelation);
      WriteString(theEnv,STDERR,".\n");
      return false;
     }

   /*========================================================*/
   /* Check that the qualifier is one of the allowed values. */
   /*========================================================*/
   
   if (strcmp(inputToken.lexemeValue->contents,setRelation) == 0)
     { *binaryFlag = true; }
   else if (strcmp(inputToken.lexemeValue->contents,clearRelation) == 0)
     { *binaryFlag = false; }
   else
     {
      PrintErrorID(theEnv,"TMPLTPSR",10,true);
      WriteString(theEnv,STDERR,"Allowed values for the ");
      WriteString(theEnv,STDERR,qualifier);
      WriteString(theEnv,STDERR," attribute are ");
      WriteString(theEnv,STDERR,clearRelation);
      WriteString(theEnv,STDERR," and ");
      WriteString(theEnv,STDERR,setRelation);
      WriteString(theEnv,STDERR,".\n");
      return false;
     }
      
   /*=============================================================*/
   /* The attribute qualifier is closed with a right parenthesis. */
   /*=============================================================*/
   
   GetToken(theEnv,readSource,&inputToken);
   if (inputToken.tknType != RIGHT_PARENTHESIS_TOKEN)
     {
      PPBackup(theEnv);
      SavePPBuffer(theEnv," ");
      SavePPBuffer(theEnv,inputToken.printForm);
      
      PrintErrorID(theEnv,"TMPLTPSR",11,true);
      WriteString(theEnv,STDERR,"Expected a ')' to close the ");
      WriteString(theEnv,STDERR,qualifier);
      WriteString(theEnv,STDERR," attribute.\n");
      return false;
     }
     
   return true;
  }

/******************************/
/* AssignInheritedAttributes: */
/******************************/
static void AssignInheritedAttributes(
  Environment *theEnv,
  struct templateSlot *newSlot,
  struct templateSlot *parentSlot)
  {
   //unsigned restrictedType = 0;
   struct expr *last, *temp;
   unsigned int parentRestrictionTypes = 0;
   unsigned int childRestrictionTypes = 0;

   if (parentSlot == NULL) return;

   /*=============*/
   /* Cardinality */
   /*=============*/
   
   if ((newSlot->constraints->minFields->value == SymbolData(theEnv)->Zero) &&
       (parentSlot->constraints->minFields->value != SymbolData(theEnv)->Zero))
     {
      ReturnExpression(theEnv,newSlot->constraints->minFields);
      newSlot->constraints->minFields = GenConstant(theEnv,parentSlot->constraints->minFields->type,
                                                           parentSlot->constraints->minFields->value);
     }

   if ((newSlot->constraints->maxFields->value == SymbolData(theEnv)->PositiveInfinity) &&
       (parentSlot->constraints->maxFields->value != SymbolData(theEnv)->PositiveInfinity))
     {
      ReturnExpression(theEnv,newSlot->constraints->maxFields);
      newSlot->constraints->maxFields = GenConstant(theEnv,parentSlot->constraints->maxFields->type,
                                                           parentSlot->constraints->maxFields->value);
     }
     
   /*=======*/
   /* Range */
   /*=======*/
   
   if ((newSlot->constraints->minValue->value == SymbolData(theEnv)->NegativeInfinity) &&
       (parentSlot->constraints->minValue->value != SymbolData(theEnv)->NegativeInfinity))
     {
      ReturnExpression(theEnv,newSlot->constraints->minValue);
      newSlot->constraints->minValue = GenConstant(theEnv,parentSlot->constraints->minValue->type,
                                                          parentSlot->constraints->minValue->value);
     }

   if ((newSlot->constraints->maxValue->value == SymbolData(theEnv)->PositiveInfinity) &&
       (parentSlot->constraints->maxValue->value != SymbolData(theEnv)->PositiveInfinity))
     {
      ReturnExpression(theEnv,newSlot->constraints->maxValue);
      newSlot->constraints->maxValue = GenConstant(theEnv,parentSlot->constraints->maxValue->type,
                                                          parentSlot->constraints->maxValue->value);
     }

   /*=========*/
   /* Default */
   /*=========*/
   
   if ((newSlot->defaultList == NULL) &&
       (newSlot->noDefault == false))
     {
      newSlot->noDefault = parentSlot->noDefault;
      newSlot->defaultPresent =  parentSlot->defaultPresent;
      newSlot->defaultDynamic = parentSlot->defaultDynamic;
      newSlot->defaultList = CopyExpression(theEnv,parentSlot->defaultList);
     }

   /*======*/
   /* Type */
   /*======*/
   
   if ((newSlot->constraints->anyAllowed) && (! parentSlot->constraints->anyAllowed))
     {
      newSlot->constraints->anyAllowed = false;
      newSlot->constraints->symbolsAllowed =  parentSlot->constraints->symbolsAllowed;
      newSlot->constraints->stringsAllowed = parentSlot->constraints->stringsAllowed;
      newSlot->constraints->floatsAllowed = parentSlot->constraints->floatsAllowed;
      newSlot->constraints->integersAllowed = parentSlot->constraints->integersAllowed;
      newSlot->constraints->instanceNamesAllowed = parentSlot->constraints->instanceNamesAllowed;
      newSlot->constraints->instanceAddressesAllowed = parentSlot->constraints->instanceAddressesAllowed;
      newSlot->constraints->externalAddressesAllowed = parentSlot->constraints->externalAddressesAllowed;
      newSlot->constraints->factAddressesAllowed = parentSlot->constraints->factAddressesAllowed;
      newSlot->constraints->voidAllowed = parentSlot->constraints->voidAllowed;
     }
   
   /* class list */
   /*    unsigned int classRestriction : 1; */
   /*=============*/
   /* Allowed-... */
   /*=============*/
   
   /*=====================================================*/
   /* Determine which types contribute to tbe restriction */
   /* list for the parent and child deftemplates.         */
   /*=====================================================*/
   
   if (newSlot->constraints->anyRestriction)
     { childRestrictionTypes = SYMBOL_BIT | STRING_BIT | FLOAT_BIT | INTEGER_BIT | INSTANCE_NAME_BIT; }
   else
     {
      childRestrictionTypes |= (newSlot->constraints->symbolRestriction ? SYMBOL_BIT : 0) |
                               (newSlot->constraints->stringRestriction ? STRING_BIT : 0) |
                               (newSlot->constraints->floatRestriction ? FLOAT_BIT : 0) |
                               (newSlot->constraints->integerRestriction ? INTEGER_BIT : 0) |
                               (newSlot->constraints->instanceNameRestriction ? INSTANCE_NAME_BIT : 0);
     }

   if (parentSlot->constraints->anyRestriction)
     { parentRestrictionTypes = SYMBOL_BIT | STRING_BIT | FLOAT_BIT | INTEGER_BIT | INSTANCE_NAME_BIT; }
   else
     {
      parentRestrictionTypes |= (parentSlot->constraints->symbolRestriction ? SYMBOL_BIT : 0) |
                                (parentSlot->constraints->stringRestriction ? STRING_BIT : 0) |
                                (parentSlot->constraints->floatRestriction ? FLOAT_BIT : 0) |
                                (parentSlot->constraints->integerRestriction ? INTEGER_BIT : 0) |
                                (parentSlot->constraints->instanceNameRestriction ? INSTANCE_NAME_BIT : 0);
     }

   /*==============================================*/
   /* Find the last value in the restriction list. */
   /*==============================================*/
      
   last = newSlot->constraints->restrictionList;
   while ((last != NULL) && (last->nextArg != NULL))
     { last = last->nextArg; }

   /*==================================================*/
   /* Check each value in the parent restriction list. */
   /*==================================================*/
      
   for (temp = parentSlot->constraints->restrictionList;
        temp != NULL;
        temp = temp->nextArg)
     {
      /*========================================================*/
      /* If the value type was not restricted in the parent, or */
      /* was restricted in the child, then the value does not   */
      /* need to be added to the child restriction list.        */
      /*========================================================*/
      
      if ((! ValueIsType(temp->value,parentRestrictionTypes)) ||
          ValueIsType(temp->value,childRestrictionTypes))
        { continue; }
           
      /*==============================================*/
      /* Add the value to the child restriction list. */
      /*==============================================*/
      
      if (last == NULL)
        {
         last = GenConstant(theEnv,temp->type,temp->value);
         newSlot->constraints->restrictionList = last;
        }
      else
        {
         last->nextArg = GenConstant(theEnv,temp->type,temp->value);
         last = last->nextArg;
        }
     }
     
    /*=================================================*/
    /* Update the child restriction types based on the */
    /* inherited restrictions from the parent and the  */
    /* restrictions specified in the child.            */
    /*=================================================*/
    
    if (! newSlot->constraints->anyRestriction)
      {
       if (parentSlot->constraints->anyRestriction)
         {
          newSlot->constraints->anyRestriction = true;
          newSlot->constraints->symbolRestriction = false;
          newSlot->constraints->stringRestriction = false;
          newSlot->constraints->floatRestriction = false;
          newSlot->constraints->integerRestriction = false;
          newSlot->constraints->instanceNameRestriction = false;
         }
       else
         {
          if (parentSlot->constraints->symbolRestriction)
            { newSlot->constraints->symbolRestriction = true; }
         
          if (parentSlot->constraints->stringRestriction)
            { newSlot->constraints->stringRestriction = true; }
       
          if (parentSlot->constraints->floatRestriction)
            { newSlot->constraints->floatRestriction = true; }
       
          if (parentSlot->constraints->integerRestriction)
            { newSlot->constraints->integerRestriction = true; }
       
          if (parentSlot->constraints->instanceNameRestriction)
            { newSlot->constraints->instanceNameRestriction = true; }
         }
      }
  }
  
/**************/
/* CreateSlot: */
/**************/
static struct templateSlot *CreateSlot(
  Environment *theEnv)
  {
   struct templateSlot *newSlot;
  
   newSlot = get_struct(theEnv,templateSlot);
   newSlot->slotName = NULL;
   newSlot->defaultList = NULL;
   newSlot->facetList = NULL;
   newSlot->constraints = GetConstraintRecord(theEnv);
   newSlot->multislot = false;
   newSlot->noDefault = false;
   newSlot->defaultPresent = false;
   newSlot->defaultDynamic = false;
   newSlot->reactive = true;
   newSlot->next = NULL;
   
   return newSlot;
  }
  
/**************/
/* CopySlot: */
/**************/
static struct templateSlot *CopySlot(
  Environment *theEnv,
  struct templateSlot *source)
  {
   struct templateSlot *newSlot;

   newSlot = get_struct(theEnv,templateSlot);
   newSlot->slotName = source->slotName;
   newSlot->defaultList = CopyExpression(theEnv,source->defaultList);
   newSlot->facetList = CopyExpression(theEnv,source->facetList);
   newSlot->constraints = CopyConstraintRecord(theEnv,source->constraints);
   newSlot->multislot = source->multislot;
   newSlot->noDefault = source->noDefault;
   newSlot->defaultPresent = source->defaultPresent;
   newSlot->defaultDynamic = source->defaultDynamic;
   newSlot->reactive = source->reactive;
   newSlot->next = NULL;

   return newSlot;
  }

#endif /* (! RUN_TIME) && (! BLOAD_ONLY) */

#endif /* DEFTEMPLATE_CONSTRUCT */


