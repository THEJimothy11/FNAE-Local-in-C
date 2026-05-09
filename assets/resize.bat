@echo off
REM resize.bat
REM Run from assets\ before convimg.
REM Requires ImageMagick on PATH.
REM WARNING: modifies images in-place. Keep originals backed up.

echo [1/6] Resizing camera feeds and characters to 160x120...
magick mogrify -resize 160x120! ^
  images\Cam1.png images\Cam2.png images\Cam3.png ^
  images\Cam4.png images\Cam5.png images\Cam6.png ^
  images\Cam7.png images\Cam8.png images\Cam9.png ^
  images\Cam10.png images\Cam11.png ^
  images\enemyep1.png images\enemyep4.png ^
  images\ep1.png images\ep4.png ^
  images\mrstephen.png ^
  images\trump2.png images\trump4.png images\trump5.png

echo [2/6] Resizing map layout to 160x240...
magick mogrify -resize 160x240! images\map_layout.png

echo [3/6] Resizing warning indicators and star to 32x32...
magick mogrify -resize 32x32! ^
  images\Warningheavy.png images\Warninglight.png images\star.png

echo [4/6] Resizing explosion to 160x40 and fa3 to 80x80...
magick mogrify -resize 160x40! images\exp2.png
magick mogrify -resize 80x80! images\fa3.png

echo [5/6] Resizing full-screen sprites to 320x240...
magick mogrify -resize 320x240! ^
  images\jump.png images\menubackground.png ^
  images\original.png images\scaryhawking.png

echo [6/6] Converting backgrounds to grayscale...
REM Camera feeds - grayscale for night-vision look
magick mogrify -colorspace Gray ^
  images\Cam1.png images\Cam2.png images\Cam3.png ^
  images\Cam4.png images\Cam5.png images\Cam6.png ^
  images\Cam7.png images\Cam8.png images\Cam9.png ^
  images\Cam10.png images\Cam11.png

REM Environment/UI sprites - grayscale
magick mogrify -colorspace Gray ^
  images\original.png ^
  images\menubackground.png ^
  images\map_layout.png ^
  images\jump.png ^
  images\fa3.png ^
  images\exp2.png ^
  images\star.png ^
  images\Warningheavy.png ^
  images\Warninglight.png

REM Characters stay COLOUR: enemyep1 enemyep4 ep1 ep4
REM                          mrstephen trump2 trump4 trump5 scaryhawking

echo Done. Now run: rd /s /q sprites ^&^& mkdir sprites ^&^& convimg