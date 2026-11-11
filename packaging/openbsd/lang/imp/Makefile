COMMENT =		compiler for the IMP72 systems programming language

GH_ACCOUNT =		moebiusV
GH_PROJECT =		imp
GH_TAGNAME =		v0.10.0

CATEGORIES =		lang

HOMEPAGE =		https://github.com/moebiusV/imp72

MAINTAINER =		David Walther <tederific@gmail.com>

# ISC; the DECUS sources in decus/ are in the public domain
PERMIT_PACKAGE =	Yes

# amd64 is the only architecture the test suite has been run on
ONLY_FOR_ARCHS =	amd64

WANTLIB +=		LLVM c c++ c++abi m pthread

USE_GMAKE =		Yes

# lld is the base system's ld.lld, which the compiler runs to link
MAKE_ENV +=		LLVM_NO_RPATH=1
MAKE_FLAGS =		CC="${CC}" CXX="${CXX}" CFLAGS="${CFLAGS}" \
			LDFLAGS="${LDFLAGS}" MANDIR="${PREFIX}/man"

TEST_TARGET =		check

.include <bsd.port.mk>
