echo "Quit the MacFreeRDP UI started in background"
./client/Mac/cli/MacFreeRDP.app/Contents/MacOS/MacFreeRDP /buildconfig /version 2>&1 >buildconfig.txt

for i in `grep "Build configuration:" buildconfig.txt | awk -F ': ' '{ print $2 }'`; do echo $i; done > build_options.txt
