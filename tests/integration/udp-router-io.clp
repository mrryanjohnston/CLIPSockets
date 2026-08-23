;;; printout, readline and get-char on a connected plaintext UDP socket.
;;;
;;; examples/server-udp.bat and examples/client-udp.bat use this path, and no
;;; test covered it. udp-datagram.clp connects a datagram socket, checks that
;;; the socket now has a logical name, and then closes it. It does not read or
;;; write through that name.
;;;
;;; Router I/O on a datagram socket is not the same as router I/O on a stream.
;;; The three differences below are properties of the socket layer and are not
;;; defects. This file checks them because DTLS has each of them, and because a
;;; reader with a different model writes a program that operates on loopback
;;; and loses data on a real network.
;;;
;;;   1. A datagram has no end mark. A read after its last character blocks on
;;;      a blocking socket and gives EAGAIN on a non-blocking socket. No value
;;;      says "that was the full message".
;;;   2. readline joins datagrams. A message with no newline continues with the
;;;      next message, and the reader cannot see the join.
;;;   3. The code cuts a datagram that is larger than the buffer of the stream.
;;;      It discards the remainder and does not put it in a queue.
;;;
;;; The read end is non-blocking in each section. Point 1 means that a test
;;; which reads one character too many would otherwise wait and not fail.

(load* "tests/lib/expect.clp")
(load* "tests/lib/socket.clp")
(test-suite "udp-router-io")
(test-plan 18)

(defglobal ?*port-a* = 18957)
(defglobal ?*port-b* = 18958)

;;; This size is much larger than any usual stdio buffer. As a result, the cut
;;; below happens on each platform and not only where the buffer is small. The
;;; code makes the string from a chunk of ten characters, and the two values
;;; stay in agreement.
(defglobal ?*chunks*    = 900)
(defglobal ?*oversized* = 9000)

(deffunction run-tests ()
   (bind ?pair (udp-connected-pair ?*port-a* ?*port-b*))
   (bind ?writer      (nth$ 1 ?pair))
   (bind ?reader      (nth$ 2 ?pair))
   (bind ?writer-name (nth$ 3 ?pair))
   (bind ?reader-name (nth$ 4 ?pair))

   (expect-gte "writer socket created" 0 ?writer)
   (expect-gte "reader socket created" 0 ?reader)
   (expect-true "writer has a logical name after connect" ?writer-name)
   (expect-true "reader has a logical name after connect" ?reader-name)

   (fcntl-add-status-flags ?reader O_NONBLOCK)

   ;;=================================================================
   ;; A line written through the name arrives as a line
   ;;=================================================================
   (set-line-buffered ?writer)
   (printout ?writer-name "hello line" crlf)
   (flush-connection ?writer)

   (expect-true "the line is readable" (poll ?reader 5000 POLLIN))
   (expect-eq "readline returns what printout wrote" "hello line"
              (readline ?reader-name))

   ;;=================================================================
   ;; A datagram has no end
   ;;=================================================================
   (sendto ?writer AF_INET "127.0.0.1" ?*port-b* "AB")
   (expect-true "the two characters are readable" (poll ?reader 5000 POLLIN))
   (expect-eq "first character" 65 (get-char ?reader-name))
   (expect-eq "second character" 66 (get-char ?reader-name))
   ;; This is not EOF. The datagram is complete and the socket is not closed,
   ;; and no value tells the two conditions apart.
   (expect-eq "reading past the datagram gives no character" -1
              (get-char ?reader-name))
   (expect-errno "reading past the datagram leaves EAGAIN" EAGAIN)

   ;;=================================================================
   ;; readline joins datagrams
   ;;=================================================================
   (sendto ?writer AF_INET "127.0.0.1" ?*port-b* "part1")
   (sendto ?writer AF_INET "127.0.0.1" ?*port-b* "part2
")
   (expect-true "both datagrams are readable" (poll ?reader 5000 POLLIN))
   ;; Two messages give one line. On a real network the second message could
   ;; be lost, or it could arrive first, and this call would still give a
   ;; line.
   (expect-eq "a message without a newline is continued by the next one"
              "part1part2" (readline ?reader-name))
   (expect-false "nothing is left after the joined line"
                 (poll ?reader 200 POLLIN))

   ;;=================================================================
   ;; An oversized datagram is truncated, and the rest is gone
   ;;=================================================================
   (bind ?big "")
   (loop-for-count ?*chunks* do
      (bind ?big (str-cat ?big "0123456789")))

   (expect-eq "the whole datagram is sent" ?*oversized*
              (sendto ?writer AF_INET "127.0.0.1" ?*port-b* ?big))
   (expect-true "the datagram is readable" (poll ?reader 5000 POLLIN))

   ;; The buffer of the stream sets the position of the cut, and that
   ;; position differs between platforms. As a result, this check tests only
   ;; that the code cut the data. The next check is the important one.
   (bind ?readable (count-readable-chars ?reader-name))
   (expect-lte "an oversized datagram is truncated" (- ?*oversized* 1) ?readable)

   ;; The remainder does not wait behind the cut. The kernel discarded it,
   ;; and no later read finds it.
   (expect-false "the truncated remainder is discarded, not queued"
                 (poll ?reader 200 POLLIN))

   (close-connection ?writer)
   (close-connection ?reader))

(run-tests)
(test-summary)
