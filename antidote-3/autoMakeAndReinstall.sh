make uninstall
automake
./configure --enable-coverage
make 
make install
chmod -R 777 *
rm /tmp/logPHD
