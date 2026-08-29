#!/bin/sh
# Fetches one CLIPS core source tree into a directory.
#
# Two places a tree can come from, because CLIPSockets is built and tested
# against three of them: the 6.4.2 release tarball on SourceForge, and the
# 64x and 70x branches of the CLIPS Subversion repository.
#
# Usage:
#   scripts/fetch-clips.sh tarball <url> <archive-path> <dest-dir>
#   scripts/fetch-clips.sh svn     <url> <revision>     <dest-dir>
#
# A Subversion revision is pinned rather than tracked, so that a build
# today and a build next month are the same build. Pass HEAD to take
# whatever is there now -- and see the note at the bottom of this file
# about what that costs.
#
# Environment:
#   CLIPS_SHA256   the SHA-256 the tarball must have. The makefile passes the
#                  digest published for 6.4.2. A release does not change, so a
#                  mismatch means either a damaged download or a file that is
#                  no longer the one this project was tested against. Empty
#                  skips the check, which is what a branch export does: there
#                  is no published digest for a moving branch.
#
# On success the tree is in <dest-dir> and <dest-dir>/.clips-source says
# what it is.
set -eu

kind=${1:?usage: fetch-clips.sh <tarball|svn> <url> <archive-path|revision> <dest-dir>}
url=${2:?usage: fetch-clips.sh <tarball|svn> <url> <archive-path|revision> <dest-dir>}
third=${3:?usage: fetch-clips.sh <tarball|svn> <url> <archive-path|revision> <dest-dir>}
dest=${4:?usage: fetch-clips.sh <tarball|svn> <url> <archive-path|revision> <dest-dir>}

want=${CLIPS_SHA256:-}

die() { echo "fetch-clips: $*" >&2; exit 1; }

fetch() {
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL -o "$2" "$1"
    elif command -v wget >/dev/null 2>&1; then
        wget -q -O "$2" "$1"
    else
        die "neither curl nor wget is installed"
    fi
}

sha256() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | cut -d' ' -f1
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$1" | cut -d' ' -f1
    else
        die "no SHA-256 tool found: need sha256sum or shasum"
    fi
}

# A half-written tree is worse than no tree, because make would take it for
# a finished one. Everything lands beside the destination and is moved into
# place only once it is whole.
staging="$dest.incoming"
rm -rf "$staging"
mkdir -p "$staging"
trap 'rm -rf "$staging"' EXIT INT TERM

case "$kind" in
tarball)
    archive=$third
    if [ ! -f "$archive" ]; then
        echo "fetch-clips: downloading $url"
        mkdir -p "$(dirname "$archive")"
        fetch "$url" "$archive.incoming" || die "download failed: $url"
        mv "$archive.incoming" "$archive"
    else
        echo "fetch-clips: using the archive already at $archive"
    fi

    # The digest is checked on the cached archive as well as on a fresh
    # download, so a file that went bad on disk is caught too.
    if [ -n "$want" ]; then
        got=$(sha256 "$archive")
        if [ "$got" != "$want" ]; then
            die "SHA-256 mismatch for $archive
   expected $want
   actual   $got
   Nothing has been unpacked. Either the download was corrupted -- remove
   that file and build again -- or the file SourceForge is serving is no
   longer the one this project was tested against, which is worth
   understanding before building."
        fi
        echo "fetch-clips: SHA-256 $got verified"
    fi

    # clips_core_source_642/core/*.c -> *.c
    tar --strip-components=2 -xf "$archive" -C "$staging" \
        || die "could not unpack $archive"
    description="tarball $url"
    ;;

svn)
    revision=$third
    command -v svn >/dev/null 2>&1 || die "svn is not installed: it is needed
   to build against a CLIPS branch. On Ubuntu-based systems that is
   'sudo apt install subversion'; on macOS, 'brew install subversion'.
   Building against the 6.4.2 release tarball instead needs no svn:
   run make with CLIPS_VERSION=6.4.2, which is the default."

    echo "fetch-clips: exporting $url at r$revision"
    svn export -q --force -r "$revision" "$url" "$staging" \
        || die "svn export failed: $url at r$revision"

    # HEAD is a moving target, so record where it actually landed.
    revision=$(svn info --show-item last-changed-revision -r "$revision" "$url" 2>/dev/null) \
        || revision=$third
    description="svn $url at r$revision"
    ;;

*)
    die "unknown source kind '$kind': expected tarball or svn"
    ;;
esac

[ -f "$staging/clips.h" ] || die "no clips.h in the tree fetched from $url"

printf '%s\n' "$description" > "$staging/.clips-source"

rm -rf "$dest"
mkdir -p "$(dirname "$dest")"
mv "$staging" "$dest"
trap - EXIT INT TERM

echo "fetch-clips: $description -> $dest"

# A note on HEAD: the directory a tree is fetched into is named after the
# revision asked for, so CLIPS_SVN_REV=HEAD reuses whatever HEAD meant the
# first time. Remove that directory to fetch it again.
