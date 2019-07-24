cd /tmp
git clone https://github.com/martinl/FreeRDP.git freerdp-smartcard-logon-nla
cd freerdp-smartcard-logon-nla/
git checkout smartcard-logon-nla
cmake -DWITH_SSE2=ON -DWITH_GSSAPI=ON -DWITH_PKCS11H=ON -DWITH_KERBEROS=ON -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DWITH_CUNIT=ON -DWITH_DEBUG_NTLM=ON -DWITH_DEBUG_NEGO=ON -DWITH_DEBUG_NLA=ON -DWITH_DEBUG_SCARD=ON . && make && make CTEST_OUTPUT_ON_FAILURE=1 test

