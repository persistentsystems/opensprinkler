#!/bin/bash
set -e
LOCATION=$1
TIMEZONE=$2
echo "generating passwords"
SVRPWD=$(openssl rand -base64 32)
USRPWD=$(openssl rand -base64 32)
USRPWDMD5=$(echo $USRPWD | md5sum)
echo $USRPWDMD5
echo "saving passwords to json"
jq -n --arg USRPWD $USRPWD --arg SVRPWD $SVRPWD '{"userPwd": $USRPWD, "servicePwd": $SVRPWD }' > passwords.json
echo "modifing code ..."
sed -i "s|GEN3PESERVICEPWD|$SVRPWD|g" defines.h
sed -i "s|GEN3PEUSERPWD|$USRPWDMD5|g" defines.h
sed -i "s|GEN3PELOCATION|$LOCATION|g" defines.h
sed -i "s|GEN3PETIMEZONE|$TIMEZONE|g" OpenSprinkler.cpp


