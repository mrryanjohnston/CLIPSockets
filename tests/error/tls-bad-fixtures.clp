;;; Certificates and keys that are incorrect in each possible manner.
;;;
;;; requires: tls
;;;
;;; A TLS library reads most of its external data when it loads a certificate
;;; or a key. Most of its error code is in that path. A suite with only
;;; correct files never reaches that code.
;;;
;;; Each check here shows that the answer is FALSE. That looks like a small
;;; test, and it is not. Each of these files is incorrect at a different stage:
;;; before the decode, during the decode, after the decode, and only after a
;;; comparison of two files. The libraries take a different path to each stage.
;;; The checks show that seven libraries agree. As a result, a program can use
;;; one answer and does not need to know which library the build used.
;;;
;;; The libraries agree on twelve of these files without help. The thirteenth
;;; file is why this test exists. mbedtls_ssl_conf_own_cert accepts a private
;;; key that belongs to a different certificate, where the other six libraries
;;; refuse it, and it reports the error later as a handshake failure that does
;;; not show the cause. The mbedTLS backend makes the same check as the others
;;; before it installs the pair.
;;;
;;; tests/fixtures/regenerate.sh makes these files, and the repository does not
;;; hold them. Two of them are copies of the real certificate and the real
;;; key.

(load* "tests/lib/expect.clp")
(test-suite "tls-bad-fixtures")
(test-plan 20)

(defglobal ?*bad* = "tests/fixtures/bad/")

;;; Each of these checks uses a new context. As a result, one refusal cannot
;;; cause the next one.
(deffunction load-cert (?path)
   (bind ?ctx (tls-create-context TLS_SERVER))
   (bind ?ok (tls-context-use-certificate-file ?ctx (str-cat ?*bad* ?path)))
   (tls-free-context ?ctx)
   (return ?ok))

(deffunction load-key (?path)
   (bind ?ctx (tls-create-context TLS_SERVER))
   (bind ?ok (tls-context-use-private-key-file ?ctx (str-cat ?*bad* ?path)))
   (tls-free-context ?ctx)
   (return ?ok))

(deffunction load-ca (?path)
   (bind ?ctx (tls-create-context TLS_CLIENT))
   (bind ?ok (tls-context-load-verify-locations ?ctx (str-cat ?*bad* ?path)))
   (tls-free-context ?ctx)
   (return ?ok))

(deffunction run-tests ()
   ;;=================================================================
   ;; A certificate that is not one
   ;;=================================================================
   ;; This file is incorrect before the decode starts. It has no PEM
   ;; header.
   (expect-false "a certificate that is prose" (load-cert "not-a-pem.txt"))

   ;; This file is incorrect during the decode. The header is correct and the
   ;; body is incomplete.
   (expect-false "a certificate cut off mid-encoding" (load-cert "truncated.pem"))

   ;; This file opens, reads correctly, and holds no certificate. This is a
   ;; different failure from a file that does not open, and the test keeps the
   ;; two apart.
   (expect-false "an empty certificate file" (load-cert "empty.pem"))

   ;; This name opens, but the code cannot read it as a file.
   (expect-false "a directory where a certificate should be"
                 (load-cert "a-directory.pem"))

   ;; There is no file at this path.
   (expect-false "a certificate that does not exist" (load-cert "nope.pem"))

   ;;=================================================================
   ;; A key that is not one
   ;;=================================================================
   ;; This file parses correctly and is the incorrect type of object. A load
   ;; function makes this check last and not first.
   (expect-false "a certificate offered as a key" (load-key "cert-as-key.pem"))
   (expect-false "a key that is prose" (load-key "not-a-pem.txt"))
   (expect-false "an empty key file" (load-key "empty.pem"))
   (expect-false "a key that does not exist" (load-key "nope.pem"))

   ;;=================================================================
   ;; A trust store that is not one
   ;;=================================================================
   (expect-false "an authority file that is prose" (load-ca "not-a-pem.txt"))
   (expect-false "an empty authority file" (load-ca "empty.pem"))
   (expect-false "a directory where an authority file should be"
                 (load-ca "a-directory.pem"))
   (expect-false "an authority file that does not exist" (load-ca "nope.pem"))

   ;;=================================================================
   ;; The directory form, and the system store
   ;;=================================================================
   ;; load-verify-locations takes a directory and also a file.
   ;;
   ;; This directory holds trust anchors and no other file, and that is
   ;; important. The two families read a directory differently. OpenSSL keeps
   ;; the path and looks in the directory later, by hash. mbedTLS and GnuTLS
   ;; parse each file in the directory immediately. As a result, one family
   ;; accepts a directory with private keys in it and the other family refuses
   ;; that directory. tests/fixtures is such a directory, and this test does
   ;; not use it.
   (bind ?dctx (tls-create-context TLS_CLIENT))
   (expect-true "a file and a directory together"
                (tls-context-load-verify-locations ?dctx
                       "tests/fixtures/ca.pem" "tests/fixtures/trust"))
   (tls-free-context ?dctx)

   ;; A directory with files that are not certificates. Some backends call
   ;; this an error and some call it an empty store. As a result, the test
   ;; does not check the answer. It checks only that the call does not stop
   ;; the process, and that the context still operates after the call.
   (bind ?dctx2 (tls-create-context TLS_CLIENT))
   (tls-context-load-verify-locations ?dctx2 "tests/fixtures/ca.pem" ?*bad*)
   (expect-true "a context survives a directory of rubbish"
                (tls-context-load-verify-locations ?dctx2 "tests/fixtures/ca.pem"))
   (tls-free-context ?dctx2)

   ;; The system store, and then a certificate and a key. Some backends make
   ;; their credentials again at each file. On those backends this sequence is
   ;; the only method to reach the system-trust code with other data
   ;; present.
   (bind ?sysctx (tls-create-context TLS_SERVER))
   (tls-context-set-default-verify-paths ?sysctx)
   (expect-true "a certificate loads over the system store"
                (tls-context-use-certificate-file ?sysctx "tests/fixtures/server.pem"))
   (expect-true "and its key with it"
                (tls-context-use-private-key-file ?sysctx "tests/fixtures/server-key.pem"))
   (tls-free-context ?sysctx)

   ;;=================================================================
   ;; Two files that are each fine and do not belong together
   ;;=================================================================
   ;; Each of the two files is correct until the code compares them. As a
   ;; result, this is the one failure that a look at one file cannot find. A
   ;; report here gives an error that names the cause. Without it, a handshake
   ;; fails much later, and the message looks like a protocol problem.
   (bind ?ctx (tls-create-context TLS_SERVER))
   (expect-true "the real certificate loads"
                (tls-context-use-certificate-file ?ctx "tests/fixtures/server.pem"))
   (expect-false "a key belonging to another certificate is refused"
                 (tls-context-use-private-key-file ?ctx
                        (str-cat ?*bad* "mismatched-key.pem")))

   ;; A program can also correct the context. That is important for a program
   ;; that got the incorrect path and wants to try again.
   ;;
   ;; The test gives both files, in that sequence, and checks only the last
   ;; answer. The two families recover differently, and each behaviour is on
   ;; purpose:
   ;;
   ;;   GnuTLS keeps the path that it refused, and the other file then
   ;;   completes the change. To replace a pair, a caller gives two files, and
   ;;   between the two calls the context always holds a pair that does not
   ;;   agree. A second call with the certificate gives FALSE for that cause.
   ;;   That is correct, and it is not the important part.
   ;;
   ;;   LibreSSL needs the certificate again before it accepts any key.
   ;;
   ;; As a result, the test does not check the answer for the certificate. It
   ;; checks only that the pair operates at the end. A caller can depend on
   ;; that on each library.
   (tls-context-use-certificate-file ?ctx "tests/fixtures/server.pem")
   (expect-true "naming both files again puts the context right"
                (tls-context-use-private-key-file ?ctx "tests/fixtures/server-key.pem"))
   (tls-free-context ?ctx))

(run-tests)
(test-summary)
