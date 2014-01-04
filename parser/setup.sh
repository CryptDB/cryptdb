#!/bin/bash

if [ "x$1" = "x" ]; then
    echo "Usage: $0 shadow-db-dir"
    exit 1
fi

if [ -d "$1" ]; then
    echo "Error: $1 already exists"
    exit 1
fi

mkdir $1

## XXX load schema for persistent metadata storage

