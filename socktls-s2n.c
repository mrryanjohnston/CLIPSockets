/*******************************************************/
/*      "C" Language Integrated Production System      */
/*                                                     */
/*            CLIPS Version ?.??  08/10/26             */
/*                                                     */
/*                 TLS BACKEND: S2N-TLS                */
/*******************************************************/

/***********************************************************************/
/* Purpose: This file implements the backend interface in socktlsbe.h  */
/*   with s2n-tls.                                                     */
/*                                                                     */
/*   Three items here have no equivalent in the other backends.        */
/*                                                                     */
/*   s2n waits after a handshake that failed. This is blinding, and it */
/*   hides how long a refusal took. The time then gives no data to an  */
/*   attacker. The wait is a maximum of thirty seconds, and it is      */
/*   inside the call. A library cannot stop its caller for that time.  */
/*   As a result, the code puts each connection into self-service      */
/*   blinding, and s2n gives the delay back to this file to apply or   */
/*   not to apply.                                                     */
/*                                                                     */
/*   s2n checks the certificate chain but leaves the name in the       */
/*   certificate to the caller. There is no equivalent of set1_host,   */
/*   only a callback that gets each name to accept or to refuse. As a  */
/*   result, this file owns the comparison rule below, and the other   */
/*   libraries keep that rule inside themselves.                       */
/*                                                                     */
/*   s2n gives a peer certificate as DER and has no call to read a     */
/*   name out of it. libcrypto has such a call, and s2n is built on    */
/*   libcrypto. As a result, libcrypto writes the subject.             */
/*                                                                     */
/* Principal Programmer(s):                                            */
/*      Ryan P. Johnston                                               */
/*                                                                     */
/* Revision History:                                                   */
/*                                                                     */
/*      ?.??: Added this file.                                         */
/*                                                                     */
/**********************************************************************/

#define _POSIX_C_SOURCE 200112L

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "setup.h"

#include "constant.h"
#include "envrnmnt.h"
#include "memalloc.h"
#include "prntutil.h"
#include "router.h"

#include "socktlsbe.h"

#ifdef USE_TLS

#include <s2n.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/evp.h>

/*==============================================================*/
/* The code selects the versions with the name of a security    */
/* policy and not with a protocol number. default_tls13 offers  */
/* TLS 1.2 and TLS 1.3, with 1.2 as the minimum. This is the    */
/* same minimum that the other backends set.                    */
/*==============================================================*/

#define S2N_POLICY_DEFAULT "default_tls13"

/*==============================================================*/
/* A config or a certificate chain that the code replaced but   */
/* cannot free yet.                                             */
/*                                                              */
/* The code cannot undo s2n_config_disable_x509_verification.   */
/* It sets a flag that the library never clears. The code also  */
/* cannot take a chain out of the store of a config. As a       */
/* result, a change to either of them makes the code build the  */
/* configuration again from the data that the caller gave the   */
/* context. The old config must stay in memory, because the     */
/* connections from it hold it by pointer. The chain in that    */
/* config must also stay, because a store holds chains by       */
/* pointer. The code keeps them here and frees them with the    */
/* context, and a context now stays until its last session      */
/* ends.                                                        */
/*==============================================================*/

struct s2nRetired
  {
   struct s2n_config *config;
   struct s2n_cert_chain_and_key *chain;
   struct s2nRetired *next;
  };

struct s2nContext
  {
   struct s2n_config *config;
   struct s2n_cert_chain_and_key *chain;
   /* The PEM text of the certificate and of its key, with the size of each
      buffer. The size is here because the memory manager of CLIPS takes the
      size at the free. strlen would not answer the question, because the size
      is the size of the file and a file can hold a zero byte. */
   char *certPem;
   size_t certPemSize;
   char *keyPem;
   size_t keyPemSize;
   /* The data that the caller gave this context. The code keeps it so that
      it can make the configuration again. s2n applies the settings to a
      config and has no call to read them back. As a result, the code must
      keep the settings to apply them again. */
   char *caFile;
   char *caDir;
   bool systemTrust;
   struct s2nRetired *retired;
   bool asClient;
   bool verifyRequired;
   /* This flag is true if config does not agree with the settings above.
      Each set function sets the flag, and ContextConfig clears it.
      ContextConfig is the only function that reads config, and it is the
      only function that needs the flag. A new context is stale and has no config.
      To make a config costs much more than to make a context, and a context
      with no session never needs a config. */
   bool configStale;
  };

struct s2nSession
  {
   struct s2n_connection *conn;
   s2n_blocked_status blocked;
   char hostname[256];
   bool verifyRequired;
   bool verified;
   int failure;
  };

static int LastError = 0;

static void RememberError(void)
{
	if (s2n_errno != S2N_ERR_T_OK) LastError = s2n_errno;
}

/*******************************************************/
/* TLSBackendStartup: s2n has global state and says    */
/*   so.                                               */
/*******************************************************/
bool TLSBackendStartup(void)
{
	return s2n_init() == S2N_SUCCESS;
}

const char *TLSBackendName(void)
{
	return "s2n";
}

const char *TLSBackendVersion(void)
{
	// s2n gives no version string of its own at run time.
	return "s2n-tls";
}

/*****************************************************************/
/* HostMatches: Tells if a name in a certificate covers the      */
/*   name that the caller asked for.                             */
/*                                                               */
/*   The rule is narrow on purpose. A first "*." matches exactly */
/*   one label and never the bare domain. Thus *.example.com     */
/*   covers a.example.com, but it covers neither example.com nor */
/*   a.b.example.com. A star at any other position is not a      */
/*   wildcard. The comparison ignores the letter case, because   */
/*   host names do. It does this for ASCII only, because a       */
/*   certificate holds names in their encoded form and this code */
/*   does not decode them.                                       */
/*                                                               */
/*   The function refuses each name that these rules do not      */
/*   permit. That is the correct direction for the one check     */
/*   between a caller and the certificate of a different         */
/*   machine.                                                    */
/*****************************************************************/
static int LowerChar(
		int c)
{
	return ((c >= 'A') && (c <= 'Z')) ? (c - 'A' + 'a') : c;
}

static bool SameName(
		const char *a,
		const char *b)
{
	while ((*a != '\0') && (*b != '\0'))
	{
		if (LowerChar((unsigned char) *a) != LowerChar((unsigned char) *b)) return false;
		a++;
		b++;
	}

	return (*a == '\0') && (*b == '\0');
}

static bool HostMatches(
		const char *wanted,
		const char *pattern)
{
	const char *afterStar;
	const char *firstDot;

	if ((wanted == NULL) || (pattern == NULL)) return false;
	if ((wanted[0] == '\0') || (pattern[0] == '\0')) return false;

	if ((pattern[0] != '*') || (pattern[1] != '.'))
	{ return SameName(wanted,pattern); }

	/*=======================================================*/
	/* After this point the pattern is a wildcard. Only a    */
	/* certificate that names an address can match an        */
	/* address.                                              */
	/*                                                       */
	/* The value 127.0.0.1 has the shape that the rules below */
	/* need: a first label and then the remainder. As a       */
	/* result, a certificate with DNS:*.0.0.1 would be        */
	/* correct for it, for 10.0.0.1, and for each other       */
	/* address with that end. Anyone can get such a name, and */
	/* that certificate would then be correct for hosts that  */
	/* its owner never saw.                                   */
	/*=======================================================*/

	if (TLSLooksLikeAddress(wanted)) return false;

	/*=======================================================*/
	/* This is a wildcard. The remainder of the pattern must */
	/* be a real name, and it must keep two labels or more.  */
	/* As a result, no one can get a "*.com" certificate for */
	/* each .com name.                                       */
	/*=======================================================*/

	afterStar = pattern + 2;
	if (afterStar[0] == '\0') return false;
	if (strchr(afterStar,'*') != NULL) return false;
	if (strchr(afterStar,'.') == NULL) return false;

	/*=======================================================*/
	/* The name that the caller wants must have a first      */
	/* label of its own. Its remainder must be equal to the  */
	/* text after the star.                                  */
	/*=======================================================*/

	firstDot = strchr(wanted,'.');
	if (firstDot == NULL) return false;
	if (firstDot == wanted) return false;

	return SameName(firstDot + 1,afterStar);
}

/*****************************************************************/
/* AcceptAnyName: The host callback for a server that checks a   */
/*   client certificate.                                         */
/*                                                               */
/*   s2n calls the callback for each name in the certificate of  */
/*   the peer. It refuses the handshake if the callback accepts  */
/*   no name. As a result, a server that authenticates clients   */
/*   must have a callback. There is no name for it to check,     */
/*   because the caller does not reach a client at an address    */
/*   that the caller selected. Nothing is expected of the name   */
/*   of a client. The chain authenticates the client, and s2n    */
/*   already checked that chain against the trust store before   */
/*   it calls this function.                                     */
/*****************************************************************/
static uint8_t AcceptAnyName(
		const char *name,
		size_t nameLen,
		void *data)
{
	(void) name;
	(void) nameLen;
	(void) data;

	return 1;
}

static uint8_t VerifyHost(
		const char *name,
		size_t nameLen,
		void *data)
{
	struct s2nSession *sess = (struct s2nSession *) data;
	char buffer[256];

	if ((sess == NULL) || (name == NULL)) return 0;
	if (nameLen >= sizeof(buffer)) return 0;

	// The name comes with a length and not with a terminator. A name with a
	// NUL byte inside it is a name that tries to look like a shorter name.
	if (memchr(name,'\0',nameLen) != NULL) return 0;

	memcpy(buffer,name,nameLen);
	buffer[nameLen] = '\0';

	return HostMatches(sess->hostname,buffer) ? 1 : 0;
}

/*=============================================================*/
/* Reads a full file, because s2n takes PEM as a string in     */
/* memory and not as a path.                                   */
/*=============================================================*/
static char *ReadWholeFile(
		Environment *theEnv,
		const char *path,
		size_t *length)
{
	FILE *fp;
	long size;
	char *text;
	size_t got;

	*length = 0;

	if ((path == NULL) || (path[0] == '\0')) return NULL;

	fp = fopen(path,"rb");
	if (fp == NULL) return NULL;

	if ((0 != fseek(fp,0,SEEK_END)) || (0 > (size = ftell(fp))) ||
	    (0 != fseek(fp,0,SEEK_SET)) || (size > (16 * 1024 * 1024)))
	{
		fclose(fp);
		return NULL;
	}

	text = (char *) gm2(theEnv,(size_t) size + 1);
	if (text == NULL)
	{
		fclose(fp);
		return NULL;
	}

	got = fread(text,1,(size_t) size,fp);
	fclose(fp);

	text[got] = '\0';

	*length = (size_t) size + 1;

	return text;
}

/****************************************************************/
/* PemParsesAs: Tells if the data is the type that the caller   */
/*   gave it as.                                                */
/*                                                              */
/*   s2n takes a certificate and its key in one call, and it    */
/*   reads neither of them until it has both. As a result, a    */
/*   bad path or a file of the incorrect type would show only   */
/*   at a later call, or not at all. libcrypto is already here  */
/*   and can answer now.                                        */
/****************************************************************/
static bool PemParsesAs(
		const char *pem,
		bool asCertificate)
{
	BIO *bio;
	X509 *cert;
	EVP_PKEY *key;
	bool ok = false;

	if (pem == NULL) return false;

	bio = BIO_new_mem_buf(pem,-1);
	if (bio == NULL) return false;

	if (asCertificate)
	{
		cert = PEM_read_bio_X509(bio,NULL,NULL,NULL);
		ok = (cert != NULL);
		if (cert != NULL) X509_free(cert);
	}
	else
	{
		key = PEM_read_bio_PrivateKey(bio,NULL,NULL,NULL);
		ok = (key != NULL);
		if (key != NULL) EVP_PKEY_free(key);
	}

	BIO_free(bio);

	return ok;
}

/****************************************************************/
/* BuildConfig: Makes a configuration that holds all the data   */
/*   that the caller gave this context.                         */
/*                                                              */
/*   This is one function because the code makes a              */
/*   configuration more than one time. A context that turns     */
/*   verification off and then on again needs a new             */
/*   configuration, because the code cannot correct the first   */
/*   one. Such a context comes back here.                       */
/*                                                              */
/*   The function uses s2n_config_new_minimal and not           */
/*   s2n_config_new. The two differ in one item: s2n_config_new */
/*   loads the system trust store into each config that it      */
/*   makes. On this machine that costs sixteen milliseconds     */
/*   against ten microseconds. It is more than one thousand     */
/*   times the cost of all the other work here, and a caller    */
/*   that names an authority of its own has no use for it.      */
/*                                                              */
/*   As a result, the code loads the store only when it needs   */
/*   the store, and it needs the store by default. A context    */
/*   that names no authority verifies against the system store. */
/*   s2n_config_new did the same, and this is what a caller     */
/*   that says nothing means. A named authority replaces the    */
/*   system store, unless the caller also asks for the system   */
/*   store by name. The code then loads both of them.           */
/****************************************************************/
static struct s2n_config *BuildConfig(
		struct s2nContext *ctx)
{
	struct s2n_config *cfg;

	cfg = s2n_config_new_minimal();
	if (cfg == NULL)
	{
		RememberError();
		return NULL;
	}

	if (S2N_SUCCESS != s2n_config_set_cipher_preferences(cfg,S2N_POLICY_DEFAULT))
	{
		RememberError();
		s2n_config_free(cfg);
		return NULL;
	}

	// For a server, a verified peer means this: ask for a certificate and
	// refuse the handshake without an acceptable one. s2n calls this client
	// auth and keeps it separate from certificate validation. A request for
	// validation alone leaves a server that accepts each client with no
	// identity.
	if (! ctx->asClient)
	{
		if (S2N_SUCCESS != s2n_config_set_client_auth_type(cfg,
				ctx->verifyRequired ? S2N_CERT_AUTH_REQUIRED : S2N_CERT_AUTH_NONE))
		{
			RememberError();
			s2n_config_free(cfg);
			return NULL;
		}
	}

	// This is the system store. The code loads it when the caller asks for it
	// by name, or by default when this context names no authority of its own.
	// There is nothing to empty first, because a minimal config starts with an
	// empty store.
	if (ctx->systemTrust || ((ctx->caFile == NULL) && (ctx->caDir == NULL)))
	{
		if (S2N_SUCCESS != s2n_config_load_system_certs(cfg))
		{
			RememberError();
			s2n_config_free(cfg);
			return NULL;
		}
	}

	if ((ctx->caFile != NULL) || (ctx->caDir != NULL))
	{
		// A named location replaces the system store and does not add to
		// it. That is what a caller means when it names its own authority,
		// and the condition above gives that result. A request for the
		// system store as well puts the system store back, which is what a
		// request for both means.
		if (S2N_SUCCESS != s2n_config_set_verification_ca_location(cfg,
				ctx->caFile,ctx->caDir))
		{
			RememberError();
			s2n_config_free(cfg);
			return NULL;
		}
	}

	if (ctx->chain != NULL)
	{
		if (S2N_SUCCESS != s2n_config_add_cert_chain_and_key_to_store(cfg,ctx->chain))
		{
			RememberError();
			s2n_config_free(cfg);
			return NULL;
		}
	}

	// This step is last, because the code cannot undo it. Each step above
	// can need a config that still verifies.
	if (! ctx->verifyRequired)
	{
		if (S2N_SUCCESS != s2n_config_disable_x509_verification(cfg))
		{
			RememberError();
			s2n_config_free(cfg);
			return NULL;
		}
	}

	return cfg;
}

/****************************************************************/
/* Retire: Keeps an item to free with the context and not now,  */
/*   because the connections from it hold it by pointer. Each   */
/*   argument can be NULL.                                      */
/****************************************************************/
static bool Retire(
		Environment *theEnv,
		struct s2nContext *ctx,
		struct s2n_config *cfg,
		struct s2n_cert_chain_and_key *chain)
{
	struct s2nRetired *old;

	if ((cfg == NULL) && (chain == NULL)) return true;

	old = (struct s2nRetired *) gm2(theEnv,sizeof(struct s2nRetired));
	if (old == NULL) return false;

	old->config = cfg;
	old->chain = chain;
	old->next = ctx->retired;
	ctx->retired = old;

	return true;
}

/****************************************************************/
/* Rebuild: Replaces the configuration with one that the code   */
/*   makes from the current settings. It keeps the old one to   */
/*   free later.                                                */
/****************************************************************/
static bool Rebuild(
		Environment *theEnv,
		struct s2nContext *ctx)
{
	struct s2n_config *cfg;

	cfg = BuildConfig(ctx);
	if (cfg == NULL) return false;

	if (! Retire(theEnv,ctx,ctx->config,NULL))
	{
		s2n_config_free(cfg);
		return false;
	}

	ctx->config = cfg;
	ctx->configStale = false;

	return true;
}

/****************************************************************/
/* ContextConfig: The configuration for this context. The       */
/*   function makes it now if the settings changed since the    */
/*   last one. It keeps the cause and gives NULL if it cannot   */
/*   make the configuration.                                    */
/*                                                              */
/*   Each set function marks the context stale and returns.     */
/*   This function does the work, and it has exactly one        */
/*   caller: the code that makes a session. Settings arrive in  */
/*   groups, and the code needs a configuration one time at the */
/*   end of a group. A context with no connection needs none at */
/*   all.                                                       */
/*                                                              */
/*   Each set function still refuses what it can refuse, and it */
/*   does this before it marks the context stale. As a result,  */
/*   a later build does not delay the report of an error. The   */
/*   code parses a certificate and a key in the call that gives */
/*   them, and it tries an authority path on a temporary config */
/*   below.                                                     */
/****************************************************************/
static struct s2n_config *ContextConfig(
		Environment *theEnv,
		struct s2nContext *ctx)
{
	if (ctx->configStale)
	{
		if (! Rebuild(theEnv,ctx)) return NULL;
	}

	return ctx->config;
}

/****************************************************************/
/* CALocationUsable: Tells if s2n accepts this pair of paths as */
/*   a trust store.                                             */
/*                                                              */
/*   The code asks here and does not leave the question to the  */
/*   build, because the answer belongs to the call that gives   */
/*   the paths. A caller that gives a file with no certificate  */
/*   must get the message at that time, and not when a later    */
/*   connection fails.                                          */
/*                                                              */
/*   The function asks on a temporary config. A minimal config  */
/*   costs almost nothing to make. To load a trust store is the */
/*   cost, and to load this store is the purpose here.          */
/****************************************************************/
static bool CALocationUsable(
		const char *file,
		const char *dir)
{
	struct s2n_config *probe;
	bool ok;

	probe = s2n_config_new_minimal();
	if (probe == NULL)
	{
		RememberError();
		return false;
	}

	ok = (S2N_SUCCESS == s2n_config_set_verification_ca_location(probe,file,dir));
	if (! ok) RememberError();

	s2n_config_free(probe);

	return ok;
}

void *TLSBackendNewContext(
		Environment *theEnv,
		bool asClient,
		bool datagram)
{
	struct s2nContext *ctx;

	// s2n-tls has no DTLS in either direction. The name does not appear in
	// any of its headers. As a result, and unlike the other backends, this
	// refusal is permanent and is not a step in the work.
	if (datagram) return NULL;

	ctx = (struct s2nContext *) gm2(theEnv,sizeof(struct s2nContext));
	if (ctx == NULL) return NULL;

	ctx->chain = NULL;
	ctx->certPem = NULL;
	ctx->certPemSize = 0;
	ctx->keyPem = NULL;
	ctx->keyPemSize = 0;
	ctx->caFile = NULL;
	ctx->caDir = NULL;
	ctx->systemTrust = false;
	ctx->retired = NULL;
	ctx->asClient = asClient;

	// Verification of the peer is the default for a client. As a result, a
	// program that does not ask for verification cannot make a connection
	// with no authentication. A server has no peer certificate to check
	// unless the program asks for one.
	ctx->verifyRequired = asClient;

	// There is no configuration yet. The code makes one when a session asks
	// for it. A context that the caller makes and frees without a connection
	// never asks.
	ctx->config = NULL;
	ctx->configStale = true;

	return ctx;
}

void TLSBackendFreeContext(
		Environment *theEnv,
		void *vctx)
{
	struct s2nContext *ctx = (struct s2nContext *) vctx;
	struct s2nRetired *old, *nextOld;

	if (ctx == NULL) return;

	// The code frees each configuration before it frees any chain, because a
	// config holds the chain by pointer and does not copy it. To add a chain
	// to the store does not give the store the ownership of the chain, and
	// that is what lets the code give the same chain to a new config. There
	// are two loops and not one, because a configuration can hold a chain that
	// the code retired before that configuration.
	//
	// No code uses these items now. The caller frees a context only after the
	// last session from that context ends, and socktls.c counts the references
	// for this.
	if (ctx->config != NULL) s2n_config_free(ctx->config);

	for (old = ctx->retired; old != NULL; old = old->next)
	{
		if (old->config != NULL) s2n_config_free(old->config);
	}

	if (ctx->chain != NULL) s2n_cert_chain_and_key_free(ctx->chain);

	for (old = ctx->retired; old != NULL; old = nextOld)
	{
		nextOld = old->next;
		if (old->chain != NULL) s2n_cert_chain_and_key_free(old->chain);
		rm(theEnv,old,sizeof(struct s2nRetired));
	}

	if (ctx->certPem != NULL) rm(theEnv,ctx->certPem,ctx->certPemSize);
	if (ctx->keyPem != NULL) rm(theEnv,ctx->keyPem,ctx->keyPemSize);
	TLSForgetPath(theEnv,&ctx->caFile);
	TLSForgetPath(theEnv,&ctx->caDir);
	rm(theEnv,ctx,sizeof(struct s2nContext));
}

/*************************************************************/
/* ApplyOwnCertificate: s2n takes the certificate and its    */
/*   key together. As a result, the code gives the pair to   */
/*   s2n after it gets both of them. The sequence of the two */
/*   is not important.                                       */
/*                                                           */
/*   A second certificate replaces the certificate in use. A */
/*   caller that replaces a certificate usually has a cause  */
/*   for the change, and a TRUE while the old certificate    */
/*   stays on the network is the worst answer.               */
/*                                                           */
/*   To replace the certificate, the code must make the full */
/*   configuration again. The code cannot take a chain out   */
/*   of a store, and a config with the old chain would offer */
/*   both of them.                                           */
/*************************************************************/
static bool ApplyOwnCertificate(
		Environment *theEnv,
		struct s2nContext *ctx)
{
	struct s2n_cert_chain_and_key *chain;

	if ((ctx->certPem == NULL) || (ctx->keyPem == NULL)) return true;

	chain = s2n_cert_chain_and_key_new();
	if (chain == NULL)
	{
		RememberError();
		return false;
	}

	if (S2N_SUCCESS != s2n_cert_chain_and_key_load_pem(chain,ctx->certPem,ctx->keyPem))
	{
		RememberError();
		s2n_cert_chain_and_key_free(chain);
		return false;
	}

	// The code keeps the old chain and does not free it. The configuration
	// that holds the chain can still serve connections, and that store holds
	// the chain by pointer.
	if (! Retire(theEnv,ctx,NULL,ctx->chain))
	{
		s2n_cert_chain_and_key_free(chain);
		return false;
	}

	ctx->chain = chain;
	ctx->configStale = true;

	return true;
}

bool TLSBackendLoadVerifyLocations(
		Environment *theEnv,
		void *vctx,
		const char *caFile,
		const char *caPath)
{
	struct s2nContext *ctx = (struct s2nContext *) vctx;
	const char *file = ((caFile != NULL) && (caFile[0] != '\0')) ? caFile : NULL;
	const char *dir = ((caPath != NULL) && (caPath[0] != '\0')) ? caPath : NULL;

	if ((file == NULL) && (dir == NULL)) return false;

	// The code tries the path before it keeps the path. As a result, it
	// refuses a path that s2n does not accept. Such a path does not become a
	// setting that stops the next configuration from this context.
	if (! CALocationUsable(file,dir)) return false;

	if (! TLSRememberPath(theEnv,&ctx->caFile,file)) return false;
	if (! TLSRememberPath(theEnv,&ctx->caDir,dir)) return false;

	// The code applies this setting when it makes the configuration again. It
	// does not change the current configuration. As a result, this setting and
	// each other setting reach a config in one sequence, whatever the sequence
	// of the calls was.
	ctx->configStale = true;

	return true;
}

bool TLSBackendLoadSystemTrust(
		void *vctx)
{
	struct s2nContext *ctx = (struct s2nContext *) vctx;

	ctx->systemTrust = true;
	ctx->configStale = true;

	return true;
}

bool TLSBackendUseCertificateFile(
		Environment *theEnv,
		void *vctx,
		const char *path)
{
	struct s2nContext *ctx = (struct s2nContext *) vctx;

	if (ctx->certPem != NULL) rm(theEnv,ctx->certPem,ctx->certPemSize);

	ctx->certPem = ReadWholeFile(theEnv,path,&ctx->certPemSize);
	if (ctx->certPem == NULL) return false;

	if (! PemParsesAs(ctx->certPem,true))
	{
		rm(theEnv,ctx->certPem,ctx->certPemSize);
		ctx->certPem = NULL;
		ctx->certPemSize = 0;
		return false;
	}

	return ApplyOwnCertificate(theEnv,ctx);
}

bool TLSBackendUsePrivateKeyFile(
		Environment *theEnv,
		void *vctx,
		const char *path)
{
	struct s2nContext *ctx = (struct s2nContext *) vctx;

	if (ctx->keyPem != NULL) rm(theEnv,ctx->keyPem,ctx->keyPemSize);

	ctx->keyPem = ReadWholeFile(theEnv,path,&ctx->keyPemSize);
	if (ctx->keyPem == NULL) return false;

	if (! PemParsesAs(ctx->keyPem,false))
	{
		rm(theEnv,ctx->keyPem,ctx->keyPemSize);
		ctx->keyPem = NULL;
		ctx->keyPemSize = 0;
		return false;
	}

	return ApplyOwnCertificate(theEnv,ctx);
}

/****************************************************************/
/* TLSBackendSetVerify: In s2n you cannot turn verification     */
/*   back on in the same config. disable_x509_verification sets */
/*   a flag that the library never clears. As a result, this    */
/*   function makes the configuration again and does not change */
/*   the current one. That is what lets a caller turn the       */
/*   setting on again. Each other backend permits this, and a   */
/*   caller has no cause to expect a different behaviour here.  */
/****************************************************************/
bool TLSBackendSetVerify(
		void *vctx,
		bool required)
{
	struct s2nContext *ctx = (struct s2nContext *) vctx;

	if (required == ctx->verifyRequired) return true;

	ctx->verifyRequired = required;
	ctx->configStale = true;

	return true;
}

/****************************************************************/
/* TLSBackendSetMinVersion: s2n selects versions with the name  */
/*   of a security policy and not with a minimum version. The   */
/*   policy in use already offers TLS 1.2 and TLS 1.3, with 1.2 */
/*   as the minimum. A request for 1.2 is already correct. A    */
/*   request for 1.3 would need a policy that removes 1.2. To   */
/*   report that condition is better than to accept the request */
/*   and then negotiate 1.2.                                    */
/****************************************************************/
bool TLSBackendSetMinVersion(
		void *vctx,
		int version)
{
	(void) vctx;

	return version == TLS_VERSION_1_2;
}

void *TLSBackendNewSession(
		Environment *theEnv,
		void *vctx,
		int fd,
		bool asClient,
		const char *hostname)
{
	struct s2nContext *ctx = (struct s2nContext *) vctx;
	struct s2nSession *sess;
	struct s2n_config *cfg;

	// This is the one caller that needs a configuration. As a result, it is
	// the one place where the code changes the settings of this context into
	// a configuration. The code does this before it allocates memory. A
	// configuration that the code cannot make then needs no cleanup.
	cfg = ContextConfig(theEnv,ctx);
	if (cfg == NULL) return NULL;

	sess = (struct s2nSession *) gm2(theEnv,sizeof(struct s2nSession));
	if (sess == NULL) return NULL;

	sess->conn = NULL;
	sess->blocked = S2N_NOT_BLOCKED;
	sess->hostname[0] = '\0';
	sess->verifyRequired = ctx->verifyRequired;
	sess->verified = false;
	sess->failure = 0;

	sess->conn = s2n_connection_new(asClient ? S2N_CLIENT : S2N_SERVER);
	if (sess->conn == NULL)
	{
		RememberError();
		rm(theEnv,sess,sizeof(struct s2nSession));
		return NULL;
	}

	// Without this call, a handshake that s2n refuses waits inside the call
	// for a maximum of thirty seconds. The wait hides which check failed.
	// Self-service blinding gives that delay back to this file.
	if (S2N_SUCCESS != s2n_connection_set_blinding(sess->conn,S2N_SELF_SERVICE_BLINDING))
	{
		RememberError();
		TLSBackendFreeSession(theEnv,sess);
		return NULL;
	}

	if (S2N_SUCCESS != s2n_connection_set_config(sess->conn,cfg))
	{
		RememberError();
		TLSBackendFreeSession(theEnv,sess);
		return NULL;
	}

	if (S2N_SUCCESS != s2n_connection_set_fd(sess->conn,fd))
	{
		RememberError();
		TLSBackendFreeSession(theEnv,sess);
		return NULL;
	}

	if (asClient && (hostname != NULL) && (hostname[0] != '\0'))
	{
		if (strlen(hostname) >= sizeof(sess->hostname))
		{
			TLSBackendFreeSession(theEnv,sess);
			return NULL;
		}

		strcpy(sess->hostname,hostname);

		if (S2N_SUCCESS != s2n_set_server_name(sess->conn,hostname))
		{
			RememberError();
			TLSBackendFreeSession(theEnv,sess);
			return NULL;
		}

		// This file checks the name, because s2n gives only the callback.
		// Without a callback, the code would accept a chain from a trusted
		// authority with any name in it.
		if (ctx->verifyRequired)
		{
			if (S2N_SUCCESS != s2n_connection_set_verify_host_callback(sess->conn,
					VerifyHost,sess))
			{
				RememberError();
				TLSBackendFreeSession(theEnv,sess);
				return NULL;
			}
		}
	}

	// A server that checks a client certificate needs a callback of its own,
	// for the opposite cause. s2n refuses the handshake when the callback
	// accepts no name, and a client is expected to have no specific name.
	if ((! asClient) && ctx->verifyRequired)
	{
		if (S2N_SUCCESS != s2n_connection_set_verify_host_callback(sess->conn,
				AcceptAnyName,sess))
		{
			RememberError();
			TLSBackendFreeSession(theEnv,sess);
			return NULL;
		}
	}

	return sess;
}

void TLSBackendFreeSession(
		Environment *theEnv,
		void *vsess)
{
	struct s2nSession *sess = (struct s2nSession *) vsess;

	if (sess == NULL) return;

	if (sess->conn != NULL)
	{
		s2n_connection_free(sess->conn);
	}

	rm(theEnv,sess,sizeof(struct s2nSession));
}

int TLSBackendHandshake(
		void *vsess)
{
	struct s2nSession *sess = (struct s2nSession *) vsess;

	s2n_errno = S2N_ERR_T_OK;

	if (s2n_negotiate(sess->conn,&sess->blocked) == S2N_SUCCESS)
	{
		sess->verified = true;
		return TLS_HANDSHAKE_DONE;
	}

	if (s2n_error_get_type(s2n_errno) == S2N_ERR_T_BLOCKED)
	{ return TLS_HANDSHAKE_AGAIN; }

	sess->failure = s2n_errno;
	RememberError();

	return TLS_HANDSHAKE_FAIL;
}

long TLSBackendRead(
		void *vsess,
		void *buf,
		size_t len)
{
	struct s2nSession *sess = (struct s2nSession *) vsess;
	ssize_t n;

	if (len > INT_MAX) len = INT_MAX;

	s2n_errno = S2N_ERR_T_OK;

	n = s2n_recv(sess->conn,buf,len,&sess->blocked);
	if (n > 0) return (long) n;
	if (n == 0) return TLS_RESULT_EOF;

	switch (s2n_error_get_type(s2n_errno))
	{
		case S2N_ERR_T_BLOCKED:
			return TLS_RESULT_AGAIN;

		case S2N_ERR_T_CLOSED:
			return TLS_RESULT_EOF;

		default:
			RememberError();
			return TLS_RESULT_FAIL;
	}
}

long TLSBackendWrite(
		void *vsess,
		const void *buf,
		size_t len)
{
	struct s2nSession *sess = (struct s2nSession *) vsess;
	ssize_t n;

	if (len > INT_MAX) len = INT_MAX;

	s2n_errno = S2N_ERR_T_OK;

	n = s2n_send(sess->conn,buf,len,&sess->blocked);
	if (n > 0) return (long) n;

	switch (s2n_error_get_type(s2n_errno))
	{
		case S2N_ERR_T_BLOCKED:
			return TLS_RESULT_AGAIN;

		case S2N_ERR_T_CLOSED:
			return TLS_RESULT_EOF;

		default:
			RememberError();
			return TLS_RESULT_FAIL;
	}
}

int TLSBackendPending(
		void *vsess)
{
	struct s2nSession *sess = (struct s2nSession *) vsess;
	uint32_t avail;

	avail = s2n_peek(sess->conn);

	return (avail > INT_MAX) ? INT_MAX : (int) avail;
}

bool TLSBackendShutdown(
		void *vsess)
{
	struct s2nSession *sess = (struct s2nSession *) vsess;
	int rv;

	s2n_errno = S2N_ERR_T_OK;

	// The code sends the close_notify of this end and does not wait for the
	// close_notify of the peer. It does the same in each other backend here,
	// because the peer possibly never replies.
	rv = s2n_shutdown_send(sess->conn,&sess->blocked);

	return (rv == S2N_SUCCESS) || (s2n_error_get_type(s2n_errno) == S2N_ERR_T_BLOCKED);
}

const char *TLSBackendCipher(
		void *vsess)
{
	struct s2nSession *sess = (struct s2nSession *) vsess;
	const char *name = s2n_connection_get_cipher(sess->conn);

	return (name != NULL) ? name : "";
}

const char *TLSBackendProtocol(
		void *vsess)
{
	struct s2nSession *sess = (struct s2nSession *) vsess;

	switch (s2n_connection_get_actual_protocol_version(sess->conn))
	{
		case S2N_TLS12: return "TLSv1.2";
		case S2N_TLS13: return "TLSv1.3";
		default: return "";
	}
}

/****************************************************************/
/* TLSBackendVerifyOK: s2n keeps no verification result to read */
/*   later. It refuses the handshake instead. As a result, a    */
/*   handshake that completed is the answer. A context that     */
/*   asked for no check has nothing to refuse.                  */
/****************************************************************/
bool TLSBackendVerifyOK(
		void *vsess)
{
	struct s2nSession *sess = (struct s2nSession *) vsess;

	if (! sess->verifyRequired) return true;

	return sess->verified;
}

bool TLSBackendVerifyDescription(
		void *vsess,
		char *out,
		size_t outLen)
{
	struct s2nSession *sess = (struct s2nSession *) vsess;
	const char *text;

	if (! sess->verifyRequired) return false;
	if (sess->verified) return false;
	if (sess->failure == 0) return false;

	text = s2n_strerror(sess->failure,"EN");
	if (text == NULL) return false;

	strncpy(out,text,outLen - 1);
	out[outLen - 1] = '\0';

	return out[0] != '\0';
}

/*****************************************************************/
/* TLSBackendPeerSubject: s2n gives the chain of the peer as DER */
/*   and has no call to read a name out of it. libcrypto has     */
/*   such a call, and s2n is built on libcrypto. As a result,    */
/*   libcrypto writes the text here.                             */
/*****************************************************************/
bool TLSBackendPeerSubject(
		void *vsess,
		char *out,
		size_t outLen)
{
	struct s2nSession *sess = (struct s2nSession *) vsess;
	struct s2n_cert_chain_and_key *chain;
	struct s2n_cert *cert = NULL;
	const uint8_t *der = NULL;
	uint32_t derLen = 0;
	uint32_t count = 0;
	X509 *x509;
	bool ok = false;

	chain = s2n_cert_chain_and_key_new();
	if (chain == NULL) return false;

	if (S2N_SUCCESS != s2n_connection_get_peer_cert_chain(sess->conn,chain))
	{
		s2n_cert_chain_and_key_free(chain);
		return false;
	}

	if ((S2N_SUCCESS != s2n_cert_chain_get_length(chain,&count)) || (count == 0))
	{
		s2n_cert_chain_and_key_free(chain);
		return false;
	}

	if ((S2N_SUCCESS != s2n_cert_chain_get_cert(chain,&cert,0)) ||
	    (S2N_SUCCESS != s2n_cert_get_der(cert,&der,&derLen)) ||
	    (der == NULL) || (derLen == 0))
	{
		s2n_cert_chain_and_key_free(chain);
		return false;
	}

	x509 = d2i_X509(NULL,&der,(long) derLen);
	if (x509 != NULL)
	{
		ok = (X509_NAME_oneline(X509_get_subject_name(x509),out,(int) outLen) != NULL);
		X509_free(x509);
	}

	s2n_cert_chain_and_key_free(chain);

	return ok;
}

void TLSBackendReportError(
		Environment *theEnv,
		const char *what)
{
	const char *text = NULL;

	if (LastError != 0)
	{
		text = s2n_strerror(LastError,"EN");
		LastError = 0;
	}

	TLSReportBackendError(theEnv,what,text);
}


/*=================*/
/* DATAGRAMS       */
/*=================*/

/* s2n-tls implements no DTLS. These functions are the final answer and are
   not a temporary result. Each entry point fails and gives no partial
   answer. As a result, the code cannot make a datagram context, and no
   subsequent code must guess the meaning of an incomplete DTLS session. */

bool TLSBackendSupportsDTLS(
		bool asServer)
{
	return false;
}

bool TLSBackendDTLSRestartable(void)
{
	return false;
}

/* The six answers at session level. socktlsbe.h has them one time, because
   the OpenSSL family needs these same answers when the build uses a library
   with no datagram BIO. */
TLS_DEFINE_NO_DTLS_SESSION_STUBS()

#endif /* USE_TLS */
