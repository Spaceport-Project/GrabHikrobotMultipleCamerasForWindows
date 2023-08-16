

#ifndef CONTAINER_H_
#define CONTAINER_H_

#include <iostream>
#include <fstream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include <libswscale/swscale.h>

#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
}




//const int STREAM_INDEX_MESH = 0;
const int STREAM_INDEX_IMG = 0;
// //const int STREAM_INDEX_SPACE_CONTAINER_MESH = 2;
// const int STREAM_INDEX_SPACE_CONTAINER_TEXTURE = 1;




class Container {

public:

	Container() {
		//av_register_all();
		format_context = NULL;





		const int speed = 10 - 0;
	}

	/**
	 * Opens file for reading or writing
	 *
	 * @param filename
	 * The name of the file to read or to write
	 *
	 * @param isWriteMode
	 * true if file is going to be written,
	 * false if file is going to be read
	 *
	 * @return bool
	 * true if it opens file successfully otherwise it returns false
	 */
	bool open(char* filename, bool isWriteMode) {
		if (isWriteMode) {
			return openForWrite(filename);
		}
		else {
			return openForRead(filename);
		}
	}

	/**
	 * Opens file for reading
	 *
	 * @param filename
	 * The name of the file to read
	 *
	 * @return bool
	 * true if it opens file successfully otherwise it returns false
	 */
	bool openForRead(char* filename) {
		this->isWriteMode = false;
		int ret = avformat_open_input(&format_context, filename, 0, 0);
		if (ret < 0) {
			return false;
		}
		return true;

	}

	/**
	 * Opens file for write
	 *
	 * @param filename
	 * The name of the file to write
	 *
	 * @return
	 * true if it opens file successfully otherwise it returns false
	 */
	bool openForWrite(char* filename) {
		this->isWriteMode = true;
		int ret;
		ret = avformat_alloc_output_context2(&format_context, NULL, NULL, filename);
		if (ret < 0) {
			std::cout << "Cannot allocate output context";
			return false;
		}



		{

			//Create stream for texture files - stream index will be 1
			out_stream_texture = avformat_new_stream(format_context, NULL);
			if (!out_stream_texture) {
				std::cout << "Cannot add new stream";
				return false;
			}

			AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_PNG);
			if (!codec) {
				std::cout << "Cannot find encoder";
				return false;
			}
			enc_ctx_texture =  avcodec_alloc_context3(codec); //out_stream->codec; //
			if (!enc_ctx_texture) {
				std::cout << "Cannot allocate for encoder context";
				return false;
			}

			enc_ctx_texture->height = 4320;
			enc_ctx_texture->width = 7680;
			AVRational aspect_ratio;
			aspect_ratio.num = 16;
			aspect_ratio.den = 9;
			enc_ctx_texture->sample_aspect_ratio = aspect_ratio;
			enc_ctx_texture->pix_fmt = AV_PIX_FMT_RGB24;
			/* video time_base can be set to whatever is handy and supported by encoder */
			AVRational time_base;
			time_base.num = 1;
			time_base.den = 25;
			enc_ctx_texture->time_base = time_base;

			ret = avcodec_open2(enc_ctx_texture, codec, NULL);
			if (ret < 0) {
				char err[4096];
				av_strerror(ret, err, sizeof(err));
				std::cout << "Cannot open video encoder for stream. error def: " << err <<std::endl;
				return false;
			}

			avcodec_parameters_from_context(out_stream_texture->codecpar, enc_ctx_texture);

			std::cout << " stream index for texture files: " << out_stream_texture->index << std::endl;

		}



		



		if (!(format_context->oformat->flags & AVFMT_NOFILE)) {
			ret = avio_open(&format_context->pb, filename, AVIO_FLAG_WRITE);
			if (ret < 0) {
				std::cout << "Could not open output file";
				return false;
			}
		}

		/* init muxer, write output file header */
		ret = avformat_write_header(format_context, NULL);
		if (ret < 0) {
			char err[4096];
			av_strerror(ret, err, sizeof(err));
			std::cout << "Error occurred when opening output file. Error: " << err << std::endl;
			return false;
		}


		//open container
		return true;
	}


     


	/**
	 * Encodes PLY Data and writes it to container that is opened before
	 *
	 * Stream index for ply data is 0
	 *
	 * @param data
	 * contains 3D mesh data in PLY format
	 *
	 * @param length
	 * length of the data
	 *
	 * @param timestamp
	 * timestamp of the data in microseconds format. Make first frame's timestamp  0(zero)
	 *
	 * @param type
	 * type of the content OBJ or PLY
	 * const int OBJ_FORMAT = 0;
	 * const int PLY_FORMAT = 1
	 *
	 * @return boolean
	 * true if it encodes data and writes to the file, otherwise it returns false
	 */







	/**
	 * Writes texture file(PNG) to the container
	 *
	 * Stream index for texture files is 1
	 *
	 */



	bool writeTextureToContainer(char* data, int length, long timestamp, int streamIndex)
	{
		bool result = false;
		//AVPacket enc_pkt;
		
        AVPacket* enc_pkt = av_packet_alloc();
        enc_pkt->data = NULL;
		enc_pkt->size = 0;
		//av_init_packet(&enc_pkt);
		enc_pkt->stream_index = streamIndex;
		enc_pkt->data = (unsigned char*)data;
		enc_pkt->size = length;

		enc_pkt->pts =  av_rescale_q(timestamp, AV_TIME_BASE_Q, out_stream_texture->time_base);
		enc_pkt->dts = enc_pkt->pts;

		int ret = av_write_frame(format_context, enc_pkt);
		if (ret >= 0) {
			result = true;
		}
		else {
			char errbuf[4096];
			av_strerror(ret, errbuf, sizeof(errbuf));
			std::cout << "Could not write to container. Error: " << errbuf << std::endl;
		}
        av_packet_free(&enc_pkt);
		return result;
	}





	/**
	 *  Reads packets from file and stored in the data field
	 *
	 *  @param data
	 *  Decoded 3D Mesh will be stored with PLY format in to this pointer
	 *  if the read packet is compressed 3d mesh
	 *  or
	 *  PNG file will be stored in to this pointer if the read packet is
	 *  texture file
	 *
	 *  @param length
	 *  lenght of the decoded data
	 *
	 *  @param type
	 * type of the content OBJ or PLY
	 * const int OBJ_FORMAT = 0;
	 * const int PLY_FORMAT = 1
	 *
	 *  @return the index of the packet
	 *  if index is zero then packet is 3D Mesh
	 *  if index is 1 then packet is Texture file (PNG)
	 *
	 *  return -1 if it does not read packet successfully
	 */
	int read(char* &data, int &length, int type) {

		if (!isWriteMode)
		{

			AVPacket pkt;
			if (av_read_frame(format_context, &pkt)<0) {
				//std::cout << "Cannot read frame " << std::endl;
				return -1;
			}


			if (pkt.stream_index == STREAM_INDEX_IMG )

			{
				if (tmpDataSize < pkt.size) {
					if ( tmpData ) {
						delete[] tmpData;
					}
					tmpData = new char[pkt.size];
					tmpDataSize = pkt.size;
				}
				memcpy(tmpData, pkt.data, pkt.size);
				data = tmpData;
				length = pkt.size;
			}

			int streamIndex = pkt.stream_index;
			av_packet_unref(&pkt);
			return streamIndex;
		}
		return -1;

	}

	/**
	 * Closes the file. Call this function for both write and read mode
	 *
	 * @return boolean
	 * true if it closes file successfully, otherwise it returns false
	 */
	bool close() {
		if (format_context != NULL) {
			if (isWriteMode) {
				int ret;
				do  {
					//flush buffer
					ret = av_write_frame(format_context, NULL);
				}
				while(ret == 0);

				av_write_trailer(format_context);
				if (enc_ctx != NULL) {
					avcodec_close(enc_ctx);
				}
				avformat_free_context(format_context);
			}
			else {
				avformat_close_input(&format_context);
			}
		}
		if (tmpData != NULL) {
			delete[] tmpData;
		}
		return true;
	}


	int getTmpDataSize() {
		return tmpDataSize;
	}

private:

	AVFormatContext* format_context = NULL;
	AVCodecContext* enc_ctx = NULL;
	AVCodecContext* enc_ctx_texture = NULL;
	AVCodecContext* enc_ctx_space_mesh = NULL;
	AVCodecContext* enc_ctx_space_texture = NULL;
	
	bool isWriteMode;
	AVStream* out_stream_texture;
	char* tmpData = NULL;  //10MB
	int tmpDataSize = 0;
};


#endif /* CONTAINER_H_ */
