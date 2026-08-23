/* Is the TLS library that the build links the same implementation as the
 * headers that the build compiles against?
 *
 * This is a check at build time and is not part of clips. The makefile
 * compiles this file with the same flags as the other files, runs it, and
 * stops the build if the two parts do not agree.
 *
 * The check is necessary because different rules find the two parts. The
 * headers come from the include search path, and the libraries come from the
 * linker search path. The build sets the two sequences independently. A
 * second TLS library under /usr/local comes before the system library for the
 * headers, because gcc keeps the standard directories in a fixed sequence and
 * ignores a -I option that names one of them. But the linker can still find
 * the system copy of the library. The build then compiles against one
 * implementation and links a different one.
 *
 * Without this check the problem shows as undefined references to the symbols
 * that the two parts do not share. Those messages do not give the cause. This
 * check gives the name of each library.
 */

#include <stdio.h>
#include <string.h>

#include <openssl/opensslv.h>
#include <openssl/crypto.h>

int main(void)
{
   const char *headerName;
   const char *headerVersion;
   const char *runtime;

#if defined(LIBRESSL_VERSION_NUMBER)
   headerName = "LibreSSL";
   headerVersion = LIBRESSL_VERSION_TEXT;
#elif defined(OPENSSL_IS_BORINGSSL)
   headerName = "BoringSSL";
   headerVersion = "BoringSSL";
#else
   headerName = "OpenSSL";
   headerVersion = OPENSSL_VERSION_TEXT;
#endif

   /*======================================================*/
   /* The description that the library gives of itself.    */
   /* This is the only value here that comes from the      */
   /* library and not from a header.                       */
   /*======================================================*/

   runtime = OpenSSL_version(OPENSSL_VERSION);
   if (runtime == NULL) runtime = "";

   /*=========================================================*/
   /* BoringSSL gives "OpenSSL 1.1.1 (compatible;             */
   /* BoringSSL)". The code looks for its name first,         */
   /* because the string has the two names and only the       */
   /* more specific name has a meaning.                       */
   /*=========================================================*/

   if (strstr(runtime,"BoringSSL") != NULL)
     { if (strcmp(headerName,"BoringSSL") == 0) return 0; }
   else if (strstr(runtime,headerName) != NULL)
     { return 0; }

   fprintf(stderr,
      "\n"
      "TLS headers and TLS library are not the same implementation.\n"
      "\n"
      "  compiled against: %s\n"
      "  linked against:   %s\n"
      "\n"
      "Building this would fail at link time, or worse, produce a binary that\n"
      "calls one library through another's headers. Point both halves at the\n"
      "same install: build with TLS_PREFIX set to its prefix, or take the\n"
      "second library off the include and library search paths.\n"
      "\n",
      headerVersion,runtime);

   return 1;
}
