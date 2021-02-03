#!/bin/sh

set -e
#set -x

try_legacy_upgrade_script()
{
    done=
    for file in /dk/image_upgrade_root /dk/image_upgrade
    do
        if [ -f "$file" ] ; then
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
        apt-get -qq -y --no-install-recommends update
        apt-get -qq -y --no-install-recommends upgrade --show-upgraded
        apt-get -qq -y clean
        exit 0
    fi
}

try_alpine_upgrade()
{
    if [ -f /etc/alpine-release ]
    then
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
        set +e
        out="`yum update -y 2>&1`"
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
    echo "OS_NOT_SUPPORTED"
    exit 96
}

(
    try_legacy_upgrade_script
    try_debian_upgrade
    try_alpine_upgrade
    try_redhat_upgrade

    not_supported
) 2>&1 </dev/null
exit $?
