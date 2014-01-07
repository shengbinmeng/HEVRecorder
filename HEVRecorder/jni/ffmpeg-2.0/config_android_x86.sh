#!/bin/bash

#
# change these variables according to your system
#
SYSROOT=$ANDROID_NDK/platforms/android-9/arch-x86
TOOLCHAIN=$ANDROID_NDK/toolchains/x86-4.8/prebuilt/darwin-x86_64
PREFIX=./android/x86

./configure \
    --prefix=$PREFIX \
    --target-os=linux \
    --arch=x86 \
    --cpu=atom \
    --enable-cross-compile \
    --cross-prefix=$TOOLCHAIN/bin/i686-linux-android- \
    --sysroot=$SYSROOT \
    --extra-cflags="-O2 -Ithirdparty/x86 -msse3 -ffast-math -mfpmath=sse" \
    --extra-ldflags="-Lthirdparty/x86" \
    --disable-amd3dnow \
    --disable-amd3dnowext \
    --disable-mmx \
    --enable-static \
    --enable-gpl \
    --enable-version3 \
    --enable-nonfree \
    --disable-doc \
    --disable-htmlpages \
    --disable-manpages \
    --disable-podpages \
    --disable-txtpages \
    --enable-ffmpeg \
    --disable-ffplay \
    --disable-ffserver \
    --disable-ffprobe \
    --disable-zlib \
    --disable-bzlib \
    --disable-avdevice \
    --disable-postproc \
    --disable-avresample \
    --enable-avutil \
    --enable-avformat \
    --enable-avcodec \
    --enable-swresample \
    --enable-swscale \
    --disable-everything \
    --enable-protocol=file \
    --enable-protocol=hls \
    --enable-protocol=http \
    --enable-protocol=httpproxy \
    --enable-protocol=https \
    --enable-decoder=aac \
    --enable-decoder=aac_latm \
    --enable-decoder=ac3 \
    --enable-decoder=mp1 \
    --enable-decoder=mp2 \
    --enable-decoder=mp3 \
    --enable-decoder=mp3adu \
    --enable-decoder=h261 \
    --enable-decoder=h263 \
    --enable-decoder=h263i \
    --enable-decoder=h263p \
    --enable-decoder=h264 \
    --enable-parser=aac \
    --enable-parser=mpegaudio \
    --enable-parser=h261 \
    --enable-parser=h263 \
    --enable-parser=h264 \
    --enable-demuxer=aac \
    --enable-demuxer=avi \
    --enable-demuxer=flv \
    --enable-demuxer=h261 \
    --enable-demuxer=h263 \
    --enable-demuxer=h264 \
    --enable-demuxer=m4v \
    --enable-demuxer=mov \
    --enable-demuxer=mp3 \
    --enable-demuxer=mpegts \
    --enable-demuxer=image2 \
    --enable-demuxer=hls \
    --enable-encoder=h263 \
    --enable-encoder=aac \
    --enable-muxer=mpegts \
    --enable-muxer=flv \