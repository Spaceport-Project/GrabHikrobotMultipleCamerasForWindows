// #ifndef CONVERTERFROMBAYER2MP4_H
// #define CONVERTERFROMBAYER2MP4_H
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include "libavutil/time.h"

}
#include <iostream>
#include <string>
#include <stdexcept>
#include <memory>

class ConverterFromBayer2Mp4
{
public:
    ConverterFromBayer2Mp4(const std::string& output_filename, int width, int height, AVPixelFormat pix_fmt_in);
     bool write2Mp4(const std::shared_ptr<uint8_t[]>  &data);
    ~ConverterFromBayer2Mp4();
   

private:
    
   
    AVCodecContext *codec_ctx_ = NULL;
    int width_;
    int height_;
    AVPixelFormat pix_fmt_in_;
    AVFrame *bayer_frame_ = NULL;
    uint8_t *buffer_out_ =NULL;
    AVFrame *yuv_frame_ = NULL;
    AVFrame *frame_out_=NULL;
    AVPixelFormat pix_fmt_out_;
    AVFormatContext *output_format_ctx_ = NULL;
    AVStream *video_stream_ = NULL;
    std::string output_filename_;
    AVCodecContext *encoder_ctx_=NULL;
    struct SwsContext *sws_ctx_;
    AVPacket *pkt_ =NULL;
    AVPacket *opkt_ =NULL;
    int frameIndex_;

};

