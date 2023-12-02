
#include "ConverterFromBayer2Mp4.h"

ConverterFromBayer2Mp4::ConverterFromBayer2Mp4(const std::string& output_filename, int width, int height, AVPixelFormat pix_fmt_in): 
output_filename_(output_filename),
frameIndex_(0),
width_(width),
height_(height),
pix_fmt_in_(pix_fmt_in),
pix_fmt_out_(AV_PIX_FMT_YUV420P)
{
    
    const AVCodec *codec_ = avcodec_find_decoder(AV_CODEC_ID_RAWVIDEO);
    if (!codec_) {
        // fprintf(stderr, "Codec not found.\n");
        throw std::runtime_error("Codec not found.");
    }

    codec_ctx_ = avcodec_alloc_context3(codec_);
    if (!codec_ctx_) {
        // fprintf(stderr, "Could not allocate codec context");
        throw std::runtime_error("Could not allocate codec context.");
    }

    codec_ctx_->width = width_;
    codec_ctx_->height = height_;
    codec_ctx_->pix_fmt = pix_fmt_in_;

  
    if (avcodec_open2(codec_ctx_, codec_, NULL) < 0) {
        // fprintf(stderr, "Could not open codec.\n");
        throw std::runtime_error("Could not open codec.");
    }

    // bayer_frame_ = av_frame_alloc();
    // if (!bayer_frame_) {
    //     throw std::runtime_error("Could not allocate video frame.");
        
    // }

    // int num_bytes_out = av_image_get_buffer_size(pix_fmt_out_, width, height, 1);
    // buffer_out_ = (uint8_t*)av_malloc(num_bytes_out);
    // if (!buffer_out_) {
    //         throw std::runtime_error("Could not allocate memory for output frame data.");
    // }

    // yuv_frame_ = av_frame_alloc();
    // if (!yuv_frame_) {
    //     throw std::runtime_error("Could not allocate output frame.");
    
    // }


    // av_image_fill_arrays(yuv_frame_->data, yuv_frame_->linesize, buffer_out_, pix_fmt_out_, width_, height_, 1);

    sws_ctx_ = sws_getContext(width_, height_, pix_fmt_in_,
                                                width_, height_, pix_fmt_out_,
                                                SWS_BILINEAR, NULL, NULL, NULL);
    if (!sws_ctx_) {
        throw std::runtime_error("Could not initialize the conversion context.");
    
    }

        // Create output format context
    avformat_alloc_output_context2(&output_format_ctx_, NULL, NULL, output_filename.c_str());
    if (!output_format_ctx_) {
        throw std::runtime_error("Could not create output context");
        
    }

        
    video_stream_ = avformat_new_stream(output_format_ctx_, NULL);
    if (!video_stream_) {
        throw std::runtime_error("Could not create stream");
        
    }

    
    const AVCodec *encoder_ = avcodec_find_encoder_by_name("h264_nvenc");
    if (!encoder_) {
        throw std::runtime_error("h264_nvenc Codec not found");
    }

    encoder_ctx_ = avcodec_alloc_context3(encoder_);
    if (!encoder_ctx_) {
        throw std::runtime_error("Failed to allocate output codec context");
        
    }

    encoder_ctx_->codec_id = encoder_->id;
    encoder_ctx_->codec_type = AVMEDIA_TYPE_VIDEO;
    // encoder_ctx->bit_rate = 1953000; // Adjust as needed
    encoder_ctx_->width = width_;
    encoder_ctx_->height = height_;
    encoder_ctx_->time_base = (AVRational){1, 25};
    encoder_ctx_->framerate = (AVRational){25, 1};
    // encoder_ctx_->sample_aspect_ratio = (AVRational){16, 9};
    encoder_ctx_->gop_size = 10;
    encoder_ctx_->pix_fmt = pix_fmt_out_;
    encoder_ctx_->max_b_frames = 1;
    // if (encoder_ctx_->codec_id == AV_CODEC_ID_H264) {
    //      av_opt_set (encoder_ctx_, "preset " , "ultrafast" , 0 );
    // }
    if (avcodec_open2(encoder_ctx_, encoder_, NULL) < 0) {
        throw std::runtime_error("Could not open output codec");

    }
    avcodec_parameters_from_context(video_stream_->codecpar, encoder_ctx_);


    if (!(output_format_ctx_->oformat->flags & AVFMT_NOFILE)) {
        int ret = avio_open(&output_format_ctx_->pb, output_filename.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            throw std::runtime_error("Could not open output file");
        }
        }
    int ret = avformat_write_header(output_format_ctx_, NULL);
    if (ret < 0) {
        char err[4096];
        av_strerror(ret, err, sizeof(err));
        fprintf(stderr, "Error occurred when opening output file. Error: %s\n", err );
        throw std::runtime_error("Error  occurred when opening output file");
    }
    

    
    // pkt_ = av_packet_alloc();
    // if (!pkt_) {
    //     throw std::runtime_error("Failed to allocate packet");
        
    // }
    opkt_ = av_packet_alloc();
    if (!opkt_) {
        throw std::runtime_error("Failed to allocate packet");
        
    }

    // bayer_frame_->width = width_;
    // bayer_frame_->height = height_;
    // bayer_frame_->format = pix_fmt_in_;

    // yuv_frame_->width = width_;
    // yuv_frame_->height = height_;
    // yuv_frame_->format = pix_fmt_out_;

    // // Allocate buffer for input frame
    // ret = av_frame_get_buffer(bayer_frame_, 32);
    // if (ret < 0) {
    //     fprintf(stderr, "Failed to allocate buffer for input frame\n");
    //     return;
    // }
    // ret = av_frame_get_buffer(yuv_frame_, 32);
    // if (ret < 0) {
    //     fprintf(stderr, "Failed to allocate buffer for YUV frame\n");
       
    //     return ;
    // }


    


    // int frame_len  = width_ * height_;
    // pkt_->data =  (uint8_t*)av_malloc(frame_len) ;

}

bool ConverterFromBayer2Mp4::write2Mp4(const std::shared_ptr<uint8_t[]>  &data){
    uint8_t *dat = (uint8_t*)av_malloc(width_ * height_) ;
    memcpy(dat, data.get(), width_ * height_);
    //  pkt_->data =  (uint8_t*)av_malloc(width_ * height_) ;
    // memcpy(pkt_->data, data.get(), width_ * height_);
    // // pkt_->data = data.get();
    //  pkt_->size = width_ * height_;
    
    // FILE *input_file = fopen("/home/hamit/Softwares/compressionTool/build/Video_20231127232708651.raw", "rb");
    // if (!input_file) {
    //     fprintf(stderr, "Could not open input file.\n");
    //     return -1;
    // }
    
  //   uint8_t *dat = (uint8_t*)av_malloc(width_ * height_) ;
 
   
// while (fread(pkt_->data, 1, width_ * height_, input_file) > 0) {
//          pkt_->size = codec_ctx_->width * codec_ctx_->height;
//      // Set input frame data

    bayer_frame_ = av_frame_alloc();
        if (!bayer_frame_) {
            fprintf(stderr, "Failed to allocate input frame\n");
            return false;
        }
    
    bayer_frame_->width = width_;
    bayer_frame_->height = height_;
    bayer_frame_->format = AV_PIX_FMT_BAYER_RGGB8;

    yuv_frame_ = av_frame_alloc();
    if (!yuv_frame_) {
        fprintf(stderr, "Failed to allocate output frame\n");
        av_frame_free(&bayer_frame_);
        return false;
    }
    yuv_frame_->format = AV_PIX_FMT_YUV420P;
    yuv_frame_->width = width_;
    yuv_frame_->height = height_;

 // Allocate buffer for input frame
    int ret = av_frame_get_buffer(bayer_frame_, 32);
    if (ret < 0) {
        fprintf(stderr, "Failed to allocate buffer for input frame\n");
        return false;
    }
    ret = av_frame_get_buffer(yuv_frame_, 32);
    if (ret < 0) {
        fprintf(stderr, "Failed to allocate buffer for YUV frame\n");
       
        return false;
    }


    ret = av_image_fill_arrays(
        bayer_frame_->data, bayer_frame_->linesize,
        dat, AV_PIX_FMT_BAYER_RGGB8,
        width_, height_, 1
    );

    if (ret < 0) {
        fprintf(stderr, "Failed to set input frame data\n");
        return false;
    }
    
    sws_scale(sws_ctx_, (const uint8_t *const *)bayer_frame_->data, bayer_frame_->linesize, 0,
                height_, yuv_frame_->data, yuv_frame_->linesize);

   
    yuv_frame_->pts =  frameIndex_++;
    ret = avcodec_send_frame(encoder_ctx_, yuv_frame_);
    if (ret < 0) {
        fprintf(stderr, "Error sending a frame for encoding\n");
        return false;
    }

     while (ret >= 0) {
        ret = avcodec_receive_packet(encoder_ctx_, opkt_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        else if (ret < 0) {
            fprintf(stderr, "Error during encoding\n");
            return false;
        }
        // AVRational destTimeBase = {1, 200000};
        // av_packet_rescale_ts(opkt, encoder_ctx_->time_base, destTimeBase);
        av_packet_rescale_ts(opkt_, encoder_ctx_->time_base, video_stream_->time_base);
        
        // int64_t now = av_gettime();
        // opkt->pts = av_rescale_q(now, AV_TIME_BASE_Q, video_stream->time_base);
        // opkt->dts = opkt->pts;
        opkt_->stream_index = video_stream_->index;
        // printf("pts:%ld\n", opkt->pts);
        av_interleaved_write_frame(output_format_ctx_, opkt_);
        av_packet_unref(opkt_);
    }
    av_packet_unref(opkt_);
    av_free(dat);
    av_frame_free(&bayer_frame_);
    av_frame_free(&yuv_frame_);
    return true;
}   
    
    // int ret = avcodec_send_packet(codec_ctx_, pkt_);

    // if (ret < 0) {
    //     fprintf(stderr, "Error sending a packet for decoding\n");
    //     return -1;
    // }

   

    // // if (av_frame_get_buffer(bayer_frame_, 32) < 0) {
    // //     fprintf(stderr, "Could not allocate video data buffer\n");
    // //     exit(EXIT_FAILURE);
    // //     }
    // while (ret >= 0) {
    //     ret = avcodec_receive_frame(codec_ctx_, bayer_frame_);
    //     if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
    //         break;
    //     else if (ret < 0) {
    //         fprintf(stderr, "Error during decoding\n");
    //         return false;
    //     }
    //         // Convert Bayer_RGGB8 to YUV420P
        

        // sws_scale(sws_ctx_, (const uint8_t *const *)bayer_frame_->data, bayer_frame_->linesize,
        //             0, height_, yuv_frame_->data, yuv_frame_->linesize);

        // yuv_frame_->format = pix_fmt_out_;
        // yuv_frame_->width = encoder_ctx_->width;
        // yuv_frame_->height = encoder_ctx_->height;
        // yuv_frame_->pts = frameIndex_++;
        
        // ret = avcodec_send_frame(encoder_ctx_, yuv_frame_);
        // if (ret < 0) {
        //     fprintf(stderr, "Error sending a frame for encoding\n");
        //     return false;
        // }
        
    
        // while (ret >= 0) {
        //     ret = avcodec_receive_packet(encoder_ctx_, opkt_);
        //     if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        //         break;
        //     else if (ret < 0) {
        //         fprintf(stderr, "Error during encoding\n");
        //         return false;
        //     }
        //     // AVRational destTimeBase = {1, 200000};
        //     // av_packet_rescale_ts(opkt, encoder_ctx_->time_base, destTimeBase);
        //     av_packet_rescale_ts(opkt_, encoder_ctx_->time_base, video_stream_->time_base);
            
        //     // int64_t now = av_gettime();
        //     // opkt->pts = av_rescale_q(now, AV_TIME_BASE_Q, video_stream->time_base);
        //     // opkt->dts = opkt->pts;
        //     opkt_->stream_index = video_stream_->index;
        //     // printf("pts:%ld\n", opkt->pts);
        //     av_interleaved_write_frame(output_format_ctx_, opkt_);
        //     av_packet_unref(opkt_);
        // }
        // av_packet_unref(opkt_);
  //  }


   // return true;
    




//}
ConverterFromBayer2Mp4::~ConverterFromBayer2Mp4()
{
   // av_packet_unref(pkt_);

    // // Flush encoder
    // int ret =  avcodec_send_frame(encoder_ctx_, NULL);
    // if (ret >= 0 ) {
    //     while (ret >= 0) {
    //         ret = avcodec_receive_packet(encoder_ctx_, opkt_);
    //         if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
    //             break;
    //         else if (ret < 0) {
    //             fprintf(stderr, "Error during encoding\n");
                
    //         }

            
    //         av_packet_rescale_ts(opkt_, encoder_ctx_->time_base, video_stream_->time_base);
        
    //         opkt_->stream_index = video_stream_->index;
    //         av_interleaved_write_frame(output_format_ctx_, opkt_);
    //         av_packet_unref(opkt_);
    //     }
    // }

    // Write file trailer
    av_write_trailer(output_format_ctx_);

    // Free resources
    avcodec_free_context(&codec_ctx_);
    avcodec_free_context(&encoder_ctx_);
    avformat_close_input(&output_format_ctx_);
    
    avformat_free_context(output_format_ctx_);
    av_frame_free(&yuv_frame_);
    av_frame_free(&frame_out_);
    av_packet_free(&pkt_);
    av_packet_free(&opkt_);
    av_free(buffer_out_);
    sws_freeContext(sws_ctx_);


}


