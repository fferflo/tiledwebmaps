#!/bin/bash
set -e -u -x

BUILD_PLATFORM="manylinux2014_x86_64"
OUTPUT_PATH="$1"
SCRIPTS_PATH="$(dirname "$(readlink -fm "$0")")"

mkdir -p $OUTPUT_PATH

docker pull quay.io/pypa/$BUILD_PLATFORM

for MINOR in 8 9 10 11; do
    docker run -t --rm -e BUILD_PLATFORM=$BUILD_PLATFORM -e BUILD_PYTHON_ROOT_PATH=/opt/python/cp3$MINOR-cp3$MINOR -e BUILD_MINOR=$MINOR -v $OUTPUT_PATH:/io -v $SCRIPTS_PATH:/scripts quay.io/pypa/$BUILD_PLATFORM sh /scripts/build_in_docker.sh
done
