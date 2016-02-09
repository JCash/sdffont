set -e
./compile_clang.sh

#lldb ./sdffont -- -i ../minosaur/content/fonts/verdana.ttf -o output.font -s 76 -w 128 -h 128 -r 10 --paddingleft 1 --paddingright 1 --paddingtop 1 --paddingbottom 1 --numoversampling 2
#time ./sdffont -i ../minosaur/content/fonts/verdana.ttf -o output.font -s 76 -w 128 -h 128 -r 10 --paddingleft 1 --paddingright 1 --paddingtop 1 --paddingbottom 1 --numoversampling 2

#time ./sdffont -i ./examples/helsinki.ttf -o output.font -s 76 -w 128 -h 128 -r 10 --paddingleft 1 --paddingright 1 --paddingtop 1 --paddingbottom 1 --numoversampling 2

#time ./sdffont -i ../minosaur/content/fonts/Tangerine_Bold.ttf -o output.font -s 96 -w 128 -h 128 -r 10 --paddingleft 1 --paddingright 1 --paddingtop 1 --paddingbottom 1 --numoversampling 1

time ./sdffont -i ../minosaur/content/fonts/legendum/Legendum.ttf -o output.font -s 96 -r 10 --paddingleft 1 --paddingright 1 --paddingtop 1 --paddingbottom 1 --numoversampling 2

cp output.font ../minosaur/content/fonts/test.font
cp output.font.png ../minosaur/content/fonts/test.png
