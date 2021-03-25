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


        //Create graph filters
        ELOG_DEBUG("Create buffersrc");
        buffersrc  = avfilter_get_by_name("buffer");
        ELOG_DEBUG("Create buffersink");
        buffersink = avfilter_get_by_name("buffersink");
        ELOG_DEBUG("Create crop");
        crop  = avfilter_get_by_name("crop");
        ELOG_DEBUG("Create resize");
        resize  = avfilter_get_by_name("scale");
        configureFilters(320, 240);
        //Init frames used
        frame = av_frame_alloc();
        filt_frame = av_frame_alloc();

        //Encoder and decoder info
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
                0,
                0,
                1000000,
                20
        };

        //Init files and arrays
        ELOG_DEBUG("Init Decoder");
        vDecoder.initDecoder(vDecodeInfo);
        outBuff.reset((unsigned char*) malloc(10000000));
        dpckg = new Vp8Depacketizer();
        //dump = fopen("dump.txt", "w+");
        //dump2 = fopen("dump2.txt", "w+");
        //latency.open ("latency.txt");
        encodeFrameBuff = (unsigned char*) malloc(100000);
        seqnum_ = 0;
        fragmenterBuffer = (unsigned char*) malloc(2000);
        rtpBuffer_ = (unsigned char*) malloc(4000);
        lengthFrag = 0;
        firstPackage = true;

    }

    CropFilter::~CropFilter() {
        avfilter_graph_free(&filter_graph);
        free(rtpBuffer_);
        free(encodeFrameBuff);
        free(fragmenterBuffer);
    }

    void CropFilter::enable() {
    }

    void CropFilter::disable() {
    }

    void CropFilter::notifyUpdate() {

    }

    Positions CropFilter::position() {
        return End;
    }

    void CropFilter::read(Context *ctx, std::shared_ptr <DataPacket> packet) {
        std::shared_ptr<DataPacket> copied_packet = std::make_shared<DataPacket>(*packet);
        RtcpHeader *rtcp_head = reinterpret_cast<RtcpHeader*> (copied_packet->data);
        if(packet->type == VIDEO_PACKET &&  !rtcp_head->isRtcp()){ //dont filter rtcp packets
            RtpHeader *copy_head = reinterpret_cast<RtpHeader*> (copied_packet->data);
            if(firstPackage){
                seqnum_ = copy_head->getSeqNumber(); //Get first seqnumber
                auto pipeline = getContext()->getPipelineShared(); //configuration for plis
                stream_ = pipeline->getService<MediaStream>().get();
                video_sink_ssrc_ = stream_->getVideoSinkSSRC();
                video_source_ssrc_ = stream_->getVideoSourceSSRC();
                firstPackage = false;
            }

            dpckg->fetchPacket(reinterpret_cast<unsigned char*>(packet->data), packet->length); //Add packet data to processor
            gotFrameFromPckg = dpckg->processPacket(); //Recover frame if available
            if(gotFrameFromPckg) {
                vDecoder.decodeVideo(dpckg->frame(), dpckg->frameSize(), outBuff.get(), outBuffLen, &gotFrame); //Decode frame
                if(gotFrame){
                    int error; //error code
                    char text[500]; //text for errors
                    frame = vDecoder.returnAVFrame(); //get frame from decoder
                    //display_frame(frame,1);
                    if(frame->width != lastWidth|| frame->height != lastHeight){
                        sizeChanged = true;
                        encoderInit = false;
                    }
                    lastWidth = frame->width;
                    lastHeight = frame->height;
                    if(sizeChanged){
                        configureFilters(frame->height, frame->width);
                        sizeChanged = false;
                    }
                    frame->pts = copy_head->getTimestamp(); //Copy timestamp
                    error = av_buffersrc_write_frame(buffersrc_ctx, frame); //Send packet to filter graph
                    av_strerror(error, text, 500);
                    av_buffersink_get_frame(buffersink_ctx, filt_frame); //Get frame from filter graph
                    // display_frame(filt_frame,2);
                    if(!encoderInit){
                        filtFrameLenght = avpicture_get_size(AV_PIX_FMT_YUV420P,filt_frame->width,filt_frame->height); //Filt frame lenght
                        numberPixels = filt_frame->width * filt_frame->height; //Number of pixels of frame, nPixel *1.5=  frame lenght in YUV420p
                        filtFrameBuffer = (unsigned char*) malloc(filtFrameLenght); //Alloc buffer for filtered frame
                        vEncodeInfo.height = filt_frame->height;
                        vEncodeInfo.width = filt_frame->width;
                        vEncoder.initEncoder(vEncodeInfo);
                        encoderInit = true;
                        if(filt_frame->key_frame != 1){
                            auto pipeline = getContext()->getPipelineShared();
                            stream_ = pipeline->getService<MediaStream>().get();
                            video_sink_ssrc_ = stream_->getVideoSinkSSRC();
                            video_source_ssrc_ = stream_->getVideoSourceSSRC();
                            sendPLI();
                        }
                    }
                    memcpy(filtFrameBuffer, filt_frame->data[0], numberPixels); //Copy Y plane to buffer
                    memcpy(&filtFrameBuffer[numberPixels], filt_frame->data[1], numberPixels/4); //Copy U plane to buffer
                    memcpy(&filtFrameBuffer[numberPixels+numberPixels/4], filt_frame->data[2], numberPixels/4); //Copy V plane to buffer
                    int l = vEncoder.encodeVideo(filtFrameBuffer, filtFrameLenght, encodeFrameBuff, encodeFrameBuffLen); //Encode frame
                    RtpVP8Fragmenter frag(encodeFrameBuff, l); //Fragmeter divides frame in fragments
                    dpckg->reset();
                    lastFragPacket = false;
                    while(!lastFragPacket){ //While we have fragments
                        lengthFrag=0;
                        frag.getPacket(fragmenterBuffer, &lengthFrag,&lastFragPacket); //Get fragment
                        RtpHeader rtpHeader; //Create header
                        rtpHeader.setSeqNumber(seqnum_++); //increase seq number
                        rtpHeader.setPayloadType(96); //Set payload type
                        rtpHeader.setSSRC(copy_head->getSSRC());//Same ssrc as original packet
                        rtpHeader.setTimestamp(copy_head->getTimestamp());//Same timestamp as original packet

                        rtpHeader.setMarker(lastFragPacket?1:0); //Set marker
                        int len = lengthFrag+rtpHeader.getHeaderLength();
                        memcpy(rtpBuffer_, &rtpHeader, rtpHeader.getHeaderLength()); //Copy header to buffer
                        memcpy(&rtpBuffer_[rtpHeader.getHeaderLength()],fragmenterBuffer,lengthFrag); //Copy data to buffer

                        //Create packet with buffer previous buffer
                        std::shared_ptr<DataPacket> exit_packet = std::make_shared<DataPacket>(0, reinterpret_cast<char*>(rtpBuffer_),
                                                                                               len, VIDEO_PACKET, copied_packet->received_time_ms);
                        ctx->fireRead(std::move(exit_packet)); //Send packet
                    }

                }else{
                    auto pipeline = getContext()->getPipelineShared();
                    stream_ = pipeline->getService<MediaStream>().get();
                    video_sink_ssrc_ = stream_->getVideoSinkSSRC();
                    video_source_ssrc_ = stream_->getVideoSourceSSRC();
                    sendPLI();
                    RtpHeader *head = reinterpret_cast<RtpHeader*> (packet->data);
                    head->setSeqNumber(seqnum_++);
                    ctx->fireRead(std::move(RtpUtils::makeVP8BlackKeyframePacket(packet)));
                }
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

    void CropFilter::sendPLI(packetPriority priority) {
        getContext()->fireRead(RtpUtils::createPLI(video_sink_ssrc_, video_source_ssrc_, priority));
    }


    void CropFilter::configureFilters(int height , int width) {
        //Config params for filters
        char text[500];
        ELOG_DEBUG("Create graph");
        snprintf(args, sizeof(args), //Arguments
                 "width=%d:height=%d:pix_fmt=yuv420p:time_base=1/20:sar=1",width,height);

        int error = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                                 args, NULL, filter_graph); //Configure
        av_strerror(error,text,500); //Error code to error text
        ELOG_DEBUG("Created source %s",text); //Log error text or success



        snprintf(args, sizeof(args),
                 "out_w=1/3*in_w:out_h=1/3*in_h");
        error = avfilter_graph_create_filter(&crop_ctx, crop, "crop",
                                             args, NULL, filter_graph);
        av_strerror(error,text,500);
        ELOG_DEBUG("Created crop?: %s",text);

        snprintf(args, sizeof(args),
                 "w=%d:h=%d",width,height);
        error = avfilter_graph_create_filter(&resize_ctx, resize, "scale",
                                             args, NULL, filter_graph);
        av_strerror(error,text,500);
        ELOG_DEBUG("Created resize?: %s",text);

        error = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                             NULL, NULL, filter_graph);

        av_strerror(error,text,500);
        ELOG_DEBUG("Created sink?: %s",text);
        ELOG_DEBUG("Created graph");


        //Link filters to create graph
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
    }





}
