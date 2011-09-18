#!/bin/sh

[[ "$PWD" == */slate-language/pkg/rpm ]] || { echo "Sorry, yum must run this script from the pkg/rpm directory in the slate git-repository!"; exit 1; }

SELF=$0
HELPMSG="\n
\e[1m${SELF##*\/}\e[0m - prepare the building of a slate-language rpm from the git-repository\n
\n
\e[1mUsage:\e[0m\t ${SELF##*\/} [\e[4moptions\e[0m] -t|--topdir\n
\n
\e[1mOptions:\e[0m\n
\t -h --help \t\t\t display this help message\n
\t -av --autoversion \t\t automatically choose the newest commit-version as version-suffix\n
\t\t\t\t\t (prefix: git, version will look like 'git\e[4mversion-suffix\e[0m'\n
\t -v --version=\e[4mversion\e[0m \t\t specify the \e[4mversion\e[0m manually\n
\t -v --branch=\e[4mpackage branch\e[0m \t specify the branch from which the rpm should be built (standard: master)\n
\t --use-compress-program=\e[4mprogram\e[0m\t specify the \e[4mprogram\e[0m used for compressing the tarball (standard: gzip)\n
\t -t --topdir=\e[4mtopdir\e[0m \t\t specify the \e[4mtopdir\e[0m where the rpm-build-directories lie (e.g. SPECS,BUILDROOT,SOURCES ...)\n
"

# evaluate the arguments
for arg; do
	case "$prevarg" in
		-v ) VERSION="git$arg";;
		-r ) RELEASE="$arg";;
		-t ) TOPDIR="$arg";;
		-b ) BRANCH="$arg";;
	esac
	case "$arg" in
		-h | --help )			echo -e $HELPMSG; exit;;
		-av | --autoversion )		VERSION="auto";;
		--version* )			VERSION="git${arg#*\=}";;
		--branch* )			BRANCH="${arg#*\=}";;
		--use-compress-program* )	COMPRESSOR="${arg#*\=}";;
		--release )			RELEASE="$arg"TOPDIR="${arg#*\=}";;
		--topdir* )			TOPDIR="${arg#*\=}";;
	esac
	prevarg=$arg
done
unset prevarg

test "$TOPDIR" || { echo -e "\n Sorry, you must specify a TOPDIR.\n"$HELPMSG; exit 2; }

if [ "$VERSION" = 'auto' ]; then
	VERSION="`git log | grep commit`"
	VERSION="${VERSION#commit\ }"
	VERSION="${VERSION%%commit*}"
	VERSION="git$VERSION"
fi
VERSION=`echo $VERSION`

sed s/slateversion/"$VERSION"/g < slate-language.spec.template > $TOPDIR/SPECS/slate-language.spec
cd ../..
git archive --prefix=slate-language-$VERSION/ --output=slate-language-$VERSION.tar ${BRANCH:=master} && ${COMPRESSOR:=gzip} slate-language-$VERSION.tar
mv slate-language-$VERSION.tar.* $TOPDIR/SOURCES
