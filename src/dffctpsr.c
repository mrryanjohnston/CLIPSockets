   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 7.00  01/22/24             */
   /*                                                     */
   /*                DEFFACTS PARSER MODULE               */
   /*******************************************************/

/*************************************************************/
/* Purpose: Parses a deffacts construct.                     */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            GetConstructNameAndComment API change.         */
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
/*      7.00: Construct hashing for quick lookup.            */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if DEFFACTS_CONSTRUCT

#if BLOAD || BLOAD_AND_BSAVE
#include "bload.h"
#endif

#include "cstrcpsr.h"
#include "dffctbsc.h"
#include "dffctdef.h"
#include "envrnmnt.h"
#include "factrhs.h"
#include "memalloc.h"
#include "modulutl.h"
#include "pprint.h"
#include "prntutil.h"
#include "router.h"

#include "dffctpsr.h"

/************************************************************/
/* ParseDeffacts: Coordinates all actions necessary for the */
/*   addition of a deffacts construct into the current      */
/*   environment. Called when parsing a construct after the */
/*   deffacts keyword has been found.                       */
/************************************************************/
bool ParseDeffacts(
  Environment *theEnv,
  const char *readSource)
  {
#if (! RUN_TIME) && (! BLOAD_ONLY)
   CLIPSLexeme *deffactsName;
   struct expr *temp;
   Deffacts *newDeffacts;
   bool deffactsError;
   struct token inputToken;

   /*=========================*/
   /* Parsing initialization. */
   /*=========================*/

   deffactsError = false;
   SetPPBufferStatus(theEnv,true);

   FlushPPBuffer(theEnv);
   SetIndentDepth(theEnv,3);
   SavePPBuffer(theEnv,"(deffacts ");

   /*==========================================================*/
   /* Deffacts can not be added when a binary image is loaded. */
   /*==========================================================*/

#if BLOAD || BLOAD_AND_BSAVE
   if ((Bloaded(theEnv) == true) && (! ConstructData(theEnv)->CheckSyntaxMode))
     {
      CannotLoadWithBloadMessage(theEnv,"deffacts");
      return true;
     }
#endif

   /*============================*/
   /* Parse the deffacts header. */
   /*============================*/

   deffactsName = GetConstructNameAndComment(theEnv,readSource,&inputToken,"deffacts",
                                             (FindConstructFunction *) FindDeffactsInModule,
                                             (DeleteConstructFunction *) Undeffacts,"$",true,
                                             true,true,false);
   if (deffactsName == NULL) { return true; }

   /*===============================================*/
   /* Parse the list of facts in the deffacts body. */
   /*===============================================*/

   temp = BuildRHSAssert(theEnv,readSource,&inputToken,&deffactsError,false,false,"deffacts");

   if (deffactsError == true) { return true; }

   if (ExpressionContainsVariables(temp,false))
     {
      LocalVariableErrorMessage(theEnv,"a deffacts construct");
      ReturnExpression(theEnv,temp);
      return true;
     }

   SavePPBuffer(theEnv,"\n");

   /*==============================================*/
   /* If we're only checking syntax, don't add the */
   /* successfully parsed deffacts to the KB.      */
   /*==============================================*/

   if (ConstructData(theEnv)->CheckSyntaxMode)
     {
      ReturnExpression(theEnv,temp);
      return false;
     }

   /*==========================*/
   /* Create the new deffacts. */
   /*==========================*/

   ExpressionInstall(theEnv,temp);
   newDeffacts = get_struct(theEnv,deffacts);
   IncrementLexemeCount(deffactsName);
   InitializeConstructHeader(theEnv,"deffacts",DEFFACTS,&newDeffacts->header,deffactsName);

   newDeffacts->assertList = PackExpression(theEnv,temp);
   ReturnExpression(theEnv,temp);

   /*=======================================================*/
   /* Save the pretty print representation of the deffacts. */
   /*=======================================================*/

   if (GetConserveMemory(theEnv) == true)
     { newDeffacts->header.ppForm = NULL; }
   else
     { newDeffacts->header.ppForm = CopyPPBuffer(theEnv); }

   /*=============================================*/
   /* Add the deffacts to the appropriate module. */
   /*=============================================*/

   AddConstructToModule(&newDeffacts->header);
   AddConstructToHashMap(theEnv,&newDeffacts->header,newDeffacts->header.whichModule);

#endif /* (! RUN_TIME) && (! BLOAD_ONLY) */

   /*================================================================*/
   /* Return false to indicate the deffacts was successfully parsed. */
   /*================================================================*/

   return false;
  }

#endif /* DEFFACTS_CONSTRUCT */


