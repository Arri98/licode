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
        VideoCodecID vDecoderID = VIDEO_CODEC_VP8;
        VideoCodecInfo vDecodeInfo = {
                vDecoderID,
                100,
                320,
                240,
                1000000,
                20
        };
        ELOG_DEBUG("Init Decoder");
        vDecoder.initDecoder(vDecodeInfo);
        outBuff.reset((unsigned char*) malloc(10000000));
        dpckg = new Vp8Depacketizer();
    }

    CropFilter::~CropFilter() {
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
        avfilter_graph_free(&filter_graph);
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
                dpckg->reset();
                ELOG_DEBUG("Add Packet");
                vDecoder.decodeVideo(dpckg->frame(), dpckg->frameSize(), outBuff.get(), outBuffLen, &gotFrame);
                ELOG_DEBUG("Decoded");
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
}