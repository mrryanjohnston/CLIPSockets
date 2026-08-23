;;; Socket support code that the tests share. These tests need a pair of
;;; endpoints that operates.
;;;
;;; A test loads this file with (load* "tests/lib/socket.clp"), together with
;;; expect.clp. No code here checks a result. It only makes the sockets, and
;;; the test then checks them. This file is separate from expect.clp for the
;;; same cause as tls.clp: each test loads expect.clp, and a file that names a
;;; function that the build does not have does not parse.

;;; A pair of UDP sockets on loopback. Each one connects to the other.
;;;
;;; A connect call gives a datagram socket a logical name, and that name lets
;;; printout and readline reach the socket. examples/server-udp.bat connects
;;; the bound socket back to its sender for this same cause. The code binds the
;;; two ends first, and each end then has a fixed port for the other end to
;;; name.
;;;
;;; Gives four fields: the two descriptors and then the two logical names. A
;;; write to the name in field 3 sends to the socket in field 2, and a write to
;;; the name in field 4 sends to the socket in field 1.
(deffunction udp-connected-pair (?port-a ?port-b)
   (bind ?a (create-socket AF_INET SOCK_DGRAM))
   (setsockopt ?a SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?a 127.0.0.1 ?port-a)

   (bind ?b (create-socket AF_INET SOCK_DGRAM))
   (setsockopt ?b SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?b 127.0.0.1 ?port-b)

   (bind ?aname (connect ?a 127.0.0.1 ?port-b))
   (bind ?bname (connect ?b 127.0.0.1 ?port-a))

   (return (create$ ?a ?b ?aname ?bname)))

;;; Reads characters until the router has no more, and gives the number of
;;; characters. The socket must be non-blocking. There is no end-of-datagram
;;; mark. As a result, on a blocking socket the read after the last character
;;; waits for a datagram that does not come.
(deffunction count-readable-chars (?name)
   (bind ?n 0)
   (while (neq (get-char ?name) -1) do
      (bind ?n (+ ?n 1)))
   (return ?n))

;;; A connected pair of TCP sockets on loopback: a listen socket, a client that
;;; connects to it, and the socket that accept gives for that client.
;;;
;;; Gives five fields: the three descriptors, then the logical name of the
;;; client and the logical name of the accepted end. A caller that needs only
;;; the descriptors takes the first three fields and ignores the others. The
;;; names are here because one half of the callers needed them, and to get them
;;; again from get-socket-logical-name is the same call that this code already
;;; makes.
;;;
;;; This function makes no socket non-blocking. A test that needs that mode
;;; sets it itself, because the choice of end is part of the test. A
;;; backpressure test needs a writer that refuses and does not wait. A timeout
;;; test needs the opposite.
(deffunction tcp-connected-pair (?port)
   (bind ?srv (create-socket AF_INET SOCK_STREAM))
   (setsockopt ?srv SOL_SOCKET SO_REUSEADDR 1)
   (bind-socket ?srv 127.0.0.1 ?port)
   (listen ?srv)

   (bind ?cli (create-socket AF_INET SOCK_STREAM))
   (bind ?cname (connect ?cli 127.0.0.1 ?port))
   (bind ?acc (accept ?srv))
   (bind ?aname (get-socket-logical-name ?acc))

   (return (create$ ?srv ?cli ?acc ?cname ?aname)))

;;; The number of descriptors that this process has open.
;;;
;;; /proc/self/fd has one entry for each open descriptor, and its length is the
;;; count. This is correct on Linux only. The leak tests that use this function
;;; first check that it gave an answer. On a system with no /proc the directory
;;; does not read, and there is no value to compare.
(deffunction count-open-descriptors ()
   (return (length$ (scandir "/proc/self/fd"))))
