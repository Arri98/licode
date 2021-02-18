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
        ELOG_DEBUG("Create resize");
        const AVFilter *resize  = avfilter_get_by_name("scale");

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

        snprintf(args, sizeof(args),
                 "w=320:h=240");
        error = avfilter_graph_create_filter(&resize_ctx, resize, "scale",
                                     args, NULL, filter_graph);
        av_strerror(error,text,500);
        ELOG_DEBUG("Created resize?: %s",text);

        error = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                     NULL, NULL, filter_graph);

        av_strerror(error,text,500);
        ELOG_DEBUG("Created sink?: %s",text);
        ELOG_DEBUG("Created graph");

        error = avfilter_link(buffersrc_ctx, 0, crop_ctx, 0);
        av_strerror(error,text,500);
        ELOG_DEBUG("Connected buffer to crop?: %s",text);

        error = avfilter_link(crop_ctx, 0, resize_ctx, 0);
        av_strerror(error,text,500);
        ELOG_DEBUG("Connected crop to resize?: %s",text);

        error = avfilter_link(resize_ctx, 0, buffersink_ctx, 0);
        av_strerror(error,text,500);
        ELOG_DEBUG("Connected resize to sink?: %s",text);


        error = avfilter_graph_config(filter_graph, NULL);
        av_strerror(error,text,500);
        ELOG_DEBUG("Config graph?: %s",text);

        frame = av_frame_alloc();
        filt_frame = av_frame_alloc();

        av_init_packet(&av_packet);
        VideoCodecID vDecoderID = VIDEO_CODEC_VP8;
        vDecodeInfo = {
                vDecoderID,
                96,
                320,
                240,
                1000000,
                20
        };
        VideoCodecID vEncoderID = VIDEO_CODEC_VP8;
        vEncodeInfo = {
                vEncoderID,
                96,
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

        encodeFrameBuff = (unsigned char*) malloc(100000);
        seqnum_ = 0;
        fragmenterBuffer = (unsigned char*) malloc(2000);
        rtpBuffer_ = (unsigned char*) malloc(4000);
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
        if(packet->type == VIDEO_PACKET && packet->length >30){
            ELOG_DEBUG("lenght %d",packet->length);
            dpckg->fetchPacket(reinterpret_cast<unsigned char*>(packet->data), packet->length);
            last_frame = dpckg->processPacket();
            if(last_frame) {
                int decodeL = vDecoder.decodeVideo(dpckg->frame(), dpckg->frameSize(), outBuff.get(), outBuffLen, &gotFrame);
                if(decodeL>0){
                    int error;
                    char text[500];
                    std::shared_ptr<DataPacket> copied_packet = std::make_shared<DataPacket>(*packet);
                    RtpHeader *copy_head = reinterpret_cast<RtpHeader*> (copied_packet->data);
                    frame = vDecoder.returnAVFrame();
                    display_frame(frame,1);
                    frame->pts = copy_head->getTimestamp();
                    error = av_buffersrc_write_frame(buffersrc_ctx, frame);
                    av_strerror(error, text, 500);
                    av_buffersink_get_frame(buffersink_ctx, filt_frame);
                    display_frame(filt_frame,2);

                    if(!encoderInit){
                        filtFrameLenght = avpicture_get_size(AV_PIX_FMT_YUV420P,320,240);
                        vEncoder.initEncoder(vEncodeInfo);
                        encoderInit = true;
                    }
                    int l = vEncoder.encodeVideo((unsigned char*)filt_frame->data, filtFrameLenght, encodeFrameBuff, encodeFrameBuffLen);
                    RtpVP8Fragmenter frag(encodeFrameBuff, l);
                    dpckg->reset();
                    lastFragPacket = false;
                    while(!lastFragPacket){
                        lengthFrag=0;
                        frag.getPacket(fragmenterBuffer, &lengthFrag,&lastFragPacket);
                        RtpHeader rtpHeader;
                        if(firstPackage){
                            seqnum_ = copy_head->getSeqNumber();
                            //ELOG_DEBUG("Get seqnumber %d",seqnum_);
                            firstPackage = false;
                        }
                        rtpHeader.setSeqNumber(seqnum_++);
                        rtpHeader.setPayloadType(96);
                        rtpHeader.setSSRC(copy_head->getSSRC());
                        rtpHeader.setTimestamp(copy_head->getTimestamp());
                        //rtpHeader.setExtension(copy_head->getExtension());
                        //rtpHeader.setExtId(copy_head->getExtId());
                        //rtpHeader.setExtLength(copy_head->getExtLength());
                        rtpHeader.setMarker(lastFragPacket?1:0);
                        int len = lengthFrag+rtpHeader.getHeaderLength();

                        ELOG_DEBUG("Cpy data");
                        memcpy(rtpBuffer_, &rtpHeader, rtpHeader.getHeaderLength());
                        memcpy(&rtpBuffer_[rtpHeader.getHeaderLength()],fragmenterBuffer,lengthFrag);

                        std::shared_ptr<DataPacket> exit_packet = std::make_shared<DataPacket>(0, reinterpret_cast<char*>(rtpBuffer_),
                                                                                          len, VIDEO_PACKET, copied_packet->received_time_ms);

                        RtpHeader *exit_head = reinterpret_cast<RtpHeader*> (exit_packet->data);
                        ELOG_DEBUG("Seq number %d",exit_head->getSeqNumber());
                        //ELOG_DEBUG("Fragment n %d",exit_packet->lastFragPacket?1:0));
                        ELOG_DEBUG("Length %d",lengthFrag);
                        ELOG_DEBUG("Header length %d",rtpHeader.getHeaderLength());
                        ELOG_DEBUG("PTS %d",exit_head->getTimestamp());
                        ELOG_DEBUG("SSRC %d",exit_head->getSSRC());
                        //ELOG_DEBUG("Keyfrmae %d",exit_packet->is_keyframe );
                        //ELOG_DEBUG("Going to send");
                        ctx->fireRead(std::move(exit_packet));
                        //ELOG_DEBUG("Sended");

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