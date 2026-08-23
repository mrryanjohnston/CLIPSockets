/*******************************************************/
/*      "C" Language Integrated Production System      */
/*                                                     */
/*            CLIPS Version ?.??  08/10/26             */
/*                                                     */
/*                 TLS BACKEND: MBEDTLS                */
/*******************************************************/

/***********************************************************************/
/* Purpose: This file implements the backend interface in socktlsbe.h  */
/*   with mbedTLS. mbedTLS shares no API with OpenSSL, and it cannot   */
/*   share socktls-openssl.c.                                          */
/*                                                                     */
/*   Three differences give this file its shape. mbedTLS keeps no      */
/*   global state. As a result, the code makes and seeds one random    */
/*   number generator for each context, and it keeps that generator    */
/*   while the configuration points at it. mbedTLS also reports a      */
/*   failure with a negative code and does not put errors in a queue.  */
/*   As a result, this file keeps the last code for                    */
/*   TLSBackendReportError to write. Last, mbedTLS knows nothing about */
/*   a system trust store, and LoadSystemTrust looks in the locations  */
/*   that the usual distributions use.                                 */
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
#include <string.h>

#include <arpa/inet.h>
#include <errno.h>
#include <sys/socket.h>

#include "setup.h"

#include "constant.h"
#include "envrnmnt.h"
#include "memalloc.h"
#include "prntutil.h"
#include "router.h"

#include "socktlsbe.h"

#ifdef USE_TLS

#include <time.h>

/* This header comes first and not in alphabetical order. It gives the major
   version, and the include lines below are a function of that version. */
#include <mbedtls/version.h>

/* Version 4.x moved the random generator into PSA Crypto. The entropy and
   DRBG contexts do not exist there, and their headers do not exist either.
   Each call that took a generator now takes none, which is the same shape as
   in 2.x. As a result, the oldest and the newest version share code that 3.x
   does not share. The tests below read "3 only" where a call differs only in
   that argument. They read "3 or more" where 4.x keeps what 3.x added. */
#if MBEDTLS_VERSION_MAJOR >= 4
#include <psa/crypto.h>
#else
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#endif

#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ssl_cookie.h>
#include <mbedtls/x509_crt.h>

/*==============================================================*/
/* A context owns each item that a session needs after the      */
/* creation of that session. mbedtls_ssl_config keeps pointers  */
/* to the trust chain, the certificate and the key, and not     */
/* copies of them. As a result, all of these items must be      */
/* here. Up to 3.x this included the random generator. Version  */
/* 4.x takes its own generator from PSA.                        */
/*==============================================================*/

struct mbedContext
  {
   mbedtls_ssl_config conf;
#if MBEDTLS_VERSION_MAJOR < 4
   mbedtls_entropy_context entropy;
   mbedtls_ctr_drbg_context drbg;
#endif
   mbedtls_x509_crt ca;
   mbedtls_x509_crt ownCert;
   mbedtls_pk_context ownKey;
   bool haveCert;
   bool haveKey;
   bool ownCertApplied;
   /* For datagram contexts only. The cookie secret is here and not in a
      session. As a result, one session can accept the reply to a
      HelloVerifyRequest that a different session sent, and the code can start
      the exchange again. */
   mbedtls_ssl_cookie_ctx cookie;
   bool datagram;
  };

struct mbedSession
  {
   mbedtls_ssl_context ssl;
   mbedtls_net_context net;
   /* The code sets this flag when the name to verify is an address and not
      a host name. The two fields below then hold the address in the format
      that a certificate uses. */
   bool peerIsAddress;
   unsigned char address[16];
   size_t addressLen;
   /* The times for retransmission. mbedTLS does not run a DTLS handshake
      without a location for these values, and it controls them through the
      two callbacks below. It asks for a signal at two points, and it asks
      which of the two points passed. These fields are here and do not come
      from mbedtls_timing_delay_context, because that structure is behind
      MBEDTLS_TIMING_C. That option is not always on in a build, and its
      absence shows only at link time. */
   struct timespec timerStart;
   uint32_t timerIntMs;
   uint32_t timerFinMs;
   bool timerRunning;
   /* The address of the peer when the server sent the cookie. The code keeps
      it because a HelloVerifyRequest makes the code reset the session, and a
      session that the code resets needs the address again. */
   unsigned char clientId[128];
   size_t clientIdLen;
  };

/* mbedTLS gives its errors and does not put them in a queue. As a result,
   the code keeps the most recent code for TLSBackendReportError. */
TLS_DEFINE_LAST_ERROR()

/*******************************************************/
/* TLSBackendStartup: Version 4.x does its             */
/*   cryptography through PSA. The code must start PSA */
/*   before it asks PSA for work. To parse a           */
/*   certificate is such work, and a handshake is not  */
/*   the only example. PSA starts one time for the     */
/*   process, and this function is the correct         */
/*   location for that. The earlier versions need no   */
/*   start call.                                       */
/*******************************************************/
bool TLSBackendStartup(void)
{
#if MBEDTLS_VERSION_MAJOR >= 4
	/* The code does not keep this status, and that is on purpose.
	   mbedtls_strerror writes the text for LastError, and it translates
	   mbedTLS codes. A psa_status_t is a different set of numbers, and
	   mbedtls_strerror would give the text of the mbedTLS error with the
	   same value. A failure with no message is better than a failure with
	   an incorrect message. */
	if (psa_crypto_init() != PSA_SUCCESS) return false;
#endif

	return true;
}

const char *TLSBackendName(void)
{
	return "mbedtls";
}

const char *TLSBackendVersion(void)
{
	return MBEDTLS_VERSION_STRING;
}

/**************************************************************/
/* TLSBackendNewContext: Makes a configuration and the        */
/*   generator that it needs. The function initializes each   */
/*   field before any step can fail. As a result, the free    */
/*   function can operate on an incomplete context and it     */
/*   reads no field that has no value.                        */
/**************************************************************/
void *TLSBackendNewContext(
		Environment *theEnv,
		bool asClient,
		bool datagram)
{
	struct mbedContext *ctx;
#if MBEDTLS_VERSION_MAJOR < 4
	static const char seed[] = "CLIPSockets";
#endif

	ctx = (struct mbedContext *) gm2(theEnv,sizeof(struct mbedContext));
	if (ctx == NULL) return NULL;

	mbedtls_ssl_config_init(&ctx->conf);
#if MBEDTLS_VERSION_MAJOR < 4
	mbedtls_entropy_init(&ctx->entropy);
	mbedtls_ctr_drbg_init(&ctx->drbg);
#endif
	mbedtls_x509_crt_init(&ctx->ca);
	mbedtls_x509_crt_init(&ctx->ownCert);
	mbedtls_pk_init(&ctx->ownKey);
	mbedtls_ssl_cookie_init(&ctx->cookie);
	ctx->datagram = datagram;
	ctx->haveCert = false;
	ctx->haveKey = false;
	ctx->ownCertApplied = false;

#if MBEDTLS_VERSION_MAJOR < 4
	if (0 != (LastError = mbedtls_ctr_drbg_seed(&ctx->drbg,mbedtls_entropy_func,
			&ctx->entropy,(const unsigned char *) seed,sizeof(seed) - 1)))
	{
		TLSBackendFreeContext(theEnv,ctx);
		return NULL;
	}
#endif

	if (0 != (LastError = mbedtls_ssl_config_defaults(&ctx->conf,
			asClient ? MBEDTLS_SSL_IS_CLIENT : MBEDTLS_SSL_IS_SERVER,
			datagram ? MBEDTLS_SSL_TRANSPORT_DATAGRAM : MBEDTLS_SSL_TRANSPORT_STREAM,
			MBEDTLS_SSL_PRESET_DEFAULT)))
	{
		TLSBackendFreeContext(theEnv,ctx);
		return NULL;
	}

	// Version 4.x takes random data from PSA, and there is no generator to
	// give it.
#if MBEDTLS_VERSION_MAJOR < 4
	mbedtls_ssl_conf_rng(&ctx->conf,mbedtls_ctr_drbg_random,&ctx->drbg);
#endif

	// A DTLS server answers the first ClientHello with a cookie. It does no
	// work until a cookie comes back that agrees with the address that it
	// sent the cookie to. The secret belongs to the context. As a result,
	// each session from that context sends and accepts the same cookies.
	if (datagram && (! asClient))
	{
#if MBEDTLS_VERSION_MAJOR >= 4
		if (0 != (LastError = mbedtls_ssl_cookie_setup(&ctx->cookie)))
#else
		if (0 != (LastError = mbedtls_ssl_cookie_setup(&ctx->cookie,
				mbedtls_ctr_drbg_random,&ctx->drbg)))
#endif
		{
			TLSBackendFreeContext(theEnv,ctx);
			return NULL;
		}

		mbedtls_ssl_conf_dtls_cookies(&ctx->conf,mbedtls_ssl_cookie_write,
				mbedtls_ssl_cookie_check,&ctx->cookie);
	}

	// This is the same minimum that the OpenSSL backend sets. As a result, a
	// context has the same meaning with each library. DTLS 1.2 has the same
	// version number as TLS 1.2 here, and one minimum gives both of them.
	// That minimum removes DTLS 1.0, and this is the intended result.
#if MBEDTLS_VERSION_MAJOR >= 3
	mbedtls_ssl_conf_min_tls_version(&ctx->conf,MBEDTLS_SSL_VERSION_TLS1_2);
#else
	mbedtls_ssl_conf_min_version(&ctx->conf,MBEDTLS_SSL_MAJOR_VERSION_3,
			MBEDTLS_SSL_MINOR_VERSION_3);
#endif

	// Verification of the peer is the default for a client. As a result, a
	// program that does not ask for verification cannot make a connection
	// with no authentication. A server has no peer certificate to check
	// unless the program asks for one.
	mbedtls_ssl_conf_authmode(&ctx->conf,
			asClient ? MBEDTLS_SSL_VERIFY_REQUIRED : MBEDTLS_SSL_VERIFY_NONE);

	return ctx;
}

void TLSBackendFreeContext(
		Environment *theEnv,
		void *vctx)
{
	struct mbedContext *ctx = (struct mbedContext *) vctx;

	if (ctx == NULL) return;

	mbedtls_ssl_cookie_free(&ctx->cookie);
	mbedtls_pk_free(&ctx->ownKey);
	mbedtls_x509_crt_free(&ctx->ownCert);
	mbedtls_x509_crt_free(&ctx->ca);
#if MBEDTLS_VERSION_MAJOR < 4
	mbedtls_ctr_drbg_free(&ctx->drbg);
	mbedtls_entropy_free(&ctx->entropy);
#endif
	mbedtls_ssl_config_free(&ctx->conf);

	rm(theEnv,ctx,sizeof(struct mbedContext));
}

/*************************************************************/
/* ApplyOwnCertificate: A certificate is usable only with    */
/*   its key. As a result, the code gives the pair to the    */
/*   library after it gets both of them. The sequence of     */
/*   the two is not important.                               */
/*************************************************************/
static bool ApplyOwnCertificate(
		struct mbedContext *ctx)
{
	if (! (ctx->haveCert && ctx->haveKey)) return true;
	if (ctx->ownCertApplied) return true;

	// This code asks if the key belongs to the certificate.
	// mbedtls_ssl_conf_own_cert does not ask this question, but each other
	// backend here does. OpenSSL asks through SSL_CTX_check_private_key, and
	// the others ask inside their own load functions. Without this code,
	// mbedTLS alone accepts a pair that does not agree. It then reports the
	// error later as a handshake failure, and that message does not show the
	// true cause.
	//
	// mbedtls_pk_check_pair cannot answer the question in 4.x. There, a parse
	// of a private key puts the key into PSA and calculates the public half
	// only on demand. That function refuses each context whose public half is
	// not already in it, which is each key that this backend reads from a
	// file. As a result, the code asks the question through
	// mbedtls_pk_write_pubkey_der. The documentation of that function says
	// that it takes "a valid public or private key" and that it calculates
	// the public half. The code then compares the two public keys as that
	// function writes them.
	//
	// Of the two older versions, only 3.x takes a generator. Version 2.x
	// never had that argument, and 4.x no longer has it.
#if MBEDTLS_VERSION_MAJOR >= 4
	{
		// The function writes at the end of the buffer. As a result, the
		// comparison starts at the offset that the length gives, and not at
		// the front. A key that is too large for the buffer gives an error
		// and not a short result. The size is a limit and not an
		// assumption.
		unsigned char certPub[MBEDTLS_PK_MAX_PUBKEY_RAW_LEN + 64];
		unsigned char keyPub[MBEDTLS_PK_MAX_PUBKEY_RAW_LEN + 64];
		int certLen, keyLen;

		if (0 > (certLen = mbedtls_pk_write_pubkey_der(&ctx->ownCert.pk,
				certPub,sizeof(certPub))))
		{ LastError = certLen; return false; }

		if (0 > (keyLen = mbedtls_pk_write_pubkey_der(&ctx->ownKey,
				keyPub,sizeof(keyPub))))
		{ LastError = keyLen; return false; }

		if ((certLen != keyLen) ||
				(0 != memcmp(certPub + sizeof(certPub) - (size_t) certLen,
						keyPub + sizeof(keyPub) - (size_t) keyLen,
						(size_t) certLen)))
		{
			LastError = MBEDTLS_ERR_PK_TYPE_MISMATCH;
			return false;
		}
	}
#elif MBEDTLS_VERSION_MAJOR == 3
	if (0 != (LastError = mbedtls_pk_check_pair(&ctx->ownCert.pk,&ctx->ownKey,
			mbedtls_ctr_drbg_random,&ctx->drbg)))
	{ return false; }
#else
	if (0 != (LastError = mbedtls_pk_check_pair(&ctx->ownCert.pk,&ctx->ownKey)))
	{ return false; }
#endif

	if (0 != (LastError = mbedtls_ssl_conf_own_cert(&ctx->conf,&ctx->ownCert,&ctx->ownKey)))
	{ return false; }

	ctx->ownCertApplied = true;

	return true;
}

/****************************************************************/
/* TLSBackendLoadVerifyLocations: A positive result from the    */
/*   parse functions is the number of certificates that they    */
/*   could not read. It is not a failure code. As a result,     */
/*   this function reads each value except zero as a refusal to */
/*   load the given locations.                                  */
/****************************************************************/
bool TLSBackendLoadVerifyLocations(
		Environment *theEnv,
		void *vctx,
		const char *caFile,
		const char *caPath)
{
	struct mbedContext *ctx = (struct mbedContext *) vctx;
	bool loaded = false;

	// mbedTLS reads a trust store into the context now, and it keeps nothing
	// for a later rebuild. As a result, this backend allocates no memory of
	// its own here.
	(void) theEnv;

	if ((caFile != NULL) && (caFile[0] != '\0'))
	{
		if (0 != (LastError = mbedtls_x509_crt_parse_file(&ctx->ca,caFile)))
		{ return false; }
		loaded = true;
	}

	if ((caPath != NULL) && (caPath[0] != '\0'))
	{
		if (0 != (LastError = mbedtls_x509_crt_parse_path(&ctx->ca,caPath)))
		{ return false; }
		loaded = true;
	}

	if (! loaded) return false;

	mbedtls_ssl_conf_ca_chain(&ctx->conf,&ctx->ca,NULL);

	return true;
}

/*****************************************************************/
/* TLSBackendLoadSystemTrust: mbedTLS knows no location for a    */
/*   system trust store. As a result, this function tries the    */
/*   usual locations in sequence. The bundles come first,        */
/*   because a parse of one file costs less than a parse of a    */
/*   directory with several hundred files.                       */
/*****************************************************************/
bool TLSBackendLoadSystemTrust(
		void *vctx)
{
	struct mbedContext *ctx = (struct mbedContext *) vctx;
	unsigned i;

	static const char *bundles[] =
	  {
	   "/etc/ssl/certs/ca-certificates.crt",   // Debian, Ubuntu, Alpine
	   "/etc/pki/tls/certs/ca-bundle.crt",     // Fedora, RHEL
	   "/etc/ssl/ca-bundle.pem",               // SUSE
	   "/etc/ssl/cert.pem",                    // OpenBSD, macOS ports
	  };

	static const char *directories[] =
	  {
	   "/etc/ssl/certs",
	   "/etc/pki/tls/certs",
	  };

	for (i = 0; i < sizeof(bundles) / sizeof(bundles[0]); i++)
	{
		if (0 == mbedtls_x509_crt_parse_file(&ctx->ca,bundles[i]))
		{
			mbedtls_ssl_conf_ca_chain(&ctx->conf,&ctx->ca,NULL);
			return true;
		}
	}

	for (i = 0; i < sizeof(directories) / sizeof(directories[0]); i++)
	{
		// A parse of a directory gives the number of files that it could
		// not read. This is not a failure while the parse reads some
		// files.
		if (0 <= mbedtls_x509_crt_parse_path(&ctx->ca,directories[i]))
		{
			if (ctx->ca.version == 0) continue;
			mbedtls_ssl_conf_ca_chain(&ctx->conf,&ctx->ca,NULL);
			return true;
		}
	}

	LastError = MBEDTLS_ERR_X509_FILE_IO_ERROR;

	return false;
}

/****************************************************************/
/* TLSBackendUseCertificateFile: The function empties the chain */
/*   first.                                                     */
/*                                                              */
/*   mbedtls_x509_crt_parse_file adds to the data that the      */
/*   object holds and does not replace that data. As a result,  */
/*   a second certificate would leave a context that offers     */
/*   both of them. A new certificate must replace the           */
/*   certificate before it.                                     */
/*                                                              */
/*   To replace the contents is sufficient. The code gave the   */
/*   configuration the address of this object and not a copy of */
/*   it. The configuration already points at the data that this */
/*   function loads.                                            */
/****************************************************************/
bool TLSBackendUseCertificateFile(
		Environment *theEnv,
		void *vctx,
		const char *path)
{
	struct mbedContext *ctx = (struct mbedContext *) vctx;

	(void) theEnv;

	if (ctx->haveCert)
	{
		mbedtls_x509_crt_free(&ctx->ownCert);
		mbedtls_x509_crt_init(&ctx->ownCert);
		ctx->haveCert = false;
	}

	if (0 != (LastError = mbedtls_x509_crt_parse_file(&ctx->ownCert,path)))
	{ return false; }

	ctx->haveCert = true;

	return ApplyOwnCertificate(ctx);
}

bool TLSBackendUsePrivateKeyFile(
		Environment *theEnv,
		void *vctx,
		const char *path)
{
	struct mbedContext *ctx = (struct mbedContext *) vctx;
	int rv;

	(void) theEnv;

	// This is the same operation as for the certificate. A new key replaces
	// the key before it, and the configuration holds this object by address
	// and needs no new call.
	if (ctx->haveKey)
	{
		mbedtls_pk_free(&ctx->ownKey);
		mbedtls_pk_init(&ctx->ownKey);
		ctx->haveKey = false;
	}

	// As for mbedtls_pk_check_pair, only 3.x has the generator argument.
#if MBEDTLS_VERSION_MAJOR == 3
	rv = mbedtls_pk_parse_keyfile(&ctx->ownKey,path,NULL,
			mbedtls_ctr_drbg_random,&ctx->drbg);
#else
	rv = mbedtls_pk_parse_keyfile(&ctx->ownKey,path,NULL);
#endif

	if (rv != 0)
	{
		LastError = rv;
		return false;
	}

	ctx->haveKey = true;

	return ApplyOwnCertificate(ctx);
}

bool TLSBackendSetVerify(
		void *vctx,
		bool required)
{
	struct mbedContext *ctx = (struct mbedContext *) vctx;

	mbedtls_ssl_conf_authmode(&ctx->conf,
			required ? MBEDTLS_SSL_VERIFY_REQUIRED : MBEDTLS_SSL_VERIFY_NONE);

	return true;
}

/****************************************************************/
/* TLSBackendSetMinVersion: mbedTLS 2.x supports no version     */
/*   after TLS 1.2. As a result, this function refuses a        */
/*   request for TLS 1.3 there. It does not accept the request  */
/*   and then ignore it.                                        */
/****************************************************************/
bool TLSBackendSetMinVersion(
		void *vctx,
		int version)
{
	struct mbedContext *ctx = (struct mbedContext *) vctx;

	switch (version)
	{
		// For mbedTLS, DTLS 1.2 and TLS 1.2 are the same version number.
		// mbedTLS reads that number against the transport that the
		// configuration already holds.
		case TLS_VERSION_DTLS_1_2:
			if (! ctx->datagram) return false;
			/* falls through */
		case TLS_VERSION_1_2:
#if MBEDTLS_VERSION_MAJOR >= 3
			mbedtls_ssl_conf_min_tls_version(&ctx->conf,MBEDTLS_SSL_VERSION_TLS1_2);
#else
			mbedtls_ssl_conf_min_version(&ctx->conf,MBEDTLS_SSL_MAJOR_VERSION_3,
					MBEDTLS_SSL_MINOR_VERSION_3);
#endif
			return true;

		case TLS_VERSION_1_3:
#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
			mbedtls_ssl_conf_min_tls_version(&ctx->conf,MBEDTLS_SSL_VERSION_TLS1_3);
			return true;
#else
			return false;
#endif

		default:
			return false;
	}
}

/*****************************************************************/
/* VerifyPeerAddress: Applies the rule that only an address can  */
/*   match an address.                                           */
/*                                                               */
/*   mbedtls_ssl_set_hostname compares the given value with the  */
/*   names in the certificate, and it applies wildcards. The     */
/*   value 127.0.0.1 has the shape that a wildcard needs: a      */
/*   first label and then the remainder. As a result, a          */
/*   certificate with DNS:*.0.0.1 is correct for that address    */
/*   and for each other address with that end. Anyone can get    */
/*   such a name, and that certificate would then be correct for */
/*   hosts that its owner never saw.                             */
/*                                                               */
/*   As a result, when the name to verify is an address, the     */
/*   certificate must hold that address as an iPAddress entry.   */
/*   This function makes no rule less strict. It leaves the      */
/*   result of the library as it is, and it can only add an      */
/*   error to that result.                                       */
/*                                                               */
/*   mbedTLS 2.x lists only dNSName and OtherName entries. An    */
/*   address is never in that list, and this function refuses    */
/*   each address. That is the correct answer for a library that */
/*   cannot check an address, and it is the safe direction.      */
/*****************************************************************/
static int VerifyPeerAddress(
		void *data,
		mbedtls_x509_crt *crt,
		int depth,
		uint32_t *flags)
{
	struct mbedSession *sess = (struct mbedSession *) data;
	const mbedtls_x509_sequence *cur;

	// Only the leaf certificate holds the names of the peer.
	if ((depth != 0) || (sess == NULL) || (! sess->peerIsAddress)) return 0;

	for (cur = &crt->subject_alt_names; cur != NULL; cur = cur->next)
	{
		// Context-specific tag 7 is iPAddress. It holds four bytes or
		// sixteen bytes.
		if ((cur->buf.tag & 0xC0) != 0x80) continue;
		if ((cur->buf.tag & 0x1F) != 7) continue;

		if ((cur->buf.len == sess->addressLen) && (cur->buf.p != NULL) &&
		    (memcmp(cur->buf.p,sess->address,sess->addressLen) == 0))
		{ return 0; }
	}

	*flags |= MBEDTLS_X509_BADCERT_CN_MISMATCH;

	return 0;
}

/*==============================================================*/
/* RETRANSMISSION TIMING                                        */
/*                                                              */
/* A DTLS handshake sends a lost flight again, and mbedTLS does */
/* not run a handshake without a clock. It asks for the clock   */
/* through two callbacks. The set callback asks for a signal at */
/* two points, an intermediate point and a final point. The get */
/* callback tells which of the two points passed. A set call    */
/* with zero stops the timer.                                   */
/*==============================================================*/

static uint32_t ElapsedMs(
		const struct timespec *since)
{
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC,&now) != 0) return 0;

	return (uint32_t) (((now.tv_sec - since->tv_sec) * 1000) +
	                   ((now.tv_nsec - since->tv_nsec) / 1000000));
}

static void TimerSet(
		void *vsess,
		uint32_t intMs,
		uint32_t finMs)
{
	struct mbedSession *sess = (struct mbedSession *) vsess;

	sess->timerIntMs = intMs;
	sess->timerFinMs = finMs;

	if (finMs == 0)
	{
		sess->timerRunning = false;
		return;
	}

	sess->timerRunning = true;
	clock_gettime(CLOCK_MONOTONIC,&sess->timerStart);
}

/*****************************************************************/
/* TimerGet: The value -1 means that the timer stopped. The      */
/*   value 0 means that the code reached neither point. The      */
/*   value 1 means the intermediate point, and 2 means the       */
/*   final point. mbedTLS reads these exact values.              */
/*****************************************************************/
static int TimerGet(
		void *vsess)
{
	struct mbedSession *sess = (struct mbedSession *) vsess;
	uint32_t elapsed;

	if (! sess->timerRunning) return -1;

	elapsed = ElapsedMs(&sess->timerStart);

	if (elapsed >= sess->timerFinMs) return 2;
	if (elapsed >= sess->timerIntMs) return 1;

	return 0;
}

/****************************************************************/
/* TLSBackendNewSession: mbedtls_ssl_set_hostname does in one   */
/*   call what OpenSSL divides between SNI and the verification */
/*   parameters. As a result, this file cannot send a name      */
/*   without a check of that name.                              */
/****************************************************************/
void *TLSBackendNewSession(
		Environment *theEnv,
		void *vctx,
		int fd,
		bool asClient,
		const char *hostname)
{
	struct mbedContext *ctx = (struct mbedContext *) vctx;
	struct mbedSession *sess;

	sess = (struct mbedSession *) gm2(theEnv,sizeof(struct mbedSession));
	if (sess == NULL) return NULL;

	mbedtls_ssl_init(&sess->ssl);
	mbedtls_net_init(&sess->net);
	sess->net.fd = fd;
	sess->peerIsAddress = false;
	sess->addressLen = 0;
	sess->timerIntMs = 0;
	sess->timerFinMs = 0;
	sess->timerRunning = false;
	sess->clientIdLen = 0;

	if (0 != (LastError = mbedtls_ssl_setup(&sess->ssl,&ctx->conf)))
	{
		TLSBackendFreeSession(theEnv,sess);
		return NULL;
	}

	if (asClient && (hostname != NULL) && (hostname[0] != '\0'))
	{
		if (0 != (LastError = mbedtls_ssl_set_hostname(&sess->ssl,hostname)))
		{
			TLSBackendFreeSession(theEnv,sess);
			return NULL;
		}

		// An address needs a second check, because the name comparison
		// above accepts a wildcard for it.
		if (1 == inet_pton(AF_INET,hostname,sess->address))
		{
			sess->peerIsAddress = true;
			sess->addressLen = 4;
		}
		else if (1 == inet_pton(AF_INET6,hostname,sess->address))
		{
			sess->peerIsAddress = true;
			sess->addressLen = 16;
		}

		if (sess->peerIsAddress)
		{ mbedtls_ssl_set_verify(&sess->ssl,VerifyPeerAddress,sess); }
	}

	// The caller owns the socket. As a result, the code gives the send and
	// receive callbacks the descriptor and nothing more. The router closes
	// the socket.
	mbedtls_ssl_set_bio(&sess->ssl,&sess->net,mbedtls_net_send,mbedtls_net_recv,NULL);

	// DTLS needs this clock. Without it, mbedtls_ssl_handshake does not run,
	// because it cannot decide when a flight is lost.
	if (ctx->datagram)
	{
		mbedtls_ssl_set_timer_cb(&sess->ssl,sess,TimerSet,TimerGet);
		mbedtls_ssl_set_mtu(&sess->ssl,TLS_DEFAULT_LINK_MTU);
	}

	return sess;
}

void TLSBackendFreeSession(
		Environment *theEnv,
		void *vsess)
{
	struct mbedSession *sess = (struct mbedSession *) vsess;

	if (sess == NULL) return;

	mbedtls_ssl_free(&sess->ssl);

	rm(theEnv,sess,sizeof(struct mbedSession));
}

int TLSBackendHandshake(
		void *vsess)
{
	struct mbedSession *sess = (struct mbedSession *) vsess;
	int rv;

	rv = mbedtls_ssl_handshake(&sess->ssl);

	if (rv == 0) return TLS_HANDSHAKE_DONE;

	if ((rv == MBEDTLS_ERR_SSL_WANT_READ) || (rv == MBEDTLS_ERR_SSL_WANT_WRITE))
	{ return TLS_HANDSHAKE_AGAIN; }

	// The server sent a cookie and waits for the cookie to come back.
	// mbedTLS reports this as an error from the handshake. The code must
	// reset the session before it can read the reply. A session that the code
	// resets no longer has the address that the cookie belongs to. As a
	// result, the code gives the address again from the value that it kept at
	// the start of the exchange.
	if (rv == MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED)
	{
		if (0 != (LastError = mbedtls_ssl_session_reset(&sess->ssl)))
		{ return TLS_HANDSHAKE_FAIL; }

		if (sess->clientIdLen > 0)
		{
			if (0 != (LastError = mbedtls_ssl_set_client_transport_id(&sess->ssl,
					sess->clientId,sess->clientIdLen)))
			{ return TLS_HANDSHAKE_FAIL; }
		}

		return TLS_HANDSHAKE_LISTEN_AGAIN;
	}

	RememberError(rv);

	return TLS_HANDSHAKE_FAIL;
}

long TLSBackendRead(
		void *vsess,
		void *buf,
		size_t len)
{
	struct mbedSession *sess = (struct mbedSession *) vsess;
	int n;

	if (len > INT_MAX) len = INT_MAX;

	n = mbedtls_ssl_read(&sess->ssl,(unsigned char *) buf,len);
	if (n > 0) return (long) n;

	switch (n)
	{
		case 0:
		case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
			return TLS_RESULT_EOF;

		case MBEDTLS_ERR_SSL_WANT_READ:
		case MBEDTLS_ERR_SSL_WANT_WRITE:
			return TLS_RESULT_AGAIN;

		case MBEDTLS_ERR_NET_CONN_RESET:
			// This is a peer that went away without a close_notify. This
			// is usual on a real network. If the code gave a failure here,
			// usual traffic would look like an error.
			return TLS_RESULT_EOF;

		default:
			RememberError(n);
			return TLS_RESULT_FAIL;
	}
}

long TLSBackendWrite(
		void *vsess,
		const void *buf,
		size_t len)
{
	struct mbedSession *sess = (struct mbedSession *) vsess;
	int n;

	if (len > INT_MAX) len = INT_MAX;

	n = mbedtls_ssl_write(&sess->ssl,(const unsigned char *) buf,len);
	if (n > 0) return (long) n;

	switch (n)
	{
		case MBEDTLS_ERR_SSL_WANT_READ:
		case MBEDTLS_ERR_SSL_WANT_WRITE:
			return TLS_RESULT_AGAIN;

		case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
			return TLS_RESULT_EOF;

		default:
			RememberError(n);
			return TLS_RESULT_FAIL;
	}
}

int TLSBackendPending(
		void *vsess)
{
	struct mbedSession *sess = (struct mbedSession *) vsess;
	size_t avail;

	avail = mbedtls_ssl_get_bytes_avail(&sess->ssl);

	return (avail > INT_MAX) ? INT_MAX : (int) avail;
}

/****************************************************************/
/* TLSBackendShutdown: This function only sends the             */
/*   close_notify of this end. mbedTLS has no call to wait for  */
/*   the close_notify of the peer, and the OpenSSL backend      */
/*   refuses to wait for it on purpose.                         */
/****************************************************************/
bool TLSBackendShutdown(
		void *vsess)
{
	struct mbedSession *sess = (struct mbedSession *) vsess;
	int rv;

	rv = mbedtls_ssl_close_notify(&sess->ssl);

	return (rv == 0) ||
	       (rv == MBEDTLS_ERR_SSL_WANT_READ) ||
	       (rv == MBEDTLS_ERR_SSL_WANT_WRITE);
}

const char *TLSBackendCipher(
		void *vsess)
{
	struct mbedSession *sess = (struct mbedSession *) vsess;
	const char *name = mbedtls_ssl_get_ciphersuite(&sess->ssl);

	return (name != NULL) ? name : "";
}

const char *TLSBackendProtocol(
		void *vsess)
{
	struct mbedSession *sess = (struct mbedSession *) vsess;
	const char *name = mbedtls_ssl_get_version(&sess->ssl);

	return (name != NULL) ? name : "";
}

bool TLSBackendVerifyOK(
		void *vsess)
{
	struct mbedSession *sess = (struct mbedSession *) vsess;

	return mbedtls_ssl_get_verify_result(&sess->ssl) == 0;
}

/*****************************************************************/
/* TLSBackendVerifyDescription: The result is a set of bits and  */
/*   not one code. As a result, it can give more than one cause  */
/*   at the same time. verify_info writes one line for each      */
/*   cause. This function removes the last newline, because the  */
/*   caller gives this text to CLIPS as a string and does not    */
/*   print it.                                                   */
/*****************************************************************/
bool TLSBackendVerifyDescription(
		void *vsess,
		char *out,
		size_t outLen)
{
	struct mbedSession *sess = (struct mbedSession *) vsess;
	uint32_t flags;
	int len;
	size_t i;

	flags = mbedtls_ssl_get_verify_result(&sess->ssl);
	if (flags == 0) return false;

	len = mbedtls_x509_crt_verify_info(out,outLen,"",flags);
	if (len <= 0) return false;

	for (i = strlen(out); (i > 0) && ((out[i - 1] == '\n') || (out[i - 1] == '\r')); i--)
	{ out[i - 1] = '\0'; }

	return out[0] != '\0';
}

bool TLSBackendPeerSubject(
		void *vsess,
		char *out,
		size_t outLen)
{
	struct mbedSession *sess = (struct mbedSession *) vsess;
	const mbedtls_x509_crt *cert;

	cert = mbedtls_ssl_get_peer_cert(&sess->ssl);
	if (cert == NULL) return false;

	if (0 > mbedtls_x509_dn_gets(out,outLen,&cert->subject)) return false;

	return true;
}

/****************************************************************/
/* TLSBackendReportError: Writes the text of the most recent    */
/*   error code. The function clears the field. As a result, a  */
/*   later failure with no detail of its own cannot report this */
/*   error a second time.                                       */
/****************************************************************/
void TLSBackendReportError(
		Environment *theEnv,
		const char *what)
{
	char buffer[256];
	const char *text = NULL;

	if (LastError != 0)
	{
		mbedtls_strerror(LastError,buffer,sizeof(buffer));
		text = buffer;
		LastError = 0;
	}

	TLSReportBackendError(theEnv,what,text);
}


/*=================*/
/* DATAGRAMS       */
/*=================*/

bool TLSBackendSupportsDTLS(
		bool asServer)
{
	return true;
}

/*****************************************************************/
/* TLSBackendDTLSRestartable: The answer is no. The sessions     */
/*   share the cookie secret, and a new session would send the   */
/*   same cookies. But the address of the exchange is in the     */
/*   session. To discard that address means that the code must   */
/*   read it again for no gain. To keep the session is also one  */
/*   allocation less in each cycle of a poll loop.               */
/*****************************************************************/
bool TLSBackendDTLSRestartable(void)
{
	return false;
}

/**************************************************************************/
/* TLSBackendDTLSListen: Finds the address of the client, but does not    */
/*   reply yet.                                                           */
/*                                                                        */
/*   mbedTLS runs the cookie exchange inside the handshake and not in a   */
/*   call of its own. It needs the address of the client before the       */
/*   handshake, and it reports the HelloVerifyRequest as an error from    */
/*   mbedtls_ssl_handshake after it. As a result, this function only      */
/*   looks at the data on the socket, does not remove that data, and      */
/*   gives the address to the session and to the caller.                  */
/*                                                                        */
/*   MSG_PEEK makes this possible. The ClientHello must still be on the   */
/*   socket when the handshake runs, or the handshake has nothing to      */
/*   answer.                                                              */
/**************************************************************************/
int TLSBackendDTLSListen(
		void *vsess,
		struct sockaddr_storage *peer,
		socklen_t *peerLen)
{
	struct mbedSession *sess = (struct mbedSession *) vsess;
	unsigned char scratch[1];
	socklen_t len = (socklen_t) sizeof(*peer);
	ssize_t n;

	memset(peer,0,sizeof(*peer));

	n = recvfrom(sess->net.fd,scratch,sizeof(scratch),MSG_PEEK,
			(struct sockaddr *) peer,&len);

	if (n < 0)
	{
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR))
		{ return TLS_HANDSHAKE_AGAIN; }

		return TLS_HANDSHAKE_FAIL;
	}

	*peerLen = len;

	// This is the address in the format that the cookie calculation uses.
	// It is the address without the port, because the port of a client
	// behind an address translator can change between the two flights.
	switch (peer->ss_family)
	{
		case AF_INET:
			sess->clientIdLen = 4;
			memcpy(sess->clientId,
					&((struct sockaddr_in *) peer)->sin_addr,sess->clientIdLen);
			break;

		case AF_INET6:
			sess->clientIdLen = 16;
			memcpy(sess->clientId,
					&((struct sockaddr_in6 *) peer)->sin6_addr,sess->clientIdLen);
			break;

		default:
			return TLS_HANDSHAKE_FAIL;
	}

	if (0 != (LastError = mbedtls_ssl_set_client_transport_id(&sess->ssl,
			sess->clientId,sess->clientIdLen)))
	{ return TLS_HANDSHAKE_FAIL; }

	return TLS_HANDSHAKE_DONE;
}

bool TLSBackendSetSocket(
		void *vsess,
		int fd)
{
	struct mbedSession *sess = (struct mbedSession *) vsess;

	// The descriptor is all that mbedTLS knows about the transport. The send
	// and receive callbacks take the descriptor from this field.
	sess->net.fd = fd;

	return true;
}

/*****************************************************************/
/* TLSBackendDTLSTimeout: The time until the code must send the  */
/*   flight again, from the clock that the handshake set. Zero   */
/*   means that the code must send it now. The value -1 means    */
/*   that no timer is in operation.                              */
/*****************************************************************/
long TLSBackendDTLSTimeout(
		void *vsess)
{
	struct mbedSession *sess = (struct mbedSession *) vsess;
	uint32_t elapsed;

	if (! sess->timerRunning) return -1;

	elapsed = ElapsedMs(&sess->timerStart);

	if (elapsed >= sess->timerFinMs) return 0;

	return (long) (sess->timerFinMs - elapsed);
}

/*****************************************************************/
/* TLSBackendDTLSHandleTimeout: There is nothing to do here.     */
/*   mbedTLS sends the flight again from inside                  */
/*   mbedtls_ssl_handshake when its timer ends. As a result, the */
/*   next handshake call of the caller is the retransmission.    */
/*   This function reports success, because a caller must not    */
/*   think that the retransmission never happens.                */
/*****************************************************************/
int TLSBackendDTLSHandleTimeout(
		void *vsess)
{
	return TLS_HANDSHAKE_DONE;
}

bool TLSBackendSetMTU(
		void *vsess,
		int mtu)
{
	struct mbedSession *sess = (struct mbedSession *) vsess;

	mbedtls_ssl_set_mtu(&sess->ssl,(uint16_t) mtu);

	return true;
}

int TLSBackendMaxPayload(
		void *vsess)
{
	struct mbedSession *sess = (struct mbedSession *) vsess;
	int size;

	size = mbedtls_ssl_get_max_out_record_payload(&sess->ssl);

	return (size > 0) ? size : 0;
}

#endif /* USE_TLS */
