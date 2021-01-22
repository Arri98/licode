//
// Created by lic on 11/1/21.
//

#include "CropFilter.h"

namespace erizo {

    DEFINE_LOGGER(CropFilter, "rtp.CropFilter");

    CropFilter::CropFilter(std::vector <std::string> parameters) : parameters{parameters} {
        ELOG_DEBUG("Created");
        avfilter_register_all();
        avcodec_register_all();
        av_register_all();

        ELOG_DEBUG("Alloc Graph");
        filter_graph = avfilter_graph_alloc();

        ELOG_DEBUG("Create buffersrc");
        const AVFilter *buffersrc  = avfilter_get_by_name("buffer");
        ELOG_DEBUG("Create buffersink");
        const AVFilter *buffersink = avfilter_get_by_name("buffersink");
        ELOG_DEBUG("Create crop");
        const AVFilter  *crop  = avfilter_get_by_name("crop");

        ELOG_DEBUG("Alloc in/out");
        AVFilterInOut *outputs = avfilter_inout_alloc();
        AVFilterInOut *inputs  = avfilter_inout_alloc();

        char text[500];
        ELOG_DEBUG("Create graph");
        snprintf(args, sizeof(args),
                 "width=320:height=240:pix_fmt=yuv420p:time_base=1/20:sar=1");

        int error = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                     args, NULL, filter_graph);
        av_strerror(error,text,500);
        ELOG_DEBUG("Created source %s",text);

        snprintf(args, sizeof(args),
                 "x=%d:y=%d",
                 200, 200);
        error = avfilter_graph_create_filter(&crop_ctx, crop, "crop",
                                     args, NULL, filter_graph);
        av_strerror(error,text,500);
        ELOG_DEBUG("Created crop?: %s",text);
        error = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                     NULL, NULL, filter_graph);
        av_strerror(error,text,500);
        ELOG_DEBUG("Created sink?: %s",text);
        ELOG_DEBUG("Created graph");

        outputs->name       = av_strdup("in");
        outputs->filter_ctx = buffersrc_ctx;
        outputs->pad_idx    = 0;
        outputs->next       = NULL;

        inputs->name       = av_strdup("out");
        inputs->filter_ctx = buffersink_ctx;
        inputs->pad_idx    = 0;
        inputs->next       = NULL;

        frame = av_frame_alloc();
        filt_frame = av_frame_alloc();

        av_init_packet(&av_packet);
        codec = avcodec_find_decoder(AV_CODEC_ID_VP8 );
        parser = av_parser_init(codec->id);
        c = avcodec_alloc_context3(codec);
        context_ = avformat_alloc_context();
        dpckg = new Vp8Depacketizer();
    }

    CropFilter::~CropFilter() {
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
        avfilter_graph_free(&filter_graph);
        av_frame_free(&frame);
        av_frame_free(&filt_frame);
    }

    void CropFilter::enable() {
    }

    void CropFilter::disable() {
    }

    void CropFilter::notifyUpdate() {

    }

    int CropFilter::position() {
        return 1;
    }

    void CropFilter::read(Context *ctx, std::shared_ptr <DataPacket> packet) {
        if(packet->type == VIDEO_PACKET){
            ELOG_DEBUG("Received video packet");
            dpckg->fetchPacket(reinterpret_cast<unsigned char*>(packet->data), packet->length);
            ELOG_DEBUG("Fetched pckg video packet");

            last_frame = dpckg->processPacket();
            if(last_frame) {
                ELOG_DEBUG("Last frame");
                //Create AVPackage
                av_packet.data = dpckg->frame();
                av_packet.size = dpckg->frameSize();
                av_packet.stream_index = 0;
                av_interleaved_write_frame(context_, &av_packet);   // takes ownership of the packet
                dpckg->reset();
                ELOG_DEBUG("Add Packet");
                int* gotFrame=0;
                avcodec_decode_video2(c,frame,gotFrame, &av_packet);
                ELOG_DEBUG("Got frame %d", gotFrame);
                ELOG_DEBUG("Receive frame");
                ELOG_DEBUG("Add frame");
                int error;
                char text[500];
                error = av_buffersrc_write_frame(buffersrc_ctx, frame);
                av_strerror(error, text, 500);
                ELOG_DEBUG("addded frame %s", text);
                ELOG_DEBUG("Process frame");
                av_buffersink_get_frame(buffersink_ctx, filt_frame);
                ELOG_DEBUG("Reset");
                dpckg->reset();
            }
        }
        ctx->fireRead(std::move(packet));
    }

    void CropFilter::write(Context *ctx, std::shared_ptr <DataPacket> packet) {
        ctx->fireWrite(std::move(packet));
    }
  /*
    void PacketToAVPackage(char* buf, int len){
        RtpHeader* head = reinterpret_cast<RtpHeader*>(buf);
        depacketizer_->fetchPacket((unsigned char*)buf, len);
        bool deliver = depacketizer_->processPacket();

        initContext();
        if (video_stream_ == nullptr) {
            // could not init our context yet.
            return;
        }

        if (deliver) {
            long long current_timestamp = head->getTimestamp();  // NOLINT
            if (current_timestamp - first_video_timestamp_ < 0) {
                // we wrapped.  add 2^32 to correct this.
                // We only handle a single wrap around since that's ~13 hours of recording, minimum.
                current_timestamp += 0xFFFFFFFF;
            }

            // All of our video offerings are using a 90khz clock.
            long long timestamp_to_write = (current_timestamp - first_video_timestamp_) /  // NOLINT
                                           (video_map_.clock_rate / video_stream_->time_base.den);

            // Adjust for our start time offset

            // in practice, our timebase den is 1000, so this operation is a no-op.
            timestamp_to_write += video_offset_ms_ / (1000 / video_stream_->time_base.den);

            AVPacket av_packet;
            av_init_packet(&av_packet);
            av_packet.data = depacketizer_->frame();
            av_packet.size = depacketizer_->frameSize();
            av_packet.pts = timestamp_to_write;
            av_packet.stream_index = 0;
            av_interleaved_write_frame(context_, &av_packet);   // takes ownership of the packet
            depacketizer_->reset();
        }
    }
    */
}