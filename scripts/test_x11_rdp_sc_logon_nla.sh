# test RDP smartcard logon with NLA
export WLOG_PREFIX='%yr%mo%dyT%hr%mi%se.%ml:%fl:%ln:%lv %fn '
export WLOG_APPENDER=FILE
export WLOG_FILEAPPENDER_OUTPUT_FILE_PATH=log
export WLOG_FILEAPPENDER_OUTPUT_FILE_NAME=freerdp-x11-smartcard-logon-nla.log
export WLOG_LEVEL=DEBUG

# smartcard logon with NLA
./client/X11/xfreerdp /v:${RDP_SERVER} /d:${RDP_DOMAIN} /smart-sizing /smartcard /smartcard-logon /pkcs11-module:/usr/lib/x86_64-linux-gnu/opensc-pkcs11.so -sec-tls -sec-rdp

# NLA example
# ./client/X11/xfreerdp /v:${RDP_SERVER} /d:${RDP_DOMAIN} /smartcard /smartcard-logon /pkcs11-module:/usr/local/lib/libiaspkcs11.so /pkinit-anchors:/etc/chain1.pem,/etc/chain2.pem /csp:'Middleware IAS ECC Cryptographic Provider'

# check logs
grep -e 'PCSC_SCardStatus_Internal\|smartcard_trace_status_return' log/${WLOG_FILEAPPENDER_OUTPUT_FILE_NAME} |head -15
