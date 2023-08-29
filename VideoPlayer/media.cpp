#include "media.h"


//Return 0 if successful, return -1 if failure.
int malloc_media_container(MediaContainer* media, int mode) {

	media->format_context = NULL;

	media->codec_description.video_cparam = NULL;
	media->codec_description.audio_cparam = NULL;

	media->codec_description.video_codec = NULL;
	media->codec_description.audio_codec = NULL;

	media->m_audio_stream_index = -1;
	media->m_video_stream_index = -1;

	media->codec_description.video_codec_context = NULL;
	media->codec_description.audio_codec_context = NULL;

	media->type = static_cast<media_type>(mode);

	return 0;
}

//It is easier to create a MediaContainer object which has static duration, as it contains simply pointers which can be allocated on stack.
void free_media_container(MediaContainer* media) {
	avcodec_free_context(&media->codec_description.video_codec_context);
	avcodec_free_context(&media->codec_description.audio_codec_context);
	avformat_close_input(&media->format_context);
}

int open_media(MediaContainer* media, const char* filename) {
	if (media->type == MEDIA_FILE_INPUT) {
		media->format_context = avformat_alloc_context();
		if (!media->format_context) {
			media_error_submit("Media format could not be allocated on memory!", __FILE__, MEDIA_ERROR_CRITICAL, __LINE__, __FUNCTION__);
			return -1;
		}

		if (avformat_open_input(&media->format_context, filename, NULL, NULL) == 0) {
			std::cout << "Opening File: " << filename << ", File Format: " << media->format_context->iformat->long_name << std::endl;
		}
		else {
			media_error_submit("File could not be opened", __FILE__, MEDIA_ERROR_CRITICAL, __LINE__, __FUNCTION__);
			return -1;
		}

		if (avformat_find_stream_info(media->format_context, NULL) < 0) {
			media_error_submit("Could not find stream information in container!", __FILE__, MEDIA_ERROR_WARNING, __LINE__, __FUNCTION__);
			return -1;
		}
		else {
			return 0;
		}
	}
	else {
		//Here different file types will be analyzed and different file types will write different headers.
		int resp;
		resp = avformat_alloc_output_context2(&media->format_context, NULL, NULL, filename);
		if (!resp) {
			if (!(media->format_context->oformat->flags & AVFMT_NOFILE)) {
				resp = avio_open(&media->format_context->pb, filename, AVIO_FLAG_WRITE);
				if (resp < 0) {
					media_error_submit("File could not be opened", __FILE__, MEDIA_ERROR_CRITICAL, __LINE__, __FUNCTION__);
					return -1;
				}
			}
			else {
				media_error_submit("Output file could not be opened due to format flags null!", __FILE__, MEDIA_ERROR_CRITICAL, __LINE__, __FUNCTION__);
				return -1;
			}
			//Header contains num of stream information, which haven't been added yet! So I must first copy streams and then write header.
			return 0; 
		}
		else { 
			media_error_submit("Output format failed to be allocated! : ", __FILE__, MEDIA_ERROR_CRITICAL, __LINE__, __FUNCTION__);
			return - 1; 
		}
	}

}

int open_media_write_header(MediaContainer* media) {
	if (media->type != MEDIA_FILE_OUTPUT) {
		media_error_submit("Cannot write to an input file!", __FILE__, MEDIA_ERROR_CRITICAL, __LINE__, __FUNCTION__);
		return -1;
	}

	int resp = avformat_write_header(media->format_context, NULL);
	if (resp < 0) {
		media_error_submit("Output file header could not be written!", __FILE__, MEDIA_ERROR_CRITICAL, __LINE__, __FUNCTION__);
		return -1;
	}
	//Set timebase.
	media->time_base = media->format_context->streams[media->m_video_stream_index]->time_base;

	return 0;
}

int open_media_write_packet(MediaContainer* media, MediaPacket* packet) {
	packet->pts = packet->packet->pts;
	int ret = av_interleaved_write_frame(media->format_context, packet->packet);
	if (ret < 0) {
		media_error_submit("Packet mux error while copying!", __FILE__, MEDIA_ERROR_WARNING, __LINE__, __FUNCTION__);
		return -1;
	}
	return 0;
}

int open_media_write_trailer(MediaContainer* media) {
	if (media->type != MEDIA_FILE_OUTPUT) {
		media_error_submit("Cannot write to an input file!", __FILE__, MEDIA_ERROR_CRITICAL, __LINE__, __FUNCTION__);
		return -1;
	}

	int resp = av_write_trailer(media->format_context);
	if (resp < 0) {
		media_error_submit("Output file trailer could not be written!", __FILE__, MEDIA_ERROR_CRITICAL, __LINE__, __FUNCTION__);
		return -1;
	}
	return 0;
}

void reset_input_container_state(MediaContainer* media) {
	av_seek_frame(media->format_context, media->m_video_stream_index, 0, AVSEEK_FLAG_ANY);
	av_seek_frame(media->format_context, media->m_audio_stream_index, 0, AVSEEK_FLAG_ANY);
}

static void populate_internal_structures(MediaContainer* media, int vcodecid, int acodecid, int width, int height, int pix_format, int bitrate, int rc_buffer_size, int rcmaxrate, int rcminrate, int fps, int audio_sample_rate) {
	media->m_fps = fps;
	media->m_width = width;
	media->m_height = height;

	media->codec_description.m_video_codec_id = vcodecid;
	media->codec_description.m_audio_codec_id = acodecid;
	media->codec_description.m_bitrate = bitrate;
	media->codec_description.m_pix_fmt = pix_format;
	media->codec_description.m_rc_buffer_size = rc_buffer_size;
	media->codec_description.m_rcmaxrate = rcmaxrate;
	media->codec_description.m_rcminrate = rcminrate;
	media->codec_description.m_audio_sample_rate = audio_sample_rate;

}

int populate_codecs_source(MediaContainer* media) {
	AVCodecParameters* cparamptr = NULL;
	AVCodec* codecptr = NULL;

	for (int i = 0; i < media->format_context->nb_streams; i++) {
		AVStream* currentStream = media->format_context->streams[i];
		AVCodecParameters* paramtemp = NULL;
		AVCodec* codectemp = NULL;

		paramtemp = currentStream->codecpar;
		codectemp = avcodec_find_decoder(paramtemp->codec_id);
		if (codectemp == NULL)
		{
			media_error_submit("Unsupported codec detected in stream", __FILE__, MEDIA_ERROR_WARNING, __LINE__, __FUNCTION__);
			continue;
		}

		if (paramtemp->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			media->codec_description.video_codec = codectemp;
			media->codec_description.video_cparam = paramtemp;
			media->m_video_stream_index = i;
		}
		else if (paramtemp->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			media->codec_description.audio_codec = codectemp;
			media->codec_description.audio_cparam = paramtemp;
			media->m_audio_stream_index = i;
		}
	}

	media->codec_description.video_codec_context = avcodec_alloc_context3(media->codec_description.video_codec);
	if (!media->codec_description.video_codec_context) {
		media_error_submit("Video codec context could not init!", __FILE__, MEDIA_ERROR_WARNING, __LINE__, __FUNCTION__);
		return -1;
	}

	if (avcodec_parameters_to_context(media->codec_description.video_codec_context, media->codec_description.video_cparam) < 0) {
		media_error_submit("Video params not copied!", __FILE__, MEDIA_ERROR_WARNING, __LINE__, __FUNCTION__);
		return -1;
	}

	media->codec_description.audio_codec_context = avcodec_alloc_context3(media->codec_description.audio_codec);
	if (!media->codec_description.audio_codec_context) {
		media_error_submit("Audio codec context could not init!", __FILE__, MEDIA_ERROR_WARNING, __LINE__, __FUNCTION__);
		return -1;
	}

	if (avcodec_parameters_to_context(media->codec_description.audio_codec_context, media->codec_description.audio_cparam) < 0) {
		media_error_submit("Audio params not copied!", __FILE__, MEDIA_ERROR_WARNING, __LINE__, __FUNCTION__);
		return -1;
	}


	if (avcodec_open2(media->codec_description.video_codec_context, media->codec_description.video_codec, NULL) < 0)
	{
		media_error_submit("Couldn't open video codec context!", __FILE__, MEDIA_ERROR_WARNING, __LINE__, __FUNCTION__);
		return -1;
	}

	if (avcodec_open2(media->codec_description.audio_codec_context, media->codec_description.audio_codec, NULL) < 0)
	{
		media_error_submit("Couldn't audio open codec context!", __FILE__, MEDIA_ERROR_WARNING, __LINE__, __FUNCTION__);
		return -1;
	}


	media->time_base = media->format_context->streams[media->m_video_stream_index]->time_base;
	std::cout << "Video Stream index: " << media->m_video_stream_index << ", Audio Stream index: " << media->m_audio_stream_index << std::endl;

	AVRational guess_fps = av_guess_frame_rate(media->format_context, media->format_context->streams[media->m_video_stream_index], NULL);
	populate_internal_structures(media, media->format_context->video_codec_id, media->format_context->audio_codec_id, media->format_context->streams[media->m_video_stream_index]->codec->width, media->format_context->streams[media->m_video_stream_index]->codec->height, media->format_context->streams[media->m_video_stream_index]->codec->pix_fmt, media->format_context->streams[media->m_video_stream_index]->codec->bit_rate, media->format_context->streams[media->m_video_stream_index]->codec->rc_buffer_size, media->format_context->streams[media->m_video_stream_index]->codec->rc_max_rate, media->format_context->streams[media->m_video_stream_index]->codec->rc_min_rate, guess_fps.num, media->format_context->streams[media->m_audio_stream_index]->codecpar->sample_rate);

}

int populate_codecs_copy(MediaContainer* media_from, MediaContainer* media_to) { 
	//Haven't added subtitle stream population and copying!
	//We have to create streams for this container, as this was an empty container to begin with. Unlike a source file.
	AVStream* video_stream_out;
	AVStream* audio_stream_out;

	//Copy codecparams from input file. As we are simply remuxing.
	AVCodecParameters* video_stream_param = media_from->format_context->streams[media_from->m_video_stream_index]->codecpar;
	AVCodecParameters* audio_stream_param = media_from->format_context->streams[media_from->m_audio_stream_index]->codecpar;

	if (media_to->m_video_stream_index == -1 && media_to->m_audio_stream_index == -1) {
		video_stream_out = avformat_new_stream(media_to->format_context, NULL);
		if (!video_stream_out) {
			media_error_submit("Media Stream creation while copying failed!", __FILE__, MEDIA_ERROR_WARNING, __LINE__, __FUNCTION__);
			return -1;
		}

		audio_stream_out = avformat_new_stream(media_to->format_context, NULL);
		if (!audio_stream_out) {
			media_error_submit("Media Stream creation while copying failed!", __FILE__, MEDIA_ERROR_WARNING, __LINE__, __FUNCTION__);
			return -1;
		}

		int r = avcodec_parameters_copy(video_stream_out->codecpar, video_stream_param);
		if (r < 0) {
			media_error_submit("Codec param copy failed!", __FILE__, MEDIA_ERROR_WARNING, __LINE__, __FUNCTION__);
			return - 1;
		}

		r = avcodec_parameters_copy(audio_stream_out->codecpar, audio_stream_param);
		if (r < 0) {
			media_error_submit("Codec param copy failed!", __FILE__, MEDIA_ERROR_WARNING, __LINE__, __FUNCTION__);
			return -1;
		}

		for (int i = 0; i < media_to->format_context->nb_streams; i++) {
			AVStream* currentStream = media_to->format_context->streams[i];
			AVCodecParameters* paramtemp = NULL;
			AVCodec* codectemp = NULL;

			paramtemp = currentStream->codecpar;
			codectemp = avcodec_find_decoder(paramtemp->codec_id);
			if (codectemp == NULL)
			{
				media_error_submit("Unsupported codec detected in stream!", __FILE__, MEDIA_ERROR_WARNING, __LINE__, __FUNCTION__);
				continue;
			}

			if (paramtemp->codec_type == AVMEDIA_TYPE_VIDEO)
			{
				media_to->codec_description.video_codec = codectemp;
				media_to->codec_description.video_cparam = paramtemp;
				media_to->m_video_stream_index = i;
			}
			else if (paramtemp->codec_type == AVMEDIA_TYPE_AUDIO)
			{
				media_to->codec_description.audio_codec = codectemp;
				media_to->codec_description.audio_cparam = paramtemp;
				media_to->m_audio_stream_index = i;
			}
		}
	}
	else {
		video_stream_out = media_to->format_context->streams[media_to->m_video_stream_index];
		if (!video_stream_out) {
			media_error_submit("Media Stream reutilization while copying failed!", __FILE__, MEDIA_ERROR_WARNING, __LINE__, __FUNCTION__);
			return -1;
		}

		audio_stream_out = media_to->format_context->streams[media_to->m_audio_stream_index];
		if (!audio_stream_out) {
			media_error_submit("Media Stream reutilization while copying failed!", __FILE__, MEDIA_ERROR_WARNING, __LINE__, __FUNCTION__);
			return -1;
		}

		int r = avcodec_parameters_copy(video_stream_out->codecpar, video_stream_param);
		if (r < 0) {
			media_error_submit("Codec param copy failed!", __FILE__, MEDIA_ERROR_WARNING, __LINE__, __FUNCTION__);
			return -1;
		}

		r = avcodec_parameters_copy(audio_stream_out->codecpar, audio_stream_param);
		if (r < 0) {
			media_error_submit("Codec param copy failed!", __FILE__, MEDIA_ERROR_WARNING, __LINE__, __FUNCTION__);
			return -1;
		}

		for (int i = 0; i < media_to->format_context->nb_streams; i++) {
			AVStream* currentStream = media_to->format_context->streams[i];
			AVCodecParameters* paramtemp = NULL;
			AVCodec* codectemp = NULL;

			paramtemp = currentStream->codecpar;
			codectemp = avcodec_find_decoder(paramtemp->codec_id);
			if (codectemp == NULL)
			{
				media_error_submit("Unsupported codec detected in stream!", __FILE__, MEDIA_ERROR_WARNING, __LINE__, __FUNCTION__);
				continue;
			}

			if (paramtemp->codec_type == AVMEDIA_TYPE_VIDEO)
			{
				media_to->codec_description.video_codec = codectemp;
				media_to->codec_description.video_cparam = paramtemp;
				media_to->m_video_stream_index = i;
			}
			else if (paramtemp->codec_type == AVMEDIA_TYPE_AUDIO)
			{
				media_to->codec_description.audio_codec = codectemp;
				media_to->codec_description.audio_cparam = paramtemp;
				media_to->m_audio_stream_index = i;
			}
		}
	}
	

	//Codecs and params copied, now open them all for use.

	media_to->codec_description.video_codec_context = avcodec_alloc_context3(media_to->codec_description.video_codec);
	if (!media_to->codec_description.video_codec_context) {
		media_error_submit("Video codec context could not init!", __FILE__, MEDIA_ERROR_WARNING, __LINE__, __FUNCTION__);
		return -1;
	}

	if (avcodec_parameters_to_context(media_to->codec_description.video_codec_context, media_to->codec_description.video_cparam) < 0) {
		media_error_submit("Video params not copied!", __FILE__, MEDIA_ERROR_WARNING, __LINE__, __FUNCTION__);
		return -1;
	}

	media_to->codec_description.audio_codec_context = avcodec_alloc_context3(media_to->codec_description.audio_codec);
	if (!media_to->codec_description.audio_codec_context) {
		media_error_submit("Audio codec context could not init!", __FILE__, MEDIA_ERROR_WARNING, __LINE__, __FUNCTION__);
		return -1;
	}

	if (avcodec_parameters_to_context(media_to->codec_description.audio_codec_context, media_to->codec_description.audio_cparam) < 0) {
		media_error_submit("Audio params not copied!", __FILE__, MEDIA_ERROR_WARNING, __LINE__, __FUNCTION__);
		return -1;
	}


	if (avcodec_open2(media_to->codec_description.video_codec_context, media_to->codec_description.video_codec, NULL) < 0)
	{
		media_error_submit("Couldn't open video codec context!", __FILE__, MEDIA_ERROR_WARNING, __LINE__, __FUNCTION__);
		return -1;
	}

	if (avcodec_open2(media_to->codec_description.audio_codec_context, media_to->codec_description.audio_codec, NULL) < 0)
	{
		media_error_submit("Couldn't audio open codec context!", __FILE__, MEDIA_ERROR_WARNING, __LINE__, __FUNCTION__);
		return -1;
	}


	media_to->time_base = media_to->format_context->streams[media_to->m_video_stream_index]->time_base;


	AVRational guess_fps = av_guess_frame_rate(media_from->format_context, media_from->format_context->streams[media_from->m_video_stream_index], NULL);
	populate_internal_structures(media_to, media_to->format_context->video_codec_id, media_to->format_context->audio_codec_id, media_to->format_context->streams[media_to->m_video_stream_index]->codec->width, media_to->format_context->streams[media_to->m_video_stream_index]->codec->height, media_to->format_context->streams[media_to->m_video_stream_index]->codec->pix_fmt, media_to->format_context->streams[media_to->m_video_stream_index]->codec->bit_rate, media_to->format_context->streams[media_to->m_video_stream_index]->codec->rc_buffer_size, media_to->format_context->streams[media_to->m_video_stream_index]->codec->rc_max_rate, media_to->format_context->streams[media_to->m_video_stream_index]->codec->rc_min_rate, guess_fps.num, media_to->format_context->streams[media_to->m_audio_stream_index]->codecpar->sample_rate);


	return 0;

}

/*
In Libav, as far as I can understand, it probably registers and records each codec / stream i create and gives me back a pointer to access that object and if i assign that pointer
to some other libav object as args or otherwise, that object continues to live even after my temp pointer used to access that resource is destroyed. And memory is NOT LEAKED.
As I can close the format etc and that should close all the other resources it acquired.
*/

int populate_codecs_user(MediaContainer* media, int vcodecid, int acodecid, int width, int height, int pix_format, int bitrate, int rc_buffer_size, int rcmaxrate, int rcminrate, float timebase_den,
						int audio_sample_rate) {
	//Create video stream in container, and create a new encoder with given id, then copy codec params to the stream.
	AVCodec* video_codec = avcodec_find_encoder(static_cast<AVCodecID>(vcodecid));
	if (!video_codec) {
		media_error_submit("Codec was not found! Recheck given codec ID!", __FILE__, MEDIA_ERROR_CRITICAL, __LINE__, __FUNCTION__);
		return -1;
	}

	AVCodecContext* video_codec_ctx = avcodec_alloc_context3(video_codec);
	if (!video_codec_ctx) {
		media_error_submit("Codec context could not be allocated!", __FILE__, MEDIA_ERROR_CRITICAL, __LINE__, __FUNCTION__);
		return -1;
	}

	AVStream* video_stream = avformat_new_stream(media->format_context, NULL);

	video_codec_ctx->pix_fmt = static_cast<AVPixelFormat>(pix_format);
	video_codec_ctx->width = width;
	video_codec_ctx->height = height;
	video_codec_ctx->bit_rate = bitrate;
	video_codec_ctx->rc_buffer_size = rc_buffer_size;
	video_codec_ctx->rc_max_rate = rcmaxrate;
	video_codec_ctx->rc_min_rate = rcminrate;

	video_codec_ctx->time_base = (av_make_q(1, timebase_den));
	video_stream->time_base = video_codec_ctx->time_base;

	if (avcodec_open2(video_codec_ctx, video_codec, NULL) < 0) {
		media_error_submit("Codec could not be opened!", __FILE__, MEDIA_ERROR_WARNING, __LINE__, __FUNCTION__);
		return -1;
	}

	//Attach codec to stream.
	avcodec_parameters_from_context(video_stream->codecpar, video_codec_ctx);

	//Audio Codec and Stream

	AVCodec* audio_codec = avcodec_find_encoder(static_cast<AVCodecID>(acodecid));
	if (!audio_codec) {
		media_error_submit("Codec was not found! Recheck given codec ID!", __FILE__, MEDIA_ERROR_CRITICAL, __LINE__, __FUNCTION__);
		return -1;
	}
	AVCodecContext* audio_codec_ctx = avcodec_alloc_context3(audio_codec);
	if (!audio_codec_ctx) {
		media_error_submit("Codec context could not be allocated!", __FILE__, MEDIA_ERROR_CRITICAL, __LINE__, __FUNCTION__);
		return -1;
	}

	AVStream* audio_stream = avformat_new_stream(media->format_context, NULL);

	int OUTPUT_CHANNELS = 2;
	int OUTPUT_BIT_RATE = 196000;
	audio_codec_ctx->channels = OUTPUT_CHANNELS;
	audio_codec_ctx->channel_layout = av_get_default_channel_layout(OUTPUT_CHANNELS);
	audio_codec_ctx->sample_rate = audio_sample_rate;
	audio_codec_ctx->sample_fmt = audio_codec->sample_fmts[0];
	audio_codec_ctx->bit_rate = OUTPUT_BIT_RATE;
	audio_codec_ctx->time_base = av_make_q( 1, audio_sample_rate );

	audio_codec_ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
	audio_stream->time_base = audio_codec_ctx->time_base;

	if (avcodec_open2(audio_codec_ctx, audio_codec, NULL) < 0) {
		media_error_submit("Codec could not be opened!", __FILE__, MEDIA_ERROR_WARNING, __LINE__, __FUNCTION__);
		return -1;
	}

	//Attach codec to stream.
	avcodec_parameters_from_context(audio_stream->codecpar, audio_codec_ctx);

	//Set all ctx's in our structures also.

	for (int i = 0; i < media->format_context->nb_streams; i++) {
		AVStream* currentStream = media->format_context->streams[i];
		AVCodecParameters* paramtemp = NULL;
		AVCodec* codectemp = NULL;
		AVCodec* codecctxtemp = NULL;

		paramtemp = currentStream->codecpar;
		codectemp = avcodec_find_decoder(paramtemp->codec_id);
		if (codectemp == NULL)
		{
			media_error_submit("Unsupported codec detected in stream!", __FILE__, MEDIA_ERROR_WARNING, __LINE__, __FUNCTION__);
			continue;
		}

		if (paramtemp->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			media->codec_description.video_codec = codectemp;
			media->codec_description.video_cparam = paramtemp;
			media->m_video_stream_index = i;
		}
		else if (paramtemp->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			media->codec_description.audio_codec = codectemp;
			media->codec_description.audio_cparam = paramtemp;
			media->m_audio_stream_index = i;
		}
	}


	//Codec contexts already open and ready for use, simply copy them.

	media->codec_description.video_codec_context = video_codec_ctx;
	media->codec_description.audio_codec_context = audio_codec_ctx;


	AVRational timebase = av_make_q(1, timebase_den);
	double fps = timebase_den / (1/timebase_den);
	//fps still not correct

	populate_internal_structures(media, vcodecid, acodecid, width, height, pix_format, bitrate, rc_buffer_size, rcmaxrate , rcminrate, fps, audio_sample_rate);
	//Time base for output must be set after header has been written! Else the timebase ios nearly always 1/30.

	return 0;


}

int remux_media_data(MediaContainer* media_from, MediaContainer* media_to) {
	bool failure = false;
	bool eof = false;
	AVPacket packet;
	
	while ((!failure) && (!eof)) {
		int response = av_read_frame(media_from->format_context, &packet);
		switch (response) {
		case 0: {
			//Success!
			//Find which stream read packet belongs to and write to output.

			if (packet.stream_index == media_from->m_video_stream_index) {

				packet.pts = av_rescale_q_rnd(packet.pts, media_from->format_context->streams[packet.stream_index]->time_base, media_to->format_context->streams[media_to->m_video_stream_index]->time_base, AV_ROUND_NEAR_INF);
				packet.dts = av_rescale_q_rnd(packet.dts, media_from->format_context->streams[packet.stream_index]->time_base, media_to->format_context->streams[media_to->m_video_stream_index]->time_base, AV_ROUND_NEAR_INF);
				packet.duration = av_rescale_q(packet.duration, media_from->format_context->streams[packet.stream_index]->time_base, media_to->format_context->streams[media_to->m_video_stream_index]->time_base);

				packet.pos = -1;
				//Convert to library structure.
				MediaPacket pack;
				malloc_media_packet(&pack);
				pack.packet = &packet;
				if (open_media_write_packet(media_to, &pack) < 0)
				{
					failure = true;
				}
				av_packet_unref(&packet);
			}
			else if (packet.stream_index == media_from->m_audio_stream_index) {
				packet.pts = av_rescale_q_rnd(packet.pts, media_from->format_context->streams[packet.stream_index]->time_base, media_to->format_context->streams[media_to->m_audio_stream_index]->time_base, AV_ROUND_NEAR_INF);
				packet.dts = av_rescale_q_rnd(packet.dts, media_from->format_context->streams[packet.stream_index]->time_base, media_to->format_context->streams[media_to->m_audio_stream_index]->time_base, AV_ROUND_NEAR_INF);
				packet.duration = av_rescale_q(packet.duration, media_from->format_context->streams[packet.stream_index]->time_base, media_to->format_context->streams[media_to->m_audio_stream_index]->time_base);

				packet.pos = -1;
				//Convert to library structure.
				MediaPacket pack;
				malloc_media_packet(&pack);
				pack.packet = &packet;
				if (open_media_write_packet(media_to, &pack) < 0)
				{
					failure = true;
				}
				av_packet_unref(&packet);
			}
			else {
				av_packet_unref(&packet);
				media_error_submit("Undefined stream packet read!", __FILE__, MEDIA_ERROR_WARNING, __LINE__, __FUNCTION__);
				failure = true;
			}

			break;
		}
		case AVERROR(EAGAIN): {
			//std::cout << "AVERROR(EAGAIN) on decode!" << std::endl;
			break;
		}
		case AVERROR(EINVAL): {
			//std::cout << "AVERROR(EINVAL) on decode!" << std::endl;
			break;
		}
		case AVERROR_EOF: {
			//std::cout << "AVERROR_EOF on decode!" << std::endl;
			eof = true;
			break;
		}
		case AVERROR(ENOMEM): {
			//std::cout << "AVERROR(ENOMEM) on decode!" << std::endl;
			break;
		}
		default: {
			failure = true;
			break;
		}

		}

		av_packet_unref(&packet);
	}
	
	if (!failure) {
		return 0;
	}
	else {
		return -1;
	}
}

int malloc_media_frame(MediaFrame* frame) {
	frame->video_frame = av_frame_alloc();
	frame->audio_frame = av_frame_alloc();

	frame->t_current_packet = av_packet_alloc();
	if (!frame->video_frame || !frame->t_current_packet || !frame->audio_frame) {
		media_error_submit("Frame Could not be allocated!", __FILE__, MEDIA_ERROR_CRITICAL, __LINE__, __FUNCTION__);
		return -1;
	}
	return 0;
}

int malloc_media_packet(MediaPacket* packet) {
	packet->packet = av_packet_alloc();
	if (!packet) {
		media_error_submit("Packet Could not be allocated!", __FILE__, MEDIA_ERROR_CRITICAL, __LINE__, __FUNCTION__);
		return -1;
	}

	return 0;
}

void free_media_frame(MediaFrame* frame) {
	//av_packet_free(&frame->t_current_packet);
	av_frame_free(&frame->video_frame);
	av_frame_free(&frame->audio_frame);
}

void free_media_packet(MediaPacket* packet) {
	av_packet_free(&packet->packet);
}

static int decode_video_packet(MediaCodecDescriptor& codec, MediaFrame* frame)
{
	int send_resp = -1;
	int response = -1;
	send_resp = avcodec_send_packet(codec.video_codec_context, frame->t_current_packet);
	if (send_resp == 0)
	{
		response = avcodec_receive_frame(codec.video_codec_context, frame->video_frame);
		return response;
	}

	return send_resp;
}

static int decode_audio_packet(MediaCodecDescriptor& codec, MediaFrame* frame)
{
	int response = -1;
	int send_resp = -1;
	send_resp = avcodec_send_packet(codec.audio_codec_context, frame->t_current_packet);
	if (send_resp == 0)
	{
		response = avcodec_receive_frame(codec.audio_codec_context, frame->audio_frame);
		return response;
	}

	return send_resp;
}

/*
I think the part when you "get to correct timestamp to discard prior frames up to desired seconds" is incorrect.
As I understand it, we can't just ignore the packed: av_packet_unref(pPacket);.
We have to decode all the packets up to the correct frame (decode and then ignore),
because after each key frame, there is a "frame dependency chain" up to the next key frames
(there are P-frames and B-frames...).

okay so its simple just decoe every packet, DONT SKIP ANY PACKET DECODE, and simply use frame number to get the right
frame data at the right time!


So essentially I was getting th issue why? because my packets werent getting decoded sequentially,
the audio pacets in the middle were proving to be a problem as they broke contiuinty and thus B Frames and P frames were
messed up. Now seperating both streams makes things easier

Very inefficinet as im essentially doing two while loops, make another funtion that takes in a function ptr which will give it
instructions on how to deal with caputred frame (so only while loop)
*/

int decode_next_frame_video(MediaContainer* media, MediaFrame* frame) {
	bool video_frame_recorded = false;

	bool failure = false;

	//Read Frame gives -ve if eof or error, splits stream into packets only.
	while (!video_frame_recorded && !failure) {

		if (av_read_frame(media->format_context, frame->t_current_packet) >= 0) {

			if (video_frame_recorded == false && frame->t_current_packet->stream_index == media->m_video_stream_index) {
				int r = decode_video_packet(media->codec_description, frame);
				switch (r) {
				case 0: {
					video_frame_recorded = true;
					//std::cout << "video frame recorded" << std::endl;
					break;
				}
				case AVERROR(EAGAIN): {
					//std::cout << "AVERROR(EAGAIN) on decode!" << std::endl;
					break;
				}
				case AVERROR(EINVAL): {
					//std::cout << "AVERROR(EINVAL) on decode!" << std::endl;
					break;
				}
				case AVERROR_EOF: {
					//std::cout << "AVERROR_EOF on decode! (End of File reached, terminating)" << std::endl;
					break;
				}
				case AVERROR(ENOMEM): {
					//std::cout << "AVERROR(ENOMEM) on decode!" << std::endl;
					break;
				}
				default: {
					failure = true;
					break;
				}

				}
			}
			av_packet_unref(frame->t_current_packet);
		}
		else {
			failure = true;
		}
	}

	if (!failure) {
		frame->frame_pts = frame->video_frame->pts;
		retrieve_pts_seconds(media, frame);
		return 0;
	}
	else {
		return -1;
	}
}

int decode_next_frame_audio(MediaContainer* media, MediaFrame* frame) {
	bool audio_frame_recorded = false;

	bool failure = false;

	//Read Frame gives -ve if eof or error, splits stream into packets only.
	while (!audio_frame_recorded && !failure) {

		if (av_read_frame(media->format_context, frame->t_current_packet) >= 0) {

			if (audio_frame_recorded == false && frame->t_current_packet->stream_index == media->m_audio_stream_index) {
				int r = decode_audio_packet(media->codec_description, frame);
				switch (r) {
				case 0: {
					audio_frame_recorded = true;
					//std::cout << "audio frame recorded" << std::endl;
					break;
				}
				case AVERROR(EAGAIN): {
					//std::cout << "AVERROR(EAGAIN) on decode!" << std::endl;
					break;
				}
				case AVERROR(EINVAL): {
					//std::cout << "AVERROR(EINVAL) on decode!" << std::endl;
					break;
				}
				case AVERROR_EOF: {
					//std::cout << "AVERROR_EOF on decode! (End of File reached, terminating)" << std::endl;
					break;
				}
				case AVERROR(ENOMEM): {
					//std::cout << "AVERROR(ENOMEM) on decode!" << std::endl;
					break;
				}
				default: {
					failure = true;
					break;
				}

				}
			}

			av_packet_unref(frame->t_current_packet);
		}
		else {
			failure = true;
		}
	}

	if (!failure) {
		frame->frame_pts = frame->video_frame->pts;
		retrieve_pts_seconds(media, frame);
		return 0;
	}
	else {
		return -1;
	}
}

int encode_next_frame_video(MediaContainer* media, MediaFrame* frame, MediaPacket* packet, MediaRational time_from, MediaRational time_to) {
	//Resize frame to output container specifications

	//SwsContext* swsContext = sws_getContext(
	//	frame->video_frame->width, frame->video_frame->height, static_cast<AVPixelFormat>(frame->video_frame->format),
	//	media->m_width, media->m_height, AV_PIX_FMT_YUV420P, // Specify the desired output dimensions and format
	//	SWS_BILINEAR, nullptr, nullptr, nullptr);

	//// Allocate memory for the resized frame
	//AVFrame* resizedFrame = av_frame_alloc();
	//resizedFrame->width = media->m_width;
	//resizedFrame->height = media->m_height;
	//resizedFrame->format = AV_PIX_FMT_YUV420P;
	//av_frame_get_buffer(resizedFrame, 0);
	//
	//// Perform the resizing
	//sws_scale(swsContext,
	//	frame->video_frame->data, frame->video_frame->linesize,
	//	0, frame->video_frame->height,
	//	resizedFrame->data, resizedFrame->linesize);

	//// Clean up the scaling context
	//sws_freeContext(swsContext);

	////Swap frames
	//resizedFrame->pts = frame->video_frame->pts;
	//frame->video_frame = (resizedFrame);

	bool video_frame_encoded = false;
	bool failure = false;
	AVPacket* output_current_packet = av_packet_alloc();
	int response = avcodec_send_frame(media->codec_description.video_codec_context, frame->video_frame);
	if (response >= 0) {
		response = avcodec_receive_packet(media->codec_description.video_codec_context, output_current_packet);
		switch (response) {
		case 0: {
			video_frame_encoded = true;
			break;
		}
		case AVERROR(EAGAIN): {
			//More frames needed.
			video_frame_encoded = true;
			failure = true;
			break;
		}
		case AVERROR(EOF): {
			failure = true;
			break;
		}
		default: {
			failure = true;
			break;
		}

		}
	}
	else {
		return -1;
	}

	if (failure == true && video_frame_encoded == true) {
		return 1;
	}
	
	if (failure == true) {
		return -1;
	}

	if (failure == false) {
		packet->packet = output_current_packet;
		output_current_packet->stream_index = media->m_video_stream_index;

		av_packet_rescale_ts(packet->packet, time_from, time_to);
		//std::cout << "video" << packet->packet->pts << std::endl;
		return 0;
	}

}

int encode_next_frame_audio(MediaContainer* media, MediaFrame* frame, MediaPacket* packet, MediaRational time_from, MediaRational time_to) {
	bool audio_frame_recorded = false;
	bool failure = false;
	AVPacket* output_current_packet = av_packet_alloc();
	int response = avcodec_send_frame(media->codec_description.audio_codec_context, frame->audio_frame);
	if (response >= 0) {
		response = avcodec_receive_packet(media->codec_description.audio_codec_context, output_current_packet);
		switch (response) {
		case 0: {
			audio_frame_recorded = true;
			break;
		}
		case AVERROR(EAGAIN): {
			//More frames needed.
			audio_frame_recorded = true;
			failure = true;
			break;
		}
		case AVERROR(EOF): {
			failure = true;
			break;
		}
		default: {
			failure = true;
			break;
		}

		}
	}
	else {
		return -1;
	}

	if (failure == true && audio_frame_recorded == true) {
		return 1;
	}

	if (failure == true) {
		return -1;
	}

	if (failure == false) {
		packet->packet = output_current_packet;
		output_current_packet->stream_index = media->m_audio_stream_index;
		av_packet_rescale_ts(packet->packet, time_from, time_to);
		return 0;
	}

}



void retrieve_pts_seconds(MediaContainer* media, MediaFrame* frame) {
	frame->frame_pts_seconds = frame->frame_pts * (double)media->time_base.num / (double)media->time_base.den;
}


void media_stream_submit_packet(MediaStreamContainer* media, const std::vector<uint8_t>& packet) {
	media->rtp_packet_stack_buffer.push_back(packet);
}

static int media_stream_request_packet(MediaStreamContainer* media, std::vector<uint8_t>& packet) {
	if (media->rtp_packet_stack_buffer.empty()) {
		return -1;
	}
	packet = media->rtp_packet_stack_buffer.front();
	media->rtp_packet_stack_buffer.pop_front();

	/*
	for (std::vector<uint8_t> i : media->packet_stack_buffer) {
		std::cout <<"PACKET DATA: " << uint8_vector_to_hex_string(i) << std::endl;
	}
	*/

	return 0;
}

static void media_stream_convert_av(AVPacket* av_packet, std::vector<uint8_t>& packet) {
	av_packet->data = packet.data();
	av_packet->size = sizeof(uint8_t) * packet.size();
}

int malloc_media_stream_container(MediaStreamContainer* media, int width, int height) {
	media->stream_width = width;
	media->stream_height = height;

	media->codec_description.video_codec = avcodec_find_decoder(MEDIA_STREAM_VIDEO_CODEC);
	media->codec_description.audio_codec = avcodec_find_decoder(MEDIA_STREAM_AUDIO_CODEC);
	media->codec_description.video_codec_context = avcodec_alloc_context3(media->codec_description.video_codec);

	media->codec_description.video_codec_context->pix_fmt = AV_PIX_FMT_ARGB;
	media->codec_description.video_codec_context->height = media->stream_height;
	media->codec_description.video_codec_context->width = media->stream_width;

	//Temporary flags, could be removed after testing, not sure if these reduce artifacting during slow interent or not.
	media->codec_description.video_codec_context->workaround_bugs = AV_EF_EXPLODE | AV_EF_AGGRESSIVE;
	if (avcodec_open2(media->codec_description.video_codec_context, media->codec_description.video_codec, NULL) < 0)
	{
		media_error_submit("Media Stream Error: Video codec context could not be created!", __FILE__, MEDIA_ERROR_CRITICAL, __LINE__, __FUNCTION__);
		return -1;
	}

	media->codec_description.audio_codec_context = avcodec_alloc_context3(media->codec_description.audio_codec);

	if (avcodec_open2(media->codec_description.audio_codec_context, media->codec_description.audio_codec, NULL) < 0)
	{
		media_error_submit("Media Stream Error: Audio codec context could not be created!", __FILE__, MEDIA_ERROR_CRITICAL, __LINE__, __FUNCTION__);
		return -1;
	}

	return 0;
}

void free_media_stream_container(MediaStreamContainer* media) {
	avcodec_close(media->codec_description.video_codec_context);
	avcodec_close(media->codec_description.audio_codec_context);
}

int decode_next_frame_video(MediaStreamContainer* media, MediaFrame* frame) {
	bool failure = false;
	std::vector<uint8_t> packet_unint;
	AVPacket packet_av;
	av_init_packet(&packet_av);

	//Unlike video files, every frame packet built and put in queue (should) and must be a complete frame.
	//No need for while to get more packets from queue

	if (media_stream_request_packet(media, packet_unint) >= 0) {
		if (packet_unint.size() == 0) {
			//Drop empty packet, or we can get an EOF decoder error.
			av_packet_unref(&packet_av);
			failure = true;
		}
		media_stream_convert_av(&packet_av, packet_unint);
		frame->t_current_packet = &packet_av;
		int r = decode_video_packet(media->codec_description, frame);

		switch (r) {
		case 0: {
			//Success!
			break;
		}
		case AVERROR(EAGAIN): {
			//std::cout << "AVERROR(EAGAIN) on decode!" << std::endl;
			break;
		}
		case AVERROR(EINVAL): {
			//std::cout << "AVERROR(EINVAL) on decode!" << std::endl;
			break;
		}
		case AVERROR_EOF: {
			//std::cout << "AVERROR_EOF on decode!" << std::endl;
			avcodec_flush_buffers(media->codec_description.video_codec_context);
			break;
		}
		case AVERROR(ENOMEM): {
			//std::cout << "AVERROR(ENOMEM) on decode!" << std::endl;
			break;
		}
		default: {
			failure = true;
			break;
		}

		}

		av_packet_unref(&packet_av);
	}
	else {
		failure = true;
	}

	if (!failure) {
		return 0;
	}
	else {
		return -1;
	}
	/*
	* ERROR REFERENCE:
	* concealing errors / no frame - Packet loss, incorrect packet ordering. Poor connection.
	* cannot find ref at POC - H265 specific, packet loss, incorrect ordering. Poor connection.
	* stream stops / EOF - null packet input into deocoder, it assumes eof and cleans up, remedy? av_codec_flush_decoder when EOF encountered.
	* no frame - incomplete packet submitted, associated with poor connection, can also happen if data handling produces random data.
	*/
}

int decode_next_frame_audio(MediaStreamContainer* media, MediaFrame* frame) {

	bool failure = false;
	std::vector<uint8_t> packet_unint;
	AVPacket packet_av;
	av_init_packet(&packet_av);

	//Unlike video files, every frame packet built and put in queue (should) and must be a complete frame.
	//No need for while to get more packets from queue

	if (media_stream_request_packet(media, packet_unint) >= 0) {
		if (packet_unint.size() == 0) {
			//Drop empty packet, or we can get an EOF decoder error.
			av_packet_unref(&packet_av);
			failure = true;
		}
		media_stream_convert_av(&packet_av, packet_unint);
		frame->t_current_packet = &packet_av;
		int r = decode_audio_packet(media->codec_description, frame);

		switch (r) {
		case 0: {
			//Success!
			break;
		}
		case AVERROR(EAGAIN): {
			//std::cout << "AVERROR(EAGAIN) on decode!" << std::endl;
			break;
		}
		case AVERROR(EINVAL): {
			//std::cout << "AVERROR(EINVAL) on decode!" << std::endl;
			break;
		}
		case AVERROR_EOF: {
			//std::cout << "AVERROR_EOF on decode!" << std::endl;
			avcodec_flush_buffers(media->codec_description.audio_codec_context);
			break;
		}
		case AVERROR(ENOMEM): {
			//std::cout << "AVERROR(ENOMEM) on decode!" << std::endl;
			break;
		}
		default: {
			failure = true;
			break;
		}

		}

		av_packet_unref(&packet_av);
	}
	else {
		failure = true;
	}

	if (!failure) {
		return 0;
	}
	else {
		return -1;
	}
}

void malloc_media_file_stream_container(MediaFileStreamingBuffer* media, float timebase_num, float timebase_den, int fps) {
	media->last_frame_pts_video = 0;
	media->last_frame_pts_audio = 0;
	media->time_base = av_make_q(timebase_num, timebase_den);
	media->m_fps = fps;
}
void free_media_file_stream_container(MediaFileStreamingBuffer* media) {
	for (auto i : media->stack_buffer_video) {
		av_packet_free(&i.packet);
	}

	for (auto i : media->stack_buffer_audio) {
		av_packet_free(&i.packet);
	}
}

static float media_file_stream_gen_pts_video(MediaFileStreamingBuffer* media) {
	if (media->last_frame_pts_video == 0 ) {
		float pts_inc_this_frame = 0.0f;
		pts_inc_this_frame = media->time_base.den / media->m_fps;
		return pts_inc_this_frame;
	}
	else {
		float pts_inc_this_frame = 0.0f;
		pts_inc_this_frame = media->time_base.den / media->m_fps;

		return media->last_frame_pts_video + pts_inc_this_frame;
	}
}

static float media_file_stream_gen_pts_audio(MediaFileStreamingBuffer* media) {
	if (media->last_frame_pts_audio == 0) {
		float pts_inc_this_frame = 0.0f;
		pts_inc_this_frame = media->time_base.den / media->m_fps;
		return pts_inc_this_frame;
	}
	else {
		float pts_inc_this_frame = 0.0f;
		pts_inc_this_frame = media->time_base.den / media->m_fps;

		return media->last_frame_pts_audio + pts_inc_this_frame;
	}
}

void media_submit_file_stream_packet_video(MediaFileStreamingBuffer* media, MediaPacket packet) {
	packet.pts = media_file_stream_gen_pts_video(media);
	media->last_frame_pts_video = packet.pts;
	media->stack_buffer_video.push_back(packet);
}

void media_submit_file_stream_packet_audio(MediaFileStreamingBuffer* media, MediaPacket packet) {
	packet.pts = media_file_stream_gen_pts_audio(media);
	media->last_frame_pts_audio = packet.pts;
	media->stack_buffer_audio.push_back(packet);
}

int media_request_file_stream_packet_video(MediaFileStreamingBuffer* media, MediaPacket& packet) {
	if (media->stack_buffer_video.empty()) {
		return -1;
	}
	packet = media->stack_buffer_video.front();
	packet.packet->pts = packet.pts;
	//packet.packet->dts = packet.pts;
	media->stack_buffer_video.pop_front();

	return 0;
}

int media_request_file_stream_packet_audio(MediaFileStreamingBuffer* media, MediaPacket& packet) {
	if (media->stack_buffer_audio.empty()) {
		return -1;
	}
	packet = media->stack_buffer_audio.front();
	packet.packet->pts = packet.pts;
	//packet.packet->dts = packet.pts;
	media->stack_buffer_audio.pop_front();

	return 0;
}