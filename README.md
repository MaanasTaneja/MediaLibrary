# Media Library

A work-in-progress media manipulation API written in C flavoured C++ using FFMPEG LibAV, currently capable of encoding, decoding, remuxing and transcoding files with a simple interface.
The Library can also be used to capture, process RTP stream packets for use in video streaming applications. The goal is to make a library that can be used to edit videos programtically, sorta like Premiere Pro but without the UI.

## Features
-Encode/Decode/Transcode data packets in most popular codecs such as H264, H265, VP9, VP8 and so on.

-Written using the FFMPEG LibAV libraries.
 
-Also has a simple windowing system, can be used to display decoded video files, (check demo functions).

## File Structure

-media.h - main library header, simply include this and you should be good to go. (FFMPEG libraries have to be linked seperately).

-media.cpp - Contains all source.

-graphics.h - Windowing system, optional, not part of library but useful for testing.