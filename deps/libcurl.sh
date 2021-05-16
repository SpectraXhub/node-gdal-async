#!/bin/bash

set -eu

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR/libcurl"

CURL_VERSION=7.76.1
dir_curl=./curl

#
# download curl source
#

rm -rf $dir_curl
if [[ ! -f curl-${CURL_VERSION}.tar.gz ]]; then
	curl -L https://curl.se/download/curl-${CURL_VERSION}.tar.gz -o curl-${CURL_VERSION}.tar.gz
fi
tar -xzf curl-${CURL_VERSION}.tar.gz
mv curl-${CURL_VERSION} $dir_curl