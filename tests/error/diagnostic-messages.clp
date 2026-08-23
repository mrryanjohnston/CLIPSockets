;;; What these functions write when they refuse a request. This file is not
;;; about the return value.
;;;
;;; Almost each UDF here gives FALSE for a bad argument. FALSE reports that
;;; something failed and gives no detail. As a result, the message on STDERR is
;;; the full explanation, and it needs a test like any other output.
;;;
;;; The test checks two properties of each message:
;;;
;;;   The message names the function that the caller used. A message that
;;;   names a different function sends a caller to code that it did not use.
;;;
;;;   The message ends with a newline. Without it the next output continues on
;;;   the same line, and the two messages are difficult to read.
;;;
;;; The test checks the newline with a match of a full captured line, and not
;;; with a search for the text. A message with no terminator is not a line of
;;; its own. As a result, an exact match fails when the newline is absent.

(load* "tests/lib/expect.clp")
(test-suite "diagnostic-messages")
(test-plan 8)

;;; A descriptor number that no process has open. GetFilenoFromArgument gives
;;; an integer to the system call and does not look it up. As a result, this
;;; value reaches the system call and fails there. That path writes the message
;;; under test, and not the "not a socket" message above it.
(defglobal ?*stale-fd* = 99999)

;;; A name that no socket has. The code looks up a lexeme argument, and this
;;; value makes that lookup fail.
(defglobal ?*no-such-socket* = not-a-socket-name)

(deffunction run-lookup-tests ()
   (capture-start)
   (getsockopt ?*no-such-socket* SOL_SOCKET SO_REUSEADDR)
   (bind ?said (capture-lines))
   (expect-contains "getsockopt names itself when the socket is not found"
                    "getsockopt: could not find router for socket file descriptor"
                    ?said)

   (capture-start)
   (setsockopt ?*no-such-socket* SOL_SOCKET SO_REUSEADDR 1)
   (bind ?said (capture-lines))
   (expect-contains "setsockopt names itself when the socket is not found"
                    "setsockopt: could not find router for socket file descriptor"
                    ?said)

   ;; The two functions that the author copied must also be correct.
   (capture-start)
   (fcntl-add-status-flags ?*no-such-socket* O_NONBLOCK)
   (bind ?said (capture-lines))
   (expect-contains "fcntl-add-status-flags names itself"
                    "add-status-flags: could not find router for socket file descriptor"
                    ?said)

   (capture-start)
   (fcntl-remove-status-flags ?*no-such-socket* O_NONBLOCK)
   (bind ?said (capture-lines))
   (expect-contains "fcntl-remove-status-flags names itself"
                    "remove-status-flags: could not find router for socket file descriptor"
                    ?said))

;;; The message from an fcntl call that failed. An old descriptor passes the
;;; argument check, because the code gives an integer to the system call and
;;; does not look it up. The call then fails in the system call, and that is
;;; the only path to these messages.
;;;
;;; The third check is for the newline. One message alone reads the same in
;;; both conditions, because the last line of a file has no terminator. A
;;; second message after the first one shows if the first message ended. The
;;; perror(3) call after each of these does not appear here. It writes to the
;;; descriptor and not through the router, and the capture never sees
;;; it.
(deffunction run-fcntl-failure-tests ()
   (capture-start)
   (fcntl-add-status-flags ?*stale-fd* O_NONBLOCK)
   (bind ?said (capture-lines))
   (expect-contains "fcntl-add-status-flags reports the descriptor it failed on"
                    (str-cat "add-status-flags: could not read the current flags of socket "
                             ?*stale-fd*)
                    ?said)

   (capture-start)
   (fcntl-remove-status-flags ?*stale-fd* O_NONBLOCK)
   (bind ?said (capture-lines))
   (expect-contains "fcntl-remove-status-flags reports the descriptor it failed on"
                    (str-cat "remove-status-flags: could not read the current flags of socket "
                             ?*stale-fd*)
                    ?said)

   ;; The code writes the message two times, and each message must be a line
   ;; of its own. A message with no newline at the end joins the next one.
   (capture-start)
   (fcntl-add-status-flags ?*stale-fd* O_NONBLOCK)
   (fcntl-add-status-flags ?*stale-fd* O_NONBLOCK)
   (bind ?said (capture-lines))
   (expect-length "two failures in a row are two lines" 2 ?said))

;;; An option name that neither function has. Each function refuses the name
;;; from its own copy of the same list, and the test asks both of them.
(deffunction run-unknown-name-tests ()
   (bind ?s (create-socket AF_INET SOCK_STREAM))
   (capture-start)
   (getsockopt ?s SOL_SOCKET NOT_AN_OPTION)
   (bind ?got (capture-lines))
   (capture-start)
   (setsockopt ?s SOL_SOCKET NOT_AN_OPTION 1)
   (bind ?set (capture-lines))
   (expect-eq "get and set refuse an unknown option the same way"
              (implode$ ?got) (implode$ ?set))
   (close-connection ?s))

(run-lookup-tests)
(run-fcntl-failure-tests)
(run-unknown-name-tests)
(test-summary)
