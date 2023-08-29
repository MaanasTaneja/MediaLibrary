#pragma once
#include <iostream>
#include <string>
#include <cassert>
#include <vector>
#include <queue>
#include <exception>

#define WINDOWS_SYSTEM

#ifdef WINDOWS_SYSTEM
#include <Windows.h>

#define MEDIA_ERROR_CRITICAL 0xF
#define MEDIA_ERROR_WARNING  0xE

static void media_error_submit(std::string message, std::string source, int level, int line, std::string function) {
	std::string response = message + " in function : " + function + ", at line : " + std::to_string(line);
	if (level == MEDIA_ERROR_CRITICAL) {
		MessageBoxA(NULL, response.c_str(), "Media Library", ERROR);
		std::exit(-1);
	}
	else if (level == MEDIA_ERROR_WARNING) {
		std::cout << response << std::endl;
	}
}

#else
#define MEDIA_ERROR_CRITICAL 0xF
#define MEDIA_ERROR_WARNING  0xE

static void media_error_submit(std::string message, std::string source, int level, int line, std::string function) {
	std::string response = message + " in function : " + function + ", at line : " + std::to_string(line);
	if (level == MEDIA_ERROR_CRITICAL) {
		std::cout << response << std::endl;
		throw std::runtime_error(response);
	}
	else if (level == MEDIA_ERROR_WARNING) {
		std::cout << response << std::endl;
	}
}

#endif

extern "C" {
#include <ffmpeg/include/libavcodec/avcodec.h>
#include <ffmpeg/include/libavformat/avformat.h>
#include <ffmpeg/include/libswscale/swscale.h>
}

//It takes much longer to decode 265 than 264.
#pragma warning(disable : 4996)

#define MEDIA_STREAM_VIDEO_CODEC AV_CODEC_ID_H264
#define MEDIA_STREAM_AUDIO_CODEC AV_CODEC_ID_MP3

enum media_type {
	MEDIA_FILE_INPUT = 0,
	MEDIA_FILE_OUTPUT = 1,

	MEDIA_STREAM_RTP = 2,
	MEDIA_STREAM_FILE = 3,
};

typedef struct {
	AVCodecParameters* video_cparam;
	AVCodecParameters* audio_cparam;

	AVCodec* video_codec;
	AVCodec* audio_codec;

	AVCodecContext* video_codec_context;
	AVCodecContext* audio_codec_context;

	int m_pix_fmt;
	int m_video_codec_id;
	int m_audio_codec_id;

	int m_bitrate;
	int m_rc_buffer_size;
	int m_rcmaxrate;
	int m_rcminrate;

	int m_audio_sample_rate;

}MediaCodecDescriptor;

typedef struct {
	AVRational video_time_base;
	int fps;

	AVRational audio_time_base;
	int audio_sample_rate;
}MediaTime;

typedef struct {
	media_type type;

	AVFormatContext* format_context;
	int m_video_stream_index;
	int m_audio_stream_index;
	int m_subtitle_stream_index;

	MediaCodecDescriptor codec_description;

	AVRational time_base;

	int m_width;
	int m_height;

	int m_fps;

}MediaContainer;

typedef struct {
	bool frame_built;
	AVFrame* video_frame;
	AVFrame* audio_frame;

	AVPacket* t_current_packet;
	double frame_number;
	double frame_pts;

	double frame_pts_seconds;
}MediaFrame;

typedef struct {
	AVPacket* packet;
	long pts;
}MediaPacket;

typedef AVRational MediaRational;

//MediaFrame -> Decoded Data, MediaPacket -> EncodedData

typedef struct {
	media_type type;
	int stream_width;
	int stream_height;
	MediaCodecDescriptor codec_description;

	std::deque<AVPacket*> file_packet_stack_buffer;  //But storing pointers is stupid, as memory will be reused!
	std::deque<std::vector<uint8_t>> rtp_packet_stack_buffer;
	bool backed_up;
}MediaStreamContainer;

struct uint8_t_packet {
	std::vector<uint8_t> packet = { 0,0,0,1 };
	long sequence_number;
};

struct rtp_raw_frame {
	std::vector<uint8_t> generated_frame_packet;

	void init(size_t default_size = 100000) {
		generated_frame_packet.reserve(default_size);
	}

	void append_pkt(uint8_t_packet& p) {
		generated_frame_packet.insert(generated_frame_packet.end(), p.packet.begin(), p.packet.end());
	}

	bool ready() {
		return (generated_frame_packet.size() > (34) ? true : false);
	}
	void clear() {
		generated_frame_packet.clear();
	}

	void show_values() {
		std::cout << "Frame packet size: " << generated_frame_packet.size() << std::endl;
	}

};

typedef struct {
	AVRational time_base;
	int m_fps;

	std::deque<MediaPacket> stack_buffer_video;
	std::deque<MediaPacket> stack_buffer_audio;

	float last_frame_pts_video;
	float last_frame_pts_audio;

	bool backed_up;
}MediaFileStreamingBuffer;

//Container functions
int malloc_media_container(MediaContainer* media, int mode);
void free_media_container(MediaContainer* media);
int open_media(MediaContainer* media, const char* filename);
int open_media_write_header(MediaContainer* media);
int open_media_write_packet(MediaContainer* media, MediaPacket* packet);
int open_media_write_trailer(MediaContainer* media);
static void populate_internal_structures(MediaContainer* media, int vcodecid, int acodecid, int width, int height, int pix_format, int bitrate, int rc_buffer_size, int rcmaxrate, int rcminrate, int fps, int audio_sample_rate);
int populate_codecs_source(MediaContainer* media);
int populate_codecs_copy(MediaContainer* media_from, MediaContainer* media_to);
int populate_codecs_user(MediaContainer* media, int vcodecid, int acodecid, int width, int height, int pix_format, int bitrate, int rc_buffer_size, int rcmaxrate, int rcminrate, float timebase_den, int audio_sample_rate);
int remux_media_data(MediaContainer* media_from, MediaContainer* media_to);
void reset_input_container_state(MediaContainer* media);
//Frame functions
int malloc_media_frame(MediaFrame* frame);
void free_media_frame(MediaFrame* frame);
int malloc_media_packet(MediaPacket* packet);
void free_media_packet(MediaPacket* packet);
static int decode_video_packet(MediaCodecDescriptor& codec, MediaFrame* frame);
static int decode_audio_packet(MediaCodecDescriptor& codec, MediaFrame* frame);	
int encode_next_frame_video(MediaContainer* media, MediaFrame* frame, MediaPacket* packet, MediaRational time_from, MediaRational time_to);
int encode_next_frame_audio(MediaContainer* media, MediaFrame* frame, MediaPacket* packet, MediaRational time_from, MediaRational time_to);
int decode_next_frame_video(MediaContainer* media, MediaFrame* frame);
int decode_next_frame_audio(MediaContainer* media, MediaFrame* frame);
int decode_next_frame_video(MediaStreamContainer* media, MediaFrame* frame);
int decode_next_frame_audio(MediaStreamContainer* media, MediaFrame* frame);
void retrieve_pts_seconds(MediaContainer* media, MediaFrame* frame);
//rtp stream capture functions, useful for WebRTC, media streaming purposes, tested for video RTC connections, able to capture H264/H265 packets and decode them in real time.
int malloc_media_stream_container(MediaStreamContainer* media, int width, int height);
void free_media_stream_container(MediaStreamContainer* media);
static int media_stream_request_packet(MediaStreamContainer* media, std::vector<uint8_t>& packet);  //Only recieves the one packet (queued packet)
static void media_stream_convert_av(AVPacket* av_packet, std::vector<uint8_t>& packet);
void media_stream_submit_packet(MediaStreamContainer* media, const std::vector<uint8_t>& packet); //Sends all packets received async to queue. Use this function when you recieve new RTP Packets.
//media file straming functions
void malloc_media_file_stream_container(MediaFileStreamingBuffer* media, float timebase_num, float timebase_den, int fps);
void free_media_file_stream_container(MediaFileStreamingBuffer* media);
static float media_file_stream_gen_pts_video(MediaFileStreamingBuffer* media);
static float media_file_stream_gen_pts_audio(MediaFileStreamingBuffer* media);
void media_submit_file_stream_packet_video(MediaFileStreamingBuffer* media, MediaPacket packet);
void media_submit_file_stream_packet_audio(MediaFileStreamingBuffer* media, MediaPacket packet);
int media_request_file_stream_packet_video(MediaFileStreamingBuffer* media, MediaPacket& packet);
int media_request_file_stream_packet_audio(MediaFileStreamingBuffer* media, MediaPacket& packet);
//Util functions 
static void save_gray_frame(unsigned char* buf, int wrap, int xsize, int ysize, const char* filename)
{
	FILE* f;
	int i;
	f = fopen(filename, "w");
	// writing the minimal required header for a pgm file format
	fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);

	// writing line by line
	for (i = 0; i < ysize; i++)
		fwrite(buf + i * wrap, 1, xsize, f);
	fclose(f);
}
static int save_frame_as_jpeg(AVFrame* pFrame, int FrameNo)
{
	AVCodec* jpegCodec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
	if (!jpegCodec) { return -1; }

	AVCodecContext* jpegContext = avcodec_alloc_context3(jpegCodec);
	if (!jpegContext) { return -1; }

	jpegContext->pix_fmt = AV_PIX_FMT_YUVJ420P;
	jpegContext->height = pFrame->height;
	jpegContext->width = pFrame->width;

	jpegContext->time_base = { 1,25 };

	if (avcodec_open2(jpegContext, jpegCodec, NULL) < 0)
	{
		return -1;
	}

	FILE* JPEGFile;
	std::string path = "..\\resource\\firstframe";

	AVPacket packet;
	packet.data = NULL;
	packet.size = 0;
	av_init_packet(&packet);
	int gotFrame = 0;

	if (avcodec_encode_video2(jpegContext, &packet, pFrame, &gotFrame) < 0)
	{
		return -1;
	}

	path = path + std::to_string(FrameNo) + ".jpg";
	JPEGFile = fopen(path.c_str(), "wb");
	fwrite(packet.data, 1, packet.size, JPEGFile);
	fflush(JPEGFile);
	fclose(JPEGFile);

	av_free_packet(&packet);
	avcodec_close(jpegContext);
	return 0;
}
static std::string uint8_vector_to_hex_string(const std::vector<uint8_t>& v) {
	std::string result;
	if (v.size() < 100) {
		result.resize(v.size() * 2);
		const char letters[] = "0123456789ABCDEF";
		char* current_hex_char = &result[0];
		for (int i = 0; i < v.size(); i++) {
			uint8_t b = v[i];
			*current_hex_char++ = letters[b >> 4];
			*current_hex_char++ = letters[b & 0xf];
		}
	}
	else {
		result.resize(100 * 2);
		const char letters[] = "0123456789ABCDEF";
		char* current_hex_char = &result[0];
		for (int i = 0; i < 100; i++) {
			uint8_t b = v[i];
			*current_hex_char++ = letters[b >> 4];
			*current_hex_char++ = letters[b & 0xf];
		}
	}
	return result;
}