# test RDP smartcard redirect with NLA
export WLOG_PREFIX='%yr%mo%dyT%hr%mi%se.%ml:%fl:%ln:%lv %fn '
export WLOG_APPENDER=FILE
export WLOG_FILEAPPENDER_OUTPUT_FILE_PATH=log
export WLOG_FILEAPPENDER_OUTPUT_FILE_NAME=freerdp-macos-smartcard-logon-nla.log
export WLOG_LEVEL=DEBUG

mkdir -p log

./client/Mac/cli/MacFreeRDP.app/Contents/MacOS/MacFreeRDP /v:${RDP_SERVER} /u:${RDP_USER} /d:${RDP_DOMAIN} /smartcard

# check logs
grep -e 'PCSC_SCardStatus_Internal\|smartcard_trace_status_return' log/${WLOG_FILEAPPENDER_OUTPUT_FILE_NAME} | tail -15
