/*******************************************************/
/*      "C" Language Integrated Production System      */
/*                                                     */
/*            CLIPS Version ?.??  08/10/26             */
/*                                                     */
/*                 TLS BACKEND: GNUTLS                 */
/*******************************************************/

/***********************************************************************/
/* Purpose: This file implements the backend interface in socktlsbe.h  */
/*   with GnuTLS. GnuTLS shares an API with neither OpenSSL nor        */
/*   mbedTLS, and it has a file of its own.                            */
/*                                                                     */
/*   The shape of this file comes from where GnuTLS keeps its data.    */
/*   The credentials are the trust store, the certificate and the key. */
/*   They belong to an object that continues after a connection ends,  */
/*   and that object is a context here. Almost all the other data      */
/*   belongs to the session, and the code can set it only after a      */
/*   session exists. This other data includes the protocol versions    */
/*   and the verification of the peer. As a result, a context also     */
/*   keeps what the caller told it, and it applies that data to each   */
/*   new session.                                                      */
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

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>

#include <poll.h>
#include <stdint.h>
#include <sys/socket.h>

#include "setup.h"

#include "constant.h"
#include "envrnmnt.h"
#include "memalloc.h"
#include "prntutil.h"
#include "router.h"

#include "socktlsbe.h"

#ifdef USE_TLS

#include <gnutls/dtls.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

/*==============================================================*/
/* A priority string selects the protocol versions. A constant  */
/* does not. These strings say "all that NORMAL permits, but of */
/* the versions only these". This is how you write a minimum    */
/* version when the language has no word for one.               */
/*==============================================================*/

#define GNU_PRIORITY_TLS12 "NORMAL:-VERS-ALL:+VERS-TLS1.2:+VERS-TLS1.3"
#define GNU_PRIORITY_TLS13 "NORMAL:-VERS-ALL:+VERS-TLS1.3"

/* Datagram sessions need the names of the datagram versions. A priority
   string with only TLS versions leaves a DTLS session with no version to
   offer. GnuTLS reports this at handshake time and not when the code sets the
   string. The message is "No or insufficient priorities were set". This
   string does not include DTLS 1.0, because DTLS 1.0 has the weaknesses of
   the TLS 1.1 that it comes from. */
#define GNU_PRIORITY_DTLS12 "NORMAL:-VERS-ALL:+VERS-DTLS1.2"

/*==============================================================*/
/* Credentials that the code replaced but cannot free yet.      */
/*                                                              */
/* A certificate and its key go into a credentials object       */
/* together, and the code cannot take them out again. To give a */
/* new certificate or a new key, the code must make the object  */
/* again. Sessions hold the object by pointer, because          */
/* gnutls_credentials_set copies no data. As a result, the old  */
/* object must stay in memory until the context goes away. The  */
/* reference count in socktls.c keeps the context until its     */
/* last session ends.                                           */
/*==============================================================*/

struct gnuRetired
  {
   gnutls_certificate_credentials_t cred;
   struct gnuRetired *next;
  };

/* The trust locations from one call. The code keeps each call and not only
   the last one. A second authority is an addition to the authorities that the
   code already trusts, and not a replacement for them. Credentials that the
   code makes again later must be the same as the credentials that they
   replace. The sequence is not important, because these are a set of anchors
   and not a list. As a result, a new entry goes at the front. */
struct gnuTrust
  {
   char *file;
   char *dir;
   struct gnuTrust *next;
  };

struct gnuContext
  {
   gnutls_certificate_credentials_t cred;
   const char *priority;
   /* All the data that the code made the credentials from. The code needs
      this data because it must apply the data again when it makes the
      credentials again. GnuTLS has no call to read the data back. */
   char *certFile;
   char *keyFile;
   struct gnuTrust *trust;
   bool systemTrust;
   struct gnuRetired *retired;
   bool asClient;
   bool verifyRequired;
   bool datagram;
   /* The secret that a DTLS server calculates its cookies under. It belongs
      to the context and not to a session. As a result, one session can accept
      a cookie that a different session sent, and the exchange is stateless. */
   gnutls_datum_t cookieKey;
   bool haveCookieKey;
  };

struct gnuSession
  {
   gnutls_session_t session;
   bool verifyRequired;
   /* For datagram sessions only. The code keeps the descriptor because the
      cookie exchange reads and writes the socket itself. At that time there
      is no session transport for the data. */
   int fd;
   bool datagram;
   /* Where the code keeps the peer of an exchange in progress. As a result,
      the HelloVerifyRequest goes to the address that the ClientHello gave,
      and to no other address. */
   struct sockaddr_storage peer;
   socklen_t peerLen;
   /* The cookie secret of the context. The session uses it and does not copy
      it. This pointer is safe because a context stays in memory until each of
      its sessions ends. socktls.c counts one reference for each session, and
      that is the purpose of the count. The pointer is NULL on a client,
      because a client sends no cookies. */
   gnutls_datum_t *cookieKey;
  };

/* GnuTLS gives error codes and does not put them in a queue. As a result,
   the code keeps the most recent code for TLSBackendReportError. */
TLS_DEFINE_LAST_ERROR()

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static gnutls_certificate_credentials_t BuildCredentials(struct gnuContext *);
static bool                    Rebuild(Environment *,struct gnuContext *);
static bool                    FileParsesAs(const char *,bool);

/*******************************************************/
/* TLSBackendStartup: This call is optional from       */
/*   GnuTLS 3.3, and it causes no problem.             */
/*******************************************************/
bool TLSBackendStartup(void)
{
	return gnutls_global_init() == GNUTLS_E_SUCCESS;
}

const char *TLSBackendName(void)
{
	return "gnutls";
}

const char *TLSBackendVersion(void)
{
	const char *version = gnutls_check_version(NULL);

	return (version != NULL) ? version : "";
}

/**************************************************************/
/* TLSBackendNewContext: The credentials are all that a       */
/*   context can hold. The code keeps the other data for      */
/*   NewSession to apply, because GnuTLS has no other         */
/*   location for it.                                         */
/**************************************************************/
void *TLSBackendNewContext(
		Environment *theEnv,
		bool asClient,
		bool datagram)
{
	struct gnuContext *ctx;

	ctx = (struct gnuContext *) gm2(theEnv,sizeof(struct gnuContext));
	if (ctx == NULL) return NULL;

	ctx->cred = NULL;
	ctx->certFile = NULL;
	ctx->keyFile = NULL;
	ctx->trust = NULL;
	ctx->systemTrust = false;
	ctx->retired = NULL;
	ctx->asClient = asClient;
	ctx->datagram = datagram;
	ctx->haveCookieKey = false;
	ctx->cookieKey.data = NULL;
	ctx->cookieKey.size = 0;
	ctx->priority = datagram ? GNU_PRIORITY_DTLS12 : GNU_PRIORITY_TLS12;

	// A DTLS server does no work for a ClientHello until one comes back with
	// a cookie that the code calculated under this key. As a result, a
	// datagram that gives the address of a different machine costs the server
	// nothing.
	if (datagram && (! asClient))
	{
		if (0 != (LastError = gnutls_key_generate(&ctx->cookieKey,
				GNUTLS_COOKIE_KEY_SIZE)))
		{
			rm(theEnv,ctx,sizeof(struct gnuContext));
			return NULL;
		}

		ctx->haveCookieKey = true;
	}

	// Verification of the peer is the default for a client. As a result, a
	// program that does not ask for verification cannot make a connection
	// with no authentication. A server has no peer certificate to check
	// unless the program asks for one.
	ctx->verifyRequired = asClient;

	ctx->cred = BuildCredentials(ctx);
	if (ctx->cred == NULL)
	{
		if (ctx->haveCookieKey) gnutls_free(ctx->cookieKey.data);
		rm(theEnv,ctx,sizeof(struct gnuContext));
		return NULL;
	}

	return ctx;
}

void TLSBackendFreeContext(
		Environment *theEnv,
		void *vctx)
{
	struct gnuContext *ctx = (struct gnuContext *) vctx;
	struct gnuRetired *old, *nextOld;
	struct gnuTrust *aTrust, *nextTrust;

	if (ctx == NULL) return;

	if (ctx->cred != NULL) gnutls_certificate_free_credentials(ctx->cred);

	if (ctx->haveCookieKey) gnutls_free(ctx->cookieKey.data);

	// These are the credentials that the code replaced. No code uses them
	// now, because the code frees a context only after the last session from
	// that context ends. socktls.c counts the references for this.
	for (old = ctx->retired; old != NULL; old = nextOld)
	{
		nextOld = old->next;
		if (old->cred != NULL) gnutls_certificate_free_credentials(old->cred);
		rm(theEnv,old,sizeof(struct gnuRetired));
	}

	for (aTrust = ctx->trust; aTrust != NULL; aTrust = nextTrust)
	{
		nextTrust = aTrust->next;
		TLSForgetPath(theEnv,&aTrust->file);
		TLSForgetPath(theEnv,&aTrust->dir);
		rm(theEnv,aTrust,sizeof(struct gnuTrust));
	}

	TLSForgetPath(theEnv,&ctx->certFile);
	TLSForgetPath(theEnv,&ctx->keyFile);
	rm(theEnv,ctx,sizeof(struct gnuContext));
}

/****************************************************************/
/* BuildCredentials: Makes a credentials object that holds all  */
/*   the data that the caller gave this context.                */
/*                                                              */
/*   This is one function because the code makes the object     */
/*   more than one time. A certificate and its key go in        */
/*   together, and the code cannot take them out again. To give */
/*   a new certificate or key, the code must start again. To    */
/*   start again, the code must also apply the trust store      */
/*   again, and for this the context keeps the trust locations. */
/****************************************************************/
static gnutls_certificate_credentials_t BuildCredentials(
		struct gnuContext *ctx)
{
	gnutls_certificate_credentials_t cred = NULL;
	const struct gnuTrust *aTrust;
	int rv;

	if (0 != (LastError = gnutls_certificate_allocate_credentials(&cred)))
	{ return NULL; }

	if (ctx->systemTrust)
	{
		rv = gnutls_certificate_set_x509_system_trust(cred);
		if (rv < 0)
		{
			LastError = rv;
			gnutls_certificate_free_credentials(cred);
			return NULL;
		}
	}

	for (aTrust = ctx->trust; aTrust != NULL; aTrust = aTrust->next)
	{
		if (aTrust->file != NULL)
		{
			rv = gnutls_certificate_set_x509_trust_file(cred,aTrust->file,GNUTLS_X509_FMT_PEM);
			if (rv < 0)
			{
				LastError = rv;
				gnutls_certificate_free_credentials(cred);
				return NULL;
			}
		}

		if (aTrust->dir != NULL)
		{
			rv = gnutls_certificate_set_x509_trust_dir(cred,aTrust->dir,GNUTLS_X509_FMT_PEM);
			if (rv < 0)
			{
				LastError = rv;
				gnutls_certificate_free_credentials(cred);
				return NULL;
			}
		}
	}

	if ((ctx->certFile != NULL) && (ctx->keyFile != NULL))
	{
		LastError = gnutls_certificate_set_x509_key_file(cred,
				ctx->certFile,ctx->keyFile,GNUTLS_X509_FMT_PEM);

		if (LastError != GNUTLS_E_SUCCESS)
		{
			gnutls_certificate_free_credentials(cred);
			return NULL;
		}
	}

	return cred;
}

/****************************************************************/
/* Rebuild: Replaces the credentials with an object that the    */
/*   code makes from the current settings. It keeps the old     */
/*   object and does not free it.                               */
/*                                                              */
/*   The code completes the new object before it releases the   */
/*   old one. As a result, a failure leaves the context as it   */
/*   was. This is why a certificate whose key did not arrive    */
/*   yet is a refusal and not damage. The pair does not agree,  */
/*   the code installs nothing, and the context still holds     */
/*   credentials that operate when the key arrives.             */
/****************************************************************/
static bool Rebuild(
		Environment *theEnv,
		struct gnuContext *ctx)
{
	gnutls_certificate_credentials_t cred;
	struct gnuRetired *old;

	cred = BuildCredentials(ctx);
	if (cred == NULL) return false;

	old = (struct gnuRetired *) gm2(theEnv,sizeof(struct gnuRetired));
	if (old == NULL)
	{
		gnutls_certificate_free_credentials(cred);
		return false;
	}

	old->cred = ctx->cred;
	old->next = ctx->retired;
	ctx->retired = old;

	ctx->cred = cred;

	return true;
}

/****************************************************************/
/* FileParsesAs: Tells if a file is present and holds the       */
/*   correct type of data.                                      */
/*                                                              */
/*   GnuTLS takes a certificate and its key in one call. As a   */
/*   result, the code can install neither of them until it has  */
/*   both. To wait until that time would let the code accept a  */
/*   path that does not exist and report the error later,       */
/*   against a different call. For this the code reads and      */
/*   parses each file when the caller gives it, and installs    */
/*   the file after that.                                       */
/****************************************************************/
static bool FileParsesAs(
		const char *path,
		bool asCertificate)
{
	gnutls_datum_t contents;
	gnutls_x509_crt_t cert;
	gnutls_x509_privkey_t key;
	int rv;

	if ((path == NULL) || (path[0] == '\0'))
	{
		LastError = GNUTLS_E_FILE_ERROR;
		return false;
	}

	if (0 != (rv = gnutls_load_file(path,&contents)))
	{
		LastError = rv;
		return false;
	}

	if (asCertificate)
	{
		if (0 != (rv = gnutls_x509_crt_init(&cert)))
		{
			gnutls_free(contents.data);
			LastError = rv;
			return false;
		}

		rv = gnutls_x509_crt_import(cert,&contents,GNUTLS_X509_FMT_PEM);
		gnutls_x509_crt_deinit(cert);
	}
	else
	{
		if (0 != (rv = gnutls_x509_privkey_init(&key)))
		{
			gnutls_free(contents.data);
			LastError = rv;
			return false;
		}

		rv = gnutls_x509_privkey_import(key,&contents,GNUTLS_X509_FMT_PEM);
		gnutls_x509_privkey_deinit(key);
	}

	gnutls_free(contents.data);

	if (rv != 0)
	{
		LastError = rv;
		return false;
	}

	return true;
}

/****************************************************************/
/* TLSBackendLoadVerifyLocations: The GnuTLS calls give the     */
/*   number of certificates that they read. A count of zero is  */
/*   a failure, although the value is not negative.             */
/*                                                              */
/*   The function applies the locations to the current          */
/*   credentials and also keeps them. It keeps them because the */
/*   code makes the credentials again at each change of the     */
/*   certificate, and the result must be the same. The code     */
/*   keeps each call and not only the most recent one, because  */
/*   trust in a second authority does not remove trust in the   */
/*   first.                                                     */
/****************************************************************/
bool TLSBackendLoadVerifyLocations(
		Environment *theEnv,
		void *vctx,
		const char *caFile,
		const char *caPath)
{
	struct gnuContext *ctx = (struct gnuContext *) vctx;
	const char *file = ((caFile != NULL) && (caFile[0] != '\0')) ? caFile : NULL;
	const char *dir = ((caPath != NULL) && (caPath[0] != '\0')) ? caPath : NULL;
	struct gnuTrust *aTrust;
	int loaded = 0;
	int rv;

	if ((file == NULL) && (dir == NULL)) return false;

	if (file != NULL)
	{
		rv = gnutls_certificate_set_x509_trust_file(ctx->cred,file,GNUTLS_X509_FMT_PEM);
		if (rv < 0)
		{
			LastError = rv;
			return false;
		}
		loaded += rv;
	}

	if (dir != NULL)
	{
		rv = gnutls_certificate_set_x509_trust_dir(ctx->cred,dir,GNUTLS_X509_FMT_PEM);
		if (rv < 0)
		{
			LastError = rv;
			return false;
		}
		loaded += rv;
	}

	if (loaded <= 0)
	{
		LastError = GNUTLS_E_NO_CERTIFICATE_FOUND;
		return false;
	}

	aTrust = (struct gnuTrust *) gm2(theEnv,sizeof(struct gnuTrust));
	if (aTrust == NULL) return false;

	aTrust->file = NULL;
	aTrust->dir = NULL;

	if (! TLSRememberPath(theEnv,&aTrust->file,file) ||
	    ! TLSRememberPath(theEnv,&aTrust->dir,dir))
	{
		TLSForgetPath(theEnv,&aTrust->file);
		TLSForgetPath(theEnv,&aTrust->dir);
		rm(theEnv,aTrust,sizeof(struct gnuTrust));
		return false;
	}

	aTrust->next = ctx->trust;
	ctx->trust = aTrust;

	return true;
}

bool TLSBackendLoadSystemTrust(
		void *vctx)
{
	struct gnuContext *ctx = (struct gnuContext *) vctx;
	int rv;

	rv = gnutls_certificate_set_x509_system_trust(ctx->cred);
	if (rv < 0)
	{
		LastError = rv;
		return false;
	}

	if (rv == 0) return false;

	// The code keeps this flag so that credentials that it makes later also
	// start from the system store.
	ctx->systemTrust = true;

	return true;
}

bool TLSBackendUseCertificateFile(
		Environment *theEnv,
		void *vctx,
		const char *path)
{
	struct gnuContext *ctx = (struct gnuContext *) vctx;

	if (! FileParsesAs(path,true)) return false;
	if (! TLSRememberPath(theEnv,&ctx->certFile,path)) return false;

	// There is no key yet, and the code can make nothing.
	if (ctx->keyFile == NULL) return true;

	// The pair possibly does not agree. To replace both of them, the caller
	// gives them one at a time. Between the two calls, this certificate is
	// against the key of the certificate before it. The code reports this
	// condition and does not hide it. It also does not change the current
	// credentials. As a result, a context never holds a certificate and a key
	// that do not agree, and the next call with the correct key completes the
	// change.
	return Rebuild(theEnv,ctx);
}

bool TLSBackendUsePrivateKeyFile(
		Environment *theEnv,
		void *vctx,
		const char *path)
{
	struct gnuContext *ctx = (struct gnuContext *) vctx;

	if (! FileParsesAs(path,false)) return false;
	if (! TLSRememberPath(theEnv,&ctx->keyFile,path)) return false;

	if (ctx->certFile == NULL) return true;

	// This is the same operation as for the certificate, and for the same
	// cause.
	return Rebuild(theEnv,ctx);
}

bool TLSBackendSetVerify(
		void *vctx,
		bool required)
{
	struct gnuContext *ctx = (struct gnuContext *) vctx;

	ctx->verifyRequired = required;

	return true;
}

/****************************************************************/
/* TLSBackendSetMinVersion: The function checks the priority    */
/*   string here and not at the session that uses it. As a      */
/*   result, the code refuses a version that this build cannot  */
/*   offer while it can still report the error against this     */
/*   call.                                                      */
/****************************************************************/
bool TLSBackendSetMinVersion(
		void *vctx,
		int version)
{
	struct gnuContext *ctx = (struct gnuContext *) vctx;
	gnutls_priority_t probe;
	const char *wanted;
	const char *errPos;

	switch (version)
	{
		case TLS_VERSION_DTLS_1_2:
			if (! ctx->datagram) return false;
			wanted = GNU_PRIORITY_DTLS12;
			break;

		case TLS_VERSION_1_2:
			if (ctx->datagram) return false;
			wanted = GNU_PRIORITY_TLS12;
			break;

		case TLS_VERSION_1_3:
			if (ctx->datagram) return false;
			wanted = GNU_PRIORITY_TLS13;
			break;

		default: return false;
	}

	if (0 != (LastError = gnutls_priority_init(&probe,wanted,&errPos)))
	{ return false; }

	gnutls_priority_deinit(probe);

	ctx->priority = wanted;

	return true;
}

/*****************************************************************/
/* PullTimeout: Tells if there is data on the socket now.        */
/*                                                               */
/*   GnuTLS asks this question before it reads during a DTLS     */
/*   handshake. It then waits for a flight and does not report   */
/*   the flight as lost. The GnuTLS function waits for the full  */
/*   time that the code gives it. On a non-blocking socket that  */
/*   is the wait that the caller wants to prevent. Also, the two */
/*   ends of a handshake can be in one thread, and then nothing  */
/*   can end the wait.                                           */
/*                                                               */
/*   An immediate answer makes that wait a GNUTLS_E_AGAIN, and   */
/*   the poll loop of the caller then does the work. This        */
/*   function ignores the given timeout on purpose.              */
/*****************************************************************/
static int PullTimeout(
		gnutls_transport_ptr_t ptr,
		unsigned int ms)
{
	struct pollfd waiting;

	// This is the descriptor, because gnutls_transport_set_int put it here.
	// The default send and receive functions read it back in the same manner.
	// If the code put a different value here, for example a session pointer,
	// those functions would write to the integer value of that pointer.
	waiting.fd = (int) (intptr_t) ptr;
	waiting.events = POLLIN;
	waiting.revents = 0;

	return poll(&waiting,1,0);
}

/****************************************************************/
/* TLSBackendNewSession: This function applies the settings of  */
/*   the context, because GnuTLS has no location for them       */
/*   before this point.                                         */
/*                                                              */
/*   The function turns the handshake timeout off, and that is  */
/*   on purpose. Without this, GnuTLS stops the handshake on    */
/*   its own schedule. On a non-blocking socket the caller      */
/*   controls the handshake and decides how long to continue.   */
/****************************************************************/
void *TLSBackendNewSession(
		Environment *theEnv,
		void *vctx,
		int fd,
		bool asClient,
		const char *hostname)
{
	struct gnuContext *ctx = (struct gnuContext *) vctx;
	struct gnuSession *sess;
	const char *errPos;
	unsigned int initFlags;

	sess = (struct gnuSession *) gm2(theEnv,sizeof(struct gnuSession));
	if (sess == NULL) return NULL;

	sess->session = NULL;
	sess->verifyRequired = ctx->verifyRequired;

	sess->fd = fd;
	sess->datagram = ctx->datagram;
	sess->cookieKey = ctx->haveCookieKey ? &ctx->cookieKey : NULL;

	initFlags = (asClient ? GNUTLS_CLIENT : GNUTLS_SERVER) |
	            (ctx->datagram ? GNUTLS_DATAGRAM : 0);
	sess->peerLen = 0;
	memset(&sess->peer,0,sizeof(sess->peer));

	// GNUTLS_NONBLOCK lets a DTLS handshake give the answer "not yet".
	// Without it, GnuTLS thinks that it can wait for a flight, and it waits
	// inside its own loop. On a socket that never blocks, nothing can end
	// that loop. Also, the two ends of a handshake can be in one thread, and
	// then no other thread can do work. The code asks the descriptor and does
	// not depend on the caller.
	{
		int flags = fcntl(fd,F_GETFL,0);

		if (ctx->datagram && (flags != -1) && ((flags & O_NONBLOCK) != 0))
		{ initFlags |= GNUTLS_NONBLOCK; }
	}

	if (0 != (LastError = gnutls_init(&sess->session,initFlags)))
	{
		rm(theEnv,sess,sizeof(struct gnuSession));
		return NULL;
	}

	if (0 != (LastError = gnutls_priority_set_direct(sess->session,ctx->priority,&errPos)))
	{
		TLSBackendFreeSession(theEnv,sess);
		return NULL;
	}

	if (0 != (LastError = gnutls_credentials_set(sess->session,
			GNUTLS_CRD_CERTIFICATE,ctx->cred)))
	{
		TLSBackendFreeSession(theEnv,sess);
		return NULL;
	}

	if (asClient && (hostname != NULL) && (hostname[0] != '\0'))
	{
		// The code sends the name so that the server knows which
		// certificate to send. It also gives the name to the verification
		// code, which compares the certificate with that name. If you do
		// only the first step, the program looks like it checks a name but
		// it checks nothing.
		if (0 != (LastError = gnutls_server_name_set(sess->session,
				GNUTLS_NAME_DNS,hostname,strlen(hostname))))
		{
			TLSBackendFreeSession(theEnv,sess);
			return NULL;
		}

		if (ctx->verifyRequired)
		{ gnutls_session_set_verify_cert(sess->session,hostname,0); }
	}

	if (! asClient && ctx->verifyRequired)
	{
		gnutls_certificate_server_set_request(sess->session,GNUTLS_CERT_REQUIRE);
		gnutls_session_set_verify_cert(sess->session,NULL,0);
	}

	gnutls_transport_set_int(sess->session,fd);

	// Zero means "no limit" on a stream session. A datagram session gets its
	// times from gnutls_dtls_set_timeouts below. If the code also sets this
	// value, the handshake stops on its own schedule.
	if (! ctx->datagram)
	{ gnutls_handshake_set_timeout(sess->session,0); }

	if (ctx->datagram)
	{
		gnutls_dtls_set_mtu(sess->session,TLS_DEFAULT_LINK_MTU);

		// The first number is the interval between transmissions. The second
		// number is the time until GnuTLS stops the handshake. A zero there
		// means stop immediately, and not never.
		gnutls_dtls_set_timeouts(sess->session,1000,60000);

		// A read must be able to give the answer "no data yet" and not
		// wait.
		gnutls_transport_set_pull_timeout_function(sess->session,PullTimeout);
	}

	return sess;
}

void TLSBackendFreeSession(
		Environment *theEnv,
		void *vsess)
{
	struct gnuSession *sess = (struct gnuSession *) vsess;

	if (sess == NULL) return;

	if (sess->session != NULL) gnutls_deinit(sess->session);

	rm(theEnv,sess,sizeof(struct gnuSession));
}

/****************************************************************/
/* TLSBackendHandshake: GnuTLS marks each of its errors as      */
/*   fatal or not fatal. An error that is not fatal means       */
/*   continue. As a result, this function uses that mark and    */
/*   not a list of codes.                                       */
/****************************************************************/
int TLSBackendHandshake(
		void *vsess)
{
	struct gnuSession *sess = (struct gnuSession *) vsess;
	int rv;

	rv = gnutls_handshake(sess->session);

	if (rv == GNUTLS_E_SUCCESS) return TLS_HANDSHAKE_DONE;

	if (gnutls_error_is_fatal(rv) == 0) return TLS_HANDSHAKE_AGAIN;

	RememberError(rv);

	return TLS_HANDSHAKE_FAIL;
}

long TLSBackendRead(
		void *vsess,
		void *buf,
		size_t len)
{
	struct gnuSession *sess = (struct gnuSession *) vsess;
	ssize_t n;

	if (len > INT_MAX) len = INT_MAX;

	n = gnutls_record_recv(sess->session,buf,len);
	if (n > 0) return (long) n;

	if (n == 0) return TLS_RESULT_EOF;

	// This is a peer that went away without a close_notify. This is usual on
	// a real network. If the code gave a failure here, usual traffic would
	// look like an error.
	if (n == GNUTLS_E_PREMATURE_TERMINATION) return TLS_RESULT_EOF;

	if (gnutls_error_is_fatal((int) n) == 0) return TLS_RESULT_AGAIN;

	RememberError((int) n);

	return TLS_RESULT_FAIL;
}

long TLSBackendWrite(
		void *vsess,
		const void *buf,
		size_t len)
{
	struct gnuSession *sess = (struct gnuSession *) vsess;
	ssize_t n;

	if (len > INT_MAX) len = INT_MAX;

	n = gnutls_record_send(sess->session,buf,len);
	if (n > 0) return (long) n;

	if (n == GNUTLS_E_PREMATURE_TERMINATION) return TLS_RESULT_EOF;

	if (gnutls_error_is_fatal((int) n) == 0) return TLS_RESULT_AGAIN;

	RememberError((int) n);

	return TLS_RESULT_FAIL;
}

int TLSBackendPending(
		void *vsess)
{
	struct gnuSession *sess = (struct gnuSession *) vsess;
	size_t avail;

	avail = gnutls_record_check_pending(sess->session);

	return (avail > INT_MAX) ? INT_MAX : (int) avail;
}

/****************************************************************/
/* TLSBackendShutdown: SHUT_WR sends the close_notify of this   */
/*   end and does not wait for the close_notify of the peer.    */
/*   The other backends use the same half-close, and for the    */
/*   same cause: the peer possibly never replies.               */
/****************************************************************/
bool TLSBackendShutdown(
		void *vsess)
{
	struct gnuSession *sess = (struct gnuSession *) vsess;
	int rv;

	rv = gnutls_bye(sess->session,GNUTLS_SHUT_WR);

	return (rv == GNUTLS_E_SUCCESS) || (gnutls_error_is_fatal(rv) == 0);
}

const char *TLSBackendCipher(
		void *vsess)
{
	struct gnuSession *sess = (struct gnuSession *) vsess;
	const char *name;

	name = gnutls_cipher_get_name(gnutls_cipher_get(sess->session));

	return (name != NULL) ? name : "";
}

/*****************************************************************/
/* TLSBackendProtocol: GnuTLS writes these names "TLS1.2" and    */
/*   "TLS1.3". OpenSSL, LibreSSL, wolfSSL and mbedTLS all write  */
/*   "TLSv1.2" and "TLSv1.3". This function changes the GnuTLS   */
/*   names and does not give them directly. As a result,         */
/*   (tls-version) has the same meaning for a CLIPS program with */
/*   each library. The function would be of little use if a      */
/*   program had to know which library the build used.           */
/*****************************************************************/
const char *TLSBackendProtocol(
		void *vsess)
{
	struct gnuSession *sess = (struct gnuSession *) vsess;
	const char *name;

	switch (gnutls_protocol_get_version(sess->session))
	{
		case GNUTLS_TLS1_2: return "TLSv1.2";
		case GNUTLS_TLS1_3: return "TLSv1.3";
		// GnuTLS writes this name "DTLS1.2" and the other libraries write
		// "DTLSv1.2". A program must not have to know which library the
		// build used before it compares the answer with a value.
		case GNUTLS_DTLS1_2: return "DTLSv1.2";
		default: break;
	}

	name = gnutls_protocol_get_name(gnutls_protocol_get_version(sess->session));

	return (name != NULL) ? name : "";
}

bool TLSBackendVerifyOK(
		void *vsess)
{
	struct gnuSession *sess = (struct gnuSession *) vsess;

	// The code made no check, and there is nothing to refuse. The OpenSSL
	// backend gives the same answer, and for the same cause.
	if (! sess->verifyRequired) return true;

	return gnutls_session_get_verify_cert_status(sess->session) == 0;
}

/*****************************************************************/
/* TLSBackendVerifyDescription: The status is a set of bits and  */
/*   not one code. As a result, it can give more than one cause  */
/*   at the same time, and GnuTLS changes the set into a         */
/*   sentence. GnuTLS allocates that sentence, and this function */
/*   frees it.                                                   */
/*****************************************************************/
bool TLSBackendVerifyDescription(
		void *vsess,
		char *out,
		size_t outLen)
{
	struct gnuSession *sess = (struct gnuSession *) vsess;
	gnutls_datum_t text;
	unsigned status;

	if (! sess->verifyRequired) return false;

	status = gnutls_session_get_verify_cert_status(sess->session);

	// This value is not a status. The verification did not run.
	if (status == (unsigned) -1) return false;
	if (status == 0) return false;

	if (0 != gnutls_certificate_verification_status_print(status,
			GNUTLS_CRT_X509,&text,0))
	{ return false; }

	if (text.data == NULL) return false;

	strncpy(out,(const char *) text.data,outLen - 1);
	out[outLen - 1] = '\0';

	gnutls_free(text.data);

	return out[0] != '\0';
}

bool TLSBackendPeerSubject(
		void *vsess,
		char *out,
		size_t outLen)
{
	struct gnuSession *sess = (struct gnuSession *) vsess;
	const gnutls_datum_t *peers;
	gnutls_x509_crt_t cert;
	unsigned count = 0;
	size_t len = outLen;
	int rv;

	peers = gnutls_certificate_get_peers(sess->session,&count);
	if ((peers == NULL) || (count == 0)) return false;

	if (0 != gnutls_x509_crt_init(&cert)) return false;

	// The peer list is in DER, because that is the format on the network.
	if (0 != gnutls_x509_crt_import(cert,&peers[0],GNUTLS_X509_FMT_DER))
	{
		gnutls_x509_crt_deinit(cert);
		return false;
	}

	rv = gnutls_x509_crt_get_dn(cert,out,&len);

	gnutls_x509_crt_deinit(cert);

	return rv == GNUTLS_E_SUCCESS;
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
	const char *text = NULL;

	if (LastError != 0)
	{
		text = gnutls_strerror(LastError);
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
/* TLSBackendDTLSRestartable: The answer is no. The cookie key   */
/*   belongs to the context, and a new session would send        */
/*   cookies that the reply of the client still agrees with. But */
/*   the exchange here reads the socket itself and keeps the     */
/*   peer that it read. To discard that data only means that the */
/*   code must find it again.                                    */
/*****************************************************************/
bool TLSBackendDTLSRestartable(void)
{
	return false;
}

/*****************************************************************/
/* CookiePush: Sends a HelloVerifyRequest.                       */
/*                                                               */
/*   The transport of the session cannot send it. There is no    */
/*   association yet, the socket is not connected, and it        */
/*   receives data from any address. As a result, the function   */
/*   gives the destination address itself. That address is the   */
/*   address that sent the ClientHello, and no other address. To */
/*   send the reply to a different address would make this       */
/*   server an amplifier, and this exchange is a defence against */
/*   that.                                                       */
/*****************************************************************/
static ssize_t CookiePush(
		gnutls_transport_ptr_t ptr,
		const void *data,
		size_t len)
{
	struct gnuSession *sess = (struct gnuSession *) ptr;

	return sendto(sess->fd,data,len,0,
			(struct sockaddr *) &sess->peer,sess->peerLen);
}

/**************************************************************************/
/* TLSBackendDTLSListen: The cookie exchange, which this file runs        */
/* itself.                                                                */
/*                                                                        */
/*   GnuTLS puts none of this work inside the handshake. This function    */
/*   reads the ClientHello from the socket, checks it, and replies to it. */
/*   Only a client that comes back with a correct cookie gets a session.  */
/*   The function reads the datagram with MSG_PEEK, and a correct         */
/*   ClientHello stays where the handshake can read it again. The         */
/*   function removes an incorrect ClientHello from the socket on         */
/*   purpose, because no other code removes it.                           */
/**************************************************************************/
int TLSBackendDTLSListen(
		void *vsess,
		struct sockaddr_storage *peer,
		socklen_t *peerLen)
{
	struct gnuSession *sess = (struct gnuSession *) vsess;
	gnutls_dtls_prestate_st prestate;
	unsigned char hello[1024];
	socklen_t len = (socklen_t) sizeof(sess->peer);
	ssize_t n;
	int rv;

	if (sess->cookieKey == NULL) return TLS_HANDSHAKE_FAIL;

	memset(&sess->peer,0,sizeof(sess->peer));

	n = recvfrom(sess->fd,hello,sizeof(hello),MSG_PEEK,
			(struct sockaddr *) &sess->peer,&len);

	if (n < 0)
	{
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR))
		{ return TLS_HANDSHAKE_AGAIN; }

		return TLS_HANDSHAKE_FAIL;
	}

	sess->peerLen = len;

	memset(&prestate,0,sizeof(prestate));

	rv = gnutls_dtls_cookie_verify(sess->cookieKey,
			&sess->peer,sizeof(sess->peer),
			hello,(size_t) n,&prestate);

	if (rv < 0)
	{
		// There is no cookie, or the cookie is not one that this key makes
		// for that address. The function sends a cookie and discards the
		// datagram. The datagram is complete, and the code would read it
		// again and again if it stayed on the socket.
		gnutls_dtls_cookie_send(sess->cookieKey,
				&sess->peer,sizeof(sess->peer),&prestate,
				(gnutls_transport_ptr_t) sess,CookiePush);

		(void) recvfrom(sess->fd,hello,sizeof(hello),0,NULL,NULL);

		return TLS_HANDSHAKE_LISTEN_AGAIN;
	}

	// The cookie is correct. The prestate holds the sequence numbers that
	// the exchange used. Without those numbers the handshake starts to count
	// again, and the client discards each message that the server sends.
	gnutls_dtls_prestate_set(sess->session,&prestate);

	memcpy(peer,&sess->peer,sizeof(*peer));
	*peerLen = sess->peerLen;

	return TLS_HANDSHAKE_DONE;
}

bool TLSBackendSetSocket(
		void *vsess,
		int fd)
{
	struct gnuSession *sess = (struct gnuSession *) vsess;

	sess->fd = fd;
	gnutls_transport_set_int(sess->session,fd);

	return true;
}

long TLSBackendDTLSTimeout(
		void *vsess)
{
	struct gnuSession *sess = (struct gnuSession *) vsess;
	unsigned int ms;

	ms = gnutls_dtls_get_timeout(sess->session);

	// Zero means that no timer is in operation. This is not the same as a
	// timer that ends now.
	if (ms == 0) return -1;

	return (long) ms;
}

/*****************************************************************/
/* TLSBackendDTLSHandleTimeout: There is nothing to do. GnuTLS   */
/*   sends the flight again from inside gnutls_handshake after   */
/*   its own timer ends. As a result, the next handshake call of */
/*   the caller is the retransmission.                           */
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
	struct gnuSession *sess = (struct gnuSession *) vsess;

	gnutls_dtls_set_mtu(sess->session,(unsigned int) mtu);

	return true;
}

int TLSBackendMaxPayload(
		void *vsess)
{
	struct gnuSession *sess = (struct gnuSession *) vsess;

	return (int) gnutls_dtls_get_data_mtu(sess->session);
}

#endif /* USE_TLS */
