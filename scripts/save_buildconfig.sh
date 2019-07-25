OS=`uname -o`

echo "Saving buildconfig to buildconfig.txt"
if [ $OS == "GNU/Linux" ]; then
	./client/X11/xfreerdp /buildconfig > buildconfig.txt
else
	echo "Quit the MacFreeRDP UI started in background to save buildconfig"
	./client/Mac/cli/MacFreeRDP.app/Contents/MacOS/MacFreeRDP /buildconfig /version 2>&1 >buildconfig.txt
fi

echo "Writing build options to build_options.txt"
for i in `grep "Build configuration:" buildconfig.txt | awk -F ': ' '{ print $2 }'`; do echo $i; done > build_options.txt
