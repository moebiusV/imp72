# mktmpdir.sh: source it (". $(dirname "$0")/mktmpdir.sh") and call `mktmpdir NAME`.  It sets $t to a new directory (mode 0700, unguessable name, made
# atomically by mktemp -d) and removes it when the script ends, fails, or gets SIGHUP, SIGINT or SIGTERM.  TMPDIR is used only if it is an absolute path to
# a directory; else /tmp.  Nothing is ever made in the current directory.
mktmpdir() {
    _d=${TMPDIR:-/tmp}
    case $_d in
    /*) [ -d "$_d" ] || _d=/tmp ;;
    *) _d=/tmp ;;
    esac
    t=$(umask 077; mktemp -d "$_d/$1.XXXXXX") || exit 1
    trap 'rm -rf "$t"' EXIT
    trap 'exit 1' HUP INT TERM
}
