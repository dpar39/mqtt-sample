#!/usr/bin/env bash
# add DNS:{{YOUR_DNS_NAME}} or IP:{{YOUR_STATIC_IP}} to the subject alt names when we've got those (example: subjectAltName=DNS:localhost,IP:127.0.0.1,DNS:objectdetector.com)
set -e

REPO_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
pushd $REPO_ROOT >/dev/null

openssl genrsa -out ca.key 4096
openssl req -new -key ca.key -x509 -out ca.crt -days 365 -batch -reqexts SAN -config <(cat /etc/ssl/openssl.cnf <(printf "[SAN]\nsubjectAltName=DNS:localhost,IP:127.0.0.1"))
openssl req -new -nodes -newkey rsa:4096 -keyout server.key -out server.req -batch -subj "/CN=localhost" -reqexts SAN -config <(cat /etc/ssl/openssl.cnf <(printf "[SAN]\nsubjectAltName=DNS:localhost,IP:127.0.0.1"))
openssl x509 -req -in server.req -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days 365 -sha256 -extfile <(printf "subjectAltName=DNS:localhost,IP:127.0.0.1")
rm ca.key ca.srl server.req

popd >/dev/null