find . -name CMakeFiles | xargs rm -r
find . -name CMakeCache.txt | xargs rm

cmake -DWITH_PCSC=ON -DWITH-FFMPEG=ON -DWITH_GSSAPI=ON .
#cmake -DWITH_PCSC=ON -DWITH-FFMPEG=ON -DWITH_GSSAPI=ON --debug-output . > debug-output-gss-set.txt
cmake --build .
