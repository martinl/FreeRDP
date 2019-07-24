#export WLOG_LEVEL=DEBUG
#export WLOG_LEVEL=TRACE

# smartcard redirect without NLA
./client/X11/xfreerdp /v:${RDP_SERVER} /u:${RDP_USER} /d:${RDP_DOMAIN} /smartcard -sec-nla

