
#!/bin/bash
set -e
IP=$1
LOCATION="$2"
TIMEZONE=$3
CERTBITS=$4
VERSION=${5:-1}
C=$PWD


echo "create certificate"
./create_cert.sh $IP $CERTBITS

echo "set device details in code"

./generate_code.sh "$LOCATION" $TIMEZONE $CERTBITS
echo "build code"
make -f make.V$VERSION

