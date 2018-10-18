#!/bin/bash
set -e
LOCATION="$1"
TIMEZONE=$2
echo "generating passwords"
SVRPWD=$(pwgen -cns 32 1)
USRPWD=$(pwgen -cnB 32 1)
USRPWDMD5=$(printf '%s' $USRPWD | md5sum | cut -d ' ' -f 1)

echo $USRPWDMD5
echo "saving passwords to json"
jq -n --arg USRPWD $USRPWD --arg SVRPWD $SVRPWD '{"userPwd": $USRPWD, "servicePwd": $SVRPWD }' > secure.json
echo "modifing code ..."
echo $LOCATION
echo $TIMEZONE

sed -i "s|GEN3PESERVICEPWD|$SVRPWD|g" defines.h
sed -i "s|GEN3PEUSERPWD|$USRPWDMD5|g" defines.h
sed -i "s|GEN3PELOCATION|$LOCATION|g" defines.h
sed -i "s|GEN3PETIMEZONE|$TIMEZONE|g" OpenSprinkler.cpp


