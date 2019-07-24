#export WLOG_LEVEL=DEBUG
#export WLOG_LEVEL=TRACE

# smartcardlogon without NLA
./client/X11/xfreerdp /v:${RDP_SERVER} /u:${RDP_USER} /smart-sizing /smartcard /smartcard-logon -sec-nla

