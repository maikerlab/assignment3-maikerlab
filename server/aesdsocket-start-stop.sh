#!/bin/sh
#
# Starts/stops aesdsocket binary.
#

EXECUTABLE=/usr/bin/aesdsocket
PIDFILE=/var/run/aesdsocket.pid

case "$1" in 
    start)
        echo "Starting aesdsocket..."
        start-stop-daemon \
            --start \
            --background \
            --make-pidfile \
            --pidfile $PIDFILE \
            --exec $EXECUTABLE -- -d
        ;;
    stop)
        echo "Stopping aesdsocket..."
        start-stop-daemon \
            --stop \
            --pidfile $PIDFILE \
            --signal TERM
        ;;
    *)
        echo "Usage: $0 {start|stop}"
    exit 1
esac
