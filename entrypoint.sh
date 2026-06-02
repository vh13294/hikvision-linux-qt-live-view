#!/bin/bash
set -e

PUID=${PUID:-1000}
PGID=${PGID:-1000}

groupmod -o -g "$PGID" developer
usermod -o -u "$PUID" developer
chown -R developer:developer /home/developer

exec gosu developer "$@"
