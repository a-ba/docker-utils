#!/bin/sh

set -e
#set -x

FIFO="/.docker-upgrade-fifo"

log_fifo()
{
    if [ -p "$FIFO" ] ; then
        cat >"$FIFO" <<EOF || true
$*
EOF
    fi
}

try_legacy_upgrade_script()
{
    done=
    for file in /dk/image_upgrade_root /dk/image_upgrade
    do
        if [ -f "$file" ] ; then
            [ -n "$done" ] || log_fifo "legacy"
            done=1
            "$file"
        fi
    done
    if [ -n "$done" ] ; then
        exit 0
    fi
}

try_debian_upgrade()
{
    if [ -f /etc/debian_version ]
    then
        log_fifo "debian"

        export DEBIAN_FRONTEND=noninteractive

        if apt-get update --allow-releaseinfo-change --ERROR  2>&1 | grep -q allow-releaseinfo-change ; then
                apt-get -qq -y update
        else
                apt-get -qq -y update --allow-releaseinfo-change-suite
        fi

        apt-get -qq -y --no-install-recommends upgrade --show-upgraded
        apt-get -qq -y clean
        exit 0
    fi
}

try_alpine_upgrade()
{
    if [ -f /etc/alpine-release ]
    then
        log_fifo "alpine"

        set +e
        date="`date`"
        out="`apk upgrade -U 2>&1`"
        code=$?
        if [ "$code" -ne 0 ]
        then
            # non-zero exit
            filter() { cat ; }
        else
            # success
            # ensure the output is empty when no packages were upgraded
            filter() { egrep -v '^(fetch |OK:)' ; } 
            # keep it in a log file
            gzip -9c >>/var/log/apk-upgrade.log.gz << EOF
======= apk upgrade launched on $date =======

$out

EOF
        fi
        filter <<EOF
$out
EOF
        exit "$code"
    fi
}

try_redhat_upgrade()
{
    if [ -f /etc/redhat-release ]
    then
        log_fifo "redhat"

	candidates="dnf microdnf yum"
	for cmd in $candidates . ; do
		if [ "$cmd" = . ] ; then
			echo "error: none of these commandes were found: $candidates" >&2
			exit 1
		fi
		if type "$cmd" >/dev/null 2>&1 ; then
			break
		fi
	done

        set +e
        out="`"$cmd" update -y 2>&1`"
        code=$?
        if [ "$code" -ne 0 ]
        then
            # failure (non-zero exit)
            filter() { cat ; }
        elif grep -q ======== <<EOF
$out
EOF
        then
            # success (have updates)
            filter() { egrep "^(  [A-Z]|Updated)" ; }
        else
            # success (no updates)
            filter() { cat >/dev/null ; }
        fi
        filter <<EOF
$out
EOF
        exit "$code"
    fi
}

not_supported()
{
    log_fifo "not_supported"
    echo "error: operating system not supported by docker-upgrade"
    exit 1
}

(
    try_legacy_upgrade_script
    try_debian_upgrade
    try_alpine_upgrade
    try_redhat_upgrade

    not_supported
) 2>&1 </dev/null
exit $?
