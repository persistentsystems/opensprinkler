#!/bin/bash
LOCATION=$1
TIMEZONE=$2
SVRPWD=openssl rand -base64 32
USRPWD=openssl rand -base64 32
json='{"userPwd": "$USRPWD", "servicePwd": "SVRPWD"}
echo $json > passwords.json
sed -i 's/GEN3PESERVICEPWD/$SVRPWD/g' defines.h
sed -i 's/GEN3PEUSERPWD/$USRPWD/g' defines.h
sed -i 's/GEN3PELOCATION/$LOCATION/g' defines.h
sed -i 's/GEN3PETIMEZONE/$TIMEZONE/g' OpenSprinkler.cpp


