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

        encoderInit = false;

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
        vDecodeInfo = {
                vDecoderID,
                100,
                320,
                240,
                1000000,
                20
        };
        VideoCodecID vEncoderID = VIDEO_CODEC_VP8;
        vEncodeInfo = {
                vEncoderID,
                100,
                0,
                0,
                1000000,
                20
        };


        ELOG_DEBUG("Init Decoder");
        vDecoder.initDecoder(vDecodeInfo);
        outBuff.reset((unsigned char*) malloc(10000000));
        dpckg = new Vp8Depacketizer();
        dump = fopen("dump.txt", "w+");
        dump2 = fopen("dump2.txt", "w+");

        encodeFrameBuff = (unsigned char*) malloc(100000);
        seqnum_ = 0;
        fragmenterBuffer = (unsigned char*) malloc(2000);
        lengthFrag = 0;
        firstPackage = true;
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
        if(packet->type == VIDEO_PACKET && packet->length >100){
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

                    if(!encoderInit){
                        vEncodeInfo.width = filt_frame->width;
                        ELOG_DEBUG("Filt frame width %d",filt_frame->width);
                        vEncodeInfo.height = filt_frame->height;
                        ELOG_DEBUG("Filt frame height %d",filt_frame->height);
                        filtFrameLenght = avpicture_get_size(AV_PIX_FMT_YUV420P,filt_frame->width,filt_frame->height);
                        ELOG_DEBUG("Filt frame lenght %d",filtFrameLenght);
                        vEncoder.initEncoder(vEncodeInfo);
                        encoderInit = true;
                    }
                    ELOG_DEBUG("Encode");
                    int l = vEncoder.encodeVideo((unsigned char*)filt_frame->data, filtFrameLenght, encodeFrameBuff, encodeFrameBuffLen);
                    ELOG_DEBUG("Fragmenter");
                    ELOG_DEBUG("Decoded length %d",l);
                    RtpVP8Fragmenter frag(encodeFrameBuff, l);
                    dpckg->reset();
                    lastFragPacket = false;
                    while(!lastFragPacket){
                        lengthFrag=0;
                        DataPacket p = *packet;
                        frag.getPacket(fragmenterBuffer, &lengthFrag,&lastFragPacket);
                        rtpHeader = reinterpret_cast< RtpHeader*>(p.data);
                        ELOG_DEBUG("Cpy data");
                        memcpy(p.data + rtpHeader->getHeaderLength(),fragmenterBuffer,lengthFrag);
                        if(firstPackage){
                            seqnum_ = rtpHeader->getSeqNumber();
                            ELOG_DEBUG("Get number %d",seqnum_);
                            firstPackage = false;
                        }
                        rtpHeader->setSeqNumber(seqnum_++);
                        p.length = lengthFrag+rtpHeader->getHeaderLength();
                        p.is_keyframe = filt_frame->key_frame;
                        ELOG_DEBUG("Seq number %d",seqnum_);
                        ELOG_DEBUG("Fragment n %d",lastFragPacket?1:0);
                        ELOG_DEBUG("Length %d",lengthFrag);
                        ELOG_DEBUG("PTS %d",p.received_time_ms);
                        //ELOG_DEBUG("Going to send");
                        ctx->fireRead(std::move(std::make_shared<DataPacket>(p)));
                        ELOG_DEBUG("Sended");

                    }
                }else{
                    ELOG_DEBUG("Need more packets to decode");
                };
            }
        }else{
            ctx->fireRead(std::move(packet));
        }
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