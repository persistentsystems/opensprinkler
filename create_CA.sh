#!/bin/bash

# 1024 or 512.  512 saves memory...
BITS=$1

pushd ~/CA/CA$BITS

openssl genrsa -out ca_key.pem $BITS

cat > certs.conf <<EOF
[ req ]
distinguished_name = req_distinguished_name
prompt = no

[ req_distinguished_name ]
O = 3peCA
CN = 3peCA.com
EOF

openssl req -out ca_x509.req -key ca_key.pem -new -config certs.conf 
openssl x509 -req -in ca_x509.req  ca_x509.pem -sha256 -days 5000 -signkey ca_key.pem 
openssl x509 -in ca_key.pem -outform DER -out ca_x509.cer

popd