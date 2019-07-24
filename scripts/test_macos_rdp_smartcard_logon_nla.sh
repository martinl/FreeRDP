export WLOG_PREFIX='%yr%mo%dyT%hr%mi%se.%ml:%fl:%ln:%lv %fn '
export WLOG_APPENDER=FILE
export WLOG_FILEAPPENDER_OUTPUT_FILE_PATH=log
export WLOG_FILEAPPENDER_OUTPUT_FILE_NAME=freerdp-macos-smartcard-logon-nla.log
export WLOG_LEVEL=DEBUG

echo "---------------" >> log/${WLOG_FILEAPPENDER_OUTPUT_FILE_NAME}
./client/Mac/cli/MacFreeRDP.app/Contents/MacOS/MacFreeRDP /v:${RDP_SERVER} /u:${RDP_USER} /d:${RDP_DOMAIN} /smartcard /smartcard-logon /sec:nla /T /pkcs11-module:/usr/local/lib/opensc-pkcs11.so

# check logs
#grep -e 'PCSC_SCardStatus_Internal\|smartcard_trace_status_return' log/${WLOG_FILEAPPENDER_OUTPUT_FILE_NAME} | tail -50
tail -50 log/${WLOG_FILEAPPENDER_OUTPUT_FILE_NAME}
