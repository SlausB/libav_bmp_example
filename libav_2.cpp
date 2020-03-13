

//sudo apt install libavformat-dev libavcodec-dev libavutil-dev

// This version is for the 0.4.9+ release of ffmpeg. This release adds the
// av_read_frame() API call, which simplifies the reading of video frames 
// considerably.
extern "C"{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <iostream>



/** BMP writer.*/
#define DATA_OFFSET_OFFSET 0x000A
#define WIDTH_OFFSET 0x0012
#define HEIGHT_OFFSET 0x0016
#define BITS_PER_PIXEL_OFFSET 0x001C
#define HEADER_SIZE 14
#define INFO_HEADER_SIZE 40
#define NO_COMPRESION 0
#define MAX_NUMBER_OF_COLORS 0
#define ALL_COLORS_REQUIRED 0
typedef unsigned int int32;
typedef short int16;
typedef unsigned char byte;
void WriteImage(const char *fileName, byte *pixels, int32 width, int32 height,int32 bytesPerPixel)
{
	FILE *outputFile = fopen(fileName, "wb");
	//*****HEADER************//
	const char *BM = "BM";
	fwrite(&BM[0], 1, 1, outputFile);
	fwrite(&BM[1], 1, 1, outputFile);
	int paddedRowSize = (int)(4 * ceil((float)width/4.0f))*bytesPerPixel;
	int32 fileSize = paddedRowSize*height + HEADER_SIZE + INFO_HEADER_SIZE;
	fwrite(&fileSize, 4, 1, outputFile);
	int32 reserved = 0x0000;
	fwrite(&reserved, 4, 1, outputFile);
	int32 dataOffset = HEADER_SIZE+INFO_HEADER_SIZE;
	fwrite(&dataOffset, 4, 1, outputFile);

	//*******INFO*HEADER******//
	int32 infoHeaderSize = INFO_HEADER_SIZE;
	fwrite(&infoHeaderSize, 4, 1, outputFile);
	fwrite(&width, 4, 1, outputFile);
	fwrite(&height, 4, 1, outputFile);
	int16 planes = 1; //always 1
	fwrite(&planes, 2, 1, outputFile);
	int16 bitsPerPixel = bytesPerPixel * 8;
	fwrite(&bitsPerPixel, 2, 1, outputFile);
	//write compression
	int32 compression = NO_COMPRESION;
	fwrite(&compression, 4, 1, outputFile);
	//write image size (in bytes)
	int32 imageSize = width*height*bytesPerPixel;
	fwrite(&imageSize, 4, 1, outputFile);
	int32 resolutionX = 11811; //300 dpi
	int32 resolutionY = 11811; //300 dpi
	fwrite(&resolutionX, 4, 1, outputFile);
	fwrite(&resolutionY, 4, 1, outputFile);
	int32 colorsUsed = MAX_NUMBER_OF_COLORS;
	fwrite(&colorsUsed, 4, 1, outputFile);
	int32 importantColors = ALL_COLORS_REQUIRED;
	fwrite(&importantColors, 4, 1, outputFile);
	int unpaddedRowSize = width*bytesPerPixel;
	for ( decltype(height) i = 0; i < height; i++ )
	{
		int pixelOffset = ((height - i) - 1)*unpaddedRowSize;
		fwrite(&pixels[pixelOffset], 1, paddedRowSize, outputFile); 
	}
	fclose(outputFile);
}



const char * VIDEO_FILE = "video.mkv";

using namespace std;

int main()
{
	av_register_all();
	avformat_network_init();
	avfilter_register_all();

	//crashes on -Ofast without =NULL initialization:
	AVFormatContext * format = NULL;
	if ( avformat_open_input( & format, VIDEO_FILE, NULL, NULL ) != 0 ) {
		cerr << "Could not open file " << VIDEO_FILE << endl;
		return -1;
	}

	// Retrieve stream information
	if ( avformat_find_stream_info( format, NULL ) < 0) {
		cerr << "avformat_find_stream_info() failed." << endl;
		return -1;
	}
	av_dump_format( format, 0, VIDEO_FILE, false );

	AVCodec * video_dec = (AVCodec*)1;
	AVCodec * audio_dec = (AVCodec*)1;
	const auto video_stream_index = av_find_best_stream( format, AVMEDIA_TYPE_VIDEO, -1, -1, & video_dec, 0 );
	const auto audio_stream_index = av_find_best_stream( format, AVMEDIA_TYPE_AUDIO, -1, -1, & audio_dec, 0 );
	if ( video_stream_index < 0 ) {
		cerr << "Failed to find video stream." << endl;
		return -1;
	}
	if ( audio_stream_index < 0 ) {
		cerr << "Failed to find audio stream." << endl;
		return -1;
	}

	AVCodecParameters * videoParams = format->streams[ video_stream_index ]->codecpar;

	const auto W = videoParams->width;
	const auto H = videoParams->height;
	const auto SRC_FORMAT = (AVPixelFormat)videoParams->format;
	//const auto SRC_FORMAT = AV_PIX_FMT_YUV420P;
	cout << "Having " << W << " | " << H << " | " << SRC_FORMAT << " video." << endl;

	av_read_play( format );

	// create decoding context
	AVCodecContext * video_ctx = avcodec_alloc_context3( video_dec );
	AVCodecContext * audio_ctx = avcodec_alloc_context3( audio_dec );
	if ( ! video_ctx || ! audio_ctx ) {
		cerr << "Failed to avcodec_alloc_context3()" << endl;
		return -1;
	}
	if ( video_dec->capabilities & AV_CODEC_CAP_TRUNCATED ) video_ctx->flags |= AV_CODEC_FLAG_TRUNCATED; // we do not send complete frames

	/* For some codecs, such as msmpeg4 and mpeg4, width and height
	MUST be initialized there because this information is not
	available in the bitstream. */

	avcodec_parameters_to_context( video_ctx, format->streams[ video_stream_index ]->codecpar );
	avcodec_parameters_to_context( audio_ctx, format->streams[ audio_stream_index ]->codecpar );
	if ( avcodec_open2( video_ctx, video_dec, NULL ) < 0 ) {
		cout << "Failed to open video decoder." << endl;
		return -1;
	}
	if ( avcodec_open2( audio_ctx, audio_dec, NULL ) < 0 ) {
		cout << "Failed to open audio decoder." << endl;
		return -1;
	}

	AVFrame* pic_src = av_frame_alloc();
	AVFrame* pic_out = av_frame_alloc();

	const auto OUT_FORMAT = AV_PIX_FMT_RGB24;
	const auto ALIGN = 32;
	
	pic_out->format = OUT_FORMAT;
	pic_out->width  = W;
	pic_out->height = H;
	const auto afgbe = av_frame_get_buffer( pic_out, ALIGN );
	if ( afgbe != 0 ) {
		const auto BUFFER_SIZE = 10000;
		auto errbuf = malloc( BUFFER_SIZE );
		av_strerror( afgbe, (char*)errbuf, BUFFER_SIZE );
		cerr << "av_frame_get_buffer() failed with error: " << (const char*)errbuf << endl;
		free( errbuf );
		return -1;
	}

	struct SwsContext * img_convert_ctx = sws_getContext(
		W, H, SRC_FORMAT,
		W, H, OUT_FORMAT,
		SWS_BICUBIC,
		NULL, NULL, NULL
	);
	if ( img_convert_ctx == NULL ) {
		cerr << "Could NOT initialize the conversion context!" << endl;
		return -1;
	}

	AVPacket packet;
	av_init_packet( & packet );

	//for first 10 frames only:
	int packet_number = 0;
	while ( av_read_frame( format, & packet ) >= 0 && packet_number < 10 ) {
		if ( packet.stream_index == video_stream_index ) {
			cout << "Video packet" << endl;
			if ( avcodec_send_packet( video_ctx, & packet ) < 0 ) {
				cerr << "Error sending a packet for decoding" << endl;
				return -1;
			}
			auto ret = 0;
			while ( ret >= 0 ) {
				ret = avcodec_receive_frame( video_ctx, pic_src );
				if ( ret == AVERROR(EAGAIN) || ret == AVERROR_EOF ) break;
				else if ( ret < 0 ) {
					cerr << "Error during decoding" << endl;
					return -1;
				}
				cout << "Frame successfully received: " << pic_src->width << " | " << pic_src->height << endl;
				
				const auto ssr = sws_scale(
					img_convert_ctx,
					pic_src->data,
					pic_src->linesize,
					0,
					H,
					pic_out->data,
					pic_out->linesize
				);
				if ( ssr != H ) {
					cerr << "sws_scale() worked out unexpectedly." << endl;
					return -1;
				}
				
				cout << "Frame successfully decoded. Writing it as BMP ..." << endl;
				std::string name = "./";
				name += std::to_string( packet_number ) + ".bmp";
				WriteImage( name.c_str(), (uint8_t*) pic_out->data, W, H, 3 );
			}
			
			++ packet_number;
		}
		else if ( packet.stream_index == audio_stream_index ) {
			cout << "Sound packet" << endl;
		}
		av_free_packet( & packet );
		av_init_packet( & packet );
	}

	return 0;
}