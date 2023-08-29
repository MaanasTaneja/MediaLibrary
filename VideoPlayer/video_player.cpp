
#include <iostream>
#include <stdio.h>
#include <string>
#include "media.h"
#include "graphics.h"

//so end i think it should be like this, read next frame or wahetver i use should be able to stream media frames directly to a media steam container and that container will buffer
//all media frames it gets, then that stream container should be ablet o encode all of those FRAMES!  (FULL FRAMES) into any codec or container we would like 

int main()
{
	//std::string filename = "..\\resource\\tesso265remux.mp4";

	//MediaContainer video;

	//malloc_media_container(&video, MEDIA_FILE_INPUT);
	//open_media(&video, filename.c_str());

	//populate_codecs_source(&video);

	//MediaFrame frame;
	//malloc_media_frame(&frame);
	//bool media_error = false;

	//Graphics gfx{ video.codec_description.video_codec_context->width, video.codec_description.video_codec_context->height };
	//gfx.init();
	//gfx.error_check();


	//while (!gfx.livestatus() && !media_error)
	//{
	//	gfx.startFrame();

	//	if (decode_next_frame_video(&video, &frame) < 0) {
	//		media_error = true;
	//	}
	//	else {
	//		gfx.bindScreenQuad();
	//		gfx.bindTexture(frame.video_frame->data);
	//		gfx.draw(frame.frame_pts_seconds);
	//	}
	//	gfx.endFrame();
	//}

	//free_media_frame(&frame);
	//free_media_container(&video);


	//std::string input = "..\\resource\\doom.mp4";
	//std::string output = "..\\resource\\dooooom.ts";

	//MediaContainer input_container;
	//malloc_media_container(&input_container, MEDIA_FILE_INPUT);
	//if (open_media(&input_container, input.c_str()) < 0) {
	//	return -1;
	//}

	//populate_codecs_source(&input_container);

	//MediaContainer output_container;
	//malloc_media_container(&output_container, MEDIA_FILE_OUTPUT);

	//if (open_media(&output_container, output.c_str()) < 0) {
	//	return -1;
	//}

	//populate_codecs_copy(&input_container, &output_container);

	//open_media_write_header(&output_container);
	//remux_media_data(&input_container, &output_container);
	//open_media_write_trailer(&output_container);

	//free_media_container(&input_container);
	//free_media_container(&output_container);


	std::string input = "..\\resource\\test.mp4";
	std::string output = "..\\resource\\tesso265remux.mp4";

	MediaContainer input_container;
	malloc_media_container(&input_container, MEDIA_FILE_INPUT);
	if (open_media(&input_container, input.c_str()) < 0) {
		return -1;
	}

	populate_codecs_source(&input_container);

	MediaContainer output_container;
	malloc_media_container(&output_container, MEDIA_FILE_OUTPUT);

	if (open_media(&output_container, output.c_str()) < 0) {
		return -1;
	}

	populate_codecs_user(&output_container, AV_CODEC_ID_HEVC, AV_CODEC_ID_AAC , input_container.m_width, input_container.m_height,
		input_container.codec_description.m_pix_fmt, 2.5 * 1000 * 1000, 4 * 1000 * 1000, 2 * 1000 * 1000, 4 * 1000 * 1000, input_container.m_fps, 
		input_container.codec_description.m_audio_sample_rate);

	//populate_codecs_user(&output_container, AV_CODEC_ID_HEVC, AV_CODEC_ID_AAC, input_container.m_width, input_container.m_height,
	//	input_container.codec_description.m_pix_fmt, input_container.codec_description.m_bitrate, input_container.codec_description.m_rc_buffer_size, input_container.codec_description.m_rcmaxrate, input_container.codec_description.m_rcminrate,  input_container.m_fps,
	//	input_container.codec_description.m_audio_sample_rate);

	open_media_write_header(&output_container);

	MediaFrame frame;
	malloc_media_frame(&frame);

	MediaPacket pkt_video;
	malloc_media_packet(&pkt_video);

	MediaPacket pkt_audio;
	malloc_media_packet(&pkt_audio);

	bool media_error = false;

	while (!media_error)
	{
		if ((decode_next_frame_video(&input_container, &frame) < 0)) {
					media_error = true;
		}
		else {
			int resp_v = encode_next_frame_video(&output_container, &frame, &pkt_video, input_container.format_context->streams[input_container.m_video_stream_index]->time_base, output_container.format_context->streams[output_container.m_video_stream_index]->time_base);
			if (resp_v == 0) {
				//Only if packet fully encoded, write.
				open_media_write_packet(&output_container, &pkt_video);
			}
		}

	}

	media_error = false;
	reset_input_container_state(&input_container);

	while (!media_error)
	{
		if ((decode_next_frame_audio(&input_container, &frame) < 0)) {
			media_error = true;
		}
		else {
			int resp_a = encode_next_frame_audio(&output_container, &frame, &pkt_audio, input_container.format_context->streams[input_container.m_audio_stream_index]->time_base, output_container.format_context->streams[output_container.m_audio_stream_index]->time_base);
			
			if (resp_a == 0) {
				open_media_write_packet(&output_container, &pkt_audio);
			}
		}

	}

	open_media_write_trailer(&output_container);
	free_media_frame(&frame);
	free_media_packet(&pkt_video);
	free_media_packet(&pkt_audio);
	free_media_container(&input_container);
	free_media_container(&output_container);

	return 0;
} 