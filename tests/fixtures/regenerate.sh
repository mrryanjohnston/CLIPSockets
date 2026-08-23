#!/bin/sh
# Makes the TLS test fixtures again.
#
#   ./tests/fixtures/regenerate.sh
#
# The result is a temporary certificate authority and one leaf certificate for
# the loopback names. Each file is local to this checkout.
#
# The repository holds none of these files, and it must hold none of them. A
# certificate authority has value only while its private key is secret, and a
# key in a repository is secret from no one. Anyone with a copy could sign a
# certificate for any name, and each machine that trusted this CA would accept
# that certificate. As a result, this script makes the files for each checkout.
#
# tests/run.sh calls this script when the files are absent, and you
# usually do not need to run it yourself. Run it when the names in the
# certificate must change, or after the certificate expires.

set -eu

cd "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"

DAYS=7300   # 20 years, so the suite does not start failing on a quiet Tuesday

rm -f ca-key.pem ca.pem server-key.pem server.pem server.csr ca.srl
rm -f wildcard-key.pem wildcard.pem wildcard.csr
rm -f client-key.pem client.pem client.csr
rm -rf bad trust

# --- certificate authority ---------------------------------------------
openssl req -x509 -newkey rsa:2048 -nodes \
	-keyout ca-key.pem -out ca.pem -days "$DAYS" \
	-subj "/CN=CLIPSockets Test CA" \
	-addext "basicConstraints=critical,CA:TRUE" \
	-addext "keyUsage=critical,keyCertSign,cRLSign" 2>/dev/null

# --- leaf, valid for the loopback names --------------------------------
openssl req -newkey rsa:2048 -nodes \
	-keyout server-key.pem -out server.csr \
	-subj "/CN=localhost" 2>/dev/null

openssl x509 -req -in server.csr -days "$DAYS" \
	-CA ca.pem -CAkey ca-key.pem -CAcreateserial \
	-out server.pem \
	-extfile - <<-'EXT' 2>/dev/null
	basicConstraints=critical,CA:FALSE
	keyUsage=critical,digitalSignature,keyEncipherment
	extendedKeyUsage=serverAuth
	subjectAltName=DNS:localhost,IP:127.0.0.1,IP:::1
	EXT

# --- leaf, for a client proving who it is ------------------------------
# This certificate is separate from the server certificate, because the two are
# not equivalent. extendedKeyUsage gives the end that a certificate is for, and
# a library that applies that field refuses a serverAuth certificate from a
# client. The client authentication tests need a certificate for that purpose.
#
# There is no subjectAltName. The code compares a client certificate with the
# trust chain and not with a host name, and there is no name to compare.
openssl req -newkey rsa:2048 -nodes \
	-keyout client-key.pem -out client.csr \
	-subj "/CN=CLIPSockets Test Client" 2>/dev/null

openssl x509 -req -in client.csr -days "$DAYS" \
	-CA ca.pem -CAkey ca-key.pem -CAcreateserial \
	-out client.pem \
	-extfile - <<-'EXT' 2>/dev/null
	basicConstraints=critical,CA:FALSE
	keyUsage=critical,digitalSignature,keyEncipherment
	extendedKeyUsage=clientAuth
	EXT

# --- leaf, carrying wildcard names -------------------------------------
# This certificate is for the checks of what a wildcard can match. It has two
# names, and no code connects to either of them. The code opens the socket to
# 127.0.0.1 and verifies only the name that it gives to (tls-connect). As a
# result, these names never go to a resolver.
#
#   *.wild.clipsockets  the usual condition. It must cover one label and only
#                       one label. It must not cover the bare domain, and it
#                       must not cover two labels.
#
#   *.0.0.1             a wildcard with the shape of an address. It must not
#                       match 127.0.0.1. Anyone can get a certificate for a
#                       name. A certificate that matched addresses by wildcard
#                       would be a certificate for each host.
openssl req -newkey rsa:2048 -nodes \
	-keyout wildcard-key.pem -out wildcard.csr \
	-subj "/CN=*.wild.clipsockets" 2>/dev/null

openssl x509 -req -in wildcard.csr -days "$DAYS" \
	-CA ca.pem -CAkey ca-key.pem -CAcreateserial \
	-out wildcard.pem \
	-extfile - <<-'EXT' 2>/dev/null
	basicConstraints=critical,CA:FALSE
	keyUsage=critical,digitalSignature,keyEncipherment
	extendedKeyUsage=serverAuth
	subjectAltName=DNS:*.wild.clipsockets,DNS:*.0.0.1
	EXT

# --- files that are wrong in each of the ways a file can be ------------
# A TLS library reads most of its external data when it loads a certificate or
# a key. Most of its error code is in that path. A fixture that parses
# reaches none of that code. A suite with only correct files never shows what
# happens with an incorrect file, and no one else knows until it happens to
# them.
#
# Each of these files is incorrect in a different manner, because the libraries
# fail at different points and one incorrect file does not replace another.
# tests/error/tls-bad-fixtures.clp holds the checks for these files.
mkdir -p bad

# This file is not a certificate. It fails at the first line, before the
# decode.
cat > bad/not-a-pem.txt <<-'EOF'
	This file is not a certificate. It is a sentence about one.
EOF

# A correct header above base64 data that is incomplete. This file fails during
# the decode and not before it, and on some libraries that is a different
# path.
{
	echo "-----BEGIN CERTIFICATE-----"
	sed -n '2,4p' server.pem
} > bad/truncated.pem

# An empty file. It opens, it reads correctly, and it gives no object. That is
# not the same as a file that does not open.
: > bad/empty.pem

# A certificate in the place of a key. It parses correctly and is the incorrect
# type of object. A library makes that check last.
cp server.pem bad/cert-as-key.pem

# A correct key that belongs to a different certificate. The file is correct
# until the code pairs it with a certificate. As a result, it reaches the check
# that runs only with both halves present. That check gives an error now
# instead of a handshake failure later with no cause.
cp client-key.pem bad/mismatched-key.pem

# A directory in the place of a file. It opens, and the code cannot read
# it.
mkdir -p bad/a-directory.pem

# --- a trust directory ------------------------------------------------
# load-verify-locations takes a directory and also a file, and the two families
# read a directory differently. OpenSSL keeps the path and looks in the
# directory later, by hash. mbedTLS and GnuTLS parse each file in the directory
# immediately. As a result, a directory with private keys in it is correct for
# one family and an error for the other. tests/fixtures is such a directory.
#
# This directory holds trust anchors and no other file. That is the correct
# content of a trust directory, and it operates for the two families.
mkdir -p trust
cp ca.pem trust/ca.pem

rm -f server.csr client.csr wildcard.csr ca.srl

echo "regenerated: ca.pem ca-key.pem server.pem server-key.pem client.pem client-key.pem wildcard.pem wildcard-key.pem"
echo "trust directory: trust/ca.pem"
echo "deliberately broken: bad/not-a-pem.txt bad/truncated.pem bad/empty.pem bad/cert-as-key.pem bad/mismatched-key.pem bad/a-directory.pem"
openssl x509 -in server.pem -noout -subject -ext subjectAltName -enddate
openssl x509 -in client.pem -noout -subject -enddate
openssl x509 -in wildcard.pem -noout -subject -ext subjectAltName -enddate
