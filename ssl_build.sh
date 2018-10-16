
#!/bin/bash
set -e
IP=$1
LOCATION=$2
TIMEZONE=$3
C=$PWD

echo "set device details in code"
./generate_code.sh $LOCATION $TIMEZONE
echo "create certificate"
./create_cert.sh $IP
echo "build code"
make -f make.lin30

cp ./Build/mainArduino/mainArduino_generic/mainArduino.bin $C/$IP.bin
