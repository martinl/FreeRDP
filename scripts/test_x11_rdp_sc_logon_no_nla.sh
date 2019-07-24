# test RDP smartcard logon without NLA
export WLOG_PREFIX='%yr%mo%dyT%hr%mi%se.%ml:%fl:%ln:%lv %fn '
export WLOG_APPENDER=FILE
export WLOG_FILEAPPENDER_OUTPUT_FILE_PATH=log
export WLOG_FILEAPPENDER_OUTPUT_FILE_NAME=freerdp-x11-smartcard-logon-no-nla.log
export WLOG_LEVEL=DEBUG

mkdir -p log

# smartcardlogon without NLA
./client/X11/xfreerdp /v:${RDP_SERVER} /u:${RDP_USER} /smart-sizing /smartcard /smartcard-logon -sec-nla

# check logs
grep -e 'PCSC_SCardStatus_Internal\|smartcard_trace_status_return' log/${WLOG_FILEAPPENDER_OUTPUT_FILE_NAME} | tail -15
