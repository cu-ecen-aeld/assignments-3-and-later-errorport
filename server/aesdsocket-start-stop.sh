#!/bin/bash

case "$1" in
    start)
        echo "Starting the socket daemon"
        start-stop-daemon -S -n aesdsocket -d
        ;;
    stop)
        echo "Stopping the socket daemon"
        start-stop-daemon -K -n aesdsocket
        ;;
    *)
        echo "Usage: $0 {start|stop}"
        exit 1
esac

exit 0
