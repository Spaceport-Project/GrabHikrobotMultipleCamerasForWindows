#include <stdio.h>
#include <stdint.h>
#include <iostream>
#include <vector>
#include <cassert>
#include <atomic>
#include <queue>
#include <condition_variable>
#include <functional>


extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
}




class BayerToH264Converter2{
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
 
    BayerToH264Converter2(unsigned int device_num, unsigned int numWriteThreads, unsigned int input_width, unsigned int input_height):
        width_(input_width), 
        height_(input_height),
        num_devices_(device_num),
        num_write_threads(numWriteThreads)
        
    {
            // pkts_.resize(num_devices_, nullptr);
            vecPkts_.resize(numWriteThreads);
            for (int j = 0; j < vecPkts_.size(); j++){
                vecPkts_[j].resize(num_devices_, nullptr);

            }

            frameIndexes_.resize(num_devices_, 0);
            vecResults_.resize(num_write_threads);
            for (auto &vec: vecResults_)
                vec.resize(num_devices_, false);
            results_.resize(num_devices_, false);
            // vec_sws_ctx_.resize(1);
            counters_.resize(num_write_threads, 0);
            codecMutexes_ = std::vector<std::mutex>(num_devices_);
            frameNums_.resize(num_write_threads, 0);


    }
    void reset(int nCurrWriteThread ) {
        for (unsigned int j = 0 ; j < num_devices_; j++) {
            av_packet_unref(vecPkts_[nCurrWriteThread][j]);
            // av_packet_free(&vecPkts_[nCurrWriteThread][j]);
            vecPkts_[nCurrWriteThread][j] = nullptr;
            vecResults_[nCurrWriteThread][j] =false;
        }

        // if (pkts_[i]) {
        //     av_packet_unref(pkts_[i]);
        //     // av_packet_free(&pkts_[i]);
        // }
        // pkts_[i] = nullptr;

        // results_[i] = false;


    }
    bool close() {
         av_write_trailer(format_context);
       
       
        // for (unsigned int j = 0 ; j < 1; j++)
        // {
            // if (j == 0){
                if (sws_ctx_) 
                {
                    sws_freeContext(sws_ctx_);
                }
            // }
            for (unsigned int i = 0 ; i < num_devices_; i++)
            {
                if (codec_contexts_[i] != nullptr) 
                {
                    avcodec_close(codec_contexts_[i]);
                }

                avcodec_free_context(&codec_contexts_[i]);
            }
        // }
       
        avformat_free_context(format_context);
        return true;

    }

    ~BayerToH264Converter2() 
    {
       close();
    }

    bool initializeContexts(const char* outputFileName) 
    {

        int ret = avformat_alloc_output_context2(&format_context, nullptr, nullptr, outputFileName);
        if (ret < 0) 
        {
            fprintf(stderr, "Error creating output context\n");
            return false;
        }
        format_context->flags |= AVFMT_FLAG_GENPTS;
        
        const AVCodec *codec = avcodec_find_encoder_by_name("h264_nvenc");
        //const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!codec) 
        {
            fprintf(stderr, "Codec not found\n");
            return false;
        }
       
        // AVDictionary *param;
        // av_dict_set(&param, "qp", "0", 0);
        // /*
        // Change options to trade off compression efficiency against encoding speed. If you specify a preset, the changes it makes will be applied before all other parameters are applied.
        // You should generally set this option to the slowest you can bear.
        // Values available: ultrafast, superfast, veryfast, faster, fast, medium, slow, slower, veryslow, placebo.
        // */
        // av_dict_set(&param, "preset", "ultrafast", 0);
        // for (unsigned int j = 0 ; j < 1; j++)
        // {
            // vec_codec_contexts_.push_back(std::vector<AVCodecContext*>());
            // vec_video_streams_.push_back(std::vector<AVStream*>());
        for (unsigned int i = 0 ; i < num_devices_ ; i++)
        {
            
            video_streams_.push_back(avformat_new_stream(format_context, codec));
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
            codec_contexts_.back()->codec_type = AVMEDIA_TYPE_VIDEO;
            codec_contexts_.back()->pix_fmt = AV_PIX_FMT_YUV420P;
            codec_contexts_.back()->width = width_;
            codec_contexts_.back()->height = height_;
            codec_contexts_.back()->time_base = {1,30} ;
            codec_contexts_.back()->framerate = {30,1}; 
            codec_contexts_.back()->gop_size = 10;
            codec_contexts_.back()->max_b_frames = 1;
            codec_contexts_.back()->thread_count = num_write_threads;
            codec_contexts_.back()->thread_type = FF_THREAD_FRAME;
    

            if (avcodec_open2(codec_contexts_.back(), codec, nullptr) < 0) 
            {
                fprintf(stderr, "Could not open codec\n");
                return false;
            }
            avcodec_parameters_from_context(video_streams_.back()->codecpar, codec_contexts_.back());


        }
      
       
       

        if (!(format_context->oformat->flags & AVFMT_NOFILE)) 
        {
            if (avio_open(&format_context->pb, outputFileName, AVIO_FLAG_WRITE) < 0) 
            {
                fprintf(stderr, "Could not open output file '%s'\n", outputFileName);
                return false;
            }
        }

        if (avformat_write_header(format_context, nullptr) < 0) 
        {
            fprintf(stderr, "Error writing header\n");
            return false;
        }

        // Initialize SwsContext for Bayer to YUV conversion
    //    for (unsigned int j = 0 ; j < 1 ;j++) {

        sws_ctx_ = sws_getContext(
                width_, height_, AV_PIX_FMT_BAYER_RGGB8,
            width_, height_, AV_PIX_FMT_YUV420P,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );

        if (!sws_ctx_) 
        {
            fprintf(stderr, "Failed to initialize SwsContext\n");
            return false;
        }
        // }

        // PacketComparator = [] (const std::vector<AVPacket*> p1, const std::vector<AVPacket*> p2)  {
        // // If both packets have valid pts values, compare them
        //     std::cout<<"Inside pts comparator -1"<<std::endl;
        //     if (p1[0]->pts != AV_NOPTS_VALUE && p2[0]->pts != AV_NOPTS_VALUE) {
        //         std::cout<<"Inside pts comparator 0"<<std::endl;
        //         return p1[0]->pts > p2[0]->pts;
        //     }
        //     // If only one packet has a valid pts, prioritize it
        //     else if (p1[0]->pts != AV_NOPTS_VALUE) {
        //         std::cout<<"Inside pts comparator 1"<<std::endl;

        //         return false;
        //     }
        //     else if (p2[0]->pts != AV_NOPTS_VALUE) {
        //         std::cout<<"Inside pts comparator 2"<<std::endl;

        //         return true;
        //     }
        //     // If both packets have no pts, compare their position in the stream
        //     else {
        //         std::cout<<"Inside pts comparator 3"<<std::endl;

        //         return p1[0]->pos > p2[0]->pos;
        //     }
        // };
        // queuevecPkts_ = std::priority_queue<std::vector<AVPacket *>, std::vector<std::vector<AVPacket*>>, PacketComparator>(PacketComparator());
        return true;

    }

    bool convertAndEncodeBayerToH264(uint8_t *bayerData, unsigned int n_curr_camera, unsigned int nCurrWriteThread,  unsigned int frameNum) 
    {
        assert( n_curr_camera < num_devices_  && "# of Current Camera must not be less than device number");

        //  uint8_t *data =  (uint8_t*)av_malloc(width_ * height_) ;
        //  memcpy(data, bayerData,  width_ * height_);

        frameNums_[nCurrWriteThread] = frameNum;
        bool ret_flag = false; 
        

        AVFrame *input_frame = av_frame_alloc();
        if (!input_frame) 
        {
            fprintf(stderr, "Failed to allocate input frame\n");
            //  //  av_free(data);
            vecResults_[nCurrWriteThread][n_curr_camera] = ret_flag;
            results_[n_curr_camera] = ret_flag;
            return ret_flag;
        }
        
   

        AVFrame *yuv_frame = av_frame_alloc();
        if (!yuv_frame) 
        {
            fprintf(stderr, "Failed to allocate output frame\n");
            av_frame_free(&input_frame);
            //  //  av_free(data);
            vecResults_[nCurrWriteThread][n_curr_camera] = ret_flag;

            results_[n_curr_camera] = ret_flag;
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
            //  av_free(data);
            vecResults_[nCurrWriteThread][n_curr_camera] = ret_flag;
            results_[n_curr_camera] = ret_flag;
            return ret_flag;
        }

        ret = av_frame_get_buffer(yuv_frame, 32);
        if (ret < 0) 
        {
            fprintf(stderr, "Failed to allocate buffer for output frame\n");
            av_frame_free(&input_frame);
            av_frame_free(&yuv_frame);
            //  av_free(data);
            vecResults_[nCurrWriteThread][n_curr_camera] = ret_flag;

            results_[n_curr_camera] = ret_flag;
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
            vecResults_[nCurrWriteThread][n_curr_camera] = ret_flag;

            results_[n_curr_camera] = ret_flag;
            return ret_flag;
        }
        {

        sws_scale(sws_ctx_, input_frame->data, input_frame->linesize, 0,
                  height_, yuv_frame->data, yuv_frame->linesize);

       

        ret = avcodec_send_frame(codec_contexts_[n_curr_camera], yuv_frame);
        if (ret < 0) 
        {
            fprintf(stderr, "Error sending a frame for encoding\n");
            av_frame_free(&input_frame);
            av_frame_free(&yuv_frame);
            vecResults_[nCurrWriteThread][n_curr_camera] = ret_flag;

            results_[n_curr_camera] = ret_flag;
            return ret_flag;
        }
        vecPkts_[nCurrWriteThread][n_curr_camera] = av_packet_alloc();
            
        ret = avcodec_receive_packet(codec_contexts_[n_curr_camera], vecPkts_[nCurrWriteThread][n_curr_camera]);
        // printf("%d.camera's counter:  %d \n", n_curr_camera, counter_);
        // printf("%d.camera's result: %d\n",n_curr_camera, ret);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
         

        }
        else if (ret < 0) {
            fprintf(stderr, "Error during encoding\n");
        } else {
    
           
  
            vecPkts_[nCurrWriteThread][n_curr_camera]->pts =  frameNum; // Presentation timestamp
            av_packet_rescale_ts(vecPkts_[nCurrWriteThread][n_curr_camera], codec_contexts_[n_curr_camera]->time_base, video_streams_[n_curr_camera]->time_base);
            vecPkts_[nCurrWriteThread][n_curr_camera]->dts = vecPkts_[nCurrWriteThread][n_curr_camera]->pts;
            vecPkts_[nCurrWriteThread][n_curr_camera]->stream_index = video_streams_[n_curr_camera]->index;
            


            ret_flag = true;
            vecResults_[nCurrWriteThread][n_curr_camera] = ret_flag;

            results_[n_curr_camera] = ret_flag;

        }
            
        } 

        av_frame_free(&input_frame);
        av_frame_free(&yuv_frame);
        //  av_free(data);
        return ret_flag;
    }

    void push2Queue(int nCurrWriteThread){
       
       
        {
            std::unique_lock<std::mutex> lk(writeMutex_);

            queuevecPkts_.push(vecPkts_[nCurrWriteThread]);
             
        }
        std::fill(vecResults_[nCurrWriteThread].begin(),vecResults_[nCurrWriteThread].end(), false);
        // if (queuevecPkts_.size() == 1) 
        counter_++;
        if (counter_ % 50 == 0 || m_bExit) {
            counter_flag=true;
            writeCond_.notify_one();
        }

    }
   
    
    std::vector<std::vector<bool>> & getResults()
    {
        return vecResults_;
    }
    std::vector<int> & getFrameNums() {
        return frameNums_;
    }

   
    bool writeAllFrames2MP42(int nCurrWriteThread)
    {
       
        for (unsigned int i = 0 ; i < num_devices_ ; i++) 
        {
            std::cout<<"Thread id:"<<nCurrWriteThread<<" pts:"<<" "<<  vecPkts_[nCurrWriteThread][i]->pts<<std::endl;
            int ret = av_interleaved_write_frame(format_context, vecPkts_[nCurrWriteThread][i]);
            av_packet_unref(vecPkts_[nCurrWriteThread][i]);
            if (ret < 0) 
            {
                fprintf(stderr, "Error writing packet to file\n");
                return false;
            }

            vecResults_[nCurrWriteThread][i] = false;

        }
        
        return true;

    }

    bool writeAllFrames2MP43()
    {
   
        while (!m_bExit ) 
        {
            
            {
                std::vector<AVPacket*> packets={nullptr};

                std::unique_lock<std::mutex> lk(writeMutex_);
                writeCond_.wait(lk, [this] { return  counter_flag  ;});
                counter_flag=false;

            
                while (!queuevecPkts_.empty()) {

                    std::vector<AVPacket*> packets = queuevecPkts_.top();
                    queuevecPkts_.pop();

                    for (unsigned int i = 0 ; i < packets.size() ; i++) 
                    {
                        int ret = av_interleaved_write_frame(format_context, packets[i]);
                        av_packet_unref(packets[i]);
                        if (ret < 0) 
                        {
                            fprintf(stderr, "Error writing packet to file\n");
                        //  return false;
                        }

                // vecResults_[nCurrWriteThread][i] = false;

                    }
                    // std::cout<<"Packet Buffer Size:"<<queuevecPkts_.size()<<std::endl;

                
                }  
                

               


                
            }
          

        
        }




           
        return true;

    }




 

    static bool m_bExit;

private:
    const unsigned int width_;
    const unsigned int height_;

    AVFormatContext *format_context = nullptr; 
    std::vector<AVCodecContext *> codec_contexts_;
    
    // std::vector<std::vector<AVCodecContext *>>  vec_codec_contexts_;
    std::mutex writeMutex_;
    std::condition_variable writeCond_;
    std::vector<AVStream *> video_streams_;
    SwsContext * sws_ctx_ = nullptr;
    // std::vector<AVPacket*> pkts_;
    std::vector<std::vector<AVPacket*>> vecPkts_;
    // std::function<bool(const std::vector<AVPacket*>, const std::vector<AVPacket*>)> PacketComparator;

    std::priority_queue<std::vector<AVPacket*>, std::vector<std::vector<AVPacket*>>, PacketComparator> queuevecPkts_;
    // std::priority_queue<std::vector<AVPacket*>, std::vector<std::vector<AVPacket*>>,  std::greater<std::vector<std::vector<AVPacket*>>::value_type >> queuevecPkts_;
    unsigned int num_devices_;
    unsigned int num_write_threads;
    unsigned int frameIndex = 0;
    // std::atomic<int> counter_{0};
    std::vector<int>  counters_ ;
    std::vector<int> frameNums_;
    int counter_ = 0;
    bool counter_flag=false;
    std::vector<unsigned int> frameIndexes_;
    std::vector<bool> results_;
    std::vector<std::vector<bool>> vecResults_;
    std::vector<std::mutex> codecMutexes_;


};

