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

        char text[500];
        ELOG_DEBUG("Create graph");
        snprintf(args, sizeof(args),
                 "width=320:height=240:pix_fmt=yuv420p:time_base=1/20:sar=1");

        int error = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                     args, NULL, filter_graph);
        av_strerror(error,text,500);
        ELOG_DEBUG("Created source %s",text);

        snprintf(args, sizeof(args),
                 "out_w=in_w-100:out_h=in_h-100:x=100:y=100");
        error = avfilter_graph_create_filter(&crop_ctx, crop, "crop",
                                     args, NULL, filter_graph);
        av_strerror(error,text,500);
        ELOG_DEBUG("Created crop?: %s",text);

        error = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                     NULL, NULL, filter_graph);


        av_strerror(error,text,500);
        ELOG_DEBUG("Created sink?: %s",text);
        ELOG_DEBUG("Created graph");

        error = avfilter_link(buffersrc_ctx, 0, crop_ctx, 0);
        av_strerror(error,text,500);
        ELOG_DEBUG("Connected buffer to crop?: %s",text);
        error = avfilter_link(crop_ctx, 0, buffersink_ctx, 0);
        av_strerror(error,text,500);
        ELOG_DEBUG("CConnected crop to sink?: %s",text);


        error = avfilter_graph_config(filter_graph, NULL);
        av_strerror(error,text,500);
        ELOG_DEBUG("Config graph?: %s",text);

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
        dump = fopen("dump.txt", "w+");
        dump2 = fopen("dump2.txt", "w+");
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
            ELOG_DEBUG("Packet size %d",packet->length);
            dpckg->fetchPacket(reinterpret_cast<unsigned char*>(packet->data), packet->length);
            last_frame = dpckg->processPacket();
            if(last_frame) {
                ELOG_DEBUG("Add Packet");
                ELOG_DEBUG("Size %d",dpckg->frameSize());
                if(vDecoder.decodeVideo(dpckg->frame(), dpckg->frameSize(), outBuff.get(), outBuffLen, &gotFrame)>0){
                    ELOG_DEBUG("Decoded");
                    int error;
                    char text[500];
                    ELOG_DEBUG("Feed Frame");
                    frame = vDecoder.returnAVFrame();
                    //display_frame(frame,1);
                    frame->pts = packet->received_time_ms;
                    ELOG_DEBUG("Pts %d",frame->pts);
                    ELOG_DEBUG("Widht %d",frame->width);
                    ELOG_DEBUG("Height %d",frame->height);
                    ELOG_DEBUG("format %d",frame->format);
                    ELOG_DEBUG("keyframe %d",frame->key_frame);
                    error = av_buffersrc_write_frame(buffersrc_ctx, frame);
                    av_strerror(error, text, 500);
                    ELOG_DEBUG("addded frame %s", text);
                    ELOG_DEBUG("Process frame");
                    av_buffersink_get_frame(buffersink_ctx, filt_frame);
                    ELOG_DEBUG("Height %d",filt_frame->height);
                    //display_frame(filt_frame,2);
                    ELOG_DEBUG("Reset");

                }else{
                    ELOG_DEBUG("Need more packets to decode");
                };
                dpckg->reset();
            }
        }
        ctx->fireRead(std::move(packet));
    }

    void CropFilter::write(Context *ctx, std::shared_ptr <DataPacket> packet) {
        ctx->fireWrite(std::move(packet));
    }

    void CropFilter::display_frame(const AVFrame *frame, int fileN)
    {
        int x, y;
        uint8_t *p0, *p;
        /* Trivial ASCII grayscale display. */
        p0 = frame->data[0];
        std::FILE *file = fileN == 1 ? dump: dump2;
        fputs("\033c",file);
        for (y = 0; y < frame->height; y++) {
            p = p0;
            for (x = 0; x < frame->width; x++)
                putc(" .-+#"[*(p++) / 52],file);
            putc('\n',file);
            p0 += frame->linesize[0];
        }
        fflush(stdout);
    }




}