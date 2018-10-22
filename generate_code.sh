#!/bin/bash
set -e
LOCATION="$1"
TIMEZONE=$2
BITS=$3
echo "generating passwords"
SVRPWD=$(pwgen -cns 32 1)
USRPWD=$(pwgen -cnB 32 1)
#USRPWDMD5=$(printf '%s' $USRPWD | md5sum | cut -d ' ' -f 1)
SERIAL=$(openssl x509 -inform DER -in tls.x509_$BITS.cer -noout -serial | sed 's/serial=//')

echo $SERIAL

echo "saving passwords to json and certificate serial"
jq -n --arg USRPWD $USRPWD --arg SVRPWD $SVRPWD --arg SERIAL $SERIAL '{"userPwd": $USRPWD, "servicePwd": $SVRPWD, "serial": $SERIAL }' > secure.json
echo "modifing code ..."
echo $LOCATION
echo $TIMEZONE

sed -i "s|GEN3PESERVICEPWD|$SVRPWD|g" defines.h
sed -i "s|GEN3PEUSERPWD|$USRPWD|g" defines.h
sed -i "s|GEN3PELOCATION|$LOCATION|g" defines.h
sed -i "s|GEN3PETIMEZONE|$TIMEZONE|g" OpenSprinkler.cpp


