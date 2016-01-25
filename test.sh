set -e
./compile_clang.sh
#./sdffont -i ../minosaur/content/fonts/verdana.ttf -o output.font -s 96 -w 1024 -h 1024 -r 10 --paddingleft 1 --paddingright 1 --paddingtop 1 --paddingbottom 1 --numoversampling 1
#lldb ./sdffont -- -i ../minosaur/content/fonts/verdana.ttf -o output.font -s 76 -w 128 -h 128 -r 10 --paddingleft 1 --paddingright 1 --paddingtop 1 --paddingbottom 1 --numoversampling 2
time ./sdffont -i ../minosaur/content/fonts/verdana.ttf -o output.font -s 76 -w 128 -h 128 -r 10 --paddingleft 1 --paddingright 1 --paddingtop 1 --paddingbottom 1 --numoversampling 2
cp output.font ../minosaur/content/fonts/test.font
cp output.font.png ../minosaur/content/fonts/test.png
