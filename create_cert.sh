#!/bin/bash
set -e
# This script generates a self-signed certificate for use by the ESP8266
# Replace your-name-here with somethine appropriate before running and use
# the generated .H files in your code as follows:
#
#      static const uint8_t rsakey[] PROGMEM = {
#        #include "key.h"
#      };
#
#      static const uint8_t x509[] PROGMEM = {
#        #include "x509.h"
#      };
#
#      ....
#      WiFiServerSecure server(443);
#      server.setServerKeyAndCert_P(rsakey, sizeof(rsakey), x509, sizeof(x509));
#      ....

# 1024 or 512.  512 saves memory...
IP=$1
BITS=512
CA_DIR=~/CA/CA$BITS



openssl genrsa -out tls.key_$BITS.pem $BITS
openssl rsa -in tls.key_$BITS.pem -out tls.key_$BITS -outform DER
cat > certs.conf <<EOF
[ req ]
distinguished_name = req_distinguished_name
prompt = no

[ req_distinguished_name ]
O = 3pe irrigation device
CN =$IP
EOF
openssl req -out tls.x509_$BITS.req -key tls.key_$BITS.pem -new -config certs.conf 
openssl x509 -req -in tls.x509_$BITS.req  -out tls.x509_$BITS.pem -sha256 -CAcreateserial -days 5000 -CA $CA_DIR/ca_x509.pem -CAkey $CA_DIR/ca_key.pem 
openssl x509 -in tls.x509_$BITS.pem -outform DER -out tls.x509_$BITS.cer

xxd -i tls.key_$BITS       | sed 's/.*{//' | sed 's/\};//' | sed 's/unsigned.*//' > "key.h"
xxd -i tls.x509_$BITS.cer  | sed 's/.*{//' | sed 's/\};//' | sed 's/unsigned.*//' > "x509.h"

SERIAL=$(openssl x509 -inform DER -in tls.x509_$BITS.cer -noout -serial | sed 's/serial=//')
echo $SERIAL

cat secure.json | jq --arg serial $SERIAL '.+{"ser" : $serial }' 
