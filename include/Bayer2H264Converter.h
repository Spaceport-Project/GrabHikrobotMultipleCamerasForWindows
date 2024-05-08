#include <stdio.h>
#include <stdint.h>
#include <iostream>
#include <vector>
#include <cassert>
#include <atomic>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
}

class BayerToH264Converter{
public:
        struct PacketComparator {

        bool operator() (const std::vector<AVPacket*> &p1, const std::vector<AVPacket*> &p2)  {
            // If both packets have valid pts values, compare them

                if (p1[0]->pts != AV_NOPTS_VALUE && p2[0]->pts != AV_NOPTS_VALUE) {
                    // std::cout<<"pts: "<<p1[0]->pts<< " "<< p2[0]->pts<< std::endl;
                    return p1[0]->pts > p2[0]->pts;
                }
                // If only one packet has a valid pts, prioritize it
                else if (p1[0]->pts != AV_NOPTS_VALUE) {

                    return false;
                }
                else if (p2[0]->pts != AV_NOPTS_VALUE) {

                    return true;
                }
                // If both packets have no pts, compare their position in the stream
                else {

                    return p1[0]->pos > p2[0]->pos;
                }
            }
    };
    BayerToH264Converter(unsigned int device_num, unsigned int input_width, unsigned int input_height):
        width_(input_width), 
        height_(input_height),
        num_devices_(device_num)
        
    {
            
            pkts_.resize(num_devices_, nullptr);
            for (int j = 0; j < num_devices_; j++){
                format_contexts.push_back(nullptr);
            }

            results_.resize(num_devices_, false);
            frame_cnts.resize(num_devices_, 0);
            codecMutexes_ = std::vector<std::mutex>(num_devices_);


    }
  
    bool close() {
        
        for (unsigned int i = 0 ; i < num_devices_; i++)
            av_write_trailer(format_contexts[i]);

        for (unsigned int j = 0 ; j < num_devices_; j++)
        {
                if (sws_ctx_[j]) 
                {
                    sws_freeContext(sws_ctx_[j]);
                }
        }
        for (unsigned int i = 0 ; i < num_devices_; i++)
        {

            
            if (codec_contexts_[i] != nullptr) 
            {
                avcodec_close(codec_contexts_[i]);
            }

            avcodec_free_context(&codec_contexts_[i]);

            avformat_free_context(format_contexts[i]);
        }
        // }
       
        // avformat_free_context(format_context);
        return true;

    }

    ~BayerToH264Converter() 
    {
       close();
    }

    bool initializeContexts(const char* suffix_file_name, std::map<int, std::string> mapSerials) 
    {
        
        // const AVOutputFormat *poutputFormat = av_guess_format(NULL, ".mkv", "video/x-matroska");
        for (int j = 0; j < num_devices_;j++){
            std::string file_name = std::string(suffix_file_name) + "_" + mapSerials[j]  + ".mp4" ;

            int ret = avformat_alloc_output_context2(&format_contexts[j], nullptr, nullptr, file_name.c_str());
            if (ret < 0) 
            {
                fprintf(stderr, "Error creating output context\n");
                return false;
            }
            format_contexts[j]->flags |= AVFMT_FLAG_GENPTS;
            format_contexts[j]->flags |= AVFMT_FLAG_FLUSH_PACKETS;
            // format_contexts[j]->flags |= AVFMT_ALLOW_FLUSH ;
            // format_contexts[j]->oformat = poutputFormat;
            // format_contexts[j]->flags  |= AVFMT_FLAG_NOBUFFER;

             

        }

      
        
        const AVCodec *codec = avcodec_find_encoder_by_name("h264_nvenc");
        //const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!codec) 
        {
            fprintf(stderr, "Codec not found\n");
            return false;
        }
       
        
        // av_dict_set(&param, "preset", "ultrafast", 0);
        // for (unsigned int j = 0 ; j < 1; j++)
        
        for (unsigned int i = 0 ; i < num_devices_ ; i++)
        {
            
            video_streams_.push_back(avformat_new_stream(format_contexts[i], codec));
            if (!video_streams_.back()) 
            {
                fprintf(stderr, "Error creating video stream\n");
                return false;
            }
            
            
            
            codec_contexts_.push_back(avcodec_alloc_context3(codec));
            if (!codec_contexts_.back()) 
            {
                fprintf(stderr, "Could not allocate codec context\n");
                return false;
            }
            // codec_contexts_.back()->codec_id = AV_CODEC_ID_H264;
            codec_contexts_.back()->codec_type = AVMEDIA_TYPE_VIDEO;
            codec_contexts_.back()->pix_fmt = AV_PIX_FMT_YUV420P;
            codec_contexts_.back()->width = width_;
            codec_contexts_.back()->height = height_;
            codec_contexts_.back()->time_base = {1,30} ;
            codec_contexts_.back()->framerate = {30,1}; 
            codec_contexts_.back()->gop_size = 10;
            codec_contexts_.back()->max_b_frames = 1;
            codec_contexts_.back()->thread_count = num_devices_;
            codec_contexts_.back()->thread_type = FF_THREAD_FRAME;
            //  codec_contexts_.back()->flags |= AV_CODEC_FLAG2_CHUNKS;
    
            // av_opt_set(codec_contexts_.back()->priv_data, "preset", "fast", 0);

            // codec_contexts_.back()->bit_rate = 400000; // Adjust the bitrate as needed
            if (avcodec_open2(codec_contexts_.back(), codec, nullptr) < 0) 
            {
                fprintf(stderr, "Could not open codec\n");
                return false;
            }
             avcodec_parameters_from_context(video_streams_.back()->codecpar, codec_contexts_.back());

            sws_ctx_.push_back(nullptr);

        }
        
       
        for (int i = 0; i < num_devices_; i++)
        {
            std::string file_name = std::string(suffix_file_name) + "_" + mapSerials[i]  + ".mp4" ;

            if (!(format_contexts[i]->oformat->flags & AVFMT_NOFILE)) 
            {
                if (avio_open(&format_contexts[i]->pb, file_name.c_str(), AVIO_FLAG_WRITE) < 0) 
                {
                    fprintf(stderr, "Could not open output file '%s'\n", file_name.c_str());
                    return false;
                }
            }

            if (avformat_write_header(format_contexts[i], nullptr) < 0) 
            {
                fprintf(stderr, "Error writing header\n");
                return false;
            }
        }

        // Initialize SwsContext for Bayer to YUV conversion
       for (unsigned int j = 0 ; j < num_devices_ ;j++) {

            sws_ctx_[j] = sws_getContext(
                    width_, height_, AV_PIX_FMT_BAYER_RGGB8,
                width_, height_, AV_PIX_FMT_YUV420P,
                SWS_BILINEAR, nullptr, nullptr, nullptr
            );

            if (!sws_ctx_[j]) 
            {
                fprintf(stderr, "Failed to initialize SwsContext\n");
                return false;
            }
        }
        return true;

    }

    bool convertAndEncodeBayerToH264(uint8_t *bayerData, unsigned int n_curr_cam_index,  int64_t time_stamp, int frame_num) 
    {
         assert( n_curr_camera < num_devices_  && "# of Current Camera must not be less than device number");

        //  uint8_t *data =  (uint8_t*)av_malloc(width_ * height_) ;
        //  memcpy(data, bayerData,  width_ * height_);

        bool ret_flag = false; 
        

        AVFrame *input_frame = av_frame_alloc();
        if (!input_frame) 
        {
            fprintf(stderr, "Failed to allocate input frame\n");
            results_[n_curr_cam_index] = ret_flag;
            return ret_flag;
        }
        
   

        AVFrame *yuv_frame = av_frame_alloc();
        if (!yuv_frame) 
        {
            fprintf(stderr, "Failed to allocate output frame\n");
            av_frame_free(&input_frame);

            results_[n_curr_cam_index] = ret_flag;
            return ret_flag;
        }
      

       

        input_frame->width = width_;
        input_frame->height = height_;
        input_frame->format = AV_PIX_FMT_BAYER_RGGB8;

      

        yuv_frame->width = width_;
        yuv_frame->height = height_;
        yuv_frame->format = AV_PIX_FMT_YUV420P;

        int ret = av_frame_get_buffer(input_frame, 32);
        if (ret < 0) 
        {
            fprintf(stderr, "Failed to allocate buffer for input frame\n");
            av_frame_free(&input_frame);
            av_frame_free(&yuv_frame);
            results_[n_curr_cam_index] = ret_flag;
            return ret_flag;
        }

        ret = av_frame_get_buffer(yuv_frame, 32);
        if (ret < 0) 
        {
            fprintf(stderr, "Failed to allocate buffer for output frame\n");
            av_frame_free(&input_frame);
            av_frame_free(&yuv_frame);

            results_[n_curr_cam_index] = ret_flag;
            return ret_flag;
        }

        ret = av_image_fill_arrays(
            input_frame->data, input_frame->linesize,
            bayerData, AV_PIX_FMT_BAYER_RGGB8,
            width_, height_, 1
        );


        if (ret < 0) 
        {
            fprintf(stderr, "Failed to set input frame data\n");
            av_frame_free(&input_frame);
            av_frame_free(&yuv_frame);

            results_[n_curr_cam_index] = ret_flag;
            return ret_flag;
        }
        {

        sws_scale(sws_ctx_[n_curr_cam_index], input_frame->data, input_frame->linesize, 0,
                  height_, yuv_frame->data, yuv_frame->linesize);

        ret = avcodec_send_frame(codec_contexts_[n_curr_cam_index], yuv_frame);
        if (ret < 0) 
        {
            fprintf(stderr, "Error sending a frame for encoding\n");
            av_frame_free(&input_frame);
            av_frame_free(&yuv_frame);

            results_[n_curr_cam_index] = ret_flag;
            return ret_flag;
        }

        pkts_[n_curr_cam_index] = av_packet_alloc();
            
        ret = avcodec_receive_packet(codec_contexts_[n_curr_cam_index], pkts_[n_curr_cam_index]);
        // printf("%d.camera's counter:  %d \n", n_curr_camera, counter_);
        // printf("%d.camera's result: %d\n",n_curr_camera, ret);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
         

        }
        else if (ret < 0) {
            fprintf(stderr, "Error during encoding\n");
        } else {
    
          
            // if (n_curr_cam_index) printf("timestamp:%I64d, %d\n", time_stamp, frame_num);
            // pkts_[n_curr_cam_index]->pts = av_rescale_q(time_stamp, {1, 1000}, video_streams_[n_curr_cam_index]->time_base);
            pkts_[n_curr_cam_index]->pts =  frame_num; // Presentation timestamp

            av_packet_rescale_ts(pkts_[n_curr_cam_index], codec_contexts_[n_curr_cam_index]->time_base, video_streams_[n_curr_cam_index]->time_base);
            pkts_[n_curr_cam_index]->dts = pkts_[n_curr_cam_index]->pts;
            pkts_[n_curr_cam_index]->stream_index = video_streams_[n_curr_cam_index]->index;

            ret_flag = true;

            results_[n_curr_cam_index] = ret_flag;

        }
            
        } 

        av_frame_free(&input_frame);
        av_frame_free(&yuv_frame);
        //  av_free(data);
        return ret_flag;
    }
  
    // void push2Queue(int nCurrCameraIndex){
        
        
    //         {
    //             std::unique_lock<std::mutex> lk(writeMutex_);
    //             vec_queue_pkts_[nCurrCameraIndex].push(pkts_)
    //             queuevecPkts_.push(vecPkts_[nCurrCameraIndex]);
    //             std::cout<<"packet buffer size:"<<queuevecPkts_.size()<<std::endl;
                
    //         }
    //         std::fill(vecResults_[nCurrWriteThread].begin(),vecResults_[nCurrWriteThread].end(), false);
    //         // if (queuevecPkts_.size() == 1) 
    //         counter_++;
    //         if (counter_ % 50 == 0 || m_bExit) {
    //             counter_flag=true;
    //             writeCond_.notify_one();
    //     }

    // }
    std::vector<bool> & getResults()
    {
        return results_;
    }
    // std::vector<int> & getFrameNums() {
    //     return frameNums_;
    // }

    bool writeSingleFrame2MP4(int nCurrCameraIndex) {
        // printf("pkts_[nCurrCamera]->stream_index: %d, %d \n", pkts_[nCurrCamera]->stream_index, video_streams_[nCurrCamera]->index);

        int ret =  av_interleaved_write_frame(format_contexts[nCurrCameraIndex], pkts_[nCurrCameraIndex]);
        // if (frame_cnts[nCurrCameraIndex] % flush_interval == 0) {
        //     // Flush the buffered data to the disk
        //     //  avio_flush(format_contexts[nCurrCameraIndex]->pb);
        //     //  av_write_frame(format_contexts[nCurrCameraIndex], NULL);
        //     //  ret = av_write_flush(format_contexts[nCurrCameraIndex]);
        //     //  av_interleaved_write_frame(format_contexts[nCurrCameraIndex], NULL);
        // }
        // av_interleaved_write_frame(format_contexts[nCurrCameraIndex], NULL);
        av_packet_unref(pkts_[nCurrCameraIndex]);
        if (ret < 0) 
        {
            fprintf(stderr, "Error writing packet to file\n");
            return false;
        }
        frame_cnts[nCurrCameraIndex]++;
        return true;

    }
    // bool writeAllFrames2MP4()
    // {
    //     for (unsigned int i = 0 ; i < num_devices_ ; i++) 
    //     {
    //         int ret = av_interleaved_write_frame(format_context, pkts_[i]);
    //         av_packet_unref(pkts_[i]);
    //         if (ret < 0) 
    //         {
    //             fprintf(stderr, "Error writing packet to file\n");
    //             return false;
    //         }



    //     }
        
    //     return true;

    // }

    // bool writeAllFrames2MP42(int n_curr_cam_index)
    // {
    
    //     int ret = av_interleaved_write_frame(format_context, pkts_[n_curr_cam_index]);
    //     av_packet_unref(pkts_[n_curr_cam_index][i]);
    //     if (ret < 0) 
    //     {
    //         fprintf(stderr, "Error writing packet to file\n");
    //         return false;
    //     }

    //     results_[n_curr_cam_index] = false;
  
        
    //     return true;

    // }

    // bool unRefPackets() {
    //     for (unsigned int i = 0 ; i < num_devices_ ; i++) 
    //     {
    //         av_packet_unref(pkts_[i]);
    //     }
    //     return true;
    // }


   


private:
    const unsigned int width_;
    const unsigned int height_;

    std::vector<AVFormatContext *> format_contexts; 
    std::vector<AVCodecContext *> codec_contexts_;
    // std::vector<std::vector<AVCodecContext *>>  vec_codec_contexts_;
    std::vector<std::priority_queue<AVPacket*, std::vector<AVPacket*>, PacketComparator>>  vec_queue_pkts_;

    std::vector<AVStream *> video_streams_;
    std::vector<SwsContext *> sws_ctx_ ;
    std::vector<AVPacket*> pkts_;
    unsigned int num_devices_;
    unsigned int frameIndex = 0;
    unsigned int flush_interval = 0;
    std::vector<bool> results_;
    std::vector<std::mutex> codecMutexes_;
    std::vector< unsigned int>  frame_cnts;;



};