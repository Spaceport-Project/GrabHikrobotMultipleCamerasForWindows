#include <stdio.h>
#include <stdint.h>
#include <iostream>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
}

class BayerToH264Converter {
public:
    BayerToH264Converter(int inputWidth, int inputHeight, int outputWidth, int outputHeight)
        : INPUT_WIDTH(inputWidth), INPUT_HEIGHT(inputHeight),
          OUTPUT_WIDTH(outputWidth), OUTPUT_HEIGHT(outputHeight),
          sws_ctx(nullptr) {
        // av_register_all();
    }

    ~BayerToH264Converter() {
         av_write_trailer(formatContext);
        if (sws_ctx) {
            sws_freeContext(sws_ctx);
        }
        av_frame_free(&input_frame);
        av_frame_free(&yuvFrame);
        av_packet_unref(pkt);
        avcodec_free_context(&codecContext);
        avformat_free_context(formatContext);
    }

    bool initializeContexts(const char* outputFileName) {
        const AVCodec *codec = avcodec_find_encoder_by_name("h264_nvenc");
        //const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!codec) {
            fprintf(stderr, "Codec not found\n");
            return false;
        }

        int ret = avformat_alloc_output_context2(&formatContext, nullptr, nullptr, outputFileName);
        if (ret < 0) {
            fprintf(stderr, "Error creating output context\n");
            return false;
        }

        videoStream = avformat_new_stream(formatContext, codec);
        if (!videoStream) {
            fprintf(stderr, "Error creating video stream\n");
            return false;
        }

        codecContext = avcodec_alloc_context3(codec);
        if (!codecContext) {
            fprintf(stderr, "Could not allocate codec context\n");
            return false;
        }

        // codecContext->codec_id = AV_CODEC_ID_H264;
        codecContext->codec_type = AVMEDIA_TYPE_VIDEO;
        codecContext->pix_fmt = AV_PIX_FMT_YUV420P;
        codecContext->width = OUTPUT_WIDTH;
        codecContext->height = OUTPUT_HEIGHT;
        codecContext->time_base = {1,30} ;
        codecContext->framerate = {30,1}; 
        // codecContext->gop_size = 10;
        // codecContext->max_b_frames = 1;

       // codecContext->bit_rate = 400000; // Adjust the bitrate as needed

        if (avcodec_open2(codecContext, codec, nullptr) < 0) {
            fprintf(stderr, "Could not open codec\n");
            return false;
        }

        // videoStream->codecpar->codec_id = AV_CODEC_ID_H264;
        // videoStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        avcodec_parameters_from_context(videoStream->codecpar, codecContext);

        if (!(formatContext->oformat->flags & AVFMT_NOFILE)) {
            if (avio_open(&formatContext->pb, outputFileName, AVIO_FLAG_WRITE) < 0) {
                fprintf(stderr, "Could not open output file '%s'\n", outputFileName);
                return false;
            }
        }

        if (avformat_write_header(formatContext, nullptr) < 0) {
            fprintf(stderr, "Error writing header\n");
            return false;
        }

        // Initialize SwsContext for Bayer to YUV conversion
        sws_ctx = sws_getContext(
            INPUT_WIDTH, INPUT_HEIGHT, AV_PIX_FMT_BAYER_RGGB8,
            OUTPUT_WIDTH, OUTPUT_HEIGHT, AV_PIX_FMT_YUV420P,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );

        if (!sws_ctx) {
            fprintf(stderr, "Failed to initialize SwsContext\n");
            return false;
        }


        // input_frame = av_frame_alloc();
        // if (!input_frame) {
        //     fprintf(stderr, "Failed to allocate input frame\n");
        //     return false;
        // }

        // input_frame->width = INPUT_WIDTH;
        // input_frame->height = INPUT_HEIGHT;
        // input_frame->format = AV_PIX_FMT_BAYER_RGGB8;


        // yuvFrame = av_frame_alloc();
        // if (!yuvFrame) {
        //     fprintf(stderr, "Failed to allocate output frame\n");
        //     av_frame_free(&input_frame);
        //     return false;
        // }

        // yuvFrame->width = OUTPUT_WIDTH;
        // yuvFrame->height = OUTPUT_HEIGHT;
        // yuvFrame->format = AV_PIX_FMT_YUV420P;

        //  ret = av_frame_get_buffer(input_frame, 32);
        // if (ret < 0) {
        //     fprintf(stderr, "Failed to allocate buffer for input frame\n");
            
        //     return false;
        // }

        // ret = av_frame_get_buffer(yuvFrame, 32);
        // if (ret < 0) {
        //     fprintf(stderr, "Failed to allocate buffer for output frame\n");
            
        //     return false;
        // }


        pkt = av_packet_alloc();
        if (!pkt) {
            fprintf(stderr,"Failed to allocate packet\n");
           return false;
        } 
        // pkt->data = nullptr;
        // pkt->size = 0;
        // pkt->data =  (uint8_t*)av_malloc(OUTPUT_HEIGHT*OUTPUT_WIDTH) ;
        // pkt->size = OUTPUT_HEIGHT*OUTPUT_WIDTH;
        return true;
    }

    bool convertAndEncodeBayerToH264(uint8_t *bayerData) {
         uint8_t *data =  (uint8_t*)av_malloc(INPUT_WIDTH * INPUT_HEIGHT) ;
         memcpy(data, bayerData,  INPUT_WIDTH * INPUT_HEIGHT);


        input_frame = av_frame_alloc();
        if (!input_frame) {
            fprintf(stderr, "Failed to allocate input frame\n");
            return false;
        }
        
   

        yuvFrame = av_frame_alloc();
        if (!yuvFrame) {
            fprintf(stderr, "Failed to allocate output frame\n");
            av_frame_free(&input_frame);
            return false;
        }
      

       

        input_frame->width = INPUT_WIDTH;
        input_frame->height = INPUT_HEIGHT;
        input_frame->format = AV_PIX_FMT_BAYER_RGGB8;

      

        yuvFrame->width = OUTPUT_WIDTH;
        yuvFrame->height = OUTPUT_HEIGHT;
        yuvFrame->format = AV_PIX_FMT_YUV420P;

        int ret = av_frame_get_buffer(input_frame, 32);
        if (ret < 0) {
            fprintf(stderr, "Failed to allocate buffer for input frame\n");
            av_frame_free(&input_frame);
            av_frame_free(&yuvFrame);
            return false;
        }

        ret = av_frame_get_buffer(yuvFrame, 32);
        if (ret < 0) {
            fprintf(stderr, "Failed to allocate buffer for output frame\n");
            av_frame_free(&input_frame);
            av_frame_free(&yuvFrame);
            return false;
        }

        ret = av_image_fill_arrays(
            input_frame->data, input_frame->linesize,
            data, AV_PIX_FMT_BAYER_RGGB8,
            INPUT_WIDTH, INPUT_HEIGHT, 1
        );

        if (ret < 0) {
            fprintf(stderr, "Failed to set input frame data\n");
            av_frame_free(&input_frame);
            av_frame_free(&yuvFrame);
            return false;
        }

        sws_scale(sws_ctx, input_frame->data, input_frame->linesize, 0,
                  INPUT_HEIGHT, yuvFrame->data, yuvFrame->linesize);

        //input_frame->pts = frameIndex++; // Presentation timestamp
        // yuvFrame->pts = frameIndex++;
        // AVPacket pkt;
        // av_init_packet(&pkt);
        // pkt->data = nullptr;
        // pkt->size = 0;

        ret = avcodec_send_frame(codecContext, yuvFrame);
        if (ret < 0) {
            fprintf(stderr, "Error sending a frame for encoding\n");
            av_frame_free(&input_frame);
            av_frame_free(&yuvFrame);
            return false;
        }

        while (ret >= 0) {
            ret = avcodec_receive_packet(codecContext, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            else if (ret < 0) {
                fprintf(stderr, "Error during encoding\n");
                av_frame_free(&input_frame);
                av_frame_free(&yuvFrame);
                return false;
            }

            pkt->pts = frameIndex++; // Presentation timestamp
            // pkt.dts = frameIndex; // Decoding timestamp

            av_packet_rescale_ts(pkt, codecContext->time_base, videoStream->time_base);
            pkt->stream_index = videoStream->index;

            ret = av_interleaved_write_frame(formatContext, pkt);
            av_packet_unref(pkt);
            if (ret < 0) {
                fprintf(stderr, "Error writing packet to file\n");
                av_frame_free(&input_frame);
                av_frame_free(&yuvFrame);
                return false;
            }
        }

        av_frame_free(&input_frame);
        av_frame_free(&yuvFrame);
        av_free(data);
        return true;
    }

private:
    const int INPUT_WIDTH;
    const int INPUT_HEIGHT;
    const int OUTPUT_WIDTH;
    const int OUTPUT_HEIGHT;

    AVFormatContext *formatContext = nullptr;
    AVCodecContext *codecContext = nullptr;
    AVStream *videoStream = nullptr;
    SwsContext *sws_ctx = nullptr;
    AVFrame *input_frame = nullptr;
    AVFrame *yuvFrame = nullptr;
    AVPacket *pkt =  nullptr;
    int frameIndex = 0;
};