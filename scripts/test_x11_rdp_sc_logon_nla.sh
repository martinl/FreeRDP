#export WLOG_LEVEL=DEBUG
#export WLOG_LEVEL=TRACE

# smartcardlogon with NLA
./client/X11/xfreerdp /v:${RDP_SERVER} /d:${RDP_DOMAIN} /smart-sizing /smartcard /smartcard-logon /pkcs11-module:/usr/lib/x86_64-linux-gnu/opensc-pkcs11.so -sec-tls -sec-rdp

# NLA example
# ./client/X11/xfreerdp /v:${RDP_SERVER} /d:${RDP_DOMAIN} /smartcard /smartcard-logon /pkcs11-module:/usr/local/lib/libiaspkcs11.so /pkinit-anchors:/etc/chain1.pem,/etc/chain2.pem /csp:'Middleware IAS ECC Cryptographic Provider'

