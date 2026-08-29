/*******************************************************/
/*      "C" Language Integrated Production System      */
/*                                                     */
/*            CLIPS Version ?.??  08/09/26             */
/*                                                     */
/*              TLS BACKEND: OPENSSL FAMILY            */
/*******************************************************/

/***********************************************************************/
/* Purpose: This file implements the backend interface in socktlsbe.h  */
/*   with the OpenSSL API. Four libraries use this file: OpenSSL       */
/*   itself, LibreSSL and BoringSSL as forks of OpenSSL, and wolfSSL   */
/*   through its OpenSSL compatibility layer. All the differences      */
/*   between them are at the top of this file.                         */
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

#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>

#include "setup.h"

#include "constant.h"
#include "envrnmnt.h"
#include "prntutil.h"
#include "router.h"

#include "socktlsbe.h"

#ifdef USE_TLS

/*==============================================================*/
/* Backend headers. wolfSSL sets its headers from the flags of  */
/* its own build, and options.h holds those flags. If you       */
/* include a different wolfSSL header first, the flags stay     */
/* unset. The result is a failure at run time with no message,  */
/* and not a compile error. options.h must always come first.   */
/* The headers cannot make that decision. As a result, the      */
/* makefile gives the backend as -DTLS_BACKEND_WOLFSSL. The     */
/* other three libraries need no such define, because they      */
/* identify themselves below, after the include lines.          */
/*==============================================================*/

#ifdef TLS_BACKEND_WOLFSSL
#include <wolfssl/options.h>
#include <wolfssl/openssl/ssl.h>
#include <wolfssl/openssl/err.h>
#else
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/x509v3.h>
#endif

/*==============================================================*/
/* Which of the four libraries this is. LIBRESSL_VERSION_NUMBER */
/* and OPENSSL_IS_BORINGSSL exist only after the headers, and   */
/* this test comes after them.                                  */
/*==============================================================*/

#if defined(TLS_BACKEND_WOLFSSL)
#define TLS_BACKEND_NAME    "wolfssl"
#define TLS_BACKEND_VERSION LIBWOLFSSL_VERSION_STRING
#elif defined(OPENSSL_IS_BORINGSSL)
#define TLS_BACKEND_NAME    "boringssl"
#define TLS_BACKEND_VERSION OPENSSL_VERSION_TEXT
#elif defined(LIBRESSL_VERSION_NUMBER)
#define TLS_BACKEND_NAME    "libressl"
#define TLS_BACKEND_VERSION OPENSSL_VERSION_TEXT
#else
#define TLS_BACKEND_NAME    "openssl"
#define TLS_BACKEND_VERSION OPENSSL_VERSION_TEXT
#endif

/*==============================================================*/
/* Differences in the available functions.                      */
/*                                                              */
/* Two of these libraries give an OPENSSL_VERSION_NUMBER that   */
/* gives neither their identity nor the functions that they     */
/* implement. LibreSSL gives 0x20000000L and BoringSSL gives    */
/* 1.1.1. Each test here removes those two libraries before it  */
/* uses that number.                                            */
/*==============================================================*/

#if defined(TLS_BACKEND_WOLFSSL)
#define TLSGetPeerCertificate(ssl) wolfSSL_get_peer_certificate(ssl)
#elif defined(OPENSSL_IS_BORINGSSL) || defined(LIBRESSL_VERSION_NUMBER)
#define TLSGetPeerCertificate(ssl) SSL_get_peer_certificate(ssl)
#elif OPENSSL_VERSION_NUMBER >= 0x30000000L
#define TLSGetPeerCertificate(ssl) SSL_get1_peer_certificate(ssl)
#else
#define TLSGetPeerCertificate(ssl) SSL_get_peer_certificate(ssl)
#endif

/*==============================================================*/
/* DTLS. Three of the four libraries can do DTLS, in two        */
/* different manners.                                           */
/*                                                              */
/*   OpenSSL,   The reference method: a datagram BIO,           */
/*   LibreSSL   DTLSv1_listen, and the cookie callbacks that    */
/*              this file supplies. The two libraries differ    */
/*              only in the names of the listen call and of     */
/*              the record size calls.                          */
/*                                                              */
/*   wolfSSL    The other method. Its compatibility layer does  */
/*              not have DTLSv1_listen, and its own API does    */
/*              the same work differently. It has no BIO, and   */
/*              it takes a descriptor and a peer address in     */
/*              place of one. It has no cookie callbacks,       */
/*              because wolfDTLS_accept_stateless makes and     */
/*              checks the cookie itself.                       */
/*                                                              */
/*   BoringSSL  This file cannot do DTLS with BoringSSL. It has */
/*              no BIO_new_dgram, and there is no datagram      */
/*              transport to put a session on. As a result it   */
/*              has no client part and no server part. To add   */
/*              them needs a file of BIO code, and not a        */
/*              define.                                         */
/*                                                              */
/* wolfSSL is also the library where its own build selects the  */
/* answer. A default build has DTLS off, and WOLFSSL_DTLS is    */
/* then not defined. Nothing is absent at link time.            */
/* tests/provision.sh builds wolfSSL with -DWOLFSSL_DTLS=yes,   */
/* but a different build can be without it.                     */
/*==============================================================*/

#if defined(TLS_BACKEND_WOLFSSL)
#ifdef WOLFSSL_DTLS
#define TLS_HAS_DTLS 1
#else
#define TLS_HAS_DTLS 0
#endif
#elif defined(OPENSSL_IS_BORINGSSL)
#define TLS_HAS_DTLS 0
#else
#define TLS_HAS_DTLS 1
#endif

/* This constant is true if this file must do the cookie exchange. wolfSSL
   does its own exchange inside wolfDTLS_accept_stateless, and it gives no
   callbacks for one. */
#if TLS_HAS_DTLS && ! defined(TLS_BACKEND_WOLFSSL)
#define TLS_DTLS_OWN_COOKIES 1
#else
#define TLS_DTLS_OWN_COOKIES 0
#endif

/* wolfSSL uses a different name for this question, and its compatibility
   layer does not have SSL_is_dtls. */
#ifdef TLS_BACKEND_WOLFSSL
#define TLSIsDTLS(ssl) (wolfSSL_dtls(ssl) == 1)
#else
#define TLSIsDTLS(ssl) (SSL_is_dtls(ssl) != 0)
#endif

/* LibreSSL does not have this bit, and OpenSSL before 1.1.0h does not have
   it. The value is one bit of an option mask, and a zero in the mask changes
   nothing. wolfSSL and BoringSSL both refuse renegotiation unless their build
   permits it, and the flag has no effect there. */
#ifndef SSL_OP_NO_RENEGOTIATION
#define SSL_OP_NO_RENEGOTIATION 0
#endif

/* Server Name Indication, the name that the code compares the peer
   certificate with, and the wildcard rule for that comparison. The first two
   calls of wolfSSL are too different for a simple rename, and they need their
   own form. wolfSSL has no equivalent of the third call. */
#ifdef TLS_BACKEND_WOLFSSL
#define TLSSetSNI(ssl,host) \
   (wolfSSL_UseSNI((ssl),WOLFSSL_SNI_HOST_NAME,(host),(unsigned short) strlen(host)) == WOLFSSL_SUCCESS)
#define TLSSetVerifyHost(ssl,host) \
   (wolfSSL_check_domain_name((ssl),(host)) == WOLFSSL_SUCCESS)
#define TLSSetHostFlags(ssl) ((void) 0)
/* The wolfSSL name for the address comparison. Its compatibility header
   already maps that name to the name below. wolfSSL_check_domain_name
   compares names only. It refuses an address that the certificate does name.
   As a result, the two conditions cannot share one call here, and they cannot
   share one call in the other backends. */
#define TLSSetVerifyAddress(ssl,ip) \
   (wolfSSL_X509_VERIFY_PARAM_set1_ip_asc(wolfSSL_get0_param(ssl),(ip)) == WOLFSSL_SUCCESS)
#else
#define TLSSetSNI(ssl,host)        (SSL_set_tlsext_host_name((ssl),(host)) == 1)
#define TLSSetVerifyHost(ssl,host) (SSL_set1_host((ssl),(host)) == 1)
#define TLSSetHostFlags(ssl)       SSL_set_hostflags((ssl),X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS)
/* Compares the address with the iPAddress entries of the certificate. This is
   a different comparison from the name comparison, and not a special
   condition of it. No wildcard applies, and the code never reads the subject
   common name. */
#define TLSSetVerifyAddress(ssl,ip) \
   (X509_VERIFY_PARAM_set1_ip_asc(SSL_get0_param(ssl),(ip)) == 1)
#endif

/*=========================================================*/
/* A context is an SSL_CTX and a session is an SSL. As a   */
/* result, the opaque pointers of the interface need no    */
/* wrapper structure here. The backends that keep state of */
/* their own are why those pointers are opaque.            */
/*=========================================================*/

#if TLS_DTLS_OWN_COOKIES

/*==============================================================*/
/* COOKIES                                                      */
/*                                                              */
/* A DTLS server must do no work for a ClientHello until it     */
/* knows that the source address is correct, because a datagram */
/* can give any address. The cookie exchange does this. The     */
/* server answers the first ClientHello with a                  */
/* HelloVerifyRequest that holds a cookie. Only a client that   */
/* can receive at the address that it gave can send that cookie */
/* back.                                                        */
/*                                                              */
/* The cookie is an HMAC of the peer address under a secret     */
/* that this process makes one time and never sends. This is    */
/* what keeps the exchange stateless. The server keeps no data  */
/* between the two flights, because it can calculate the cookie */
/* again and compare it.                                        */
/*==============================================================*/

#define TLS_COOKIE_SECRET_LEN 16

static unsigned char CookieSecret[TLS_COOKIE_SECRET_LEN];
static bool CookieSecretReady = false;

static bool EnsureCookieSecret(void)
{
	if (CookieSecretReady) return true;

	if (RAND_bytes(CookieSecret,sizeof(CookieSecret)) != 1) return false;

	CookieSecretReady = true;

	return true;
}

/*****************************************************************/
/* CookieForPeer: The cookie for the address that sent the       */
/*   current ClientHello. OpenSSL sets the peer address of the   */
/*   BIO before it calls this function. As a result, the address */
/*   is available and the code needs no table.                   */
/*****************************************************************/
static int CookieForPeer(
		SSL *ssl,
		unsigned char *cookie,
		unsigned int *cookieLen)
{
	struct sockaddr_storage peer;
	unsigned int len = 0;

	if (! EnsureCookieSecret()) return 0;

	// The code sets the structure to zero first. The HMAC covers the full
	// structure, and the bytes after the address must be the same on the two
	// flights.
	memset(&peer,0,sizeof(peer));

	if (BIO_dgram_get_peer(SSL_get_rbio(ssl),&peer) <= 0) return 0;

	if (HMAC(EVP_sha256(),CookieSecret,sizeof(CookieSecret),
			(const unsigned char *) &peer,sizeof(peer),
			cookie,&len) == NULL)
	{ return 0; }

	*cookieLen = len;

	return 1;
}

static int GenerateCookie(
		SSL *ssl,
		unsigned char *cookie,
		unsigned int *cookieLen)
{
	return CookieForPeer(ssl,cookie,cookieLen);
}

static int VerifyCookie(
		SSL *ssl,
		const unsigned char *cookie,
		unsigned int cookieLen)
{
	unsigned char expected[EVP_MAX_MD_SIZE];
	unsigned int expectedLen = 0;

	if (! CookieForPeer(ssl,expected,&expectedLen)) return 0;

	if (cookieLen != expectedLen) return 0;

	// The comparison takes the same time for each value, because it compares
	// a value from an attacker with a value that the attacker tries to find.
	return CRYPTO_memcmp(cookie,expected,cookieLen) == 0;
}

#endif /* TLS_DTLS_OWN_COOKIES */

#if TLS_HAS_DTLS

/*****************************************************************/
/* SetLinkMTU and DataMTU: The quantity of data in one record.   */
/*                                                               */
/*   OpenSSL does the two parts of this work. The code gives it  */
/*   the link MTU, and OpenSSL calculates how much application   */
/*   data remains after the record header and the overhead of    */
/*   the cipher.                                                 */
/*                                                               */
/*   LibreSSL accepts the link MTU but has no call to read the   */
/*   result back. This file calculates the result, and it uses a */
/*   large value for the overhead. The overhead is a function of */
/*   the cipher suite. An estimate that is too large costs some  */
/*   bytes in each record. An estimate that is too small gives a */
/*   record that the library cannot send. The number below       */
/*   covers an IPv6 header, UDP, the DTLS record header, an      */
/*   explicit IV, a SHA-384 tag and block padding. Each item is  */
/*   the largest possible.                                       */
/*****************************************************************/

#if defined(TLS_BACKEND_WOLFSSL)

/* wolfSSL_dtls_set_mtu exists only in a build with WOLFSSL_DTLS_MTU or
   WOLFSSL_SCTP. A default build has neither of them, and its cmake options
   cannot set them. As a result, dtls-set-mtu reports that this backend does
   not accept an MTU, and the library keeps its own default. To read the
   record size back needs no such build flag. */
static bool SetLinkMTU(
		SSL *ssl,
		int mtu)
{
#if defined(WOLFSSL_DTLS_MTU) || defined(WOLFSSL_SCTP)
	return wolfSSL_dtls_set_mtu(ssl,(unsigned short) mtu) == WOLFSSL_SUCCESS;
#else
	return false;
#endif
}

static int DataMTU(
		SSL *ssl)
{
	int size = wolfSSL_GetMaxOutputSize(ssl);

	return (size > 0) ? size : 0;
}

#elif defined(LIBRESSL_VERSION_NUMBER)

#define TLS_DGRAM_OVERHEAD 200

/* Where the code keeps the link MTU, because LibreSSL has no call to read
   it. The code puts an int in the pointer field and allocates no memory. As
   a result, nothing needs a free when the session goes away. */
static int LinkMTUIndex = -1;

static bool SetLinkMTU(
		SSL *ssl,
		int mtu)
{
	if (LinkMTUIndex < 0)
	{
		LinkMTUIndex = SSL_get_ex_new_index(0,NULL,NULL,NULL,NULL);
		if (LinkMTUIndex < 0) return false;
	}

	if (SSL_set_mtu(ssl,(long) mtu) <= 0) return false;

	return SSL_set_ex_data(ssl,LinkMTUIndex,(void *) (intptr_t) mtu) == 1;
}

static int DataMTU(
		SSL *ssl)
{
	int mtu;

	if (LinkMTUIndex < 0) return 0;

	mtu = (int) (intptr_t) SSL_get_ex_data(ssl,LinkMTUIndex);

	if (mtu <= TLS_DGRAM_OVERHEAD) return 0;

	return mtu - TLS_DGRAM_OVERHEAD;
}

#else

static bool SetLinkMTU(
		SSL *ssl,
		int mtu)
{
	return DTLS_set_link_mtu(ssl,(long) mtu) == 1;
}

static int DataMTU(
		SSL *ssl)
{
	return (int) DTLS_get_data_mtu(ssl);
}

#endif /* LIBRESSL_VERSION_NUMBER */

/*****************************************************************/
/* ListenForClient: The stateless part of the acceptance of a    */
/*   DTLS association. LibreSSL keeps the older form, which      */
/*   takes a sockaddr where OpenSSL takes a BIO_ADDR. The code   */
/*   reads the address from the BIO in the two conditions. As a  */
/*   result, the argument is only a location for data that this  */
/*   file does not use.                                          */
/*****************************************************************/
static int ListenForClient(
		SSL *ssl)
{
#if defined(TLS_BACKEND_WOLFSSL)
	// This call does the full exchange itself, and the cookie with it. It
	// reports success only after a ClientHello comes back with a correct
	// cookie.
	return (wolfDTLS_accept_stateless(ssl) == WOLFSSL_SUCCESS) ? 1 : 0;
#elif defined(LIBRESSL_VERSION_NUMBER)
	struct sockaddr_storage unused;

	memset(&unused,0,sizeof(unused));

	return DTLSv1_listen(ssl,(struct sockaddr *) &unused);
#else
	BIO_ADDR *addr;
	int rv;

	addr = BIO_ADDR_new();
	if (addr == NULL) return -1;

	rv = DTLSv1_listen(ssl,addr);

	BIO_ADDR_free(addr);

	return rv;
#endif
}

/*****************************************************************/
/* MarkConnected: Tells a datagram BIO that its socket now sends */
/*   to one address only. The BIO then sends to the destination  */
/*   of the socket and does not give an address in each write.   */
/*                                                               */
/*   The function does nothing if the socket has no peer. The    */
/*   listen socket of a server has no peer yet, and that is the  */
/*   correct condition until the server verifies a cookie.       */
/*****************************************************************/
#ifndef TLS_BACKEND_WOLFSSL
static void MarkConnected(
		BIO *bio,
		int fd)
{
	struct sockaddr_storage peer;
	socklen_t peerLen = sizeof(peer);

	memset(&peer,0,sizeof(peer));

	if (getpeername(fd,(struct sockaddr *) &peer,&peerLen) != 0) return;

	BIO_ctrl(bio,BIO_CTRL_DGRAM_SET_CONNECTED,0,&peer);
}
#endif /* ! TLS_BACKEND_WOLFSSL */

/*****************************************************************/
/* AttachDatagramBIO: Gives a session a datagram transport on    */
/*   the descriptor, with a fixed record size.                   */
/*****************************************************************/
static bool AttachDatagramBIO(
		SSL *ssl,
		int fd)
{
#ifdef TLS_BACKEND_WOLFSSL
	struct sockaddr_storage peer;
	socklen_t peerLen = sizeof(peer);

	// There is no BIO here. wolfSSL takes the descriptor directly and keeps
	// the peer address itself. It sends to that address.
	if (SSL_set_fd(ssl,fd) != 1) return false;

	// The code must tell wolfSSL that the socket is non-blocking, because
	// wolfSSL does not find this itself. A DTLS session that thinks that its
	// socket blocks reads a would-block condition as a lost flight and stops.
	// The handshake then fails on the first call and does not wait for the
	// peer. The other libraries here find the condition themselves. This one
	// needs the data, and the code asks the descriptor for it.
	{
		int flags = fcntl(fd,F_GETFL,0);

		if ((flags != -1) && ((flags & O_NONBLOCK) != 0))
		{ wolfSSL_dtls_set_using_nonblock(ssl,1); }
	}

	memset(&peer,0,sizeof(peer));

	if (getpeername(fd,(struct sockaddr *) &peer,&peerLen) == 0)
	{
		if (wolfSSL_dtls_set_peer(ssl,&peer,(unsigned int) peerLen) != WOLFSSL_SUCCESS)
		{ return false; }
	}

	return true;
#else
	BIO *bio;

	bio = BIO_new_dgram(fd,BIO_NOCLOSE);
	if (bio == NULL) return false;

	MarkConnected(bio,fd);

	// These two lines are one decision: stop the questions to the socket
	// about the path MTU, and give the value to use in place of it.
	SSL_set_options(ssl,SSL_OP_NO_QUERY_MTU);

	// This call takes the BIO for the two directions, and no code here frees
	// the BIO. SSL_free frees it, and a subsequent SSL_set_bio also frees
	// it.
	SSL_set_bio(ssl,bio,bio);

	SetLinkMTU(ssl,TLS_DEFAULT_LINK_MTU);

	return true;
#endif
}

#endif /* TLS_HAS_DTLS */


/***********************************************************/
/* TLSBackendStartup: Starts the library. It is safe to    */
/*   call this function more than one time, on each        */
/*   backend in this family.                               */
/***********************************************************/
bool TLSBackendStartup(void)
{
#if defined(TLS_BACKEND_WOLFSSL)
	return wolfSSL_Init() == WOLFSSL_SUCCESS;
#elif defined(OPENSSL_IS_BORINGSSL)
	CRYPTO_library_init();
	return true;
#else
	return OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS,NULL) == 1;
#endif
}

const char *TLSBackendName(void)
{
	return TLS_BACKEND_NAME;
}

const char *TLSBackendVersion(void)
{
	return TLS_BACKEND_VERSION;
}

/***********************************************************************/
/* TLSBackendReportError: Writes the error queue to STDERR. OpenSSL    */
/*   collects errors in a stack. This function reports each error in   */
/*   the queue and leaves the queue empty for the next operation.      */
/***********************************************************************/
void TLSBackendReportError(
		Environment *theEnv,
		const char *what)
{
	unsigned long code;
	char buffer[256];
	bool reported = false;

	while ((code = ERR_get_error()) != 0)
	{
		ERR_error_string_n(code,buffer,sizeof(buffer));
		TLSReportBackendError(theEnv,what,buffer);
		reported = true;
	}

	// A failure with an empty queue must still write a message. The shared
	// function does this when it gets no detail.
	if (! reported)
	{ TLSReportBackendError(theEnv,what,NULL); }
}

/*=================*/
/* CONTEXTS        */
/*=================*/

void *TLSBackendNewContext(
		Environment *theEnv,
		bool asClient,
		bool datagram)
{
	SSL_CTX *ctx;

	// This backend keeps no memory of its own. Each object here belongs to
	// OpenSSL, which frees it with a call of its own.
	(void) theEnv;

#if TLS_HAS_DTLS
	if (datagram)
	{
		ctx = SSL_CTX_new(asClient ? DTLS_client_method() : DTLS_server_method());
		if (ctx == NULL) return NULL;

		// DTLS 1.0 has the weaknesses of TLS 1.1, and this file does not
		// offer it.
		SSL_CTX_set_min_proto_version(ctx,DTLS1_2_VERSION);

#if TLS_DTLS_OWN_COOKIES
		if (! asClient)
		{
			// Without this option the handshake continues directly from
			// the first ClientHello. The server then does work for an
			// address that no code checked.
			SSL_CTX_set_options(ctx,SSL_OP_COOKIE_EXCHANGE);
			SSL_CTX_set_cookie_generate_cb(ctx,GenerateCookie);
			SSL_CTX_set_cookie_verify_cb(ctx,VerifyCookie);
		}
#endif
	}
	else
#endif
	{
		if (datagram) return NULL;

		ctx = SSL_CTX_new(asClient ? TLS_client_method() : TLS_server_method());
		if (ctx == NULL) return NULL;

		SSL_CTX_set_min_proto_version(ctx,TLS1_2_VERSION);
	}

	SSL_CTX_set_options(ctx,SSL_OP_NO_RENEGOTIATION);

	// Verification of the peer is the default for a client. As a result, a
	// program that does not ask for verification cannot make a connection
	// with no authentication. A server has no peer certificate to check
	// unless the program asks for one.
	if (asClient)
	{ SSL_CTX_set_verify(ctx,SSL_VERIFY_PEER,NULL); }

	return ctx;
}

void TLSBackendFreeContext(
		Environment *theEnv,
		void *ctx)
{
	(void) theEnv;

	if (ctx != NULL) SSL_CTX_free((SSL_CTX *) ctx);
}

bool TLSBackendLoadVerifyLocations(
		Environment *theEnv,
		void *ctx,
		const char *caFile,
		const char *caPath)
{
	(void) theEnv;

	// OpenSSL reads an empty string as a path to try, and not as an absent
	// value. As a result, the code must give it NULL for an empty string.
	if ((caFile != NULL) && (caFile[0] == '\0')) caFile = NULL;
	if ((caPath != NULL) && (caPath[0] == '\0')) caPath = NULL;

	if ((caFile == NULL) && (caPath == NULL)) return false;

	return SSL_CTX_load_verify_locations((SSL_CTX *) ctx,caFile,caPath) == 1;
}

bool TLSBackendLoadSystemTrust(
		void *ctx)
{
	return SSL_CTX_set_default_verify_paths((SSL_CTX *) ctx) == 1;
}

bool TLSBackendUseCertificateFile(
		Environment *theEnv,
		void *ctx,
		const char *file)
{
	(void) theEnv;

	// The code uses the chain function and not the one-certificate
	// function. A leaf certificate alone is sufficient only if the peer
	// already holds each intermediate certificate.
	return SSL_CTX_use_certificate_chain_file((SSL_CTX *) ctx,file) == 1;
}

bool TLSBackendUsePrivateKeyFile(
		Environment *theEnv,
		void *ctx,
		const char *file)
{
	(void) theEnv;

	if (SSL_CTX_use_PrivateKey_file((SSL_CTX *) ctx,file,SSL_FILETYPE_PEM) != 1)
	{ return false; }

	// A key that does not agree with the certificate fails at handshake
	// time. The error message then does not show the true cause.
	return SSL_CTX_check_private_key((SSL_CTX *) ctx) == 1;
}

/*****************************************************************/
/* TLSBackendSetVerify: SSL_VERIFY_PEER alone is not sufficient  */
/*   on a server. It asks the client for a certificate and       */
/*   checks the certificate that comes back. But it lets through */
/*   a client that sends no certificate. A server that asked for */
/*   a verified peer would then accept a peer with no identity,  */
/*   which is the opposite of the request.                       */
/*   SSL_VERIFY_FAIL_IF_NO_PEER_CERT makes the request a         */
/*   condition.                                                  */
/*                                                               */
/*   The function sets the bit in all conditions, because        */
/*   OpenSSL ignores it in client mode. In client mode a server  */
/*   certificate is never optional. As a result, this backend    */
/*   does not need to know which end it is, and no other code    */
/*   here needs that either.                                     */
/*****************************************************************/
bool TLSBackendSetVerify(
		void *ctx,
		bool required)
{
	SSL_CTX_set_verify((SSL_CTX *) ctx,
			required ? (SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT) : SSL_VERIFY_NONE,
			NULL);
	return true;
}

bool TLSBackendSetMinVersion(
		void *ctx,
		int version)
{
	int native;

	switch (version)
	{
		case TLS_VERSION_DTLS_1_2:
#if TLS_HAS_DTLS
			native = DTLS1_2_VERSION;
			break;
#else
			return false;
#endif
		case TLS_VERSION_1_2:
			native = TLS1_2_VERSION;
			break;
		case TLS_VERSION_1_3:
#ifdef TLS1_3_VERSION
			native = TLS1_3_VERSION;
			break;
#else
			return false;
#endif
		default:
			return false;
	}

	return SSL_CTX_set_min_proto_version((SSL_CTX *) ctx,native) == 1;
}

/*=================*/
/* SESSIONS        */
/*=================*/

void *TLSBackendNewSession(
		Environment *theEnv,
		void *ctx,
		int fd,
		bool asClient,
		const char *hostname)
{
	SSL *ssl;

	(void) theEnv;

	ssl = SSL_new((SSL_CTX *) ctx);
	if (ssl == NULL) return NULL;

#if TLS_HAS_DTLS
	// A datagram session needs a datagram BIO. SSL_set_fd would give it a
	// socket BIO. A socket BIO reads and writes the descriptor correctly,
	// but it knows nothing about MTUs or record limits. The handshake then
	// operates on loopback, but the first divided flight on a real link
	// fails.
	if (TLSIsDTLS(ssl))
	{
		if (! AttachDatagramBIO(ssl,fd))
		{
			SSL_free(ssl);
			return NULL;
		}
	}
	else
#endif
	if (SSL_set_fd(ssl,fd) != 1)
	{
		SSL_free(ssl);
		return NULL;
	}

	// The partial-write mode lets the code do a short write again from the
	// point where it stopped. The moving-buffer mode lets that second
	// attempt give a different address. The address does change, because the
	// caller moves the remaining data to the start of its buffer between the
	// attempts.
	SSL_set_mode(ssl,SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

	if (asClient && (hostname != NULL) && (hostname[0] != '\0'))
	{
		if (TLSLooksLikeAddress(hostname))
		{
			// An address is not a name. If the code gives an address to
			// the name comparison, a certificate with DNS:*.0.0.1 becomes
			// correct for 127.0.0.1 and for each other address with that
			// end. BoringSSL does this. The address comparison applies no
			// wildcards.
			//
			// The code sends no SNI either. RFC 6066 does not permit a
			// literal address in that extension, and a server can refuse a
			// message that has one.
			if (! TLSSetVerifyAddress(ssl,hostname))
			{
				SSL_free(ssl);
				return NULL;
			}
		}
		else
		{
			// SNI tells the server which certificate to send. The
			// verification parameter decides if the code accepts that
			// certificate. If you set only the first item, the program
			// looks like it checks a name but it checks nothing.
			if (! TLSSetSNI(ssl,hostname))
			{
				SSL_free(ssl);
				return NULL;
			}

			TLSSetHostFlags(ssl);

			if (! TLSSetVerifyHost(ssl,hostname))
			{
				SSL_free(ssl);
				return NULL;
			}
		}
	}

	if (asClient) SSL_set_connect_state(ssl);
	else          SSL_set_accept_state(ssl);

	return ssl;
}

void TLSBackendFreeSession(
		Environment *theEnv,
		void *sess)
{
	(void) theEnv;

	if (sess != NULL) SSL_free((SSL *) sess);
}

int TLSBackendHandshake(
		void *sess)
{
	SSL *ssl = (SSL *) sess;
	int rv, err;

	rv = SSL_do_handshake(ssl);
	if (rv == 1) return TLS_HANDSHAKE_DONE;

	err = SSL_get_error(ssl,rv);
	if ((err == SSL_ERROR_WANT_READ) || (err == SSL_ERROR_WANT_WRITE))
	{ return TLS_HANDSHAKE_AGAIN; }

	return TLS_HANDSHAKE_FAIL;
}

long TLSBackendRead(
		void *sess,
		void *buf,
		size_t len)
{
	SSL *ssl = (SSL *) sess;
	int n, err;

	if (len > INT_MAX) len = INT_MAX;

	n = SSL_read(ssl,buf,(int) len);
	if (n > 0) return (long) n;

	err = SSL_get_error(ssl,n);

	switch (err)
	{
		case SSL_ERROR_ZERO_RETURN:
			return TLS_RESULT_EOF;

		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
			return TLS_RESULT_AGAIN;

		case SSL_ERROR_SYSCALL:
			// A syscall error with an empty queue and no errno is a peer
			// that went away without a close_notify. This is usual on a
			// real network. If the code gave a failure here, usual
			// traffic would look like an error.
			if (ERR_peek_error() == 0) return TLS_RESULT_EOF;
			return TLS_RESULT_FAIL;

		default:
			return TLS_RESULT_FAIL;
	}
}

long TLSBackendWrite(
		void *sess,
		const void *buf,
		size_t len)
{
	SSL *ssl = (SSL *) sess;
	int n, err;

	if (len > INT_MAX) len = INT_MAX;

	n = SSL_write(ssl,buf,(int) len);
	if (n > 0) return (long) n;

	err = SSL_get_error(ssl,n);

	if ((err == SSL_ERROR_WANT_READ) || (err == SSL_ERROR_WANT_WRITE))
	{ return TLS_RESULT_AGAIN; }

	if (err == SSL_ERROR_ZERO_RETURN) return TLS_RESULT_EOF;

	return TLS_RESULT_FAIL;
}

int TLSBackendPending(
		void *sess)
{
	return SSL_pending((SSL *) sess);
}

bool TLSBackendShutdown(
		void *sess)
{
	int rv;

	// Zero shows that the close_notify of this end went out and that the
	// close_notify of the peer did not come back. This is a complete
	// half-close and not a failure. To wait for the other half would block on
	// a peer that possibly never replies.
	rv = SSL_shutdown((SSL *) sess);

	return (rv == 0) || (rv == 1);
}

/*=================*/
/* INTROSPECTION   */
/*=================*/

const char *TLSBackendCipher(
		void *sess)
{
	const char *name = SSL_get_cipher_name((SSL *) sess);

	return (name != NULL) ? name : "";
}

const char *TLSBackendProtocol(
		void *sess)
{
	const char *name = SSL_get_version((SSL *) sess);

	return (name != NULL) ? name : "";
}

/*****************************************************************/
/* CertificateFailure: The cause of the refusal of a peer        */
/*   certificate, or zero if the code did not refuse it.         */
/*                                                               */
/*   Three of these four libraries keep the cause as a           */
/*   verification result, and one call gives it. wolfSSL applies */
/*   the host name in wolfSSL_check_domain_name. That call stops */
/*   the handshake but sets no verification result, and the      */
/*   result stays X509_V_OK. The error state of the session      */
/*   holds the cause instead. This function reads both of them,  */
/*   and the same string table gives the text for the codes.     */
/*                                                               */
/*   This is why the libraries share this file by API and not by */
/*   behaviour. The calls are the same, but their meanings are   */
/*   not always the same.                                        */
/*****************************************************************/
static long CertificateFailure(
		SSL *ssl)
{
	long result;

	result = SSL_get_verify_result(ssl);
	if (result != X509_V_OK) return result;

#ifdef TLS_BACKEND_WOLFSSL
	{
		int stored = SSL_get_error(ssl,-1);

		// Only a certificate error is of use here. A session that waits on
		// the socket, or a session that never failed, has nothing to
		// report.
		if ((stored != 0) &&
		    (stored != SSL_ERROR_WANT_READ) &&
		    (stored != SSL_ERROR_WANT_WRITE) &&
		    (stored != SSL_ERROR_NONE))
		{ return stored; }
	}
#endif

	return X509_V_OK;
}

bool TLSBackendVerifyOK(
		void *sess)
{
	return CertificateFailure((SSL *) sess) == X509_V_OK;
}

bool TLSBackendVerifyDescription(
		void *sess,
		char *out,
		size_t outLen)
{
	const char *text;

	text = X509_verify_cert_error_string(CertificateFailure((SSL *) sess));
	if (text == NULL) return false;

	strncpy(out,text,outLen - 1);
	out[outLen - 1] = '\0';

	return true;
}

bool TLSBackendPeerSubject(
		void *sess,
		char *out,
		size_t outLen)
{
	X509 *cert;

	cert = TLSGetPeerCertificate((SSL *) sess);
	if (cert == NULL) return false;

	// The two forms of this call give back a reference for this code. As a
	// result, the code frees a reference each time, and not only on the
	// OpenSSL 3 path.
	if (X509_NAME_oneline(X509_get_subject_name(cert),out,(int) outLen) == NULL)
	{
		X509_free(cert);
		return false;
	}

	X509_free(cert);

	return true;
}


/*=================*/
/* DATAGRAMS       */
/*=================*/

bool TLSBackendSupportsDTLS(
		bool asServer)
{
#if TLS_HAS_DTLS
	return true;
#else
	return false;
#endif
}

bool TLSBackendDTLSRestartable(void)
{
#ifdef TLS_BACKEND_WOLFSSL
	// Its cookie comes from state in the session. As a result, only the
	// session that sent the HelloVerifyRequest accepts the reply.
	return false;
#else
	return true;
#endif
}

#if TLS_HAS_DTLS

/**************************************************************************/
/* TLSBackendDTLSListen: Replies to each ClientHello until one arrives    */
/*   with a cookie that agrees with the address that sent it.             */
/*                                                                        */
/*   DTLSv1_listen reads from the socket itself. Until the cookie is      */
/*   correct, it replies and returns, and it does not start a handshake.  */
/*   It keeps no data between those calls. The address is correct because */
/*   the code can calculate the cookie from it again, and not because the */
/*   code stored data.                                                    */
/**************************************************************************/
int TLSBackendDTLSListen(
		void *sess,
		struct sockaddr_storage *peer,
		socklen_t *peerLen)
{
	SSL *ssl = (SSL *) sess;
	int rv;

	// The code reads the address from the BIO below and not from the data
	// that the listen call writes. The caller must give a sockaddr to
	// connect(2), and the two libraries write different data.
	rv = ListenForClient(ssl);

	if (rv > 0)
	{
		memset(peer,0,sizeof(*peer));

#ifdef TLS_BACKEND_WOLFSSL
		{
			unsigned int len = (unsigned int) sizeof(*peer);

			if (wolfSSL_dtls_get_peer(ssl,peer,&len) != WOLFSSL_SUCCESS)
			{ return TLS_HANDSHAKE_FAIL; }
		}
#else
		if (BIO_dgram_get_peer(SSL_get_rbio(ssl),peer) <= 0)
		{ return TLS_HANDSHAKE_FAIL; }
#endif

		*peerLen = (peer->ss_family == AF_INET6)
				? (socklen_t) sizeof(struct sockaddr_in6)
				: (socklen_t) sizeof(struct sockaddr_in);

		return TLS_HANDSHAKE_DONE;
	}

	if (rv < 0)
	{
		// On OpenSSL this is a fatal error. On LibreSSL it means "not
		// yet". The DTLSv1_listen of LibreSSL is SSL_accept below, and it
		// gives the -1 of that call for a socket with no data. The
		// question costs nothing on the library where the answer is always
		// the same.
		int err = SSL_get_error(ssl,rv);

		if ((err == SSL_ERROR_WANT_READ) || (err == SSL_ERROR_WANT_WRITE))
		{ return TLS_HANDSHAKE_AGAIN; }

		return TLS_HANDSHAKE_FAIL;
	}

	// Zero is each condition that is not fatal. A HelloVerifyRequest went
	// out and the client did not reply yet. Or a datagram arrived that was
	// not a ClientHello. Or the socket had no data. OpenSSL does not separate
	// these conditions, and SSL_get_error gives no useful answer for a return
	// of zero. The conditions also need no different operation here. Each of
	// them means the same: come back when the socket has data, and keep this
	// session so that the exchange continues.
	return TLS_HANDSHAKE_AGAIN;
}

/**************************************************************************/
/* TLSBackendSetSocket: Gives the session a descriptor that is now        */
/*   connected to the peer.                                               */
/*                                                                        */
/*   The function uses the same BIO again and does not replace it. That   */
/*   BIO holds the flight that the cookie exchange read, and a new BIO    */
/*   would not have that flight.                                          */
/**************************************************************************/
bool TLSBackendSetSocket(
		void *sess,
		int fd)
{
	SSL *ssl = (SSL *) sess;
#ifndef TLS_BACKEND_WOLFSSL
	BIO *bio;
#endif

	if (! TLSIsDTLS(ssl)) return false;

#ifdef TLS_BACKEND_WOLFSSL
	// This is the same call that made the transport at the start. wolfSSL
	// keeps only the descriptor and the peer for the transport. The flight
	// that the code read is in the session, and not in an item that this
	// call replaces.
	return AttachDatagramBIO(ssl,fd);
#else
	bio = SSL_get_rbio(ssl);
	if (bio == NULL) return false;

	if (BIO_set_fd(bio,fd,BIO_NOCLOSE) <= 0) return false;

	MarkConnected(bio,fd);

	return true;
#endif
}

long TLSBackendDTLSTimeout(
		void *sess)
{
#ifdef TLS_BACKEND_WOLFSSL
	// Its compatibility layer has DTLSv1_get_timeout but gives no answer
	// through it. Its own call gives the interval between transmissions, and
	// not the time that remains. The unit is seconds.
	int seconds = wolfSSL_dtls_get_current_timeout((SSL *) sess);

	return (seconds > 0) ? (long) seconds * 1000 : -1;
#else
	struct timeval tv;

	memset(&tv,0,sizeof(tv));

	// Zero means that there is no flight to send again. This is not the same
	// as a flight that the code must send now.
	if (DTLSv1_get_timeout((SSL *) sess,&tv) != 1) return -1;

	return (long) ((tv.tv_sec * 1000) + (tv.tv_usec / 1000));
#endif
}

int TLSBackendDTLSHandleTimeout(
		void *sess)
{
	int rv;

#ifdef TLS_BACKEND_WOLFSSL
	// This is its own call and not the compatibility name.
	// wolfSSL_DTLSv1_handle_timeout reports success but sends nothing again.
	// A lost flight then stays lost and the handshake stops.
	rv = wolfSSL_dtls_got_timeout((SSL *) sess);

	if (rv == WOLFSSL_SUCCESS) return TLS_HANDSHAKE_DONE;

	// This is not an error to report. The call gives the same result when
	// there was no flight to send.
	return TLS_HANDSHAKE_DONE;
#else
	rv = DTLSv1_handle_timeout((SSL *) sess);

	// Zero means that there was no flight to send. This is an answer and not
	// a failure. A caller with a poll loop asks more frequently than flights
	// are lost.
	if (rv < 0) return TLS_HANDSHAKE_FAIL;

	return TLS_HANDSHAKE_DONE;
#endif
}

bool TLSBackendSetMTU(
		void *sess,
		int mtu)
{
	SSL *ssl = (SSL *) sess;

	if (! TLSIsDTLS(ssl)) return false;

	SSL_set_options(ssl,SSL_OP_NO_QUERY_MTU);

	return SetLinkMTU(ssl,mtu);
}

/**************************************************************************/
/* TLSBackendMaxPayload: The quantity of application data in one record.  */
/*                                                                        */
/*   The function gives zero until the handshake selects a cipher,        */
/*   because the overhead is not known before that point. The caller asks */
/*   again at the end of the handshake. That is the point at which the    */
/*   value is known and necessary.                                        */
/**************************************************************************/
int TLSBackendMaxPayload(
		void *sess)
{
	SSL *ssl = (SSL *) sess;

	if (! TLSIsDTLS(ssl)) return 0;

	return DataMTU(ssl);
}

#else /* ! TLS_HAS_DTLS */

/* BoringSSL comes here, because it has no BIO_new_dgram to put a datagram
   session on. The six functions give the same answers as each backend without
   DTLS. socktlsbe.h has them one time. As a result, a seventh call in the
   interface is not an item to remember in two files. */
TLS_DEFINE_NO_DTLS_SESSION_STUBS()

#endif /* TLS_HAS_DTLS */

#endif /* USE_TLS */
